**Audit boundary:** the local read-only command runner failed before it could open the checkout. I inspected the public source at commit `32ba206` instead. Its correspondence to the local checkout, and any uncommitted files, are **not verified**. I did not build or run it.

## 1. WHAT IT IS

A Rust 2024, Cargo **binary crate** (`skate3_bevy`) using Bevy 0.18.1; [Cargo.toml lines 1–15](https://github.com/chasmlol/skate3clone/blob/32ba206/Cargo.toml#L1-L15) declares no library target. [src/main.rs lines 1–42](https://github.com/chasmlol/skate3clone/blob/32ba206/src/main.rs#L1-L42) wires gameplay, animation, camera, world and tooling modules into one application. `src/sim.rs` alone is about **13,000 lines**; a defensible gameplay-versus-render/tooling total was not measurable with the available access. The public history has **one commit**, dated 3 September 2026. Tests and a headless replay path exist ([src/main.rs lines 213–232](https://github.com/chasmlol/skate3clone/blob/32ba206/src/main.rs#L213-L232)); source supports an early playable slice, but play quality was not checked. [Git history](https://github.com/chasmlol/skate3clone/commits/main).

## 2. PROVENANCE OF GAMEPLAY LOGIC

**No category (a) runtime retail-code execution was found in this crate.** These are Rust implementations, with different confidence levels:

| System | Classification and source evidence |
|---|---|
| Flickit input | **(b), partial.** [src/trick_input.rs lines 1–55](https://github.com/chasmlol/skate3clone/blob/32ba206/src/trick_input.rs#L1-L55) records TU3 function addresses, a captured matcher value and unresolved publication semantics. Geometry recognition is not proof of a complete retail trick winner. |
| Trick selection | **(b), partial.** Recovered graph and clip mappings are explicit, but [src/sim.rs lines 3947–3955](https://github.com/chasmlol/skate3clone/blob/32ba206/src/sim.rs#L3947-L3955) says non-basic trick entry awaits an external Flickit winner. |
| Board physics/integration | **Mixed (b)/(c).** [src/skateboard_solver.rs lines 1–100](https://github.com/chasmlol/skate3clone/blob/32ba206/src/skateboard_solver.rs#L1-L100) traces contact logic to TU3 addresses while listing unresolved backend requirements. [src/retail_skateboard.rs lines 35–60](https://github.com/chasmlol/skate3clone/blob/32ba206/src/retail_skateboard.rs#L35-L60) fits lateral response to captured motion because retail tuning arrays are missing. |
| Grinding | **(b), partial.** [src/grind_contact.rs lines 1–34](https://github.com/chasmlol/skate3clone/blob/32ba206/src/grind_contact.rs#L1-L34) has addresses and hashes but requires missing rail-query and trajectory decisions; [src/grind_graph.rs lines 1–10](https://github.com/chasmlol/skate3clone/blob/32ba206/src/grind_graph.rs#L1-L10) covers graph routing, not a complete grind simulation. |
| Manuals | **(b)/(c).** [src/manual_control.rs lines 1–55](https://github.com/chasmlol/skate3clone/blob/32ba206/src/manual_control.rs#L1-L55) implements recovered intent behavior in a new fixed-step controller. |
| Push/carve | **(b) push tree, (c) fitted carve/slide behavior.** See [src/retail_push.rs lines 1–47](https://github.com/chasmlol/skate3clone/blob/32ba206/src/retail_push.rs#L1-L47) and the captured composite curve above. |
| Bails | **Not found** as an implemented system in the inspected simulation/module list. |
| Animation selection | **(b), partial.** [src/trick_catalog.rs lines 1–26](https://github.com/chasmlol/skate3clone/blob/32ba206/src/trick_catalog.rs#L1-L26) maps recovered resources and hashes; unresolved blends remain. |

## 3. PROPRIETARY CONTENT

Shipped: hand-written retail-derived constants, names, hashes and graph mappings; conversion tools and synthetic/parity assets. I found no shipped ISO/XEX, raw ABIN/RX2, retail model or animation bank. The [ignore rules](https://github.com/chasmlol/skate3clone/blob/32ba206/.gitignore#L5-L18) exclude those files, while [animation.rs lines 17–36](https://github.com/chasmlol/skate3clone/blob/32ba206/src/animation.rs#L17-L36) expects locally generated private GLBs. The user supplies an owned game image for that asset/world pipeline. Ignore rules alone cannot prove every tracked file’s contents; the local tree was inaccessible.

## 4. LICENCE

**Project gameplay code: no licence granted**; the repository’s [licence statement](https://github.com/chasmlol/skate3clone/blob/32ba206/README.md#L263-L267) says public visibility grants no copying or derivative-work permission. That blocks lifting or linking it into an ISC project without permission. Vendored UTT and University conversion sources retain [MIT](https://github.com/chasmlol/skate3clone/blob/32ba206/tools/vendor/utt/LICENSE#L1-L19) [terms](https://github.com/chasmlol/skate3clone/blob/32ba206/tools/vendor/university/LICENSE-PROJECT.md#L1-L25); these do **not** license the gameplay code or retail data. No GPL or non-commercial project-code licence was found.

## 5. ARCHITECTURE FOR EMBEDDING

There is **no ready engine-agnostic gameplay library**. `SkateSim` holds position, velocity and action state; `SkateInput` holds sticks, triggers and pad events ([src/sim.rs lines 3162–3170](https://github.com/chasmlol/skate3clone/blob/32ba206/src/sim.rs#L3162-L3170), [4770–4779](https://github.com/chasmlol/skate3clone/blob/32ba206/src/sim.rs#L4770-L4779)). The step is a Bevy ECS system, `fixed_step(ResMut<SkateSim>, ResMut<SkateInput>, Res<SkateGround>, Option<Res<LevelSpawn>>)` at **120 Hz** ([src/sim.rs lines 6156–6169](https://github.com/chasmlol/skate3clone/blob/32ba206/src/sim.rs#L6156-L6169)), so Jak 3’s 60 Hz loop would need two steps per frame.

`GroundProvider` is a concrete analytic plane/triangle query provider, **not a host callback trait**; contacts carry point, normal, surface/material and triangle data ([src/ground_provider.rs lines 1–37](https://github.com/chasmlol/skate3clone/blob/32ba206/src/ground_provider.rs#L1-L37), [250–285](https://github.com/chasmlol/skate3clone/blob/32ba206/src/ground_provider.rs#L250-L285)). The live step queries downward ([src/sim.rs lines 6635–6644](https://github.com/chasmlol/skate3clone/blob/32ba206/src/sim.rs#L6635-L6644)); complete host sweeps and grind-edge callbacks were **not found**. Coordinates are Y-up (`RETAIL_UP_VECTOR`); metres appear in diagnostics, but a complete handedness/unit contract was **not found**. The main simulation depends on Bevy; `trick_input.rs` and several graph/solver modules are closer to `std`-only. No C ABI, IPC, shared-memory or worker protocol was found.

Animation emits clip names, weights and seek times ([src/sim.rs lines 378–389](https://github.com/chasmlol/skate3clone/blob/32ba206/src/sim.rs#L378-L389)); Bevy loads a private retail-derived GLB rig ([src/animation.rs lines 17–36](https://github.com/chasmlol/skate3clone/blob/32ba206/src/animation.rs#L17-L36)). Bone count was **not found**. Jak’s skeleton would need clip retargeting or replacement animation.

## 6. REUSABILITY VERDICT

**Architecturally:** lift the small recognition and graph algorithms first (`trick_input.rs`, `retail_push.rs`, `manual_balance.rs`, selected trick/landing/grind graphs), subject to obtaining a licence. Replace Bevy input/ECS, collision and rail queries, board integration gaps, asset bank, animation rig and camera. A worker process adds a per-frame state protocol without solving those gaps; a C ABI wrapper around isolated Rust logic is the simpler eventual boundary. **Estimate:** several weeks for a legally cleared stick-input → limited trick recognition → approximate board motion prototype; substantially longer for convincing Skate 3 parity in Jak 3.

Skipped: local checkout inspection and runtime/play testing because the read-only command runner failed. Risk: the public commit may differ from the checkout you asked me to audit.