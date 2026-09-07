#version 450
// Shared fragment shader for the bunny benchmark: textured quad.
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform texture2D tex;
layout(binding = 2) uniform sampler samp;

void main() {
  outColor = texture(sampler2D(tex, samp), uv);
}
