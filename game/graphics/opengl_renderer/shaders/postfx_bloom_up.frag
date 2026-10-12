#version 410 core

// (AI-assisted) Bloom, on the way back up: a smaller level, smoothed, mixed into the next
// larger one by blending with the alpha given here. See PostFx.cpp.

out vec4 color;
in vec2 tex_coord;

uniform sampler2D tex_T0;
uniform vec2 texel; // of the source
uniform float mix_amount;

vec3 fetch(float x, float y) {
  return texture(tex_T0, tex_coord + vec2(x, y) * texel).rgb;
}

void main() {
  vec3 c = fetch(0.0, 0.0) * 4.0;
  c += (fetch(-1.0, 0.0) + fetch(1.0, 0.0) + fetch(0.0, -1.0) + fetch(0.0, 1.0)) * 2.0;
  c += fetch(-1.0, -1.0) + fetch(1.0, -1.0) + fetch(-1.0, 1.0) + fetch(1.0, 1.0);
  color = vec4(c / 16.0, mix_amount);
}
