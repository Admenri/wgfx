// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUPipelineLayoutImpl handle;
// define it here so gfx::PipelineLayout can derive from it, making the
// generated bridge's static_cast<gfx::PipelineLayout*>(handle) a valid
// base->derived downcast.
struct WGPUPipelineLayoutImpl {};

namespace gfx {

class BindGroupLayout;
class Device;

class PipelineLayout : public WGPUPipelineLayoutImpl,
                       public RefCounted<PipelineLayout> {
 public:
  PipelineLayout(RefPtr<Device> device,
                 WGPUPipelineLayoutDescriptor const * descriptor);
  // Builds a layout from automatic (reflected) bind group layouts.
  PipelineLayout(RefPtr<Device> device,
                 std::vector<RefPtr<BindGroupLayout>> layouts,
                 uint32_t immediate_size, VkShaderStageFlags immediate_stages);
  ~PipelineLayout();

  VkPipelineLayout GetVkPipelineLayout() const { return pipeline_layout_; }
  const std::vector<RefPtr<BindGroupLayout>>& GetBindGroupLayouts() const {
    return bind_group_layouts_;
  }
  VkShaderStageFlags GetImmediateStages() const { return immediate_stages_; }

  void SetLabel(WGPUStringView label);

 private:
  RefPtr<Device> device_;
  VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
  std::vector<RefPtr<BindGroupLayout>> bind_group_layouts_;
  VkShaderStageFlags immediate_stages_ = 0;
};

}  // namespace gfx
