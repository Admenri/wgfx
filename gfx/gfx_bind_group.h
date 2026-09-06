// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/ref_holder.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUBindGroupImpl handle;
// define it here so gfx::BindGroup can derive from it, making the
// generated bridge's static_cast<gfx::BindGroup*>(handle) a valid
// base->derived downcast.
struct WGPUBindGroupImpl {};

namespace gfx {

class BindGroupLayout;
class Device;

class BindGroup : public WGPUBindGroupImpl, public RefCounted<BindGroup> {
 public:
  BindGroup(RefPtr<Device> device, RefPtr<BindGroupLayout> layout,
            WGPUBindGroupDescriptor const * descriptor);
  ~BindGroup();

  VkDescriptorSet GetVkDescriptorSet() const { return descriptor_set_; }
  BindGroupLayout* GetLayout() const { return layout_.get(); }

  void SetLabel(WGPUStringView label);

 private:
  RefPtr<Device> device_;
  RefPtr<BindGroupLayout> layout_;
  VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
  VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;
};

}  // namespace gfx
