// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUComputePassEncoderImpl handle;
// define it here so gfx::ComputePassEncoder can derive from it, making the
// generated bridge's static_cast<gfx::ComputePassEncoder*>(handle) a valid
// base->derived downcast.
struct WGPUComputePassEncoderImpl {};

namespace gfx {

class CommandEncoder;
class ComputePipeline;

class ComputePassEncoder : public WGPUComputePassEncoderImpl,
                           public RefCounted<ComputePassEncoder> {
 public:
  ComputePassEncoder(RefPtr<CommandEncoder> encoder,
                     WGPUComputePassDescriptor const * descriptor);
  ~ComputePassEncoder();

  void InsertDebugMarker(WGPUStringView markerLabel);
  void PopDebugGroup();
  void PushDebugGroup(WGPUStringView groupLabel);
  void SetPipeline(WGPUComputePipeline pipeline);
  void SetBindGroup(uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets);
  void SetImmediates(uint32_t offset, void const * data, size_t size);
  void DispatchWorkgroups(uint32_t workgroupCountX, uint32_t workgroupCountY, uint32_t workgroupCountZ);
  void DispatchWorkgroupsIndirect(WGPUBuffer indirectBuffer, uint64_t indirectOffset);
  void End();
  void SetLabel(WGPUStringView label);

 private:
  RefPtr<CommandEncoder> encoder_;
  VkCommandBuffer buffer_ = VK_NULL_HANDLE;
  ComputePipeline* current_pipeline_ = nullptr;
  bool ended_ = false;
  const WGPUPassTimestampWrites* timestamp_writes_ = nullptr;
};

}  // namespace gfx
