// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUInstanceImpl handle;
// define it here so gfx::Instance can derive from it, making the
// generated bridge's static_cast<gfx::Instance*>(handle) a valid
// base->derived downcast.
struct WGPUInstanceImpl {};

namespace gfx {

class Instance : public WGPUInstanceImpl, public RefCounted<Instance> {
 public:
  explicit Instance(WGPUInstanceDescriptor const * descriptor);
  ~Instance();

  VkInstance GetVkInstance() const { return instance_; }
  // Runtime-detected instance API version, capped at 1.3.
  uint32_t GetAPIVersion() const { return api_version_; }

  // Registers a future whose completion fires |try_fire|. |try_fire|
  // returns true when the future was consumed (its callbacks fired).
  uint64_t AddFuture(std::function<bool(uint64_t)> try_fire);
  // Attempts to fire a pending future; returns false if still pending.
  bool TryFireFuture(uint64_t future_id, uint64_t timeout_ns);

  gfx::Surface* CreateSurface(WGPUSurfaceDescriptor const * descriptor);
  void GetWGSLLanguageFeatures(WGPUSupportedWGSLLanguageFeatures * features);
  WGPUBool HasWGSLLanguageFeature(WGPUWGSLLanguageFeatureName feature);
  void ProcessEvents();
  WGPUFuture RequestAdapter(WGPURequestAdapterOptions const * options, WGPURequestAdapterCallbackInfo callbackInfo);
  WGPUWaitStatus WaitAny(size_t futureCount, WGPUFutureWaitInfo * futures, uint64_t timeoutNS);

 private:
  VkInstance instance_ = VK_NULL_HANDLE;
  uint32_t api_version_ = VK_API_VERSION_1_1;
  VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;

  std::mutex future_mutex_;
  std::atomic<uint64_t> next_future_id_{1};
  std::unordered_map<uint64_t, std::function<bool(uint64_t)>> futures_;
};

}  // namespace gfx
