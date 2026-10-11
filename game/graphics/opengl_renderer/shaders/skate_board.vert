#version 410 core

// (AI-assisted) World-space mesh supplied by the optional Skate 3 bridge.

layout (location = 0) in vec3 position_in;
layout (location = 1) in vec3 normal_in;
layout (location = 2) in vec2 uv_in;

uniform vec4 cam_trans;
uniform mat4 pc_camera;

out vec2 uv;
out float light;

void main() {
  // the same camera transform as the background geometry (see tfrag3.vert)
  vec3 vert = position_in - cam_trans.xyz;
  vec4 transformed = -pc_camera[3];
  transformed.w = 0;
  transformed -= pc_camera[0] * vert.x;
  transformed -= pc_camera[1] * vert.y;
  transformed -= pc_camera[2] * vert.z;
  transformed.y *= SCISSOR_ADJUST * HEIGHT_SCALE;
  gl_Position = transformed;

  uv = uv_in;
  // ambient plus a sun from above and slightly to one side
  vec3 n = normalize(normal_in);
  light = 0.55 + 0.45 * max(dot(n, normalize(vec3(0.3, 1.0, 0.2))), 0.0);
}
