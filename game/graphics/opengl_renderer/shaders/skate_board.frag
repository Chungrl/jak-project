#version 410 core

// (AI-assisted) World-space mesh supplied by the optional Skate 3 bridge.

in vec2 uv;
in float light;

uniform sampler2D tex;

out vec4 color;

void main() {
  vec4 texel = texture(tex, uv);
  color = vec4(texel.rgb * light, 1.0);
}
