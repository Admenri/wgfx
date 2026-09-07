// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_render_bundle_encoder.h"

#include "gfx/common/vulkan_conversions.h"
#include "vk_mem_alloc.h"
#include "gfx/gfx_bind_group.h"
#include "gfx/gfx_buffer.h"
#include "gfx/common/ref_holder.h"
#include "gfx/gfx_device.h"
#include "gfx/gfx_render_bundle.h"
#include "gfx/gfx_render_pipeline.h"

namespace gfx {

RenderBundleEncoder::RenderBundleEncoder(
    RefPtr<Device> device,
    WGPURenderBundleEncoderDescriptor const * descriptor)
    : device_(std::move(device)) {
  // Compatibility render pass from the declared attachment formats (the
  // same contract as the executing render pass; pipelines recorded into
  // the bundle must be compatible with it).
  std::vector<VkAttachmentDescription> attachments;
  std::vector<VkAttachmentReference> color_refs;
  VkAttachmentReference depth_ref = {};
  bool has_depth = descriptor->depthStencilFormat !=
                   WGPUTextureFormat_Undefined;

  for (size_t i = 0; i < descriptor->colorFormatCount; ++i) {
    VkAttachmentDescription attachment = {};
    attachment.format =
        GetFormatInfo(descriptor->colorFormats[i]).vk_format;
    attachment.samples = static_cast<VkSampleCountFlagBits>(
        descriptor->sampleCount ? descriptor->sampleCount : 1);
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = Device::kImageLayout;
    attachment.finalLayout = Device::kImageLayout;
    color_refs.push_back({static_cast<uint32_t>(attachments.size()),
                          Device::kImageLayout});
    attachments.push_back(attachment);
  }
  if (has_depth) {
    VkAttachmentDescription attachment = {};
    attachment.format =
        GetFormatInfo(descriptor->depthStencilFormat).vk_format;
    attachment.samples = static_cast<VkSampleCountFlagBits>(
        descriptor->sampleCount ? descriptor->sampleCount : 1);
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.initialLayout = Device::kImageLayout;
    attachment.finalLayout = Device::kImageLayout;
    depth_ref = {static_cast<uint32_t>(attachments.size()),
                 Device::kImageLayout};
    attachments.push_back(attachment);
  }

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = static_cast<uint32_t>(color_refs.size());
  subpass.pColorAttachments = color_refs.data();
  subpass.pDepthStencilAttachment = has_depth ? &depth_ref : nullptr;

  VkRenderPassCreateInfo pass_info = {};
  pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
  pass_info.pAttachments = attachments.data();
  pass_info.subpassCount = 1;
  pass_info.pSubpasses = &subpass;
  vkCreateRenderPass(device_->GetVkDevice(), &pass_info, nullptr,
                     &render_pass_);

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

  // Begin the secondary command buffer in render-pass-continue mode so it
  // can be executed inside a compatible primary render pass.
  VkCommandBufferInheritanceInfo inheritance = {};
  inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
  inheritance.renderPass = render_pass_;
  inheritance.subpass = 0;

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
                     VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
  begin_info.pInheritanceInfo = &inheritance;
  vkBeginCommandBuffer(buffer_, &begin_info);
}

RenderBundleEncoder::~RenderBundleEncoder() {
  VkDevice vk_device = device_->GetVkDevice();
  if (render_pass_)
    vkDestroyRenderPass(vk_device, render_pass_, nullptr);
  if (pool_) {
    // Unfinished bundles never ended their command buffer; reset the pool
    // so destruction is legal.
    if (!finished_)
      vkResetCommandPool(vk_device, pool_, 0);
    vkDestroyCommandPool(vk_device, pool_, nullptr);
  }
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
  device_->SetObjectLabel(reinterpret_cast<uint64_t>(buffer_),
                          VK_OBJECT_TYPE_COMMAND_BUFFER,
                          descriptor ? descriptor->label : WGPUStringView{});
  vkEndCommandBuffer(buffer_);
  gfx::RenderBundle* bundle = ToAPIRef(new RenderBundle(device_, pool_, buffer_));
  bundle->AdoptResources(std::move(resources_));
  return bundle;
}

void RenderBundleEncoder::SetLabel(WGPUStringView label) {}

}  // namespace gfx
