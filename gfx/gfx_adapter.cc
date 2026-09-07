// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_adapter.h"

#include <algorithm>
#include "gfx/gfx_device.h"
#include "gfx/gfx_instance.h"
#include "gfx/gfx_surface.h"

namespace gfx {

namespace {

bool HasExtension(const std::vector<VkExtensionProperties>& extensions,
                  const char* name) {
  for (const VkExtensionProperties& extension : extensions) {
    if (std::strcmp(extension.extensionName, name) == 0)
      return true;
  }
  return false;
}

bool FormatSupports(VkPhysicalDevice physical, VkFormat format,
                    VkFormatFeatureFlags features) {
  VkFormatProperties properties = {};
  vkGetPhysicalDeviceFormatProperties(physical, format, &properties);
  return (properties.optimalTilingFeatures & features) == features;
}

}  // namespace

Adapter::Adapter(RefPtr<Instance> instance, VkPhysicalDevice physical)
    : instance_(std::move(instance)), physical_(physical) {
  vkGetPhysicalDeviceProperties(physical_, &properties_);
  limits_ = properties_.limits;

  // Query device features and extension support in one pass.
  uint32_t extension_count = 0;
  vkEnumerateDeviceExtensionProperties(physical_, nullptr, &extension_count,
                                       nullptr);
  std::vector<VkExtensionProperties> extensions(extension_count);
  vkEnumerateDeviceExtensionProperties(physical_, nullptr, &extension_count,
                                       extensions.data());

  VkPhysicalDeviceFeatures2 features2 = {};
  features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  VkPhysicalDevice16BitStorageFeatures vulkan11_features = {};
  vulkan11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
  VkPhysicalDeviceShaderDrawParametersFeatures draw_parameters = {};
  draw_parameters.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
  VkPhysicalDeviceShaderFloat16Int8Features float16_int8 = {};
  float16_int8.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
  VkPhysicalDeviceSubgroupSizeControlFeatures subgroup_size_control = {};
  subgroup_size_control.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES;
  VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroup_size_props = {};
  subgroup_size_props.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT;
  features2.pNext = &vulkan11_features;
  vulkan11_features.pNext = &draw_parameters;
  draw_parameters.pNext = &float16_int8;
  float16_int8.pNext = &subgroup_size_control;
  subgroup_size_control.pNext = &subgroup_size_props;
  vkGetPhysicalDeviceFeatures2(physical_, &features2);
  subgroup_min_size_ = subgroup_size_props.minSubgroupSize;
  subgroup_max_size_ = subgroup_size_props.maxSubgroupSize;
  const VkPhysicalDeviceFeatures& features = features2.features;

  VkPhysicalDeviceProperties2 properties2 = {};
  properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  VkPhysicalDeviceSubgroupProperties subgroup_properties = {};
  subgroup_properties.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
  properties2.pNext = &subgroup_properties;
  vkGetPhysicalDeviceProperties2(physical_, &properties2);

  // Map native support to WebGPU features.
  depth_clip_control_ =
      HasExtension(extensions, VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME);
  depth32_float_stencil8_ =
      FormatSupports(physical_, VK_FORMAT_D32_SFLOAT_S8_UINT,
                     VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  texture_compression_bc_ = features.textureCompressionBC != VK_FALSE;
  texture_compression_etc2_ = features.textureCompressionETC2 != VK_FALSE;
  texture_compression_astc_ = features.textureCompressionASTC_LDR != VK_FALSE;

  uint32_t family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_, &family_count, nullptr);
  std::vector<VkQueueFamilyProperties> families(family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_, &family_count,
                                           families.data());
  for (const VkQueueFamilyProperties& family : families) {
    if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
        family.timestampValidBits > 0)
      timestamp_query_ = true;
  }

  indirect_first_instance_ = features.drawIndirectFirstInstance != VK_FALSE;
  shader_f16_ =
      float16_int8.shaderFloat16 != VK_FALSE &&
      vulkan11_features.storageBuffer16BitAccess != VK_FALSE &&
      HasExtension(extensions, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
  rg11b10_ufloat_renderable_ = FormatSupports(
      physical_, VK_FORMAT_B10G11R11_UFLOAT_PACK32,
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
          VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT);
  bgra8_unorm_storage_ =
      FormatSupports(physical_, VK_FORMAT_B8G8R8A8_UNORM,
                     VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);
  float32_filterable_ = FormatSupports(
      physical_, VK_FORMAT_R32_SFLOAT,
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
  float32_blendable_ = FormatSupports(
      physical_, VK_FORMAT_R32G32B32A32_SFLOAT,
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT);
  dual_source_blending_ = features.dualSrcBlend != VK_FALSE;
  subgroups_ =
      (subgroup_properties.supportedStages &
       VK_SHADER_STAGE_COMPUTE_BIT) != 0 &&
      (subgroup_properties.supportedOperations &
       VK_SUBGROUP_FEATURE_ARITHMETIC_BIT) != 0 &&
      (subgroup_properties.supportedOperations &
       VK_SUBGROUP_FEATURE_BASIC_BIT) != 0;
  primitive_index_ = draw_parameters.shaderDrawParameters != VK_FALSE;
  subgroup_size_control_ =
      subgroup_size_control.subgroupSizeControl != VK_FALSE &&
      HasExtension(extensions,
                   VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
  // Texture swizzling is core Vulkan; the feature is freely supported.
  texture_component_swizzle_ = true;

  // ClipDistances requires the shaderClipDistance feature, core in
  // Vulkan 1.3. The driver and loader must both speak 1.3+.
  clip_distances_ = features.shaderClipDistance != VK_FALSE;

  // Format tiers aggregate the individual format capabilities above.
  texture_formats_tier1_ =
      bgra8_unorm_storage_ && rg11b10_ufloat_renderable_ &&
      float32_filterable_ && float32_blendable_;
  texture_formats_tier2_ =
      texture_formats_tier1_ &&
      FormatSupports(physical_, VK_FORMAT_R32G32_SFLOAT,
                     VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                         VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                         VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) &&
      FormatSupports(physical_, VK_FORMAT_R16G16B16A16_SFLOAT,
                     VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                         VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                         VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
}

bool Adapter::SupportsFeature(WGPUFeatureName feature) {
  switch (feature) {
    case WGPUFeatureName_CoreFeaturesAndLimits:
      return true;
    case WGPUFeatureName_DepthClipControl:
      return depth_clip_control_;
    case WGPUFeatureName_Depth32FloatStencil8:
      return depth32_float_stencil8_;
    case WGPUFeatureName_TextureCompressionBC:
      return texture_compression_bc_;
    case WGPUFeatureName_TextureCompressionETC2:
      return texture_compression_etc2_;
    case WGPUFeatureName_TextureCompressionASTC:
      return texture_compression_astc_;
    case WGPUFeatureName_TimestampQuery:
      return timestamp_query_;
    case WGPUFeatureName_IndirectFirstInstance:
      return indirect_first_instance_;
    case WGPUFeatureName_ShaderF16:
      return shader_f16_;
    case WGPUFeatureName_RG11B10UfloatRenderable:
      return rg11b10_ufloat_renderable_;
    case WGPUFeatureName_BGRA8UnormStorage:
      return bgra8_unorm_storage_;
    case WGPUFeatureName_Float32Filterable:
      return float32_filterable_;
    case WGPUFeatureName_Float32Blendable:
      return float32_blendable_;
    case WGPUFeatureName_DualSourceBlending:
      return dual_source_blending_;
    case WGPUFeatureName_Subgroups:
      return subgroups_;
    case WGPUFeatureName_PrimitiveIndex:
      return primitive_index_;
    case WGPUFeatureName_SubgroupSizeControl:
      return subgroup_size_control_;
    case WGPUFeatureName_TextureComponentSwizzle:
      return texture_component_swizzle_;
    case WGPUFeatureName_ClipDistances:
      return clip_distances_;
    case WGPUFeatureName_TextureFormatsTier1:
      return texture_formats_tier1_;
    case WGPUFeatureName_TextureFormatsTier2:
      return texture_formats_tier2_;
    default:
      return false;
  }
}

const char* Adapter::GetFeatureExtension(WGPUFeatureName feature) {
  switch (feature) {
    case WGPUFeatureName_DepthClipControl:
      return VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME;
    case WGPUFeatureName_ShaderF16:
      return VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME;
    case WGPUFeatureName_SubgroupSizeControl:
      return VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME;
    default:
      return nullptr;
  }
}

Adapter::~Adapter() = default;

bool Adapter::SupportsSurface(Surface* surface) {
  return surface->SupportsAdapter(this);
}

WGPUStatus Adapter::GetLimits(WGPULimits * limits) {
  // WebGPU default limits, adjusted with the adapter's Vulkan limits.
  limits->maxTextureDimension1D = 8192;
  limits->maxTextureDimension2D = std::min(8192u, limits_.maxImageDimension2D);
  limits->maxTextureDimension3D = std::min(2048u, limits_.maxImageDimension3D);
  limits->maxTextureArrayLayers = std::min(2048u, limits_.maxImageArrayLayers);
  limits->maxBindGroups = 8;
  limits->maxBindGroupsPlusVertexBuffers = 24;
  limits->maxBindingsPerBindGroup = 1000;
  limits->maxDynamicUniformBuffersPerPipelineLayout = 8;
  limits->maxDynamicStorageBuffersPerPipelineLayout = 8;
  limits->maxSampledTexturesPerShaderStage = 16;
  limits->maxSamplersPerShaderStage = 16;
  limits->maxStorageBuffersPerShaderStage = 8;
  limits->maxStorageTexturesPerShaderStage = 8;
  limits->maxUniformBuffersPerShaderStage = 12;
  limits->maxUniformBufferBindingSize = 65536;
  limits->maxStorageBufferBindingSize = 134217728;
  limits->minUniformBufferOffsetAlignment =
      std::max(uint32_t{256}, static_cast<uint32_t>(limits_.minUniformBufferOffsetAlignment));
  limits->minStorageBufferOffsetAlignment =
      std::max(uint32_t{256}, static_cast<uint32_t>(limits_.minStorageBufferOffsetAlignment));
  limits->maxVertexBuffers = 8;
  limits->maxBufferSize = 268435456;
  limits->maxVertexAttributes = 16;
  limits->maxVertexBufferArrayStride = 2048;
  limits->maxInterStageShaderVariables = 16;
  limits->maxColorAttachments = 8;
  limits->maxColorAttachmentBytesPerSample = 32;
  limits->maxComputeWorkgroupStorageSize = 32768;
  limits->maxComputeInvocationsPerWorkgroup = 256;
  limits->maxComputeWorkgroupSizeX = 256;
  limits->maxComputeWorkgroupSizeY = 256;
  limits->maxComputeWorkgroupSizeZ = 64;
  limits->maxComputeWorkgroupsPerDimension = 65535;
  // Vulkan guarantees at least 128 bytes of push constants.
  limits->maxImmediateSize =
      std::min(128u, limits_.maxPushConstantsSize);
  return WGPUStatus_Success;
}

WGPUBool Adapter::HasFeature(WGPUFeatureName feature) {
  return SupportsFeature(feature);
}

void Adapter::GetFeatures(WGPUSupportedFeatures * features) {
  static const WGPUFeatureName all_features[] = {
      WGPUFeatureName_CoreFeaturesAndLimits,
      WGPUFeatureName_DepthClipControl,
      WGPUFeatureName_Depth32FloatStencil8,
      WGPUFeatureName_TextureCompressionBC,
      WGPUFeatureName_TextureCompressionBCSliced3D,
      WGPUFeatureName_TextureCompressionETC2,
      WGPUFeatureName_TextureCompressionASTC,
      WGPUFeatureName_TextureCompressionASTCSliced3D,
      WGPUFeatureName_TimestampQuery,
      WGPUFeatureName_IndirectFirstInstance,
      WGPUFeatureName_ShaderF16,
      WGPUFeatureName_RG11B10UfloatRenderable,
      WGPUFeatureName_BGRA8UnormStorage,
      WGPUFeatureName_Float32Filterable,
      WGPUFeatureName_Float32Blendable,
      WGPUFeatureName_ClipDistances,
      WGPUFeatureName_DualSourceBlending,
      WGPUFeatureName_Subgroups,
      WGPUFeatureName_PrimitiveIndex,
      WGPUFeatureName_SubgroupSizeControl,
      WGPUFeatureName_TextureComponentSwizzle,
      WGPUFeatureName_TextureFormatsTier1,
      WGPUFeatureName_TextureFormatsTier2,
  };
  static WGPUFeatureName supported[std::size(all_features)];
  uint32_t count = 0;
  for (WGPUFeatureName feature : all_features) {
    if (SupportsFeature(feature))
      supported[count++] = feature;
  }
  features->featureCount = count;
  features->features = supported;
}

WGPUStatus Adapter::GetInfo(WGPUAdapterInfo * info) {
  char* vendor = new char[8];
  std::memcpy(vendor, "unknown", 8);
  info->vendor.data = vendor;
  info->vendor.length = 7;

  char* architecture = new char[8];
  std::memcpy(architecture, "unknown", 8);
  info->architecture.data = architecture;
  info->architecture.length = 7;

  const char* device_name = properties_.deviceName;
  char* device_copy = new char[std::strlen(device_name) + 1];
  std::memcpy(device_copy, device_name, std::strlen(device_name) + 1);
  info->device.data = device_copy;
  info->device.length = std::strlen(device_name);

  char* description = new char[16];
  std::memcpy(description, "Vulkan adapter ", 15);
  info->description.data = description;
  info->description.length = 14;

  info->backendType = WGPUBackendType_Vulkan;
  switch (properties_.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      info->adapterType = WGPUAdapterType_DiscreteGPU;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      info->adapterType = WGPUAdapterType_IntegratedGPU;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      info->adapterType = WGPUAdapterType_CPU;
      break;
    default:
      info->adapterType = WGPUAdapterType_Unknown;
      break;
  }
  info->vendorID = properties_.vendorID;
  info->deviceID = properties_.deviceID;
  info->subgroupMinSize = subgroup_min_size_;
  info->subgroupMaxSize = subgroup_max_size_;
  return WGPUStatus_Success;
}

WGPUFuture Adapter::RequestDevice(WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceCallbackInfo callbackInfo) {
  gfx::Device* device = ToAPIRef(new Device(RefPtr<Adapter>(this), descriptor));

  if (!callbackInfo.callback ||
      callbackInfo.mode == WGPUCallbackMode_AllowSpontaneous) {
    if (callbackInfo.callback) {
      callbackInfo.callback(WGPURequestDeviceStatus_Success, device,
                            WGPU_STRING_VIEW_INIT, callbackInfo.userdata1,
                            callbackInfo.userdata2);
    }
    return WGPUFuture{0};
  }

  gfx::Device* captured_device = device;
  WGPURequestDeviceCallbackInfo captured = callbackInfo;
  uint64_t future_id = instance_->AddFuture([captured, captured_device](uint64_t) {
    if (captured.callback) {
      captured.callback(WGPURequestDeviceStatus_Success, captured_device,
                        WGPU_STRING_VIEW_INIT, captured.userdata1,
                        captured.userdata2);
    }
    return true;
  });
  return WGPUFuture{future_id};
}

}  // namespace gfx
