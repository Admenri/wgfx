// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/ref_holder.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUCommandEncoderImpl handle;
// define it here so gfx::CommandEncoder can derive from it, making the
// generated bridge's static_cast<gfx::CommandEncoder*>(handle) a valid
// base->derived downcast.
struct WGPUCommandEncoderImpl {};

namespace gfx {

class CommandBuffer;
class ComputePassEncoder;
class Device;
class RenderPassEncoder;

class CommandEncoder : public WGPUCommandEncoderImpl,
                       public RefCounted<CommandEncoder> {
 public:
  explicit CommandEncoder(RefPtr<Device> device,
                          WGPUCommandEncoderDescriptor const * descriptor);
  ~CommandEncoder();

  VkCommandBuffer GetVkCommandBuffer() const { return buffer_; }
  Device* GetDevice() const { return device_.get(); }

  // Keeps a referenced object alive until the finished command buffer is
  // released after execution.
  void KeepResource(RefHolder holder);
  void KeepVulkanResource(std::function<void()> destroy);

  gfx::CommandBuffer* Finish(WGPUCommandBufferDescriptor const * descriptor);
  gfx::ComputePassEncoder* BeginComputePass(WGPUComputePassDescriptor const * descriptor);
  gfx::RenderPassEncoder* BeginRenderPass(WGPURenderPassDescriptor const * descriptor);
  void CopyBufferToBuffer(WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size);
  void CopyBufferToTexture(WGPUTexelCopyBufferInfo const * source, WGPUTexelCopyTextureInfo const * destination, WGPUExtent3D const * copySize);
  void CopyTextureToBuffer(WGPUTexelCopyTextureInfo const * source, WGPUTexelCopyBufferInfo const * destination, WGPUExtent3D const * copySize);
  void CopyTextureToTexture(WGPUTexelCopyTextureInfo const * source, WGPUTexelCopyTextureInfo const * destination, WGPUExtent3D const * copySize);
  void ClearBuffer(WGPUBuffer buffer, uint64_t offset, uint64_t size);
  void InsertDebugMarker(WGPUStringView markerLabel);
  void PopDebugGroup();
  void PushDebugGroup(WGPUStringView groupLabel);
  void ResolveQuerySet(WGPUQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGPUBuffer destination, uint64_t destinationOffset);
  void WriteTimestamp(WGPUQuerySet querySet, uint32_t queryIndex);
  void SetLabel(WGPUStringView label);

 private:
  RefPtr<Device> device_;
  VkCommandPool pool_ = VK_NULL_HANDLE;
  VkCommandBuffer buffer_ = VK_NULL_HANDLE;
  bool finished_ = false;
  std::vector<RefHolder> resources_;
  std::vector<std::function<void()>> vulkan_cleanup_;
  std::vector<QuerySet*> reset_query_pools_;
  bool IsQueryPoolReset(QuerySet* query_set) const;
};

}  // namespace gfx
