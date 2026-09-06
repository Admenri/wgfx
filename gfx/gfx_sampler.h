// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUSamplerImpl handle;
// define it here so gfx::Sampler can derive from it, making the
// generated bridge's static_cast<gfx::Sampler*>(handle) a valid
// base->derived downcast.
struct WGPUSamplerImpl {};

namespace gfx {

class Device;

class Sampler : public WGPUSamplerImpl, public RefCounted<Sampler> {
 public:
  Sampler(RefPtr<Device> device, WGPUSamplerDescriptor const * descriptor);
  ~Sampler();

  VkSampler GetVkSampler() const { return sampler_; }

  void SetLabel(WGPUStringView label);

 private:
  RefPtr<Device> device_;
  VkSampler sampler_ = VK_NULL_HANDLE;
};

}  // namespace gfx
