// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_queue.h"

#include "gfx/common/vulkan_conversions.h"
#include "gfx/gfx_buffer.h"
#include "gfx/gfx_command_buffer.h"
#include "gfx/gfx_device.h"
#include "gfx/gfx_instance.h"
#include "vk_mem_alloc.h"
#include "gfx/gfx_texture.h"

namespace gfx {

Queue::Queue(RefPtr<Device> device) : device_(std::move(device)) {}

Queue::~Queue() = default;

void Queue::Submit(size_t commandCount, WGPUCommandBuffer const * commands) {
  std::vector<VkCommandBuffer> buffers;
  std::vector<RefHolder> resources;
  buffers.reserve(commandCount);
  for (size_t i = 0; i < commandCount; ++i) {
    auto* command_buffer = static_cast<gfx::CommandBuffer*>(commands[i]);
    buffers.push_back(command_buffer->GetVkCommandBuffer());
    // Keep the command buffer (and its pool) alive until execution done.
    resources.emplace_back(RefHolder::Of(command_buffer));
  }

  VkSemaphore wait_semaphore =
      device_->AcquireWaitSemaphoreForSubmit();
  VkSemaphore signal_semaphore = VK_NULL_HANDLE;
  VkSemaphoreCreateInfo semaphore_info = {};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  vkCreateSemaphore(device_->GetVkDevice(), &semaphore_info, nullptr,
                    &signal_semaphore);
  device_->SetRenderDoneSemaphore(signal_semaphore);

  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = static_cast<uint32_t>(buffers.size());
  submit_info.pCommandBuffers = buffers.data();
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &signal_semaphore;
  if (wait_semaphore) {
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &wait_semaphore;
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit_info.pWaitDstStageMask = &wait_stage;
  }

  VkFence fence = VK_NULL_HANDLE;
  VkFenceCreateInfo fence_info = {};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  vkCreateFence(device_->GetVkDevice(), &fence_info, nullptr, &fence);

  vkQueueSubmit(device_->GetVkQueue(), 1, &submit_info, fence);
  device_->TrackSubmission(fence, std::move(resources));
  device_->PollPendingSubmissions(0);
}

WGPUFuture Queue::OnSubmittedWorkDone(WGPUQueueWorkDoneCallbackInfo callbackInfo) {
  RefPtr<Device> device = device_;
  RefPtr<Queue> queue_self = RefPtr<Queue>(this);
  WGPUQueueWorkDoneCallbackInfo captured = callbackInfo;
  uint64_t sequence = device->NextSubmissionSequence();

  if (!callbackInfo.callback)
    return WGPUFuture{0};

  Instance* instance = device->GetInstance();
  uint64_t future_id = instance->AddFuture(
      [device, queue_self, captured, sequence](uint64_t timeout_ns) {
        if (!device->WaitForSequence(sequence, timeout_ns))
          return false;
        if (captured.callback) {
          captured.callback(WGPUQueueWorkDoneStatus_Success,
                            WGPU_STRING_VIEW_INIT, captured.userdata1,
                            captured.userdata2);
        }
        return true;
      });
  return WGPUFuture{future_id};
}

void Queue::WriteBuffer(WGPUBuffer buffer, uint64_t bufferOffset, void const * data, size_t size) {
  auto* dst = static_cast<gfx::Buffer*>(buffer);
  if (dst->GetMappedData()) {
    std::memcpy(static_cast<char*>(dst->GetMappedData()) + bufferOffset, data,
                size);
    vmaFlushAllocation(device_->GetVmaAllocator(), dst->GetAllocation(),
                       bufferOffset, size);
    return;
  }
  UploadBuffer(buffer, bufferOffset, data, size);
}

void Queue::UploadBuffer(WGPUBuffer buffer, uint64_t bufferOffset,
                         void const * data, size_t size) {
  auto* dst = static_cast<gfx::Buffer*>(buffer);
  Device* device = device_.get();

  // Staging upload for device-local buffers.
  VkBufferCreateInfo staging_info = {};
  staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  staging_info.size = size;
  staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VkBuffer staging_buffer = VK_NULL_HANDLE;
  VmaAllocation staging_allocation = nullptr;
  VmaAllocationInfo allocation_info = {};
  vmaCreateBuffer(device->GetVmaAllocator(), &staging_info, &alloc_info,
                  &staging_buffer, &staging_allocation, &allocation_info);
  std::memcpy(allocation_info.pMappedData, data, size);

  device->RunOneTimeSubmit([&](VkCommandBuffer cmd) {
    VkBufferCopy copy = {};
    copy.srcOffset = 0;
    copy.dstOffset = bufferOffset;
    copy.size = size;
    vkCmdCopyBuffer(cmd, staging_buffer, dst->GetVkBuffer(), 1, &copy);
  });

  vmaDestroyBuffer(device->GetVmaAllocator(), staging_buffer,
                   staging_allocation);
}

void Queue::WriteTexture(WGPUTexelCopyTextureInfo const * destination, void const * data, size_t dataSize, WGPUTexelCopyBufferLayout const * dataLayout, WGPUExtent3D const * writeSize) {
  auto* texture = static_cast<gfx::Texture*>(destination->texture);
  Device* device = device_.get();
  FormatInfo format_info = GetFormatInfo(texture->GetFormat());

  // Compute the tightly packed size of the upload.
  uint32_t blocks_wide = (writeSize->width + format_info.block_width - 1) /
                         format_info.block_width;
  uint32_t blocks_tall = (writeSize->height + format_info.block_height - 1) /
                         format_info.block_height;
  uint32_t row_pitch =
      dataLayout->bytesPerRow ? dataLayout->bytesPerRow
                              : blocks_wide * format_info.bytes_per_block;
  uint64_t depth_pitch =
      dataLayout->rowsPerImage
          ? static_cast<uint64_t>(dataLayout->rowsPerImage) * row_pitch
          : static_cast<uint64_t>(blocks_tall) * row_pitch;
  uint64_t total_size =
      dataLayout->offset + depth_pitch * writeSize->depthOrArrayLayers;

  // Staging buffer with the upload data.
  VkBufferCreateInfo staging_info = {};
  staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  staging_info.size = total_size;
  staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VkBuffer staging_buffer = VK_NULL_HANDLE;
  VmaAllocation staging_allocation = nullptr;
  VmaAllocationInfo allocation_info = {};
  vmaCreateBuffer(device->GetVmaAllocator(), &staging_info, &alloc_info,
                  &staging_buffer, &staging_allocation, &allocation_info);
  std::memcpy(allocation_info.pMappedData, data, dataSize);

  device->RunOneTimeSubmit([&](VkCommandBuffer cmd) {
    VkBufferImageCopy copy = {};
    copy.bufferOffset = dataLayout->offset;
    copy.bufferRowLength =
        row_pitch / (format_info.bytes_per_block / format_info.block_width);
    copy.bufferImageHeight =
        dataLayout->rowsPerImage ? dataLayout->rowsPerImage *
                                       format_info.block_height
                                 : 0;
    copy.imageSubresource.aspectMask = GetAspectMask(destination->aspect);
    copy.imageSubresource.mipLevel = destination->mipLevel;
    copy.imageSubresource.baseArrayLayer = destination->origin.z;
    copy.imageSubresource.layerCount = writeSize->depthOrArrayLayers;
    copy.imageOffset = {static_cast<int32_t>(destination->origin.x),
                        static_cast<int32_t>(destination->origin.y),
                        static_cast<int32_t>(destination->origin.z)};
    copy.imageExtent = {writeSize->width, writeSize->height,
                        writeSize->depthOrArrayLayers};
    vkCmdCopyBufferToImage(cmd, staging_buffer, texture->GetVkImage(),
                           Device::kImageLayout, 1, &copy);
  });

  vmaDestroyBuffer(device->GetVmaAllocator(), staging_buffer,
                   staging_allocation);
}

void Queue::SetLabel(WGPUStringView label) {}

}  // namespace gfx
