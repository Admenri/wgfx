# wgfx

A native WebGPU implementation built on top of Vulkan.  

Shaders are consumed as **SPIR-V** (`WGPUShaderSourceSPIRV`); WGSL can be
used by transpiling it to SPIR-V ahead of time.  

## Building

Requirements: CMake 3.30+, C++20, a C++20 compiler (MSVC tested), the Vulkan
SDK (validation layers + `glslc` for the tests), and the vendored third-party
dependencies in `third_party/` (Vulkan-Headers, VulkanMemoryAllocator, volk,
GLFW, webgpu-headers — pulled in as submodules).

```sh
cmake -S . -B build
cmake --build build --config Release
```

This produces the `wgfx` shared library plus two test executables:

- `wgfx_tests` — the W3C webgpu-samples triangle rendered offscreen (pixel
  verified), a compute test, and 30 presented frames to a GLFW window.
- `wgfx_feature_tests` — end-to-end feature conformance: dual-source
  blending, timestamps, indirect-first-instance, f16 compute, subgroups,
  compressed texture sampling, float32 blending/filterability, clip
  distances, component swizzle, and more. Each test is gated on the device
  advertising the feature.

Debug builds enable the Khronos validation layer automatically; both suites
run clean (zero validation messages).
