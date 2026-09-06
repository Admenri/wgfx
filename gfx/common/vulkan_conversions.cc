// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/common/vulkan_conversions.h"

namespace gfx {

FormatInfo GetFormatInfo(WGPUTextureFormat format) {
  switch (format) {
    case WGPUTextureFormat_R8Unorm:
      return {VK_FORMAT_R8_UNORM, 1, 1, 1, false, false};
    case WGPUTextureFormat_R8Snorm:
      return {VK_FORMAT_R8_SNORM, 1, 1, 1, false, false};
    case WGPUTextureFormat_R8Uint:
      return {VK_FORMAT_R8_UINT, 1, 1, 1, false, false};
    case WGPUTextureFormat_R8Sint:
      return {VK_FORMAT_R8_SINT, 1, 1, 1, false, false};
    case WGPUTextureFormat_R16Unorm:
      return {VK_FORMAT_R16_UNORM, 2, 1, 1, false, false};
    case WGPUTextureFormat_R16Snorm:
      return {VK_FORMAT_R16_SNORM, 2, 1, 1, false, false};
    case WGPUTextureFormat_R16Uint:
      return {VK_FORMAT_R16_UINT, 2, 1, 1, false, false};
    case WGPUTextureFormat_R16Sint:
      return {VK_FORMAT_R16_SINT, 2, 1, 1, false, false};
    case WGPUTextureFormat_R16Float:
      return {VK_FORMAT_R16_SFLOAT, 2, 1, 1, false, false};
    case WGPUTextureFormat_RG8Unorm:
      return {VK_FORMAT_R8G8_UNORM, 2, 1, 1, false, false};
    case WGPUTextureFormat_RG8Snorm:
      return {VK_FORMAT_R8G8_SNORM, 2, 1, 1, false, false};
    case WGPUTextureFormat_RG8Uint:
      return {VK_FORMAT_R8G8_UINT, 2, 1, 1, false, false};
    case WGPUTextureFormat_RG8Sint:
      return {VK_FORMAT_R8G8_SINT, 2, 1, 1, false, false};
    case WGPUTextureFormat_R32Float:
      return {VK_FORMAT_R32_SFLOAT, 4, 1, 1, false, false};
    case WGPUTextureFormat_R32Uint:
      return {VK_FORMAT_R32_UINT, 4, 1, 1, false, false};
    case WGPUTextureFormat_R32Sint:
      return {VK_FORMAT_R32_SINT, 4, 1, 1, false, false};
    case WGPUTextureFormat_RG16Unorm:
      return {VK_FORMAT_R16G16_UNORM, 4, 1, 1, false, false};
    case WGPUTextureFormat_RG16Snorm:
      return {VK_FORMAT_R16G16_SNORM, 4, 1, 1, false, false};
    case WGPUTextureFormat_RG16Uint:
      return {VK_FORMAT_R16G16_UINT, 4, 1, 1, false, false};
    case WGPUTextureFormat_RG16Sint:
      return {VK_FORMAT_R16G16_SINT, 4, 1, 1, false, false};
    case WGPUTextureFormat_RG16Float:
      return {VK_FORMAT_R16G16_SFLOAT, 4, 1, 1, false, false};
    case WGPUTextureFormat_RGBA8Unorm:
      return {VK_FORMAT_R8G8B8A8_UNORM, 4, 1, 1, false, false};
    case WGPUTextureFormat_RGBA8UnormSrgb:
      return {VK_FORMAT_R8G8B8A8_SRGB, 4, 1, 1, false, false};
    case WGPUTextureFormat_RGBA8Snorm:
      return {VK_FORMAT_R8G8B8A8_SNORM, 4, 1, 1, false, false};
    case WGPUTextureFormat_RGBA8Uint:
      return {VK_FORMAT_R8G8B8A8_UINT, 4, 1, 1, false, false};
    case WGPUTextureFormat_RGBA8Sint:
      return {VK_FORMAT_R8G8B8A8_SINT, 4, 1, 1, false, false};
    case WGPUTextureFormat_BGRA8Unorm:
      return {VK_FORMAT_B8G8R8A8_UNORM, 4, 1, 1, false, false};
    case WGPUTextureFormat_BGRA8UnormSrgb:
      return {VK_FORMAT_B8G8R8A8_SRGB, 4, 1, 1, false, false};
    case WGPUTextureFormat_RGB10A2Uint:
      return {VK_FORMAT_A2B10G10R10_UINT_PACK32, 4, 1, 1, false, false};
    case WGPUTextureFormat_RGB10A2Unorm:
      return {VK_FORMAT_A2B10G10R10_UNORM_PACK32, 4, 1, 1, false, false};
    case WGPUTextureFormat_RG11B10Ufloat:
      return {VK_FORMAT_B10G11R11_UFLOAT_PACK32, 4, 1, 1, false, false};
    case WGPUTextureFormat_RGB9E5Ufloat:
      return {VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, 4, 1, 1, false, false};
    case WGPUTextureFormat_RG32Float:
      return {VK_FORMAT_R32G32_SFLOAT, 8, 1, 1, false, false};
    case WGPUTextureFormat_RG32Uint:
      return {VK_FORMAT_R32G32_UINT, 8, 1, 1, false, false};
    case WGPUTextureFormat_RG32Sint:
      return {VK_FORMAT_R32G32_SINT, 8, 1, 1, false, false};
    case WGPUTextureFormat_RGBA16Unorm:
      return {VK_FORMAT_R16G16B16A16_UNORM, 8, 1, 1, false, false};
    case WGPUTextureFormat_RGBA16Snorm:
      return {VK_FORMAT_R16G16B16A16_SNORM, 8, 1, 1, false, false};
    case WGPUTextureFormat_RGBA16Uint:
      return {VK_FORMAT_R16G16B16A16_UINT, 8, 1, 1, false, false};
    case WGPUTextureFormat_RGBA16Sint:
      return {VK_FORMAT_R16G16B16A16_SINT, 8, 1, 1, false, false};
    case WGPUTextureFormat_RGBA16Float:
      return {VK_FORMAT_R16G16B16A16_SFLOAT, 8, 1, 1, false, false};
    case WGPUTextureFormat_RGBA32Float:
      return {VK_FORMAT_R32G32B32A32_SFLOAT, 16, 1, 1, false, false};
    case WGPUTextureFormat_RGBA32Uint:
      return {VK_FORMAT_R32G32B32A32_UINT, 16, 1, 1, false, false};
    case WGPUTextureFormat_RGBA32Sint:
      return {VK_FORMAT_R32G32B32A32_SINT, 16, 1, 1, false, false};
    case WGPUTextureFormat_Stencil8:
      return {VK_FORMAT_S8_UINT, 1, 1, 1, false, true};
    case WGPUTextureFormat_Depth16Unorm:
      return {VK_FORMAT_D16_UNORM, 2, 1, 1, true, false};
    case WGPUTextureFormat_Depth24Plus:
      return {VK_FORMAT_D32_SFLOAT, 4, 1, 1, true, false};
    case WGPUTextureFormat_Depth24PlusStencil8:
      return {VK_FORMAT_D32_SFLOAT_S8_UINT, 8, 1, 1, true, true};
    case WGPUTextureFormat_Depth32Float:
      return {VK_FORMAT_D32_SFLOAT, 4, 1, 1, true, false};
    case WGPUTextureFormat_Depth32FloatStencil8:
      return {VK_FORMAT_D32_SFLOAT_S8_UINT, 8, 1, 1, true, true};
    case WGPUTextureFormat_BC1RGBAUnorm:
    case WGPUTextureFormat_BC1RGBAUnormSrgb:
      return {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8, 4, 4, false, false};
    case WGPUTextureFormat_BC2RGBAUnorm:
    case WGPUTextureFormat_BC2RGBAUnormSrgb:
      return {VK_FORMAT_BC2_UNORM_BLOCK, 16, 4, 4, false, false};
    case WGPUTextureFormat_BC3RGBAUnorm:
    case WGPUTextureFormat_BC3RGBAUnormSrgb:
      return {VK_FORMAT_BC3_UNORM_BLOCK, 16, 4, 4, false, false};
    case WGPUTextureFormat_BC4RUnorm:
      return {VK_FORMAT_BC4_UNORM_BLOCK, 8, 4, 4, false, false};
    case WGPUTextureFormat_BC4RSnorm:
      return {VK_FORMAT_BC4_SNORM_BLOCK, 8, 4, 4, false, false};
    case WGPUTextureFormat_BC5RGUnorm:
      return {VK_FORMAT_BC5_UNORM_BLOCK, 16, 4, 4, false, false};
    case WGPUTextureFormat_BC5RGSnorm:
      return {VK_FORMAT_BC5_SNORM_BLOCK, 16, 4, 4, false, false};
    case WGPUTextureFormat_BC6HRGBUfloat:
      return {VK_FORMAT_BC6H_UFLOAT_BLOCK, 16, 4, 4, false, false};
    case WGPUTextureFormat_BC6HRGBFloat:
      return {VK_FORMAT_BC6H_SFLOAT_BLOCK, 16, 4, 4, false, false};
    case WGPUTextureFormat_BC7RGBAUnorm:
    case WGPUTextureFormat_BC7RGBAUnormSrgb:
      return {VK_FORMAT_BC7_UNORM_BLOCK, 16, 4, 4, false, false};
    case WGPUTextureFormat_ETC2RGB8Unorm:
    case WGPUTextureFormat_ETC2RGB8UnormSrgb:
      return {VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, 8, 4, 4, false, false};
    case WGPUTextureFormat_ETC2RGB8A1Unorm:
    case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
      return {VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, 8, 4, 4, false, false};
    case WGPUTextureFormat_ETC2RGBA8Unorm:
    case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
      return {VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, 16, 4, 4, false, false};
    case WGPUTextureFormat_EACR11Unorm:
      return {VK_FORMAT_EAC_R11_UNORM_BLOCK, 8, 4, 4, false, false};
    case WGPUTextureFormat_EACR11Snorm:
      return {VK_FORMAT_EAC_R11_SNORM_BLOCK, 8, 4, 4, false, false};
    case WGPUTextureFormat_EACRG11Unorm:
      return {VK_FORMAT_EAC_R11G11_UNORM_BLOCK, 16, 4, 4, false, false};
    case WGPUTextureFormat_EACRG11Snorm:
      return {VK_FORMAT_EAC_R11G11_SNORM_BLOCK, 16, 4, 4, false, false};
    default:
      if (format >= WGPUTextureFormat_ASTC4x4Unorm &&
          format <= WGPUTextureFormat_ASTC4x4UnormSrgb)
        return {VK_FORMAT_ASTC_4x4_UNORM_BLOCK, 16, 4, 4, false, false};
      if (format >= WGPUTextureFormat_ASTC5x4Unorm &&
          format <= WGPUTextureFormat_ASTC5x4UnormSrgb)
        return {VK_FORMAT_ASTC_5x4_UNORM_BLOCK, 16, 5, 4, false, false};
      if (format >= WGPUTextureFormat_ASTC5x5Unorm &&
          format <= WGPUTextureFormat_ASTC5x5UnormSrgb)
        return {VK_FORMAT_ASTC_5x5_UNORM_BLOCK, 16, 5, 5, false, false};
      if (format >= WGPUTextureFormat_ASTC6x5Unorm &&
          format <= WGPUTextureFormat_ASTC6x5UnormSrgb)
        return {VK_FORMAT_ASTC_6x5_UNORM_BLOCK, 16, 6, 5, false, false};
      if (format >= WGPUTextureFormat_ASTC6x6Unorm &&
          format <= WGPUTextureFormat_ASTC6x6UnormSrgb)
        return {VK_FORMAT_ASTC_6x6_UNORM_BLOCK, 16, 6, 6, false, false};
      if (format >= WGPUTextureFormat_ASTC8x5Unorm &&
          format <= WGPUTextureFormat_ASTC8x5UnormSrgb)
        return {VK_FORMAT_ASTC_8x5_UNORM_BLOCK, 16, 8, 5, false, false};
      if (format >= WGPUTextureFormat_ASTC8x6Unorm &&
          format <= WGPUTextureFormat_ASTC8x6UnormSrgb)
        return {VK_FORMAT_ASTC_8x6_UNORM_BLOCK, 16, 8, 6, false, false};
      if (format >= WGPUTextureFormat_ASTC8x8Unorm &&
          format <= WGPUTextureFormat_ASTC8x8UnormSrgb)
        return {VK_FORMAT_ASTC_8x8_UNORM_BLOCK, 16, 8, 8, false, false};
      if (format >= WGPUTextureFormat_ASTC10x5Unorm &&
          format <= WGPUTextureFormat_ASTC10x5UnormSrgb)
        return {VK_FORMAT_ASTC_10x5_UNORM_BLOCK, 16, 10, 5, false, false};
      if (format >= WGPUTextureFormat_ASTC10x6Unorm &&
          format <= WGPUTextureFormat_ASTC10x6UnormSrgb)
        return {VK_FORMAT_ASTC_10x6_UNORM_BLOCK, 16, 10, 6, false, false};
      if (format >= WGPUTextureFormat_ASTC10x8Unorm &&
          format <= WGPUTextureFormat_ASTC10x8UnormSrgb)
        return {VK_FORMAT_ASTC_10x8_UNORM_BLOCK, 16, 10, 8, false, false};
      if (format >= WGPUTextureFormat_ASTC10x10Unorm &&
          format <= WGPUTextureFormat_ASTC10x10UnormSrgb)
        return {VK_FORMAT_ASTC_10x10_UNORM_BLOCK, 16, 10, 10, false, false};
      if (format >= WGPUTextureFormat_ASTC12x10Unorm &&
          format <= WGPUTextureFormat_ASTC12x10UnormSrgb)
        return {VK_FORMAT_ASTC_12x10_UNORM_BLOCK, 16, 12, 10, false, false};
      if (format >= WGPUTextureFormat_ASTC12x12Unorm &&
          format <= WGPUTextureFormat_ASTC12x12UnormSrgb)
        return {VK_FORMAT_ASTC_12x12_UNORM_BLOCK, 16, 12, 12, false, false};
      return {VK_FORMAT_UNDEFINED, 1, 1, 1, false, false};
  }
}

VkShaderStageFlags GetShaderStageFlags(WGPUShaderStage stages) {
  VkShaderStageFlags flags = 0;
  if (stages & WGPUShaderStage_Vertex)
    flags |= VK_SHADER_STAGE_VERTEX_BIT;
  if (stages & WGPUShaderStage_Fragment)
    flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
  if (stages & WGPUShaderStage_Compute)
    flags |= VK_SHADER_STAGE_COMPUTE_BIT;
  return flags;
}

VkShaderStageFlagBits GetSingleShaderStage(WGPUShaderStage stage) {
  switch (stage) {
    case WGPUShaderStage_Vertex:
      return VK_SHADER_STAGE_VERTEX_BIT;
    case WGPUShaderStage_Fragment:
      return VK_SHADER_STAGE_FRAGMENT_BIT;
    case WGPUShaderStage_Compute:
      return VK_SHADER_STAGE_COMPUTE_BIT;
    default:
      return VK_SHADER_STAGE_ALL;
  }
}

VkImageType GetImageType(WGPUTextureDimension dimension) {
  switch (dimension) {
    case WGPUTextureDimension_1D:
      return VK_IMAGE_TYPE_1D;
    case WGPUTextureDimension_3D:
      return VK_IMAGE_TYPE_3D;
    default:
      return VK_IMAGE_TYPE_2D;
  }
}

VkImageViewType GetImageViewType(WGPUTextureViewDimension dimension) {
  switch (dimension) {
    case WGPUTextureViewDimension_1D:
      return VK_IMAGE_VIEW_TYPE_1D;
    case WGPUTextureViewDimension_2DArray:
      return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case WGPUTextureViewDimension_Cube:
      return VK_IMAGE_VIEW_TYPE_CUBE;
    case WGPUTextureViewDimension_CubeArray:
      return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    case WGPUTextureViewDimension_3D:
      return VK_IMAGE_VIEW_TYPE_3D;
    default:
      return VK_IMAGE_VIEW_TYPE_2D;
  }
}

VkImageUsageFlags GetImageUsage(WGPUTextureUsage usage) {
  VkImageUsageFlags flags = 0;
  if (usage & WGPUTextureUsage_CopySrc)
    flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  if (usage & WGPUTextureUsage_CopyDst)
    flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (usage & WGPUTextureUsage_TextureBinding)
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  if (usage & WGPUTextureUsage_StorageBinding)
    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  return flags;
}

VkImageUsageFlags AddAttachmentUsageForFormat(VkImageUsageFlags usage,
                                              VkFormat format) {
  FormatInfo info = GetFormatInfo(static_cast<WGPUTextureFormat>(0));
  (void)info;
  // Depth/stencil formats take the depth-stencil attachment usage; all
  // others take the color attachment usage.
  switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return usage | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    default:
      return usage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
}

VkBufferUsageFlags GetBufferUsage(WGPUBufferUsage usage) {
  VkBufferUsageFlags flags = 0;
  if (usage & WGPUBufferUsage_CopySrc)
    flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  if (usage & WGPUBufferUsage_CopyDst)
    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  if (usage & WGPUBufferUsage_MapRead)
    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  if (usage & WGPUBufferUsage_MapWrite)
    flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  if (usage & WGPUBufferUsage_Uniform)
    flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  if (usage & WGPUBufferUsage_Storage)
    flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  if (usage & WGPUBufferUsage_Index)
    flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  if (usage & WGPUBufferUsage_Vertex)
    flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  if (usage & WGPUBufferUsage_Indirect)
    flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  if (usage & WGPUBufferUsage_QueryResolve)
    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  return flags;
}

VkIndexType GetIndexType(WGPUIndexFormat format) {
  return format == WGPUIndexFormat_Uint32 ? VK_INDEX_TYPE_UINT32
                                          : VK_INDEX_TYPE_UINT16;
}

VkPrimitiveTopology GetPrimitiveTopology(WGPUPrimitiveTopology topology) {
  switch (topology) {
    case WGPUPrimitiveTopology_PointList:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case WGPUPrimitiveTopology_LineList:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case WGPUPrimitiveTopology_LineStrip:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case WGPUPrimitiveTopology_TriangleStrip:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    default:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }
}

VkFrontFace GetFrontFace(WGPUFrontFace front_face) {
  // The implementation renders with a negative-height viewport, which
  // mirrors the winding order, so the front face is inverted.
  return front_face == WGPUFrontFace_CW ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                                        : VK_FRONT_FACE_CLOCKWISE;
}

VkCullModeFlags GetCullMode(WGPUCullMode cull_mode) {
  switch (cull_mode) {
    case WGPUCullMode_Front:
      return VK_CULL_MODE_FRONT_BIT;
    case WGPUCullMode_Back:
      return VK_CULL_MODE_BACK_BIT;
    default:
      return VK_CULL_MODE_NONE;
  }
}

VkCompareOp GetCompareOp(WGPUCompareFunction function) {
  switch (function) {
    case WGPUCompareFunction_Never:
      return VK_COMPARE_OP_NEVER;
    case WGPUCompareFunction_Less:
      return VK_COMPARE_OP_LESS;
    case WGPUCompareFunction_Equal:
      return VK_COMPARE_OP_EQUAL;
    case WGPUCompareFunction_LessEqual:
      return VK_COMPARE_OP_LESS_OR_EQUAL;
    case WGPUCompareFunction_Greater:
      return VK_COMPARE_OP_GREATER;
    case WGPUCompareFunction_NotEqual:
      return VK_COMPARE_OP_NOT_EQUAL;
    case WGPUCompareFunction_GreaterEqual:
      return VK_COMPARE_OP_GREATER_OR_EQUAL;
    default:
      return VK_COMPARE_OP_ALWAYS;
  }
}

VkStencilOp GetStencilOp(WGPUStencilOperation operation) {
  switch (operation) {
    case WGPUStencilOperation_Zero:
      return VK_STENCIL_OP_ZERO;
    case WGPUStencilOperation_Replace:
      return VK_STENCIL_OP_REPLACE;
    case WGPUStencilOperation_Invert:
      return VK_STENCIL_OP_INVERT;
    case WGPUStencilOperation_IncrementClamp:
      return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case WGPUStencilOperation_DecrementClamp:
      return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case WGPUStencilOperation_IncrementWrap:
      return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case WGPUStencilOperation_DecrementWrap:
      return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    default:
      return VK_STENCIL_OP_KEEP;
  }
}

VkBlendFactor GetBlendFactor(WGPUBlendFactor factor) {
  switch (factor) {
    case WGPUBlendFactor_Zero:
      return VK_BLEND_FACTOR_ZERO;
    case WGPUBlendFactor_One:
      return VK_BLEND_FACTOR_ONE;
    case WGPUBlendFactor_Src:
      return VK_BLEND_FACTOR_SRC_COLOR;
    case WGPUBlendFactor_OneMinusSrc:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case WGPUBlendFactor_SrcAlpha:
      return VK_BLEND_FACTOR_SRC_ALPHA;
    case WGPUBlendFactor_OneMinusSrcAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case WGPUBlendFactor_Dst:
      return VK_BLEND_FACTOR_DST_COLOR;
    case WGPUBlendFactor_OneMinusDst:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case WGPUBlendFactor_DstAlpha:
      return VK_BLEND_FACTOR_DST_ALPHA;
    case WGPUBlendFactor_OneMinusDstAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case WGPUBlendFactor_SrcAlphaSaturated:
      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case WGPUBlendFactor_Constant:
      return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case WGPUBlendFactor_OneMinusConstant:
      return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case WGPUBlendFactor_Src1:
      return VK_BLEND_FACTOR_SRC1_COLOR;
    case WGPUBlendFactor_OneMinusSrc1:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case WGPUBlendFactor_Src1Alpha:
      return VK_BLEND_FACTOR_SRC1_ALPHA;
    case WGPUBlendFactor_OneMinusSrc1Alpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    default:
      return VK_BLEND_FACTOR_ZERO;
  }
}

VkBlendOp GetBlendOp(WGPUBlendOperation operation) {
  switch (operation) {
    case WGPUBlendOperation_Subtract:
      return VK_BLEND_OP_SUBTRACT;
    case WGPUBlendOperation_ReverseSubtract:
      return VK_BLEND_OP_REVERSE_SUBTRACT;
    case WGPUBlendOperation_Min:
      return VK_BLEND_OP_MIN;
    case WGPUBlendOperation_Max:
      return VK_BLEND_OP_MAX;
    default:
      return VK_BLEND_OP_ADD;
  }
}

VkFilter GetFilterMode(WGPUFilterMode mode) {
  return mode == WGPUFilterMode_Linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
}

VkSamplerMipmapMode GetMipmapFilterMode(WGPUMipmapFilterMode mode) {
  return mode == WGPUMipmapFilterMode_Linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                                             : VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

VkSamplerAddressMode GetAddressMode(WGPUAddressMode mode) {
  switch (mode) {
    case WGPUAddressMode_Repeat:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case WGPUAddressMode_MirrorRepeat:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    default:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
}

VkFormat GetVertexFormat(WGPUVertexFormat format) {
  switch (format) {
    case WGPUVertexFormat_Uint8:
      return VK_FORMAT_R8_UINT;
    case WGPUVertexFormat_Uint8x2:
      return VK_FORMAT_R8G8_UINT;
    case WGPUVertexFormat_Uint8x4:
      return VK_FORMAT_R8G8B8A8_UINT;
    case WGPUVertexFormat_Sint8:
      return VK_FORMAT_R8_SINT;
    case WGPUVertexFormat_Sint8x2:
      return VK_FORMAT_R8G8_SINT;
    case WGPUVertexFormat_Sint8x4:
      return VK_FORMAT_R8G8B8A8_SINT;
    case WGPUVertexFormat_Unorm8:
      return VK_FORMAT_R8_UNORM;
    case WGPUVertexFormat_Unorm8x2:
      return VK_FORMAT_R8G8_UNORM;
    case WGPUVertexFormat_Unorm8x4:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case WGPUVertexFormat_Snorm8:
      return VK_FORMAT_R8_SNORM;
    case WGPUVertexFormat_Snorm8x2:
      return VK_FORMAT_R8G8_SNORM;
    case WGPUVertexFormat_Snorm8x4:
      return VK_FORMAT_R8G8B8A8_SNORM;
    case WGPUVertexFormat_Uint16:
      return VK_FORMAT_R16_UINT;
    case WGPUVertexFormat_Uint16x2:
      return VK_FORMAT_R16G16_UINT;
    case WGPUVertexFormat_Uint16x4:
      return VK_FORMAT_R16G16B16A16_UINT;
    case WGPUVertexFormat_Sint16:
      return VK_FORMAT_R16_SINT;
    case WGPUVertexFormat_Sint16x2:
      return VK_FORMAT_R16G16_SINT;
    case WGPUVertexFormat_Sint16x4:
      return VK_FORMAT_R16G16B16A16_SINT;
    case WGPUVertexFormat_Unorm16:
      return VK_FORMAT_R16_UNORM;
    case WGPUVertexFormat_Unorm16x2:
      return VK_FORMAT_R16G16_UNORM;
    case WGPUVertexFormat_Unorm16x4:
      return VK_FORMAT_R16G16B16A16_UNORM;
    case WGPUVertexFormat_Snorm16:
      return VK_FORMAT_R16_SNORM;
    case WGPUVertexFormat_Snorm16x2:
      return VK_FORMAT_R16G16_SNORM;
    case WGPUVertexFormat_Snorm16x4:
      return VK_FORMAT_R16G16B16A16_SNORM;
    case WGPUVertexFormat_Float16:
      return VK_FORMAT_R16_SFLOAT;
    case WGPUVertexFormat_Float16x2:
      return VK_FORMAT_R16G16_SFLOAT;
    case WGPUVertexFormat_Float16x4:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case WGPUVertexFormat_Float32:
      return VK_FORMAT_R32_SFLOAT;
    case WGPUVertexFormat_Float32x2:
      return VK_FORMAT_R32G32_SFLOAT;
    case WGPUVertexFormat_Float32x3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case WGPUVertexFormat_Float32x4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case WGPUVertexFormat_Uint32:
      return VK_FORMAT_R32_UINT;
    case WGPUVertexFormat_Uint32x2:
      return VK_FORMAT_R32G32_UINT;
    case WGPUVertexFormat_Uint32x3:
      return VK_FORMAT_R32G32B32_UINT;
    case WGPUVertexFormat_Uint32x4:
      return VK_FORMAT_R32G32B32A32_UINT;
    case WGPUVertexFormat_Sint32:
      return VK_FORMAT_R32_SINT;
    case WGPUVertexFormat_Sint32x2:
      return VK_FORMAT_R32G32_SINT;
    case WGPUVertexFormat_Sint32x3:
      return VK_FORMAT_R32G32B32_SINT;
    case WGPUVertexFormat_Sint32x4:
      return VK_FORMAT_R32G32B32A32_SINT;
    case WGPUVertexFormat_Unorm10_10_10_2:
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case WGPUVertexFormat_Unorm8x4BGRA:
      return VK_FORMAT_B8G8R8A8_UNORM;
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

VkQueryType GetQueryType(WGPUQueryType type) {
  return type == WGPUQueryType_Timestamp ? VK_QUERY_TYPE_TIMESTAMP
                                         : VK_QUERY_TYPE_OCCLUSION;
}

VkPresentModeKHR GetPresentMode(WGPUPresentMode mode) {
  switch (mode) {
    case WGPUPresentMode_Immediate:
      return VK_PRESENT_MODE_IMMEDIATE_KHR;
    case WGPUPresentMode_Mailbox:
      return VK_PRESENT_MODE_MAILBOX_KHR;
    case WGPUPresentMode_FifoRelaxed:
      return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    default:
      return VK_PRESENT_MODE_FIFO_KHR;
  }
}

VkComponentMapping GetAspectComponents(WGPUTextureAspect aspect) {
  switch (aspect) {
    case WGPUTextureAspect_DepthOnly:
      return {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R,
              VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R};
    case WGPUTextureAspect_StencilOnly:
      return {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R,
              VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R};
    default:
      return {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
              VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
  }
}

VkComponentSwizzle GetComponentSwizzle(WGPUComponentSwizzle swizzle) {
  switch (swizzle) {
    case WGPUComponentSwizzle_Zero:
      return VK_COMPONENT_SWIZZLE_ZERO;
    case WGPUComponentSwizzle_One:
      return VK_COMPONENT_SWIZZLE_ONE;
    case WGPUComponentSwizzle_R:
      return VK_COMPONENT_SWIZZLE_R;
    case WGPUComponentSwizzle_G:
      return VK_COMPONENT_SWIZZLE_G;
    case WGPUComponentSwizzle_B:
      return VK_COMPONENT_SWIZZLE_B;
    case WGPUComponentSwizzle_A:
      return VK_COMPONENT_SWIZZLE_A;
    default:
      return VK_COMPONENT_SWIZZLE_IDENTITY;
  }
}

VkImageAspectFlags GetFormatAspectMask(WGPUTextureFormat format,
                                       WGPUTextureAspect aspect) {
  if (aspect != WGPUTextureAspect_All)
    return GetAspectMask(aspect);
  FormatInfo info = GetFormatInfo(format);
  VkImageAspectFlags mask = 0;
  if (info.depth)
    mask |= VK_IMAGE_ASPECT_DEPTH_BIT;
  if (info.stencil)
    mask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  if (!mask)
    mask = VK_IMAGE_ASPECT_COLOR_BIT;
  return mask;
}

VkImageAspectFlags GetAspectMask(WGPUTextureAspect aspect) {
  switch (aspect) {
    case WGPUTextureAspect_DepthOnly:
      return VK_IMAGE_ASPECT_DEPTH_BIT;
    case WGPUTextureAspect_StencilOnly:
      return VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
      return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

}  // namespace gfx
