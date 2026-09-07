#version 450
// Shared-storage bunny mode: one storage buffer carries every bunny's
// transform; vertices carry the bunny index.
layout(location = 0) in vec2 inCorner;    // -1..1 quad corner
layout(location = 1) in vec2 inUV;        // atlas uv
layout(location = 2) in uint inBunnyId;
layout(location = 0) out vec2 uv;

layout(std430, binding = 0) readonly buffer Bunnies {
  vec4 bunnyData[];  // xy = NDC position, zw = NDC half size
};

void main() {
  vec4 data = bunnyData[inBunnyId];
  gl_Position = vec4(data.xy + inCorner * data.zw, 0.0, 1.0);
  uv = inUV;
}
