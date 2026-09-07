// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#include "gfx/gfx_surface.h"

#include <cstring>

#include "gfx/common/vulkan_conversions.h"
#include "gfx/gfx_adapter.h"
#include "gfx/gfx_device.h"
#include "gfx/gfx_instance.h"
#include "gfx/gfx_texture.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace gfx {

Surface::Surface(RefPtr<Instance> instance) : instance_(std::move(instance)) {}

Surface::~Surface() {
  DestroySwapchain();
  if (present_pool_ && device_)
    vkDestroyCommandPool(device_->GetVkDevice(), present_pool_, nullptr);
  if (surface_)
    vkDestroySurfaceKHR(instance_->GetVkInstance(), surface_, nullptr);
}

void Surface::Initialize(WGPUSurfaceDescriptor const * descriptor) {
#if defined(_WIN32)
  for (const WGPUChainedStruct* chain = descriptor->nextInChain; chain;
       chain = chain->next) {
    if (chain->sType == WGPUSType_SurfaceSourceWindowsHWND) {
      auto* source = reinterpret_cast<const WGPUSurfaceSourceWindowsHWND*>(chain);
      VkWin32SurfaceCreateInfoKHR create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
      create_info.hinstance =
          reinterpret_cast<HINSTANCE>(source->hinstance);
      create_info.hwnd = reinterpret_cast<HWND>(source->hwnd);
      vkCreateWin32SurfaceKHR(instance_->GetVkInstance(), &create_info,
                              nullptr, &surface_);
      break;
    }
  }
#elif defined(__APPLE__)
  for (const WGPUChainedStruct* chain = descriptor->nextInChain; chain;
       chain = chain->next) {
    if (chain->sType == WGPUSType_SurfaceSourceMetalLayer) {
      auto* source = reinterpret_cast<const WGPUSurfaceSourceMetalLayer*>(chain);
      VkMetalSurfaceCreateInfoEXT create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
      create_info.pLayer = source->layer;
      vkCreateMetalSurfaceEXT(instance_->GetVkInstance(), &create_info,
                              nullptr, &surface_);
      break;
    }
  }
#elif defined(__ANDROID__)
  for (const WGPUChainedStruct* chain = descriptor->nextInChain; chain;
       chain = chain->next) {
    if (chain->sType == WGPUSType_SurfaceSourceAndroidNativeWindow) {
      auto* source =
          reinterpret_cast<const WGPUSurfaceSourceAndroidNativeWindow*>(chain);
      VkAndroidSurfaceCreateInfoKHR create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
      create_info.window =
          reinterpret_cast<ANativeWindow*>(source->window);
      vkCreateAndroidSurfaceKHR(instance_->GetVkInstance(), &create_info,
                                nullptr, &surface_);
      break;
    }
  }
#elif defined(__linux__)
  for (const WGPUChainedStruct* chain = descriptor->nextInChain; chain;
       chain = chain->next) {
    if (chain->sType == WGPUSType_SurfaceSourceXlibWindow) {
      auto* source = reinterpret_cast<const WGPUSurfaceSourceXlibWindow*>(chain);
      VkXlibSurfaceCreateInfoKHR create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
      create_info.dpy =
          reinterpret_cast<Display*>(source->display);
      create_info.window = static_cast<Window>(source->window);
      vkCreateXlibSurfaceKHR(instance_->GetVkInstance(), &create_info,
                             nullptr, &surface_);
      break;
    }
    if (chain->sType == WGPUSType_SurfaceSourceXCBWindow) {
      auto* source = reinterpret_cast<const WGPUSurfaceSourceXCBWindow*>(chain);
      VkXcbSurfaceCreateInfoKHR create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
      create_info.connection =
          reinterpret_cast<xcb_connection_t*>(source->connection);
      create_info.window = source->window;
      vkCreateXcbSurfaceKHR(instance_->GetVkInstance(), &create_info, nullptr,
                            &surface_);
      break;
    }
    if (chain->sType == WGPUSType_SurfaceSourceWaylandSurface) {
      auto* source = reinterpret_cast<const WGPUSurfaceSourceWaylandSurface*>(chain);
      VkWaylandSurfaceCreateInfoKHR create_info = {};
      create_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
      create_info.display =
          reinterpret_cast<wl_display*>(source->display);
      create_info.surface = reinterpret_cast<wl_surface*>(source->surface);
      vkCreateWaylandSurfaceKHR(instance_->GetVkInstance(), &create_info,
                                nullptr, &surface_);
      break;
    }
  }
#endif
}

bool Surface::SupportsAdapter(Adapter* adapter) {
  if (!surface_)
    return true;  // No surface source: compatible with any adapter.
  VkBool32 supported = VK_FALSE;
  // The device implementation uses the first graphics+compute family.
  vkGetPhysicalDeviceSurfaceSupportKHR(
      adapter->GetVkPhysicalDevice(), 0, surface_, &supported);
  if (supported)
    return true;
  // Check all queue families.
  uint32_t family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(adapter->GetVkPhysicalDevice(),
                                           &family_count, nullptr);
  for (uint32_t i = 0; i < family_count; ++i) {
    vkGetPhysicalDeviceSurfaceSupportKHR(adapter->GetVkPhysicalDevice(), i,
                                         surface_, &supported);
    if (supported)
      return true;
  }
  return false;
}

WGPUStatus Surface::GetCapabilities(WGPUAdapter adapter, WGPUSurfaceCapabilities * capabilities) {
  if (!surface_)
    return WGPUStatus_Error;

  auto* gfx_adapter = static_cast<gfx::Adapter*>(adapter);
  VkPhysicalDevice physical = gfx_adapter->GetVkPhysicalDevice();
  VkSurfaceCapabilitiesKHR vk_caps = {};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface_, &vk_caps);

  capabilities->usages = WGPUTextureUsage_RenderAttachment |
                         WGPUTextureUsage_CopySrc;

  uint32_t format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface_, &format_count,
                                       nullptr);
  std::vector<VkSurfaceFormatKHR> vk_formats(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface_, &format_count,
                                       vk_formats.data());
  WGPUTextureFormat* formats =

      static_cast<WGPUTextureFormat*>(malloc(sizeof(WGPUTextureFormat) *
                                             format_count));
  for (uint32_t i = 0; i < format_count; ++i) {
    switch (vk_formats[i].format) {
      case VK_FORMAT_R8G8B8A8_UNORM:
        formats[i] = WGPUTextureFormat_RGBA8Unorm;
        break;
      case VK_FORMAT_B8G8R8A8_UNORM:
        formats[i] = WGPUTextureFormat_BGRA8Unorm;
        break;
      case VK_FORMAT_R8G8B8A8_SRGB:
        formats[i] = WGPUTextureFormat_RGBA8UnormSrgb;
        break;
      case VK_FORMAT_B8G8R8A8_SRGB:
        formats[i] = WGPUTextureFormat_BGRA8UnormSrgb;
        break;
      default:
        formats[i] = WGPUTextureFormat_RGBA8Unorm;
        break;
    }
  }
  capabilities->formats = formats;
  capabilities->formatCount = format_count;

  uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface_,
                                            &present_mode_count, nullptr);
  std::vector<VkPresentModeKHR> vk_modes(present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface_,
                                            &present_mode_count,
                                            vk_modes.data());
  WGPUPresentMode* present_modes = static_cast<WGPUPresentMode*>(
      malloc(sizeof(WGPUPresentMode) * present_mode_count));
  for (uint32_t i = 0; i < present_mode_count; ++i) {
    present_modes[i] = vk_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR
                           ? WGPUPresentMode_Immediate
                           : WGPUPresentMode_Fifo;
  }
  capabilities->presentModes = present_modes;
  capabilities->presentModeCount = present_mode_count;

  // Always advertise the composite modes every Vulkan surface supports.
  static const WGPUCompositeAlphaMode alpha_modes[] = {
      WGPUCompositeAlphaMode_Opaque,
      WGPUCompositeAlphaMode_Premultiplied,
      WGPUCompositeAlphaMode_Unpremultiplied,
      WGPUCompositeAlphaMode_Inherit,
  };
  WGPUCompositeAlphaMode* alpha_copy = static_cast<WGPUCompositeAlphaMode*>(
      malloc(sizeof(alpha_modes)));
  std::memcpy(alpha_copy, alpha_modes, sizeof(alpha_modes));
  capabilities->alphaModes = alpha_copy;
  capabilities->alphaModeCount = std::size(alpha_modes);
  return WGPUStatus_Success;
}

void Surface::Configure(WGPUSurfaceConfiguration const * config) {
  device_ = RefPtr<Device>(static_cast<gfx::Device*>(config->device));
  VkPhysicalDevice physical = device_->GetVkPhysicalDevice();
  VkDevice vk_device = device_->GetVkDevice();

  if (!present_pool_) {
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = device_->GetQueueFamilyIndex();
    vkCreateCommandPool(vk_device, &pool_info, nullptr, &present_pool_);
  }

  format_ = config->format;
  alpha_mode_ = config->alphaMode;
  width_ = config->width;
  height_ = config->height;

  VkSurfaceCapabilitiesKHR caps = {};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface_, &caps);

  DestroySwapchain();

  uint32_t image_count = caps.minImageCount + 1;
  if (caps.maxImageCount && image_count > caps.maxImageCount)
    image_count = caps.maxImageCount;

  VkSwapchainCreateInfoKHR create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = surface_;
  create_info.minImageCount = image_count;
  create_info.imageFormat = GetFormatInfo(format_).vk_format;
  create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  create_info.imageExtent = {width_, height_};
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = AddAttachmentUsageForFormat(
      GetImageUsage(config->usage), GetFormatInfo(format_).vk_format);
  create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  create_info.preTransform = caps.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = GetPresentMode(config->presentMode);
  create_info.clipped = VK_TRUE;
  if (config->viewFormatCount)
    create_info.flags |= VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR;

  vkCreateSwapchainKHR(vk_device, &create_info, nullptr, &swapchain_);
  device_->SetObjectLabel(reinterpret_cast<uint64_t>(swapchain_),
                          VK_OBJECT_TYPE_SWAPCHAIN_KHR, label_);

  uint32_t swapchain_image_count = 0;
  vkGetSwapchainImagesKHR(vk_device, swapchain_, &swapchain_image_count,
                          nullptr);
  images_.resize(swapchain_image_count);
  vkGetSwapchainImagesKHR(vk_device, swapchain_, &swapchain_image_count,
                          images_.data());

  image_views_.resize(swapchain_image_count);
  for (uint32_t i = 0; i < swapchain_image_count; ++i) {
    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = images_[i];
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = GetFormatInfo(format_).vk_format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    vkCreateImageView(vk_device, &view_info, nullptr, &image_views_[i]);
  }
}

void Surface::GetCurrentTexture(WGPUSurfaceTexture * surfaceTexture) {
  VkDevice vk_device = device_->GetVkDevice();

  VkSemaphore acquire_semaphore = VK_NULL_HANDLE;
  VkSemaphoreCreateInfo semaphore_info = {};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &acquire_semaphore);

  VkResult result = vkAcquireNextImageKHR(vk_device, swapchain_, UINT64_MAX,
                                          acquire_semaphore, VK_NULL_HANDLE,
                                          &acquired_index_);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    vkDestroySemaphore(vk_device, acquire_semaphore, nullptr);
    surfaceTexture->status = WGPUSurfaceGetCurrentTextureStatus_Outdated;
    surfaceTexture->texture = nullptr;
    return;
  }

  // Hand the acquire semaphore to the next queue submission.
  device_->SetAcquireSemaphore(acquire_semaphore);

  acquired_texture_ = RefPtr<Texture>(new Texture(
      device_, images_[acquired_index_], format_, width_, height_,
      WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc));

  surfaceTexture->texture = acquired_texture_.release();
  surfaceTexture->status = result == VK_SUBOPTIMAL_KHR
                               ? WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal
                               : WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal;
}

WGPUStatus Surface::Present() {
  // Transition the acquired image into the present layout.
  {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = present_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device_->GetVkDevice(), &alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &begin_info);
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = Device::kImageLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = images_[acquired_index_];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr,
                         1, &barrier);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    vkQueueSubmit(device_->GetVkQueue(), 1, &submit_info, VK_NULL_HANDLE);
  }

  VkSemaphore render_done = device_->TakeRenderDoneSemaphore();
  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = render_done ? 1 : 0;
  present_info.pWaitSemaphores = &render_done;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain_;
  present_info.pImageIndices = &acquired_index_;

  VkResult result = vkQueuePresentKHR(device_->GetVkQueue(), &present_info);
  acquired_texture_ = nullptr;
  return result == VK_SUCCESS ? WGPUStatus_Success : WGPUStatus_Error;
}

void Surface::Unconfigure() {
  DestroySwapchain();
  device_ = nullptr;
}

void Surface::SetLabel(WGPUStringView label) {
  label_ = label;
  if (device_) {
    device_->SetObjectLabel(reinterpret_cast<uint64_t>(surface_),
                            VK_OBJECT_TYPE_SURFACE_KHR, label);
    if (swapchain_) {
      device_->SetObjectLabel(reinterpret_cast<uint64_t>(swapchain_),
                              VK_OBJECT_TYPE_SWAPCHAIN_KHR, label);
    }
  }
}

void Surface::DestroySwapchain() {
  if (!device_)
    return;
  VkDevice vk_device = device_->GetVkDevice();
  vkDeviceWaitIdle(vk_device);
  for (VkImageView view : image_views_)
    vkDestroyImageView(vk_device, view, nullptr);
  image_views_.clear();
  images_.clear();
  if (swapchain_) {
    vkDestroySwapchainKHR(vk_device, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }
}

}  // namespace gfx
