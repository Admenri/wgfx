// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_render_bundle.h"

#include "gfx/gfx_device.h"
#include "vk_mem_alloc.h"

namespace gfx {

RenderBundle::RenderBundle(RefPtr<Device> device, VkCommandPool pool,
                           VkCommandBuffer buffer)
    : device_(std::move(device)), pool_(pool), buffer_(buffer) {}

RenderBundle::~RenderBundle() {
  if (pool_)
    vkDestroyCommandPool(device_->GetVkDevice(), pool_, nullptr);
}

void RenderBundle::KeepResource(RefHolder holder) {
  resources_.push_back(std::move(holder));
}

void RenderBundle::SetLabel(WGPUStringView label) {}

}  // namespace gfx
