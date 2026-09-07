// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUSurfaceImpl handle;
// define it here so gfx::Surface can derive from it, making the
// generated bridge's static_cast<gfx::Surface*>(handle) a valid
// base->derived downcast.
struct WGPUSurfaceImpl {};

namespace gfx {

class Adapter;
class Device;
class Instance;
class Texture;

class Surface : public WGPUSurfaceImpl, public RefCounted<Surface> {
 public:
  explicit Surface(RefPtr<Instance> instance);
  ~Surface();

  void Initialize(WGPUSurfaceDescriptor const * descriptor);

  bool SupportsAdapter(Adapter* adapter);

  void Configure(WGPUSurfaceConfiguration const * config);
  WGPUStatus GetCapabilities(WGPUAdapter adapter, WGPUSurfaceCapabilities * capabilities);
  void GetCurrentTexture(WGPUSurfaceTexture * surfaceTexture);
  WGPUStatus Present();
  void Unconfigure();
  void SetLabel(WGPUStringView label);

 private:
  void DestroySwapchain();

  RefPtr<Instance> instance_;
  RefPtr<Device> device_;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  std::vector<VkImage> images_;
  std::vector<VkImageView> image_views_;
  WGPUTextureFormat format_ = WGPUTextureFormat_Undefined;
  WGPUCompositeAlphaMode alpha_mode_ = WGPUCompositeAlphaMode_Opaque;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint32_t acquired_index_ = 0;
  RefPtr<Texture> acquired_texture_;
  VkCommandPool present_pool_ = VK_NULL_HANDLE;
  WGPUStringView label_ = WGPU_STRING_VIEW_INIT;
};

}  // namespace gfx
