#pragma once

/*!
 * @file skate3_bridge.h
 * (AI-assisted) Optional bridge from GOAL to an external Skate 3 gameplay engine.
 *
 * The engine is not part of this repository and is never linked into gk. It is a separate
 * library ("skate_ffi", a C interface over the Skate 3 Rust engine) that the player builds
 * and points to with environment variables, together with data converted from their own
 * copy of Skate 3:
 *   OPENGOAL_SKATE3_DLL     full path to skate_ffi.dll
 *   OPENGOAL_SKATE3_ASSETS  full path to the converted "assets" folder
 * With either unset, every call reports an error status and the game is unaffected.
 */

#include "common/common_types.h"

namespace jak3 {
namespace skate3_bridge {

enum Command : s32 {
  kStep = 0,      // advance one fixed engine step with the given pad state
  kActivate = 1,  // start the engine if needed and put the skater at `position`
  kShutdown = 2,  // free the engine session
};

enum Status : s32 {
  kOk = 0,
  kUnavailable = -1,   // not configured, unsupported platform, or the library failed to load
  kEngineError = -2,   // the engine reported an error; see the gk log
  kNotActive = -3,     // step requested before a successful activate
  kBadCommand = -4,
};

/*!
 * Mirrors the GOAL type skate3-bridge-io (goal_src/jak3/engine/target/board/skate3-bridge.gc).
 * Positions and velocities are in game units (4096 per metre), game axes (Y up).
 */
struct BridgeIO {
  float position[4];  // in: activation position. out: skater root
  float velocity[4];  // out: board velocity, units per second
  float forward[4];   // in: activation facing. out: unit travel direction, flattened to XZ
  s32 command;        // in: Command
  s32 status;         // out: Status
  u32 buttons;        // in: XInput button bits
  float left_x;       // in: sticks, -1..1, up and right positive
  float left_y;
  float right_x;
  float right_y;
  float left_trigger;  // in: 0..1
  float right_trigger;
  u32 state;           // out: Skate physical state (100s riding, 200s air, 300 bail, 400s grind)
  u32 wheels;          // out: wheels on the ground
  u32 pops;            // out: counters, only ever increase
  u32 landings;
  u32 grinds;
  u32 bails;
  u32 tick;            // out: engine steps since the session started
};
static_assert(sizeof(BridgeIO) == 112, "skate3-bridge-io layout");

/*!
 * GOAL entry point: (pc-skate3-bridge io). Returns the status also written to io.
 */
s64 pc_skate3_bridge(u32 io_ptr);

/*!
 * Free the engine session, if any. Safe to call at shutdown.
 */
void shutdown();

}  // namespace skate3_bridge
}  // namespace jak3
