#version 450
// wgpu_quad benchmark vertex shader: rotates the quad around Z by `time`
// (pushed through the pipeline's 16-byte immediate block per draw).
layout(push_constant) uniform Uniforms {
  float time;
  float aspect;  // declared for parity with main.cc, unused
  vec2 pad;
} u;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 fragColor;

void main() {
  float c = cos(u.time);
  float s = sin(u.time);
  vec2 r = vec2(position.x * c - position.y * s, position.x * s + position.y * c);
  gl_Position = vec4(r, 0.0, 1.0);
  fragColor = color;
}
