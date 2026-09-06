// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_entry.h"

#include <cstdlib>

#include "gfx/gfx_instance.h"

namespace gfx {

gfx::Instance* Entry::CreateInstance(WGPUInstanceDescriptor const * descriptor) {
  return new Instance(descriptor);
}

void Entry::GetInstanceFeatures(WGPUSupportedInstanceFeatures * features) {
  features->featureCount = 0;
  features->features = nullptr;
}

WGPUStatus Entry::GetInstanceLimits(WGPUInstanceLimits * limits) {
  limits->timedWaitAnyMaxCount = 0;
  return WGPUStatus_Success;
}

WGPUBool Entry::HasInstanceFeature(WGPUInstanceFeatureName feature) {
  return feature == WGPUInstanceFeatureName_ShaderSourceSPIRV;
}

void Entry::AdapterInfoFreeMembers(WGPUAdapterInfo value) {
  delete[] value.vendor.data;
  delete[] value.architecture.data;
  delete[] value.device.data;
  delete[] value.description.data;
}

void Entry::SupportedFeaturesFreeMembers(WGPUSupportedFeatures value) {}

void Entry::SupportedInstanceFeaturesFreeMembers(WGPUSupportedInstanceFeatures value) {}

void Entry::SupportedWGSLLanguageFeaturesFreeMembers(WGPUSupportedWGSLLanguageFeatures value) {}

void Entry::SurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities value) {
  free(const_cast<WGPUTextureFormat*>(value.formats));
  free(const_cast<WGPUPresentMode*>(value.presentModes));
  free(const_cast<WGPUCompositeAlphaMode*>(value.alphaModes));
}

}  // namespace gfx
