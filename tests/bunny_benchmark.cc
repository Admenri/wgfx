// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

// Bunny mark benchmark for the wgfx Vulkan backend, in the spirit of the
// webgpu-samples "bunnys" demo. Renders an animated swarm of 2D bunny
// sprites (from tests/bunnys.png, a 1-column atlas of 5 bunny types) into
// an offscreen target and measures frame throughput. The GPU is waited on
// every frame, so timings include the full submit + execute round trip.
//
// The benchmark variant is selected through macros, so each mode is a
// separate build of this translation unit:
//
//   BUNNY_BATCH_MODE    0 = one draw call per bunny
//                       1 = 10 bunnies per draw call (default). In the
//                           per-bunny-uniform mode a "batch" still issues one
//                           bind group + draw per bunny, grouped in tens.
//   BUNNY_UNIFORM_MODE  0 = one shared storage buffer + one bind group for
//                         all bunny transforms (default)
//                       1 = one uniform buffer + one bind group per bunny
//                       2 = no uniform transport at all; the CPU bakes the
//                           final vertex positions/uvs every frame
//   BUNNY_COUNT         number of bunnies (default 2000)
//   BUNNY_FRAMES        number of measured frames (default 5000, roughly
//                       2-3 seconds at uncapped fps; close the window to
//                       stop early)
//   BUNNY_BATCH_SIZE    bunnies per batched draw call (default 10)
//   BUNNY_VSYNC         0 = immediate present for uncapped fps (default),
//                       1 = FIFO vsync

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "webgpu-headers/webgpu.h"

#include "bunnys_rgba.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"
#endif
#include "bunny.frag.spv.h"

#if BUNNY_UNIFORM_MODE == 0
#include "bunny_shared.vert.spv.h"
#elif BUNNY_UNIFORM_MODE == 1
#include "bunny_ubo.vert.spv.h"
#else
#include "bunny_baked.vert.spv.h"
#endif

#ifndef BUNNY_BATCH_MODE
#define BUNNY_BATCH_MODE 1
#endif
#ifndef BUNNY_UNIFORM_MODE
#define BUNNY_UNIFORM_MODE 0
#endif
#ifndef BUNNY_COUNT
#define BUNNY_COUNT 10000
#endif
#ifndef BUNNY_FRAMES
#define BUNNY_FRAMES 5000
#endif
#ifndef BUNNY_BATCH_SIZE
#define BUNNY_BATCH_SIZE 10
#endif
#ifndef BUNNY_VSYNC
#define BUNNY_VSYNC 0
#endif

namespace {

constexpr uint32_t kWidth = 1280;
constexpr uint32_t kHeight = 720;
constexpr uint32_t kBunnyTypes = 5;
constexpr float kSpriteW = static_cast<float>(kBunnyAtlasWidth);
constexpr float kSpriteH =
    static_cast<float>(kBunnyAtlasHeight) / kBunnyTypes;

WGPUInstance g_instance = nullptr;
WGPUDevice g_device = nullptr;
WGPUQueue g_queue = nullptr;

struct BenchmarkState {
  WGPURenderPipeline pipeline = nullptr;
  WGPUTexture atlas = nullptr;
  WGPUSampler sampler = nullptr;
  // Single descriptor set: binding 0 = transform resource (modes 0/1),
  // binding 1 = atlas view, binding 2 = sampler (all modes).
  WGPUBindGroup transform_group = nullptr;  // mode 0
  WGPUBindGroup* bunny_groups = nullptr;    // mode 1
  WGPUBindGroup texture_group = nullptr;    // mode 2 (atlas + sampler only)
  WGPUBuffer* bunny_ubos = nullptr;         // mode 1: per-bunny UBOs
  WGPUBuffer storage_buffer = nullptr;      // mode 0
  WGPUBuffer vertex_buffer = nullptr;       // static (0/1) or dynamic (2)
  uint32_t stride = 0;
  uint32_t quads_per_draw = 0;
};

WGPUShaderModule CreateShaderModule(WGPUDevice device, const uint32_t* code,
                                    size_t code_size) {
  WGPUShaderSourceSPIRV source = WGPU_SHADER_SOURCE_SPIRV_INIT;
  source.codeSize = static_cast<uint32_t>(code_size);
  source.code = code;
  WGPUShaderModuleDescriptor descriptor = {};
  descriptor.nextInChain = &source.chain;
  return wgpuDeviceCreateShaderModule(device, &descriptor);
}

void BuildStaticVertexData(BenchmarkState* state, uint32_t bunny_count) {
  static const float corners[6][2] = {{0, 0}, {1, 0}, {0, 1},
                                      {1, 0}, {1, 1}, {0, 1}};
#if BUNNY_UNIFORM_MODE == 0
  // Static vertex buffer: NDC corner + atlas uv + bunny id, all bunnies.
  state->stride = 20;
  WGPUBufferDescriptor descriptor = {};
  descriptor.usage = WGPUBufferUsage_Vertex;
  descriptor.size = static_cast<uint64_t>(bunny_count) * 6 * state->stride;
  descriptor.mappedAtCreation = WGPUBool(true);
  state->vertex_buffer = wgpuDeviceCreateBuffer(g_device, &descriptor);
  auto* vertices = static_cast<float*>(
      wgpuBufferGetMappedRange(state->vertex_buffer, 0, WGPU_WHOLE_SIZE));
  auto* ids = reinterpret_cast<uint32_t*>(vertices + 4);
  for (uint32_t b = 0; b < bunny_count; ++b) {
    float uv0y = static_cast<float>(b % kBunnyTypes) / kBunnyTypes;
    for (uint32_t v = 0; v < 6; ++v) {
      float* out = vertices + (b * 6 + v) * 5;
      out[0] = corners[v][0] * 2.0f - 1.0f;
      out[1] = corners[v][1] * 2.0f - 1.0f;
      out[2] = corners[v][0];
      out[3] = uv0y + corners[v][1] / kBunnyTypes;
      ids[b * 6 + v] = b;
    }
  }
  wgpuBufferUnmap(state->vertex_buffer);
#elif BUNNY_UNIFORM_MODE == 1
  // Static shared quad: 0..1 corners; uv comes from the per-bunny uniform.
  state->stride = 8;
  WGPUBufferDescriptor descriptor = {};
  descriptor.usage = WGPUBufferUsage_Vertex;
  descriptor.size = 6 * state->stride;
  descriptor.mappedAtCreation = WGPUBool(true);
  state->vertex_buffer = wgpuDeviceCreateBuffer(g_device, &descriptor);
  auto* vertices = static_cast<float*>(
      wgpuBufferGetMappedRange(state->vertex_buffer, 0, WGPU_WHOLE_SIZE));
  for (uint32_t v = 0; v < 6; ++v) {
    vertices[v * 2 + 0] = corners[v][0];
    vertices[v * 2 + 1] = corners[v][1];
  }
  wgpuBufferUnmap(state->vertex_buffer);
#else
  // Dynamic vertex buffer: final pos + uv per vertex, rewritten each frame.
  state->stride = 16;
  WGPUBufferDescriptor descriptor = {};
  descriptor.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_MapWrite;
  descriptor.size = static_cast<uint64_t>(bunny_count) * 6 * state->stride;
  state->vertex_buffer = wgpuDeviceCreateBuffer(g_device, &descriptor);
#endif
}

WGPURenderPipeline CreatePipeline(WGPUDevice device,
                                  WGPUTextureFormat target_format) {
#if BUNNY_UNIFORM_MODE == 0
  WGPUShaderModule vertex =
      CreateShaderModule(device, kBunnySharedVertSpv, kBunnySharedVertSpvSize);
  WGPUVertexAttribute attributes[3] = {};
  attributes[0].format = WGPUVertexFormat_Float32x2;
  attributes[1].format = WGPUVertexFormat_Float32x2;
  attributes[1].shaderLocation = 1;
  attributes[2].format = WGPUVertexFormat_Uint32;
  attributes[2].offset = 16;
  attributes[2].shaderLocation = 2;
  WGPUVertexBufferLayout layout = {};
  layout.stepMode = WGPUVertexStepMode_Vertex;
  layout.arrayStride = 20;
  layout.attributeCount = 3;
  layout.attributes = attributes;
#elif BUNNY_UNIFORM_MODE == 1
  WGPUShaderModule vertex =
      CreateShaderModule(device, kBunnyUboVertSpv, kBunnyUboVertSpvSize);
  WGPUVertexAttribute attributes[1] = {};
  attributes[0].format = WGPUVertexFormat_Float32x2;
  WGPUVertexBufferLayout layout = {};
  layout.stepMode = WGPUVertexStepMode_Vertex;
  layout.arrayStride = 8;
  layout.attributeCount = 1;
  layout.attributes = attributes;
#else
  WGPUShaderModule vertex =
      CreateShaderModule(device, kBunnyBakedVertSpv, kBunnyBakedVertSpvSize);
  WGPUVertexAttribute attributes[2] = {};
  attributes[0].format = WGPUVertexFormat_Float32x2;
  attributes[1].format = WGPUVertexFormat_Float32x2;
  attributes[1].shaderLocation = 1;
  WGPUVertexBufferLayout layout = {};
  layout.stepMode = WGPUVertexStepMode_Vertex;
  layout.arrayStride = 16;
  layout.attributeCount = 2;
  layout.attributes = attributes;
#endif

  WGPUVertexState vertex_state = {};
  vertex_state.module = vertex;
  vertex_state.bufferCount = 1;
  vertex_state.buffers = &layout;

  WGPUShaderModule fragment =
      CreateShaderModule(device, kBunnyFragSpv, kBunnyFragSpvSize);
  WGPUColorTargetState target = {};
  target.format = target_format;
  target.writeMask = WGPUColorWriteMask_All;
  WGPUBlendState blend = {};
  blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
  blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
  blend.color.operation = WGPUBlendOperation_Add;
  blend.alpha.srcFactor = WGPUBlendFactor_One;
  blend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
  blend.alpha.operation = WGPUBlendOperation_Add;
  target.blend = &blend;
  WGPUFragmentState fragment_state = {};
  fragment_state.module = fragment;
  fragment_state.targetCount = 1;
  fragment_state.targets = &target;

  WGPURenderPipelineDescriptor descriptor = {};
  descriptor.vertex = vertex_state;
  descriptor.fragment = &fragment_state;
  descriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  descriptor.primitive.frontFace = WGPUFrontFace_CCW;
  descriptor.multisample.count = 1;
  WGPURenderPipeline pipeline =
      wgpuDeviceCreateRenderPipeline(device, &descriptor);
  wgpuShaderModuleRelease(vertex);
  wgpuShaderModuleRelease(fragment);
  return pipeline;
}

void SubmitAndWait(WGPUQueue queue, WGPUCommandEncoder encoder) {
  WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(encoder, nullptr);
  wgpuCommandEncoderRelease(encoder);
  wgpuQueueSubmit(queue, 1, &command_buffer);
  wgpuCommandBufferRelease(command_buffer);

  // Block until the GPU finished: keeps frame timing honest and avoids
  // write-after-use hazards on the per-frame uploads.
  bool work_done = false;
  WGPUQueueWorkDoneCallbackInfo done_info = {};
  done_info.mode = WGPUCallbackMode_WaitAnyOnly;
  done_info.callback = [](WGPUQueueWorkDoneStatus, WGPUStringView, void* ud1,
                          void*) { *static_cast<bool*>(ud1) = true; };
  done_info.userdata1 = &work_done;
  WGPUFuture future = wgpuQueueOnSubmittedWorkDone(queue, done_info);
  WGPUFutureWaitInfo wait_info = {};
  wait_info.future = future;
  wgpuInstanceWaitAny(g_instance, 1, &wait_info, UINT64_MAX);
}

}  // namespace

int main() {
#if BUNNY_UNIFORM_MODE == 0
  const char* uniform_mode = "shared storage buffer + one bind group";
#elif BUNNY_UNIFORM_MODE == 1
  const char* uniform_mode = "per-bunny uniform buffer + bind group";
#else
  const char* uniform_mode = "no uniforms (baked vertices)";
#endif
#if BUNNY_BATCH_MODE
  const char* batch_mode = "batched (10 bunnies per draw call)";
#else
  const char* batch_mode = "one draw call per bunny";
#endif

  std::printf("wgfx bunny benchmark\n");
  std::printf("  bunnies : %d\n", BUNNY_COUNT);
  std::printf("  frames  : %d\n", BUNNY_FRAMES);
  std::printf("  draws   : %s\n", batch_mode);
  std::printf("  uniforms: %s\n", uniform_mode);
  std::printf("  present : %s\n",
              BUNNY_VSYNC ? "FIFO (vsync)" : "immediate (uncapped)");
  std::fflush(stdout);

  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window =
      glfwCreateWindow(kWidth, kHeight, "wgfx bunny benchmark", nullptr,
                       nullptr);
  if (!window) {
    std::printf("FAIL: no window\n");
    return 1;
  }

  WGPUInstance instance = wgpuCreateInstance(nullptr);
  g_instance = instance;

#if defined(_WIN32)
  WGPUSurfaceSourceWindowsHWND hwnd_source = {};
  hwnd_source.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
  hwnd_source.hinstance = reinterpret_cast<void*>(GetModuleHandle(nullptr));
  hwnd_source.hwnd = reinterpret_cast<void*>(glfwGetWin32Window(window));
  WGPUSurfaceDescriptor surface_descriptor = {};
  surface_descriptor.nextInChain = &hwnd_source.chain;
#else
  std::printf("FAIL: surface source not implemented on this platform\n");
  return 1;
#endif
  WGPUSurface surface = wgpuInstanceCreateSurface(instance, &surface_descriptor);
  if (!surface) {
    std::printf("FAIL: no surface\n");
    return 1;
  }

  WGPUAdapter adapter = nullptr;
  WGPURequestAdapterOptions adapter_options = {};
  adapter_options.compatibleSurface = surface;
  WGPURequestAdapterCallbackInfo adapter_info = {};
  adapter_info.mode = WGPUCallbackMode_AllowSpontaneous;
  adapter_info.callback = [](WGPURequestAdapterStatus status, WGPUAdapter a,
                             WGPUStringView, void* userdata1, void*) {
    *static_cast<WGPUAdapter*>(userdata1) =
        status == WGPURequestAdapterStatus_Success ? a : nullptr;
  };
  adapter_info.userdata1 = &adapter;
  wgpuInstanceRequestAdapter(instance, &adapter_options, adapter_info);
  if (!adapter) {
    std::printf("FAIL: no adapter\n");
    return 1;
  }

  WGPUDevice device = nullptr;
  WGPURequestDeviceCallbackInfo device_info = {};
  device_info.mode = WGPUCallbackMode_AllowSpontaneous;
  device_info.callback = [](WGPURequestDeviceStatus status, WGPUDevice d,
                            WGPUStringView, void* userdata1, void*) {
    *static_cast<WGPUDevice*>(userdata1) =
        status == WGPURequestDeviceStatus_Success ? d : nullptr;
  };
  device_info.userdata1 = &device;
  wgpuAdapterRequestDevice(adapter, nullptr, device_info);
  if (!device) {
    std::printf("FAIL: no device\n");
    return 1;
  }
  g_device = device;
  g_queue = wgpuDeviceGetQueue(g_device);

  // Configure the surface with a supported format; the pipeline is built
  // against the same format for render-pass compatibility.
  WGPUSurfaceCapabilities capabilities = {};
  if (wgpuSurfaceGetCapabilities(surface, adapter, &capabilities) !=
          WGPUStatus_Success ||
      !capabilities.formatCount) {
    std::printf("FAIL: no surface formats\n");
    return 1;
  }
  WGPUTextureFormat surface_format = capabilities.formats[0];
  for (size_t i = 0; i < capabilities.formatCount; ++i) {
    if (capabilities.formats[i] == WGPUTextureFormat_BGRA8Unorm)
      surface_format = WGPUTextureFormat_BGRA8Unorm;
  }
  WGPUPresentMode present_mode =
      BUNNY_VSYNC ? WGPUPresentMode_Fifo : WGPUPresentMode_Immediate;
  bool present_mode_supported = BUNNY_VSYNC;  // FIFO is always supported
  for (size_t i = 0; i < capabilities.presentModeCount; ++i) {
    if (capabilities.presentModes[i] == present_mode)
      present_mode_supported = true;
  }
  if (!present_mode_supported)
    present_mode = WGPUPresentMode_Fifo;

  WGPUSurfaceConfiguration config = {};
  config.device = device;
  config.format = surface_format;
  config.usage = WGPUTextureUsage_RenderAttachment;
  config.width = kWidth;
  config.height = kHeight;
  config.alphaMode = WGPUCompositeAlphaMode_Opaque;
  config.presentMode = present_mode;
  wgpuSurfaceConfigure(surface, &config);

  BenchmarkState state = {};
  state.pipeline = CreatePipeline(g_device, surface_format);
  state.quads_per_draw = BUNNY_BATCH_MODE ? BUNNY_BATCH_SIZE : 1;
  BuildStaticVertexData(&state, BUNNY_COUNT);

  // Atlas texture from the decoded bunnys.png.
  WGPUTextureDescriptor atlas_descriptor = {};
  atlas_descriptor.usage =
      WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
  atlas_descriptor.dimension = WGPUTextureDimension_2D;
  atlas_descriptor.size = {kBunnyAtlasWidth, kBunnyAtlasHeight, 1};
  atlas_descriptor.format = WGPUTextureFormat_RGBA8Unorm;
  atlas_descriptor.mipLevelCount = 1;
  atlas_descriptor.sampleCount = 1;
  state.atlas = wgpuDeviceCreateTexture(g_device, &atlas_descriptor);
  WGPUTexelCopyTextureInfo atlas_destination = {};
  atlas_destination.texture = state.atlas;
  WGPUTexelCopyBufferLayout atlas_layout = {};
  atlas_layout.bytesPerRow = kBunnyAtlasWidth * 4;
  WGPUExtent3D atlas_extent = {kBunnyAtlasWidth, kBunnyAtlasHeight, 1};
  wgpuQueueWriteTexture(g_queue, &atlas_destination, kBunnyAtlasPixels,
                        sizeof(kBunnyAtlasPixels), &atlas_layout,
                        &atlas_extent);

  WGPUSamplerDescriptor sampler_descriptor = {};
  sampler_descriptor.magFilter = WGPUFilterMode_Linear;
  sampler_descriptor.minFilter = WGPUFilterMode_Linear;
  sampler_descriptor.mipmapFilter = WGPUMipmapFilterMode_Nearest;
  state.sampler = wgpuDeviceCreateSampler(g_device, &sampler_descriptor);

  // The automatic layout merges the vertex and fragment resource bindings
  // into one descriptor set: binding 0 = transform resource (modes 0/1),
  // binding 1 = atlas view, binding 2 = sampler.
  WGPUBindGroupLayout set_layout =
      wgpuRenderPipelineGetBindGroupLayout(state.pipeline, 0);
  WGPUTextureView atlas_view = wgpuTextureCreateView(state.atlas, nullptr);

#if BUNNY_UNIFORM_MODE == 0
  // Shared storage buffer with one transform per bunny.
  WGPUBufferDescriptor storage_descriptor = {};
  storage_descriptor.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
  storage_descriptor.size = static_cast<uint64_t>(BUNNY_COUNT) * 16;
  state.storage_buffer = wgpuDeviceCreateBuffer(g_device, &storage_descriptor);
  WGPUBindGroupEntry entries[3] = {};
  entries[0].binding = 0;
  entries[0].buffer = state.storage_buffer;
  entries[0].offset = 0;
  entries[0].size = storage_descriptor.size;
  entries[1].binding = 1;
  entries[1].textureView = atlas_view;
  entries[2].binding = 2;
  entries[2].sampler = state.sampler;
  WGPUBindGroupDescriptor group_descriptor = {};
  group_descriptor.layout = set_layout;
  group_descriptor.entryCount = 3;
  group_descriptor.entries = entries;
  state.transform_group =
      wgpuDeviceCreateBindGroup(g_device, &group_descriptor);
#elif BUNNY_UNIFORM_MODE == 1
  // One uniform buffer + one bind group (with the atlas and sampler) per
  // bunny. The UBOs are created mapped so per-frame updates are plain host
  // writes through WriteBuffer.
  state.bunny_ubos = new WGPUBuffer[BUNNY_COUNT];
  state.bunny_groups = new WGPUBindGroup[BUNNY_COUNT];
  for (uint32_t b = 0; b < BUNNY_COUNT; ++b) {
    WGPUBufferDescriptor ubo_descriptor = {};
    ubo_descriptor.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    ubo_descriptor.size = 32;
    ubo_descriptor.mappedAtCreation = WGPUBool(true);
    state.bunny_ubos[b] = wgpuDeviceCreateBuffer(g_device, &ubo_descriptor);
    wgpuBufferUnmap(state.bunny_ubos[b]);
    WGPUBindGroupEntry entries[3] = {};
    entries[0].binding = 0;
    entries[0].buffer = state.bunny_ubos[b];
    entries[0].offset = 0;
    entries[0].size = 32;
    entries[1].binding = 1;
    entries[1].textureView = atlas_view;
    entries[2].binding = 2;
    entries[2].sampler = state.sampler;
    WGPUBindGroupDescriptor group_descriptor = {};
    group_descriptor.layout = set_layout;
    group_descriptor.entryCount = 3;
    group_descriptor.entries = entries;
    state.bunny_groups[b] =
        wgpuDeviceCreateBindGroup(g_device, &group_descriptor);
  }
#else
  WGPUBindGroupEntry entries[2] = {};
  entries[0].binding = 1;
  entries[0].textureView = atlas_view;
  entries[1].binding = 2;
  entries[1].sampler = state.sampler;
  WGPUBindGroupDescriptor group_descriptor = {};
  group_descriptor.layout = set_layout;
  group_descriptor.entryCount = 2;
  group_descriptor.entries = entries;
  state.texture_group =
      wgpuDeviceCreateBindGroup(g_device, &group_descriptor);
#endif

  glfwSetWindowTitle(window, "wgfx bunny benchmark");

  // Bunny physics state (screen space, y down).
  struct Bunny {
    float x, y, vx, vy;
  };
  std::vector<Bunny> bunnies(BUNNY_COUNT);
  for (uint32_t i = 0; i < BUNNY_COUNT; ++i) {
    bunnies[i].x = 40.0f + (i % 120) * 10.0f;
    bunnies[i].y = 40.0f + (i / 120) * 24.0f;
    bunnies[i].vx = 40.0f + 60.0f * ((i * 2654435761u) % 97) / 97.0f;
    bunnies[i].vy = -300.0f * ((i * 40503u) % 89) / 89.0f;
  }

  const float half_w_ndc = kSpriteW / kWidth / 2.0f;
  const float half_h_ndc = kSpriteH / kHeight / 2.0f;
  const float gravity = 1500.0f;
  const float floor_y = static_cast<float>(kHeight) - kSpriteH;
  const float dt = 1.0f / 60.0f;

  WGPURenderPassColorAttachment attachment = {};
  attachment.loadOp = WGPULoadOp_Clear;
  attachment.storeOp = WGPUStoreOp_Store;
  attachment.clearValue = {0.1, 0.12, 0.2, 1.0};
  WGPURenderPassDescriptor pass_descriptor = {};
  pass_descriptor.colorAttachmentCount = 1;
  pass_descriptor.colorAttachments = &attachment;

  std::vector<double> frame_times;
  frame_times.reserve(BUNNY_FRAMES);
  double sim_time_total = 0;
  double upload_time_total = 0;
  double render_time_total = 0;
  std::vector<float> transforms(BUNNY_COUNT * 4);
#if BUNNY_UNIFORM_MODE == 2
  std::vector<float> baked_vertices(BUNNY_COUNT * 6 * 4);
#endif

  uint32_t frame = 0;
  while (frame < BUNNY_FRAMES && !glfwWindowShouldClose(window)) {
    glfwPollEvents();
    auto frame_start = std::chrono::steady_clock::now();

    // Simulation.
    auto sim_start = std::chrono::steady_clock::now();
    for (Bunny& bunny : bunnies) {
      bunny.vy += gravity * dt;
      bunny.x += bunny.vx * dt;
      bunny.y += bunny.vy * dt;
      if (bunny.y > floor_y && bunny.vy > 0.0f) {
        bunny.y = floor_y;
        bunny.vy *= -0.85f;
      }
      if (bunny.x < 0.0f || bunny.x > kWidth - kSpriteW)
        bunny.vx *= -1.0f;
      bunny.x = std::clamp(bunny.x, 0.0f, kWidth - kSpriteW);
    }

    auto sim_end = std::chrono::steady_clock::now();
    sim_time_total +=
        std::chrono::duration<double, std::milli>(sim_end - sim_start).count();

    // Per-mode transform upload.
    auto upload_start = std::chrono::steady_clock::now();
#if BUNNY_UNIFORM_MODE == 0
    for (uint32_t b = 0; b < BUNNY_COUNT; ++b) {
      transforms[b * 4 + 0] =
          2.0f * (bunnies[b].x + kSpriteW / 2) / kWidth - 1.0f;
      transforms[b * 4 + 1] =
          1.0f - 2.0f * (bunnies[b].y + kSpriteH / 2) / kHeight;
      transforms[b * 4 + 2] = half_w_ndc;
      transforms[b * 4 + 3] = half_h_ndc;
    }
    wgpuQueueWriteBuffer(g_queue, state.storage_buffer, 0, transforms.data(),
                         transforms.size() * sizeof(float));
#elif BUNNY_UNIFORM_MODE == 1
    for (uint32_t b = 0; b < BUNNY_COUNT; ++b) {
      float data[8] = {
          2.0f * (bunnies[b].x + kSpriteW / 2) / kWidth - 1.0f,
          1.0f - 2.0f * (bunnies[b].y + kSpriteH / 2) / kHeight,
          half_w_ndc,      half_h_ndc,
          0.0f,            static_cast<float>(b % kBunnyTypes) / kBunnyTypes,
          1.0f,            1.0f / kBunnyTypes,
      };
      wgpuQueueWriteBuffer(g_queue, state.bunny_ubos[b], 0, data,
                           sizeof(data));
    }
#else
    for (uint32_t b = 0; b < BUNNY_COUNT; ++b) {
      float x0 = 2.0f * bunnies[b].x / kWidth - 1.0f;
      float x1 = 2.0f * (bunnies[b].x + kSpriteW) / kWidth - 1.0f;
      float y0 = 1.0f - 2.0f * (bunnies[b].y + kSpriteH) / kHeight;
      float y1 = 1.0f - 2.0f * bunnies[b].y / kHeight;
      float uv0y = static_cast<float>(b % kBunnyTypes) / kBunnyTypes;
      static const float corner_x[6] = {0, 1, 0, 1, 1, 0};
      static const float corner_y[6] = {0, 0, 1, 0, 1, 1};
      for (uint32_t v = 0; v < 6; ++v) {
        float* out = baked_vertices.data() + (b * 6 + v) * 4;
        out[0] = corner_x[v] ? x1 : x0;
        out[1] = corner_y[v] ? y1 : y0;
        out[2] = corner_x[v];
        out[3] = uv0y + corner_y[v] / kBunnyTypes;
      }
    }
    wgpuBufferWriteMappedRange(state.vertex_buffer, 0, baked_vertices.data(),
                               baked_vertices.size() * sizeof(float));
#endif

    auto upload_end = std::chrono::steady_clock::now();
    upload_time_total +=
        std::chrono::duration<double, std::milli>(upload_end - upload_start)
            .count();
    auto render_start = upload_end;

    // Acquire the swapchain image.
    WGPUSurfaceTexture surface_texture = {};
    wgpuSurfaceGetCurrentTexture(surface, &surface_texture);
    if (surface_texture.status !=
        WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
      wgpuTextureRelease(surface_texture.texture);
      continue;
    }
    attachment.view =
        wgpuTextureCreateView(surface_texture.texture, nullptr);

    // Encoding.
    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(g_device, nullptr);
    WGPURenderPassEncoder pass =
        wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
    wgpuRenderPassEncoderSetPipeline(pass, state.pipeline);
#if BUNNY_UNIFORM_MODE == 0
    wgpuRenderPassEncoderSetBindGroup(pass, 0, state.transform_group, 0,
                                      nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, state.vertex_buffer, 0,
                                         WGPU_WHOLE_SIZE);
    for (uint32_t start = 0; start < BUNNY_COUNT;
         start += state.quads_per_draw) {
      uint32_t quads = std::min(state.quads_per_draw, BUNNY_COUNT - start);
      wgpuRenderPassEncoderDraw(pass, quads * 6, 1, start * 6, 0);
    }
#elif BUNNY_UNIFORM_MODE == 1
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, state.vertex_buffer, 0,
                                         WGPU_WHOLE_SIZE);
    for (uint32_t start = 0; start < BUNNY_COUNT;
         start += state.quads_per_draw) {
      uint32_t quads = std::min(state.quads_per_draw, BUNNY_COUNT - start);
      for (uint32_t b = start; b < start + quads; ++b) {
        wgpuRenderPassEncoderSetBindGroup(pass, 0, state.bunny_groups[b], 0,
                                          nullptr);
        wgpuRenderPassEncoderDraw(pass, 6, 1, 0, 0);
      }
    }
#else
    wgpuRenderPassEncoderSetBindGroup(pass, 0, state.texture_group, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, state.vertex_buffer, 0,
                                         WGPU_WHOLE_SIZE);
    for (uint32_t start = 0; start < BUNNY_COUNT;
         start += state.quads_per_draw) {
      uint32_t quads = std::min(state.quads_per_draw, BUNNY_COUNT - start);
      wgpuRenderPassEncoderDraw(pass, quads * 6, 1, start * 6, 0);
    }
#endif
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    // Submit (waits on the swapchain acquire semaphore) and present.
    WGPUCommandBuffer command_buffer =
        wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);
    wgpuQueueSubmit(g_queue, 1, &command_buffer);
    wgpuCommandBufferRelease(command_buffer);
    wgpuSurfacePresent(surface);
    wgpuTextureViewRelease(attachment.view);
    wgpuTextureRelease(surface_texture.texture);

    auto frame_end = std::chrono::steady_clock::now();
    render_time_total +=
        std::chrono::duration<double, std::milli>(frame_end - render_start)
            .count();
    frame_times.push_back(
        std::chrono::duration<double, std::milli>(frame_end - frame_start)
            .count());
    ++frame;
  }

  // Report.
  std::printf("  frames rendered       : %zu\n", frame_times.size());
  std::vector<double> sorted(frame_times);
  std::sort(sorted.begin(), sorted.end());
  double total = 0;
  for (double t : frame_times)
    total += t;
  double avg = total / frame_times.size();
  double p95 = sorted[sorted.size() * 95 / 100];
  uint32_t draws_per_frame =
      (BUNNY_COUNT + state.quads_per_draw - 1) / state.quads_per_draw;

  std::printf("  draws/frame            : %u\n", draws_per_frame);
  std::printf("  avg sim time           : %.3f ms\n",
              sim_time_total / frame_times.size());
  std::printf("  avg upload time        : %.3f ms\n",
              upload_time_total / frame_times.size());
  std::printf("  avg render+present time: %.3f ms\n",
              render_time_total / frame_times.size());
  std::printf("  avg frame time         : %.3f ms\n", avg);
  std::printf("  min frame time         : %.3f ms\n", sorted.front());
  std::printf("  p95 frame time         : %.3f ms\n", p95);
  std::printf("  max frame time         : %.3f ms\n", sorted.back());
  std::printf("  average fps            : %.1f\n", 1000.0 / avg);
#if BUNNY_UNIFORM_MODE == 2
  std::printf("  vertex bytes/frame     : %llu\n",
              (unsigned long long)(BUNNY_COUNT * 6 * 16));
#endif

  // Cleanup.
  wgpuSurfaceUnconfigure(surface);
  wgpuSurfaceRelease(surface);
  glfwDestroyWindow(window);
  glfwTerminate();
  wgpuBufferRelease(state.vertex_buffer);
#if BUNNY_UNIFORM_MODE == 0
  wgpuBindGroupRelease(state.transform_group);
  wgpuBufferRelease(state.storage_buffer);
#elif BUNNY_UNIFORM_MODE == 1
  for (uint32_t b = 0; b < BUNNY_COUNT; ++b) {
    wgpuBindGroupRelease(state.bunny_groups[b]);
    wgpuBufferRelease(state.bunny_ubos[b]);
  }
  delete[] state.bunny_groups;
  delete[] state.bunny_ubos;
#endif
#if BUNNY_UNIFORM_MODE == 2
  wgpuBindGroupRelease(state.texture_group);
#endif
  wgpuTextureViewRelease(atlas_view);
  wgpuSamplerRelease(state.sampler);
  wgpuTextureRelease(state.atlas);
  wgpuRenderPipelineRelease(state.pipeline);
  wgpuQueueRelease(g_queue);
  wgpuDeviceRelease(g_device);
  wgpuAdapterRelease(adapter);
  wgpuInstanceRelease(instance);
  return 0;
}
