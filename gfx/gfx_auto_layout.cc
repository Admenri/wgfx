// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_auto_layout.h"

#include <algorithm>
#include <map>

#include "gfx/common/vulkan_conversions.h"
#include "vk_mem_alloc.h"
#include "gfx/gfx_bind_group_layout.h"
#include "gfx/gfx_device.h"
#include "gfx/gfx_pipeline_layout.h"
#include "gfx/gfx_shader_module.h"

namespace gfx {

RefPtr<PipelineLayout> BuildAutoPipelineLayout(
    Device* device, const std::vector<ShaderModule*>& modules) {
  // Merge reflected bindings by (set, binding) with a visibility union.
  std::map<std::pair<uint32_t, uint32_t>, WGPUBindGroupLayoutEntry> merged;
  for (ShaderModule* module : modules) {
    if (!module)
      continue;
    for (const ReflectedBinding& binding : module->GetReflection().bindings) {
      auto key = std::make_pair(binding.set, binding.binding);
      auto it = merged.find(key);
      if (it == merged.end()) {
        merged[key] = binding.layout_entry;
      } else {
        it->second.visibility |= binding.layout_entry.visibility;
        it->second.buffer.type = binding.layout_entry.buffer.type;
        it->second.sampler.type = binding.layout_entry.sampler.type;
        it->second.texture.sampleType = binding.layout_entry.texture.sampleType;
        it->second.storageTexture.access =
            binding.layout_entry.storageTexture.access;
      }
    }
  }

  // One bind group layout per descriptor set.
  std::vector<RefPtr<BindGroupLayout>> set_layouts;
  uint32_t current_set = 0;
  std::vector<WGPUBindGroupLayoutEntry> current_entries;
  auto flush_set = [&](uint32_t set) {
    while (set_layouts.size() < set) {
      set_layouts.emplace_back(
          new BindGroupLayout(RefPtr<Device>(device),
                              std::vector<WGPUBindGroupLayoutEntry>{}));
    }
    set_layouts.emplace_back(new BindGroupLayout(RefPtr<Device>(device),
                                                 current_entries));
    current_entries.clear();
  };
  for (const auto& [key, entry] : merged) {
    if (key.first != current_set && !current_entries.empty()) {
      flush_set(key.first);
      current_set = key.first;
    }
    current_entries.push_back(entry);
  }
  if (!current_entries.empty())
    flush_set(current_set);
  if (set_layouts.empty()) {
    set_layouts.emplace_back(new BindGroupLayout(
        RefPtr<Device>(device), std::vector<WGPUBindGroupLayoutEntry>{}));
  }

  // Push constant block: the largest reflected immediate size.
  uint32_t immediate_size = 0;
  VkShaderStageFlags immediate_stages = 0;
  for (ShaderModule* module : modules) {
    if (!module)
      continue;
    const SpirvReflection& reflection = module->GetReflection();
    if (reflection.push_constant_size) {
      immediate_size = std::max(immediate_size, reflection.push_constant_size);
      immediate_stages |= GetShaderStageFlags(reflection.push_constant_stages);
    }
  }

  return RefPtr<PipelineLayout>(new PipelineLayout(
      RefPtr<Device>(device), std::move(set_layouts), immediate_size,
      immediate_stages ? immediate_stages
                       : (VK_SHADER_STAGE_VERTEX_BIT |
                          VK_SHADER_STAGE_FRAGMENT_BIT |
                          VK_SHADER_STAGE_COMPUTE_BIT)));
}

}  // namespace gfx
