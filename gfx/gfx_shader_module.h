// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"
#include "gfx/common/spirv_reflect.h"

// webgpu.h only forward-declares the opaque WGPUShaderModuleImpl handle;
// define it here so gfx::ShaderModule can derive from it, making the
// generated bridge's static_cast<gfx::ShaderModule*>(handle) a valid
// base->derived downcast.
struct WGPUShaderModuleImpl {};

namespace gfx {

class Device;

class ShaderModule : public WGPUShaderModuleImpl, public RefCounted<ShaderModule> {
 public:
  ShaderModule(RefPtr<Device> device, WGPUShaderModuleDescriptor const * descriptor);
  ~ShaderModule();

  // SPIR-V words of the (first) entry point used by pipelines; the module
  // keeps its full reflection data for automatic layouts.
  const uint32_t* GetCode() const { return code_.data(); }
  size_t GetCodeWordCount() const { return code_.size(); }
  const SpirvReflection& GetReflection() const { return reflection_; }

  WGPUFuture GetCompilationInfo(WGPUCompilationInfoCallbackInfo callbackInfo);
  void SetLabel(WGPUStringView label);

 private:
  RefPtr<Device> device_;
  std::vector<uint32_t> code_;
  SpirvReflection reflection_;
};

}  // namespace gfx
