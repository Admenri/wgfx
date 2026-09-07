// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_compute_pass_encoder.h"

#include "gfx/common/ref_holder.h"
#include "gfx/common/vulkan_conversions.h"
#include "gfx/gfx_bind_group.h"
#include "gfx/gfx_buffer.h"
#include "gfx/gfx_command_encoder.h"
#include "gfx/gfx_compute_pipeline.h"
#include "gfx/gfx_query_set.h"
#include "gfx/common/ref_holder.h"
#include "gfx/gfx_device.h"

namespace gfx {

ComputePassEncoder::ComputePassEncoder(
    RefPtr<CommandEncoder> encoder,
    WGPUComputePassDescriptor const * descriptor)
    : encoder_(std::move(encoder)),
      buffer_(encoder_->GetVkCommandBuffer()) {
  if (descriptor && descriptor->timestampWrites &&
      descriptor->timestampWrites->querySet) {
    timestamp_writes_ = descriptor->timestampWrites;
    encoder_->KeepResource(RefHolder::Of(
        static_cast<gfx::QuerySet*>(timestamp_writes_->querySet)));
    if (timestamp_writes_->beginningOfPassWriteIndex !=
        WGPU_QUERY_SET_INDEX_UNDEFINED) {
      encoder_->RecordTimestamp(timestamp_writes_->querySet,
                                timestamp_writes_->beginningOfPassWriteIndex,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    }
  }
}

ComputePassEncoder::~ComputePassEncoder() = default;

void ComputePassEncoder::InsertDebugMarker(WGPUStringView markerLabel) {}

void ComputePassEncoder::PopDebugGroup() {}

void ComputePassEncoder::PushDebugGroup(WGPUStringView groupLabel) {}

void ComputePassEncoder::SetPipeline(WGPUComputePipeline pipeline) {
  current_pipeline_ = static_cast<ComputePipeline*>(pipeline);
  encoder_->KeepResource(RefHolder::Of(current_pipeline_));
  vkCmdBindPipeline(buffer_, VK_PIPELINE_BIND_POINT_COMPUTE,
                    current_pipeline_->GetVkPipeline());
}

void ComputePassEncoder::SetBindGroup(uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) {
  if (!current_pipeline_)
    return;
  auto* bind_group = static_cast<gfx::BindGroup*>(group);
  encoder_->KeepResource(RefHolder::Of(bind_group));
  VkDescriptorSet set = bind_group->GetVkDescriptorSet();
  vkCmdBindDescriptorSets(
      buffer_, VK_PIPELINE_BIND_POINT_COMPUTE,
      current_pipeline_->GetVkPipelineLayout(), groupIndex, 1, &set,
      static_cast<uint32_t>(dynamicOffsetCount), dynamicOffsets);
}

void ComputePassEncoder::SetImmediates(uint32_t offset, void const * data, size_t size) {
  if (!current_pipeline_)
    return;
  vkCmdPushConstants(buffer_, current_pipeline_->GetVkPipelineLayout(),
                     current_pipeline_->GetImmediateStages(), offset,
                     static_cast<uint32_t>(size), data);
}

void ComputePassEncoder::DispatchWorkgroups(uint32_t workgroupCountX, uint32_t workgroupCountY, uint32_t workgroupCountZ) {
  vkCmdDispatch(buffer_, workgroupCountX, workgroupCountY, workgroupCountZ);
}

void ComputePassEncoder::DispatchWorkgroupsIndirect(WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
  auto* buffer = static_cast<gfx::Buffer*>(indirectBuffer);
  encoder_->KeepResource(RefHolder::Of(buffer));
  vkCmdDispatchIndirect(buffer_, buffer->GetVkBuffer(),
                        static_cast<uint32_t>(indirectOffset));
}

void ComputePassEncoder::End() {
  ended_ = true;
  if (timestamp_writes_ &&
      timestamp_writes_->endOfPassWriteIndex !=
          WGPU_QUERY_SET_INDEX_UNDEFINED) {
    encoder_->RecordTimestamp(timestamp_writes_->querySet,
                              timestamp_writes_->endOfPassWriteIndex,
                              VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
  }
}

void ComputePassEncoder::SetLabel(WGPUStringView label) {}

}  // namespace gfx
