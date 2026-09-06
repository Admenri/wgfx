// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/ref_holder.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPURenderBundleImpl handle;
// define it here so gfx::RenderBundle can derive from it, making the
// generated bridge's static_cast<gfx::RenderBundle*>(handle) a valid
// base->derived downcast.
struct WGPURenderBundleImpl {};

namespace gfx {

class Device;

class RenderBundle : public WGPURenderBundleImpl,
                     public RefCounted<RenderBundle> {
 public:
  RenderBundle(RefPtr<Device> device, VkCommandPool pool,
               VkCommandBuffer buffer);
  ~RenderBundle();

  VkCommandBuffer GetVkCommandBuffer() const { return buffer_; }
  void KeepResource(RefHolder holder);

  void SetLabel(WGPUStringView label);

 private:
  RefPtr<Device> device_;
  VkCommandPool pool_ = VK_NULL_HANDLE;
  VkCommandBuffer buffer_ = VK_NULL_HANDLE;
  std::vector<RefHolder> resources_;
};

}  // namespace gfx
