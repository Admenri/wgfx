// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#ifndef GFX_COMMON_SPIRV_REFLECT_H_
#define GFX_COMMON_SPIRV_REFLECT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "webgpu-headers/webgpu.h"

namespace gfx {

// Result of reflecting one descriptor binding out of a SPIR-V module.
struct ReflectedBinding {
  uint32_t set;
  uint32_t binding;
  WGPUShaderStage visibility;
  WGPUBindGroupLayoutEntry layout_entry;
};

struct ReflectedEntryPoint {
  std::string name;
  WGPUShaderStage stage;
};

// Minimal SPIR-V reflection: extracts entry points, resource bindings,
// and the push-constant block size. Assumes well-formed SPIR-V produced
// by a conforming compiler (no validation is performed).
struct SpirvReflection {
  std::vector<ReflectedEntryPoint> entry_points;
  std::vector<ReflectedBinding> bindings;
  uint32_t push_constant_size = 0;
  WGPUShaderStage push_constant_stages = 0;
  WGPUShaderStage module_stages = 0;

  bool Reflect(const uint32_t* code, size_t word_count);

  WGPUShaderStage GetStageForEntryPoint(const std::string& name) const;
};

}  // namespace gfx

#endif  // GFX_COMMON_SPIRV_REFLECT_H_
