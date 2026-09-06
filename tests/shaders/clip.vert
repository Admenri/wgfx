#version 450
// ClipDistances test: clips nothing but exercises the ClipDistance
// capability from a vertex shader.
layout(location = 0) out vec4 fragColor;

void main() {
  vec2 positions[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0),
                              vec2(-1.0, 3.0));
  vec2 position = positions[gl_VertexIndex];
  gl_Position = vec4(position, 0.5, 1.0);
  gl_ClipDistance[0] = 1.0;  // keep the triangle visible
  fragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
