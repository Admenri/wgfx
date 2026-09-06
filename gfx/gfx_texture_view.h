// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUTextureViewImpl handle;
// define it here so gfx::TextureView can derive from it, making the
// generated bridge's static_cast<gfx::TextureView*>(handle) a valid
// base->derived downcast.
struct WGPUTextureViewImpl {};

namespace gfx {

class Device;
class Texture;

class TextureView : public WGPUTextureViewImpl, public RefCounted<TextureView> {
 public:
  TextureView(RefPtr<Texture> texture,
              WGPUTextureViewDescriptor const * descriptor);
  ~TextureView();

  VkImageView GetVkImageView() const { return image_view_; }
  Texture* GetTexture() const { return texture_.get(); }
  WGPUTextureAspect GetAspect() const { return aspect_; }

  void SetLabel(WGPUStringView label);

 private:
  RefPtr<Texture> texture_;
  VkImageView image_view_ = VK_NULL_HANDLE;
  WGPUTextureAspect aspect_ = WGPUTextureAspect_All;
};

}  // namespace gfx
