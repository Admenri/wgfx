# Copyright 2026 Admenri.
# Use of this source code is governed by a MIT-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.30)

# Compiles the test GLSL shaders into SPIR-V with glslc (Vulkan SDK) and
# embeds them as headers for the test program.
find_program(WGFX_GLSLC_EXECUTABLE glslc
  HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")

if (NOT WGFX_GLSLC_EXECUTABLE)
  message(FATAL_ERROR "glslc not found; install the Vulkan SDK")
endif ()

# Emits <binary-dir>/shaders/<base>.spv.h defining <var_name> and
# <var_name>Size, invoked from the test source.
function(wgfx_embed_spirv source_file var_name out_header)
  set(spv_path "${CMAKE_CURRENT_BINARY_DIR}/shaders/${source_file}.spv")
  set(header_path "${CMAKE_CURRENT_BINARY_DIR}/shaders/${source_file}.spv.h")

  add_custom_command(
    OUTPUT "${spv_path}"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    COMMAND ${WGFX_GLSLC_EXECUTABLE} -O --target-env=vulkan1.1 -o "${spv_path}"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${source_file}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${source_file}"
    COMMENT "glslc ${source_file} -> SPIR-V"
  )
  add_custom_command(
    OUTPUT "${header_path}"
    COMMAND ${CMAKE_COMMAND}
            -DSPIRV_BIN="${spv_path}"
            -DHEADER_OUT="${header_path}"
            -DVAR_NAME="${var_name}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
    DEPENDS "${spv_path}" "${CMAKE_CURRENT_SOURCE_DIR}/embed_spirv.cmake"
    COMMENT "Embedding ${source_file}.spv as ${var_name}"
  )
  set(${out_header} "${header_path}" PARENT_SCOPE)
endfunction ()

wgfx_embed_spirv(triangle.vert kTriangleVertSpv kTriangleVertHeader)
wgfx_embed_spirv(triangle.frag kTriangleFragSpv kTriangleFragHeader)
wgfx_embed_spirv(compute.comp kComputeCompSpv kComputeHeader)
wgfx_embed_spirv(dual_source.frag kDualSourceFragSpv kDualSourceFragHeader)
wgfx_embed_spirv(instance_color.vert kInstanceColorVertSpv kInstanceColorVertHeader)
wgfx_embed_spirv(instance_color.frag kInstanceColorFragSpv kInstanceColorFragHeader)
wgfx_embed_spirv(compute_f16.comp kComputeF16CompSpv kComputeF16Header)
wgfx_embed_spirv(compute_subgroup.comp kComputeSubgroupCompSpv kComputeSubgroupHeader)
wgfx_embed_spirv(fullscreen.vert kFullscreenVertSpv kFullscreenVertHeader)
wgfx_embed_spirv(tex_sample.frag kTexSampleFragSpv kTexSampleFragHeader)
wgfx_embed_spirv(clip.vert kClipVertSpv kClipVertHeader)
wgfx_embed_spirv(clip.frag kClipFragSpv kClipFragHeader)
wgfx_embed_spirv(solid_red.frag kSolidRedFragSpv kSolidRedFragHeader)
wgfx_embed_spirv(bunny_shared.vert kBunnySharedVertSpv kBunnySharedVertHeader)
wgfx_embed_spirv(bunny_ubo.vert kBunnyUboVertSpv kBunnyUboVertHeader)
wgfx_embed_spirv(bunny_baked.vert kBunnyBakedVertSpv kBunnyBakedVertHeader)
wgfx_embed_spirv(bunny.frag kBunnyFragSpv kBunnyFragHeader)
wgfx_embed_spirv(quad.vert kQuadVertSpv kQuadVertHeader)
wgfx_embed_spirv(quad.frag kQuadFragSpv kQuadFragHeader)

add_executable(wgfx_tests)

target_sources(wgfx_tests PRIVATE
  wgfx_tests.cc
  "${kTriangleVertHeader}"
  "${kTriangleFragHeader}"
  "${kComputeHeader}"
)

target_include_directories(wgfx_tests PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/shaders"
)

target_link_libraries(wgfx_tests PRIVATE
  wgfx
  glfw
)

add_executable(wgfx_feature_tests)

target_sources(wgfx_feature_tests PRIVATE
  feature_tests.cc
  "${kDualSourceFragHeader}"
  "${kInstanceColorVertHeader}"
  "${kInstanceColorFragHeader}"
  "${kComputeF16Header}"
  "${kComputeSubgroupHeader}"
  "${kFullscreenVertHeader}"
  "${kTexSampleFragHeader}"
  "${kClipVertHeader}"
  "${kClipFragHeader}"
  "${kSolidRedFragHeader}"
)

target_include_directories(wgfx_feature_tests PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/shaders"
)

target_link_libraries(wgfx_feature_tests PRIVATE
  wgfx
)

# ---------------------------------------------------------------------------
# Bunny mark benchmark. Six variants: {batched, per-draw} x {shared storage,
# per-bunny uniform, baked vertices}. Build all of them through the
# "bunny_benchmarks" meta target.
# ---------------------------------------------------------------------------
set(WGFX_BUNNY_HEADERS
  "${kBunnySharedVertHeader}"
  "${kBunnyUboVertHeader}"
  "${kBunnyBakedVertHeader}"
  "${kBunnyFragHeader}"
)

function(wgfx_add_bunny_variant name batch_mode uniform_mode)
  add_executable(wgfx_bunny_${name} bunny_benchmark.cc
    ${WGFX_BUNNY_HEADERS})
  target_compile_definitions(wgfx_bunny_${name} PRIVATE
    BUNNY_BATCH_MODE=${batch_mode}
    BUNNY_UNIFORM_MODE=${uniform_mode}
    WGFX_NO_VALIDATION
  )
  target_include_directories(wgfx_bunny_${name} PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_link_libraries(wgfx_bunny_${name} PRIVATE wgfx glfw)
endfunction ()

wgfx_add_bunny_variant(batched_storage 1 0)
wgfx_add_bunny_variant(batched_ubo 1 1)
wgfx_add_bunny_variant(batched_baked 1 2)
wgfx_add_bunny_variant(dc_storage 0 0)
wgfx_add_bunny_variant(dc_ubo 0 1)
wgfx_add_bunny_variant(dc_baked 0 2)

add_executable(wgfx_quad_benchmark quad_benchmark.cc
  ${kQuadVertHeader} ${kQuadFragHeader})
target_include_directories(wgfx_quad_benchmark PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/shaders"
)
target_link_libraries(wgfx_quad_benchmark PRIVATE wgfx glfw)

add_custom_target(bunny_benchmarks
  DEPENDS wgfx_bunny_batched_storage wgfx_bunny_batched_ubo
          wgfx_bunny_batched_baked wgfx_bunny_dc_storage wgfx_bunny_dc_ubo
          wgfx_bunny_dc_baked
)
