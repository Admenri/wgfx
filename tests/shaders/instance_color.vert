#version 450
// Indirect-first-instance test: the instance index decides the color.
layout(location = 0) in vec2 inPosition;
layout(location = 0) out vec4 fragColor;

void main() {
  gl_Position = vec4(inPosition, 0.0, 1.0);
  fragColor = gl_InstanceIndex == 1 ? vec4(0.0, 1.0, 0.0, 1.0)
                                    : vec4(1.0, 0.0, 0.0, 1.0);
}
