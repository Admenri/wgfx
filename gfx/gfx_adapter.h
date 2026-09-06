// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUAdapterImpl handle;
// define it here so gfx::Adapter can derive from it, making the
// generated bridge's static_cast<gfx::Adapter*>(handle) a valid
// base->derived downcast.
struct WGPUAdapterImpl {};

namespace gfx {

class Instance;
class Surface;
class Device;

class Adapter : public WGPUAdapterImpl, public RefCounted<Adapter> {
 public:
  Adapter(RefPtr<Instance> instance, VkPhysicalDevice physical);
  ~Adapter();

  Instance* GetInstance() const { return instance_.get(); }
  VkPhysicalDevice GetVkPhysicalDevice() const { return physical_; }
  const VkPhysicalDeviceProperties& GetProperties() const { return properties_; }
  const VkPhysicalDeviceLimits& GetVulkanLimits() const { return limits_; }

  // Whether the adapter's queue family can present on the surface.
  bool SupportsSurface(Surface* surface);
  // Whether the adapter natively supports a WebGPU feature.
  bool SupportsFeature(WGPUFeatureName feature);
  // Vulkan device extensions required to enable |feature|.
  const char* GetFeatureExtension(WGPUFeatureName feature);

  WGPUStatus GetLimits(WGPULimits * limits);
  WGPUBool HasFeature(WGPUFeatureName feature);
  void GetFeatures(WGPUSupportedFeatures * features);
  WGPUStatus GetInfo(WGPUAdapterInfo * info);
  WGPUFuture RequestDevice(WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceCallbackInfo callbackInfo);

 private:
  RefPtr<Instance> instance_;
  VkPhysicalDevice physical_;
  VkPhysicalDeviceProperties properties_;
  VkPhysicalDeviceLimits limits_;

  // Native support bits for WebGPU features beyond the core set.
  bool depth_clip_control_ = false;
  bool depth32_float_stencil8_ = false;
  bool texture_compression_bc_ = false;
  bool texture_compression_etc2_ = false;
  bool texture_compression_astc_ = false;
  bool timestamp_query_ = false;
  bool indirect_first_instance_ = false;
  bool shader_f16_ = false;
  bool rg11b10_ufloat_renderable_ = false;
  bool bgra8_unorm_storage_ = false;
  bool float32_filterable_ = false;
  bool float32_blendable_ = false;
  bool dual_source_blending_ = false;
  bool subgroups_ = false;
  bool primitive_index_ = false;
  bool subgroup_size_control_ = false;
  bool texture_component_swizzle_ = false;
  bool clip_distances_ = false;
  bool texture_formats_tier1_ = false;
  bool texture_formats_tier2_ = false;
};

}  // namespace gfx
