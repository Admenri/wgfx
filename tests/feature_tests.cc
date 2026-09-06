// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

// W3C WebGPU feature conformance tests for the wgfx Vulkan backend.
// Each test exercises one optional WebGPU feature end to end and is
// gated on the device advertising that feature.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#include "webgpu-headers/webgpu.h"

#include "dual_source.frag.spv.h"
#include "instance_color.vert.spv.h"
#include "instance_color.frag.spv.h"
#include "compute_f16.comp.spv.h"
#include "compute_subgroup.comp.spv.h"
#include "fullscreen.vert.spv.h"
#include "tex_sample.frag.spv.h"
#include "clip.vert.spv.h"
#include "clip.frag.spv.h"
#include "solid_red.frag.spv.h"

namespace {

int g_failures = 0;
WGPUDevice g_device = nullptr;
WGPUQueue g_queue = nullptr;
WGPUInstance g_instance = nullptr;

#define CHECK(cond, message)                                          \
  do {                                                                \
    if (g_device && !(cond)) {                                        \
      std::printf("FAIL: %s\n", message);                             \
      ++g_failures;                                                   \
    } else if (g_device) {                                            \
      std::printf("PASS: %s\n", message);                             \
    }                                                                 \
  } while (0)

void QueueWorkDoneCallback(WGPUQueueWorkDoneStatus status,
                           WGPUStringView message, void* userdata1,
                           void* userdata2) {
  *static_cast<bool*>(userdata1) = status == WGPUQueueWorkDoneStatus_Success;
}

// Resolves a future by waiting through the global instance handle.
void WaitForFuture(WGPUFuture future) {
  if (!g_instance)
    return;
  WGPUFutureWaitInfo wait_info = {};
  wait_info.future = future;
  wgpuInstanceWaitAny(g_instance, 1, &wait_info, UINT64_MAX);
}

// Submits |encoder| and blocks until the work completes.
void SubmitAndWait(WGPUCommandEncoder encoder) {
  WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(encoder, nullptr);
  wgpuCommandEncoderRelease(encoder);
  wgpuQueueSubmit(g_queue, 1, &command_buffer);
  wgpuCommandBufferRelease(command_buffer);

  bool work_done = false;
  WGPUQueueWorkDoneCallbackInfo done_info = {};
  done_info.mode = WGPUCallbackMode_WaitAnyOnly;
  done_info.callback = QueueWorkDoneCallback;
  done_info.userdata1 = &work_done;
  WGPUFuture future = wgpuQueueOnSubmittedWorkDone(g_queue, done_info);
  WaitForFuture(future);
}

WGPUShaderModule CreateShaderModule(const uint32_t* code, size_t code_size) {
  WGPUShaderSourceSPIRV source = WGPU_SHADER_SOURCE_SPIRV_INIT;
  source.codeSize = static_cast<uint32_t>(code_size);
  source.code = code;
  WGPUShaderModuleDescriptor descriptor = {};
  descriptor.nextInChain = &source.chain;
  return wgpuDeviceCreateShaderModule(g_device, &descriptor);
}

WGPUBuffer CreateBuffer(WGPUBufferUsage usage, uint64_t size,
                        bool mapped = false) {
  WGPUBufferDescriptor descriptor = {};
  descriptor.usage = usage;
  descriptor.size = size;
  descriptor.mappedAtCreation = mapped ? WGPUBool(true) : WGPUBool(false);
  return wgpuDeviceCreateBuffer(g_device, &descriptor);
}

WGPUTexture CreateTexture(WGPUTextureUsage usage, WGPUTextureFormat format,
                          uint32_t width, uint32_t height) {
  WGPUTextureDescriptor descriptor = {};
  descriptor.usage = usage;
  descriptor.dimension = WGPUTextureDimension_2D;
  descriptor.size = {width, height, 1};
  descriptor.format = format;
  descriptor.mipLevelCount = 1;
  descriptor.sampleCount = 1;
  return wgpuDeviceCreateTexture(g_device, &descriptor);
}

const uint8_t* MapReadBuffer(WGPUBuffer buffer, uint64_t size) {
  WGPUBufferMapCallbackInfo map_info = {};
  map_info.mode = WGPUCallbackMode_AllowSpontaneous;
  map_info.callback = [](WGPUMapAsyncStatus, WGPUStringView, void*, void*) {};
  wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, size, map_info);
  return static_cast<const uint8_t*>(
      wgpuBufferGetConstMappedRange(buffer, 0, size));
}

// ---------------------------------------------------------------------------
// DualSourceBlending: red output blended with the second source alpha over a
// green clear color; result is yellow.
// ---------------------------------------------------------------------------
void TestDualSourceBlending() {
  WGPUShaderModule fragment = CreateShaderModule(kDualSourceFragSpv,
                                                 kDualSourceFragSpvSize);
  WGPUShaderModule vertex = CreateShaderModule(kFullscreenVertSpv,
                                               kFullscreenVertSpvSize);

  WGPUVertexState vertex_state = {};
  vertex_state.module = vertex;
  WGPUBlendState blend = {};
  blend.color.srcFactor = WGPUBlendFactor_One;
  blend.color.dstFactor = WGPUBlendFactor_Src1Alpha;
  blend.color.operation = WGPUBlendOperation_Add;
  blend.alpha.srcFactor = WGPUBlendFactor_One;
  blend.alpha.dstFactor = WGPUBlendFactor_Zero;
  blend.alpha.operation = WGPUBlendOperation_Add;
  WGPUColorTargetState target = {};
  target.format = WGPUTextureFormat_RGBA8Unorm;
  target.blend = &blend;
  target.writeMask = WGPUColorWriteMask_All;
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
      wgpuDeviceCreateRenderPipeline(g_device, &descriptor);
  CHECK(pipeline != nullptr, "DualSourceBlending: pipeline creation");

  if (pipeline) {
    WGPUTexture texture = CreateTexture(
        WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
        WGPUTextureFormat_RGBA8Unorm, 64, 64);
    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);

    WGPURenderPassColorAttachment attachment = {};
    attachment.view = view;
    attachment.loadOp = WGPULoadOp_Clear;
    attachment.storeOp = WGPUStoreOp_Store;
    attachment.clearValue = {0.0, 1.0, 0.0, 1.0};
    WGPURenderPassDescriptor pass_descriptor = {};
    pass_descriptor.colorAttachmentCount = 1;
    pass_descriptor.colorAttachments = &attachment;

    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(g_device, nullptr);
    WGPURenderPassEncoder pass =
        wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    SubmitAndWait(encoder);

    // Read back the blended pixel.
    WGPUBuffer readback = CreateBuffer(WGPUBufferUsage_MapRead |
                                           WGPUBufferUsage_CopyDst,
                                       64 * 64 * 4);
    WGPUTexelCopyTextureInfo source = {};
    source.texture = texture;
    WGPUTexelCopyBufferInfo destination = {};
    destination.layout.bytesPerRow = 64 * 4;
    destination.buffer = readback;
    WGPUExtent3D size = {64, 64, 1};
    WGPUCommandEncoder copy_encoder =
        wgpuDeviceCreateCommandEncoder(g_device, nullptr);
    wgpuCommandEncoderCopyTextureToBuffer(copy_encoder, &source, &destination,
                                          &size);
    SubmitAndWait(copy_encoder);

    const uint8_t* pixels = MapReadBuffer(readback, 64 * 64 * 4);
    const uint8_t* center = pixels + (32 * 64 * 4) + (32 * 4);
    std::printf("  dual blend pixel = (%d, %d, %d, %d)\n", (int)center[0],
                (int)center[1], (int)center[2], (int)center[3]);
    CHECK(std::abs(static_cast<int>(center[0]) - 255) < 2 &&
              std::abs(static_cast<int>(center[1]) - 127) < 2 &&
              center[2] < 2,
          "DualSourceBlending: blended pixel is red over 0.5 alpha");
    wgpuBufferUnmap(readback);
    wgpuBufferRelease(readback);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(texture);
  }

  wgpuShaderModuleRelease(fragment);
  wgpuShaderModuleRelease(vertex);
}

// ---------------------------------------------------------------------------
// TimestampQuery: two timestamps written outside passes, resolved into a
// buffer; the second must not be smaller than the first.
// ---------------------------------------------------------------------------
void TestTimestampQuery() {
  WGPUQuerySetDescriptor query_descriptor = {};
  query_descriptor.type = WGPUQueryType_Timestamp;
  query_descriptor.count = 2;
  WGPUQuerySet query_set = wgpuDeviceCreateQuerySet(g_device, &query_descriptor);
  CHECK(query_set != nullptr, "TimestampQuery: query set creation");

  WGPUBuffer resolve = CreateBuffer(WGPUBufferUsage_QueryResolve |
                                        WGPUBufferUsage_MapRead,
                                    16);

  WGPUCommandEncoder encoder =
      wgpuDeviceCreateCommandEncoder(g_device, nullptr);
  wgpuCommandEncoderWriteTimestamp(encoder, query_set, 0);
  wgpuCommandEncoderWriteTimestamp(encoder, query_set, 1);
  wgpuCommandEncoderResolveQuerySet(encoder, query_set, 0, 2, resolve, 0);
  SubmitAndWait(encoder);

  const uint8_t* data = MapReadBuffer(resolve, 16);
  uint64_t t0 = 0;
  uint64_t t1 = 0;
  std::memcpy(&t0, data, 8);
  std::memcpy(&t1, data + 8, 8);
  std::printf("  timestamps: %llu -> %llu\n", (unsigned long long)t0,
              (unsigned long long)t1);
  CHECK((t0 != 0 || t1 != 0) && t1 >= t0,
        "TimestampQuery: timestamps resolve monotonically");
  wgpuBufferUnmap(resolve);
  wgpuBufferRelease(resolve);
  wgpuQuerySetRelease(query_set);
}

// ---------------------------------------------------------------------------
// IndirectFirstInstance: an indirect draw with firstInstance = 1 must paint
// green (instance index 1), red otherwise.
// ---------------------------------------------------------------------------
void TestIndirectFirstInstance() {
  WGPUShaderModule vertex = CreateShaderModule(kInstanceColorVertSpv,
                                               kInstanceColorVertSpvSize);
  WGPUShaderModule fragment = CreateShaderModule(kInstanceColorFragSpv,
                                                 kInstanceColorFragSpvSize);

  WGPUVertexAttribute attribute = {};
  attribute.format = WGPUVertexFormat_Float32x2;
  attribute.offset = 0;
  attribute.shaderLocation = 0;
  WGPUVertexBufferLayout layout = {};
  layout.stepMode = WGPUVertexStepMode_Vertex;
  layout.arrayStride = 8;
  layout.attributeCount = 1;
  layout.attributes = &attribute;
  WGPUVertexState vertex_state = {};
  vertex_state.module = vertex;
  vertex_state.bufferCount = 1;
  vertex_state.buffers = &layout;

  WGPUColorTargetState target = {};
  target.format = WGPUTextureFormat_RGBA8Unorm;
  target.writeMask = WGPUColorWriteMask_All;
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
      wgpuDeviceCreateRenderPipeline(g_device, &descriptor);

  // Fullscreen triangle covering (0,0).
  static const float vertices[6] = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
  WGPUBuffer vertex_buffer =
      CreateBuffer(WGPUBufferUsage_Vertex, sizeof(vertices), true);
  std::memcpy(wgpuBufferGetMappedRange(vertex_buffer, 0, sizeof(vertices)),
              vertices, sizeof(vertices));
  wgpuBufferUnmap(vertex_buffer);

  // firstInstance = 1 -> green.
  struct DrawIndirectArgs {
    uint32_t vertex_count;
    uint32_t instance_count;
    uint32_t first_vertex;
    uint32_t first_instance;
  } args = {3, 1, 0, 1};
  WGPUBuffer indirect = CreateBuffer(WGPUBufferUsage_Indirect | WGPUBufferUsage_CopyDst,
                                     sizeof(args), true);
  std::memcpy(wgpuBufferGetMappedRange(indirect, 0, sizeof(args)), &args,
              sizeof(args));
  wgpuBufferUnmap(indirect);

  WGPUTexture texture = CreateTexture(
      WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
      WGPUTextureFormat_RGBA8Unorm, 64, 64);
  WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);
  WGPURenderPassColorAttachment attachment = {};
  attachment.view = view;
  attachment.loadOp = WGPULoadOp_Clear;
  attachment.storeOp = WGPUStoreOp_Store;
  WGPURenderPassDescriptor pass_descriptor = {};
  pass_descriptor.colorAttachmentCount = 1;
  pass_descriptor.colorAttachments = &attachment;

  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, nullptr);
  WGPURenderPassEncoder pass =
      wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
  wgpuRenderPassEncoderSetPipeline(pass, pipeline);
  wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertex_buffer, 0,
                                       WGPU_WHOLE_SIZE);
  wgpuRenderPassEncoderDrawIndirect(pass, indirect, 0);
  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);
  SubmitAndWait(encoder);

  WGPUBuffer readback = CreateBuffer(WGPUBufferUsage_MapRead |
                                         WGPUBufferUsage_CopyDst,
                                     64 * 64 * 4);
  WGPUTexelCopyTextureInfo source = {};
  source.texture = texture;
  WGPUTexelCopyBufferInfo destination = {};
  destination.layout.bytesPerRow = 64 * 4;
  destination.buffer = readback;
  WGPUExtent3D size = {64, 64, 1};
  WGPUCommandEncoder copy_encoder =
      wgpuDeviceCreateCommandEncoder(g_device, nullptr);
  wgpuCommandEncoderCopyTextureToBuffer(copy_encoder, &source, &destination,
                                        &size);
  SubmitAndWait(copy_encoder);

  const uint8_t* pixels = MapReadBuffer(readback, 64 * 64 * 4);
  const uint8_t* center = pixels + (32 * 64 * 4) + (32 * 4);
  CHECK(center[1] > 200 && center[0] < 50,
        "IndirectFirstInstance: firstInstance = 1 reaches the shader");
  wgpuBufferUnmap(readback);
  wgpuBufferRelease(readback);
  wgpuTextureViewRelease(view);
  wgpuTextureRelease(texture);
  wgpuBufferRelease(indirect);
  wgpuBufferRelease(vertex_buffer);
  wgpuShaderModuleRelease(vertex);
  wgpuShaderModuleRelease(fragment);
}

// ---------------------------------------------------------------------------
// ShaderF16: doubles a float16 storage buffer.
// ---------------------------------------------------------------------------
uint16_t ToHalf(float value) {
  // IEEE 754 binary16 encoding for the small magnitudes used here.
  uint32_t bits = 0;
  std::memcpy(&bits, &value, 4);
  uint32_t sign = (bits >> 16) & 0x8000;
  int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xFF) - 127 + 15;
  uint32_t mantissa = (bits >> 13) & 0x3FF;
  if (exponent <= 0)
    return static_cast<uint16_t>(sign);
  if (exponent >= 31)
    return static_cast<uint16_t>(sign | 0x7C00);
  return static_cast<uint16_t>(sign | (exponent << 10) | mantissa);
}

float FromHalf(uint16_t half) {
  uint32_t sign = (half & 0x8000) << 16;
  uint32_t exponent = (half >> 10) & 0x1F;
  uint32_t mantissa = half & 0x3FF;
  uint32_t bits;
  if (exponent == 0) {
    bits = sign | (mantissa << 13);
  } else if (exponent == 0x1F) {
    bits = sign | 0x7F800000 | (mantissa << 13);
  } else {
    bits = sign | ((exponent - 15 + 127) << 23) | (mantissa << 13);
  }
  float value = 0;
  std::memcpy(&value, &bits, 4);
  return value;
}

void TestShaderF16() {
  WGPUShaderModule module = CreateShaderModule(kComputeF16CompSpv,
                                               kComputeF16CompSpvSize);
  WGPUComputeState compute = {};
  compute.module = module;
  WGPUComputePipelineDescriptor descriptor = {};
  descriptor.compute = compute;
  WGPUComputePipeline pipeline =
      wgpuDeviceCreateComputePipeline(g_device, &descriptor);
  CHECK(pipeline != nullptr, "ShaderF16: pipeline creation");

  static const float input[8] = {0.5f, 1.0f, 1.5f, 2.0f,
                                 2.5f, 3.0f, 3.5f, 4.0f};
  uint16_t halves[8];
  for (size_t i = 0; i < 8; ++i)
    halves[i] = ToHalf(input[i]);

  WGPUBuffer storage = CreateBuffer(
      WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
          WGPUBufferUsage_MapRead,
      sizeof(halves));
  wgpuQueueWriteBuffer(g_queue, storage, 0, halves, sizeof(halves));

  WGPUBindGroupLayout layout =
      wgpuComputePipelineGetBindGroupLayout(pipeline, 0);
  WGPUBindGroupEntry entry = {};
  entry.binding = 0;
  entry.buffer = storage;
  entry.offset = 0;
  entry.size = sizeof(halves);
  WGPUBindGroupDescriptor group_descriptor = {};
  group_descriptor.layout = layout;
  group_descriptor.entryCount = 1;
  group_descriptor.entries = &entry;
  WGPUBindGroup group = wgpuDeviceCreateBindGroup(g_device, &group_descriptor);

  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, nullptr);
  WGPUComputePassEncoder pass =
      wgpuCommandEncoderBeginComputePass(encoder, nullptr);
  wgpuComputePassEncoderSetPipeline(pass, pipeline);
  wgpuComputePassEncoderSetBindGroup(pass, 0, group, 0, nullptr);
  wgpuComputePassEncoderDispatchWorkgroups(pass, 8, 1, 1);
  wgpuComputePassEncoderEnd(pass);
  wgpuComputePassEncoderRelease(pass);
  SubmitAndWait(encoder);

  const uint8_t* data = MapReadBuffer(storage, sizeof(halves));
  bool doubled = true;
  for (size_t i = 0; i < 8; ++i) {
    uint16_t half = 0;
    std::memcpy(&half, data + i * 2, 2);
    float value = FromHalf(half);
    if (std::fabs(value - input[i] * 2.0f) > 0.01f)
      doubled = false;
  }
  CHECK(doubled, "ShaderF16: float16 buffer doubled by the shader");
  wgpuBufferUnmap(storage);
  wgpuBufferRelease(storage);
  wgpuBindGroupRelease(group);
  wgpuBindGroupLayoutRelease(layout);
  wgpuComputePipelineRelease(pipeline);
  wgpuShaderModuleRelease(module);
}

// ---------------------------------------------------------------------------
// Subgroups: the compute shader exposes the subgroup size and count.
// ---------------------------------------------------------------------------
void TestSubgroups() {
  WGPUShaderModule module = CreateShaderModule(kComputeSubgroupCompSpv,
                                               kComputeSubgroupCompSpvSize);
  WGPUComputeState compute = {};
  compute.module = module;
  WGPUComputePipelineDescriptor descriptor = {};
  descriptor.compute = compute;
  WGPUComputePipeline pipeline =
      wgpuDeviceCreateComputePipeline(g_device, &descriptor);

  WGPUBuffer storage = CreateBuffer(
      WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
          WGPUBufferUsage_MapRead,
      8);
  uint32_t zeros[2] = {0, 0};
  wgpuQueueWriteBuffer(g_queue, storage, 0, zeros, sizeof(zeros));

  WGPUBindGroupLayout layout =
      wgpuComputePipelineGetBindGroupLayout(pipeline, 0);
  WGPUBindGroupEntry entry = {};
  entry.binding = 0;
  entry.buffer = storage;
  entry.offset = 0;
  entry.size = 8;
  WGPUBindGroupDescriptor group_descriptor = {};
  group_descriptor.layout = layout;
  group_descriptor.entryCount = 1;
  group_descriptor.entries = &entry;
  WGPUBindGroup group = wgpuDeviceCreateBindGroup(g_device, &group_descriptor);

  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, nullptr);
  WGPUComputePassEncoder pass =
      wgpuCommandEncoderBeginComputePass(encoder, nullptr);
  wgpuComputePassEncoderSetPipeline(pass, pipeline);
  wgpuComputePassEncoderSetBindGroup(pass, 0, group, 0, nullptr);
  wgpuComputePassEncoderDispatchWorkgroups(pass, 1, 1, 1);
  wgpuComputePassEncoderEnd(pass);
  wgpuComputePassEncoderRelease(pass);
  SubmitAndWait(encoder);

  const uint8_t* data = MapReadBuffer(storage, 8);
  uint32_t subgroup_size = 0;
  uint32_t num_subgroups = 0;
  std::memcpy(&subgroup_size, data, 4);
  std::memcpy(&num_subgroups, data + 4, 4);
  std::printf("  subgroupSize=%u numSubgroups=%u\n", subgroup_size,
              num_subgroups);
  bool size_power_of_two =
      subgroup_size >= 1 && (subgroup_size & (subgroup_size - 1)) == 0;
  CHECK(size_power_of_two && num_subgroups >= 1,
        "Subgroups: subgroup size is a power of two");
  wgpuBufferUnmap(storage);
  wgpuBufferRelease(storage);
  wgpuBindGroupRelease(group);
  wgpuBindGroupLayoutRelease(layout);
  wgpuComputePipelineRelease(pipeline);
  wgpuShaderModuleRelease(module);
}

// ---------------------------------------------------------------------------
// Depth32FloatStencil8: depth-rendering into a 32-bit float + stencil target.
// ---------------------------------------------------------------------------
void TestDepth32FloatStencil8() {
  WGPUTexture texture = CreateTexture(
      WGPUTextureUsage_RenderAttachment, WGPUTextureFormat_Depth32FloatStencil8,
      64, 64);
  CHECK(texture != nullptr, "Depth32FloatStencil8: texture creation");

  WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);
  WGPURenderPassDepthStencilAttachment depth_attachment = {};
  depth_attachment.view = view;
  depth_attachment.depthLoadOp = WGPULoadOp_Clear;
  depth_attachment.depthStoreOp = WGPUStoreOp_Store;
  depth_attachment.depthClearValue = 1.0f;
  depth_attachment.stencilLoadOp = WGPULoadOp_Clear;
  depth_attachment.stencilStoreOp = WGPUStoreOp_Store;
  depth_attachment.stencilClearValue = 0;
  WGPURenderPassDescriptor pass_descriptor = {};
  pass_descriptor.depthStencilAttachment = &depth_attachment;

  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, nullptr);
  WGPURenderPassEncoder pass =
      wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);
  SubmitAndWait(encoder);
  CHECK(true, "Depth32FloatStencil8: depth pass executes");
  wgpuTextureViewRelease(view);
  wgpuTextureRelease(texture);
}

// ---------------------------------------------------------------------------
// Texture compression: create compressed textures and upload one block.
// ---------------------------------------------------------------------------
void TestTextureCompressionBC() {
  WGPUTexture texture = CreateTexture(
      WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
      WGPUTextureFormat_BC1RGBAUnorm, 64, 64);
  CHECK(texture != nullptr, "TextureCompressionBC: BC1 texture creation");

  if (texture) {
    // Fill the whole texture with the same 4x4 BC1 block: color0 = 0xF800
    // (pure red), color1 = black, all texel indices -> color0.
    static const uint8_t block[8] = {0x00, 0xF8, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> blocks(16 * 16 * 8);
    for (size_t i = 0; i < 16 * 16; ++i)
      std::memcpy(blocks.data() + i * 8, block, 8);
    WGPUTexelCopyTextureInfo destination = {};
    destination.texture = texture;
    WGPUTexelCopyBufferLayout layout = {};
    layout.bytesPerRow = 16 * 8;
    WGPUExtent3D extent = {64, 64, 1};
    wgpuQueueWriteTexture(g_queue, &destination, blocks.data(), blocks.size(),
                          &layout, &extent);
    CHECK(true, "TextureCompressionBC: block upload");

    // Sample the compressed texture and verify the decoded texel color.
    // color0 = 0xF800 (pure red), color1 = black, all indices -> color0.
    WGPUShaderModule vertex = CreateShaderModule(kFullscreenVertSpv,
                                                 kFullscreenVertSpvSize);
    WGPUShaderModule fragment = CreateShaderModule(kTexSampleFragSpv,
                                                   kTexSampleFragSpvSize);
    WGPUVertexState vertex_state = {};
    vertex_state.module = vertex;
    WGPUColorTargetState target = {};
    target.format = WGPUTextureFormat_RGBA8Unorm;
    target.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState fragment_state = {};
    fragment_state.module = fragment;
    fragment_state.targetCount = 1;
    fragment_state.targets = &target;
    WGPURenderPipelineDescriptor pipeline_descriptor = {};
    pipeline_descriptor.vertex = vertex_state;
    pipeline_descriptor.fragment = &fragment_state;
    pipeline_descriptor.primitive.topology =
        WGPUPrimitiveTopology_TriangleList;
    pipeline_descriptor.primitive.frontFace = WGPUFrontFace_CCW;
    pipeline_descriptor.multisample.count = 1;
    WGPURenderPipeline pipeline =
        wgpuDeviceCreateRenderPipeline(g_device, &pipeline_descriptor);

    WGPUSamplerDescriptor sampler_descriptor = {};
    sampler_descriptor.magFilter = WGPUFilterMode_Nearest;
    sampler_descriptor.minFilter = WGPUFilterMode_Nearest;
    sampler_descriptor.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    WGPUSampler sampler =
        wgpuDeviceCreateSampler(g_device, &sampler_descriptor);
    WGPUTextureView bc_view = wgpuTextureCreateView(texture, nullptr);

    WGPUBindGroupLayout bgl =
        wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
    WGPUBindGroupEntry entries[2] = {};
    entries[0].binding = 0;
    entries[0].textureView = bc_view;
    entries[1].binding = 1;
    entries[1].sampler = sampler;
    WGPUBindGroupDescriptor group_descriptor = {};
    group_descriptor.layout = bgl;
    group_descriptor.entryCount = 2;
    group_descriptor.entries = entries;
    WGPUBindGroup group =
        wgpuDeviceCreateBindGroup(g_device, &group_descriptor);

    WGPUTexture color = CreateTexture(
        WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
        WGPUTextureFormat_RGBA8Unorm, 64, 64);
    WGPUTextureView color_view = wgpuTextureCreateView(color, nullptr);
    WGPURenderPassColorAttachment attachment = {};
    attachment.view = color_view;
    attachment.loadOp = WGPULoadOp_Clear;
    attachment.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor pass_descriptor = {};
    pass_descriptor.colorAttachmentCount = 1;
    pass_descriptor.colorAttachments = &attachment;

    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(g_device, nullptr);
    WGPURenderPassEncoder pass =
        wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, group, 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    SubmitAndWait(encoder);

    WGPUBuffer readback = CreateBuffer(WGPUBufferUsage_MapRead |
                                           WGPUBufferUsage_CopyDst,
                                       64 * 64 * 4);
    WGPUTexelCopyTextureInfo source = {};
    source.texture = color;
    WGPUTexelCopyBufferInfo buffer_destination = {};
    buffer_destination.layout.bytesPerRow = 64 * 4;
    buffer_destination.buffer = readback;
    WGPUExtent3D size = {64, 64, 1};
    WGPUCommandEncoder copy_encoder =
        wgpuDeviceCreateCommandEncoder(g_device, nullptr);
    wgpuCommandEncoderCopyTextureToBuffer(copy_encoder, &source,
                                          &buffer_destination, &size);
    SubmitAndWait(copy_encoder);

    const uint8_t* pixels = MapReadBuffer(readback, 64 * 64 * 4);
    const uint8_t* center = pixels + (32 * 64 * 4) + (32 * 4);
    CHECK(center[0] > 200 && center[1] < 50,
          "TextureCompressionBC: sampled BC1 texel decodes to red");
    wgpuBufferUnmap(readback);
    wgpuBufferRelease(readback);
    wgpuTextureViewRelease(color_view);
    wgpuTextureRelease(color);
    wgpuBindGroupRelease(group);
    wgpuBindGroupLayoutRelease(bgl);
    wgpuTextureViewRelease(bc_view);
    wgpuSamplerRelease(sampler);
    wgpuRenderPipelineRelease(pipeline);
    wgpuShaderModuleRelease(vertex);
    wgpuShaderModuleRelease(fragment);
    wgpuTextureRelease(texture);
  }
}

// ---------------------------------------------------------------------------
// Float32Blendable: blends on a 32-bit float attachment and reads the exact
// float result back.
// ---------------------------------------------------------------------------
void TestFloat32BlendablePixel() {
  WGPUShaderModule vertex = CreateShaderModule(kFullscreenVertSpv,
                                               kFullscreenVertSpvSize);
  WGPUShaderModule fragment = CreateShaderModule(kSolidRedFragSpv,
                                                 kSolidRedFragSpvSize);
  WGPUVertexState vertex_state = {};
  vertex_state.module = vertex;
  WGPUBlendState blend = {};
  blend.color.srcFactor = WGPUBlendFactor_One;
  blend.color.dstFactor = WGPUBlendFactor_One;
  blend.color.operation = WGPUBlendOperation_Add;
  blend.alpha.srcFactor = WGPUBlendFactor_One;
  blend.alpha.dstFactor = WGPUBlendFactor_Zero;
  blend.alpha.operation = WGPUBlendOperation_Add;
  WGPUColorTargetState target = {};
  target.format = WGPUTextureFormat_RGBA32Float;
  target.blend = &blend;
  target.writeMask = WGPUColorWriteMask_All;
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
      wgpuDeviceCreateRenderPipeline(g_device, &descriptor);
  CHECK(pipeline != nullptr, "Float32Blendable: blended pipeline creation");

  if (pipeline) {
    WGPUTexture texture = CreateTexture(
        WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
        WGPUTextureFormat_RGBA32Float, 16, 16);
    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);
    WGPURenderPassColorAttachment attachment = {};
    attachment.view = view;
    attachment.loadOp = WGPULoadOp_Clear;
    attachment.storeOp = WGPUStoreOp_Store;
    attachment.clearValue = {0.0, 0.0, 0.0, 0.0};
    WGPURenderPassDescriptor pass_descriptor = {};
    pass_descriptor.colorAttachmentCount = 1;
    pass_descriptor.colorAttachments = &attachment;

    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(g_device, nullptr);
    WGPURenderPassEncoder pass =
        wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    SubmitAndWait(encoder);

    WGPUBuffer readback = CreateBuffer(WGPUBufferUsage_MapRead |
                                           WGPUBufferUsage_CopyDst,
                                       16 * 16 * 16);
    WGPUTexelCopyTextureInfo source = {};
    source.texture = texture;
    WGPUTexelCopyBufferInfo destination = {};
    destination.layout.bytesPerRow = 16 * 16;
    destination.buffer = readback;
    WGPUExtent3D size = {16, 16, 1};
    WGPUCommandEncoder copy_encoder =
        wgpuDeviceCreateCommandEncoder(g_device, nullptr);
    wgpuCommandEncoderCopyTextureToBuffer(copy_encoder, &source, &destination,
                                          &size);
    SubmitAndWait(copy_encoder);

    const uint8_t* data = MapReadBuffer(readback, 16 * 16 * 16);
    const float* pixel = reinterpret_cast<const float*>(
        data + (8 * 16 * 16) + (8 * 16));
    CHECK(std::fabs(pixel[0] - 0.25f) < 0.01f &&
              std::fabs(pixel[1] - 0.5f) < 0.01f &&
              std::fabs(pixel[2] - 0.75f) < 0.01f,
          "Float32Blendable: blended float values are exact");
    wgpuBufferUnmap(readback);
    wgpuBufferRelease(readback);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(texture);
  }
  wgpuShaderModuleRelease(vertex);
  wgpuShaderModuleRelease(fragment);
}

// ---------------------------------------------------------------------------
// ClipDistances: a vertex shader using gl_ClipDistance renders through.
// ---------------------------------------------------------------------------
void TestClipDistances() {
  WGPUShaderModule vertex = CreateShaderModule(kClipVertSpv, kClipVertSpvSize);
  WGPUShaderModule fragment = CreateShaderModule(kClipFragSpv,
                                                 kClipFragSpvSize);
  WGPUVertexState vertex_state = {};
  vertex_state.module = vertex;
  WGPUColorTargetState target = {};
  target.format = WGPUTextureFormat_RGBA8Unorm;
  target.writeMask = WGPUColorWriteMask_All;
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
      wgpuDeviceCreateRenderPipeline(g_device, &descriptor);
  CHECK(pipeline != nullptr, "ClipDistances: clip-distance pipeline creation");

  if (pipeline) {
    WGPUTexture texture = CreateTexture(
        WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
        WGPUTextureFormat_RGBA8Unorm, 64, 64);
    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);
    WGPURenderPassColorAttachment attachment = {};
    attachment.view = view;
    attachment.loadOp = WGPULoadOp_Clear;
    attachment.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor pass_descriptor = {};
    pass_descriptor.colorAttachmentCount = 1;
    pass_descriptor.colorAttachments = &attachment;

    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(g_device, nullptr);
    WGPURenderPassEncoder pass =
        wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    SubmitAndWait(encoder);

    WGPUBuffer readback = CreateBuffer(WGPUBufferUsage_MapRead |
                                           WGPUBufferUsage_CopyDst,
                                       64 * 64 * 4);
    WGPUTexelCopyTextureInfo source = {};
    source.texture = texture;
    WGPUTexelCopyBufferInfo destination = {};
    destination.layout.bytesPerRow = 64 * 4;
    destination.buffer = readback;
    WGPUExtent3D size = {64, 64, 1};
    WGPUCommandEncoder copy_encoder =
        wgpuDeviceCreateCommandEncoder(g_device, nullptr);
    wgpuCommandEncoderCopyTextureToBuffer(copy_encoder, &source, &destination,
                                          &size);
    SubmitAndWait(copy_encoder);

    const uint8_t* pixels = MapReadBuffer(readback, 64 * 64 * 4);
    const uint8_t* center = pixels + (32 * 64 * 4) + (32 * 4);
    CHECK(center[1] > 200 && center[0] < 50,
          "ClipDistances: clipped triangle renders green");
    wgpuBufferUnmap(readback);
    wgpuBufferRelease(readback);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(texture);
  }
  wgpuShaderModuleRelease(vertex);
  wgpuShaderModuleRelease(fragment);
}

// ---------------------------------------------------------------------------
// RG11B10UfloatRenderable / BGRA8UnormStorage / Float32Blendable: creation
// and renderability checks.
// ---------------------------------------------------------------------------
void TestFormatFeatures() {
  WGPUTexture rg11b10 = CreateTexture(WGPUTextureUsage_RenderAttachment,
                                      WGPUTextureFormat_RG11B10Ufloat, 64, 64);
  CHECK(rg11b10 != nullptr, "RG11B10UfloatRenderable: render target creation");
  if (rg11b10) {
    WGPUTextureView view = wgpuTextureCreateView(rg11b10, nullptr);
    WGPURenderPassColorAttachment attachment = {};
    attachment.view = view;
    attachment.loadOp = WGPULoadOp_Clear;
    attachment.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor pass_descriptor = {};
    pass_descriptor.colorAttachmentCount = 1;
    pass_descriptor.colorAttachments = &attachment;
    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(g_device, nullptr);
    WGPURenderPassEncoder pass =
        wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    SubmitAndWait(encoder);
    CHECK(true, "RG11B10UfloatRenderable: clear pass executes");
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(rg11b10);
  }

  WGPUTexture bgra_storage = CreateTexture(
      WGPUTextureUsage_StorageBinding | WGPUTextureUsage_TextureBinding,
      WGPUTextureFormat_BGRA8Unorm, 64, 64);
  CHECK(bgra_storage != nullptr, "BGRA8UnormStorage: storage texture creation");
  if (bgra_storage)
    wgpuTextureRelease(bgra_storage);

  WGPUTexture rgba32 = CreateTexture(
      WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
      WGPUTextureFormat_RGBA32Float, 16, 16);
  if (rgba32) {
    WGPUTextureView view = wgpuTextureCreateView(rgba32, nullptr);
    WGPURenderPassColorAttachment attachment = {};
    attachment.view = view;
    attachment.loadOp = WGPULoadOp_Clear;
    attachment.storeOp = WGPUStoreOp_Store;
    attachment.clearValue = {0.0, 0.0, 0.0, 0.0};
    WGPURenderPassDescriptor pass_descriptor = {};
    pass_descriptor.colorAttachmentCount = 1;
    pass_descriptor.colorAttachments = &attachment;
    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(g_device, nullptr);
    WGPURenderPassEncoder pass =
        wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    SubmitAndWait(encoder);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(rgba32);
    CHECK(true, "Float32Blendable: RGBA32Float attachment clears");
  }
}

// ---------------------------------------------------------------------------
// Float32Filterable: linear sampling of an R32Float texture through separate
// texture/sampler bindings.
// ---------------------------------------------------------------------------
void TestFloat32Filterable() {
  WGPUShaderModule vertex = CreateShaderModule(kFullscreenVertSpv,
                                               kFullscreenVertSpvSize);
  WGPUShaderModule fragment = CreateShaderModule(kTexSampleFragSpv,
                                                 kTexSampleFragSpvSize);
  WGPUVertexState vertex_state = {};
  vertex_state.module = vertex;
  WGPUColorTargetState target = {};
  target.format = WGPUTextureFormat_RGBA8Unorm;
  target.writeMask = WGPUColorWriteMask_All;
  WGPUFragmentState fragment_state = {};
  fragment_state.module = fragment;
  fragment_state.targetCount = 1;
  fragment_state.targets = &target;
  WGPURenderPipelineDescriptor pipeline_descriptor = {};
  pipeline_descriptor.vertex = vertex_state;
  pipeline_descriptor.fragment = &fragment_state;
  pipeline_descriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pipeline_descriptor.primitive.frontFace = WGPUFrontFace_CCW;
  pipeline_descriptor.multisample.count = 1;
  WGPURenderPipeline pipeline =
      wgpuDeviceCreateRenderPipeline(g_device, &pipeline_descriptor);

  // 64x64 R32Float texture filled with 0.5.
  WGPUTexture texture = CreateTexture(
      WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
      WGPUTextureFormat_R32Float, 64, 64);
  std::vector<float> texels(64 * 64, 0.5f);
  WGPUTexelCopyTextureInfo destination = {};
  destination.texture = texture;
  WGPUTexelCopyBufferLayout data_layout = {};
  data_layout.bytesPerRow = 64 * 4;
  WGPUExtent3D extent = {64, 64, 1};
  wgpuQueueWriteTexture(g_queue, &destination, texels.data(),
                        texels.size() * 4, &data_layout, &extent);

  WGPUSamplerDescriptor sampler_descriptor = {};
  sampler_descriptor.magFilter = WGPUFilterMode_Linear;
  sampler_descriptor.minFilter = WGPUFilterMode_Linear;
  sampler_descriptor.mipmapFilter = WGPUMipmapFilterMode_Nearest;
  sampler_descriptor.addressModeU = WGPUAddressMode_ClampToEdge;
  sampler_descriptor.addressModeV = WGPUAddressMode_ClampToEdge;
  sampler_descriptor.addressModeW = WGPUAddressMode_ClampToEdge;
  WGPUSampler sampler = wgpuDeviceCreateSampler(g_device, &sampler_descriptor);
  WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);

  WGPUBindGroupLayout layout =
      wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
  WGPUBindGroupEntry entries[2] = {};
  entries[0].binding = 0;
  entries[0].textureView = view;
  entries[1].binding = 1;
  entries[1].sampler = sampler;
  WGPUBindGroupDescriptor group_descriptor = {};
  group_descriptor.layout = layout;
  group_descriptor.entryCount = 2;
  group_descriptor.entries = entries;
  WGPUBindGroup group = wgpuDeviceCreateBindGroup(g_device, &group_descriptor);

  WGPUTexture color = CreateTexture(
      WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
      WGPUTextureFormat_RGBA8Unorm, 64, 64);
  WGPUTextureView color_view = wgpuTextureCreateView(color, nullptr);
  WGPURenderPassColorAttachment attachment = {};
  attachment.view = color_view;
  attachment.loadOp = WGPULoadOp_Clear;
  attachment.storeOp = WGPUStoreOp_Store;
  WGPURenderPassDescriptor pass_descriptor = {};
  pass_descriptor.colorAttachmentCount = 1;
  pass_descriptor.colorAttachments = &attachment;

  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, nullptr);
  WGPURenderPassEncoder pass =
      wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
  wgpuRenderPassEncoderSetPipeline(pass, pipeline);
  wgpuRenderPassEncoderSetBindGroup(pass, 0, group, 0, nullptr);
  wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);
  SubmitAndWait(encoder);

  WGPUBuffer readback = CreateBuffer(WGPUBufferUsage_MapRead |
                                         WGPUBufferUsage_CopyDst,
                                     64 * 64 * 4);
  WGPUTexelCopyTextureInfo source = {};
  source.texture = color;
  WGPUTexelCopyBufferInfo buffer_destination = {};
  buffer_destination.layout.bytesPerRow = 64 * 4;
  buffer_destination.buffer = readback;
  WGPUExtent3D size = {64, 64, 1};
  WGPUCommandEncoder copy_encoder =
      wgpuDeviceCreateCommandEncoder(g_device, nullptr);
  wgpuCommandEncoderCopyTextureToBuffer(copy_encoder, &source,
                                        &buffer_destination, &size);
  SubmitAndWait(copy_encoder);

  const uint8_t* pixels = MapReadBuffer(readback, 64 * 64 * 4);
  const uint8_t* center = pixels + (32 * 64 * 4) + (32 * 4);
  std::printf("  f32 filterable pixel = (%d, %d, %d, %d)\n", (int)center[0],
              (int)center[1], (int)center[2], (int)center[3]);
  CHECK(std::abs(static_cast<int>(center[0]) - 128) < 2,
        "Float32Filterable: linear-sampled R32Float reads 0.5");
  wgpuBufferUnmap(readback);
  wgpuBufferRelease(readback);
  wgpuTextureViewRelease(color_view);
  wgpuTextureRelease(color);
  wgpuBindGroupRelease(group);
  wgpuBindGroupLayoutRelease(layout);
  wgpuTextureViewRelease(view);
  wgpuSamplerRelease(sampler);
  wgpuTextureRelease(texture);
  wgpuRenderPipelineRelease(pipeline);
  wgpuShaderModuleRelease(vertex);
  wgpuShaderModuleRelease(fragment);
}

// ---------------------------------------------------------------------------
// DepthClipControl: a pipeline with unclippedDepth must create successfully.
// ---------------------------------------------------------------------------
void TestDepthClipControl() {
  WGPUShaderModule vertex = CreateShaderModule(kFullscreenVertSpv,
                                               kFullscreenVertSpvSize);
  WGPUVertexState vertex_state = {};
  vertex_state.module = vertex;
  WGPURenderPipelineDescriptor descriptor = {};
  descriptor.vertex = vertex_state;
  descriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  descriptor.primitive.unclippedDepth = WGPUBool(true);
  descriptor.multisample.count = 1;
  WGPURenderPipeline pipeline =
      wgpuDeviceCreateRenderPipeline(g_device, &descriptor);
  CHECK(pipeline != nullptr, "DepthClipControl: unclippedDepth pipeline");
  if (pipeline)
    wgpuRenderPipelineRelease(pipeline);
  wgpuShaderModuleRelease(vertex);
}

// ---------------------------------------------------------------------------
// TextureComponentSwizzle: sampling through a swizzled view remaps channels.
// ---------------------------------------------------------------------------
void TestTextureComponentSwizzle() {
  WGPUShaderModule vertex = CreateShaderModule(kFullscreenVertSpv,
                                               kFullscreenVertSpvSize);
  WGPUShaderModule fragment = CreateShaderModule(kTexSampleFragSpv,
                                                 kTexSampleFragSpvSize);
  WGPUVertexState vertex_state = {};
  vertex_state.module = vertex;
  WGPUColorTargetState target = {};
  target.format = WGPUTextureFormat_RGBA8Unorm;
  target.writeMask = WGPUColorWriteMask_All;
  WGPUFragmentState fragment_state = {};
  fragment_state.module = fragment;
  fragment_state.targetCount = 1;
  fragment_state.targets = &target;
  WGPURenderPipelineDescriptor pipeline_descriptor = {};
  pipeline_descriptor.vertex = vertex_state;
  pipeline_descriptor.fragment = &fragment_state;
  pipeline_descriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pipeline_descriptor.primitive.frontFace = WGPUFrontFace_CCW;
  pipeline_descriptor.multisample.count = 1;
  WGPURenderPipeline pipeline =
      wgpuDeviceCreateRenderPipeline(g_device, &pipeline_descriptor);

  // Red texture; swizzled view maps sampled r <- a, a <- r.
  WGPUTexture texture = CreateTexture(
      WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
      WGPUTextureFormat_RGBA8Unorm, 64, 64);
  static const uint8_t texel[4] = {255, 0, 0, 128};
  std::vector<uint8_t> texels(64 * 64 * 4);
  for (size_t i = 0; i < 64 * 64; ++i)
    std::memcpy(texels.data() + i * 4, texel, 4);
  WGPUTexelCopyTextureInfo destination = {};
  destination.texture = texture;
  WGPUTexelCopyBufferLayout data_layout = {};
  data_layout.bytesPerRow = 64 * 4;
  WGPUExtent3D extent = {64, 64, 1};
  wgpuQueueWriteTexture(g_queue, &destination, texels.data(), texels.size(),
                        &data_layout, &extent);

  WGPUTextureViewDescriptor swizzle_view = {};
  WGPUTextureComponentSwizzleDescriptor swizzle = {};
  swizzle.chain.next = nullptr;
  swizzle.chain.sType = WGPUSType_TextureComponentSwizzleDescriptor;
  swizzle.swizzle.r = WGPUComponentSwizzle_A;
  swizzle.swizzle.g = WGPUComponentSwizzle_B;
  swizzle.swizzle.b = WGPUComponentSwizzle_G;
  swizzle.swizzle.a = WGPUComponentSwizzle_R;
  swizzle_view.nextInChain = &swizzle.chain;
  swizzle_view.format = WGPUTextureFormat_RGBA8Unorm;
  swizzle_view.dimension = WGPUTextureViewDimension_2D;
  swizzle_view.mipLevelCount = 1;
  swizzle_view.arrayLayerCount = 1;
  swizzle_view.aspect = WGPUTextureAspect_All;
  WGPUTextureView view = wgpuTextureCreateView(texture, &swizzle_view);

  WGPUSamplerDescriptor sampler_descriptor = {};
  sampler_descriptor.magFilter = WGPUFilterMode_Nearest;
  sampler_descriptor.minFilter = WGPUFilterMode_Nearest;
  sampler_descriptor.mipmapFilter = WGPUMipmapFilterMode_Nearest;
  WGPUSampler sampler = wgpuDeviceCreateSampler(g_device, &sampler_descriptor);

  WGPUBindGroupLayout layout =
      wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
  WGPUBindGroupEntry entries[2] = {};
  entries[0].binding = 0;
  entries[0].textureView = view;
  entries[1].binding = 1;
  entries[1].sampler = sampler;
  WGPUBindGroupDescriptor group_descriptor = {};
  group_descriptor.layout = layout;
  group_descriptor.entryCount = 2;
  group_descriptor.entries = entries;
  WGPUBindGroup group = wgpuDeviceCreateBindGroup(g_device, &group_descriptor);

  WGPUTexture color = CreateTexture(
      WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
      WGPUTextureFormat_RGBA8Unorm, 64, 64);
  WGPUTextureView color_view = wgpuTextureCreateView(color, nullptr);
  WGPURenderPassColorAttachment attachment = {};
  attachment.view = color_view;
  attachment.loadOp = WGPULoadOp_Clear;
  attachment.storeOp = WGPUStoreOp_Store;
  WGPURenderPassDescriptor pass_descriptor = {};
  pass_descriptor.colorAttachmentCount = 1;
  pass_descriptor.colorAttachments = &attachment;

  WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_device, nullptr);
  WGPURenderPassEncoder pass =
      wgpuCommandEncoderBeginRenderPass(encoder, &pass_descriptor);
  wgpuRenderPassEncoderSetPipeline(pass, pipeline);
  wgpuRenderPassEncoderSetBindGroup(pass, 0, group, 0, nullptr);
  wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);
  SubmitAndWait(encoder);

  WGPUBuffer readback = CreateBuffer(WGPUBufferUsage_MapRead |
                                         WGPUBufferUsage_CopyDst,
                                     64 * 64 * 4);
  WGPUTexelCopyTextureInfo source = {};
  source.texture = color;
  WGPUTexelCopyBufferInfo buffer_destination = {};
  buffer_destination.layout.bytesPerRow = 64 * 4;
  buffer_destination.buffer = readback;
  WGPUExtent3D size = {64, 64, 1};
  WGPUCommandEncoder copy_encoder =
      wgpuDeviceCreateCommandEncoder(g_device, nullptr);
  wgpuCommandEncoderCopyTextureToBuffer(copy_encoder, &source,
                                        &buffer_destination, &size);
  SubmitAndWait(copy_encoder);

  const uint8_t* pixels = MapReadBuffer(readback, 64 * 64 * 4);
  const uint8_t* center = pixels + (32 * 64 * 4) + (32 * 4);
  // Sampled r = texture alpha (128), sampled a = texture r (255).
  std::printf("  swizzle pixel = (%d, %d, %d, %d)\n", (int)center[0],
              (int)center[1], (int)center[2], (int)center[3]);
  CHECK(std::abs(static_cast<int>(center[0]) - 128) < 2 &&
            center[3] == 255,
        "TextureComponentSwizzle: channels remapped by the view");
  wgpuBufferUnmap(readback);
  wgpuBufferRelease(readback);
  wgpuTextureViewRelease(color_view);
  wgpuTextureRelease(color);
  wgpuBindGroupRelease(group);
  wgpuBindGroupLayoutRelease(layout);
  wgpuSamplerRelease(sampler);
  wgpuTextureViewRelease(view);
  wgpuTextureRelease(texture);
  wgpuRenderPipelineRelease(pipeline);
  wgpuShaderModuleRelease(vertex);
  wgpuShaderModuleRelease(fragment);
}

}  // namespace

namespace {

struct AdapterCapture {
  WGPUAdapter adapter = nullptr;
};

struct DeviceCapture {
  WGPUDevice device = nullptr;
};

}  // namespace

int main() {
  WGPUInstance instance = wgpuCreateInstance(nullptr);
  g_instance = instance;

  AdapterCapture adapter_capture;
  WGPURequestAdapterCallbackInfo adapter_info = {};
  adapter_info.mode = WGPUCallbackMode_AllowSpontaneous;
  adapter_info.callback =
      [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
         WGPUStringView, void* userdata1, void*) {
        static_cast<AdapterCapture*>(userdata1)->adapter =
            status == WGPURequestAdapterStatus_Success ? adapter : nullptr;
      };
  adapter_info.userdata1 = &adapter_capture;
  wgpuInstanceRequestAdapter(instance, nullptr, adapter_info);

  if (!adapter_capture.adapter) {
    std::printf("FAIL: no adapter\n");
    return 1;
  }

  // Report every feature the adapter supports.
  WGPUSupportedFeatures supported = {};
  wgpuAdapterGetFeatures(adapter_capture.adapter, &supported);
  std::printf("Adapter features (%u):\n",
              static_cast<unsigned>(supported.featureCount));
  for (size_t i = 0; i < supported.featureCount; ++i)
    std::printf("  - WGPUFeatureName(%u)\n",
                static_cast<unsigned>(supported.features[i]));

  // Request everything the adapter supports.
  DeviceCapture device_capture;
  WGPUDeviceDescriptor device_descriptor = {};
  device_descriptor.requiredFeatureCount = supported.featureCount;
  device_descriptor.requiredFeatures = supported.features;
  WGPURequestDeviceCallbackInfo device_info = {};
  device_info.mode = WGPUCallbackMode_AllowSpontaneous;
  device_info.callback =
      [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView,
         void* userdata1, void*) {
        static_cast<DeviceCapture*>(userdata1)->device =
            status == WGPURequestDeviceStatus_Success ? device : nullptr;
      };
  device_info.userdata1 = &device_capture;
  wgpuAdapterRequestDevice(adapter_capture.adapter, &device_descriptor,
                           device_info);

  if (!device_capture.device) {
    std::printf("FAIL: no device\n");
    return 1;
  }
  g_device = device_capture.device;
  g_queue = wgpuDeviceGetQueue(g_device);

  // Verify the device really enabled everything requested.
  bool all_enabled = true;
  for (size_t i = 0; i < supported.featureCount; ++i) {
    if (!wgpuDeviceHasFeature(g_device, supported.features[i]))
      all_enabled = false;
  }
  CHECK(all_enabled, "device enables all requested adapter features");

  // Run the feature conformance tests, each gated on its feature.
  if (wgpuDeviceHasFeature(g_device, WGPUFeatureName_DualSourceBlending))
    TestDualSourceBlending();
  if (wgpuDeviceHasFeature(g_device, WGPUFeatureName_TimestampQuery))
    TestTimestampQuery();
  if (wgpuDeviceHasFeature(g_device, WGPUFeatureName_IndirectFirstInstance))
    TestIndirectFirstInstance();
  if (wgpuDeviceHasFeature(g_device, WGPUFeatureName_ShaderF16))
    TestShaderF16();
  if (wgpuDeviceHasFeature(g_device, WGPUFeatureName_Subgroups))
    TestSubgroups();
  if (wgpuDeviceHasFeature(g_device, WGPUFeatureName_Depth32FloatStencil8))
    TestDepth32FloatStencil8();
  if (wgpuDeviceHasFeature(g_device, WGPUFeatureName_TextureCompressionBC))
    TestTextureCompressionBC();
  if (wgpuDeviceHasFeature(g_device, WGPUFeatureName_Float32Blendable))
    TestFloat32BlendablePixel();
  if (wgpuDeviceHasFeature(g_device, WGPUFeatureName_ClipDistances))
    TestClipDistances();
  TestFormatFeatures();
  if (wgpuDeviceHasFeature(g_device, WGPUFeatureName_Float32Filterable))
    TestFloat32Filterable();
  if (wgpuDeviceHasFeature(g_device, WGPUFeatureName_DepthClipControl))
    TestDepthClipControl();
  if (wgpuDeviceHasFeature(g_device, WGPUFeatureName_TextureComponentSwizzle))
    TestTextureComponentSwizzle();

  wgpuQueueRelease(g_queue);
  wgpuDeviceRelease(g_device);
  wgpuAdapterRelease(adapter_capture.adapter);
  wgpuInstanceRelease(instance);

  std::printf(g_failures ? "\n%d test(s) FAILED\n" : "\nAll tests passed\n",
              g_failures);
  return g_failures ? 1 : 0;
}
