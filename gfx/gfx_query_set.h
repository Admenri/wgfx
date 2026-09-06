// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUQuerySetImpl handle;
// define it here so gfx::QuerySet can derive from it, making the
// generated bridge's static_cast<gfx::QuerySet*>(handle) a valid
// base->derived downcast.
struct WGPUQuerySetImpl {};

namespace gfx {

class Device;

class QuerySet : public WGPUQuerySetImpl, public RefCounted<QuerySet> {
 public:
  QuerySet(RefPtr<Device> device, WGPUQuerySetDescriptor const * descriptor);
  ~QuerySet();

  VkQueryPool GetVkQueryPool() const { return query_pool_; }
  WGPUQueryType GetType() const { return type_; }
  uint32_t GetCount() const { return count_; }

  void SetLabel(WGPUStringView label);
  WGPUQueryType GetType();
  uint32_t GetCount();
  void Destroy();

 private:
  RefPtr<Device> device_;
  VkQueryPool query_pool_ = VK_NULL_HANDLE;
  WGPUQueryType type_ = WGPUQueryType_Occlusion;
  uint32_t count_ = 0;
};

}  // namespace gfx
