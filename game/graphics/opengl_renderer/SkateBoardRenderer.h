#pragma once

/*!
 * @file SkateBoardRenderer.h
 * (AI-assisted) Draws the skateboard supplied by the optional Skate 3 bridge
 * (game/kernel/jak3/skate3_bridge.h). The model is not part of this project: the bridge
 * reads it at run time from the external engine and publishes it here.
 */

#include <mutex>
#include <vector>

#include "common/common_types.h"

#include "game/graphics/opengl_renderer/BucketRenderer.h"

/*!
 * The skateboard as the game thread last published it. Guarded by `mutex`; the game
 * thread writes, the render thread reads.
 */
struct SkateBoardMesh {
  struct Surface {
    u32 first_vertex = 0;
    u32 vertex_count = 0;
    std::vector<u16> indices;  // triangle list, relative to first_vertex
    std::vector<float> uvs;    // u, v per vertex of this surface
    std::vector<u8> rgba;      // width * height * 4
    u32 width = 0;
    u32 height = 0;
  };

  std::mutex mutex;
  bool visible = false;
  // Bumped whenever `surfaces` changes, so the renderer rebuilds its textures and indices.
  u32 model_version = 0;
  std::vector<Surface> surfaces;
  // Position then normal, 6 floats per vertex, in game world space.
  std::vector<float> vertices;
};

SkateBoardMesh& skate_board_mesh();

class SkateBoardRenderer {
 public:
  SkateBoardRenderer();
  ~SkateBoardRenderer();
  void render(SharedRenderState* render_state, ScopedProfilerNode& prof);

 private:
  struct GlSurface {
    GLuint texture = 0;
    GLuint index_buffer = 0;
    GLuint uv_buffer = 0;
    u32 first_vertex = 0;
    u32 vertex_count = 0;
    u32 index_count = 0;
  };
  void rebuild(const SkateBoardMesh& mesh);
  void release();

  GLuint m_vao = 0;
  GLuint m_vertex_buffer = 0;
  std::vector<GlSurface> m_surfaces;
  std::vector<float> m_vertices;
  u32 m_model_version = 0;
};
