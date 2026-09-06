// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#ifndef GFX_COMMON_VULKAN_CONVERSIONS_H_
#define GFX_COMMON_VULKAN_CONVERSIONS_H_

#include <cstdint>

#include "volk/volk.h"
#include "webgpu-headers/webgpu.h"

namespace gfx {

struct FormatInfo {
  VkFormat vk_format;
  uint32_t bytes_per_block;
  uint32_t block_width;
  uint32_t block_height;
  bool depth;
  bool stencil;
};

// Returns the Vulkan format and block layout for a WebGPU texture format.
FormatInfo GetFormatInfo(WGPUTextureFormat format);

VkShaderStageFlags GetShaderStageFlags(WGPUShaderStage stages);
VkShaderStageFlagBits GetSingleShaderStage(WGPUShaderStage stage);
VkImageType GetImageType(WGPUTextureDimension dimension);
VkImageViewType GetImageViewType(WGPUTextureViewDimension dimension);
VkImageUsageFlags GetImageUsage(WGPUTextureUsage usage);
// Adds COLOR or DEPTH_STENCIL attachment usage based on the format.
VkImageUsageFlags AddAttachmentUsageForFormat(VkImageUsageFlags usage,
                                               VkFormat format);
VkBufferUsageFlags GetBufferUsage(WGPUBufferUsage usage);
VkIndexType GetIndexType(WGPUIndexFormat format);
VkPrimitiveTopology GetPrimitiveTopology(WGPUPrimitiveTopology topology);
VkFrontFace GetFrontFace(WGPUFrontFace front_face);
VkCullModeFlags GetCullMode(WGPUCullMode cull_mode);
VkCompareOp GetCompareOp(WGPUCompareFunction function);
VkStencilOp GetStencilOp(WGPUStencilOperation operation);
VkBlendFactor GetBlendFactor(WGPUBlendFactor factor);
VkBlendOp GetBlendOp(WGPUBlendOperation operation);
VkFilter GetFilterMode(WGPUFilterMode mode);
VkSamplerMipmapMode GetMipmapFilterMode(WGPUMipmapFilterMode mode);
VkSamplerAddressMode GetAddressMode(WGPUAddressMode mode);
VkFormat GetVertexFormat(WGPUVertexFormat format);
VkQueryType GetQueryType(WGPUQueryType type);
VkPresentModeKHR GetPresentMode(WGPUPresentMode mode);
VkComponentMapping GetAspectComponents(WGPUTextureAspect aspect);
VkComponentSwizzle GetComponentSwizzle(WGPUComponentSwizzle swizzle);
// Aspect mask resolved against the texture format (All resolves to the
// depth/stencil aspects for depth formats).
VkImageAspectFlags GetFormatAspectMask(WGPUTextureFormat format,
                                       WGPUTextureAspect aspect);
VkComponentSwizzle GetComponentSwizzle(WGPUComponentSwizzle swizzle);
// Aspect mask resolved against the texture format (All resolves to the
// depth/stencil aspects for depth formats).
VkImageAspectFlags GetFormatAspectMask(WGPUTextureFormat format,
                                       WGPUTextureAspect aspect);

// Aspect mask for an optional aspect enum (default All).
VkImageAspectFlags GetAspectMask(WGPUTextureAspect aspect);

}  // namespace gfx

#endif  // GFX_COMMON_VULKAN_CONVERSIONS_H_
