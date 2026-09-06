// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/gfx_fwd.h"

namespace gfx {

class Entry {
 public:
  static gfx::Instance* CreateInstance(WGPUInstanceDescriptor const * descriptor);
  static void GetInstanceFeatures(WGPUSupportedInstanceFeatures * features);
  static WGPUStatus GetInstanceLimits(WGPUInstanceLimits * limits);
  static WGPUBool HasInstanceFeature(WGPUInstanceFeatureName feature);
  static void AdapterInfoFreeMembers(WGPUAdapterInfo value);
  static void SupportedFeaturesFreeMembers(WGPUSupportedFeatures value);
  static void SupportedInstanceFeaturesFreeMembers(WGPUSupportedInstanceFeatures value);
  static void SupportedWGSLLanguageFeaturesFreeMembers(WGPUSupportedWGSLLanguageFeatures value);
  static void SurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities value);
};

}  // namespace gfx
