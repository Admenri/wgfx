// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_texture.h"

#include "gfx/common/vulkan_conversions.h"
#include "gfx/gfx_device.h"
#include "vk_mem_alloc.h"
#include "gfx/gfx_texture_view.h"

namespace gfx {

Texture::Texture(RefPtr<Device> device,
                 WGPUTextureDescriptor const * descriptor)
    : device_(std::move(device)),
      extent_(descriptor->size),
      format_(descriptor->format),
      dimension_(descriptor->dimension),
      mip_level_count_(descriptor->mipLevelCount),
      sample_count_(descriptor->sampleCount),
      usage_(descriptor->usage) {
  FormatInfo format_info = GetFormatInfo(format_);

  VkImageCreateInfo image_info = {};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = GetImageType(dimension_);
  image_info.format = format_info.vk_format;
  image_info.extent.width = extent_.width;
  image_info.extent.height = extent_.height;
  image_info.extent.depth = extent_.depthOrArrayLayers;
  if (dimension_ != WGPUTextureDimension_3D) {
    image_info.arrayLayers = extent_.depthOrArrayLayers;
    image_info.extent.depth = 1;
  }
  image_info.mipLevels = mip_level_count_;
  image_info.samples = static_cast<VkSampleCountFlagBits>(sample_count_);
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = GetImageUsage(usage_);
  if (usage_ & WGPUTextureUsage_RenderAttachment) {
    image_info.usage = AddAttachmentUsageForFormat(
        image_info.usage, format_info.vk_format);
  }
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  vmaCreateImage(device_->GetVmaAllocator(), &image_info, &alloc_info,
                 &image_, &allocation_, nullptr);

  // Transition the image into the layout shared by every operation.
  device_->RunOneTimeSubmit([&](VkCommandBuffer cmd) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = Device::kImageLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image_;
    barrier.subresourceRange.aspectMask =
        GetFormatAspectMask(format_, WGPUTextureAspect_All);
    barrier.subresourceRange.levelCount = mip_level_count_;
    barrier.subresourceRange.layerCount = image_info.arrayLayers;
    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr,
                         1, &barrier);
  });
}

Texture::Texture(RefPtr<Device> device, VkImage image,
                 WGPUTextureFormat format, uint32_t width, uint32_t height,
                 WGPUTextureUsage usage)
    : device_(std::move(device)),
      external_image_(true),
      format_(format),
      usage_(usage) {
  extent_ = {width, height, 1};
  image_ = image;
}

Texture::~Texture() {
  if (image_ && !external_image_)
    vmaDestroyImage(device_->GetVmaAllocator(), image_, allocation_);
}

gfx::TextureView* Texture::CreateView(WGPUTextureViewDescriptor const * descriptor) {
  WGPUTextureViewDescriptor defaults = {};
  if (!descriptor) {
    defaults.format = format_;
    defaults.dimension =
        dimension_ == WGPUTextureDimension_1D
            ? WGPUTextureViewDimension_1D
            : (extent_.depthOrArrayLayers > 1
                   ? WGPUTextureViewDimension_2DArray
                   : WGPUTextureViewDimension_2D);
    defaults.mipLevelCount = mip_level_count_;
    defaults.arrayLayerCount = extent_.depthOrArrayLayers;
    defaults.aspect = WGPUTextureAspect_All;
    descriptor = &defaults;
  }
  return ToAPIRef(new TextureView(RefPtr<Texture>(this), descriptor));
}

void Texture::SetLabel(WGPUStringView label) {}

uint32_t Texture::GetWidth() { return extent_.width; }

uint32_t Texture::GetHeight() { return extent_.height; }

uint32_t Texture::GetDepthOrArrayLayers() {
  return extent_.depthOrArrayLayers;
}

uint32_t Texture::GetMipLevelCount() { return mip_level_count_; }

uint32_t Texture::GetSampleCount() { return sample_count_; }

WGPUTextureDimension Texture::GetDimension() { return dimension_; }

WGPUTextureViewDimension Texture::GetTextureBindingViewDimension() {
  return extent_.depthOrArrayLayers > 1 ? WGPUTextureViewDimension_2DArray
                                        : WGPUTextureViewDimension_2D;
}

WGPUTextureFormat Texture::GetFormat() { return format_; }

WGPUTextureUsage Texture::GetUsage() { return usage_; }

void Texture::Destroy() {
  if (image_ && !external_image_) {
    vmaDestroyImage(device_->GetVmaAllocator(), image_, allocation_);
    allocation_ = nullptr;
  }
  image_ = VK_NULL_HANDLE;
}

}  // namespace gfx
