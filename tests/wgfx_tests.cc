// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

// W3C webgpu-samples conformance smoke test, rendered through the wgfx
// Vulkan backend. Shaders are compiled GLSL -> SPIR-V (the WebGPU WGSL
// samples transpiled), since WGSL is not implemented yet.
//
//  1. "triangle": renders the classic W3C triangle offscreen and verifies
//     the interpolated center pixel and the cleared background.
//  2. "compute": runs the W3C times-two compute shader and verifies the
//     storage buffer contents.
//  3. "surface": presents the triangle to a GLFW window via the swapchain.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "webgpu-headers/webgpu.h"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include "GLFW/glfw3native.h"
#endif

#include "triangle.vert.spv.h"
#include "triangle.frag.spv.h"
#include "compute.comp.spv.h"

namespace {

int g_failures = 0;

#define CHECK(cond, message)                                        \
  do {                                                              \
    if (!(cond)) {                                                  \
      std::printf("FAIL: %s\n", message);                           \
      ++g_failures;                                                 \
    } else {                                                        \
      std::printf("PASS: %s\n", message);                           \
    }                                                               \
  } while (0)

struct AdapterCapture {
  WGPUAdapter adapter = nullptr;
};

void RequestAdapterCallback(WGPURequestAdapterStatus status,
                            WGPUAdapter adapter, WGPUStringView message,
                            void* userdata1, void* userdata2) {
  auto* capture = static_cast<AdapterCapture*>(userdata1);
  capture->adapter = status == WGPURequestAdapterStatus_Success ? adapter
                                                                : nullptr;
}

struct DeviceCapture {
  WGPUDevice device = nullptr;
};

void RequestDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device,
                           WGPUStringView message, void* userdata1,
                           void* userdata2) {
  auto* capture = static_cast<DeviceCapture*>(userdata1);
  capture->device = status == WGPURequestDeviceStatus_Success ? device
                                                              : nullptr;
}

void MapAsyncCallback(WGPUMapAsyncStatus status, WGPUStringView message,
                      void* userdata1, void* userdata2) {
  *static_cast<bool*>(userdata1) = status == WGPUMapAsyncStatus_Success;
}

void QueueWorkDoneCallback(WGPUQueueWorkDoneStatus status,
                           WGPUStringView message, void* userdata1,
                           void* userdata2) {
  *static_cast<bool*>(userdata1) = status == WGPUQueueWorkDoneStatus_Success;
}

WGPUShaderModule CreateShaderModule(WGPUDevice device,
                                    const uint32_t* code,
                                    size_t code_size) {
  WGPUShaderSourceSPIRV source = WGPU_SHADER_SOURCE_SPIRV_INIT;
  source.codeSize = static_cast<uint32_t>(code_size);
  source.code = code;

  WGPUShaderModuleDescriptor descriptor = {};
  descriptor.nextInChain = &source.chain;
  return wgpuDeviceCreateShaderModule(device, &descriptor);
}

// Shared pipeline state templates used for both the offscreen and
// on-surface triangle pipelines. The vertex layout storage must outlive
// the descriptors, hence the static arrays.
static WGPUVertexAttribute kVertexAttributes[2];
static WGPUVertexBufferLayout kVertexLayout;
static WGPUVertexState vertex_state_template;
static WGPUPrimitiveState primitive_template;
static WGPUMultisampleState multisample_template;

// Creates the triangle render pipeline from the W3C "triangle" sample.
WGPURenderPipeline CreateTrianglePipeline(WGPUDevice device) {
  WGPUShaderModule vertex_module =
      CreateShaderModule(device, kTriangleVertSpv, kTriangleVertSpvSize);
  WGPUShaderModule fragment_module =
      CreateShaderModule(device, kTriangleFragSpv, kTriangleFragSpvSize);

  kVertexAttributes[0].format = WGPUVertexFormat_Float32x2;
  kVertexAttributes[0].offset = 0;
  kVertexAttributes[0].shaderLocation = 0;
  kVertexAttributes[1].format = WGPUVertexFormat_Float32x3;
  kVertexAttributes[1].offset = 8;
  kVertexAttributes[1].shaderLocation = 1;

  kVertexLayout.stepMode = WGPUVertexStepMode_Vertex;
  kVertexLayout.arrayStride = 20;
  kVertexLayout.attributeCount = 2;
  kVertexLayout.attributes = kVertexAttributes;

  WGPUVertexState vertex = {};
  vertex.module = vertex_module;
  vertex.entryPoint = WGPU_STRING_VIEW_INIT;  // "main"
  vertex.bufferCount = 1;
  vertex.buffers = &kVertexLayout;

  WGPUBlendState blend = {};
  blend.color.srcFactor = WGPUBlendFactor_One;
  blend.color.dstFactor = WGPUBlendFactor_Zero;
  blend.color.operation = WGPUBlendOperation_Add;
  blend.alpha = blend.color;

  WGPUColorTargetState color_target = {};
  color_target.format = WGPUTextureFormat_RGBA8Unorm;
  color_target.blend = &blend;
  color_target.writeMask = WGPUColorWriteMask_All;

  WGPUFragmentState fragment = {};
  fragment.module = fragment_module;
  fragment.entryPoint = WGPU_STRING_VIEW_INIT;
  fragment.targetCount = 1;
  fragment.targets = &color_target;

  WGPUPrimitiveState primitive = {};
  primitive.topology = WGPUPrimitiveTopology_TriangleList;
  primitive.frontFace = WGPUFrontFace_CCW;
  primitive.cullMode = WGPUCullMode_None;
  primitive_template = primitive;

  WGPUMultisampleState multisample = {};
  multisample.count = 1;
  multisample_template = multisample;

  WGPURenderPipelineDescriptor descriptor = {};
  descriptor.vertex = vertex;
  descriptor.fragment = &fragment;
  descriptor.primitive = primitive;
  descriptor.multisample = multisample;

  WGPURenderPipeline pipeline =
      wgpuDeviceCreateRenderPipeline(device, &descriptor);
  vertex_state_template = vertex;
  wgpuShaderModuleRelease(vertex_module);
  std::fflush(stdout);
  wgpuShaderModuleRelease(fragment_module);
  std::fflush(stdout);
  return pipeline;
}

WGPUBuffer CreateTriangleVertexBuffer(WGPUDevice device) {
  // Vertex positions and colors from the W3C webgpu-samples triangle.
  static const float vertices[15] = {
      0.0f, 0.5f, 1.0f, 0.0f, 0.0f,     //
      -0.5f, -0.5f, 0.0f, 1.0f, 0.0f,   //
      0.5f, -0.5f, 0.0f, 0.0f, 1.0f,    //
  };

  WGPUBufferDescriptor descriptor = {};
  descriptor.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
  descriptor.size = sizeof(vertices);
  descriptor.mappedAtCreation = WGPUBool(true);
  WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);
  std::memcpy(wgpuBufferGetMappedRange(buffer, 0, WGPU_WHOLE_SIZE), vertices,
              sizeof(vertices));
  wgpuBufferUnmap(buffer);
  return buffer;
}

// Renders the W3C triangle into an offscreen texture and verifies pixels.
void TestTriangleRender(WGPUInstance instance, WGPUDevice device,
                        WGPURenderPipeline pipeline) {
  constexpr uint32_t kWidth = 256;
  constexpr uint32_t kHeight = 256;
  constexpr uint32_t kBytesPerRow = kWidth * 4;

  WGPUTextureDescriptor texture_descriptor = {};
  texture_descriptor.usage = WGPUTextureUsage_RenderAttachment |
                             WGPUTextureUsage_CopySrc;
  texture_descriptor.dimension = WGPUTextureDimension_2D;
  texture_descriptor.size = {kWidth, kHeight, 1};
  texture_descriptor.format = WGPUTextureFormat_RGBA8Unorm;
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;
  WGPUTexture texture = wgpuDeviceCreateTexture(device, &texture_descriptor);
  WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);

  WGPUBuffer readback_buffer = nullptr;
  {
    WGPUBufferDescriptor descriptor = {};
    descriptor.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    descriptor.size = kBytesPerRow * kHeight;
    readback_buffer = wgpuDeviceCreateBuffer(device, &descriptor);
  }

  WGPUBuffer vertex_buffer = CreateTriangleVertexBuffer(device);

  WGPURenderPassColorAttachment color_attachment = {};
  color_attachment.view = view;
  color_attachment.loadOp = WGPULoadOp_Clear;
  color_attachment.storeOp = WGPUStoreOp_Store;
  color_attachment.clearValue = {0.0, 0.0, 0.0, 0.0};

  WGPURenderPassDescriptor pass_descriptor = {};
  pass_descriptor.colorAttachmentCount = 1;
  pass_descriptor.colorAttachments = &color_attachment;

  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
  WGPURenderPassEncoder pass =
      wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
  wgpuRenderPassEncoderSetPipeline(pass, pipeline);
  wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertex_buffer, 0,
                                       WGPU_WHOLE_SIZE);
  wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);

  WGPUTexelCopyTextureInfo texture_source = {};
  texture_source.texture = texture;

  WGPUTexelCopyBufferInfo buffer_destination = {};
  buffer_destination.layout.bytesPerRow = kBytesPerRow;
  buffer_destination.buffer = readback_buffer;

  WGPUExtent3D copy_size = {kWidth, kHeight, 1};
  wgpuCommandEncoderCopyTextureToBuffer(encoder, &texture_source,
                                        &buffer_destination, &copy_size);

  WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(encoder, nullptr);
  wgpuCommandEncoderRelease(encoder);

  WGPUQueue queue = wgpuDeviceGetQueue(device);
  wgpuQueueSubmit(queue, 1, &command_buffer);
  wgpuCommandBufferRelease(command_buffer);

  bool work_done = false;
  WGPUQueueWorkDoneCallbackInfo done_info = {};
  done_info.mode = WGPUCallbackMode_WaitAnyOnly;
  done_info.callback = QueueWorkDoneCallback;
  done_info.userdata1 = &work_done;
  WGPUFuture work_done_future = wgpuQueueOnSubmittedWorkDone(queue, done_info);

  // Wait for the submission to complete before reading back pixels.
  WGPUFutureWaitInfo wait_info = {};
  wait_info.future = work_done_future;
  WGPUWaitStatus wait_status =
      wgpuInstanceWaitAny(instance, 1, &wait_info, UINT64_MAX);
  CHECK(wait_status == WGPUWaitStatus_Success && work_done,
        "queue work done completes");
  (void)wait_status;

  // Map the readback buffer and inspect the pixels.
  bool mapped = false;
  WGPUBufferMapCallbackInfo map_info = {};
  map_info.mode = WGPUCallbackMode_AllowSpontaneous;
  map_info.callback = MapAsyncCallback;
  map_info.userdata1 = &mapped;
  wgpuBufferMapAsync(readback_buffer, WGPUMapMode_Read, 0,
                     kBytesPerRow * kHeight, map_info);

  const uint8_t* pixels =
      static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(
          readback_buffer, 0, kBytesPerRow * kHeight));
  (void)mapped;

  // Center pixel: interpolated one-third red, green, blue.
  const uint8_t* center = pixels + (kHeight / 2 * kBytesPerRow) +
                          (kWidth / 2 * 4);
  std::printf("center pixel = (%d, %d, %d, %d)" "\n", (int)center[0],
              (int)center[1], (int)center[2], (int)center[3]);
  std::fflush(stdout);
  // Center of the framebuffer is NDC (0, 0): barycentric weights
  // (0.5, 0.25, 0.25) over the red/green/blue vertices.
  CHECK(std::abs(static_cast<int>(center[0]) - 127) < 2 &&
            std::abs(static_cast<int>(center[1]) - 64) < 2 &&
            std::abs(static_cast<int>(center[2]) - 64) < 2 &&
            center[3] == 255,
        "triangle center pixel is the interpolated vertex color");
  // Corner pixel: the cleared background.
  const uint8_t* corner = pixels + (10 * kBytesPerRow) + (10 * 4);
  CHECK(corner[0] == 0 && corner[1] == 0 && corner[2] == 0 && corner[3] == 0,
        "triangle background pixel is the clear color");

  wgpuBufferUnmap(readback_buffer);
  wgpuBufferRelease(readback_buffer);
  wgpuBufferRelease(vertex_buffer);
  wgpuTextureViewRelease(view);
  wgpuTextureRelease(texture);
  wgpuQueueRelease(queue);
}

// Runs the W3C "compute" times-two shader over a storage buffer.
void TestCompute(WGPUInstance instance, WGPUDevice device) {
  WGPUShaderModule module =
      CreateShaderModule(device, kComputeCompSpv, kComputeCompSpvSize);

  WGPUComputeState compute = {};
  compute.module = module;

  WGPUComputePipelineDescriptor descriptor = {};
  descriptor.compute = compute;
  WGPUComputePipeline pipeline =
      wgpuDeviceCreateComputePipeline(device, &descriptor);

  static const float input[8] = {1.0f, 2.0f, 3.0f, 4.0f,
                                 5.0f, 6.0f, 7.0f, 8.0f};
  WGPUBuffer storage_buffer = nullptr;
  {
    WGPUBufferDescriptor buffer_descriptor = {};
    buffer_descriptor.usage = WGPUBufferUsage_Storage |
                              WGPUBufferUsage_CopyDst |
                              WGPUBufferUsage_CopySrc |
                              WGPUBufferUsage_MapRead;
    buffer_descriptor.size = sizeof(input);
    storage_buffer = wgpuDeviceCreateBuffer(device, &buffer_descriptor);
  }

  WGPUQueue queue = wgpuDeviceGetQueue(device);
  wgpuQueueWriteBuffer(queue, storage_buffer, 0, input, sizeof(input));

  // Bind the storage buffer through the pipeline's auto layout.
  WGPUBindGroupLayout bgl =
      wgpuComputePipelineGetBindGroupLayout(pipeline, 0);
  WGPUBindGroupEntry entry = {};
  entry.binding = 0;
  entry.buffer = storage_buffer;
  entry.offset = 0;
  entry.size = sizeof(input);
  WGPUBindGroupDescriptor bind_group_descriptor = {};
  bind_group_descriptor.layout = bgl;
  bind_group_descriptor.entryCount = 1;
  bind_group_descriptor.entries = &entry;
  WGPUBindGroup bind_group =
      wgpuDeviceCreateBindGroup(device, &bind_group_descriptor);

  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
  WGPUComputePassEncoder pass =
      wgpuCommandEncoderBeginComputePass(encoder, nullptr);
  wgpuComputePassEncoderSetPipeline(pass, pipeline);
  wgpuComputePassEncoderSetBindGroup(pass, 0, bind_group, 0, nullptr);
  wgpuComputePassEncoderDispatchWorkgroups(pass, 8, 1, 1);
  wgpuComputePassEncoderEnd(pass);
  wgpuComputePassEncoderRelease(pass);

  WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(encoder, nullptr);
  wgpuCommandEncoderRelease(encoder);
  wgpuQueueSubmit(queue, 1, &command_buffer);
  wgpuCommandBufferRelease(command_buffer);

  // Wait for the dispatch to complete before reading back the buffer.
  bool work_done = false;
  WGPUQueueWorkDoneCallbackInfo done_info = {};
  done_info.mode = WGPUCallbackMode_WaitAnyOnly;
  done_info.callback = QueueWorkDoneCallback;
  done_info.userdata1 = &work_done;
  WGPUFuture work_done_future = wgpuQueueOnSubmittedWorkDone(queue, done_info);
  WGPUFutureWaitInfo wait_info = {};
  wait_info.future = work_done_future;
  wgpuInstanceWaitAny(instance, 1, &wait_info, UINT64_MAX);

  bool mapped = false;
  WGPUBufferMapCallbackInfo map_info = {};
  map_info.mode = WGPUCallbackMode_AllowSpontaneous;
  map_info.callback = MapAsyncCallback;
  map_info.userdata1 = &mapped;
  wgpuBufferMapAsync(storage_buffer, WGPUMapMode_Read, 0, sizeof(input),
                     map_info);

  const float* result =
      static_cast<const float*>(wgpuBufferGetConstMappedRange(
          storage_buffer, 0, sizeof(input)));
  bool doubled = true;
  for (size_t i = 0; i < 8; ++i) {
    std::printf("compute[%d] = %f (want %f)" "\n", (int)i, result[i],
                input[i] * 2.0f);
    doubled = doubled && std::fabs(result[i] - input[i] * 2.0f) < 1e-6f;
  }
  std::fflush(stdout);
  CHECK(doubled, "compute shader doubled the storage buffer");
  (void)mapped;

  wgpuBufferUnmap(storage_buffer);
  wgpuBufferRelease(storage_buffer);
  wgpuBindGroupRelease(bind_group);
  wgpuBindGroupLayoutRelease(bgl);
  wgpuComputePipelineRelease(pipeline);
  wgpuShaderModuleRelease(module);
  wgpuQueueRelease(queue);
}

// Presents the W3C triangle to a GLFW window through the swapchain.
void TestSurfacePresentation(WGPUInstance instance, WGPUDevice device,
                             WGPURenderPipeline /*pipeline*/) {
  if (!glfwInit()) {
    CHECK(false, "GLFW initialization");
    return;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(400, 300, "wgfx W3C triangle",
                                        nullptr, nullptr);
  CHECK(window != nullptr, "GLFW window creation");

  WGPUSurfaceDescriptor surface_descriptor = {};
#if defined(_WIN32)
  WGPUSurfaceSourceWindowsHWND hwnd_source = {};
  hwnd_source.chain.next = nullptr;
  hwnd_source.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
  hwnd_source.hinstance = reinterpret_cast<void*>(GetModuleHandle(nullptr));
  hwnd_source.hwnd = reinterpret_cast<void*>(glfwGetWin32Window(window));
  surface_descriptor.nextInChain = &hwnd_source.chain;
#elif defined(__linux__)
  if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
    static WGPUSurfaceSourceWaylandSurface wayland_source = {};
    wayland_source.chain.next = nullptr;
    wayland_source.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
    wayland_source.display = glfwGetWaylandDisplay();
    wayland_source.surface = glfwGetWaylandWindow(window);
    surface_descriptor.nextInChain = &wayland_source.chain;
  } else {
    static WGPUSurfaceSourceXlibWindow xlib_source = {};
    xlib_source.chain.next = nullptr;
    xlib_source.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    xlib_source.display = glfwGetX11Display();
    xlib_source.window = glfwGetX11Window(window);
    surface_descriptor.nextInChain = &xlib_source.chain;
  }
#else
  WGPUSurface surface = nullptr;
  CHECK(false, "surface test unsupported on this platform in this build");
#endif
#if defined(_WIN32) || defined(__linux__)
  WGPUSurface surface =
      wgpuInstanceCreateSurface(instance, &surface_descriptor);
  CHECK(surface != nullptr, "surface creation from native window");
#endif

  if (surface) {
    WGPUTextureFormat config_format = WGPUTextureFormat_BGRA8Unorm;
    WGPUSurfaceConfiguration config = {};
    config.device = device;
    config.format = config_format;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = 400;
    config.height = 300;
    config.alphaMode = WGPUCompositeAlphaMode_Opaque;
    config.presentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(surface, &config);

    // The pipeline's render pass must match the surface format.
    WGPUShaderModule surface_vertex_module =
        CreateShaderModule(device, kTriangleVertSpv, kTriangleVertSpvSize);
    WGPUShaderModule surface_fragment_module =
        CreateShaderModule(device, kTriangleFragSpv, kTriangleFragSpvSize);
    WGPUVertexState surface_vertex = vertex_state_template;
    surface_vertex.module = surface_vertex_module;
    WGPUColorTargetState surface_target = {};
    surface_target.format = config_format;
    surface_target.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState surface_fragment = {};
    surface_fragment.module = surface_fragment_module;
    surface_fragment.targetCount = 1;
    surface_fragment.targets = &surface_target;
    WGPURenderPipelineDescriptor surface_pipeline_descriptor = {};
    surface_pipeline_descriptor.vertex = surface_vertex;
    surface_pipeline_descriptor.fragment = &surface_fragment;
    surface_pipeline_descriptor.primitive = primitive_template;
    surface_pipeline_descriptor.multisample = multisample_template;
    WGPURenderPipeline surface_pipeline =
        wgpuDeviceCreateRenderPipeline(device, &surface_pipeline_descriptor);
    wgpuShaderModuleRelease(surface_vertex_module);
    wgpuShaderModuleRelease(surface_fragment_module);

    WGPUBuffer vertex_buffer = CreateTriangleVertexBuffer(device);
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view = nullptr;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearValue = {0.0, 0.0, 0.0, 1.0};

    WGPURenderPassDescriptor pass_descriptor = {};
    pass_descriptor.colorAttachmentCount = 1;
    pass_descriptor.colorAttachments = &color_attachment;

    bool presented = true;
    for (int frame = 0; frame < 30 && !glfwWindowShouldClose(window);
         ++frame) {
      glfwPollEvents();

      WGPUSurfaceTexture surface_texture = {};
      wgpuSurfaceGetCurrentTexture(surface, &surface_texture);
      if (surface_texture.status !=
          WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
        wgpuTextureRelease(surface_texture.texture);
        presented = false;
        break;
      }

      color_attachment.view = wgpuTextureCreateView(surface_texture.texture,
                                                    nullptr);
      WGPUCommandEncoder encoder =
          wgpuDeviceCreateCommandEncoder(device, nullptr);
      WGPURenderPassEncoder pass =
          wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
      wgpuRenderPassEncoderSetPipeline(pass, surface_pipeline);
      wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertex_buffer, 0,
                                           WGPU_WHOLE_SIZE);
      wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
      wgpuRenderPassEncoderEnd(pass);
      wgpuRenderPassEncoderRelease(pass);

      WGPUCommandBuffer command_buffer =
          wgpuCommandEncoderFinish(encoder, nullptr);
      wgpuCommandEncoderRelease(encoder);
      wgpuQueueSubmit(queue, 1, &command_buffer);
      wgpuCommandBufferRelease(command_buffer);

      if (wgpuSurfacePresent(surface) != WGPUStatus_Success)
        presented = false;
      wgpuTextureViewRelease(color_attachment.view);
      wgpuTextureRelease(surface_texture.texture);
    }
    CHECK(presented, "30 surface frames presented");

    wgpuBufferRelease(vertex_buffer);
    wgpuRenderPipelineRelease(surface_pipeline);
    wgpuQueueRelease(queue);
    wgpuSurfaceUnconfigure(surface);
    wgpuSurfaceRelease(surface);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
}

}  // namespace

int main() {
  std::fflush(stdout);
  WGPUInstance instance = wgpuCreateInstance(nullptr);
  CHECK(instance != nullptr, "instance creation");
  std::fflush(stdout);

  std::fflush(stdout);
  AdapterCapture adapter_capture;
  WGPURequestAdapterCallbackInfo adapter_info = {};
  adapter_info.mode = WGPUCallbackMode_AllowSpontaneous;
  adapter_info.callback = RequestAdapterCallback;
  adapter_info.userdata1 = &adapter_capture;
  wgpuInstanceRequestAdapter(instance, nullptr, adapter_info);
  std::fflush(stdout);
  CHECK(adapter_capture.adapter != nullptr, "adapter enumeration");

  std::fflush(stdout);
  DeviceCapture device_capture;
  if (adapter_capture.adapter) {
    WGPURequestDeviceCallbackInfo device_info = {};
    device_info.mode = WGPUCallbackMode_AllowSpontaneous;
    device_info.callback = RequestDeviceCallback;
    device_info.userdata1 = &device_capture;
    wgpuAdapterRequestDevice(adapter_capture.adapter, nullptr, device_info);
  }
  std::fflush(stdout);
  CHECK(device_capture.device != nullptr, "device creation");

  std::fflush(stdout);

  if (device_capture.device) {
    WGPURenderPipeline pipeline =
        CreateTrianglePipeline(device_capture.device);
      std::fflush(stdout);
    CHECK(pipeline != nullptr, "triangle pipeline creation (SPIR-V)");

    TestTriangleRender(instance, device_capture.device, pipeline);
    TestCompute(instance, device_capture.device);
    TestSurfacePresentation(instance, device_capture.device, pipeline);

    if (pipeline)
      wgpuRenderPipelineRelease(pipeline);
    wgpuDeviceRelease(device_capture.device);
  }

  if (adapter_capture.adapter)
    wgpuAdapterRelease(adapter_capture.adapter);
  wgpuInstanceRelease(instance);

  std::printf(g_failures ? "\n%d test(s) FAILED\n" : "\nAll tests passed\n",
              g_failures);
  return g_failures ? 1 : 0;
}
