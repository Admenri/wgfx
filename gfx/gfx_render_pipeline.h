// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPURenderPipelineImpl handle;
// define it here so gfx::RenderPipeline can derive from it, making the
// generated bridge's static_cast<gfx::RenderPipeline*>(handle) a valid
// base->derived downcast.
struct WGPURenderPipelineImpl {};

namespace gfx {

class BindGroupLayout;
class Device;
class PipelineLayout;

class RenderPipeline : public WGPURenderPipelineImpl,
                       public RefCounted<RenderPipeline> {
 public:
  RenderPipeline(RefPtr<Device> device,
                 WGPURenderPipelineDescriptor const * descriptor);
  ~RenderPipeline();

  VkPipeline GetVkPipeline() const { return pipeline_; }
  VkPipelineLayout GetVkPipelineLayout() const;
  VkShaderStageFlags GetImmediateStages() const;
  // Bind group layout for |groupIndex| (user provided or automatic).
  BindGroupLayout* GetBindGroupLayoutObject(uint32_t group_index) const;

  gfx::BindGroupLayout* GetBindGroupLayout(uint32_t groupIndex);
  void SetLabel(WGPUStringView label);

 private:
  RefPtr<Device> device_;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  RefPtr<PipelineLayout> layout_;
};

}  // namespace gfx
