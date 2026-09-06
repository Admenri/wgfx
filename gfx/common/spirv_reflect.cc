// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/common/spirv_reflect.h"

#include <unordered_map>

namespace gfx {

namespace {

constexpr uint32_t kSpvMagic = 0x07230203;

// Opcodes used by the reflection walk.
constexpr uint32_t kOpEntryPoint = 15;
constexpr uint32_t kOpTypeInt = 21;
constexpr uint32_t kOpTypeFloat = 22;
constexpr uint32_t kOpTypeVector = 23;
constexpr uint32_t kOpTypeImage = 25;
constexpr uint32_t kOpTypeSampler = 26;
constexpr uint32_t kOpTypeSampledImage = 27;
constexpr uint32_t kOpTypeArray = 28;
constexpr uint32_t kOpTypeRuntimeArray = 29;
constexpr uint32_t kOpTypeStruct = 30;
constexpr uint32_t kOpTypePointer = 32;
constexpr uint32_t kOpConstant = 43;
constexpr uint32_t kOpVariable = 59;
constexpr uint32_t kOpDecorate = 71;
constexpr uint32_t kOpMemberDecorate = 72;

// Decorations.
constexpr uint32_t kDecorationBlock = 2;
constexpr uint32_t kDecorationBufferBlock = 3;
constexpr uint32_t kDecorationOffset = 35;
constexpr uint32_t kDecorationBinding = 33;
constexpr uint32_t kDecorationDescriptorSet = 34;
constexpr uint32_t kDecorationNonWritable = 45;

// Storage classes.
constexpr uint32_t kStorageClassUniformConstant = 0;
constexpr uint32_t kStorageClassInput = 1;
constexpr uint32_t kStorageClassUniform = 2;
constexpr uint32_t kStorageClassOutput = 3;
constexpr uint32_t kStorageClassPushConstant = 9;
constexpr uint32_t kStorageClassStorageBuffer = 12;

// Execution models.
constexpr uint32_t kExecutionModelVertex = 0;
constexpr uint32_t kExecutionModelFragment = 4;
constexpr uint32_t kExecutionModelGLCompute = 5;

// Image dimensionality.
constexpr uint32_t kImageDim1D = 0;
constexpr uint32_t kImageDim2D = 1;
constexpr uint32_t kImageDim3D = 2;
constexpr uint32_t kImageDimCube = 3;

struct ImageInfo {
  uint32_t dim;
  uint32_t depth;
  uint32_t arrayed;
  uint32_t ms;
  uint32_t sampled;
  uint32_t sampled_type_id;
};

enum class ResourceKind {
  kUniformBuffer,
  kStorageBuffer,
  kSampler,
  kSampledTexture,
  kStorageTexture,
};

struct Resource {
  uint32_t set = 0;
  uint32_t binding = 0;
  bool non_writable = false;
  ResourceKind kind = ResourceKind::kUniformBuffer;
  ImageInfo image = {};
  uint32_t image_sampled_type = 0;
};

bool GetLiteralString(const uint32_t* words, uint32_t word_count,
                      uint32_t word_offset, std::string* out) {
  if (word_offset >= word_count)
    return false;
  const char* chars = reinterpret_cast<const char*>(words + word_offset);
  out->assign(chars, strnlen(chars, (word_count - word_offset) * 4));
  return true;
}

}  // namespace

bool SpirvReflection::Reflect(const uint32_t* code, size_t word_count) {
  if (!code || word_count < 5 || code[0] != kSpvMagic)
    return false;

  std::unordered_map<uint32_t, uint32_t> constants;    // id -> value
  std::unordered_map<uint32_t, uint32_t> int_widths;   // int id -> bit width
  std::unordered_map<uint32_t, uint32_t> int_signed;   // int id -> signedness
  std::unordered_map<uint32_t, uint32_t> float_widths;
  std::unordered_map<uint32_t, uint32_t> vector_sizes;  // vector id -> count
  std::unordered_map<uint32_t, uint32_t> vector_element;  // vector id -> elem
  std::unordered_map<uint32_t, uint32_t> array_element;  // array id -> elem type
  std::unordered_map<uint32_t, uint32_t> array_length;   // array id -> const id
  std::unordered_map<uint32_t, std::vector<uint32_t>> struct_members;
  std::unordered_map<uint32_t, uint32_t> struct_flags;  // bit0 Block, bit1 BufferBlock
  std::unordered_map<uint32_t, uint32_t> member_offsets;
  std::unordered_map<uint32_t, ImageInfo> image_types;
  std::unordered_map<uint32_t, uint32_t> sampled_image_types;
  std::unordered_map<uint32_t, uint32_t> pointer_storage;  // ptr -> class
  std::unordered_map<uint32_t, uint32_t> pointer_pointee;  // ptr -> type
  std::unordered_map<uint32_t, Resource> resources;        // var id -> resource
  std::unordered_map<uint32_t, uint32_t> var_pointer;      // var id -> ptr type
  std::unordered_map<uint32_t, bool> descriptor_vars;      // vars with descriptor decorations

  // Two-pass walk: the first pass collects constants, the second resolves
  // types and variables.
  for (size_t i = 5; i < word_count;) {
    uint32_t count = code[i] >> 16;
    if (!count)
      return false;
    if ((code[i] & 0xFFFFu) == kOpConstant && count >= 4)
      constants[code[i + 2]] = code[i + 3];
    i += count;
  }

  std::vector<uint32_t> push_constant_vars;
  for (size_t i = 5; i < word_count;) {
    uint32_t opcode = code[i] & 0xFFFFu;
    uint32_t count = code[i] >> 16;
    if (!count)
      return false;
    const uint32_t* operands = code + i + 1;
    uint32_t operand_count = count - 1;

    switch (opcode) {
      case kOpEntryPoint: {
        ReflectedEntryPoint entry;
        switch (operands[0]) {
          case kExecutionModelVertex:
            entry.stage = WGPUShaderStage_Vertex;
            break;
          case kExecutionModelFragment:
            entry.stage = WGPUShaderStage_Fragment;
            break;
          case kExecutionModelGLCompute:
            entry.stage = WGPUShaderStage_Compute;
            break;
          default:
            entry.stage = WGPUShaderStage_None;
            break;
        }
        module_stages |= entry.stage;
        GetLiteralString(operands, operand_count, 2, &entry.name);
        entry_points.push_back(std::move(entry));
        break;
      }
      case kOpTypeInt:
        int_widths[operands[0]] = operands[1];
        int_signed[operands[0]] = operands[2];
        break;
      case kOpTypeFloat:
        float_widths[operands[0]] = operands[1];
        break;
      case kOpTypeVector:
        vector_sizes[operands[0]] = operands[2];
        vector_element[operands[0]] = operands[1];
        break;
      case kOpTypeArray:
      case kOpTypeRuntimeArray:
        array_element[operands[0]] = operands[1];
        array_length[operands[0]] = operands[2];
        break;
      case kOpTypeStruct:
        struct_members[operands[0]].assign(operands + 1, operands + count - 1);
        break;
      case kOpTypeImage: {
        ImageInfo info = {};
        info.sampled_type_id = operands[1];
        info.dim = operands[2];
        info.depth = operands[3];
        info.arrayed = operands[4];
        info.ms = operands[5];
        info.sampled = operands[6];
        image_types[operands[0]] = info;
        break;
      }
      case kOpTypeSampledImage:
        sampled_image_types[operands[0]] = operands[1];
        break;
      case kOpTypePointer:
        pointer_storage[operands[0]] = operands[1];
        pointer_pointee[operands[0]] = operands[2];
        break;
      case kOpDecorate: {
        uint32_t target = operands[0];
        switch (operands[1]) {
          case kDecorationBinding:
            resources[target].binding = operands[2];
            descriptor_vars[target] = true;
            break;
          case kDecorationDescriptorSet:
            resources[target].set = operands[2];
            descriptor_vars[target] = true;
            break;
          case kDecorationNonWritable:
            resources[target].non_writable = true;
            break;
          case kDecorationBlock:
            struct_flags[target] |= 1;
            break;
          case kDecorationBufferBlock:
            struct_flags[target] |= 2;
            break;
          default:
            break;
        }
        break;
      }
      case kOpMemberDecorate: {
        if (operands[1] == kDecorationOffset)
          member_offsets[(operands[0] << 8) | operands[2]] = operands[3];
        break;
      }
      case kOpVariable: {
        uint32_t result_id = operands[1];
        uint32_t storage_class = pointer_storage[operands[0]];
        uint32_t pointee = pointer_pointee[operands[0]];
        var_pointer[result_id] = operands[0];
        switch (storage_class) {
          case kStorageClassUniform: {
            Resource& r = resources[result_id];
            r.kind = (struct_flags[pointee] & 2) ? ResourceKind::kStorageBuffer
                                                 : ResourceKind::kUniformBuffer;
            break;
          }
          case kStorageClassStorageBuffer: {
            Resource& r = resources[result_id];
            r.kind = ResourceKind::kStorageBuffer;
            break;
          }
          case kStorageClassPushConstant:
            push_constant_vars.push_back(result_id);
            resources.erase(result_id);
            break;
          case kStorageClassUniformConstant: {
            Resource& r = resources[result_id];
            if (image_types.count(pointee)) {
              r.image = image_types[pointee];
              r.image_sampled_type = r.image.sampled_type_id;
              r.kind = r.image.sampled == 2 ? ResourceKind::kStorageTexture
                                            : ResourceKind::kSampledTexture;
            } else if (sampled_image_types.count(pointee)) {
              uint32_t image_id = sampled_image_types[pointee];
              r.image = image_types[image_id];
              r.image_sampled_type = r.image.sampled_type_id;
              r.kind = ResourceKind::kSampledTexture;
            } else {
              r.kind = ResourceKind::kSampler;
            }
            break;
          }
          default:
            resources.erase(result_id);
            break;
        }
        break;
      }
      default:
        break;
    }
    i += count;
  }

  // Recursively compute the byte size of a type (for push constants).
  auto type_size = [&](auto&& self, uint32_t type) -> uint32_t {
    if (int_widths.count(type))
      return (int_widths[type] + 7) / 8;
    if (float_widths.count(type))
      return (float_widths[type] + 7) / 8;
    if (vector_sizes.count(type))
      return vector_sizes[type] * self(self, vector_element[type]);
    return 4;  // Fallback for unknown types.
  };

  // Push constant block size: the maximum member offset + size.
  for (uint32_t var : push_constant_vars) {
    uint32_t block_struct = pointer_pointee[var_pointer[var]];
    uint32_t size = 0;
    const std::vector<uint32_t>& members = struct_members[block_struct];
    for (size_t index = 0; index < members.size(); ++index) {
      uint32_t offset =
          member_offsets[(block_struct << 8) | static_cast<uint32_t>(index)];
      uint32_t member_size = type_size(type_size, members[index]);
      size = std::max(size, offset + member_size);
    }
    push_constant_size = std::max(push_constant_size, size);
  }
  // Emit one layout entry per resource binding. Visibility covers the
  // module's entry point stages (single-stage modules, the common case,
  // get the exact stage).
  for (const auto& [id, resource] : resources) {
    if (!descriptor_vars.count(id))
      continue;  // Push constants and internal variables have no binding.
    ReflectedBinding reflected = {};
    reflected.set = resource.set;
    reflected.binding = resource.binding;
    reflected.visibility = module_stages;

    WGPUBindGroupLayoutEntry& entry = reflected.layout_entry;
    entry.binding = resource.binding;
    entry.visibility = module_stages;
    entry.bindingArraySize = 1;

    switch (resource.kind) {
      case ResourceKind::kUniformBuffer:
        entry.buffer.type = WGPUBufferBindingType_Uniform;
        break;
      case ResourceKind::kStorageBuffer:
        entry.buffer.type = resource.non_writable
                                ? WGPUBufferBindingType_ReadOnlyStorage
                                : WGPUBufferBindingType_Storage;
        break;
      case ResourceKind::kSampler:
        entry.sampler.type = WGPUSamplerBindingType_Filtering;
        break;
      case ResourceKind::kSampledTexture: {
        switch (resource.image.dim) {
          case kImageDim1D:
            entry.texture.viewDimension = WGPUTextureViewDimension_1D;
            break;
          case kImageDim3D:
            entry.texture.viewDimension = WGPUTextureViewDimension_3D;
            break;
          case kImageDimCube:
            entry.texture.viewDimension =
                resource.image.arrayed ? WGPUTextureViewDimension_CubeArray
                                       : WGPUTextureViewDimension_Cube;
            break;
          default:
            entry.texture.viewDimension =
                resource.image.arrayed ? WGPUTextureViewDimension_2DArray
                                       : WGPUTextureViewDimension_2D;
            break;
        }
        entry.texture.multisampled = resource.image.ms ? WGPUBool(true)
                                                       : WGPUBool(false);
        entry.texture.sampleType = WGPUTextureSampleType_Float;
        if (resource.image.depth)
          entry.texture.sampleType = WGPUTextureSampleType_Depth;
        else if (int_widths[resource.image_sampled_type])
          entry.texture.sampleType =
              int_signed[resource.image_sampled_type]
                  ? WGPUTextureSampleType_Sint
                  : WGPUTextureSampleType_Uint;
        else if (float_widths[resource.image_sampled_type] == 32)
          entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
        break;
      }
      case ResourceKind::kStorageTexture: {
        entry.storageTexture.access =
            resource.non_writable ? WGPUStorageTextureAccess_ReadOnly
                                  : WGPUStorageTextureAccess_WriteOnly;
        entry.storageTexture.viewDimension =
            resource.image.arrayed ? WGPUTextureViewDimension_2DArray
                                   : WGPUTextureViewDimension_2D;
        break;
      }
    }
    bindings.push_back(reflected);
  }

  return true;
}

WGPUShaderStage SpirvReflection::GetStageForEntryPoint(
    const std::string& name) const {
  for (const auto& entry : entry_points) {
    if (entry.name == name)
      return entry.stage;
  }
  return WGPUShaderStage_None;
}

}  // namespace gfx
