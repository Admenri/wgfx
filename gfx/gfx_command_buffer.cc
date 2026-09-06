// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_command_buffer.h"

#include "gfx/gfx_device.h"
#include "vk_mem_alloc.h"

namespace gfx {

CommandBuffer::CommandBuffer(RefPtr<Device> device, VkCommandPool pool,
                             VkCommandBuffer buffer)
    : device_(std::move(device)), pool_(pool), buffer_(buffer) {}

CommandBuffer::~CommandBuffer() {
  if (pool_) {
    vkDestroyCommandPool(device_->GetVkDevice(), pool_, nullptr);
    pool_ = VK_NULL_HANDLE;
    buffer_ = VK_NULL_HANDLE;
  }
}

void CommandBuffer::KeepResource(RefHolder holder) {
  resources_.push_back(std::move(holder));
}

void CommandBuffer::KeepVulkanResource(std::function<void()> destroy) {
  vulkan_cleanup_.push_back(std::move(destroy));
}

void CommandBuffer::SetLabel(WGPUStringView label) {}

}  // namespace gfx
