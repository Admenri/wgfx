// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_sampler.h"

#include "gfx/common/vulkan_conversions.h"
#include "vk_mem_alloc.h"
#include "gfx/gfx_device.h"

namespace gfx {

Sampler::Sampler(RefPtr<Device> device, WGPUSamplerDescriptor const * descriptor)
    : device_(std::move(device)) {
  VkSamplerCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  create_info.magFilter = GetFilterMode(descriptor->magFilter);
  create_info.minFilter = GetFilterMode(descriptor->minFilter);
  create_info.mipmapMode = GetMipmapFilterMode(descriptor->mipmapFilter);
  create_info.addressModeU = GetAddressMode(descriptor->addressModeU);
  create_info.addressModeV = GetAddressMode(descriptor->addressModeV);
  create_info.addressModeW = GetAddressMode(descriptor->addressModeW);
  create_info.mipLodBias = 0.0f;
  create_info.anisotropyEnable = VK_FALSE;
  create_info.maxAnisotropy = static_cast<float>(descriptor->maxAnisotropy);
  create_info.compareEnable =
      descriptor->compare != WGPUCompareFunction_Undefined &&
      descriptor->compare != WGPUCompareFunction_Always;
  create_info.compareOp = GetCompareOp(descriptor->compare);
  create_info.minLod = static_cast<float>(descriptor->lodMinClamp);
  create_info.maxLod = static_cast<float>(descriptor->lodMaxClamp);
  create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
  create_info.unnormalizedCoordinates = VK_FALSE;

  vkCreateSampler(device_->GetVkDevice(), &create_info, nullptr, &sampler_);
}

Sampler::~Sampler() {
  if (sampler_)
    vkDestroySampler(device_->GetVkDevice(), sampler_, nullptr);
}

void Sampler::SetLabel(WGPUStringView label) {}

}  // namespace gfx
