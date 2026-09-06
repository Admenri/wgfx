// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_buffer.h"

#include <cstring>

#include "gfx/common/vulkan_conversions.h"
#include "gfx/gfx_device.h"
#include "vk_mem_alloc.h"

namespace gfx {

Buffer::Buffer(RefPtr<Device> device, WGPUBufferDescriptor const * descriptor)
    : device_(std::move(device)),
      size_(descriptor->size),
      usage_(descriptor->usage) {
  VkBufferCreateInfo buffer_info = {};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size_ ? size_ : 16;
  buffer_info.usage = GetBufferUsage(usage_);
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  bool host_visible = (usage_ & (WGPUBufferUsage_MapRead |
                                 WGPUBufferUsage_MapWrite)) != 0 ||
                      descriptor->mappedAtCreation;
  bool random_access = (usage_ & WGPUBufferUsage_MapRead) != 0;
  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.flags = host_visible
                         ? (random_access
                                ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                                : VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) |
                               VMA_ALLOCATION_CREATE_MAPPED_BIT
                         : 0;

  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = nullptr;
  VmaAllocationInfo allocation_info = {};
  vmaCreateBuffer(device_->GetVmaAllocator(), &buffer_info, &alloc_info,
                  &buffer, &allocation, &allocation_info);
  buffer_ = buffer;
  allocation_ = allocation;
  if (host_visible)
    mapped_data_ = allocation_info.pMappedData;
}

Buffer::~Buffer() {
  if (buffer_)
    vmaDestroyBuffer(device_->GetVmaAllocator(), buffer_, allocation_);
}

WGPUFuture Buffer::MapAsync(WGPUMapMode mode, size_t offset, size_t size, WGPUBufferMapCallbackInfo callbackInfo) {
  // Buffers with map usage live in host-visible memory and are persistently
  // mapped at creation, so the map completes synchronously. GPU-written
  // data must be invalidated before the CPU reads it.
  if (mode & WGPUMapMode_Read) {
    vmaInvalidateAllocation(device_->GetVmaAllocator(), allocation_, 0,
                            VK_WHOLE_SIZE);
  }
  map_state_ = WGPUBufferMapState_Mapped;
  if (callbackInfo.callback) {
    callbackInfo.callback(WGPUMapAsyncStatus_Success, WGPU_STRING_VIEW_INIT,
                          callbackInfo.userdata1, callbackInfo.userdata2);
  }
  return WGPUFuture{0};
}

void * Buffer::GetMappedRange(size_t offset, size_t size) {
  if (!mapped_data_)
    return nullptr;
  if (size == WGPU_WHOLE_SIZE)
    size = size_ - offset;
  return static_cast<char*>(mapped_data_) + offset;
}

void const * Buffer::GetConstMappedRange(size_t offset, size_t size) {
  return const_cast<const void*>(GetMappedRange(offset, size));
}

WGPUStatus Buffer::ReadMappedRange(size_t offset, void * data, size_t size) {
  std::memcpy(data, GetMappedRange(offset, size), size);
  return WGPUStatus_Success;
}

WGPUStatus Buffer::WriteMappedRange(size_t offset, void const * data, size_t size) {
  std::memcpy(GetMappedRange(offset, size), data, size);
  return WGPUStatus_Success;
}

void Buffer::SetLabel(WGPUStringView label) {
}

WGPUBufferUsage Buffer::GetUsage() {
  return usage_;
}

uint64_t Buffer::GetSize() {
  return size_;
}

WGPUBufferMapState Buffer::GetMapState() {
  return map_state_;
}

void Buffer::Unmap() {
  map_state_ = WGPUBufferMapState_Unmapped;
}

void Buffer::Destroy() {
  if (buffer_) {
    vmaDestroyBuffer(device_->GetVmaAllocator(), buffer_, allocation_);
    buffer_ = VK_NULL_HANDLE;
    allocation_ = nullptr;
  }
  map_state_ = WGPUBufferMapState_Unmapped;
}

}  // namespace gfx
