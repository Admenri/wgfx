#version 450
// Dual-source blending test fragment shader (W3C DualSourceBlending).
layout(location = 0, index = 0) out vec4 outColor;
layout(location = 0, index = 1) out vec4 outBlend;

void main() {
  outColor = vec4(1.0, 0.0, 0.0, 1.0);
  outBlend = vec4(0.0, 0.0, 0.0, 0.5);
}
