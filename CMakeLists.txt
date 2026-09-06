# Copyright 2026 Admenri.
# Use of this source code is governed by a MIT-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.30)

project(wgfx LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(third_party)

add_subdirectory(third_party/Vulkan-Headers)
add_subdirectory(third_party/VulkanMemoryAllocator)
add_subdirectory(third_party/volk)

add_subdirectory(third_party/webgpu-cpp)

add_subdirectory(third_party/glfw)
add_subdirectory(gfx)
add_subdirectory(tests)
