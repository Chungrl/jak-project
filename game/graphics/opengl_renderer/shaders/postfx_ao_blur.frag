#version 410 core

// (AI-assisted) The first, horizontal half of the blur of the ambient occlusion and sun
// shadow. It does not mix across steps in depth, so the shading stays on the surface it
// belongs to. The vertical half is done in postfx_light.frag. See PostFx.cpp.

out vec4 color;
in vec2 tex_coord;

uniform sampler2D tex_T0; // ambient occlusion and the two shadows
uniform sampler2D tex_T1; // depth
uniform vec4 proj;
uniform vec2 step_uv;

float view_depth(vec2 uv) {
  float d = textureLod(tex_T1, uv, 0.0).r;
  if (d < 0.00002) {
    return 1.0e9;
  }
  return proj.z / (d * 2.0 - 1.0 - proj.w);
}

void main() {
  const float WEIGHTS[5] = float[5](0.227, 0.194, 0.122, 0.054, 0.016);
  float depth = view_depth(tex_coord);
  float tolerance = 0.04 * depth;
  vec3 sum = textureLod(tex_T0, tex_coord, 0.0).rgb * WEIGHTS[0];
  float total = WEIGHTS[0];
  for (int i = 1; i < 5; i++) {
    for (int side = -1; side <= 1; side += 2) {
      vec2 uv = tex_coord + step_uv * float(i * side);
      float w = WEIGHTS[i] * max(0.0, 1.0 - abs(view_depth(uv) - depth) / tolerance);
      sum += textureLod(tex_T0, uv, 0.0).rgb * w;
      total += w;
    }
  }
  color = vec4(sum / total, 1.0);
}
