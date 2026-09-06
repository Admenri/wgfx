// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_command_encoder.h"

#include <cstring>

#include "gfx/common/ref_holder.h"
#include "gfx/common/vulkan_conversions.h"
#include "vk_mem_alloc.h"
#include "gfx/gfx_buffer.h"
#include "gfx/gfx_command_buffer.h"
#include "gfx/gfx_compute_pass_encoder.h"
#include "gfx/gfx_device.h"
#include "gfx/gfx_query_set.h"
#include "gfx/gfx_render_pass_encoder.h"
#include "gfx/gfx_texture.h"
#include "gfx/gfx_texture_view.h"

namespace gfx {

CommandEncoder::CommandEncoder(RefPtr<Device> device,
                               WGPUCommandEncoderDescriptor const * descriptor)
    : device_(std::move(device)) {
  VkCommandPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = device_->GetQueueFamilyIndex();
  vkCreateCommandPool(device_->GetVkDevice(), &pool_info, nullptr, &pool_);

  VkCommandBufferAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = pool_;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = 1;
  vkAllocateCommandBuffers(device_->GetVkDevice(), &alloc_info, &buffer_);

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(buffer_, &begin_info);
}

CommandEncoder::~CommandEncoder() {
  if (pool_ && !finished_)
    vkDestroyCommandPool(device_->GetVkDevice(), pool_, nullptr);
}

void CommandEncoder::KeepResource(RefHolder holder) {
  resources_.push_back(std::move(holder));
}

void CommandEncoder::KeepVulkanResource(std::function<void()> destroy) {
  vulkan_cleanup_.push_back(std::move(destroy));
}

gfx::CommandBuffer* CommandEncoder::Finish(WGPUCommandBufferDescriptor const * descriptor) {
  finished_ = true;
  vkEndCommandBuffer(buffer_);
  gfx::CommandBuffer* command_buffer =
      ToAPIRef(new CommandBuffer(device_, pool_, buffer_));
  for (RefHolder& holder : resources_)
    command_buffer->KeepResource(std::move(holder));
  for (std::function<void()>& destroy : vulkan_cleanup_)
    command_buffer->KeepVulkanResource(std::move(destroy));
  return command_buffer;
}

gfx::ComputePassEncoder* CommandEncoder::BeginComputePass(WGPUComputePassDescriptor const * descriptor) {
  return ToAPIRef(new ComputePassEncoder(RefPtr<CommandEncoder>(this), descriptor));
}

gfx::RenderPassEncoder* CommandEncoder::BeginRenderPass(WGPURenderPassDescriptor const * descriptor) {
  return ToAPIRef(new RenderPassEncoder(RefPtr<CommandEncoder>(this), descriptor));
}

void CommandEncoder::CopyBufferToBuffer(WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size) {
  gfx::Buffer* src = static_cast<gfx::Buffer*>(source);
  gfx::Buffer* dst = static_cast<gfx::Buffer*>(destination);
  KeepResource(RefHolder::Of(src));
  KeepResource(RefHolder::Of(dst));
  if (size == WGPU_WHOLE_SIZE)
    size = src->GetSize() - sourceOffset;

  VkBufferCopy copy = {};
  copy.srcOffset = sourceOffset;
  copy.dstOffset = destinationOffset;
  copy.size = size;
  vkCmdCopyBuffer(buffer_, src->GetVkBuffer(), dst->GetVkBuffer(), 1, &copy);
}

namespace {
}  // namespace

void CommandEncoder::CopyBufferToTexture(WGPUTexelCopyBufferInfo const * source, WGPUTexelCopyTextureInfo const * destination, WGPUExtent3D const * copySize) {
  gfx::Buffer* src = static_cast<gfx::Buffer*>(source->buffer);
  gfx::Texture* dst = static_cast<gfx::Texture*>(destination->texture);
  KeepResource(RefHolder::Of(src));
  KeepResource(RefHolder::Of(dst));
  FormatInfo format_info = GetFormatInfo(dst->GetFormat());

  VkBufferImageCopy copy = {};
  copy.bufferOffset = source->layout.offset;
  copy.bufferRowLength =
      source->layout.bytesPerRow
          ? source->layout.bytesPerRow /
                (format_info.bytes_per_block / format_info.block_width)
          : 0;
  copy.bufferImageHeight =
      source->layout.rowsPerImage
          ? source->layout.rowsPerImage * format_info.block_height
          : 0;
  copy.imageSubresource.aspectMask = GetAspectMask(destination->aspect);
  copy.imageSubresource.mipLevel = destination->mipLevel;
  copy.imageSubresource.baseArrayLayer = destination->origin.z;
  copy.imageSubresource.layerCount = copySize->depthOrArrayLayers;
  copy.imageOffset.x = destination->origin.x;
  copy.imageOffset.y = destination->origin.y;
  copy.imageOffset.z = destination->origin.z;
  copy.imageExtent.width = copySize->width;
  copy.imageExtent.height = copySize->height;
  copy.imageExtent.depth = copySize->depthOrArrayLayers;

  vkCmdCopyBufferToImage(buffer_, src->GetVkBuffer(), dst->GetVkImage(),
                         Device::kImageLayout, 1, &copy);
}

void CommandEncoder::CopyTextureToBuffer(WGPUTexelCopyTextureInfo const * source, WGPUTexelCopyBufferInfo const * destination, WGPUExtent3D const * copySize) {
  gfx::Texture* src = static_cast<gfx::Texture*>(source->texture);
  gfx::Buffer* dst = static_cast<gfx::Buffer*>(destination->buffer);
  KeepResource(RefHolder::Of(src));
  KeepResource(RefHolder::Of(dst));
  FormatInfo format_info = GetFormatInfo(src->GetFormat());

  VkBufferImageCopy copy = {};
  copy.bufferOffset = destination->layout.offset;
  copy.bufferRowLength =
      destination->layout.bytesPerRow
          ? destination->layout.bytesPerRow /
                (format_info.bytes_per_block / format_info.block_width)
          : 0;
  copy.bufferImageHeight =
      destination->layout.rowsPerImage
          ? destination->layout.rowsPerImage * format_info.block_height
          : 0;
  copy.imageSubresource.aspectMask = GetAspectMask(source->aspect);
  copy.imageSubresource.mipLevel = source->mipLevel;
  copy.imageSubresource.baseArrayLayer = source->origin.z;
  copy.imageSubresource.layerCount = copySize->depthOrArrayLayers;
  copy.imageOffset.x = source->origin.x;
  copy.imageOffset.y = source->origin.y;
  copy.imageExtent.width = copySize->width;
  copy.imageExtent.height = copySize->height;
  copy.imageExtent.depth = copySize->depthOrArrayLayers;

  vkCmdCopyImageToBuffer(buffer_, src->GetVkImage(), Device::kImageLayout,
                         dst->GetVkBuffer(), 1, &copy);
}

void CommandEncoder::CopyTextureToTexture(WGPUTexelCopyTextureInfo const * source, WGPUTexelCopyTextureInfo const * destination, WGPUExtent3D const * copySize) {
  gfx::Texture* src = static_cast<gfx::Texture*>(source->texture);
  gfx::Texture* dst = static_cast<gfx::Texture*>(destination->texture);
  KeepResource(RefHolder::Of(src));
  KeepResource(RefHolder::Of(dst));

  VkImageCopy copy = {};
  copy.srcSubresource.aspectMask = GetAspectMask(source->aspect);
  copy.srcSubresource.mipLevel = source->mipLevel;
  copy.srcSubresource.baseArrayLayer = source->origin.z;
  copy.srcSubresource.layerCount = copySize->depthOrArrayLayers;
  copy.srcOffset = {static_cast<int32_t>(source->origin.x),
                    static_cast<int32_t>(source->origin.y),
                    static_cast<int32_t>(source->origin.z)};
  copy.dstSubresource.aspectMask = GetAspectMask(destination->aspect);
  copy.dstSubresource.mipLevel = destination->mipLevel;
  copy.dstSubresource.baseArrayLayer = destination->origin.z;
  copy.dstSubresource.layerCount = copySize->depthOrArrayLayers;
  copy.dstOffset = {static_cast<int32_t>(destination->origin.x),
                    static_cast<int32_t>(destination->origin.y),
                    static_cast<int32_t>(destination->origin.z)};
  copy.extent.width = copySize->width;
  copy.extent.height = copySize->height;
  copy.extent.depth = copySize->depthOrArrayLayers;

  vkCmdCopyImage(buffer_, src->GetVkImage(), Device::kImageLayout,
                 dst->GetVkImage(), Device::kImageLayout, 1, &copy);
}

void CommandEncoder::ClearBuffer(WGPUBuffer buffer, uint64_t offset, uint64_t size) {
  gfx::Buffer* dst = static_cast<gfx::Buffer*>(buffer);
  KeepResource(RefHolder::Of(dst));
  vkCmdFillBuffer(buffer_, dst->GetVkBuffer(), offset,
                  size == WGPU_WHOLE_SIZE ? VK_WHOLE_SIZE : size, 0);
}

void CommandEncoder::InsertDebugMarker(WGPUStringView markerLabel) {
}

void CommandEncoder::PopDebugGroup() {
}

void CommandEncoder::PushDebugGroup(WGPUStringView groupLabel) {
}

void CommandEncoder::ResolveQuerySet(WGPUQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGPUBuffer destination, uint64_t destinationOffset) {
  gfx::QuerySet* query_set = static_cast<gfx::QuerySet*>(querySet);
  gfx::Buffer* dst = static_cast<gfx::Buffer*>(destination);
  KeepResource(RefHolder::Of(query_set));
  KeepResource(RefHolder::Of(dst));
  vkCmdCopyQueryPoolResults(
    buffer_, query_set->GetVkQueryPool(), firstQuery, queryCount,
    dst->GetVkBuffer(), destinationOffset, 8,
    VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
}

void CommandEncoder::WriteTimestamp(WGPUQuerySet querySet, uint32_t queryIndex) {
  gfx::QuerySet* query_set = static_cast<gfx::QuerySet*>(querySet);
  KeepResource(RefHolder::Of(query_set));
  if (!IsQueryPoolReset(query_set)) {
    vkCmdResetQueryPool(buffer_, query_set->GetVkQueryPool(), 0,
                        query_set->GetCount());
    reset_query_pools_.push_back(query_set);
  }
  vkCmdWriteTimestamp(buffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                      query_set->GetVkQueryPool(), queryIndex);
}

bool CommandEncoder::IsQueryPoolReset(gfx::QuerySet* query_set) const {
  for (gfx::QuerySet* reset : reset_query_pools_) {
    if (reset == query_set)
      return true;
  }
  return false;
}

void CommandEncoder::SetLabel(WGPUStringView label) {}

}  // namespace gfx
