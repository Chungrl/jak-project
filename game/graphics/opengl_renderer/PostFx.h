#pragma once

/*!
 * @file PostFx.h
 * (AI-assisted) Optional image enhancement for Jak 3, on top of the original look:
 *  - ambient occlusion and short sun shadows, worked out from the depth buffer, and relief
 *    and shine of surfaces under the sun, worked out from the picture. These are put on the
 *    picture after the world and the characters are drawn, before water, particles and the
 *    HUD. The sun is the strongest light of the game's current mood (pc-set-sun-light).
 *  - bloom, gathered from the picture just before the HUD is drawn and added in the final
 *    pass (shaders/post_processing.frag), which also sharpens and grades the picture.
 * Everything is scaled by Gfx::g_global_settings.post_enhance (0 = off, the original
 * picture) and tuned by Gfx::g_global_settings.post_params.
 */

#include "game/graphics/opengl_renderer/BucketRenderer.h"

class PostFx {
 public:
  ~PostFx();

  /*!
   * Remember the depth buffer as it is now, with the world drawn and the characters not yet,
   * so that apply_lighting can tell the two apart.
   */
  void capture_world_depth(SharedRenderState* render_state);

  /*!
   * Shade everything drawn so far in the render framebuffer: darken creases, corners and
   * contact points, add short shadows away from the sun, and give surfaces relief and shine
   * under it.
   */
  void apply_lighting(SharedRenderState* render_state, ScopedProfilerNode& prof);

  /*!
   * Gather the glow of the bright parts of the render framebuffer as it is now.
   */
  void prepare_bloom(SharedRenderState* render_state, ScopedProfilerNode& prof);

  /*!
   * The texture made by prepare_bloom this frame, or 0 if there is none.
   */
  GLuint bloom_texture(const SharedRenderState* render_state) const;

  void draw_debug_window();

 private:
  struct Target {
    GLuint fbo = 0;
    GLuint tex = 0;
    int w = 0;
    int h = 0;
  };
  static constexpr int kBloomLevels = 6;

  void init();
  void resize(int w, int h);
  void make_target(Target& target, int w, int h, GLint internal_format, bool depth);
  void free_target(Target& target);
  void draw_quad(ScopedProfilerNode& prof);
  void begin_timer(int which);
  void end_timer();

  // GPU time of apply_lighting (0) and prepare_bloom (1), measured with timer queries
  GLuint m_timers[2] = {0, 0};
  bool m_timer_pending[2] = {false, false};
  float m_gpu_ms[2] = {0.f, 0.f};
  int m_timed_frames = 0;
  bool m_timers_active = false;

  bool m_ready = false;
  GLuint m_vao = 0;
  GLuint m_vbo = 0;
  int m_w = 0;
  int m_h = 0;
  Target m_depth;        // the depth buffer as a texture
  Target m_world_depth;  // the same, before the characters were drawn
  u64 m_world_depth_frame = ~u64(0);
  Target m_ao[2];  // ambient occlusion and sun shadow
  Target m_scene;  // the picture before the HUD
  Target m_bloom[kBloomLevels];
  u64 m_bloom_frame = ~u64(0);
};
