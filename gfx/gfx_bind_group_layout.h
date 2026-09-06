// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include <vector>

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUBindGroupLayoutImpl handle;
// define it here so gfx::BindGroupLayout can derive from it, making the
// generated bridge's static_cast<gfx::BindGroupLayout*>(handle) a valid
// base->derived downcast.
struct WGPUBindGroupLayoutImpl {};

namespace gfx {

class Device;

class BindGroupLayout : public WGPUBindGroupLayoutImpl,
                        public RefCounted<BindGroupLayout> {
 public:
  BindGroupLayout(RefPtr<Device> device,
                  WGPUBindGroupLayoutDescriptor const * descriptor);
  // Builds a layout from SPIR-V reflection entries (automatic layouts).
  BindGroupLayout(RefPtr<Device> device,
                  const std::vector<WGPUBindGroupLayoutEntry>& entries);
  ~BindGroupLayout();

  VkDescriptorSetLayout GetVkDescriptorSetLayout() const {
    return descriptor_set_layout_;
  }
  const std::vector<WGPUBindGroupLayoutEntry>& GetEntries() const {
    return entries_;
  }

  void SetLabel(WGPUStringView label);

 private:
  void CreateVulkanLayout();

  RefPtr<Device> device_;
  VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
  std::vector<WGPUBindGroupLayoutEntry> entries_;
};

}  // namespace gfx
