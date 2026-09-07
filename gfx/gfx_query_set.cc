// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_query_set.h"

#include "gfx/common/vulkan_conversions.h"
#include "vk_mem_alloc.h"
#include "gfx/gfx_device.h"

namespace gfx {

QuerySet::QuerySet(RefPtr<Device> device,
                   WGPUQuerySetDescriptor const * descriptor)
    : device_(std::move(device)),
      type_(descriptor->type),
      count_(descriptor->count) {
  VkQueryPoolCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  create_info.queryType = GetQueryType(type_);
  create_info.queryCount = count_;

  VkQueryPipelineStatisticFlags pipeline_statistics = 0;
  create_info.pipelineStatistics = pipeline_statistics;

  vkCreateQueryPool(device_->GetVkDevice(), &create_info, nullptr,
                    &query_pool_);
  device_->SetObjectLabel(reinterpret_cast<uint64_t>(query_pool_),
                          VK_OBJECT_TYPE_QUERY_POOL, descriptor->label);
}

QuerySet::~QuerySet() {
  if (query_pool_)
    vkDestroyQueryPool(device_->GetVkDevice(), query_pool_, nullptr);
}

void QuerySet::SetLabel(WGPUStringView label) {
  device_->SetObjectLabel(reinterpret_cast<uint64_t>(query_pool_),
                          VK_OBJECT_TYPE_QUERY_POOL, label);
}

WGPUQueryType QuerySet::GetType() { return type_; }

uint32_t QuerySet::GetCount() { return count_; }

void QuerySet::Destroy() {
  if (query_pool_) {
    vkDestroyQueryPool(device_->GetVkDevice(), query_pool_, nullptr);
    query_pool_ = VK_NULL_HANDLE;
  }
}

}  // namespace gfx
