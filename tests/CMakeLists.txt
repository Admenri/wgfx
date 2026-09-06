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
