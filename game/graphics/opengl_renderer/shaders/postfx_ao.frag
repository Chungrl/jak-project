#version 410 core

// (AI-assisted) Shading worked out from the depth buffer. See PostFx.cpp.
//  red:   ambient occlusion, how much of the space around each point is filled by nearby
//         surfaces (1 = open)
//  green: whether a character close by stands between the point and the sun (1 = lit),
//         found by walking the depth buffer
//  blue:  whether the level stands between the point and the sun (1 = lit), from the
//         shadow maps

out vec4 color;
in vec2 tex_coord;

uniform sampler2D tex_T0; // depth
uniform sampler2D tex_T1; // depth before the characters were drawn
uniform sampler2DShadow tex_T2; // the level as the sun sees it, near the camera
uniform sampler2DShadow tex_T3; // the same, far around it
uniform mat3 world_from_view;
uniform vec3 shadow_r;      // the shadow maps' right, up and depth directions in the world
uniform vec3 shadow_u;
uniform vec3 shadow_f;
uniform vec3 shadow_delta[2]; // the middle of each map, relative to the camera
uniform vec4 shadow_scale;  // 1 / half width of each map, 1 / half depth, above 0.5: on
uniform vec4 proj;        // view x per ndc x and depth, the same for y, depth scale, depth offset
uniform vec4 params;      // radius, distance where the effects start to fade, where they end,
                          // reach of the sun shadow (0: none)
uniform vec3 sun_dir;     // towards the sun, in view space
uniform vec2 texel;

const int SAMPLES = 20;
const float TURNS = 7.0;
const float TAU = 6.2831853;
const int SHADOW_STEPS = 20;

// distance in front of the camera, in game units
float to_view_depth(float d) {
  if (d < 0.00002) {
    return 1.0e9; // nothing was drawn here
  }
  return proj.z / (d * 2.0 - 1.0 - proj.w);
}

float view_depth(vec2 uv) {
  return to_view_depth(textureLod(tex_T0, uv, 0.0).r);
}

vec3 view_position(vec2 uv, float depth) {
  return vec3((uv * 2.0 - 1.0) * proj.xy * depth, depth);
}

// How much of the sun reaches a point of the level: 1 lit, 0 shadowed.
float level_lit(vec3 p, vec3 n, float noise) {
  for (int cascade = 0; cascade < 2; cascade++) {
    float scale = cascade == 0 ? shadow_scale.x : shadow_scale.y;
    // The collision mesh the map was drawn from is coarser than what is seen. Looking a
    // little off the surface keeps it from shadowing itself.
    float texel_size = 2.0 / (scale * 4096.0);
    vec3 w = world_from_view * (p + n * max(3.0 * texel_size, 700.0)) - shadow_delta[cascade];
    vec3 l = vec3(dot(w, shadow_r) * scale, dot(w, shadow_u) * scale, dot(w, shadow_f) * shadow_scale.z);
    if (abs(l.x) > 0.97 || abs(l.y) > 0.97) {
      continue;
    }
    vec3 at = l * 0.5 + 0.5;
    at.z -= 0.00002;
    // a ring of filtered lookups, turned differently at each pixel
    float spread = (cascade == 0 ? 2.2 : 1.4) / 4096.0;
    float sum = 0.0;
    for (int i = 0; i < 8; i++) {
      float a = TAU * (float(i) + noise) / 8.0;
      float radius = spread * sqrt((float(i) + 0.5) / 8.0);
      vec3 where = vec3(at.xy + vec2(cos(a), sin(a)) * radius, at.z);
      sum += cascade == 0 ? texture(tex_T2, where) : texture(tex_T3, where);
    }
    return sum / 8.0;
  }
  return 1.0;
}

void main() {
  float depth = view_depth(tex_coord);
  float fade = 1.0 - smoothstep(params.y, params.z, depth);
  if (fade <= 0.0) {
    color = vec4(1.0);
    return;
  }
  vec3 p = view_position(tex_coord, depth);

  // The surface normal, from the neighbour on each axis that is closer in depth, so that it
  // does not bend around the edges of objects.
  vec2 dx = vec2(texel.x, 0.0);
  vec2 dy = vec2(0.0, texel.y);
  float zl = view_depth(tex_coord - dx);
  float zr = view_depth(tex_coord + dx);
  float zd = view_depth(tex_coord - dy);
  float zu = view_depth(tex_coord + dy);
  vec3 ddx = abs(zr - depth) < abs(depth - zl) ? view_position(tex_coord + dx, zr) - p
                                                 : p - view_position(tex_coord - dx, zl);
  vec3 ddy = abs(zu - depth) < abs(depth - zd) ? view_position(tex_coord + dy, zu) - p
                                                 : p - view_position(tex_coord - dy, zd);
  vec3 n = normalize(cross(ddx, ddy));
  if (dot(n, p) > 0.0) {
    n = -n;
  }

  float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));

  // ambient occlusion. The radius on screen is limited so that surfaces right in front of
  // the camera stay cheap.
  float ao = 1.0;
  float radius = params.x;
  vec2 radius_uv = radius / (2.0 * proj.xy * depth);
  float limit = min(1.0, 0.11 / radius_uv.y);
  radius_uv *= limit;
  radius *= limit;
  if (radius_uv.y > 1.5 * texel.y) {
    // the bias also keeps steps of a few centimetres, as where two pieces of ground
    // overlap, from drawing a line
    float bias = 0.07 + 0.0015 * depth / radius;
    float sum = 0.0;
    for (int i = 0; i < SAMPLES; i++) {
      float t = (float(i) + noise) / float(SAMPLES);
      float angle = TAU * (t * TURNS + noise);
      vec2 uv = tex_coord + vec2(cos(angle), sin(angle)) * t * radius_uv;
      // in units of the radius
      vec3 v = (view_position(uv, view_depth(uv)) - p) / radius;
      float vv = dot(v, v);
      float vn = dot(v, n);
      float f = max(1.0 - vv, 0.0);
      sum += f * f * f * max((vn - bias) / (vv + 0.01), 0.0);
    }
    ao = max(0.0, 1.0 - sum * 5.0 / float(SAMPLES));
  }

  // sun shadow: walk from the point towards the sun and see whether the picture has a
  // character in the way. Only what is on screen can cast one, so it is kept short.
  float lit = 1.0;
  float facing = dot(n, sun_dir);
  if (params.w > 0.0 && facing > 0.02) {
    float reach = params.w;
    float thickness = 0.3 * reach;
    vec3 start = p + n * (0.004 * depth + 0.01 * reach);
    for (int i = 0; i < SHADOW_STEPS; i++) {
      float t = (float(i) + noise) / float(SHADOW_STEPS);
      vec3 q = start + sun_dir * (t * t * reach + 0.012 * reach);
      if (q.z < 1.0) {
        break;
      }
      vec2 uv = q.xy / (proj.xy * q.z) * 0.5 + 0.5;
      if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        break;
      }
      float blocker = view_depth(uv);
      float behind = q.z - blocker;
      bool character = blocker < to_view_depth(textureLod(tex_T1, uv, 0.0).r) * 0.9985 - 4.0;
      if (character && behind > 0.002 * q.z + 0.01 * reach && behind < thickness) {
        // softer towards the far end of the reach
        lit = min(lit, smoothstep(0.55, 1.0, t));
        break;
      }
    }
    // no hard line where the surface turns away from the sun
    lit = mix(1.0, lit, smoothstep(0.02, 0.2, facing));
  }

  // the level's own shadow
  float level = 1.0;
  if (shadow_scale.w > 0.5) {
    level = level_lit(p, n, noise);
    // A surface turned away from the sun is in its own shadow. The lighting painted into
    // the level already darkens it, so only part of the shadow is added there.
    level = min(level, mix(0.45, 1.0, smoothstep(-0.05, 0.2, facing)));
  }

  color = vec4(mix(1.0, ao, fade), mix(1.0, lit, fade), mix(1.0, level, fade), 1.0);
}
