// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_shader_module.h"

#include "gfx/gfx_device.h"
#include "gfx/gfx_instance.h"

namespace gfx {

ShaderModule::ShaderModule(RefPtr<Device> device,
                           WGPUShaderModuleDescriptor const * descriptor)
    : device_(std::move(device)) {
  // Only SPIR-V sources are supported; WGSL chains are ignored.
  for (const WGPUChainedStruct* chain = descriptor->nextInChain; chain;
       chain = chain->next) {
    if (chain->sType == WGPUSType_ShaderSourceSPIRV) {
      const WGPUShaderSourceSPIRV* source =
          reinterpret_cast<const WGPUShaderSourceSPIRV*>(chain);
      // codeSize counts 32-bit words.
      code_.assign(source->code, source->code + source->codeSize);
      break;
    }
  }
  reflection_.Reflect(code_.data(), code_.size());
}

ShaderModule::~ShaderModule() = default;

WGPUFuture ShaderModule::GetCompilationInfo(WGPUCompilationInfoCallbackInfo callbackInfo) {
  WGPUCompilationInfo info = {};
  info.messageCount = 0;
  info.messages = nullptr;
  if (callbackInfo.callback) {
    callbackInfo.callback(WGPUCompilationInfoRequestStatus_Success, &info,
                          callbackInfo.userdata1, callbackInfo.userdata2);
  }
  return WGPUFuture{0};
}

void ShaderModule::SetLabel(WGPUStringView label) {
  // Shader modules are destroyed after pipeline creation, so no Vulkan
  // object name is attached; the label is kept for the WebGPU object.
}

}  // namespace gfx
