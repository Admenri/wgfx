// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/common/ref_holder.h"
#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPURenderBundleEncoderImpl
// handle; define it here so gfx::RenderBundleEncoder can derive from it,
// making the generated bridge's static_cast<gfx::RenderBundleEncoder*>(
// handle) a valid base->derived downcast.
struct WGPURenderBundleEncoderImpl {};

namespace gfx {

class Device;
class RenderBundle;
class RenderPipeline;

class RenderBundleEncoder : public WGPURenderBundleEncoderImpl,
                            public RefCounted<RenderBundleEncoder> {
 public:
  RenderBundleEncoder(RefPtr<Device> device,
                      WGPURenderBundleEncoderDescriptor const * descriptor);
  ~RenderBundleEncoder();

  void SetPipeline(WGPURenderPipeline pipeline);
  void SetBindGroup(uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets);
  void SetImmediates(uint32_t offset, void const * data, size_t size);
  void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
  void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance);
  void DrawIndirect(WGPUBuffer indirectBuffer, uint64_t indirectOffset);
  void DrawIndexedIndirect(WGPUBuffer indirectBuffer, uint64_t indirectOffset);
  void InsertDebugMarker(WGPUStringView markerLabel);
  void PopDebugGroup();
  void PushDebugGroup(WGPUStringView groupLabel);
  void SetVertexBuffer(uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size);
  void SetIndexBuffer(WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size);
  gfx::RenderBundle* Finish(WGPURenderBundleDescriptor const * descriptor);
  void SetLabel(WGPUStringView label);

 private:
  RefPtr<Device> device_;
  VkCommandPool pool_ = VK_NULL_HANDLE;
  VkCommandBuffer buffer_ = VK_NULL_HANDLE;
  bool finished_ = false;
  RenderPipeline* current_pipeline_ = nullptr;
  // Compatibility render pass built from the encoder's declared formats;
  // secondary command buffers require it for render-pass-continue records.
  VkRenderPass render_pass_ = VK_NULL_HANDLE;
  std::vector<RefHolder> resources_;
};

}  // namespace gfx
