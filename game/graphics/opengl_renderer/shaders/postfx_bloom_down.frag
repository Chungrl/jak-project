#version 410 core

// (AI-assisted) Bloom, on the way down: the picture at half the size, keeping only what is
// brighter than the threshold on the first step. See PostFx.cpp.

out vec4 color;
in vec2 tex_coord;

uniform sampler2D tex_T0;
uniform vec2 texel;     // of the source
uniform vec2 threshold; // threshold (negative: keep everything), softness of its edge

float limit;

vec3 fetch(float x, float y) {
  vec3 c = textureLod(tex_T0, tex_coord + vec2(x, y) * texel, 0.0).rgb;
  if (threshold.x >= 0.0) {
    float brightness = max(c.r, max(c.g, c.b));
    float soft = clamp(brightness - limit + threshold.y, 0.0, 2.0 * threshold.y);
    soft = soft * soft / (4.0 * threshold.y + 0.0001);
    c *= max(soft, brightness - limit) / max(brightness, 0.0001);
  }
  return c;
}

void main() {
  // What glows is what stands out from the rest of the picture: in a bright scene, like the
  // desert at noon, the threshold rises with the average brightness, so that the ground
  // does not turn into haze.
  vec3 average = textureLod(tex_T0, vec2(0.5), 20.0).rgb;
  limit = max(threshold.x, dot(average, vec3(0.299, 0.587, 0.114)) + 0.3);

  // 13 taps: a wide, smooth filter that does not flicker when bright things move
  vec3 outer = fetch(-2.0, 2.0) + fetch(2.0, 2.0) + fetch(-2.0, -2.0) + fetch(2.0, -2.0);
  vec3 edges = fetch(0.0, 2.0) + fetch(-2.0, 0.0) + fetch(2.0, 0.0) + fetch(0.0, -2.0);
  vec3 inner = fetch(-1.0, 1.0) + fetch(1.0, 1.0) + fetch(-1.0, -1.0) + fetch(1.0, -1.0);
  vec3 c = fetch(0.0, 0.0) * 0.125 + outer * 0.03125 + edges * 0.0625 + inner * 0.125;
  color = vec4(c, 1.0);
}
