// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_render_bundle_encoder.h"

#include "gfx/common/vulkan_conversions.h"
#include "vk_mem_alloc.h"
#include "gfx/gfx_bind_group.h"
#include "gfx/gfx_buffer.h"
#include "gfx/gfx_device.h"
#include "gfx/gfx_render_bundle.h"
#include "gfx/gfx_render_pipeline.h"

namespace gfx {

RenderBundleEncoder::RenderBundleEncoder(
    RefPtr<Device> device,
    WGPURenderBundleEncoderDescriptor const * descriptor)
    : device_(std::move(device)) {
  VkCommandPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex = device_->GetQueueFamilyIndex();
  vkCreateCommandPool(device_->GetVkDevice(), &pool_info, nullptr, &pool_);

  VkCommandBufferAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = pool_;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  alloc_info.commandBufferCount = 1;
  vkAllocateCommandBuffers(device_->GetVkDevice(), &alloc_info, &buffer_);
}

RenderBundleEncoder::~RenderBundleEncoder() {
  if (pool_ && !finished_)
    vkDestroyCommandPool(device_->GetVkDevice(), pool_, nullptr);
}

void RenderBundleEncoder::SetPipeline(WGPURenderPipeline pipeline) {
  // Render bundles execute inside an active render pass; the compatible
  // render pass is derived from the bundle encoder's declared formats.
  current_pipeline_ = static_cast<RenderPipeline*>(pipeline);
  vkCmdBindPipeline(buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    current_pipeline_->GetVkPipeline());
}

void RenderBundleEncoder::SetBindGroup(uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) {
  if (!current_pipeline_)
    return;
  auto* bind_group = static_cast<gfx::BindGroup*>(group);
  VkDescriptorSet set = bind_group->GetVkDescriptorSet();
  vkCmdBindDescriptorSets(
      buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
      current_pipeline_->GetVkPipelineLayout(), groupIndex, 1, &set,
      static_cast<uint32_t>(dynamicOffsetCount), dynamicOffsets);
}

void RenderBundleEncoder::SetImmediates(uint32_t offset, void const * data, size_t size) {
  if (!current_pipeline_)
    return;
  vkCmdPushConstants(buffer_, current_pipeline_->GetVkPipelineLayout(),
                     current_pipeline_->GetImmediateStages(), offset,
                     static_cast<uint32_t>(size), data);
}

void RenderBundleEncoder::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
  vkCmdDraw(buffer_, vertexCount, instanceCount, firstVertex, firstInstance);
}

void RenderBundleEncoder::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) {
  vkCmdDrawIndexed(buffer_, indexCount, instanceCount, firstIndex, baseVertex,
                   firstInstance);
}

void RenderBundleEncoder::DrawIndirect(WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
  auto* buffer = static_cast<gfx::Buffer*>(indirectBuffer);
  vkCmdDrawIndirect(buffer_, buffer->GetVkBuffer(),
                    static_cast<uint32_t>(indirectOffset), 1, 0);
}

void RenderBundleEncoder::DrawIndexedIndirect(WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
  auto* buffer = static_cast<gfx::Buffer*>(indirectBuffer);
  vkCmdDrawIndexedIndirect(buffer_, buffer->GetVkBuffer(),
                           static_cast<uint32_t>(indirectOffset), 1, 0);
}

void RenderBundleEncoder::InsertDebugMarker(WGPUStringView markerLabel) {}

void RenderBundleEncoder::PopDebugGroup() {}

void RenderBundleEncoder::PushDebugGroup(WGPUStringView groupLabel) {}

void RenderBundleEncoder::SetVertexBuffer(uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
  auto* vk_buffer = static_cast<gfx::Buffer*>(buffer);
  VkDeviceSize vk_offset = static_cast<VkDeviceSize>(offset);
  VkBuffer handle = vk_buffer->GetVkBuffer();
  vkCmdBindVertexBuffers(buffer_, slot, 1, &handle, &vk_offset);
}

void RenderBundleEncoder::SetIndexBuffer(WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) {
  auto* vk_buffer = static_cast<gfx::Buffer*>(buffer);
  vkCmdBindIndexBuffer(buffer_, vk_buffer->GetVkBuffer(),
                       static_cast<VkDeviceSize>(offset),
                       GetIndexType(format));
}

gfx::RenderBundle* RenderBundleEncoder::Finish(WGPURenderBundleDescriptor const * descriptor) {
  finished_ = true;
  gfx::RenderBundle* bundle = ToAPIRef(new RenderBundle(device_, pool_, buffer_));
  for (RefHolder& holder : resources_)
    bundle->KeepResource(std::move(holder));
  return bundle;
}

void RenderBundleEncoder::SetLabel(WGPUStringView label) {}

}  // namespace gfx
