## 1. WHAT IT IS

This is a Rust 2024 Cargo workspace with nine crates. `skate-core` contains engine-independent gameplay calculations; `skate-data` loads stock data; `skate-game` is the Bevy executable; `skate-dynamics` depends on Rapier. The game startup loads assets, graphs, physics and a skater before constructing the Bevy app. [Cargo.toml:1–13](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/Cargo.toml) · [skate-game/src/main.rs:90–174](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-game/src/main.rs) · [skate-dynamics/Cargo.toml:6–9](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-dynamics/Cargo.toml)

The source contains unit tests, including gesture and frame-scheduler tests, and a substantial game runtime. **Playability was not verified.** Approximate gameplay versus rendering/tooling line counts and the checkout’s last commit date: **not found**. The local command runner failed before inspection, so the linked source evidence is from the public upstream `main` branch; I cannot establish that it exactly matches this checkout.

## 2. PROVENANCE OF GAMEPLAY LOGIC

These are **independent Rust implementations, not (a) retail executable code**: `skate-core` explicitly says its arithmetic is independent and original-hardware numerical agreement is unverified. [skate-core/src/lib.rs:0–5](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/lib.rs)

| System | Classification and source evidence |
|---|---|
| Flickit recognition | **(b) Reverse-engineered.** The pattern matcher identifies TU3 function addresses, buffer order and scoring behavior; authored `.pat` points are loaded separately. [input/gesture.rs:0–2,63–68,126–174](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/input/gesture.rs) · [gesture_patterns.rs:0–8](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-data/src/gesture_patterns.rs) |
| Trick selection | **(b) for graph execution; stock choices require extracted graph data.** The host compiles loaded ActionGraph and MotionGraph bindings, then runs their operations. [graph_runtime.rs:0–18,276–306](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-game/src/graph_runtime.rs) |
| Board physics/integration | **(b).** The solver path cites TU3 job and drive addresses and takes bodies, forces, contacts and settings from its caller. [physics/board_step.rs:0–5,111–146](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/physics/board_step.rs) |
| Grinding | **(b).** Contact and trajectory code cites TU3 addresses and operates on supplied spline primitives. [physics/grind_contact.rs:0–26,209–230](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/physics/grind_contact.rs) |
| Manuals | **(b), with an explicit missing numerical dependency.** [physics/manual/controller.rs:0–2,43–81](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/physics/manual/controller.rs) |
| Push/carve | **(b).** Push force and steering cite TU3 addresses and expose processed inputs and tuning values. [riding/push.rs:0–31](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/riding/push.rs) · [riding/steering.rs:0–37](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/riding/steering.rs) |
| Bails | **Not found:** modules are declared, but I could not inspect enough implementation to classify their provenance. [player/mod.rs:4–19](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/player/mod.rs) |
| Animation selection | **(b) for playback logic; clips and graph choices are external.** Playback cites TU3 addresses and requests named clips through a service. [animation/playback.rs:0–5,16–49,54–100](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/animation/playback.rs) |

Tests visible in these files check Rust behavior; a comprehensive retail trace/parity suite was **not found**.

## 3. PROPRIETARY CONTENT

The code expects user-supplied converted Skate 3 content. Its manifest requires a character GLB and ActionGraph/MotionGraph files; the game also loads animation banks. Startup has tools that read `default.xex` to extract tables. [manifest.rs:7–13,23–37](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-data/src/manifest.rs) · [skater_animation.rs:33–44](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-game/src/skater_animation.rs) · [main.rs:179–219](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-game/src/main.rs)

Retail ISO/XEX, extracted assets, animation clips, graph files or decompiled executable shipped **in-repo: not verified** from the accessible file inventory. The source itself does not show a build-time XEX dependency; runtime setup and normal play have different asset requirements.

## 4. LICENCE

The project’s `LICENSE` is **GNU GPL version 3, 29 June 2007**. It permits commercial use but imposes copyleft and corresponding-source obligations when covered binaries are distributed. Directly linking this code into an ISC-only OpenGOAL distribution would require resolving that licence conflict, typically by licensing the combined covered work compatibly or obtaining separate permission. A worker process does not automatically settle the combined-work question. [LICENSE:0–5,185–205](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/LICENSE)

The workspace patches vendored Bevy rendering crates and `sdl3-sys`. The vendor note identifies Bevy’s MIT/Apache licences; I could not independently inspect every vendored licence file or tooling licence. No project-code non-commercial restriction was found. [Cargo.toml:25–28](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/Cargo.toml) · [vendor/README.md:30–33,84–92](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/vendor/README.md)

## 5. ARCHITECTURE FOR EMBEDDING

`skate-core` is headless and has no declared crate dependencies, but **no single public `step(input) -> output` API**. Its `FrameScheduler` requires host implementations of animation, world, physical and solver phases; `BoardRuntime::advance` takes contacts, truck targets and settings. [skate-core/Cargo.toml:0–4](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/Cargo.toml) · [player/frame.rs:50–97,111–129](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/player/frame.rs) · [physics/board_runtime.rs:215–247](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/physics/board_runtime.rs)

World integration needs ordered collision triangles with material/tag/adjacency data, line and swept queries, moving geometry, and grind-edge primitives. `BoardWorld::ExternalQueries` supplies `line` and `nearby`; `BoardWorld` itself owns triangle contact generation. [physics/board_world.rs:34–55,99–139,231–247](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/physics/board_world.rs)

The core accepts caller-supplied seconds per step; the Bevy app schedules input, controls and physics in `FixedUpdate`. An explicit 60 Hz configuration was **not found**. Source fields label positions in metres and velocities in m/s; grind math uses Y as up. Handedness: **not found**. [player/frame.rs:58–63](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/player/frame.rs) · [app.rs:42–46,96–114](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-game/src/app.rs) · [air/trajectory/types.rs:4–20](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/air/trajectory/types.rs)

No C ABI export, static/dynamic library target, or per-frame IPC/shared-memory host bridge was found. Rust can be wrapped for a C ABI, but that wrapper is new work. Pose evaluation loads animation banks and emits pose matrices for its source skeleton; bone count was **not found**. Jak’s skeleton would need retargeting or replacement clips. [skater_animation.rs:33–44,109–125](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-game/src/skater_animation.rs) · [animation/output/mod.rs:0–15](https://raw.githubusercontent.com/SK8-ENGINE/skate-3-rust-engine/main/crates/skate-core/src/animation/output/mod.rs)

## 6. REUSABILITY VERDICT

**A narrow library transplant is plausible; a drop-in Skate 3 gameplay worker is not present.** Start with `input/gesture.rs`, the PAT loader, and selected `riding`/`physics` kernels. Replace the Bevy host, asset/graph loading boundary, world collision adapter, character pose mapping and scheduling glue. The largest risks are GPL integration, dependence on user-extracted stock graphs/clips/tuning, collision topology differences, and incomplete numerical parity. A rough **several-week prototype** for stick input → recognized trick → simplified board motion is plausible; full skating, grinding and animation parity is a substantially larger port.

Skipped: local line-count and commit audit, builds, execution and play testing because the read-only command runner failed. Risk: upstream `main` may differ from this checkout.