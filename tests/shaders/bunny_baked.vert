#version 450
// Baked vertex mode: no uniforms at all; the CPU bakes every bunny's final
// position and uv into the vertex buffer each frame.
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 0) out vec2 uv;

void main() {
  gl_Position = vec4(inPos, 0.0, 1.0);
  uv = inUV;
}
