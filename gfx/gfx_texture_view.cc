// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_texture_view.h"

#include "gfx/common/vulkan_conversions.h"
#include "vk_mem_alloc.h"
#include "gfx/gfx_device.h"
#include "gfx/gfx_texture.h"

namespace gfx {

TextureView::TextureView(RefPtr<Texture> texture,
                         WGPUTextureViewDescriptor const * descriptor)
    : texture_(std::move(texture)), aspect_(descriptor->aspect) {
  VkImageViewCreateInfo view_info = {};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = texture_->GetVkImage();
  view_info.viewType = GetImageViewType(descriptor->dimension);
  view_info.format = GetFormatInfo(descriptor->format).vk_format;
  view_info.components = GetAspectComponents(descriptor->aspect);
  for (const WGPUChainedStruct* chain = descriptor->nextInChain; chain;
       chain = chain->next) {
    if (chain->sType == WGPUSType_TextureComponentSwizzleDescriptor) {
      auto* swizzle =
          reinterpret_cast<const WGPUTextureComponentSwizzleDescriptor*>(
              chain);
      view_info.components = {
          GetComponentSwizzle(swizzle->swizzle.r),
          GetComponentSwizzle(swizzle->swizzle.g),
          GetComponentSwizzle(swizzle->swizzle.b),
          GetComponentSwizzle(swizzle->swizzle.a),
      };
      break;
    }
  }
  for (const WGPUChainedStruct* chain = descriptor->nextInChain; chain;
       chain = chain->next) {
    if (chain->sType == WGPUSType_TextureComponentSwizzleDescriptor) {
      auto* swizzle =
          reinterpret_cast<const WGPUTextureComponentSwizzleDescriptor*>(
              chain);
      view_info.components = {
          GetComponentSwizzle(swizzle->swizzle.r),
          GetComponentSwizzle(swizzle->swizzle.g),
          GetComponentSwizzle(swizzle->swizzle.b),
          GetComponentSwizzle(swizzle->swizzle.a),
      };
      break;
    }
  }
  view_info.subresourceRange.aspectMask =
      GetFormatAspectMask(descriptor->format, descriptor->aspect);
  view_info.subresourceRange.baseMipLevel = descriptor->baseMipLevel;
  view_info.subresourceRange.levelCount =
      descriptor->mipLevelCount == WGPU_MIP_LEVEL_COUNT_UNDEFINED
          ? VK_REMAINING_MIP_LEVELS
          : descriptor->mipLevelCount;
  view_info.subresourceRange.baseArrayLayer = descriptor->baseArrayLayer;
  view_info.subresourceRange.layerCount =
      descriptor->arrayLayerCount == WGPU_ARRAY_LAYER_COUNT_UNDEFINED
          ? VK_REMAINING_ARRAY_LAYERS
          : descriptor->arrayLayerCount;

  vkCreateImageView(texture_->GetDevice()->GetVkDevice(), &view_info, nullptr,
                    &image_view_);
}

TextureView::~TextureView() {
  if (image_view_)
    vkDestroyImageView(texture_->GetDevice()->GetVkDevice(), image_view_,
                       nullptr);
}

void TextureView::SetLabel(WGPUStringView label) {}

}  // namespace gfx
