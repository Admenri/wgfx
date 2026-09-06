// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_compute_pipeline.h"

#include "gfx/gfx_auto_layout.h"
#include "gfx/gfx_bind_group_layout.h"
#include "gfx/gfx_device.h"
#include "gfx/gfx_pipeline_layout.h"
#include "gfx/gfx_shader_module.h"

namespace gfx {

ComputePipeline::ComputePipeline(
    RefPtr<Device> device, WGPUComputePipelineDescriptor const * descriptor)
    : device_(std::move(device)) {
  gfx::ShaderModule* module =
      static_cast<gfx::ShaderModule*>(descriptor->compute.module);

  if (descriptor->layout) {
    layout_ = RefPtr<PipelineLayout>(
        static_cast<gfx::PipelineLayout*>(descriptor->layout));
  } else {
    layout_ = BuildAutoPipelineLayout(device_.get(), {module});
  }

  VkShaderModuleCreateInfo module_info = {};
  module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  module_info.codeSize = module->GetCodeWordCount() * sizeof(uint32_t);
  module_info.pCode = module->GetCode();
  VkShaderModule shader_module = VK_NULL_HANDLE;
  vkCreateShaderModule(device_->GetVkDevice(), &module_info, nullptr,
                       &shader_module);

  std::string entry_name(gfx::FromWGPUStringView(descriptor->compute.entryPoint));
  if (entry_name.empty())
    entry_name = "main";

  VkPipelineShaderStageCreateInfo stage_info = {};
  stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage_info.module = shader_module;
  stage_info.pName = entry_name.c_str();

  VkComputePipelineCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  create_info.stage = stage_info;
  create_info.layout = layout_->GetVkPipelineLayout();

  vkCreateComputePipelines(device_->GetVkDevice(), VK_NULL_HANDLE, 1,
                           &create_info, nullptr, &pipeline_);

  vkDestroyShaderModule(device_->GetVkDevice(), shader_module, nullptr);
}

ComputePipeline::~ComputePipeline() {
  if (pipeline_)
    vkDestroyPipeline(device_->GetVkDevice(), pipeline_, nullptr);
}

VkPipelineLayout ComputePipeline::GetVkPipelineLayout() const {
  return layout_->GetVkPipelineLayout();
}

VkShaderStageFlags ComputePipeline::GetImmediateStages() const {
  return layout_->GetImmediateStages();
}

BindGroupLayout* ComputePipeline::GetBindGroupLayoutObject(
    uint32_t group_index) const {
  return layout_->GetBindGroupLayouts()[group_index].get();
}

gfx::BindGroupLayout* ComputePipeline::GetBindGroupLayout(uint32_t groupIndex) {
  BindGroupLayout* layout = GetBindGroupLayoutObject(groupIndex);
  layout->AddRef();
  return layout;
}

void ComputePipeline::SetLabel(WGPUStringView label) {}

}  // namespace gfx
