// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_render_pass_encoder.h"

#include "gfx/common/ref_holder.h"
#include "gfx/common/vulkan_conversions.h"
#include "vk_mem_alloc.h"
#include "gfx/gfx_bind_group.h"
#include "gfx/gfx_buffer.h"
#include "gfx/gfx_command_encoder.h"
#include "gfx/gfx_device.h"
#include "gfx/gfx_query_set.h"
#include "gfx/gfx_render_bundle.h"
#include "gfx/gfx_render_pipeline.h"
#include "gfx/gfx_texture.h"
#include "gfx/gfx_texture_view.h"

namespace gfx {

namespace {

FormatInfo AttachmentFormatInfo(const WGPUTextureView& view_handle) {
  auto* view = static_cast<gfx::TextureView*>(view_handle);
  return GetFormatInfo(view->GetTexture()->GetFormat());
}

}  // namespace

RenderPassEncoder::RenderPassEncoder(
    RefPtr<CommandEncoder> encoder,
    WGPURenderPassDescriptor const * descriptor)
    : encoder_(std::move(encoder)),
      buffer_(encoder_->GetVkCommandBuffer()) {
  Device* device = encoder_->GetDevice();
  VkDevice vk_device = device->GetVkDevice();

  if (descriptor->occlusionQuerySet)
    occlusion_query_set_ = static_cast<gfx::QuerySet*>(descriptor->occlusionQuerySet);

  // Build the render pass and framebuffer from the attachment views.
  std::vector<VkAttachmentDescription> attachments;
  std::vector<VkAttachmentReference> color_refs;
  std::vector<VkClearValue> clear_values;
  VkAttachmentReference depth_ref = {};
  bool has_depth = false;

  for (size_t i = 0; i < descriptor->colorAttachmentCount; ++i) {
    const WGPURenderPassColorAttachment& attachment = descriptor->colorAttachments[i];
    auto* view = static_cast<gfx::TextureView*>(attachment.view);
    FormatInfo info =
        GetFormatInfo(view->GetTexture()->GetFormat());

    VkAttachmentDescription vk_attachment = {};
    vk_attachment.format = info.vk_format;
    vk_attachment.samples = static_cast<VkSampleCountFlagBits>(
        view->GetTexture()->GetSampleCount());
    vk_attachment.loadOp =
        attachment.loadOp == WGPULoadOp_Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                              : VK_ATTACHMENT_LOAD_OP_LOAD;
    vk_attachment.storeOp =
        attachment.storeOp == WGPUStoreOp_Discard
            ? VK_ATTACHMENT_STORE_OP_DONT_CARE
            : VK_ATTACHMENT_STORE_OP_STORE;
    vk_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    vk_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    vk_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vk_attachment.finalLayout = Device::kImageLayout;

    VkClearValue clear = {};
    clear.color = {{static_cast<float>(attachment.clearValue.r),
                    static_cast<float>(attachment.clearValue.g),
                    static_cast<float>(attachment.clearValue.b),
                    static_cast<float>(attachment.clearValue.a)}};
    if (attachment.loadOp != WGPULoadOp_Load)
      clear_values.push_back(clear);
    else if (i == 0)
      clear_values.push_back(clear);

    color_refs.push_back({static_cast<uint32_t>(attachments.size()),
                          Device::kImageLayout});
    attachments.push_back(vk_attachment);
    encoder_->KeepResource(RefHolder::Of(view));
    if (attachment.resolveTarget)
      encoder_->KeepResource(
          RefHolder::Of(static_cast<gfx::TextureView*>(attachment.resolveTarget)));
  }

  if (descriptor->depthStencilAttachment &&
      descriptor->depthStencilAttachment->view) {
    const WGPURenderPassDepthStencilAttachment& attachment =
        *descriptor->depthStencilAttachment;
    auto* view = static_cast<gfx::TextureView*>(attachment.view);
    FormatInfo info =
        GetFormatInfo(view->GetTexture()->GetFormat());

    VkAttachmentDescription vk_attachment = {};
    vk_attachment.format = info.vk_format;
    vk_attachment.samples = static_cast<VkSampleCountFlagBits>(
        view->GetTexture()->GetSampleCount());
    vk_attachment.loadOp =
        attachment.depthLoadOp == WGPULoadOp_Clear
            ? VK_ATTACHMENT_LOAD_OP_CLEAR
            : (attachment.depthLoadOp == WGPULoadOp_Load
                   ? VK_ATTACHMENT_LOAD_OP_LOAD
                   : VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    vk_attachment.storeOp =
        attachment.depthStoreOp == WGPUStoreOp_Discard
            ? VK_ATTACHMENT_STORE_OP_DONT_CARE
            : VK_ATTACHMENT_STORE_OP_STORE;
    vk_attachment.stencilLoadOp =
        attachment.stencilLoadOp == WGPULoadOp_Clear
            ? VK_ATTACHMENT_LOAD_OP_CLEAR
            : (attachment.stencilLoadOp == WGPULoadOp_Load
                   ? VK_ATTACHMENT_LOAD_OP_LOAD
                   : VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    vk_attachment.stencilStoreOp =
        attachment.stencilStoreOp == WGPUStoreOp_Discard
            ? VK_ATTACHMENT_STORE_OP_DONT_CARE
            : VK_ATTACHMENT_STORE_OP_STORE;
    vk_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vk_attachment.finalLayout = Device::kImageLayout;

    VkClearValue clear = {};
    clear.depthStencil = {1.0f, attachment.stencilClearValue};
    clear_values.push_back(clear);

    has_depth = true;
    depth_ref = {static_cast<uint32_t>(attachments.size()),
                 Device::kImageLayout};
    attachments.push_back(vk_attachment);
    encoder_->KeepResource(RefHolder::Of(view));
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
  vkCreateRenderPass(vk_device, &pass_info, nullptr, &render_pass_);
  encoder_->KeepVulkanResource([device, rp = render_pass_] {
    vkDestroyRenderPass(device->GetVkDevice(), rp, nullptr);
  });

  std::vector<VkImageView> framebuffer_views;
  uint32_t width = 1;
  uint32_t height = 1;
  for (size_t i = 0; i < descriptor->colorAttachmentCount; ++i) {
    auto* view = static_cast<gfx::TextureView*>(
        descriptor->colorAttachments[i].view);
    framebuffer_views.push_back(view->GetVkImageView());
    width = view->GetTexture()->GetExtent().width;
    height = view->GetTexture()->GetExtent().height;
  }
  if (descriptor->depthStencilAttachment &&
      descriptor->depthStencilAttachment->view) {
    auto* view = static_cast<gfx::TextureView*>(
        descriptor->depthStencilAttachment->view);
    framebuffer_views.push_back(view->GetVkImageView());
    width = view->GetTexture()->GetExtent().width;
    height = view->GetTexture()->GetExtent().height;
  }

  VkFramebufferCreateInfo framebuffer_info = {};
  framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_info.renderPass = render_pass_;
  framebuffer_info.attachmentCount =
      static_cast<uint32_t>(framebuffer_views.size());
  framebuffer_info.pAttachments = framebuffer_views.data();
  framebuffer_info.width = width;
  framebuffer_info.height = height;
  framebuffer_info.layers = 1;
  VkFramebuffer framebuffer = VK_NULL_HANDLE;
  vkCreateFramebuffer(vk_device, &framebuffer_info, nullptr, &framebuffer);
  encoder_->KeepVulkanResource([device, fb = framebuffer] {
    vkDestroyFramebuffer(device->GetVkDevice(), fb, nullptr);
  });

  if (descriptor->occlusionQuerySet)
    encoder_->KeepResource(
        RefHolder::Of(static_cast<gfx::QuerySet*>(descriptor->occlusionQuerySet)));

  VkRenderPassBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin_info.renderPass = render_pass_;
  begin_info.framebuffer = framebuffer;
  begin_info.renderArea.extent = {width, height};
  begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
  begin_info.pClearValues = clear_values.data();
  vkCmdBeginRenderPass(buffer_, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

  // Default full-attachment viewport (negative height for WebGPU NDC
  // orientation) and scissor.
  VkViewport viewport = {0.0f, static_cast<float>(height),
                         static_cast<float>(width),
                         -static_cast<float>(height), 0.0f, 1.0f};
  VkRect2D scissor = {{0, 0}, {width, height}};
  vkCmdSetViewport(buffer_, 0, 1, &viewport);
  vkCmdSetScissor(buffer_, 0, 1, &scissor);
}

RenderPassEncoder::~RenderPassEncoder() = default;

void RenderPassEncoder::SetPipeline(WGPURenderPipeline pipeline) {
  current_pipeline_ = static_cast<RenderPipeline*>(pipeline);
  encoder_->KeepResource(RefHolder::Of(current_pipeline_));
  vkCmdBindPipeline(buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    current_pipeline_->GetVkPipeline());
}

void RenderPassEncoder::SetBindGroup(uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) {
  if (!current_pipeline_)
    return;
  auto* bind_group = static_cast<gfx::BindGroup*>(group);
  encoder_->KeepResource(RefHolder::Of(bind_group));
  VkDescriptorSet set = bind_group->GetVkDescriptorSet();
  vkCmdBindDescriptorSets(
      buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
      current_pipeline_->GetVkPipelineLayout(), groupIndex, 1, &set,
      static_cast<uint32_t>(dynamicOffsetCount), dynamicOffsets);
}

void RenderPassEncoder::SetImmediates(uint32_t offset, void const * data, size_t size) {
  if (!current_pipeline_)
    return;
  vkCmdPushConstants(buffer_, current_pipeline_->GetVkPipelineLayout(),
                     current_pipeline_->GetImmediateStages(), offset,
                     static_cast<uint32_t>(size), data);
}

void RenderPassEncoder::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
  vkCmdDraw(buffer_, vertexCount, instanceCount, firstVertex, firstInstance);
}

void RenderPassEncoder::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) {
  vkCmdDrawIndexed(buffer_, indexCount, instanceCount, firstIndex, baseVertex,
                   firstInstance);
}

void RenderPassEncoder::DrawIndirect(WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
  auto* buffer = static_cast<gfx::Buffer*>(indirectBuffer);
  encoder_->KeepResource(RefHolder::Of(buffer));
  vkCmdDrawIndirect(buffer_, buffer->GetVkBuffer(),
                    static_cast<uint32_t>(indirectOffset), 1, 0);
}

void RenderPassEncoder::DrawIndexedIndirect(WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
  auto* buffer = static_cast<gfx::Buffer*>(indirectBuffer);
  encoder_->KeepResource(RefHolder::Of(buffer));
  vkCmdDrawIndexedIndirect(buffer_, buffer->GetVkBuffer(),
                           static_cast<uint32_t>(indirectOffset), 1, 0);
}

void RenderPassEncoder::ExecuteBundles(size_t bundleCount, WGPURenderBundle const * bundles) {
  std::vector<VkCommandBuffer> secondary;
  secondary.reserve(bundleCount);
  for (size_t i = 0; i < bundleCount; ++i) {
    auto* bundle = static_cast<gfx::RenderBundle*>(bundles[i]);
    encoder_->KeepResource(RefHolder::Of(bundle));
    secondary.push_back(bundle->GetVkCommandBuffer());
  }
  vkCmdExecuteCommands(buffer_, static_cast<uint32_t>(secondary.size()),
                       secondary.data());
}

void RenderPassEncoder::InsertDebugMarker(WGPUStringView markerLabel) {
}

void RenderPassEncoder::PopDebugGroup() {
}

void RenderPassEncoder::PushDebugGroup(WGPUStringView groupLabel) {
}

void RenderPassEncoder::SetStencilReference(uint32_t reference) {
  vkCmdSetStencilReference(buffer_, VK_STENCIL_FACE_FRONT_AND_BACK, reference);
}

void RenderPassEncoder::SetBlendConstant(WGPUColor const * color) {
  float constant[4] = {static_cast<float>(color->r),
                       static_cast<float>(color->g),
                       static_cast<float>(color->b),
                       static_cast<float>(color->a)};
  vkCmdSetBlendConstants(buffer_, constant);
}

void RenderPassEncoder::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) {
  VkViewport viewport = {x, y + height, width, -height, minDepth, maxDepth};
  vkCmdSetViewport(buffer_, 0, 1, &viewport);
}

void RenderPassEncoder::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
  VkRect2D scissor = {{static_cast<int32_t>(x), static_cast<int32_t>(y)},
                      {width, height}};
  vkCmdSetScissor(buffer_, 0, 1, &scissor);
}

void RenderPassEncoder::SetVertexBuffer(uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
  auto* vk_buffer = static_cast<gfx::Buffer*>(buffer);
  encoder_->KeepResource(RefHolder::Of(vk_buffer));
  VkDeviceSize vk_offset = static_cast<VkDeviceSize>(offset);
  VkBuffer handle = vk_buffer->GetVkBuffer();
  vkCmdBindVertexBuffers(buffer_, slot, 1, &handle, &vk_offset);
}

void RenderPassEncoder::SetIndexBuffer(WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) {
  auto* vk_buffer = static_cast<gfx::Buffer*>(buffer);
  encoder_->KeepResource(RefHolder::Of(vk_buffer));
  vkCmdBindIndexBuffer(buffer_, vk_buffer->GetVkBuffer(),
                       static_cast<VkDeviceSize>(offset),
                       GetIndexType(format));
}

void RenderPassEncoder::BeginOcclusionQuery(uint32_t queryIndex) {
  // Reset the whole pool once per pass before the first use.
  if (!occlusion_reset_done_) {
    if (occlusion_query_set_) {
      vkCmdResetQueryPool(buffer_, occlusion_query_set_->GetVkQueryPool(), 0,
                          occlusion_query_set_->GetCount());
    }
    occlusion_reset_done_ = true;
  }
  if (occlusion_query_set_) {
    occlusion_query_index_ = queryIndex;
    vkCmdBeginQuery(buffer_, occlusion_query_set_->GetVkQueryPool(),
                    queryIndex, 0);
  }
}

void RenderPassEncoder::EndOcclusionQuery() {
  if (occlusion_query_set_)
    vkCmdEndQuery(buffer_, occlusion_query_set_->GetVkQueryPool(),
                  occlusion_query_index_);
}

void RenderPassEncoder::End() {
  ended_ = true;
  vkCmdEndRenderPass(buffer_);
}

void RenderPassEncoder::SetLabel(WGPUStringView label) {}

}  // namespace gfx
