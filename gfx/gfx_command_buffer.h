// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include <functional>
#include <vector>

#include "gfx/gfx_common.h"
#include "gfx/common/ref_holder.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUCommandBufferImpl handle;
// define it here so gfx::CommandBuffer can derive from it, making the
// generated bridge's static_cast<gfx::CommandBuffer*>(handle) a valid
// base->derived downcast.
struct WGPUCommandBufferImpl {};

namespace gfx {

class Device;

class CommandBuffer : public WGPUCommandBufferImpl,
                      public RefCounted<CommandBuffer> {
 public:
  CommandBuffer(RefPtr<Device> device, VkCommandPool pool,
                VkCommandBuffer buffer);
  ~CommandBuffer();

  VkCommandBuffer GetVkCommandBuffer() const { return buffer_; }

  // Keeps a referenced object or Vulkan handle alive until the command
  // buffer has finished executing on the GPU.
  void KeepResource(RefHolder holder);
  void KeepVulkanResource(std::function<void()> destroy);
  // Moves finished encoder resource lists into this command buffer.
  void AdoptResources(std::vector<RefHolder> holders,
                      std::vector<std::function<void()>> vulkan_cleanup) {
    resources_ = std::move(holders);
    vulkan_cleanup_ = std::move(vulkan_cleanup);
  }

  void SetLabel(WGPUStringView label);

 private:
  RefPtr<Device> device_;
  VkCommandPool pool_ = VK_NULL_HANDLE;
  VkCommandBuffer buffer_ = VK_NULL_HANDLE;
  std::vector<RefHolder> resources_;
  std::vector<std::function<void()>> vulkan_cleanup_;
};

}  // namespace gfx
