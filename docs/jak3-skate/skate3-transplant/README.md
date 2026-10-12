# Skate 3 transplantation research (AI-assisted)

Recorded 2026-10-11. Research only: no implementation was changed, nothing from
the audited repositories was built, run, or copied into this tree.

Goal under assessment: bring Skate 3's Flickit controls, trick recognition,
physics and animation into Jak 3 by reusing existing Skate 3 technology rather
than designing a bespoke system. **Do not repeat these audits**; extend them
only where a specific question below is still open.

## How the audits were run

Each repository was shallow-cloned, then audited by one Codex CLI agent using
the prompt in [codex-audits/AUDIT_PROMPT.md](codex-audits/AUDIT_PROMPT.md):

```sh
codex exec -s read-only --skip-git-repo-check --ephemeral --color never \
  -C <clone-dir> -o <report>.md - < audit-prompt.md
```

No model or effort flag was passed, so all five used the `~/.codex/config.toml`
defaults: **`gpt-6-sol`, reasoning effort `medium`**. All five exited 0 and
produced a report.

**Limitation:** in every run Codex's local command runner failed
(`CreateProcess ... helper_unknown_error: setup refresh had errors`), so the
agents could not read the local clones. They audited the public GitHub source
instead, and each report says so. Line references are to the public default
branches at these commits, which matched the local clones' HEADs:

| Report | Repository | Commit (date) |
| --- | --- | --- |
| [Faiqie_BullySkate.md](codex-audits/Faiqie_BullySkate.md) | Faiqie/BullySkate | `761c8de` (2026-10-04) |
| [SK8-ENGINE_skate-3-rust-engine.md](codex-audits/SK8-ENGINE_skate-3-rust-engine.md) | SK8-ENGINE/skate-3-rust-engine | `2e16697` (2026-10-09) |
| [andrewnakas_SK8-Engine.md](codex-audits/andrewnakas_SK8-Engine.md) | andrewnakas/SK8-Engine | `f6e0ae8` (2026-07-24) |
| [tvllerr_SK8-Engine.md](codex-audits/tvllerr_SK8-Engine.md) | tvllerr/SK8-Engine | `311cd40` (2026-08-28) |
| [chasmlol_skate3clone.md](codex-audits/chasmlol_skate3clone.md) | chasmlol/skate3clone | `32ba206` (2026-09-03) |

## Verified locally afterwards (by the lead agent, in the clones)

- Licences: skate-3-rust-engine is GPL-3.0; BullySkate is MIT for its own
  additions only and states its vendored engine copy (pinned at `60efdef`) came
  with no licence; tvllerr is MIT for project-owned additions only; andrewnakas
  has no licence file; chasmlol's README says no licence has been granted.
- Sizes (lines of Rust/C/C++): `skate-core` about 93,000 with no crate
  dependencies and `#![forbid(unsafe_code)]`; `skate-game` (Bevy) about
  121,000; BullySkate `skate-runtime` about 60,000, native adapter about 1,800.
- BullySkate worker: `src/skate_worker.rs`, 161 lines, one fixed 6,184-byte
  shared-memory struct per frame (pad input in; board pose, 36-bone skater
  pose, camera, state out), driven by `SkateHost::tick(XboxState)`.
  `SkateHost::flat(root)` builds a flat-ground world.
- World interface: `ExternalQueries` trait in
  `skate-core/src/physics/board_world.rs` (`line` sweep and `nearby` triangles).
- Flickit recogniser: `skate-core/src/input/gesture.rs`, 248 lines, no
  dependencies, needs the game's `.pat` pattern files.
- No repository ships Skate 3 data. The Rust engine needs 18 files extracted
  locally from an Xbox 360 copy (`extractor/extract.py` in BullySkate).
- This machine has no Rust toolchain; OpenGOAL has no Rust build integration.

Not verified by anyone: that any of these projects builds or plays, and the
per-subsystem provenance classifications beyond the cited source comments.

## Findings in brief

| Repository | Gameplay provenance | Reuse for Jak 3 |
| --- | --- | --- |
| SK8-ENGINE/skate-3-rust-engine | Hand-ported from retail disassembly; parity explicitly unverified upstream | The engine to transplant |
| Faiqie/BullySkate | Same engine adapted to a host game through a worker process | Integration blueprint |
| andrewnakas/SK8-Engine | Retail code recompiled from the user's executable | Research only; gameplay not separable |
| tvllerr/SK8-Engine | Same recompiled retail code plus a custom world | Research only; gameplay not separable |
| chasmlol/skate3clone | Partial reverse-engineering, some fitted behaviour, no bails | Research only; no licence |

## Route ranking (provisional, pending the open decisions)

1. **A. Rust engine as a separate worker process** (BullySkate pattern):
   highest achievable fidelity without retail code, highest reuse.
2. **C. Selected modules** (for example the Flickit recogniser) feeding the
   recovered GOAL implementation: fastest, partial fidelity, a subset of A.
3. **D. Bespoke GOAL implementation**: fallback only.
4. **B. Recompiled retail runtime**: highest theoretical fidelity, but the
   gameplay cannot be separated from the emulated runtime and Skate 3's world.

Proposed first demonstration: the worker running `SkateHost::flat`, a small C++
bridge in `game/kernel/jak3` exposing shared-memory send/receive to GOAL, and
the board state forwarding raw pad input and applying the returned board
transform (metres to Jak units at 4096 per metre; both Y-up; both 60 Hz).

## Engine runs headless on real data (2026-10-11, evening)

A Skate 3 Xbox 360 disc image became available. Everything below lives in
`C:\Users\Alan\Projects\skate3-work`, outside this repository; no Skate 3 data
or GPL code is in the tree.

- BullySkate's `extractor/extract.py` extracted and checksum-verified all 18
  required files ("Extraction verified: 18 required files").
- BullySkate `761c8de` `skate-runtime` examples built in WSL Ubuntu 24.04
  (`cargo build --release --example flick_probe --example probe`, exit 0) and
  ran against that data on `SkateHost::flat`:
  - `flick_probe` (right stick down 9 ticks, up 5 ticks): states
    `PhysicsGround` -> `KnownAir` -> `PhysicsGround`, clips `B_ANTIC_INTO`,
    `B_OLLIE_G`, `B_OLLIE_A`, `B_AIR_CYC`, `BLEND_LAND`; deck height range
    0.031 to 1.017 m; exit 0.
  - `probe` (push button held ticks 120-419): clips `B_MONGO_PUSH_INTO`,
    `B_MONGO_PUSH_CYC2`; board travelled about 48.5 m; 600 ticks in 565 ms
    (under 1 ms per 60 Hz tick); exit 0.
- The Windows GNU Rust toolchain cannot build this dependency tree (its bundled
  `dlltool` needs an assembler that is not shipped). Building natively on
  Windows needs Visual Studio Build Tools; WSL works today.

Not yet shown: anything connected to Jak 3, real level collision, grinds,
manuals, bails, or the 36-bone pose applied to any character.

## Other host-game mods and a ready C interface (2026-10-11, night)

Web research (READMEs only, authors' own claims) found the same engine hosted
in GTA V (`user08274/gtav-skate3-rs`), GTA San Andreas
(`ryglizzy/GTA-San-AnSkateas`, and GTSkate on Nexus), Minecraft
(`tomTTFB/mineskate3`), Garry's Mod (`the-schwilliam/SkateGM`,
`NotEnxvinity/skate3-gmod`), WoW 1.12 (`Kimmo3223/world-of-skatecraft`) and
`chasmlol/2010-rust-rewrite-mashup`. Most run the engine in-process; BullySkate's
separate worker is the exception. Hosts feed collision live (GTA V by host
raycasts, Minecraft by triangles around the skater).

Read locally in `skate3-work/repos` (mashup `f608f85`, San Andreas `fde499b`):

- The mashup carries the engine in `skate/` (`skate-core` about 84,000 lines,
  `skate-host` about 55,000, Bevy 0.19 math only) under the repository's
  **Apache-2.0** LICENSE; its NOTICE describes `skate/` as the fork author's own
  project. This is a different snapshot from the GPL-3.0 SK8-ENGINE repository.
- `GTA-San-AnSkateas/skate-ffi` (about 2,000 lines, `cdylib`) wraps
  `skate_host::bridge::Session` in a C interface with a header
  (`include/skate_ffi.h`): `sk_session_new` (triangles, rails,
  `meters_per_unit`), `sk_install_world` / `sk_queue_world` (replace collision
  as the world streams; rails are found in the triangles), `sk_rig_setup` /
  `sk_rig_pose` (fit the host skeleton to the skater), `sk_activate`,
  `sk_set_velocity`, `sk_step` (one fixed step from an XInput-style pad),
  `sk_update`, `sk_get_pose` (root matrix, velocity, camera), `sk_board_pose`,
  `sk_feedback` (state, pops, landings, grinds, bails), `sk_score`.
  Host space is Z-up; Jak is Y-up at 4096 units per metre.
- **`skate-ffi` and `sa-plugin` carry no licence file.** Private use only until
  the author grants one, or until an own wrapper over the Apache-2.0
  `skate_host::bridge::Session` replaces it.
- It needs data in the mashup converter's format (`skate/converter`), not the
  BullySkate extraction format already produced.

Not yet done: building `skate-ffi` for x64, converting the data, or calling it.

## Overnight state (2026-10-11 to 12): Jak skates

How to play it, the controls and the known limits are in
[PLAYING.md](PLAYING.md). In short, while the bridge is active:

- the engine rides the level's own background collision, gathered around Jak
  through the collide cache and re-gathered as he moves;
- Jak's skeleton is fitted to the Skate 3 skater's pose every step
  (`sk_rig_setup` / `sk_rig_pose`, applied through `postbind-function`);
- the engine's own skateboard model is drawn by `SkateBoardRenderer`;
- the bridge replaces the jetboard's movement update instead of following it;
- Skate 3's trick names and score are shown as text.

Findings worth keeping:

- Jak is drawn 1.4 times the size of his model (the scale is in his bone
  matrices), and his world is built to match. The engine is told one of its
  metres is 4096 x 1.4 game units; without that the board and speeds look small.
- A joint's `bind-pose` field in GOAL is the inverse bind matrix.
- A GOAL matrix in memory is the same 16 floats as a column-major matrix, so
  matrices cross the bridge without transposing.
- The level's collision triangles already wind the way the engine expects.
  An early version reversed every triangle when most near-level ones faced
  down; on Haven City's piers, which have more underside than floor, that
  turned the ground inside out and the skater rode two metres under it. The
  winding is now left alone.
- Starting an engine session does not look for grind rails; installing a world
  does. The first world is installed again straight after the session starts.
- The game binds the keyboard to the pad (P is R2, Space is Cross, R is
  Triangle, I/J/K/L the right stick), so key presses sent to the game window
  with `SendInput` exercise the real input path without a controller.
- `CollideMeshRenderer`'s camera maths did not put geometry on screen in Jak 3;
  the skateboard uses the background shaders' `pc_camera` projection, now also
  kept in `SharedRenderState::camera_pc`.
- After a bail the engine respawns the skater at a remembered spot, or at its
  origin when it has none. The bridge catches a jump outside the gathered world
  and restarts him at the last place the wheels were down.
- A function spawned with `process-spawn-function` has about 256 bytes of stack
  to suspend with; the manual checks are step functions called once a frame.
- `sk_preload` on a background thread at boot did not shorten the first
  activation (about 9 s) and was removed.
- Codex agents cannot run local commands on this machine in the `read-only` or
  `workspace-write` sandboxes; they do under the default configuration.

Three bounded research tasks and one code review went to Codex `gpt-6-luna` at
high effort (rig and board handling in the San Andreas host; Daxter, the
jetboard model and native effects in this repository; a defect review of the
bridge). The review's one finding, that water and moving collision are not
handled while skating, stands as a documented limit.

## First in-game bridge run (2026-10-11, about 21:37)

Route chosen: the engine in-process, through the `skate-ffi` C interface.

- `skate_ffi.dll` built for x86-64 MSVC from `GTA-San-AnSkateas` `fde499b` (the
  repository's engine copy lacks the `skate-data` crate; it was copied from the
  mashup repository). Its `tests-c/smoke.c` passed on the converted data (exit 0).
- Data converted with the mashup's `iw4l_skate_convert.py` driving the
  SK8-ENGINE `tools` tree at `cb79689`, into `skate3-work/skate-data-mashup`.
- Jak side: `game/kernel/jak3/skate3_bridge.{h,cpp}` (loads the DLL at run time
  from `OPENGOAL_SKATE3_DLL` / `OPENGOAL_SKATE3_ASSETS`; nothing linked into gk),
  `goal_src/jak3/engine/target/board/skate3-bridge.gc` (default off), and one
  call in `target-board-post`.
- `goal_src/jak3/pc/debug/skate3-bridge-test.gc`, run in Spargus on Jak 3 NTSC
  (SCUS-97330) with scripted input, logged: engine start about 5 s; holding
  push took the board from rest to about 8.5 m/s and moved Jak about 78 m in
  11 s; a right-stick down-then-up flick gave one pop (state 201, airborne),
  one landing, and a peak 1.06 m above the start height; no bails.

Limits of that run: flat plane at the activation height instead of level
collision; Jak keeps the jetboard animation and the native board code still
runs underneath; no grinds, pose or camera; input was scripted, not a pad.
The REPL must stay connected while the test runs: `goalc` resets the game when
it disconnects (`Compiler::~Compiler`).

## Decisions and build check (2026-10-11, later the same day)

- **Skate 3 data:** the project lead owns the physical Xbox 360 disc but has no
  dump. This blocks every route that uses authentic Skate 3 behaviour (A and
  C): the engine cannot start without the extracted patterns, graphs, animation
  banks and physics database. Dumping the owned disc is the unblocking step.
- **Distribution:** private for now, possibly shared later. Design for sharing
  from the start: keep the GPL-3.0 engine in a separate worker program, never
  link it into the ISC-licensed `gk` runtime, and never commit Skate 3 data.
- **Rust:** installed via rustup (rustc 1.99.0). This machine has no Visual
  Studio / MSVC linker, so the default MSVC toolchain cannot link; the
  self-contained `stable-x86_64-pc-windows-gnu` toolchain was added and works.
- **Build check:** `cargo +stable-x86_64-pc-windows-gnu test -p skate-core`
  on skate-3-rust-engine `2e16697` compiles; library tests gave 652 passed,
  3 failed (`rigid_body` quaternion order, `drive_frames` fused-order,
  `skeleton_body` collision feedback). The failures were not investigated and
  the crate's integration tests did not run because of them. The Bevy game,
  the BullySkate worker and anything needing game data remain unbuilt/unrun.
