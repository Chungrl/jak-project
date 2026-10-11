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

#include "common/log/log.h"

#include "game/kernel/common/Ptr.h"

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

struct SkSession;

constexpr float kMetersPerUnit = 1.f / 4096.f;
// Half-size of the flat ground laid under the skater on activation, in game units (500 m).
constexpr float kFlatGroundExtent = 500.f * 4096.f;

#ifdef _WIN32
struct Engine {
  HMODULE library = nullptr;
  SkSession* session = nullptr;
  bool load_attempted = false;
  bool active = false;
  float last_forward[3] = {0.f, 0.f, 1.f};

  SkSession*(__cdecl* session_new)(const char*,
                                   const float*,
                                   u32,
                                   const float*,
                                   const u32*,
                                   u32,
                                   float) = nullptr;
  void(__cdecl* session_free)(SkSession*) = nullptr;
  int(__cdecl* install_world)(SkSession*, const float*, u32) = nullptr;
  int(__cdecl* activate)(SkSession*, const float*, float) = nullptr;
  int(__cdecl* step)(SkSession*, const SkControls*) = nullptr;
  int(__cdecl* get_pose)(SkSession*, SkPose*) = nullptr;
  int(__cdecl* feedback)(SkSession*, SkFeedback*) = nullptr;
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
  ok = resolve(g_engine.activate, "sk_activate") && ok;
  ok = resolve(g_engine.step, "sk_step") && ok;
  ok = resolve(g_engine.get_pose, "sk_get_pose") && ok;
  ok = resolve(g_engine.feedback, "sk_feedback") && ok;
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

  // Facing follows travel once the board is moving; below that, keep the last direction.
  const float vx = io->velocity[0];
  const float vz = io->velocity[2];
  const float speed = std::sqrt(vx * vx + vz * vz);
  if (speed > 0.25f * 4096.f) {
    g_engine.last_forward[0] = vx / speed;
    g_engine.last_forward[1] = 0.f;
    g_engine.last_forward[2] = vz / speed;
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
  io->status = kOk;
}

void do_activate(BridgeIO* io) {
  if (!load_library()) {
    io->status = kUnavailable;
    return;
  }
  float ground[18];
  build_flat_ground(io->position, ground);

  if (!g_engine.session) {
    const auto assets = env("OPENGOAL_SKATE3_ASSETS");
    if (assets.empty()) {
      lg::warn("[skate3] OPENGOAL_SKATE3_ASSETS is not set; the Skate 3 bridge is unavailable");
      io->status = kUnavailable;
      return;
    }
    lg::info("[skate3] starting the engine (this blocks for several seconds)");
    g_engine.session =
        g_engine.session_new(assets.c_str(), ground, 2, nullptr, nullptr, 0, kMetersPerUnit);
    if (!g_engine.session) {
      lg::error("[skate3] the engine did not start: {}", engine_error());
      io->status = kEngineError;
      return;
    }
  } else if (g_engine.install_world(g_engine.session, ground, 2) < 0) {
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
  write_outputs(io);
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
  if (g_engine.step(g_engine.session, &controls) < 0) {
    lg::error("[skate3] step failed: {}", engine_error());
    io->status = kEngineError;
    g_engine.active = false;
    return;
  }
  write_outputs(io);
}
#endif  // _WIN32

}  // namespace

void shutdown() {
#ifdef _WIN32
  if (g_engine.session && g_engine.session_free) {
    g_engine.session_free(g_engine.session);
  }
  g_engine.session = nullptr;
  g_engine.active = false;
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
