#version 410 core

uniform sampler2D tex_T0;
out vec4 color;
in vec2 tex_coord;

uniform vec4 color_mult;
uniform vec4 color_add;

// (AI-assisted) optional image enhancement, off when enhance is 0. See PostFx.h.
uniform sampler2D tex_T1; // bloom, gathered before the HUD was drawn
uniform float enhance;
uniform vec4 fx;          // sharpening, bloom, contrast, saturation
uniform vec4 fx2;         // vignette, size of an output pixel in texture coordinates (x, y)

float luma(vec3 c) {
  return dot(c, vec3(0.299, 0.587, 0.114));
}

// Contrast-adaptive sharpening from the four direct neighbours: flat areas and hard edges
// are left alone, soft detail is brought out.
vec3 sharpen(vec3 centre) {
  vec3 n = texture(tex_T0, tex_coord + vec2(0.0, fx2.z)).rgb;
  vec3 s = texture(tex_T0, tex_coord - vec2(0.0, fx2.z)).rgb;
  vec3 e = texture(tex_T0, tex_coord + vec2(fx2.y, 0.0)).rgb;
  vec3 w = texture(tex_T0, tex_coord - vec2(fx2.y, 0.0)).rgb;
  vec3 lo = min(centre, min(min(n, s), min(e, w)));
  vec3 hi = max(centre, max(max(n, s), max(e, w)));
  vec3 headroom = min(lo, 1.0 - hi) / max(hi, vec3(0.001));
  vec3 amount = sqrt(clamp(headroom, 0.0, 1.0)) * (-0.2 * min(fx.x, 1.2));
  return clamp((centre + (n + s + e + w) * amount) / (1.0 + 4.0 * amount), 0.0, 1.0);
}

void main() {
  vec3 c = texture(tex_T0, tex_coord).rgb;

  if (enhance > 0.0) {
    if (fx.x > 0.0) {
      c = sharpen(c);
    }

    // the glow of bright things, added so that it cannot overflow
    vec3 glow = min(texture(tex_T1, tex_coord).rgb * fx.y, vec3(1.0));
    c = 1.0 - (1.0 - c) * (1.0 - glow);

    // a gentle S-curve for contrast, with the dark end kept open
    vec3 curved = c * c * (3.0 - 2.0 * c);
    c = mix(c, curved, 0.24 * fx.z);
    // the added shading takes light away; give a little back
    c *= 1.0 + 0.07 * enhance;

    // more colour where there is little, less of an increase where it is already strong
    float strongest = max(c.r, max(c.g, c.b));
    float weakest = min(c.r, min(c.g, c.b));
    float l = luma(c);
    c = mix(vec3(l), c, 1.0 + 0.3 * fx.w * (1.0 - (strongest - weakest)));

    // darken the corners slightly
    vec2 centred = tex_coord - 0.5;
    c *= 1.0 - 0.28 * fx2.x * dot(centred, centred);

    // a pixel of noise against banding in skies and fog
    float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    c += (noise - 0.5) / 255.0;
    c = clamp(c, 0.0, 1.0);
  }

  color = vec4(c * color_mult.rgb * color_mult.a, 1.0) + color_add;
}
