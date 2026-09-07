// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPURenderPassEncoderImpl handle;
// define it here so gfx::RenderPassEncoder can derive from it, making the
// generated bridge's static_cast<gfx::RenderPassEncoder*>(handle) a valid
// base->derived downcast.
struct WGPURenderPassEncoderImpl {};

namespace gfx {

class CommandEncoder;
class Device;
class RenderPipeline;

class RenderPassEncoder : public WGPURenderPassEncoderImpl,
                          public RefCounted<RenderPassEncoder> {
 public:
  RenderPassEncoder(RefPtr<CommandEncoder> encoder,
                    WGPURenderPassDescriptor const * descriptor);
  ~RenderPassEncoder();

  void SetPipeline(WGPURenderPipeline pipeline);
  void SetBindGroup(uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets);
  void SetImmediates(uint32_t offset, void const * data, size_t size);
  void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
  void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance);
  void DrawIndirect(WGPUBuffer indirectBuffer, uint64_t indirectOffset);
  void DrawIndexedIndirect(WGPUBuffer indirectBuffer, uint64_t indirectOffset);
  void ExecuteBundles(size_t bundleCount, WGPURenderBundle const * bundles);
  void InsertDebugMarker(WGPUStringView markerLabel);
  void PopDebugGroup();
  void PushDebugGroup(WGPUStringView groupLabel);
  void SetStencilReference(uint32_t reference);
  void SetBlendConstant(WGPUColor const * color);
  void SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth);
  void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
  void SetVertexBuffer(uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size);
  void SetIndexBuffer(WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size);
  void BeginOcclusionQuery(uint32_t queryIndex);
  void EndOcclusionQuery();
  void End();
  void SetLabel(WGPUStringView label);

 private:
  RefPtr<CommandEncoder> encoder_;
  VkCommandBuffer buffer_ = VK_NULL_HANDLE;
  RenderPipeline* current_pipeline_ = nullptr;
  VkRenderPass render_pass_ = VK_NULL_HANDLE;
  bool ended_ = false;
  QuerySet* occlusion_query_set_ = nullptr;
  bool occlusion_reset_done_ = false;
  uint32_t occlusion_query_index_ = 0;
  const WGPUPassTimestampWrites* timestamp_writes_ = nullptr;
  struct ResolveOp {
    VkImage src;
    VkImage dst;
    VkImageResolve region;
  };
  std::vector<ResolveOp> resolve_ops_;
};

}  // namespace gfx
