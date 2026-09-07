// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_device.h"

#include "gfx/common/vulkan_conversions.h"
#include "gfx/gfx_adapter.h"
#include "gfx/gfx_auto_layout.h"
#include "gfx/gfx_bind_group.h"
#include "gfx/gfx_bind_group_layout.h"
#include "gfx/gfx_buffer.h"
#include "gfx/gfx_command_encoder.h"
#include "gfx/gfx_compute_pipeline.h"
#include "gfx/gfx_instance.h"
#include "gfx/gfx_pipeline_layout.h"
#include "gfx/gfx_query_set.h"
#include "gfx/gfx_queue.h"
#include "gfx/gfx_render_bundle_encoder.h"
#include "gfx/gfx_render_pipeline.h"
#include "gfx/gfx_sampler.h"
#include "gfx/gfx_shader_module.h"
#include "gfx/gfx_texture.h"

// Vulkan Memory Allocator implementation (single translation unit).
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

namespace gfx {

Device::Device(RefPtr<Adapter> adapter, WGPUDeviceDescriptor const * descriptor)
    : adapter_(std::move(adapter)) {
  instance_ = RefPtr<Instance>(adapter_->GetInstance());
  VkPhysicalDevice physical = adapter_->GetVkPhysicalDevice();

  // Select a queue family with graphics and compute support.
  uint32_t family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical, &family_count, nullptr);
  std::vector<VkQueueFamilyProperties> families(family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical, &family_count,
                                           families.data());
  for (uint32_t i = 0; i < family_count; ++i) {
    if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
        (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
      queue_family_ = i;
      break;
    }
  }

  // Enable the features the application requested and the adapter
  // supports, along with the Vulkan extensions and feature bits they
  // require.
  std::vector<const char*> device_extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
  enabled_features_.push_back(WGPUFeatureName_CoreFeaturesAndLimits);

  VkPhysicalDeviceFeatures2 features2 = {};
  features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  VkPhysicalDeviceShaderDrawParametersFeatures draw_parameters = {};
  draw_parameters.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
  features2.pNext = &draw_parameters;

  VkPhysicalDevice16BitStorageFeatures vulkan11_features = {};
  vulkan11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
  VkPhysicalDeviceShaderFloat16Int8Features float16_int8 = {};
  float16_int8.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
  VkPhysicalDeviceSubgroupSizeControlFeatures subgroup_size_control = {};
  subgroup_size_control.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES;
  bool enable_float16_int8 = false;
  bool enable_subgroup_size_control = false;
  bool enable_16bit_storage = false;

  if (descriptor) {
    for (size_t i = 0; i < descriptor->requiredFeatureCount; ++i) {
      WGPUFeatureName feature = descriptor->requiredFeatures[i];
      if (feature == WGPUFeatureName_CoreFeaturesAndLimits ||
          !adapter_->SupportsFeature(feature) ||
          IsFeatureEnabled(feature))
        continue;
      enabled_features_.push_back(feature);

      if (const char* extension = adapter_->GetFeatureExtension(feature))
        device_extensions.push_back(extension);

      switch (feature) {
        case WGPUFeatureName_IndirectFirstInstance:
          features2.features.drawIndirectFirstInstance = VK_TRUE;
          break;
        case WGPUFeatureName_DualSourceBlending:
          features2.features.dualSrcBlend = VK_TRUE;
          break;
        case WGPUFeatureName_ShaderF16:
          float16_int8.shaderFloat16 = VK_TRUE;
          enable_float16_int8 = true;
          // float16 storage buffers require 16-bit storage access.
          vulkan11_features.storageBuffer16BitAccess = VK_TRUE;
          enable_16bit_storage = true;
          break;
        case WGPUFeatureName_PrimitiveIndex:
          draw_parameters.shaderDrawParameters = VK_TRUE;
          break;
        case WGPUFeatureName_SubgroupSizeControl:
          subgroup_size_control.subgroupSizeControl = VK_TRUE;
          enable_subgroup_size_control = true;
          break;
        case WGPUFeatureName_ClipDistances:
          features2.features.shaderClipDistance = VK_TRUE;
          break;
        default:
          break;
      }
    }
  }

  // Only feature structs whose extension is enabled may be chained.
  VkBaseOutStructure* feature_chain =
      reinterpret_cast<VkBaseOutStructure*>(&features2);
  if (enable_16bit_storage) {
    feature_chain->pNext =
        reinterpret_cast<VkBaseOutStructure*>(&vulkan11_features);
    feature_chain = feature_chain->pNext;
  }
  if (enable_float16_int8) {
    feature_chain->pNext =
        reinterpret_cast<VkBaseOutStructure*>(&float16_int8);
    feature_chain = feature_chain->pNext;
  }
  if (enable_subgroup_size_control) {
    feature_chain->pNext =
        reinterpret_cast<VkBaseOutStructure*>(&subgroup_size_control);
    feature_chain = feature_chain->pNext;
  }

  float priority = 1.0f;
  VkDeviceQueueCreateInfo queue_info = {};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = queue_family_;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &priority;

  VkDeviceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.queueCreateInfoCount = 1;
  create_info.pQueueCreateInfos = &queue_info;
  create_info.enabledExtensionCount =
      static_cast<uint32_t>(device_extensions.size());
  create_info.ppEnabledExtensionNames = device_extensions.data();
  create_info.pNext = &features2;
  vkCreateDevice(physical, &create_info, nullptr, &device_);
  volkLoadDevice(device_);

  vkGetDeviceQueue(device_, queue_family_, 0, &queue_);

  // VMA with volk's loaded function pointers.
  VmaVulkanFunctions vulkan_functions = {};
  vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  vulkan_functions.vkAllocateMemory = vkAllocateMemory;
  vulkan_functions.vkBindBufferMemory = vkBindBufferMemory;
  vulkan_functions.vkBindImageMemory = vkBindImageMemory;
  vulkan_functions.vkCreateBuffer = vkCreateBuffer;
  vulkan_functions.vkCreateImage = vkCreateImage;
  vulkan_functions.vkDestroyBuffer = vkDestroyBuffer;
  vulkan_functions.vkDestroyImage = vkDestroyImage;
  vulkan_functions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
  vulkan_functions.vkInvalidateMappedMemoryRanges =
      vkInvalidateMappedMemoryRanges;
  vulkan_functions.vkFreeMemory = vkFreeMemory;
  vulkan_functions.vkGetBufferMemoryRequirements =
      vkGetBufferMemoryRequirements;
  vulkan_functions.vkGetImageMemoryRequirements =
      vkGetImageMemoryRequirements;
  vulkan_functions.vkMapMemory = vkMapMemory;
  vulkan_functions.vkUnmapMemory = vkUnmapMemory;

  VmaAllocatorCreateInfo allocator_info = {};
  allocator_info.physicalDevice = physical;
  allocator_info.device = device_;
  allocator_info.instance = instance_->GetVkInstance();
  allocator_info.vulkanApiVersion = instance_->GetAPIVersion();
  allocator_info.pVulkanFunctions = &vulkan_functions;
  vmaCreateAllocator(&allocator_info, &allocator_);

  VkCommandPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex = queue_family_;
  vkCreateCommandPool(device_, &pool_info, nullptr, &transient_pool_);

  PopulateDefaultLimits();

  queue_object_ = RefPtr<Queue>(new Queue(RefPtr<Device>(this)));

  set_object_name_ = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
      vkGetDeviceProcAddr(device_, "vkSetDebugUtilsObjectNameEXT"));
  SetObjectLabel(reinterpret_cast<uint64_t>(device_),
                 VK_OBJECT_TYPE_DEVICE, descriptor ? descriptor->label
                                                   : WGPUStringView{});
  SetObjectLabel(reinterpret_cast<uint64_t>(queue_),
                 VK_OBJECT_TYPE_QUEUE,
                 descriptor ? descriptor->defaultQueue.label
                            : WGPUStringView{});
}

Device::~Device() {
  queue_object_ = nullptr;
  for (PendingSubmission& pending : pending_submissions_)
    vkDestroyFence(device_, pending.fence, nullptr);
  pending_submissions_.clear();
  if (acquire_semaphore_)
    vkDestroySemaphore(device_, acquire_semaphore_, nullptr);
  if (render_done_semaphore_)
    vkDestroySemaphore(device_, render_done_semaphore_, nullptr);
  if (transient_pool_)
    vkDestroyCommandPool(device_, transient_pool_, nullptr);
  if (allocator_)
    vmaDestroyAllocator(allocator_);
  if (device_)
    vkDestroyDevice(device_, nullptr);
}

VkPhysicalDevice Device::GetVkPhysicalDevice() const {
  return adapter_->GetVkPhysicalDevice();
}

Instance* Device::GetInstance() const {
  return instance_.get();
}

void Device::PopulateDefaultLimits() {
  VkPhysicalDeviceProperties properties = {};
  vkGetPhysicalDeviceProperties(adapter_->GetVkPhysicalDevice(), &properties);
  const VkPhysicalDeviceLimits& vk_limits = properties.limits;

  limits_.maxTextureDimension1D = 8192;
  limits_.maxTextureDimension2D = std::min(8192u, vk_limits.maxImageDimension2D);
  limits_.maxTextureDimension3D = std::min(2048u, vk_limits.maxImageDimension3D);
  limits_.maxTextureArrayLayers = std::min(2048u, vk_limits.maxImageArrayLayers);
  limits_.maxBindGroups = 8;
  limits_.maxBindGroupsPlusVertexBuffers = 24;
  limits_.maxBindingsPerBindGroup = 1000;
  limits_.maxDynamicUniformBuffersPerPipelineLayout = 8;
  limits_.maxDynamicStorageBuffersPerPipelineLayout = 8;
  limits_.maxSampledTexturesPerShaderStage = 16;
  limits_.maxSamplersPerShaderStage = 16;
  limits_.maxStorageBuffersPerShaderStage = 8;
  limits_.maxStorageTexturesPerShaderStage = 8;
  limits_.maxUniformBuffersPerShaderStage = 12;
  limits_.maxUniformBufferBindingSize = 65536;
  limits_.maxStorageBufferBindingSize = 134217728;
  limits_.minUniformBufferOffsetAlignment =
      std::max(uint32_t{256}, static_cast<uint32_t>(vk_limits.minUniformBufferOffsetAlignment));
  limits_.minStorageBufferOffsetAlignment =
      std::max(uint32_t{256}, static_cast<uint32_t>(vk_limits.minStorageBufferOffsetAlignment));
  limits_.maxVertexBuffers = 8;
  limits_.maxBufferSize = 268435456;
  limits_.maxVertexAttributes = 16;
  limits_.maxVertexBufferArrayStride = 2048;
  limits_.maxInterStageShaderVariables = 16;
  limits_.maxColorAttachments = 8;
  limits_.maxColorAttachmentBytesPerSample = 32;
  limits_.maxComputeWorkgroupStorageSize = 32768;
  limits_.maxComputeInvocationsPerWorkgroup = 256;
  limits_.maxComputeWorkgroupSizeX = 256;
  limits_.maxComputeWorkgroupSizeY = 256;
  limits_.maxComputeWorkgroupSizeZ = 64;
  limits_.maxComputeWorkgroupsPerDimension = 65535;
  limits_.maxImmediateSize = std::min(128u, vk_limits.maxPushConstantsSize);
}

void Device::RunOneTimeSubmit(std::function<void(VkCommandBuffer)> record) {
  VkCommandBufferAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = transient_pool_;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = 1;
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  vkAllocateCommandBuffers(device_, &alloc_info, &cmd);

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &begin_info);
  record(cmd);
  vkEndCommandBuffer(cmd);

  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd;
  vkQueueSubmit(queue_, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue_);
  vkResetCommandPool(device_, transient_pool_, 0);
}

void Device::TrackSubmission(VkFence fence, std::vector<RefHolder> resources) {
  PendingSubmission pending = {};
  pending.sequence = ++next_sequence_;
  pending.fence = fence;
  pending.resources = std::move(resources);
  pending_submissions_.push_back(std::move(pending));
}

bool Device::PollPendingSubmissions(uint64_t sequence) {
  bool reached_sequence = true;
  for (size_t i = 0; i < pending_submissions_.size();) {
    PendingSubmission& pending = pending_submissions_[i];
    if (vkGetFenceStatus(device_, pending.fence) == VK_SUCCESS) {
      vkDestroyFence(device_, pending.fence, nullptr);
      pending_submissions_.erase(pending_submissions_.begin() + i);
      continue;
    }
    if (pending.sequence <= sequence)
      reached_sequence = false;
    ++i;
  }
  return reached_sequence;
}

bool Device::WaitForSequence(uint64_t sequence, uint64_t timeout_ns) {
  std::vector<VkFence> fences;
  for (const PendingSubmission& pending : pending_submissions_) {
    if (pending.sequence <= sequence)
      fences.push_back(pending.fence);
  }
  if (!fences.empty()) {
    vkWaitForFences(device_, static_cast<uint32_t>(fences.size()),
                    fences.data(), VK_TRUE, timeout_ns);
  }
  return PollPendingSubmissions(sequence);
}

void Device::SetAcquireSemaphore(VkSemaphore semaphore) {
  if (acquire_semaphore_)
    vkDestroySemaphore(device_, acquire_semaphore_, nullptr);
  acquire_semaphore_ = semaphore;
}

VkSemaphore Device::AcquireWaitSemaphoreForSubmit() {
  VkSemaphore semaphore = acquire_semaphore_;
  acquire_semaphore_ = VK_NULL_HANDLE;
  return semaphore;
}

void Device::SetRenderDoneSemaphore(VkSemaphore semaphore) {
  if (render_done_semaphore_)
    vkDestroySemaphore(device_, render_done_semaphore_, nullptr);
  render_done_semaphore_ = semaphore;
}

VkSemaphore Device::TakeRenderDoneSemaphore() {
  VkSemaphore semaphore = render_done_semaphore_;
  render_done_semaphore_ = VK_NULL_HANDLE;
  return semaphore;
}

gfx::BindGroup* Device::CreateBindGroup(WGPUBindGroupDescriptor const * descriptor) {
  return ToAPIRef(new BindGroup(
      RefPtr<Device>(this),
      RefPtr<BindGroupLayout>(static_cast<gfx::BindGroupLayout*>(descriptor->layout)),
      descriptor));
}

gfx::BindGroupLayout* Device::CreateBindGroupLayout(WGPUBindGroupLayoutDescriptor const * descriptor) {
  return ToAPIRef(new BindGroupLayout(RefPtr<Device>(this), descriptor));
}

gfx::Buffer* Device::CreateBuffer(WGPUBufferDescriptor const * descriptor) {
  return ToAPIRef(new Buffer(RefPtr<Device>(this), descriptor));
}

gfx::CommandEncoder* Device::CreateCommandEncoder(WGPUCommandEncoderDescriptor const * descriptor) {
  return ToAPIRef(new CommandEncoder(RefPtr<Device>(this), descriptor));
}

gfx::ComputePipeline* Device::CreateComputePipeline(WGPUComputePipelineDescriptor const * descriptor) {
  return ToAPIRef(new ComputePipeline(RefPtr<Device>(this), descriptor));
}

WGPUFuture Device::CreateComputePipelineAsync(WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncCallbackInfo callbackInfo) {
  gfx::ComputePipeline* pipeline = ToAPIRef(CreateComputePipeline(descriptor));
  if (callbackInfo.callback) {
    callbackInfo.callback(WGPUCreatePipelineAsyncStatus_Success, pipeline,
                          WGPU_STRING_VIEW_INIT, callbackInfo.userdata1,
                          callbackInfo.userdata2);
  }
  return WGPUFuture{0};
}

gfx::PipelineLayout* Device::CreatePipelineLayout(WGPUPipelineLayoutDescriptor const * descriptor) {
  return ToAPIRef(new PipelineLayout(RefPtr<Device>(this), descriptor));
}

gfx::QuerySet* Device::CreateQuerySet(WGPUQuerySetDescriptor const * descriptor) {
  return ToAPIRef(new QuerySet(RefPtr<Device>(this), descriptor));
}

WGPUFuture Device::CreateRenderPipelineAsync(WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo) {
  gfx::RenderPipeline* pipeline = ToAPIRef(CreateRenderPipeline(descriptor));
  if (callbackInfo.callback) {
    callbackInfo.callback(WGPUCreatePipelineAsyncStatus_Success, pipeline,
                          WGPU_STRING_VIEW_INIT, callbackInfo.userdata1,
                          callbackInfo.userdata2);
  }
  return WGPUFuture{0};
}

gfx::RenderBundleEncoder* Device::CreateRenderBundleEncoder(WGPURenderBundleEncoderDescriptor const * descriptor) {
  return ToAPIRef(new RenderBundleEncoder(RefPtr<Device>(this), descriptor));
}

gfx::RenderPipeline* Device::CreateRenderPipeline(WGPURenderPipelineDescriptor const * descriptor) {
  return ToAPIRef(new RenderPipeline(RefPtr<Device>(this), descriptor));
}

gfx::Sampler* Device::CreateSampler(WGPUSamplerDescriptor const * descriptor) {
  return ToAPIRef(new Sampler(RefPtr<Device>(this), descriptor));
}

gfx::ShaderModule* Device::CreateShaderModule(WGPUShaderModuleDescriptor const * descriptor) {
  return ToAPIRef(new ShaderModule(RefPtr<Device>(this), descriptor));
}

gfx::Texture* Device::CreateTexture(WGPUTextureDescriptor const * descriptor) {
  return ToAPIRef(new Texture(RefPtr<Device>(this), descriptor));
}

void Device::Destroy() {
}

WGPUFuture Device::GetLostFuture() {
  return WGPUFuture{0};
}

WGPUStatus Device::GetLimits(WGPULimits * limits) {
  *limits = limits_;
  return WGPUStatus_Success;
}

WGPUBool Device::HasFeature(WGPUFeatureName feature) {
  return IsFeatureEnabled(feature);
}

bool Device::IsFeatureEnabled(WGPUFeatureName feature) const {
  for (WGPUFeatureName enabled : enabled_features_) {
    if (enabled == feature)
      return true;
  }
  return false;
}

void Device::GetFeatures(WGPUSupportedFeatures * features) {
  features->featureCount = static_cast<uint32_t>(enabled_features_.size());
  features->features = enabled_features_.data();
}

WGPUStatus Device::GetAdapterInfo(WGPUAdapterInfo * adapterInfo) {
  return adapter_->GetInfo(adapterInfo);
}

gfx::Queue* Device::GetQueue() {
  queue_object_->AddRef();
  return queue_object_.get();
}

void Device::PushErrorScope(WGPUErrorFilter filter) {
}

WGPUFuture Device::PopErrorScope(WGPUPopErrorScopeCallbackInfo callbackInfo) {
  if (callbackInfo.callback) {
    callbackInfo.callback(WGPUPopErrorScopeStatus_Success, WGPUErrorType_NoError,
                          WGPU_STRING_VIEW_INIT, callbackInfo.userdata1,
                          callbackInfo.userdata2);
  }
  return WGPUFuture{0};
}

void Device::SetLabel(WGPUStringView label) {
  SetObjectLabel(reinterpret_cast<uint64_t>(device_), VK_OBJECT_TYPE_DEVICE,
                 label);
}

void Device::SetObjectLabel(uint64_t handle, VkObjectType type,
                            WGPUStringView label) {
  if (!set_object_name_ || !handle || !label.data || !label.length)
    return;
  std::string name(FromWGPUStringView(label));
  VkDebugUtilsObjectNameInfoEXT info = {};
  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  info.objectType = type;
  info.objectHandle = handle;
  info.pObjectName = name.c_str();
  set_object_name_(device_, &info);
}

}  // namespace gfx
