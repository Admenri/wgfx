// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_bind_group.h"

#include "gfx/common/vulkan_conversions.h"
#include "vk_mem_alloc.h"
#include "gfx/gfx_bind_group_layout.h"
#include "gfx/gfx_buffer.h"
#include "gfx/gfx_device.h"
#include "gfx/gfx_sampler.h"
#include "gfx/gfx_texture_view.h"

namespace gfx {

BindGroup::BindGroup(RefPtr<Device> device, RefPtr<BindGroupLayout> layout,
                     WGPUBindGroupDescriptor const * descriptor)
    : device_(std::move(device)), layout_(std::move(layout)) {
  VkDescriptorPoolSize pool_size = {};
  pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLER;
  pool_size.descriptorCount = 1;

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = 1;
  pool_info.poolSizeCount = 8;
  VkDescriptorPoolSize sizes[8];
  static const VkDescriptorType kAllTypes[] = {
      VK_DESCRIPTOR_TYPE_SAMPLER,
      VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
      VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
  };
  for (uint32_t i = 0; i < 8; ++i) {
    sizes[i].type = kAllTypes[i];
    sizes[i].descriptorCount = 1;
  }
  pool_info.pPoolSizes = sizes;

  vkCreateDescriptorPool(device_->GetVkDevice(), &pool_info, nullptr,
                         &descriptor_pool_);

  VkDescriptorSetLayout set_layout = layout_->GetVkDescriptorSetLayout();
  VkDescriptorSetAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = descriptor_pool_;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &set_layout;
  vkAllocateDescriptorSets(device_->GetVkDevice(), &alloc_info,
                           &descriptor_set_);

  // Write descriptors from the entries, resolving types against the layout.
  std::vector<VkWriteDescriptorSet> writes;
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  std::vector<VkDescriptorImageInfo> image_infos;
  writes.reserve(descriptor->entryCount);
  buffer_infos.reserve(descriptor->entryCount);
  image_infos.reserve(descriptor->entryCount);

  for (size_t i = 0; i < descriptor->entryCount; ++i) {
    const WGPUBindGroupEntry& entry = descriptor->entries[i];
    const WGPUBindGroupLayoutEntry* layout_entry = nullptr;
    for (const WGPUBindGroupLayoutEntry& candidate : layout_->GetEntries()) {
      if (candidate.binding == entry.binding) {
        layout_entry = &candidate;
        break;
      }
    }
    if (!layout_entry)
      continue;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set_;
    write.dstBinding = entry.binding;
    write.descriptorCount = 1;

    if (entry.buffer) {
      gfx::Buffer* buffer = static_cast<gfx::Buffer*>(entry.buffer);
      VkDescriptorBufferInfo& info = buffer_infos.emplace_back();
      info.buffer = buffer->GetVkBuffer();
      info.offset = entry.offset;
      info.range = entry.size == WGPU_WHOLE_SIZE ? VK_WHOLE_SIZE : entry.size;
      write.descriptorType =
          layout_entry->buffer.type == WGPUBufferBindingType_Uniform
              ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
              : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      write.pBufferInfo = &info;
    } else if (entry.sampler) {
      gfx::Sampler* sampler = static_cast<gfx::Sampler*>(entry.sampler);
      VkDescriptorImageInfo& info = image_infos.emplace_back();
      info.sampler = sampler->GetVkSampler();
      write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
      write.pImageInfo = &info;
    } else if (entry.textureView) {
      gfx::TextureView* view =
          static_cast<gfx::TextureView*>(entry.textureView);
      VkDescriptorImageInfo& info = image_infos.emplace_back();
      info.imageView = view->GetVkImageView();
      info.imageLayout = Device::kImageLayout;
      bool storage =
          layout_entry->storageTexture.access !=
          WGPUStorageTextureAccess_BindingNotUsed;
      write.descriptorType = storage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                     : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      write.pImageInfo = &info;
    } else {
      continue;
    }
    writes.push_back(write);
  }

  if (!writes.empty())
    vkUpdateDescriptorSets(device_->GetVkDevice(),
                           static_cast<uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
}

BindGroup::~BindGroup() {
  if (descriptor_pool_)
    vkDestroyDescriptorPool(device_->GetVkDevice(), descriptor_pool_, nullptr);
}

void BindGroup::SetLabel(WGPUStringView label) {}

}  // namespace gfx
