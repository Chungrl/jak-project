#include "PostFx.h"

#include <array>
#include <cmath>

#include "common/log/log.h"

#include "game/graphics/gfx.h"

#include "third-party/imgui/imgui.h"

namespace {
float param(Gfx::PostParam which) {
  return Gfx::g_global_settings.post_params[which];
}

/*!
 * What a pass in the middle of the frame changes and the renderers after it may rely on.
 */
struct SavedState {
  GLboolean depth_test, stencil_test, scissor_test, blend, depth_mask;
  GLboolean color_mask[4];

  SavedState() {
    depth_test = glIsEnabled(GL_DEPTH_TEST);
    stencil_test = glIsEnabled(GL_STENCIL_TEST);
    scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    blend = glIsEnabled(GL_BLEND);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
    glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
  }

  static void set(GLenum what, GLboolean on) {
    if (on) {
      glEnable(what);
    } else {
      glDisable(what);
    }
  }

  void restore(const SharedRenderState* render_state) const {
    glBindFramebuffer(GL_FRAMEBUFFER, render_state->render_fb);
    glViewport(render_state->render_fb_x, render_state->render_fb_y, render_state->render_fb_w,
               render_state->render_fb_h);
    set(GL_DEPTH_TEST, depth_test);
    set(GL_STENCIL_TEST, stencil_test);
    set(GL_SCISSOR_TEST, scissor_test);
    set(GL_BLEND, blend);
    glDepthMask(depth_mask);
    glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
    glBlendEquation(GL_FUNC_ADD);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
  }
};

void bind_texture(int unit, GLuint tex) {
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, tex);
}
}  // namespace

PostFx::~PostFx() {
  free_target(m_depth);
  free_target(m_world_depth);
  free_target(m_ao[0]);
  free_target(m_ao[1]);
  free_target(m_scene);
  for (auto& level : m_bloom) {
    free_target(level);
  }
  if (m_ready) {
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
    glDeleteQueries(2, m_timers);
  }
}

void PostFx::init() {
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);
  constexpr std::array<float, 8> vertices = {-1, -1, -1, 1, 1, -1, 1, 1};
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  glGenQueries(2, m_timers);
  m_ready = true;
}

void PostFx::free_target(Target& target) {
  if (target.fbo) {
    glDeleteFramebuffers(1, &target.fbo);
    glDeleteTextures(1, &target.tex);
  }
  target = Target();
}

void PostFx::make_target(Target& target, int w, int h, GLint internal_format, bool depth) {
  free_target(target);
  target.w = std::max(w, 1);
  target.h = std::max(h, 1);
  glGenFramebuffers(1, &target.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
  glGenTextures(1, &target.tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, target.tex);
  if (depth) {
    // the same format as the render framebuffer's depth buffer, which a depth copy requires
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, target.w, target.h, 0, GL_DEPTH_STENCIL,
                 GL_UNSIGNED_INT_24_8, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, target.tex,
                           0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
  } else {
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, target.w, target.h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.tex, 0);
    GLenum render_targets[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, render_targets);
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    lg::error("[PostFx] framebuffer {}x{} (depth {}) is not complete", target.w, target.h, depth);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
}

void PostFx::resize(int w, int h) {
  if (!m_ready) {
    init();
  }
  if (w == m_w && h == m_h) {
    return;
  }
  m_w = w;
  m_h = h;
  make_target(m_depth, w, h, 0, true);
  make_target(m_world_depth, w, h, 0, true);
  make_target(m_ao[0], w, h, GL_RG8, false);
  make_target(m_ao[1], w, h, GL_RG8, false);
  make_target(m_scene, w, h, GL_RGBA8, false);
  // the bloom reads the average brightness of the picture from its smallest mip level
  glBindTexture(GL_TEXTURE_2D, m_scene.tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  for (int i = 0; i < kBloomLevels; i++) {
    make_target(m_bloom[i], w >> (i + 1), h >> (i + 1), GL_RGBA16F, false);
  }
}

void PostFx::begin_timer(int which) {
  if (m_timer_pending[which]) {
    GLint available = 0;
    glGetQueryObjectiv(m_timers[which], GL_QUERY_RESULT_AVAILABLE, &available);
    if (!available) {
      // keep waiting for the last one. Nothing is measured this frame.
      m_timers_active = false;
      return;
    }
    GLuint64 ns = 0;
    glGetQueryObjectui64v(m_timers[which], GL_QUERY_RESULT, &ns);
    m_gpu_ms[which] = m_gpu_ms[which] * 0.95f + (ns / 1.0e6f) * 0.05f;
    m_timer_pending[which] = false;
  }
  glBeginQuery(GL_TIME_ELAPSED, m_timers[which]);
  m_timer_pending[which] = true;
  m_timers_active = true;
}

void PostFx::end_timer() {
  if (m_timers_active) {
    glEndQuery(GL_TIME_ELAPSED);
    m_timers_active = false;
  }
}

void PostFx::draw_quad(ScopedProfilerNode& prof) {
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  prof.add_draw_call();
  prof.add_tri(2);
}

void PostFx::capture_world_depth(SharedRenderState* render_state) {
  if (Gfx::g_global_settings.post_enhance <= 0.f || !render_state->has_pc_data) {
    return;
  }
  const int w = render_state->render_fb_w;
  const int h = render_state->render_fb_h;
  SavedState saved;
  resize(w, h);
  glDisable(GL_SCISSOR_TEST);
  glDepthMask(GL_TRUE);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, render_state->render_fb);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_world_depth.fbo);
  glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  m_world_depth_frame = render_state->frame_idx;
  saved.restore(render_state);
}

void PostFx::apply_lighting(SharedRenderState* render_state, ScopedProfilerNode& prof) {
  const auto& settings = Gfx::g_global_settings;
  const float enhance = settings.post_enhance;
  // the depth of the picture can only be read back once a background renderer has published
  // the camera of this frame.
  if (enhance <= 0.f || !render_state->has_pc_data) {
    return;
  }

  // Depth as written by the shaders is clip z / clip w, with
  //   clip z = -(cam[3][2] + depth * s),  clip w = -(depth * pzw)
  // where s and pzw scale the camera's forward axis in the third and fourth column.
  const auto& cam = render_state->camera_pc;
  const float ww = cam[0][3] * cam[0][3] + cam[1][3] * cam[1][3] + cam[2][3] * cam[2][3];
  const float zw = cam[0][2] * cam[0][3] + cam[1][2] * cam[1][3] + cam[2][2] * cam[2][3];
  const float x_scale =
      std::sqrt(cam[0][0] * cam[0][0] + cam[1][0] * cam[1][0] + cam[2][0] * cam[2][0]);
  const float y_scale =
      std::sqrt(cam[0][1] * cam[0][1] + cam[1][1] * cam[1][1] + cam[2][1] * cam[2][1]);
  if (ww < 1e-20f || x_scale < 1e-12f || y_scale < 1e-12f) {
    return;
  }
  const float pzw = std::sqrt(ww);
  const float depth_b = zw / ww;
  const float depth_a = std::abs(cam[3][2] / pzw);
  // the height scaling that the vertex shaders apply after the camera (see Shader.cpp)
  const float height_scale = 0.5f * 512.f / 416.f;
  const float kx = pzw / x_scale;
  const float ky = pzw / (y_scale * height_scale);

  // The sun in the view space that the shaders rebuild from the depth: x and y as on screen,
  // z into the picture.
  const float side = cam[3][2] < 0.f ? -1.f : 1.f;
  float sun[3];
  {
    const float* d = settings.sun_dir;
    sun[0] = side * (cam[0][0] * d[0] + cam[1][0] * d[1] + cam[2][0] * d[2]) / x_scale;
    sun[1] = side * (cam[0][1] * d[0] + cam[1][1] * d[1] + cam[2][1] * d[2]) / y_scale;
    sun[2] = side * (cam[0][3] * d[0] + cam[1][3] * d[1] + cam[2][3] * d[2]) / pzw;
    const float len = std::sqrt(sun[0] * sun[0] + sun[1] * sun[1] + sun[2] * sun[2]);
    for (auto& x : sun) {
      x /= std::max(len, 1e-6f);
    }
  }
  const float* sun_color = settings.sun_color;
  const bool have_sun = sun_color[0] + sun_color[1] + sun_color[2] > 0.01f;

  // whether the depth of the world without the characters was captured this frame
  const bool have_world_depth = m_world_depth_frame == render_state->frame_idx;
  const float fade_start = 4096.f * 70.f;
  const float fade_end = 4096.f * 170.f;
  const int w = render_state->render_fb_w;
  const int h = render_state->render_fb_h;
  SavedState saved;
  resize(w, h);
  begin_timer(0);

  // the depth buffer and the picture so far, as textures
  glDisable(GL_SCISSOR_TEST);
  glDepthMask(GL_TRUE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, render_state->render_fb);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_depth.fbo);
  glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_scene.fbo);
  glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_BLEND);
  glDepthMask(GL_FALSE);
  glBindVertexArray(m_vao);
  glViewport(0, 0, w, h);

  // ambient occlusion and sun shadow
  {
    auto& shader = render_state->shaders[ShaderId::POSTFX_AO];
    shader.activate();
    glBindFramebuffer(GL_FRAMEBUFFER, m_ao[0].fbo);
    bind_texture(1, m_world_depth.tex);
    bind_texture(0, m_depth.tex);
    glUniform1i(glGetUniformLocation(shader.id(), "tex_T0"), 0);
    glUniform4f(glGetUniformLocation(shader.id(), "proj"), kx, ky, depth_a, depth_b);
    glUniform4f(
        glGetUniformLocation(shader.id(), "params"), 4096.f * param(Gfx::POST_AO_RADIUS),
        fade_start, fade_end,
        have_sun && have_world_depth && param(Gfx::POST_SUN_SHADOW) > 0.f ? 4096.f * 2.6f : 0.f);
    glUniform3f(glGetUniformLocation(shader.id(), "sun_dir"), sun[0], sun[1], sun[2]);
    glUniform2f(glGetUniformLocation(shader.id(), "texel"), 1.f / w, 1.f / h);
    draw_quad(prof);
  }

  // smooth them without crossing edges: across...
  {
    auto& shader = render_state->shaders[ShaderId::POSTFX_AO_BLUR];
    shader.activate();
    glBindFramebuffer(GL_FRAMEBUFFER, m_ao[1].fbo);
    bind_texture(1, m_depth.tex);
    bind_texture(0, m_ao[0].tex);
    glUniform1i(glGetUniformLocation(shader.id(), "tex_T0"), 0);
    glUniform4f(glGetUniformLocation(shader.id(), "proj"), kx, ky, depth_a, depth_b);
    glUniform2f(glGetUniformLocation(shader.id(), "step_uv"), 1.f / w, 0.f);
    draw_quad(prof);
  }

  // ...and down, while putting them and the sun's relief and shine on the picture
  {
    auto& shader = render_state->shaders[ShaderId::POSTFX_LIGHT];
    shader.activate();
    glBindFramebuffer(GL_FRAMEBUFFER, render_state->render_fb);
    bind_texture(3, have_world_depth ? m_world_depth.tex : m_depth.tex);
    bind_texture(2, m_scene.tex);
    bind_texture(1, m_depth.tex);
    bind_texture(0, m_ao[1].tex);
    glUniform1i(glGetUniformLocation(shader.id(), "tex_T0"), 0);
    glUniform1f(glGetUniformLocation(shader.id(), "rim"),
                have_world_depth ? enhance * param(Gfx::POST_RIM_LIGHT) : 0.f);
    glUniform4f(glGetUniformLocation(shader.id(), "proj"), kx, ky, depth_a, depth_b);
    glUniform2f(glGetUniformLocation(shader.id(), "texel"), 1.f / w, 1.f / h);
    glUniform3f(glGetUniformLocation(shader.id(), "sun_dir"), sun[0], sun[1], sun[2]);
    glUniform3f(glGetUniformLocation(shader.id(), "sun_color"), sun_color[0], sun_color[1],
                sun_color[2]);
    glUniform4f(glGetUniformLocation(shader.id(), "amounts"),
                enhance * param(Gfx::POST_AO_STRENGTH), enhance * param(Gfx::POST_SUN_SHADOW),
                have_sun ? enhance * param(Gfx::POST_RELIEF) : 0.f,
                have_sun ? enhance * param(Gfx::POST_SPECULAR) : 0.f);
    glUniform4f(glGetUniformLocation(shader.id(), "params"), fade_start, fade_end, 50.f,
                param(Gfx::POST_DEBUG_VIEW));
    // the destination alpha means something to the renderers that follow
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    draw_quad(prof);
    bind_texture(3, 0);
    bind_texture(2, 0);
  }

  end_timer();
  saved.restore(render_state);

  // say once what it costs
  if (++m_timed_frames == 900) {
    lg::info("[PostFx] at {}x{}: lighting {:.2f} ms, bloom {:.2f} ms of GPU time per frame", w, h,
             m_gpu_ms[0], m_gpu_ms[1]);
  }
}

void PostFx::prepare_bloom(SharedRenderState* render_state, ScopedProfilerNode& prof) {
  const float enhance = Gfx::g_global_settings.post_enhance;
  if (enhance <= 0.f || param(Gfx::POST_BLOOM) <= 0.f) {
    return;
  }
  const int w = render_state->render_fb_w;
  const int h = render_state->render_fb_h;
  SavedState saved;
  resize(w, h);
  begin_timer(1);

  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, render_state->render_fb);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_scene.fbo);
  glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_BLEND);
  glDepthMask(GL_FALSE);
  glBindVertexArray(m_vao);
  bind_texture(0, m_scene.tex);
  glGenerateMipmap(GL_TEXTURE_2D);

  // the bright parts, at half the size each time
  {
    auto& shader = render_state->shaders[ShaderId::POSTFX_BLOOM_DOWN];
    shader.activate();
    glUniform1i(glGetUniformLocation(shader.id(), "tex_T0"), 0);
    const Target* src = &m_scene;
    for (int i = 0; i < kBloomLevels; i++) {
      auto& dst = m_bloom[i];
      glBindFramebuffer(GL_FRAMEBUFFER, dst.fbo);
      glViewport(0, 0, dst.w, dst.h);
      bind_texture(0, src->tex);
      glUniform2f(glGetUniformLocation(shader.id(), "texel"), 1.f / src->w, 1.f / src->h);
      glUniform2f(glGetUniformLocation(shader.id(), "threshold"),
                  i == 0 ? param(Gfx::POST_BLOOM_THRESHOLD) : -1.f, 0.12f);
      draw_quad(prof);
      src = &dst;
    }
  }

  // and back up, each size mixed into the next larger one
  {
    auto& shader = render_state->shaders[ShaderId::POSTFX_BLOOM_UP];
    shader.activate();
    glUniform1i(glGetUniformLocation(shader.id(), "tex_T0"), 0);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = kBloomLevels - 2; i >= 0; i--) {
      auto& src = m_bloom[i + 1];
      auto& dst = m_bloom[i];
      glBindFramebuffer(GL_FRAMEBUFFER, dst.fbo);
      glViewport(0, 0, dst.w, dst.h);
      bind_texture(0, src.tex);
      glUniform2f(glGetUniformLocation(shader.id(), "texel"), 1.f / src.w, 1.f / src.h);
      glUniform1f(glGetUniformLocation(shader.id(), "mix_amount"), 0.62f);
      draw_quad(prof);
    }
  }

  m_bloom_frame = render_state->frame_idx;
  end_timer();
  saved.restore(render_state);
}

GLuint PostFx::bloom_texture(const SharedRenderState* render_state) const {
  return m_bloom_frame == render_state->frame_idx ? m_bloom[0].tex : 0;
}

void PostFx::draw_debug_window() {
  auto& settings = Gfx::g_global_settings;
  ImGui::Text("GPU time: lighting %.2f ms, bloom %.2f ms", m_gpu_ms[0], m_gpu_ms[1]);
  ImGui::SliderFloat("Strength (0 = original picture)", &settings.post_enhance, 0, 2);
  ImGui::SliderFloat("Ambient occlusion", &settings.post_params[Gfx::POST_AO_STRENGTH], 0, 2);
  ImGui::SliderFloat("Ambient occlusion radius (m)", &settings.post_params[Gfx::POST_AO_RADIUS],
                     0.2f, 4);
  ImGui::SliderFloat("Sun shadow", &settings.post_params[Gfx::POST_SUN_SHADOW], 0, 1);
  ImGui::SliderFloat("Surface relief", &settings.post_params[Gfx::POST_RELIEF], 0, 3);
  ImGui::SliderFloat("Surface shine", &settings.post_params[Gfx::POST_SPECULAR], 0, 3);
  ImGui::SliderFloat("Bloom", &settings.post_params[Gfx::POST_BLOOM], 0, 2);
  ImGui::SliderFloat("Bloom threshold", &settings.post_params[Gfx::POST_BLOOM_THRESHOLD], 0, 1);
  ImGui::SliderFloat("Sharpening", &settings.post_params[Gfx::POST_SHARPEN], 0, 2);
  ImGui::SliderFloat("Contrast", &settings.post_params[Gfx::POST_CONTRAST], 0, 2);
  ImGui::SliderFloat("Saturation", &settings.post_params[Gfx::POST_SATURATION], 0, 2);
  ImGui::SliderFloat("Vignette", &settings.post_params[Gfx::POST_VIGNETTE], 0, 2);
  ImGui::SliderFloat("Character rim light", &settings.post_params[Gfx::POST_RIM_LIGHT], 0, 2);
  bool debug_view = settings.post_params[Gfx::POST_DEBUG_VIEW] > 0.5f;
  if (ImGui::Checkbox("Show the added shading alone", &debug_view)) {
    settings.post_params[Gfx::POST_DEBUG_VIEW] = debug_view ? 1.f : 0.f;
  }
}
