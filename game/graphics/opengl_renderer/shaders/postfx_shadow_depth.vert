#version 410 core

// (AI-assisted) Draws the level's collision mesh as the sun sees it, for the shadow map of
// the image enhancement. See PostFx.cpp.

layout (location = 0) in vec3 position_in;
layout (location = 3) in uint pat;

uniform vec3 center;  // world position at the middle of the map
uniform vec3 axis_r;  // the map's right, up and depth directions, divided by their extents
uniform vec3 axis_u;
uniform vec3 axis_f;

void main() {
  vec3 p = position_in - center;
  gl_Position = vec4(dot(p, axis_r), dot(p, axis_u), dot(p, axis_f), 1.0);
  // surfaces that only stop the camera are not there to be seen
  if ((pat & 1u) != 0u) {
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
  }
}
