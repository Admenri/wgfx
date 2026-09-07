// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_instance.h"

#include "gfx/gfx_adapter.h"
#include "gfx/gfx_device.h"
#include "gfx/gfx_entry.h"
#include "gfx/gfx_surface.h"

namespace gfx {

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data) {
  (void)type;
  (void)user_data;
  std::printf("[vulkan] %s\n", data->pMessage);
  std::fflush(stdout);
  return VK_FALSE;
}

}  // namespace

Instance::Instance(WGPUInstanceDescriptor const * descriptor) {
  volkInitialize();

  // Use the newest API version the loader and Vulkan SDK support, capped
  // at 1.3; 1.1 devices keep working because device feature enabling is
  // driven by what the physical device reports, not by this constant.
  uint32_t loader_version = VK_API_VERSION_1_1;
  vkEnumerateInstanceVersion(&loader_version);
  api_version_ = std::min(loader_version, VK_API_VERSION_1_3);

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.apiVersion = api_version_;

  const char* extensions[] = {
      VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(_WIN32)
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
      VK_KHR_XCB_SURFACE_EXTENSION_NAME,
      VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
      VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
  };

  VkInstanceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  create_info.enabledExtensionCount = 1;
#if defined(_WIN32)
  create_info.enabledExtensionCount = 2;
#elif defined(__linux__)
  create_info.enabledExtensionCount = 4;
#endif
  create_info.ppEnabledExtensionNames = extensions;

#if !defined(NDEBUG) && !defined(WGFX_NO_VALIDATION)
  // Validation messages surface API misuse in debug builds only. Define
  // WGFX_NO_VALIDATION to opt out (e.g. for API-stress benchmarks).
  const char* validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
  create_info.enabledLayerCount = 1;
  create_info.ppEnabledLayerNames = validation_layers;
#endif

  vkCreateInstance(&create_info, nullptr, &instance_);
  volkLoadInstance(instance_);

#if !defined(NDEBUG)
  VkDebugUtilsMessengerCreateInfoEXT messenger_info = {};
  messenger_info.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  messenger_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  messenger_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  messenger_info.pfnUserCallback = DebugMessageCallback;
  auto vkCreateDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
          vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
  if (vkCreateDebugUtilsMessengerEXT)
    vkCreateDebugUtilsMessengerEXT(instance_, &messenger_info, nullptr,
                                   &debug_messenger_);
#endif
}

Instance::~Instance() {
  if (instance_)
    vkDestroyInstance(instance_, nullptr);
}

uint64_t Instance::AddFuture(std::function<bool(uint64_t)> try_fire) {
  uint64_t id = next_future_id_.fetch_add(1);
  std::lock_guard<std::mutex> lock(future_mutex_);
  futures_[id] = std::move(try_fire);
  return id;
}

bool Instance::TryFireFuture(uint64_t future_id, uint64_t timeout_ns) {
  std::function<bool(uint64_t)> fire;
  {
    std::lock_guard<std::mutex> lock(future_mutex_);
    auto it = futures_.find(future_id);
    if (it == futures_.end())
      return true;  // Already fired.
    fire = it->second;
  }
  if (!fire || !fire(timeout_ns))
    return false;
  std::lock_guard<std::mutex> lock(future_mutex_);
  futures_.erase(future_id);
  return true;
}

gfx::Surface* Instance::CreateSurface(WGPUSurfaceDescriptor const * descriptor) {
  gfx::Surface* surface = new Surface(RefPtr<Instance>(this));
  surface->Initialize(descriptor);
  surface->SetLabel(descriptor ? descriptor->label : WGPUStringView{});
  return ToAPIRef(surface);
}

void Instance::GetWGSLLanguageFeatures(WGPUSupportedWGSLLanguageFeatures * features) {
  features->featureCount = 0;
  features->features = nullptr;
}

WGPUBool Instance::HasWGSLLanguageFeature(WGPUWGSLLanguageFeatureName feature) {
  return WGPUBool(false);
}

void Instance::ProcessEvents() {
  std::vector<uint64_t> pending;
  {
    std::lock_guard<std::mutex> lock(future_mutex_);
    pending.reserve(futures_.size());
    for (const auto& [id, fire] : futures_)
      pending.push_back(id);
  }
  for (uint64_t id : pending)
    TryFireFuture(id, 0);
}

WGPUFuture Instance::RequestAdapter(WGPURequestAdapterOptions const * options, WGPURequestAdapterCallbackInfo callbackInfo) {
  gfx::Adapter* selected = nullptr;

  // Honor the adapter filters wgfx can express. There is no software
  // fallback adapter; compatibility mode and non-Vulkan backends are not
  // implemented, so those requests yield no adapter.
  bool wants_fallback = options && options->forceFallbackAdapter;
  bool backend_supported =
      !options || options->backendType == WGPUBackendType_Undefined ||
      options->backendType == WGPUBackendType_Vulkan;
  bool feature_level_supported =
      !options || options->featureLevel != WGPUFeatureLevel_Compatibility;
  bool prefer_low_power =
      options && options->powerPreference == WGPUPowerPreference_LowPower;

  uint32_t device_count = 0;
  if (!wants_fallback && backend_supported && feature_level_supported)
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

  if (device_count) {
    std::vector<VkPhysicalDevice> physical_devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count,
                               physical_devices.data());

    gfx::Surface* compatible_surface = nullptr;
    if (options && options->compatibleSurface)
      compatible_surface = static_cast<gfx::Surface*>(options->compatibleSurface);

    VkPhysicalDeviceType preferred_type =
        prefer_low_power ? VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
                         : VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    for (VkPhysicalDevice physical : physical_devices) {
      gfx::Adapter* candidate =
          ToAPIRef(new Adapter(RefPtr<Instance>(this), physical));
      if (!compatible_surface || candidate->SupportsSurface(compatible_surface)) {
        if (selected) {
          bool better = candidate->GetProperties().deviceType ==
                            preferred_type &&
                        selected->GetProperties().deviceType != preferred_type;
          if (better) {
            selected->Release();
            selected = candidate;
            continue;
          }
          candidate->Release();
          continue;
        }
        selected = candidate;
        continue;
      }
      candidate->Release();
    }
  }

  // Fire the callback directly when allowed to run spontaneously.
  if (!callbackInfo.callback ||
      callbackInfo.mode == WGPUCallbackMode_AllowSpontaneous) {
    if (callbackInfo.callback) {
      callbackInfo.callback(
          selected ? WGPURequestAdapterStatus_Success
                   : WGPURequestAdapterStatus_Unavailable,
          selected, WGPU_STRING_VIEW_INIT, callbackInfo.userdata1,
          callbackInfo.userdata2);
    }
    return WGPUFuture{0};
  }

  gfx::Adapter* captured_adapter = selected;
  WGPURequestAdapterCallbackInfo captured = callbackInfo;
  uint64_t future_id = AddFuture([captured, captured_adapter](uint64_t) {
    if (captured.callback) {
      captured.callback(
          captured_adapter ? WGPURequestAdapterStatus_Success
                           : WGPURequestAdapterStatus_Unavailable,
          captured_adapter, WGPU_STRING_VIEW_INIT, captured.userdata1,
          captured.userdata2);
    }
    return true;
  });
  return WGPUFuture{future_id};
}

WGPUWaitStatus Instance::WaitAny(size_t futureCount, WGPUFutureWaitInfo * futures, uint64_t timeoutNS) {
  if (!futureCount)
    return WGPUWaitStatus_Success;

  bool all_completed = true;
  for (size_t i = 0; i < futureCount; ++i) {
    if (futures[i].completed)
      continue;
    if (futures[i].future.id == 0) {
      // Synchronously completed futures fire their callbacks immediately
      // and never register a pending entry.
      futures[i].completed = WGPUBool(true);
      continue;
    }
    if (TryFireFuture(futures[i].future.id, timeoutNS))
      futures[i].completed = WGPUBool(true);
    else
      all_completed = false;
  }
  return all_completed ? WGPUWaitStatus_Success : WGPUWaitStatus_TimedOut;
}

}  // namespace gfx
