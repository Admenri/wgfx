// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

// main_wgpu.cc adapted for wgfx — a native WebGPU port of the sokol-gfx
// "sg" quad demo (originally webgpu_quad.html running on wgpu-native).
//
// Functionally identical to the original:
//   - a 1280x720 window (GLFW here instead of SDL)
//   - red clear color (0.5, 0, 0, 1)
//   - one 3-vertex quad (TRIANGLE_STRIP, 32 bytes/vertex: float4 position +
//     float4 color) drawn 10000 times per frame
//   - each draw has a slightly different rotation angle
//     (time = (ticks + i*10) * 0.001)
//   - the VS rotates the quad around Z by `time`, the FS passes the color
//     through
//   - VSYNC presentation (WGPUPresentMode_Fifo, the native equivalent of
//     requestAnimationFrame)
//
// Differences imposed by wgfx:
//   - shaders are consumed as SPIR-V (the WGSL sources transpiled to GLSL
//     and compiled with glslc at build time)
//   - per-draw uniforms ride the pipeline's immediate-data block
//     (wgpuRenderPassEncoderSetImmediates, i.e. push constants). wgfx
//     always provides up to maxImmediateSize = 128 bytes, so no feature
//     request is needed (wgpu-native requires WGPUNativeFeature_Immediates)
//   - the window is created with GLFW and presented through a
//     WGPUSurfaceSourceWindowsHWND surface

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include "GLFW/glfw3native.h"
#endif

#include "webgpu-headers/webgpu.h"

#include "quad.frag.spv.h"
#include "quad.vert.spv.h"

/* ── Constants (match webgpu_quad.html) ──────────────────────────────────── */

#define WINDOW_TITLE "sg — wgfx"
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

#ifndef QUAD_DRAW_ITERATIONS
#define QUAD_DRAW_ITERATIONS 10000
#endif

/* Clear color — a solid red background (matches main.cc). */
static const float kClearColor[4] = {0.5f, 0.0f, 0.0f, 1.0f};

/* Number of vertices in the quad. */
#define QUAD_VERTEX_COUNT 3

/* Byte size of one per-draw immediate block (time + aspect + pad = 16B). */
#define IMMEDIATE_SLOT_SIZE 16

/* CPU-side layout for one per-draw immediate block. */
typedef struct vs_params_t {
  float time;
  float aspect;
  float pad[2];
} vs_params_t;

static_assert(sizeof(vs_params_t) == IMMEDIATE_SLOT_SIZE,
              "vs_params_t must match the shader immediate block size");

/* ── Vertex data ─────────────────────────────────────────────────────────────
 * The exact same quad: 3 vertices (a TRIANGLE_STRIP), each holding a float4
 * position and a float4 color — 32 bytes/vertex. */
static const float kQuad[QUAD_VERTEX_COUNT * 8] = {
    // x,  y,      z,    w       r,    g,    b,    a
    +0.0f, +0.5f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,  // lt
    -0.5f, -0.25f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // rt
    +0.5f, -0.25f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // rb
};

/* ── Shader helper ───────────────────────────────────────────────────────────
 */

static WGPUShaderModule CreateSpirvModule(WGPUDevice device,
                                          const uint32_t* code,
                                          size_t code_size) {
  WGPUShaderSourceSPIRV source = WGPU_SHADER_SOURCE_SPIRV_INIT;
  source.codeSize = static_cast<uint32_t>(code_size);
  source.code = code;
  WGPUShaderModuleDescriptor descriptor = {};
  descriptor.nextInChain = &source.chain;
  return wgpuDeviceCreateShaderModule(device, &descriptor);
}

/* ── Application state ───────────────────────────────────────────────────────
 */

typedef struct wgpu_ctx {
  WGPUInstance instance;
  WGPUSurface surface;
  WGPUAdapter adapter;
  WGPUDevice device;
  WGPUQueue queue;

  WGPUTextureFormat surface_format;
  WGPUCompositeAlphaMode alpha_mode;
  uint32_t width;  /* current window size in pixels */
  uint32_t height;

  WGPUPipelineLayout pipeline_layout;
  WGPURenderPipeline pipeline;
  WGPUBuffer vertex_buffer;

  /* Per-draw data (time/aspect) is uploaded straight into the pipeline's
   * immediate-data block with wgpuRenderPassEncoderSetImmediates, so no
   * buffers or bind groups are created for it. */

  /* FPS counter — rolling 500 ms window (matches webgpu_quad.html). */
  uint32_t fps_frames;
  uint32_t fps_timer;
} wgpu_ctx;

static uint32_t g_ticks() {
  return static_cast<uint32_t>(glfwGetTime() * 1000.0);
}

/* ── Callbacks ───────────────────────────────────────────────────────────────
 * wgfx runs these synchronously on the calling thread. */

static void on_request_adapter(WGPURequestAdapterStatus status,
                               WGPUAdapter adapter,
                               WGPUStringView message,
                               void* userdata1,
                               void* userdata2) {
  wgpu_ctx* ctx = (wgpu_ctx*)userdata1;
  if (status == WGPURequestAdapterStatus_Success) {
    ctx->adapter = adapter;
  } else {
    std::printf("[wgpu] requestAdapter failed (status %#.8x)\n",
                (unsigned)status);
  }
}

static void on_request_device(WGPURequestDeviceStatus status,
                              WGPUDevice device,
                              WGPUStringView message,
                              void* userdata1,
                              void* userdata2) {
  wgpu_ctx* ctx = (wgpu_ctx*)userdata1;
  if (status == WGPURequestDeviceStatus_Success) {
    ctx->device = device;
  } else {
    std::printf("[wgpu] requestDevice failed (status %#.8x)\n",
                (unsigned)status);
  }
}

/* ── Surface helpers ────────────────────────────────────────────────────────
 */

/* (Re)configure the surface (the "swapchain").  Called at init and whenever
 * the window is resized. */
static void reconfigure_surface(wgpu_ctx* ctx) {
  WGPUSurfaceConfiguration config = {};
  config.device = ctx->device;
  config.format = ctx->surface_format;
  config.usage = WGPUTextureUsage_RenderAttachment;
  config.width = ctx->width;
  config.height = ctx->height;
  config.alphaMode = ctx->alpha_mode;
  config.presentMode = WGPUPresentMode_Fifo; /* VSYNC, matches rAF */
  wgpuSurfaceConfigure(ctx->surface, &config);
}

/* ── Initialisation ─────────────────────────────────────────────────────────
 */

static bool init_wgpu(wgpu_ctx* ctx, GLFWwindow* win) {
  memset(ctx, 0, sizeof(*ctx));

  /* ── 1. Instance + surface (the "GPU device" of the HTML) ─────────── */
  ctx->instance = wgpuCreateInstance(NULL);
  if (!ctx->instance) {
    std::printf("[wgpu] wgpuCreateInstance failed\n");
    return false;
  }

  /* Grab the native Win32 HWND from the GLFW window. */
  WGPUSurfaceSourceWindowsHWND hwnd_source_win = {};
  static WGPUSurfaceSourceXlibWindow xlib_source = {};
  static WGPUSurfaceSourceWaylandSurface wayland_source = {};
  WGPUSurfaceDescriptor surface_desc = {};
#if defined(_WIN32)
  HWND hwnd = glfwGetWin32Window(win);
  if (!hwnd) {
    std::printf("[wgpu] no Win32 HWND available from GLFW window\n");
    return false;
  }
  hwnd_source_win.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
  hwnd_source_win.hinstance = GetModuleHandle(NULL);
  hwnd_source_win.hwnd = hwnd;
  surface_desc.nextInChain = &hwnd_source_win.chain;
#elif defined(__linux__)
  if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
    wayland_source.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
    wayland_source.display = glfwGetWaylandDisplay();
    wayland_source.surface = glfwGetWaylandWindow(win);
    surface_desc.nextInChain = &wayland_source.chain;
  } else {
    xlib_source.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    xlib_source.display = glfwGetX11Display();
    xlib_source.window = glfwGetX11Window(win);
    surface_desc.nextInChain = &xlib_source.chain;
  }
#else
  std::printf("[wgpu] this benchmark has no surface source on this platform\n");
  return false;
#endif

  ctx->surface = wgpuInstanceCreateSurface(ctx->instance, &surface_desc);
  if (!ctx->surface) {
    std::printf("[wgpu] wgpuInstanceCreateSurface failed\n");
    return false;
  }

  /* ── 2. Adapter + device (navigator.gpu.requestAdapter/requestDevice) */
  WGPURequestAdapterOptions adapter_opts = {};
  adapter_opts.compatibleSurface = ctx->surface;
  WGPURequestAdapterCallbackInfo adapter_cb = {};
  adapter_cb.mode = WGPUCallbackMode_AllowSpontaneous;
  adapter_cb.callback = on_request_adapter;
  adapter_cb.userdata1 = ctx;
  wgpuInstanceRequestAdapter(ctx->instance, &adapter_opts, adapter_cb);
  if (!ctx->adapter) {
    std::printf("[wgpu] no adapter found\n");
    return false;
  }

  /* Immediates (push constants) are always available on wgfx devices with
   * maxImmediateSize = 128, so no feature request is needed. */
  WGPUDeviceDescriptor device_desc = {};
  device_desc.uncapturedErrorCallbackInfo = {};

  WGPURequestDeviceCallbackInfo device_cb = {};
  device_cb.mode = WGPUCallbackMode_AllowSpontaneous;
  device_cb.callback = on_request_device;
  device_cb.userdata1 = ctx;
  wgpuAdapterRequestDevice(ctx->adapter, &device_desc, device_cb);
  if (!ctx->device) {
    std::printf("[wgpu] requestDevice failed\n");
    return false;
  }

  ctx->queue = wgpuDeviceGetQueue(ctx->device);
  if (!ctx->queue) {
    std::printf("[wgpu] wgpuDeviceGetQueue failed\n");
    return false;
  }

  /* ── 3. Surface capabilities + configure (the "swapchain") ────────── */
  WGPUSurfaceCapabilities caps = {};
  wgpuSurfaceGetCapabilities(ctx->surface, ctx->adapter, &caps);
  ctx->surface_format =
      (caps.formatCount > 0) ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
  ctx->alpha_mode = (caps.alphaModeCount > 0)
                        ? caps.alphaModes[0]
                        : WGPUCompositeAlphaMode_Auto;
  wgpuSurfaceCapabilitiesFreeMembers(caps);

  int w = 0, h = 0;
  glfwGetFramebufferSize(win, &w, &h);
  ctx->width = (uint32_t)w;
  ctx->height = (uint32_t)h;
  reconfigure_surface(ctx);

  /* ── 4/5/6. Pipeline: SPIR-V modules + explicit 16-byte immediate
   *     layout + graphics states ────────────────────────────────────── */
  WGPUShaderModule vs_module =
      CreateSpirvModule(ctx->device, kQuadVertSpv, kQuadVertSpvSize);
  WGPUShaderModule fs_module =
      CreateSpirvModule(ctx->device, kQuadFragSpv, kQuadFragSpvSize);
  if (!vs_module || !fs_module) {
    std::printf("[wgpu] failed to create shader modules\n");
    return false;
  }

  /* A 16-byte immediate-data block per draw — no bind groups in use. */
  WGPUPipelineLayoutDescriptor pl_desc = {};
  pl_desc.immediateSize = IMMEDIATE_SLOT_SIZE;
  ctx->pipeline_layout = wgpuDeviceCreatePipelineLayout(ctx->device, &pl_desc);
  if (!ctx->pipeline_layout) {
    std::printf("[wgpu] failed to create pipeline layout\n");
    wgpuShaderModuleRelease(fs_module);
    wgpuShaderModuleRelease(vs_module);
    return false;
  }

  WGPUVertexAttribute vertex_attribs[2] = {};
  vertex_attribs[0].format = WGPUVertexFormat_Float32x4; /* position */
  vertex_attribs[0].offset = 0;
  vertex_attribs[0].shaderLocation = 0;
  vertex_attribs[1].format = WGPUVertexFormat_Float32x4; /* color */
  vertex_attribs[1].offset = 16;
  vertex_attribs[1].shaderLocation = 1;

  WGPUVertexBufferLayout vertex_layout = {};
  vertex_layout.arrayStride = 32; /* 8 floats * 4 bytes */
  vertex_layout.stepMode = WGPUVertexStepMode_Vertex;
  vertex_layout.attributeCount = 2;
  vertex_layout.attributes = vertex_attribs;

  WGPUVertexState vertex_state = {};
  vertex_state.module = vs_module;
  /* Empty entry point: wgfx falls back to the SPIR-V "main" entry. */
  vertex_state.bufferCount = 1;
  vertex_state.buffers = &vertex_layout;

  WGPUColorTargetState color_target = {};
  color_target.format = ctx->surface_format;
  color_target.writeMask = WGPUColorWriteMask_All;

  WGPUFragmentState fragment_state = {};
  fragment_state.module = fs_module;
  fragment_state.targetCount = 1;
  fragment_state.targets = &color_target;

  WGPUPrimitiveState primitive_state = {};
  primitive_state.topology = WGPUPrimitiveTopology_TriangleStrip; /* STRIP */
  primitive_state.cullMode = WGPUCullMode_None; /* sokol default: no culling */

  WGPUMultisampleState multisample_state = {};
  multisample_state.count = 1;
  multisample_state.mask = 0xFFFFFFFF;

  WGPURenderPipelineDescriptor pipeline_desc = {};
  pipeline_desc.layout = ctx->pipeline_layout;
  pipeline_desc.vertex = vertex_state;
  pipeline_desc.primitive = primitive_state;
  pipeline_desc.multisample = multisample_state;
  pipeline_desc.fragment = &fragment_state;
  /* no depth-stencil state (matches sokol: depth pixel format NONE) */

  ctx->pipeline = wgpuDeviceCreateRenderPipeline(ctx->device, &pipeline_desc);
  wgpuShaderModuleRelease(fs_module);
  wgpuShaderModuleRelease(vs_module);
  if (!ctx->pipeline) {
    std::printf("[wgpu] failed to create render pipeline\n");
    return false;
  }

  /* ── 7. Vertex buffer (the quad) ──────────────────────────────────── */
  WGPUBufferDescriptor vb_desc = {};
  vb_desc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
  vb_desc.size = sizeof(kQuad);
  ctx->vertex_buffer = wgpuDeviceCreateBuffer(ctx->device, &vb_desc);
  if (!ctx->vertex_buffer) {
    std::printf("[wgpu] failed to create vertex buffer\n");
    return false;
  }
  wgpuQueueWriteBuffer(ctx->queue, ctx->vertex_buffer, 0, kQuad, sizeof(kQuad));

  return true;
}

/* ── Frame ──────────────────────────────────────────────────────────────────
 * Record + submit + present synchronously, exactly like the original's
 * frame().  Per-draw time is written straight into the pipeline's
 * immediate-data block right before each draw.  Returns false if the frame
 * was skipped (surface resize etc.). */
static bool render_frame(wgpu_ctx* ctx, GLFWwindow* win) {
  static bool first_frame = true;
  if (first_frame) {
    std::fprintf(stderr, "frame 1 begin\n");
    first_frame = false;
  }
  uint32_t ticks = g_ticks();
  float aspect =
      (ctx->height > 0) ? (float)ctx->width / (float)ctx->height : 1.0f;

  /* Update the on-screen frame rate every ~500 ms (matches the HTML). */
  ctx->fps_frames++;
  uint32_t now = g_ticks();
  uint32_t elapsed = now - ctx->fps_timer;
  if (elapsed >= 500) {
    float fps = (ctx->fps_frames * 1000.0f) / (float)elapsed;
    std::fprintf(stderr, "wgfx running | %.1f FPS (%d draws/frame)\n", fps,
                 QUAD_DRAW_ITERATIONS);
    ctx->fps_frames = 0;
    ctx->fps_timer = now;
  }

  /* Acquire the next swapchain texture (VSYNC FIFO). */
  WGPUSurfaceTexture surface_texture = {};
  wgpuSurfaceGetCurrentTexture(ctx->surface, &surface_texture);
  switch (surface_texture.status) {
    case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
    case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
      break;
    case WGPUSurfaceGetCurrentTextureStatus_Timeout:
    case WGPUSurfaceGetCurrentTextureStatus_Outdated:
    case WGPUSurfaceGetCurrentTextureStatus_Lost:
      if (surface_texture.texture != NULL) {
        wgpuTextureRelease(surface_texture.texture);
      }
      {
        int w = 0, h = 0;
        glfwGetFramebufferSize(win, &w, &h);
        if (w != 0 && h != 0) {
          ctx->width = (uint32_t)w;
          ctx->height = (uint32_t)h;
          reconfigure_surface(ctx);
        }
      }
      return false;
    default:
      std::printf("[wgpu] getCurrentTexture status=%#.8x\n",
                  (unsigned)surface_texture.status);
      return false;
  }

  WGPUTextureView frame = wgpuTextureCreateView(surface_texture.texture, NULL);

  /* ── Record + submit the frame synchronously ─────────────────────── */
  WGPUCommandEncoder command_encoder =
      wgpuDeviceCreateCommandEncoder(ctx->device, NULL);

  WGPURenderPassColorAttachment color_attachment = {};
  color_attachment.view = frame;
  color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
  color_attachment.loadOp = WGPULoadOp_Clear;
  color_attachment.storeOp = WGPUStoreOp_Store;
  color_attachment.clearValue.r = kClearColor[0];
  color_attachment.clearValue.g = kClearColor[1];
  color_attachment.clearValue.b = kClearColor[2];
  color_attachment.clearValue.a = kClearColor[3];

  WGPURenderPassDescriptor pass_desc = {};
  pass_desc.colorAttachmentCount = 1;
  pass_desc.colorAttachments = &color_attachment;

  WGPURenderPassEncoder pass =
      wgpuCommandEncoderBeginRenderPass(command_encoder, &pass_desc);

  wgpuRenderPassEncoderSetPipeline(pass, ctx->pipeline);
  wgpuRenderPassEncoderSetVertexBuffer(pass, 0, ctx->vertex_buffer, 0,
                                       sizeof(kQuad));
  vs_params_t params = {};
  params.aspect = aspect;
  for (uint32_t i = 0; i < QUAD_DRAW_ITERATIONS; i++) {
    /* Each draw rotates slightly more: time advances 10 ms per draw. */
    params.time = (float)(ticks + i * 10u) * 0.001f;
    wgpuRenderPassEncoderSetImmediates(pass, 0, &params, sizeof(params));
    wgpuRenderPassEncoderDraw(pass, QUAD_VERTEX_COUNT, 1, 0, 0);
  }
  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);

  WGPUCommandBuffer command_buffer =
      wgpuCommandEncoderFinish(command_encoder, NULL);
  wgpuCommandEncoderRelease(command_encoder);

  wgpuQueueSubmit(ctx->queue, 1, &command_buffer);
  std::fprintf(stderr, "submitted\n");
  wgpuSurfacePresent(ctx->surface);
  std::fprintf(stderr, "presented\n");

  wgpuCommandBufferRelease(command_buffer);
  wgpuTextureViewRelease(frame);
  wgpuTextureRelease(surface_texture.texture);
  return true;
}

/* ── Cleanup ────────────────────────────────────────────────────────────────
 */

static void shutdown_wgpu(wgpu_ctx* ctx) {
  if (ctx->vertex_buffer) {
    wgpuBufferRelease(ctx->vertex_buffer);
  }
  if (ctx->pipeline) {
    wgpuRenderPipelineRelease(ctx->pipeline);
  }
  if (ctx->pipeline_layout) {
    wgpuPipelineLayoutRelease(ctx->pipeline_layout);
  }
  if (ctx->queue) {
    wgpuQueueRelease(ctx->queue);
  }
  if (ctx->device) {
    wgpuDeviceRelease(ctx->device);
  }
  if (ctx->adapter) {
    wgpuAdapterRelease(ctx->adapter);
  }
  if (ctx->surface) {
    wgpuSurfaceRelease(ctx->surface);
  }
  if (ctx->instance) {
    wgpuInstanceRelease(ctx->instance);
  }
  memset(ctx, 0, sizeof(*ctx));
}

/* ── Main ───────────────────────────────────────────────────────────────────
 * GLFW creates the window and runs the event loop; everything else is wgfx.
 * Resize reconfigures the surface; frames whose swapchain image could not be
 * acquired are skipped. */
int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* win = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                                     WINDOW_TITLE, NULL, NULL);
  if (!win) {
    std::printf("glfwCreateWindow failed\n");
    glfwTerminate();
    return 1;
  }

  wgpu_ctx ctx;
  if (!init_wgpu(&ctx, win)) {
    std::printf("wgfx initialisation failed\n");
    shutdown_wgpu(&ctx);
    glfwDestroyWindow(win);
    glfwTerminate();
    return 1;
  }

  std::printf("WebGPU backend: wgfx (Vulkan)\n");
  ctx.fps_timer = g_ticks();

  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();

    /* Reconfigure the surface when the window is resized. */
    int w = 0, h = 0;
    glfwGetFramebufferSize(win, &w, &h);
    if (w != 0 && h != 0 &&
        ((uint32_t)w != ctx.width || (uint32_t)h != ctx.height)) {
      ctx.width = (uint32_t)w;
      ctx.height = (uint32_t)h;
      reconfigure_surface(&ctx);
    }

    render_frame(&ctx, win);
  }

  shutdown_wgpu(&ctx);
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
