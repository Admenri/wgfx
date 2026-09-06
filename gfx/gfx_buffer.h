// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

struct VmaAllocation_T;

// webgpu.h only forward-declares the opaque WGPUBufferImpl handle;
// define it here so gfx::Buffer can derive from it, making the
// generated bridge's static_cast<gfx::Buffer*>(handle) a valid
// base->derived downcast.
struct WGPUBufferImpl {};

namespace gfx {

class Device;

class Buffer : public WGPUBufferImpl, public RefCounted<Buffer> {
 public:
  Buffer(RefPtr<Device> device, WGPUBufferDescriptor const * descriptor);
  ~Buffer();

  VkBuffer GetVkBuffer() const { return buffer_; }
  VmaAllocation_T* GetAllocation() const { return allocation_; }
  uint64_t GetSize() const { return size_; }
  WGPUBufferUsage GetUsage() const { return usage_; }
  // Persistent host mapping, or null for device-local buffers.
  void* GetMappedData() const { return mapped_data_; }

  WGPUFuture MapAsync(WGPUMapMode mode, size_t offset, size_t size, WGPUBufferMapCallbackInfo callbackInfo);
  void * GetMappedRange(size_t offset, size_t size);
  void const * GetConstMappedRange(size_t offset, size_t size);
  WGPUStatus ReadMappedRange(size_t offset, void * data, size_t size);
  WGPUStatus WriteMappedRange(size_t offset, void const * data, size_t size);
  void SetLabel(WGPUStringView label);
  WGPUBufferUsage GetUsage();
  uint64_t GetSize();
  WGPUBufferMapState GetMapState();
  void Unmap();
  void Destroy();

 private:
  RefPtr<Device> device_;
  VkBuffer buffer_ = VK_NULL_HANDLE;
  VmaAllocation_T* allocation_ = nullptr;
  void* mapped_data_ = nullptr;
  uint64_t size_ = 0;
  WGPUBufferUsage usage_ = WGPUBufferUsage_None;
  WGPUBufferMapState map_state_ = WGPUBufferMapState_Unmapped;
};

}  // namespace gfx
