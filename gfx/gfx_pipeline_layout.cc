// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_pipeline_layout.h"

#include "gfx/gfx_bind_group_layout.h"
#include "gfx/gfx_device.h"

namespace gfx {

PipelineLayout::PipelineLayout(RefPtr<Device> device,
                               WGPUPipelineLayoutDescriptor const * descriptor)
    : device_(std::move(device)),
      immediate_stages_(VK_SHADER_STAGE_VERTEX_BIT |
                        VK_SHADER_STAGE_FRAGMENT_BIT |
                        VK_SHADER_STAGE_COMPUTE_BIT) {
  bind_group_layouts_.reserve(descriptor->bindGroupLayoutCount);
  for (size_t i = 0; i < descriptor->bindGroupLayoutCount; ++i) {
    bind_group_layouts_.emplace_back(
        static_cast<gfx::BindGroupLayout*>(descriptor->bindGroupLayouts[i]));
  }

  std::vector<VkDescriptorSetLayout> set_layouts;
  set_layouts.reserve(bind_group_layouts_.size());
  for (const RefPtr<BindGroupLayout>& layout : bind_group_layouts_)
    set_layouts.push_back(layout->GetVkDescriptorSetLayout());

  VkPushConstantRange push_range = {};
  if (descriptor->immediateSize) {
    push_range.stageFlags = immediate_stages_;
    push_range.offset = 0;
    push_range.size = descriptor->immediateSize;
  }

  VkPipelineLayoutCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  create_info.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
  create_info.pSetLayouts = set_layouts.data();
  create_info.pushConstantRangeCount = descriptor->immediateSize ? 1 : 0;
  create_info.pPushConstantRanges = &push_range;
  vkCreatePipelineLayout(device_->GetVkDevice(), &create_info, nullptr,
                         &pipeline_layout_);
}

PipelineLayout::PipelineLayout(
    RefPtr<Device> device, std::vector<RefPtr<BindGroupLayout>> layouts,
    uint32_t immediate_size, VkShaderStageFlags immediate_stages)
    : device_(std::move(device)),
      bind_group_layouts_(std::move(layouts)),
      immediate_stages_(immediate_stages) {
  std::vector<VkDescriptorSetLayout> set_layouts;
  set_layouts.reserve(bind_group_layouts_.size());
  for (const RefPtr<BindGroupLayout>& layout : bind_group_layouts_)
    set_layouts.push_back(layout->GetVkDescriptorSetLayout());

  VkPushConstantRange push_range = {};
  if (immediate_size) {
    push_range.stageFlags = immediate_stages_;
    push_range.offset = 0;
    push_range.size = immediate_size;
  }

  VkPipelineLayoutCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  create_info.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
  create_info.pSetLayouts = set_layouts.data();
  create_info.pushConstantRangeCount = immediate_size ? 1 : 0;
  create_info.pPushConstantRanges = &push_range;
  vkCreatePipelineLayout(device_->GetVkDevice(), &create_info, nullptr,
                         &pipeline_layout_);
}

PipelineLayout::~PipelineLayout() {
  if (pipeline_layout_)
    vkDestroyPipelineLayout(device_->GetVkDevice(), pipeline_layout_, nullptr);
}

void PipelineLayout::SetLabel(WGPUStringView label) {
  device_->SetObjectLabel(reinterpret_cast<uint64_t>(pipeline_layout_),
                          VK_OBJECT_TYPE_PIPELINE_LAYOUT, label);
}

}  // namespace gfx
