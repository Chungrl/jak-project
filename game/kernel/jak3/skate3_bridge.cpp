/*!
 * @file skate3_bridge.cpp
 * (AI-assisted) Optional bridge from GOAL to an external Skate 3 gameplay engine.
 * See skate3_bridge.h. Nothing from the engine or from Skate 3 is compiled into gk: the
 * library is located and loaded at run time, and only on request from GOAL.
 */

#include "skate3_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include "common/log/log.h"

#include "game/graphics/opengl_renderer/SkateBoardRenderer.h"
#include "game/kernel/common/Ptr.h"
#include "game/kernel/common/kscheme.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace jak3 {
namespace skate3_bridge {

namespace {

// Layouts of the engine's C interface. The engine's "host space" is Z up.
struct SkControls {
  u16 buttons;
  u8 triggers[2];
  s16 left[2];
  s16 right[2];
};
static_assert(sizeof(SkControls) == 12, "SkControls layout");

struct SkPose {
  u64 tick;
  float root[16];  // column-major
  float velocity[3];
  u32 bone_count;
  s32 has_camera;
  float camera_position[3];
  float camera_forward[3];
  float camera_up[3];
  float camera_fov;
};
static_assert(sizeof(SkPose) == 136, "SkPose layout");

struct SkFeedback {
  u32 state;
  u32 wheels;
  float board_velocity[3];
  float board_spin[3];
  float rider_velocity[3];
  u32 pops, landings, grinds, bails;
  float pop_speed;
  float landing_speed;
  float bail_speed;
  u32 landing_grade;
  float brake;
  u32 dismounts;
  u32 bail_checks[2];
  u32 bail_first_check;
};
static_assert(sizeof(SkFeedback) == 96, "SkFeedback layout");

struct SkWorldInfo {
  u32 generation;
  u32 triangles;
  u32 rails;
  u32 failures;
  float build_ms;
};
static_assert(sizeof(SkWorldInfo) == 20, "SkWorldInfo layout");

struct SkScore {
  char trick[64];  // the engine's text key for the last trick, e.g. ID_TRICK_FLIP_KICKFLIP
  u32 tricks;
  u32 converts;
  u32 sequences;
  u32 bails;
  u32 banks;
  u32 stance;
  s32 sequence_active;
  float sequence;
  float line;
  float total;
  float multiplier;
  float line_time;
  float last_reward;
  float banked;
};
static_assert(sizeof(SkScore) == 120, "SkScore layout");

struct SkSession;

// GOAL's collide-cache-tri: three vertices (xyzw) and 16 bytes of surface data.
struct CollideCacheTri {
  float vertex[3][4];
  u8 extra[16];
};
static_assert(sizeof(CollideCacheTri) == 64, "collide-cache-tri layout");

// GOAL's collide-cache holds at most this many triangles.
constexpr u32 kMaxAppend = 461;

constexpr float kUnitsPerMeter = 4096.f;
constexpr u32 kMaxRigBones = 96;
// Most fixed steps one kStep runs, so a long hitch cannot stall the game.
constexpr int kMaxStepsPerFrame = 4;

// A host bone as the engine's C interface takes it.
struct SkRigBone {
  const char* skate;
  const char* skate_child;
  s32 parent;
  s32 child;
  float bind[16];
};

// One textured part of the engine's skateboard model. Only the vertex ranges are used here.
struct SkBoardSurface {
  u32 first_vertex;
  u32 vertex_count;
  const u16* indices;
  u32 index_count;
  const float* uvs;
  const u8* rgba;
  u32 width, height;
};

// The joint of Jak's that carries the jetboard model (and Daxter while riding).
const char* const kBoardJoint = "board";

// Which of Jak's joints follow which bones of the Skate 3 skater. `child` is the joint at
// the far end of the bone's segment. Joints not listed ride on their parents.
struct RigMapEntry {
  const char* joint;
  const char* skate;
  const char* skate_child;
  const char* child;
};
const RigMapEntry kRigMap[] = {
    {"main", "HIPS", "SPINE", "upper_body"},
    {"upper_body", "SPINE", "SPINE2", "chest"},
    {"chest", "SPINE2", "NECK", "neckA"},
    {"Lshould", "LEFTSHOULDER", "LEFTARM", "Larm"},
    {"Larm", "LEFTARM", "LEFTFOREARM", "Lforarm"},
    {"Lforarm", "LEFTFOREARM", "LEFTHAND", "sk_lhand"},
    {"sk_lhand", "LEFTHAND", nullptr, nullptr},
    {"Rshould", "RIGHTSHOULDER", "RIGHTARM", "Rarm"},
    {"Rarm", "RIGHTARM", "RIGHTFOREARM", "Rforarm"},
    {"Rforarm", "RIGHTFOREARM", "RIGHTHAND", "sk_rhand"},
    {"sk_rhand", "RIGHTHAND", nullptr, nullptr},
    {"Lthigh", "LEFTUPLEG", "LEFTLEG", "Lknee"},
    {"Lknee", "LEFTLEG", "LEFTFOOT", "Lankle"},
    {"Lankle", "LEFTFOOT", "LEFTTOEBASE", "Lball"},
    {"Lball", "LEFTTOEBASE", nullptr, nullptr},
    {"Rthigh", "RIGHTUPLEG", "RIGHTLEG", "Rknee"},
    {"Rknee", "RIGHTLEG", "RIGHTFOOT", "Rankle"},
    {"Rankle", "RIGHTFOOT", "RIGHTTOEBASE", "Rball"},
    {"Rball", "RIGHTTOEBASE", nullptr, nullptr},
};
// Half-size of the flat ground laid under the skater on activation, in game units (500 m).
constexpr float kFlatGroundExtent = 500.f * 4096.f;

#ifdef _WIN32
struct Engine {
  HMODULE library = nullptr;
  SkSession* session = nullptr;
  bool load_attempted = false;
  bool active = false;
  float last_forward[3] = {0.f, 0.f, 1.f};
  // Triangles gathered for the next world, in the engine's host space, 9 floats each.
  std::vector<float> world;
  std::unordered_set<u64> world_seen;
  bool winding_logged = false;
  // Size of the host character relative to its model; fixed when the session starts.
  float unit_scale = 1.f;
  // Game units per engine metre, including the world scale; fixed when the session starts.
  float units_per_meter = 4096.f;
  float world_scale = 1.f;
  u32 rig_count = 0;
  // The skateboard's transform is recovered from three vertices of its deck: nose, tail and
  // one on the deck's edge. `board_fix` turns the frame they span into the board's frame.
  s32 board_joint = -1;
  u32 board_vertex_count = 0;
  u32 board_ref[3] = {0, 0, 0};
  float board_fix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  bool board_ready = false;
  std::vector<float> board_vertices;
  float slowest_build_ms = 20.f;
  // Game time not yet turned into fixed engine steps.
  float time_bank = 0.f;
  float period = 1.f / 60.f;

  SkSession*(__cdecl* session_new)(const char*,
                                   const float*,
                                   u32,
                                   const float*,
                                   const u32*,
                                   u32,
                                   float) = nullptr;
  void(__cdecl* session_free)(SkSession*) = nullptr;
  int(__cdecl* install_world)(SkSession*, const float*, u32) = nullptr;
  int(__cdecl* queue_world)(SkSession*, const float*, u32) = nullptr;
  int(__cdecl* world_info)(SkSession*, SkWorldInfo*) = nullptr;
  int(__cdecl* rig_setup)(SkSession*, const SkRigBone*, u32, float) = nullptr;
  int(__cdecl* rig_pose)(SkSession*, float*, u32) = nullptr;
  float(__cdecl* step_period)(SkSession*) = nullptr;
  int(__cdecl* board_mesh)(SkSession*, SkBoardSurface*, u32) = nullptr;
  int(__cdecl* board_pose)(SkSession*, float*, u32) = nullptr;
  int(__cdecl* set_velocity)(SkSession*, const float*) = nullptr;
  // optional: only in a wrapper built with the local debug additions
  int(__cdecl* debug_rail_count)() = nullptr;
  int(__cdecl* debug_rail)(u32, float*, u32) = nullptr;
  int(__cdecl* activate)(SkSession*, const float*, float) = nullptr;
  int(__cdecl* step)(SkSession*, const SkControls*) = nullptr;
  int(__cdecl* get_pose)(SkSession*, SkPose*) = nullptr;
  int(__cdecl* feedback)(SkSession*, SkFeedback*) = nullptr;
  int(__cdecl* score)(SkSession*, SkScore*) = nullptr;
  const char*(__cdecl* last_error)() = nullptr;
};

Engine g_engine;

std::string env(const char* name) {
  const char* value = std::getenv(name);
  return value ? std::string(value) : std::string();
}

const char* engine_error() {
  const char* message = g_engine.last_error ? g_engine.last_error() : nullptr;
  return message ? message : "(no message)";
}

template <typename T>
bool resolve(T& out, const char* name) {
  out = (T)GetProcAddress(g_engine.library, name);
  if (!out) {
    lg::error("[skate3] {} is missing from the engine library", name);
  }
  return out != nullptr;
}

bool load_library() {
  if (g_engine.load_attempted) {
    return g_engine.library != nullptr;
  }
  g_engine.load_attempted = true;
  const auto path = env("OPENGOAL_SKATE3_DLL");
  if (path.empty()) {
    lg::warn("[skate3] OPENGOAL_SKATE3_DLL is not set; the Skate 3 bridge is unavailable");
    return false;
  }
  g_engine.library = LoadLibraryA(path.c_str());
  if (!g_engine.library) {
    lg::error("[skate3] could not load {} (error {})", path, GetLastError());
    return false;
  }
  bool ok = resolve(g_engine.session_new, "sk_session_new");
  ok = resolve(g_engine.session_free, "sk_session_free") && ok;
  ok = resolve(g_engine.install_world, "sk_install_world") && ok;
  ok = resolve(g_engine.queue_world, "sk_queue_world") && ok;
  ok = resolve(g_engine.world_info, "sk_world_info") && ok;
  ok = resolve(g_engine.rig_setup, "sk_rig_setup") && ok;
  ok = resolve(g_engine.rig_pose, "sk_rig_pose") && ok;
  ok = resolve(g_engine.step_period, "sk_period") && ok;
  ok = resolve(g_engine.board_mesh, "sk_board_mesh") && ok;
  ok = resolve(g_engine.board_pose, "sk_board_pose") && ok;
  ok = resolve(g_engine.set_velocity, "sk_set_velocity") && ok;
  g_engine.debug_rail_count =
      (int(__cdecl*)())GetProcAddress(g_engine.library, "sk_debug_rail_count");
  g_engine.debug_rail =
      (int(__cdecl*)(u32, float*, u32))GetProcAddress(g_engine.library, "sk_debug_rail");
  ok = resolve(g_engine.activate, "sk_activate") && ok;
  ok = resolve(g_engine.step, "sk_step") && ok;
  ok = resolve(g_engine.get_pose, "sk_get_pose") && ok;
  ok = resolve(g_engine.feedback, "sk_feedback") && ok;
  ok = resolve(g_engine.score, "sk_score") && ok;
  ok = resolve(g_engine.last_error, "sk_last_error") && ok;
  if (!ok) {
    FreeLibrary(g_engine.library);
    g_engine.library = nullptr;
  }
  return ok;
}

// Game axes are Y up; the engine's host space is Z up. This is a rotation about X, so
// handedness is preserved: host = (x, -z, y), game = (x, z, -y).
void game_to_host(const float* game, float* host) {
  host[0] = game[0];
  host[1] = -game[2];
  host[2] = game[1];
}

void host_to_game(const float* host, float* game) {
  game[0] = host[0];
  game[1] = host[2];
  game[2] = -host[1];
}

// 4x4 matrices below are column-major, which is also how GOAL lays out a matrix in memory.
void mat_mul(const float* a, const float* b, float* out) {
  float r[16];
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 4; row++) {
      r[col * 4 + row] = a[0 * 4 + row] * b[col * 4 + 0] + a[1 * 4 + row] * b[col * 4 + 1] +
                         a[2 * 4 + row] * b[col * 4 + 2] + a[3 * 4 + row] * b[col * 4 + 3];
    }
  }
  memcpy(out, r, sizeof(r));
}

// The game-to-host rotation of game_to_host as a matrix, and its inverse.
const float kGameToHost[16] = {1, 0, 0, 0, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1};
const float kHostToGame[16] = {1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1};

/*!
 * Invert a rotation-scale-translation matrix and return the result as a rigid transform,
 * with the scale dropped from its rotation. false if the matrix is degenerate.
 */
bool rigid_inverse(const float* m, float* out) {
  float axis[3][3];
  float scale[3];
  for (int col = 0; col < 3; col++) {
    scale[col] = std::sqrt(m[col * 4] * m[col * 4] + m[col * 4 + 1] * m[col * 4 + 1] +
                           m[col * 4 + 2] * m[col * 4 + 2]);
    if (scale[col] < 1e-6f) {
      return false;
    }
    for (int row = 0; row < 3; row++) {
      axis[col][row] = m[col * 4 + row] / scale[col];
    }
  }
  memset(out, 0, 16 * sizeof(float));
  // The inverse rotation is the transpose.
  for (int col = 0; col < 3; col++) {
    for (int row = 0; row < 3; row++) {
      out[col * 4 + row] = axis[row][col];
    }
  }
  // p = R S x + t  =>  x = S^-1 R^T (p - t), so the inverse translation is -S^-1 R^T t.
  for (int row = 0; row < 3; row++) {
    out[12 + row] =
        -(axis[row][0] * m[12] + axis[row][1] * m[13] + axis[row][2] * m[14]) / scale[row];
  }
  out[15] = 1.f;
  return true;
}

/*!
 * Two upward-facing triangles forming a square of flat ground centred on a game position.
 */
void build_flat_ground(const float* game_center, float* triangles) {
  float c[3];
  game_to_host(game_center, c);
  const float e = kFlatGroundExtent;
  const float quad[6][2] = {{-e, -e}, {e, -e}, {e, e}, {-e, -e}, {e, e}, {-e, e}};
  for (int i = 0; i < 6; i++) {
    triangles[i * 3 + 0] = c[0] + quad[i][0];
    triangles[i * 3 + 1] = c[1] + quad[i][1];
    triangles[i * 3 + 2] = c[2];
  }
}

/*!
 * Turn the engine's text key for a trick into something to show: ID_TRICK_FLIP_KICKFLIP
 * becomes KICKFLIP. Writes at most `capacity` bytes including the terminator.
 */
void readable_trick_name(const char* key, char* out, size_t capacity) {
  const char* name = key;
  const char* prefix = "ID_TRICK_";
  if (strncmp(name, prefix, strlen(prefix)) == 0) {
    name += strlen(prefix);
    // the first word is the kind of trick; keep it only when it is the whole name
    if (const char* rest = strchr(name, '_')) {
      name = rest + 1;
    }
  }
  size_t length = 0;
  for (; name[length] && length + 1 < capacity; length++) {
    out[length] = name[length] == '_' ? ' ' : name[length];
  }
  out[length] = 0;
}

u64 hash_triangle(const float* triangle) {
  u64 hash = 1469598103934665603ull;
  for (int i = 0; i < 9; i++) {
    u32 bits;
    memcpy(&bits, &triangle[i], sizeof(bits));
    hash = (hash ^ bits) * 1099511628211ull;
  }
  return hash;
}

/*!
 * The engine takes a triangle's solid side from its winding, and the game's collision is
 * already wound the way it expects: ground faces up. (Measured in Spargus: every near-level
 * triangle around Jak faced up. Deciding by majority instead is wrong wherever there is more
 * underside than floor, such as on Haven City's piers.) This only reports the counts once.
 */
void orient_world() {
  if (g_engine.winding_logged) {
    return;
  }
  g_engine.winding_logged = true;
  const auto& w = g_engine.world;
  size_t up = 0;
  size_t down = 0;
  for (size_t i = 0; i + 9 <= w.size(); i += 9) {
    const float* t = &w[i];
    const float e1[3] = {t[3] - t[0], t[4] - t[1], t[5] - t[2]};
    const float e2[3] = {t[6] - t[0], t[7] - t[1], t[8] - t[2]};
    const float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                        e1[0] * e2[1] - e1[1] * e2[0]};
    const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (length <= 0.f || std::abs(n[2]) < 0.7f * length) {
      continue;
    }
    (n[2] > 0.f ? up : down)++;
  }
  lg::info("[skate3] level collision: {} near-level triangles face up, {} down", up, down);
}

void do_world_begin(BridgeIO* io) {
  g_engine.world.clear();
  g_engine.world_seen.clear();
  io->status = kOk;
}

void do_world_append(BridgeIO* io) {
  if (!io->world_tris || io->world_count > kMaxAppend) {
    io->status = kBadCommand;
    return;
  }
  const auto* tris = Ptr<CollideCacheTri>(io->world_tris).c();
  for (u32 i = 0; i < io->world_count; i++) {
    float host[9];
    for (int v = 0; v < 3; v++) {
      game_to_host(tris[i].vertex[v], &host[v * 3]);
    }
    // Neighbouring gather boxes overlap at their shared faces.
    if (g_engine.world_seen.insert(hash_triangle(host)).second) {
      g_engine.world.insert(g_engine.world.end(), host, host + 9);
    }
  }
  io->status = kOk;
}

void do_world_commit(BridgeIO* io) {
  if (!g_engine.session) {
    io->status = kNotActive;
    return;
  }
  if (g_engine.world.empty()) {
    // Nothing nearby (far above the level, or between levels): keep the current world.
    io->status = kOk;
    return;
  }
  orient_world();
  if (g_engine.queue_world(g_engine.session, g_engine.world.data(),
                           (u32)(g_engine.world.size() / 9)) < 0) {
    lg::error("[skate3] queueing the level collision failed: {}", engine_error());
    io->status = kEngineError;
    return;
  }
  io->status = kOk;
}

void write_outputs(BridgeIO* io) {
  SkPose pose;
  SkFeedback fb;
  memset(&pose, 0, sizeof(pose));
  memset(&fb, 0, sizeof(fb));
  if (g_engine.get_pose(g_engine.session, &pose) != 0 ||
      g_engine.feedback(g_engine.session, &fb) != 0) {
    lg::error("[skate3] reading the pose failed: {}", engine_error());
    io->status = kEngineError;
    return;
  }
  host_to_game(&pose.root[12], io->position);
  io->position[3] = 1.f;
  host_to_game(pose.velocity, io->velocity);
  io->velocity[3] = 0.f;

  // The skater faces along the root's -Y axis in host space. Keep the last direction when
  // that is close to vertical (mid-flip or bailed).
  const float host_facing[3] = {-pose.root[4], -pose.root[5], -pose.root[6]};
  float facing[3];
  host_to_game(host_facing, facing);
  const float flat = std::sqrt(facing[0] * facing[0] + facing[2] * facing[2]);
  if (flat > 0.3f) {
    g_engine.last_forward[0] = facing[0] / flat;
    g_engine.last_forward[1] = 0.f;
    g_engine.last_forward[2] = facing[2] / flat;
  }
  io->forward[0] = g_engine.last_forward[0];
  io->forward[1] = 0.f;
  io->forward[2] = g_engine.last_forward[2];
  io->forward[3] = 0.f;

  io->state = fb.state;
  io->wheels = fb.wheels;
  io->pops = fb.pops;
  io->landings = fb.landings;
  io->grinds = fb.grinds;
  io->bails = fb.bails;
  io->tick = (u32)pose.tick;
  SkWorldInfo info;
  memset(&info, 0, sizeof(info));
  if (g_engine.world_info(g_engine.session, &info) == 0) {
    io->world_triangles = info.triangles;
    io->world_rails = info.rails;
    // a world that takes long to build is worth knowing about; say so once per new record
    if (info.build_ms > g_engine.slowest_build_ms + 10.f) {
      g_engine.slowest_build_ms = info.build_ms;
      lg::info("[skate3] slowest world build so far: {:.0f} ms for {} triangles", info.build_ms,
               info.triangles);
    }
  }
  SkScore score;
  memset(&score, 0, sizeof(score));
  if (g_engine.score(g_engine.session, &score) == 0) {
    score.trick[sizeof(score.trick) - 1] = 0;
    if (io->trick_name && score.tricks != io->tricks) {
      readable_trick_name(score.trick, Ptr<String>(io->trick_name).c()->data(), 64);
    }
    io->tricks = score.tricks;
    io->sequence_active = score.sequence_active;
    io->sequence = score.sequence;
    io->multiplier = score.multiplier;
    io->total = score.total;
  }
  io->status = kOk;
}

void do_activate(BridgeIO* io) {
  if (!load_library()) {
    io->status = kUnavailable;
    return;
  }
  // Ride the level collision gathered by the host; with none, lay flat ground instead.
  float flat[18];
  const float* ground = flat;
  u32 ground_count = 2;
  if (g_engine.world.empty()) {
    build_flat_ground(io->position, flat);
  } else {
    orient_world();
    ground = g_engine.world.data();
    ground_count = (u32)(g_engine.world.size() / 9);
  }

  // A different world scale needs a new session: the engine's unit is fixed when it starts.
  const float world_scale =
      (io->world_scale > 0.5f && io->world_scale < 3.f) ? io->world_scale : 1.f;
  if (g_engine.session && std::abs(world_scale - g_engine.world_scale) > 0.001f) {
    lg::info("[skate3] world scale changed from {:.2f} to {:.2f}: restarting the engine",
             g_engine.world_scale, world_scale);
    g_engine.session_free(g_engine.session);
    g_engine.session = nullptr;
    g_engine.rig_count = 0;
    g_engine.board_ready = false;
  }

  if (!g_engine.session) {
    const auto assets = env("OPENGOAL_SKATE3_ASSETS");
    if (assets.empty()) {
      lg::warn("[skate3] OPENGOAL_SKATE3_ASSETS is not set; the Skate 3 bridge is unavailable");
      io->status = kUnavailable;
      return;
    }
    g_engine.unit_scale =
        (io->unit_scale > 0.5f && io->unit_scale < 3.f) ? io->unit_scale : 1.f;
    g_engine.world_scale = world_scale;
    g_engine.units_per_meter = kUnitsPerMeter * g_engine.unit_scale * g_engine.world_scale;
    lg::info("[skate3] starting the engine at character scale {:.3f}, world scale {:.2f} (this "
             "blocks for several seconds)",
             g_engine.unit_scale, g_engine.world_scale);
    g_engine.session =
        g_engine.session_new(assets.c_str(), ground, ground_count, nullptr, nullptr, 0,
                             1.f / g_engine.units_per_meter);
    if (!g_engine.session) {
      lg::error("[skate3] the engine did not start: {}", engine_error());
      io->status = kEngineError;
      return;
    }
    // Starting a session does not look for grind rails in its collision; installing the
    // same world does.
    if (!g_engine.world.empty() &&
        g_engine.install_world(g_engine.session, ground, ground_count) < 0) {
      lg::warn("[skate3] installing the first world failed: {}", engine_error());
    }
  } else if (g_engine.install_world(g_engine.session, ground, ground_count) < 0) {
    lg::error("[skate3] installing the ground failed: {}", engine_error());
    io->status = kEngineError;
    return;
  }

  float host_position[3];
  float host_forward[3];
  game_to_host(io->position, host_position);
  game_to_host(io->forward, host_forward);
  // The engine's yaw is counter-clockwise from host +X about host +Z.
  const float yaw = std::atan2(host_forward[1], host_forward[0]);
  if (g_engine.activate(g_engine.session, host_position, yaw) != 0) {
    lg::error("[skate3] activate failed: {}", engine_error());
    io->status = kEngineError;
    g_engine.active = false;
    return;
  }
  const float length =
      std::sqrt(io->forward[0] * io->forward[0] + io->forward[2] * io->forward[2]);
  if (length > 0.001f) {
    g_engine.last_forward[0] = io->forward[0] / length;
    g_engine.last_forward[2] = io->forward[2] / length;
  }
  g_engine.active = true;
  g_engine.time_bank = 0.f;
  const float period = g_engine.step_period(g_engine.session);
  if (period > 0.001f && period < 0.1f) {
    g_engine.period = period;
  }
  write_outputs(io);
}

void vec_sub(const float* a, const float* b, float* out) {
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
  out[2] = a[2] - b[2];
}

void vec_cross(const float* a, const float* b, float* out) {
  const float r[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                      a[0] * b[1] - a[1] * b[0]};
  memcpy(out, r, sizeof(r));
}

float vec_dot(const float* a, const float* b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

bool vec_normalize(float* v) {
  const float length = std::sqrt(vec_dot(v, v));
  if (length < 1e-9f) {
    return false;
  }
  v[0] /= length;
  v[1] /= length;
  v[2] /= length;
  return true;
}

/*!
 * A rigid frame from three points: origin midway between a and b, first axis along a->b,
 * third axis normal to the plane of the three. Column-major. false if they are collinear.
 */
bool frame_from_points(const float* a, const float* b, const float* c, float* out) {
  float x[3], ac[3], n[3], y[3];
  vec_sub(b, a, x);
  vec_sub(c, a, ac);
  if (!vec_normalize(x)) {
    return false;
  }
  vec_cross(x, ac, n);
  if (!vec_normalize(n)) {
    return false;
  }
  vec_cross(n, x, y);
  memset(out, 0, 16 * sizeof(float));
  for (int i = 0; i < 3; i++) {
    out[0 + i] = x[i];
    out[4 + i] = y[i];
    out[8 + i] = n[i];
    out[12 + i] = 0.5f * (a[i] + b[i]);
  }
  out[15] = 1.f;
  return true;
}

bool read_board_vertices() {
  const u32 count = g_engine.board_vertex_count;
  return count > 0 && g_engine.board_pose(g_engine.session, g_engine.board_vertices.data(),
                                          count) == (int)count;
}

void set_board_visible(bool visible) {
  auto& mesh = skate_board_mesh();
  std::lock_guard<std::mutex> lock(mesh.mutex);
  mesh.visible = visible;
}

/*!
 * Hand the renderer the board's fixed data: triangles, texture coordinates and textures.
 */
void publish_board_model(const SkBoardSurface* parts, int surfaces) {
  auto& mesh = skate_board_mesh();
  std::lock_guard<std::mutex> lock(mesh.mutex);
  mesh.surfaces.clear();
  for (int i = 0; i < surfaces; i++) {
    const auto& in = parts[i];
    if (!in.indices || !in.uvs || !in.rgba || in.width == 0 || in.height == 0) {
      continue;
    }
    SkateBoardMesh::Surface out;
    out.first_vertex = in.first_vertex;
    out.vertex_count = in.vertex_count;
    out.indices.assign(in.indices, in.indices + in.index_count);
    out.uvs.assign(in.uvs, in.uvs + (size_t)in.vertex_count * 2);
    out.rgba.assign(in.rgba, in.rgba + (size_t)in.width * in.height * 4);
    out.width = in.width;
    out.height = in.height;
    mesh.surfaces.push_back(std::move(out));
  }
  mesh.model_version++;
}

/*!
 * Hand the renderer the board's vertices for the engine's latest step, in game space.
 * Expects read_board_vertices to have just succeeded.
 */
void publish_board_vertices() {
  auto& mesh = skate_board_mesh();
  std::lock_guard<std::mutex> lock(mesh.mutex);
  const size_t count = g_engine.board_vertex_count;
  mesh.vertices.resize(count * 6);
  const float* in = g_engine.board_vertices.data();
  for (size_t i = 0; i < count; i++) {
    host_to_game(&in[i * 6], &mesh.vertices[i * 6]);
    host_to_game(&in[i * 6 + 3], &mesh.vertices[i * 6 + 3]);
  }
  mesh.visible = true;
}

/*!
 * Called with the skater just placed, so the board is upright and at rest. Picks the three
 * reference vertices and works out how the frame they span sits relative to the board.
 */
void setup_board() {
  g_engine.board_ready = false;
  SkBoardSurface parts[16];
  const int surfaces = g_engine.board_mesh(g_engine.session, parts, 16);
  if (surfaces <= 0) {
    lg::warn("[skate3] no skateboard model: {}", engine_error());
    return;
  }
  u32 total = 0;
  for (int i = 0; i < surfaces && i < 16; i++) {
    total = std::max(total, parts[i].first_vertex + parts[i].vertex_count);
  }
  if (total < 3 || total > 100000) {
    return;
  }
  g_engine.board_vertex_count = total;
  g_engine.board_vertices.assign((size_t)total * 6, 0.f);
  if (!read_board_vertices()) {
    lg::warn("[skate3] could not read the skateboard pose: {}", engine_error());
    return;
  }
  const float* v = g_engine.board_vertices.data();
  auto pos = [&](u32 i) { return &v[(size_t)i * 6]; };
  auto normal = [&](u32 i) { return &v[(size_t)i * 6 + 3]; };

  // Nose and tail: the two vertices furthest apart.
  u32 a = 0, b = 0;
  float best = -1.f;
  for (u32 i = 0; i < total; i++) {
    for (u32 j = i + 1; j < total; j++) {
      float d[3];
      vec_sub(pos(i), pos(j), d);
      const float dist = vec_dot(d, d);
      if (dist > best) {
        best = dist;
        a = i;
        b = j;
      }
    }
  }
  // A deck-edge vertex: on the top surface (facing up, at the usual top height, which
  // rules out the wheels and trucks below) and furthest from the nose-tail line.
  std::vector<float> top_heights;
  for (u32 i = 0; i < total; i++) {
    if (normal(i)[2] > 0.9f) {
      top_heights.push_back(pos(i)[2]);
    }
  }
  if (top_heights.size() < 3) {
    return;
  }
  std::nth_element(top_heights.begin(), top_heights.begin() + top_heights.size() / 2,
                   top_heights.end());
  const float top = top_heights[top_heights.size() / 2];
  const float tolerance = 0.01f * g_engine.units_per_meter;
  float axis[3];
  vec_sub(pos(b), pos(a), axis);
  vec_normalize(axis);
  u32 edge = a;
  best = -1.f;
  for (u32 i = 0; i < total; i++) {
    if (normal(i)[2] <= 0.9f || std::abs(pos(i)[2] - top) > tolerance) {
      continue;
    }
    float d[3], off[3];
    vec_sub(pos(i), pos(a), d);
    vec_cross(axis, d, off);
    const float dist = vec_dot(off, off);
    if (dist > best) {
      best = dist;
      edge = i;
    }
  }
  float frame[16];
  if (best <= 0.f || !frame_from_points(pos(a), pos(b), pos(edge), frame)) {
    return;
  }
  g_engine.board_ref[0] = a;
  g_engine.board_ref[1] = b;
  g_engine.board_ref[2] = edge;
  publish_board_model(parts, std::min(surfaces, 16));

  // The board's own frame at rest, in host space: Z up, X along the deck the way the
  // skater faces, at the centre of the deck's top.
  SkPose pose;
  memset(&pose, 0, sizeof(pose));
  g_engine.get_pose(g_engine.session, &pose);
  const float facing[3] = {-pose.root[4], -pose.root[5], 0.f};
  float forward[3] = {frame[0], frame[1], 0.f};
  if (!vec_normalize(forward)) {
    return;
  }
  if (vec_dot(forward, facing) < 0.f) {
    forward[0] = -forward[0];
    forward[1] = -forward[1];
  }
  const float up[3] = {0.f, 0.f, 1.f};
  float side[3];
  vec_cross(up, forward, side);
  float rest[16];
  memset(rest, 0, sizeof(rest));
  for (int i = 0; i < 3; i++) {
    rest[0 + i] = forward[i];
    rest[4 + i] = side[i];
    rest[8 + i] = up[i];
  }
  rest[12] = frame[12];
  rest[13] = frame[13];
  rest[14] = top;
  rest[15] = 1.f;
  float inverse[16];
  rigid_inverse(frame, inverse);
  mat_mul(inverse, rest, g_engine.board_fix);
  g_engine.board_ready = true;
  float length[3];
  vec_sub(pos(b), pos(a), length);
  lg::info("[skate3] skateboard: {} vertices in {} parts, {:.2f} m long", total, surfaces,
           std::sqrt(vec_dot(length, length)) / g_engine.units_per_meter);
}

/*!
 * The skateboard's transform for the engine's latest step as a game-space matrix whose
 * rows are right, up, forward and position, i.e. laid out like a GOAL matrix.
 */
bool board_matrix(float* out) {
  if (!g_engine.board_ready || !read_board_vertices()) {
    return false;
  }
  publish_board_vertices();
  const float* v = g_engine.board_vertices.data();
  float frame[16];
  if (!frame_from_points(&v[(size_t)g_engine.board_ref[0] * 6],
                         &v[(size_t)g_engine.board_ref[1] * 6],
                         &v[(size_t)g_engine.board_ref[2] * 6], frame)) {
    return false;
  }
  float board[16];
  mat_mul(frame, g_engine.board_fix, board);
  float forward[3], side[3], up[3], right[3];
  host_to_game(&board[0], forward);
  host_to_game(&board[4], side);
  host_to_game(&board[8], up);
  vec_cross(up, forward, right);
  memset(out, 0, 16 * sizeof(float));
  for (int i = 0; i < 3; i++) {
    out[0 + i] = right[i];
    out[4 + i] = up[i];
    out[8 + i] = forward[i];
  }
  host_to_game(&board[12], &out[12]);
  out[15] = 1.f;
  return true;
}

const RigMapEntry* find_rig_map(const char* joint) {
  for (const auto& entry : kRigMap) {
    if (strcmp(entry.joint, joint) == 0) {
      return &entry;
    }
  }
  return nullptr;
}

void do_rig_setup(BridgeIO* io) {
  g_engine.rig_count = 0;
  if (!g_engine.session || !g_engine.active) {
    io->status = kNotActive;
    return;
  }
  if (!io->rig_bones || io->rig_count == 0 || io->rig_count > kMaxRigBones) {
    io->status = kBadCommand;
    return;
  }
  const auto* in = Ptr<RigBone>(io->rig_bones).c();
  const u32 count = io->rig_count;
  std::vector<const char*> names(count, "");
  for (u32 i = 0; i < count; i++) {
    if (in[i].name) {
      names[i] = Ptr<String>(in[i].name).c()->data();
    }
  }
  auto index_of = [&](const char* name) -> s32 {
    for (u32 i = 0; name && i < count; i++) {
      if (strcmp(names[i], name) == 0) {
        return (s32)i;
      }
    }
    return -1;
  };

  std::vector<SkRigBone> bones(count);
  u32 mapped = 0;
  for (u32 i = 0; i < count; i++) {
    auto& out = bones[i];
    memset(&out, 0, sizeof(out));
    out.parent = (in[i].parent >= 0 && (u32)in[i].parent < i) ? in[i].parent : -1;
    out.child = -1;
    if (const auto* map = find_rig_map(names[i])) {
      out.skate = map->skate;
      out.skate_child = map->skate_child;
      out.child = index_of(map->child);
      mapped++;
    }
    // Model-space bind pose, at the size the character is drawn, turned into host axes.
    float bind[16];
    if (!rigid_inverse(in[i].inv_bind, bind)) {
      memset(bind, 0, sizeof(bind));
      bind[0] = bind[5] = bind[10] = bind[15] = 1.f;
    }
    for (int a = 0; a < 3; a++) {
      bind[12 + a] *= g_engine.unit_scale;
    }
    mat_mul(kGameToHost, bind, bind);
    mat_mul(bind, kHostToGame, out.bind);
  }
  // NaN asks the engine to measure which way the host model faces from both skeletons.
  const int result = g_engine.rig_setup(g_engine.session, bones.data(), count, std::nanf(""));
  if (result != (int)count) {
    lg::error("[skate3] rig setup failed ({} of {} bones, {} mapped): {}", result, count, mapped,
              engine_error());
    io->status = kEngineError;
    return;
  }
  lg::info("[skate3] rig set up: {} bones, {} following the skater", count, mapped);
  g_engine.rig_count = count;
  g_engine.board_joint = index_of(kBoardJoint);
  // The engine works out how the board is carried while posing the rig, so pose it once
  // before reading the board.
  float scratch[kMaxRigBones * 16];
  g_engine.rig_pose(g_engine.session, scratch, count);
  setup_board();
  io->status = kOk;
}

void do_rig_pose(BridgeIO* io) {
  if (!g_engine.session || !g_engine.active || g_engine.rig_count == 0) {
    io->status = kNotActive;
    return;
  }
  if (!io->rig_pose || io->rig_count != g_engine.rig_count) {
    io->status = kBadCommand;
    return;
  }
  float host[kMaxRigBones * 16];
  const u32 count = g_engine.rig_count;
  if (g_engine.rig_pose(g_engine.session, host, count) != (int)count) {
    lg::error("[skate3] rig pose failed: {}", engine_error());
    io->status = kEngineError;
    return;
  }
  auto* out = Ptr<float>(io->rig_pose).c();
  for (u32 i = 0; i < count; i++) {
    float* m = &out[i * 16];
    mat_mul(kHostToGame, &host[i * 16], m);
    mat_mul(m, kGameToHost, m);
    // The game draws the character larger than its model: put that scale back.
    for (int a = 0; a < 12; a++) {
      if ((a & 3) != 3) {
        m[a] *= g_engine.unit_scale;
      }
    }
  }
  // Jak's board joint carries the jetboard model: put it where the skateboard is.
  if (g_engine.board_joint >= 0 && (u32)g_engine.board_joint < count) {
    float board[16];
    if (board_matrix(board)) {
      memcpy(&out[(size_t)g_engine.board_joint * 16], board, sizeof(board));
    }
  }
  io->status = kOk;
}

void do_debug_rail(BridgeIO* io) {
  if (!g_engine.session || !g_engine.debug_rail || !g_engine.debug_rail_count) {
    io->status = kUnavailable;
    return;
  }
  const int rails = g_engine.debug_rail_count();
  io->world_rails = rails > 0 ? (u32)rails : 0;
  float points[64 * 3];
  const int count = g_engine.debug_rail(io->world_count, points, 64);
  if (count < 2) {
    io->status = kBadCommand;
    return;
  }
  // the longest straight piece of the rail
  int best = 0;
  float best_length = -1.f;
  for (int i = 0; i + 1 < std::min(count, 64); i++) {
    float d[3];
    vec_sub(&points[(i + 1) * 3], &points[i * 3], d);
    const float length = vec_dot(d, d);
    if (length > best_length) {
      best_length = length;
      best = i;
    }
  }
  host_to_game(&points[best * 3], io->position);
  io->position[3] = 1.f;
  host_to_game(&points[(best + 1) * 3], io->velocity);
  io->velocity[3] = 1.f;
  io->status = kOk;
}

void do_set_velocity(BridgeIO* io) {
  if (!g_engine.session || !g_engine.active) {
    io->status = kNotActive;
    return;
  }
  float host[3];
  game_to_host(io->velocity, host);
  if (g_engine.set_velocity(g_engine.session, host) != 0) {
    lg::error("[skate3] setting the velocity failed: {}", engine_error());
    io->status = kEngineError;
    return;
  }
  io->status = kOk;
}

s16 stick(float value) {
  return (s16)(std::clamp(value, -1.f, 1.f) * 32767.f);
}

u8 trigger(float value) {
  return (u8)(std::clamp(value, 0.f, 1.f) * 255.f);
}

void do_step(BridgeIO* io) {
  if (!g_engine.session || !g_engine.active) {
    io->status = kNotActive;
    return;
  }
  SkControls controls;
  memset(&controls, 0, sizeof(controls));
  controls.buttons = (u16)io->buttons;
  controls.triggers[0] = trigger(io->left_trigger);
  controls.triggers[1] = trigger(io->right_trigger);
  controls.left[0] = stick(io->left_x);
  controls.left[1] = stick(io->left_y);
  controls.right[0] = stick(io->right_x);
  controls.right[1] = stick(io->right_y);
  // The engine runs at a fixed rate; turn the game's frame time into whole steps.
  int steps = 1;
  if (io->dt > 0.f) {
    g_engine.time_bank += std::min(io->dt, 0.1f);
    steps = 0;
    while (g_engine.time_bank + 1e-5f >= g_engine.period && steps < kMaxStepsPerFrame) {
      g_engine.time_bank -= g_engine.period;
      steps++;
    }
    if (steps == kMaxStepsPerFrame) {
      g_engine.time_bank = 0.f;
    }
  }
  for (int i = 0; i < steps; i++) {
    if (g_engine.step(g_engine.session, &controls) < 0) {
      lg::error("[skate3] step failed: {}", engine_error());
      io->status = kEngineError;
      g_engine.active = false;
      return;
    }
  }
  io->steps = (u32)steps;
  write_outputs(io);
}
#endif  // _WIN32

}  // namespace

void shutdown() {
#ifdef _WIN32
  set_board_visible(false);
  if (g_engine.session && g_engine.session_free) {
    g_engine.session_free(g_engine.session);
  }
  g_engine.session = nullptr;
  g_engine.active = false;
  g_engine.rig_count = 0;
  g_engine.board_ready = false;
#endif
}

s64 pc_skate3_bridge(u32 io_ptr) {
  if (!io_ptr) {
    return kBadCommand;
  }
  auto* io = Ptr<BridgeIO>(io_ptr).c();
#ifdef _WIN32
  switch (io->command) {
    case kStep:
      do_step(io);
      break;
    case kActivate:
      do_activate(io);
      break;
    case kShutdown:
      shutdown();
      io->status = kOk;
      break;
    case kWorldBegin:
      do_world_begin(io);
      break;
    case kWorldAppend:
      do_world_append(io);
      break;
    case kWorldCommit:
      do_world_commit(io);
      break;
    case kRigSetup:
      do_rig_setup(io);
      break;
    case kRigPose:
      do_rig_pose(io);
      break;
    case kQuery:
      io->status = (env("OPENGOAL_SKATE3_DLL").empty() || env("OPENGOAL_SKATE3_ASSETS").empty())
                       ? kUnavailable
                       : kOk;
      break;
    case kDebugRail:
      do_debug_rail(io);
      break;
    case kSetVelocity:
      do_set_velocity(io);
      break;
    case kRelease:
      set_board_visible(false);
      g_engine.active = false;
      g_engine.rig_count = 0;
      io->status = kOk;
      break;
    default:
      io->status = kBadCommand;
      break;
  }
#else
  io->status = kUnavailable;
#endif
  return io->status;
}

}  // namespace skate3_bridge
}  // namespace jak3
