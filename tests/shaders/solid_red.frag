#version 450
// Float32Blendable test: writes a constant color to be blended on a
// 32-bit float attachment.
layout(location = 0) out vec4 outColor;

void main() {
  outColor = vec4(0.25, 0.5, 0.75, 1.0);
}
