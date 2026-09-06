// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUComputePipelineImpl handle;
// define it here so gfx::ComputePipeline can derive from it, making the
// generated bridge's static_cast<gfx::ComputePipeline*>(handle) a valid
// base->derived downcast.
struct WGPUComputePipelineImpl {};

namespace gfx {

class BindGroupLayout;
class Device;
class PipelineLayout;

class ComputePipeline : public WGPUComputePipelineImpl,
                        public RefCounted<ComputePipeline> {
 public:
  ComputePipeline(RefPtr<Device> device,
                  WGPUComputePipelineDescriptor const * descriptor);
  ~ComputePipeline();

  VkPipeline GetVkPipeline() const { return pipeline_; }
  VkPipelineLayout GetVkPipelineLayout() const;
  VkShaderStageFlags GetImmediateStages() const;
  BindGroupLayout* GetBindGroupLayoutObject(uint32_t group_index) const;

  gfx::BindGroupLayout* GetBindGroupLayout(uint32_t groupIndex);
  void SetLabel(WGPUStringView label);

 private:
  RefPtr<Device> device_;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  RefPtr<PipelineLayout> layout_;
};

}  // namespace gfx
