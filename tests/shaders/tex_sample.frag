#version 450
// Samples a texture with separate sampler and texture bindings.
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform texture2D tex;
layout(binding = 1) uniform sampler samp;

void main() {
  outColor = texture(sampler2D(tex, samp), uv);
}
