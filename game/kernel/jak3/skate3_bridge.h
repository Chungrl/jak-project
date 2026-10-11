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
  kStep = 0,      // advance the engine by `dt` with the given pad state
  kActivate = 1,  // start the engine if needed and put the skater at `position`
  kShutdown = 2,  // free the engine session
  // Level collision. The host gathers triangles around the skater between a begin and a
  // commit; a commit hands them to the engine, which rebuilds its world in the background.
  kWorldBegin = 3,   // discard any triangles gathered so far
  kWorldAppend = 4,  // add `world_count` collide-cache triangles starting at `world_tris`
  kWorldCommit = 5,  // queue the gathered triangles as the engine's world
  // Skater pose. Setup describes the host character's skeleton once; after that each pose
  // request writes one world matrix per host bone for the engine's latest step.
  kRigSetup = 6,  // `rig_count` skate3-rig-bone entries at `rig_bones`
  kRigPose = 7,   // write `rig_count` matrices to `rig_pose`
  kRelease = 8,   // stop driving the character; the session is kept for the next activate
  kQuery = 9,     // status kOk if the bridge is configured (both environment variables set)
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
 * An activate uses the triangles gathered so far as the world; with none gathered it lays
 * flat ground at the activation height instead.
 *
 * unit_scale: the host character is drawn larger than its model (Jak's skeleton is scaled by
 * about 1.39, and his world is built to match). The engine is told that one of its metres is
 * 4096 * unit_scale game units, so the skater, board and speeds are in proportion to him.
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
  u32 world_tris;      // in: address of an inline array of collide-cache-tri
  u32 world_count;     // in: triangles at world_tris
  u32 world_triangles; // out: triangles in the engine's installed world
  u32 world_rails;     // out: grind rails the engine found in them
  u32 rig_bones;       // in: address of an inline array of skate3-rig-bone
  u32 rig_count;       // in: host bones in the rig
  u32 rig_pose;        // in: address of an inline array of matrix to fill
  float unit_scale;    // in: size of the host character relative to its model (see below)
  float dt;            // in: seconds of game time for kStep; 0 or less runs exactly one step
  u32 steps;           // out: fixed engine steps the last kStep ran (0 is normal on fast frames)
  u32 trick_name;      // in: GOAL string of at least 64 characters, filled with the last trick
  u32 tricks;          // out: tricks announced so far; a change means trick_name is new
  s32 sequence_active; // out: nonzero while a run of tricks is being scored
  float sequence;      // out: points of the running sequence, multiplier included
  float multiplier;    // out
  float total;         // out: points of every sequence landed this session
};
static_assert(sizeof(BridgeIO) == 176, "skate3-bridge-io layout");

/*!
 * Mirrors the GOAL type skate3-rig-bone: one host bone for kRigSetup, parents before children.
 */
struct RigBone {
  float inv_bind[16];  // the joint's inverse bind matrix, in model space
  u32 name;            // GOAL string, or 0
  s32 parent;          // index earlier in the array, or -1
  u32 pad[2];
};
static_assert(sizeof(RigBone) == 80, "skate3-rig-bone layout");

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
