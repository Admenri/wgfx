// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUQueueImpl handle;
// define it here so gfx::Queue can derive from it, making the
// generated bridge's static_cast<gfx::Queue*>(handle) a valid
// base->derived downcast.
struct WGPUQueueImpl {};

namespace gfx {

class Device;

class Queue : public WGPUQueueImpl, public RefCounted<Queue> {
 public:
  explicit Queue(RefPtr<Device> device);
  ~Queue();

  Device* GetDevice() const { return device_.get(); }

  void Submit(size_t commandCount, WGPUCommandBuffer const * commands);
  WGPUFuture OnSubmittedWorkDone(WGPUQueueWorkDoneCallbackInfo callbackInfo);
  void WriteBuffer(WGPUBuffer buffer, uint64_t bufferOffset, void const * data, size_t size);
  void WriteTexture(WGPUTexelCopyTextureInfo const * destination, void const * data, size_t dataSize, WGPUTexelCopyBufferLayout const * dataLayout, WGPUExtent3D const * writeSize);
  void SetLabel(WGPUStringView label);

 private:
  // Uploads |data| into |buffer| through a staging buffer.
  void UploadBuffer(WGPUBuffer buffer, uint64_t bufferOffset,
                    void const * data, size_t size);

  RefPtr<Device> device_;
};

}  // namespace gfx
