#version 450
// Per-bunny uniform mode: each bunny owns a uniform buffer + bind group.
layout(location = 0) in vec2 inCorner;  // 0..1 quad corner
layout(location = 0) out vec2 uv;

layout(std140, binding = 0) uniform Bunny {
  vec4 pos_size;    // xy = NDC position, zw = NDC half size
  vec4 uv_offset;   // xy = uv origin, zw = uv scale
};

void main() {
  gl_Position = vec4(pos_size.xy + (inCorner * 2.0 - 1.0) * pos_size.zw, 0.0,
                     1.0);
  uv = uv_offset.xy + inCorner * uv_offset.zw;
}
