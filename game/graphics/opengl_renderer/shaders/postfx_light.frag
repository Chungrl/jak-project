#version 410 core

// (AI-assisted) Puts the extra shading on the picture: ambient occlusion and sun shadow
// (after the second half of their blur), and the relief and shine of surfaces under the
// sun. The relief is taken from the picture itself: darker is treated as deeper, which is
// how these textures were painted. See PostFx.cpp.

out vec4 color;
in vec2 tex_coord;

uniform sampler2D tex_T0; // ambient occlusion and sun shadow, blurred horizontally
uniform sampler2D tex_T1; // depth
uniform sampler2D tex_T2; // the picture so far
uniform sampler2D tex_T3; // depth before the characters were drawn
uniform sampler2D tex_T4; // the shading before its blur; its smallest mip is the average
uniform float world_shadow; // strength of the level's own shadows
uniform float rim;        // light on the edges of characters
uniform vec4 proj;
uniform vec2 texel;
uniform vec3 sun_dir;     // towards the sun, in view space
uniform vec3 sun_color;
uniform vec4 amounts;     // ambient occlusion, sun shadow, relief, shine
uniform vec4 params;      // distance where the effects start to fade, where they end,
                          // height of the relief, above 0.5: show the shading alone

float to_view_depth(float d) {
  if (d < 0.00002) {
    return 1.0e9;
  }
  return proj.z / (d * 2.0 - 1.0 - proj.w);
}

float view_depth(vec2 uv) {
  return to_view_depth(textureLod(tex_T1, uv, 0.0).r);
}

vec3 view_position(vec2 uv, float depth) {
  return vec3((uv * 2.0 - 1.0) * proj.xy * depth, depth);
}

float luma(vec3 c) {
  return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
  vec3 scene = textureLod(tex_T2, tex_coord, 0.0).rgb;
  float depth = view_depth(tex_coord);
  if (params.w > 1.5) {
    // debug: distance, in bands of 10 m (red), 100 m (green) and 1000 m (blue)
    float m = depth / 4096.0;
    color = vec4(fract(m / 10.0), fract(m / 100.0), fract(m / 1000.0), 1.0);
    return;
  }
  float fade = 1.0 - smoothstep(params.x, params.y, depth);
  if (fade <= 0.0) {
    color = vec4(scene, 1.0);
    return;
  }

  // second half of the blur
  const float WEIGHTS[5] = float[5](0.227, 0.194, 0.122, 0.054, 0.016);
  float tolerance = 0.04 * depth;
  vec3 sum = texture(tex_T0, tex_coord).rgb * WEIGHTS[0];
  float total = WEIGHTS[0];
  for (int i = 1; i < 5; i++) {
    for (int side = -1; side <= 1; side += 2) {
      vec2 uv = tex_coord + vec2(0.0, texel.y * float(i * side));
      float w = WEIGHTS[i] * max(0.0, 1.0 - abs(view_depth(uv) - depth) / tolerance);
      sum += texture(tex_T0, uv).rgb * w;
      total += w;
    }
  }
  vec3 shade = sum / total;
  // A character is whatever was drawn in front of the world after it. Its small shapes
  // would be darkened all over by shading sized for the world, so it gets less of it.
  float world_depth = to_view_depth(textureLod(tex_T3, tex_coord, 0.0).r);
  float character = depth < world_depth * 0.9985 - 4.0 ? 1.0 : 0.0;
  float brightness = luma(scene);

  float ao = mix(1.0, pow(shade.r, max(amounts.x, 1.0)), min(amounts.x, 1.0));
  // lights and glowing things are not darkened by what stands around them
  ao = mix(ao, 1.0, max(0.55 * character, smoothstep(0.6, 1.0, brightness) * 0.7));
  float sun_strength = clamp(luma(sun_color) * 1.4, 0.0, 1.0);
  float shadow = (1.0 - shade.g) * min(amounts.y, 1.0) * sun_strength * (1.0 - 0.6 * character);
  // The level's shadows only say something where part of the view is lit. Under a roof or
  // deep in a shadow everything is shadowed alike, and they are let go.
  float lit_share = textureLod(tex_T4, vec2(0.5), 20.0).b;
  float level_shadow = (1.0 - shade.b) * min(world_shadow, 1.0) * sun_strength *
                       smoothstep(0.12, 0.4, lit_share) * (1.0 - 0.5 * character);
  shadow = max(shadow, level_shadow);
  // shadows are a little cooler than what the sun lights
  vec3 shading = ao * mix(vec3(1.0), vec3(0.56, 0.6, 0.7), shadow);

  if (params.w > 0.5) {
    color = vec4(shading, 1.0);
    return;
  }

  // the surface, as in postfx_ao.frag
  vec3 p = view_position(tex_coord, depth);
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

  vec3 result = scene * shading;
  vec3 to_eye = normalize(-p);
  if (character > 0.5) {
    // a rim of light on the side of a character that is turned towards the sun
    float edge = pow(1.0 - max(dot(n, to_eye), 0.0), 3.0);
    float towards_sun = 0.3 + 0.7 * max(dot(n, sun_dir), 0.0);
    result += (sun_color * 0.5 + scene * 0.6) * edge * towards_sun * rim * 0.5 * fade;
  } else if (amounts.z > 0.0 || amounts.w > 0.0) {
    // Tilt the surface by the slope of the brightness of the picture across it.
    float hl = luma(textureLod(tex_T2, tex_coord - dx, 0.0).rgb);
    float hr = luma(textureLod(tex_T2, tex_coord + dx, 0.0).rgb);
    float hd = luma(textureLod(tex_T2, tex_coord - dy, 0.0).rgb);
    float hu = luma(textureLod(tex_T2, tex_coord + dy, 0.0).rgb);
    vec2 slope = vec2(hr - hl, hu - hd) * 0.5;
    vec3 r1 = cross(ddy, n);
    vec3 r2 = cross(n, ddx);
    float det = dot(ddx, r1);
    vec3 gradient = sign(det) * (slope.x * r1 + slope.y * r2) / max(abs(det), 0.000001);
    // up close a pixel covers very little of the surface: keep the relief from growing
    // without limit there
    float footprint = max(length(ddx), length(ddy));
    vec3 tilt = gradient * params.z * min(1.0, footprint / 10.0);
    float steepness = length(tilt);
    tilt *= min(1.0, 1.2 / max(steepness, 0.0001));
    vec3 bumped = normalize(n - tilt);

    // relief: only the change that the tilt makes to the sun's light, so that the
    // lighting painted into the world stays as it is
    float change = dot(bumped, sun_dir) - dot(n, sun_dir);
    result *= clamp(1.0 + change * amounts.z * 1.6 * sun_strength * fade, 0.55, 1.6);

    // shine: the sun's reflection on the tilted surface, where the sun reaches it
    vec3 halfway = normalize(sun_dir + to_eye);
    float facing = smoothstep(0.0, 0.25, dot(n, sun_dir));
    float shine = pow(max(dot(bumped, halfway), 0.0), 36.0);
    // more at grazing angles, as on real surfaces
    float graze = pow(1.0 - max(dot(bumped, to_eye), 0.0), 4.0);
    shine *= 0.25 + 0.75 * graze;
    result += sun_color * shine * facing * (1.0 - shadow) * ao * (0.25 + brightness) *
              amounts.w * 0.5 * fade;
  }

  color = vec4(result, 1.0);
}
