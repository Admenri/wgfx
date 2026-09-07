// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include <vector>

#include "gfx/gfx_common.h"
#include "gfx/common/ref_holder.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

struct VmaAllocator_T;
struct VmaAllocation_T;
struct VmaAllocationInfo;

// webgpu.h only forward-declares the opaque WGPUDeviceImpl handle;
// define it here so gfx::Device can derive from it, making the
// generated bridge's static_cast<gfx::Device*>(handle) a valid
// base->derived downcast.
struct WGPUDeviceImpl {};

namespace gfx {

class Adapter;
class Instance;
class Queue;

class Device : public WGPUDeviceImpl, public RefCounted<Device> {
 public:
  Device(RefPtr<Adapter> adapter, WGPUDeviceDescriptor const * descriptor);
  ~Device();

  VkDevice GetVkDevice() const { return device_; }
  VkPhysicalDevice GetVkPhysicalDevice() const;
  VkQueue GetVkQueue() const { return queue_; }
  uint32_t GetQueueFamilyIndex() const { return queue_family_; }
  struct VmaAllocator_T* GetVmaAllocator() const { return allocator_; }
  VkCommandPool GetTransientPool() const { return transient_pool_; }
  Instance* GetInstance() const;
  Adapter* GetAdapter() const { return adapter_.get(); }
  const WGPULimits& GetDeviceLimits() const { return limits_; }
  // Whether the feature was requested and enabled on this device.
  bool IsFeatureEnabled(WGPUFeatureName feature) const;
  // Names a Vulkan object for debugging tools (no-op when the debug utils
  // extension is unavailable or the label is empty).
  void SetObjectLabel(uint64_t handle, VkObjectType type,
                      WGPUStringView label);

  // Renders everything in GENERAL layout; no explicit transitions needed.
  static constexpr VkImageLayout kImageLayout = VK_IMAGE_LAYOUT_GENERAL;

  // Submits a one-shot command buffer on the internal pool and waits for
  // it to complete. Used for synchronous queue uploads.
  void RunOneTimeSubmit(std::function<void(VkCommandBuffer)> record);

  // Tracks a submitted command buffer until its fence signals.
  void TrackSubmission(VkFence fence, std::vector<RefHolder> resources);
  // Releases resources of signaled submissions; returns true when the
  // submission identified by |sequence| (or newer) is complete.
  bool PollPendingSubmissions(uint64_t sequence);
  // Blocks until all submissions up to |sequence| complete (or timeout).
  bool WaitForSequence(uint64_t sequence, uint64_t timeout_ns);

  // Semaphore plumbing for surface presentation.
  void SetAcquireSemaphore(VkSemaphore semaphore);
  VkSemaphore AcquireWaitSemaphoreForSubmit();
  void SetRenderDoneSemaphore(VkSemaphore semaphore);
  VkSemaphore TakeRenderDoneSemaphore();

  uint64_t NextSubmissionSequence() { return next_sequence_ + 1; }

  gfx::BindGroup* CreateBindGroup(WGPUBindGroupDescriptor const * descriptor);
  gfx::BindGroupLayout* CreateBindGroupLayout(WGPUBindGroupLayoutDescriptor const * descriptor);
  gfx::Buffer* CreateBuffer(WGPUBufferDescriptor const * descriptor);
  gfx::CommandEncoder* CreateCommandEncoder(WGPUCommandEncoderDescriptor const * descriptor);
  gfx::ComputePipeline* CreateComputePipeline(WGPUComputePipelineDescriptor const * descriptor);
  WGPUFuture CreateComputePipelineAsync(WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncCallbackInfo callbackInfo);
  gfx::PipelineLayout* CreatePipelineLayout(WGPUPipelineLayoutDescriptor const * descriptor);
  gfx::QuerySet* CreateQuerySet(WGPUQuerySetDescriptor const * descriptor);
  WGPUFuture CreateRenderPipelineAsync(WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo);
  gfx::RenderBundleEncoder* CreateRenderBundleEncoder(WGPURenderBundleEncoderDescriptor const * descriptor);
  gfx::RenderPipeline* CreateRenderPipeline(WGPURenderPipelineDescriptor const * descriptor);
  gfx::Sampler* CreateSampler(WGPUSamplerDescriptor const * descriptor);
  gfx::ShaderModule* CreateShaderModule(WGPUShaderModuleDescriptor const * descriptor);
  gfx::Texture* CreateTexture(WGPUTextureDescriptor const * descriptor);
  void Destroy();
  WGPUFuture GetLostFuture();
  WGPUStatus GetLimits(WGPULimits * limits);
  WGPUBool HasFeature(WGPUFeatureName feature);
  void GetFeatures(WGPUSupportedFeatures * features);
  WGPUStatus GetAdapterInfo(WGPUAdapterInfo * adapterInfo);
  gfx::Queue* GetQueue();
  void PushErrorScope(WGPUErrorFilter filter);
  WGPUFuture PopErrorScope(WGPUPopErrorScopeCallbackInfo callbackInfo);
  void SetLabel(WGPUStringView label);

 private:
  void PopulateDefaultLimits();

  RefPtr<Adapter> adapter_;
  VkDevice device_ = VK_NULL_HANDLE;
  uint32_t queue_family_ = 0;
  VkQueue queue_ = VK_NULL_HANDLE;
  struct VmaAllocator_T* allocator_ = nullptr;
  VkCommandPool transient_pool_ = VK_NULL_HANDLE;

  WGPULimits limits_ = {};

  // Feature names requested by the application and enabled on VkDevice.
  std::vector<WGPUFeatureName> enabled_features_;

  RefPtr<Queue> queue_object_;
  RefPtr<Instance> instance_;

  // Outstanding submissions, released once their fence signals.
  struct PendingSubmission {
    uint64_t sequence;
    VkFence fence;
    std::vector<RefHolder> resources;
  };
  std::vector<PendingSubmission> pending_submissions_;
  uint64_t next_sequence_ = 0;

  VkSemaphore acquire_semaphore_ = VK_NULL_HANDLE;
  VkSemaphore render_done_semaphore_ = VK_NULL_HANDLE;
  PFN_vkSetDebugUtilsObjectNameEXT set_object_name_ = nullptr;
};

}  // namespace gfx