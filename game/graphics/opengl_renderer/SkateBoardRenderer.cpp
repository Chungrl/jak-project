/*!
 * @file SkateBoardRenderer.cpp
 * (AI-assisted) Draws the skateboard supplied by the optional Skate 3 bridge.
 */

#include "SkateBoardRenderer.h"

SkateBoardMesh& skate_board_mesh() {
  static SkateBoardMesh mesh;
  return mesh;
}

SkateBoardRenderer::SkateBoardRenderer() {
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vertex_buffer);
}

SkateBoardRenderer::~SkateBoardRenderer() {
  release();
  glDeleteBuffers(1, &m_vertex_buffer);
  glDeleteVertexArrays(1, &m_vao);
}

void SkateBoardRenderer::release() {
  for (auto& s : m_surfaces) {
    glDeleteTextures(1, &s.texture);
    glDeleteBuffers(1, &s.index_buffer);
    glDeleteBuffers(1, &s.uv_buffer);
  }
  m_surfaces.clear();
}

void SkateBoardRenderer::rebuild(const SkateBoardMesh& mesh) {
  release();
  for (const auto& in : mesh.surfaces) {
    GlSurface out;
    out.first_vertex = in.first_vertex;
    out.vertex_count = in.vertex_count;
    out.index_count = (u32)in.indices.size();

    glGenBuffers(1, &out.index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, in.indices.size() * sizeof(u16), in.indices.data(),
                 GL_STATIC_DRAW);

    glGenBuffers(1, &out.uv_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, out.uv_buffer);
    glBufferData(GL_ARRAY_BUFFER, in.uvs.size() * sizeof(float), in.uvs.data(), GL_STATIC_DRAW);

    glGenTextures(1, &out.texture);
    glBindTexture(GL_TEXTURE_2D, out.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, in.width, in.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 in.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    m_surfaces.push_back(out);
  }
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  m_model_version = mesh.model_version;
}

void SkateBoardRenderer::render(SharedRenderState* render_state, ScopedProfilerNode& prof) {
  {
    auto& mesh = skate_board_mesh();
    std::lock_guard<std::mutex> lock(mesh.mutex);
    if (!mesh.visible || mesh.surfaces.empty() || mesh.vertices.empty()) {
      return;
    }
    // Buffer bindings made while rebuilding must land in this renderer's vertex array.
    glBindVertexArray(m_vao);
    if (mesh.model_version != m_model_version || m_surfaces.size() != mesh.surfaces.size()) {
      rebuild(mesh);
    }
    m_vertices = mesh.vertices;
  }
  render_state->shaders[ShaderId::SKATE_BOARD].activate();
  const auto shader = render_state->shaders[ShaderId::SKATE_BOARD].id();
  glUniformMatrix4fv(glGetUniformLocation(shader, "pc_camera"), 1, GL_FALSE,
                     render_state->camera_pc[0].data());
  glUniform4f(glGetUniformLocation(shader, "cam_trans"), render_state->camera_pos[0],
              render_state->camera_pos[1], render_state->camera_pos[2],
              render_state->camera_pos[3]);
  glUniform1i(glGetUniformLocation(shader, "tex"), 0);

  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(float), m_vertices.data(),
               GL_DYNAMIC_DRAW);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_GEQUAL);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glActiveTexture(GL_TEXTURE0);

  const u32 total_vertices = (u32)(m_vertices.size() / 6);
  for (const auto& s : m_surfaces) {
    if (s.first_vertex + s.vertex_count > total_vertices || s.index_count == 0) {
      continue;
    }
    const size_t base = (size_t)s.first_vertex * 6 * sizeof(float);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)base);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void*)(base + 3 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, s.uv_buffer);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s.index_buffer);
    glBindTexture(GL_TEXTURE_2D, s.texture);
    glDrawElements(GL_TRIANGLES, s.index_count, GL_UNSIGNED_SHORT, nullptr);
    prof.add_draw_call();
    prof.add_tri(s.index_count / 3);
  }

  glDisableVertexAttribArray(2);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindVertexArray(0);
  glEnable(GL_BLEND);
}
