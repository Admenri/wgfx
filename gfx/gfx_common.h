// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

/* Enable every WSI platform this build can target. The instance only
 * requests the WSI extensions the loader actually exposes (see
 * Instance::Instance), so missing platform libraries degrade gracefully. */
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#elif defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#include "volk/volk.h"
#include "webgpu-headers/webgpu.h"

namespace gfx {

template <class Ty>
inline Ty* AdaptExternalRefCounted(Ty* obj) {
  if (obj)
    obj->AddRef();
  return obj;
}

inline WGPUStringView MakeStringView(const std::string_view& view) {
  if (view.empty())
    return WGPUStringView();

  char* data = new char[view.size() + 1];
  std::memcpy(data, view.data(), view.size() + 1);
  return WGPUStringView{data, view.size()};
}

inline void FreeStringView(WGPUStringView& view) {
  if (view.data)
    delete[] view.data;
}

inline std::string_view FromWGPUStringView(const WGPUStringView& view) {
  if (!view.data || !view.length)
    return std::string_view();

  return std::string_view(view.data, view.length);
}

}  // namespace gfx
