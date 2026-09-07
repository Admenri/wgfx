# wgfx

A native WebGPU implementation built on top of Vulkan.  

Shaders are consumed as **SPIR-V** (`WGPUShaderSourceSPIRV`); WGSL can be
used by transpiling it to SPIR-V ahead of time.

Surfaces are supported on every Vulkan WSI platform: `WindowsHWND`,
`XcbWindow`, `XlibWindow`, and `WaylandSurface` (Linux), `MetalLayer`
(macOS/iOS via MoltenVK), and `AndroidNativeWindow`. The instance requests
only the WSI extensions the loader actually exposes, so a Linux system
without Wayland still works through XCB/Xlib and vice versa.

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
- `wgfx_bunny_*` — bunny-mark benchmarks (`tests/bunny_benchmark.cc`), built
  as six variants through the `bunny_benchmarks` meta target: batched
  (10 bunnies per draw call) vs one draw call per bunny, crossed with a
  shared storage buffer, per-bunny uniform buffers + bind groups, or no
  uniform transport (CPU-baked vertices). Selected via `BUNNY_BATCH_MODE`,
  `BUNNY_UNIFORM_MODE`, `BUNNY_COUNT`, `BUNNY_FRAMES`, `BUNNY_BATCH_SIZE`.
- `wgfx_quad_benchmark` — the sokol-gfx "sg" quad demo adapted for wgfx
  (`tests/quad_benchmark.cc`): one 3-vertex triangle-strip quad drawn 10000
  times per frame with a per-draw rotation angle pushed through the
  pipeline's immediate-data block (`wgpuRenderPassEncoderSetImmediates`,
  i.e. push constants), presented VSYNC'd to a GLFW window. FPS logs to
  stderr every 500 ms; draw iterations configurable via
  `QUAD_DRAW_ITERATIONS`.

Debug builds enable the Khronos validation layer automatically; all suites
run clean (zero validation messages).
