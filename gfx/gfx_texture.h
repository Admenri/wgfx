// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

struct VmaAllocation_T;

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUTextureImpl handle;
// define it here so gfx::Texture can derive from it, making the
// generated bridge's static_cast<gfx::Texture*>(handle) a valid
// base->derived downcast.
struct WGPUTextureImpl {};

namespace gfx {

class Device;

class Texture : public WGPUTextureImpl, public RefCounted<Texture> {
 public:
  Texture(RefPtr<Device> device, WGPUTextureDescriptor const * descriptor);
  // Wraps an externally owned image (swapchain image).
  Texture(RefPtr<Device> device, VkImage image, WGPUTextureFormat format,
          uint32_t width, uint32_t height, WGPUTextureUsage usage);
  ~Texture();

  VkImage GetVkImage() const { return image_; }
  Device* GetDevice() const { return device_.get(); }
  WGPUTextureFormat GetFormat() const { return format_; }
  const WGPUExtent3D& GetExtent() const { return extent_; }
  uint32_t GetMipLevels() const { return mip_level_count_; }
  uint32_t GetSampleCount() const { return sample_count_; }
  WGPUTextureUsage GetTextureUsage() const { return usage_; }
  WGPUTextureDimension GetDimension() const { return dimension_; }

  gfx::TextureView* CreateView(WGPUTextureViewDescriptor const * descriptor);
  void SetLabel(WGPUStringView label);
  uint32_t GetWidth();
  uint32_t GetHeight();
  uint32_t GetDepthOrArrayLayers();
  uint32_t GetMipLevelCount();
  uint32_t GetSampleCount();
  WGPUTextureDimension GetDimension();
  WGPUTextureViewDimension GetTextureBindingViewDimension();
  WGPUTextureFormat GetFormat();
  WGPUTextureUsage GetUsage();
  void Destroy();

 private:
  RefPtr<Device> device_;
  VkImage image_ = VK_NULL_HANDLE;
  VmaAllocation_T* allocation_ = nullptr;
  bool external_image_ = false;
  WGPUExtent3D extent_ = {};
  WGPUTextureFormat format_ = WGPUTextureFormat_Undefined;
  WGPUTextureDimension dimension_ = WGPUTextureDimension_2D;
  uint32_t mip_level_count_ = 1;
  uint32_t sample_count_ = 1;
  WGPUTextureUsage usage_ = WGPUTextureUsage_None;
};

}  // namespace gfx
