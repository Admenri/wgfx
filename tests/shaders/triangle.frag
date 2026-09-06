#version 450
// Transpiled from the W3C webgpu-samples "triangle" WGSL fragment shader.
layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
  outColor = vec4(fragColor, 1.0);
}
