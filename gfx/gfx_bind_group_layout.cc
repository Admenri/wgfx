// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_bind_group_layout.h"

#include "gfx/common/vulkan_conversions.h"
#include "gfx/gfx_device.h"

namespace gfx {

BindGroupLayout::BindGroupLayout(
    RefPtr<Device> device, WGPUBindGroupLayoutDescriptor const * descriptor)
    : device_(std::move(device)),
      entries_(descriptor->entries,
               descriptor->entries + descriptor->entryCount) {
  CreateVulkanLayout();
}

BindGroupLayout::BindGroupLayout(
    RefPtr<Device> device, const std::vector<WGPUBindGroupLayoutEntry>& entries)
    : device_(std::move(device)), entries_(entries) {
  CreateVulkanLayout();
}

BindGroupLayout::~BindGroupLayout() {
  if (descriptor_set_layout_)
    vkDestroyDescriptorSetLayout(device_->GetVkDevice(),
                                 descriptor_set_layout_, nullptr);
}

void BindGroupLayout::CreateVulkanLayout() {
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  bindings.reserve(entries_.size());
  for (const WGPUBindGroupLayoutEntry& entry : entries_) {
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = entry.binding;
    binding.descriptorCount = 1;
    binding.stageFlags = GetShaderStageFlags(entry.visibility);

    WGPUBindGroupLayoutEntry resolved = entry;
    // Resolve "undefined" binding types to their resource kind first
    // (WebGPU treats entries with a single populated layout as such).
    if (resolved.buffer.type == WGPUBufferBindingType_Undefined &&
        resolved.sampler.type == WGPUSamplerBindingType_Undefined &&
        resolved.texture.sampleType == WGPUTextureSampleType_Undefined &&
        resolved.storageTexture.access == WGPUStorageTextureAccess_Undefined) {
      if (entry.buffer.type == WGPUBufferBindingType_BindingNotUsed &&
          entry.sampler.type == WGPUSamplerBindingType_BindingNotUsed &&
          entry.texture.sampleType == WGPUTextureSampleType_BindingNotUsed &&
          entry.storageTexture.access ==
              WGPUStorageTextureAccess_BindingNotUsed)
        continue;
    }

    if (resolved.buffer.type == WGPUBufferBindingType_Uniform) {
      binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    } else if (resolved.buffer.type == WGPUBufferBindingType_Storage) {
      binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    } else if (resolved.buffer.type == WGPUBufferBindingType_ReadOnlyStorage) {
      binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    } else if (resolved.sampler.type == WGPUSamplerBindingType_Comparison) {
      binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    } else if (resolved.sampler.type != WGPUSamplerBindingType_BindingNotUsed) {
      binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    } else if (resolved.storageTexture.access !=
               WGPUStorageTextureAccess_BindingNotUsed) {
      binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    } else {
      binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    }

    binding.pImmutableSamplers = nullptr;
    bindings.push_back(binding);
  }

  VkDescriptorSetLayoutCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  create_info.bindingCount = static_cast<uint32_t>(bindings.size());
  create_info.pBindings = bindings.data();
  vkCreateDescriptorSetLayout(device_->GetVkDevice(), &create_info, nullptr,
                              &descriptor_set_layout_);
}

void BindGroupLayout::SetLabel(WGPUStringView label) {}

}  // namespace gfx
