# Current skating recovery status (AI-assisted)

`jak3-skate-controls` preserves the interrupted work from `jak3-nextgen-lab`.
The latest integration build fails; 25 earlier model tests passed. See
[RECOVERY_STATUS.md](RECOVERY_STATUS.md) for evidence, incomplete features and
the known scripted-state camera-ownership concern. No implementation was added
during recovery. The project status below describes the historical lab foundation.

# Jak 3: Next Generation — project status (AI-assisted)

Updated: 2026-10-11. Lab branch: `jak3-nextgen-lab`.
Baseline: `efb21c3e8c5a40a74f2e86510f915da55eaeba34` in the user's fork,
[Chungrl/jak-project](https://github.com/Chungrl/jak-project). This is an experimental
fork project, not an upstream OpenGOAL release.

## Implemented experiment

**Neutral-stick ground coasting**, disabled by default. Original board movement
continues to supply powered forward thrust when the stick is neutral. The lab
toggle removes that thrust on ordinary ground so the existing drag slows the board.
This is a small movement change toward deliberate player control, not a full
Skate-style physics implementation or momentum conservation.

Enable in a connected Jak 3 GOAL REPL:

```lisp
(set! *jak3-nextgen-board-coast* #t)
```

Restore original movement immediately:

```lisp
(set! *jak3-nextgen-board-coast* #f)
```

The flag is runtime-only and resets to `#f` when its object is loaded, including
a fresh game boot. No save format, persistent settings, input mapping, or structure
layout changed. The default path skips the helper and retains original arithmetic.
Enabling applies only when on-surface, `pat-mode ground`, using `*board-walk-mods*`,
with neutral processed pad magnitude and neither RC controls nor forced stick-off.
Airborne, duck, spin, rail, and halfpipe movement keeps the original path. Gravity,
speed caps, surface drag, wall recovery, collision integration, and cameras remain
in the existing pipeline.

The experiment switches thrust off at neutral and back to the original minimum
thrust for any active input. The feel of that transition is a local acceptance
check; input filtering and acceleration smoothing are future work.

Implementation:

- [board-nextgen.gc](goal_src/jak3/engine/target/board/board-nextgen.gc): default-off toggle and testable production thrust helper.
- [target-board.gc](goal_src/jak3/engine/target/board/target-board.gc): guarded call inside `board-add-thrust`, before existing integration.
- [game.gd](goal_src/jak3/dgos/game.gd): loads the helper object before `target-board.o`; the project generates compilation dependencies from this list.
- [test_goal_kernel3.cpp](test/goalc/test_goal_kernel3.cpp) and [board-nextgen-test.gc](test/goalc/source_templates/jak3/board-nextgen-test.gc): compiled GOAL regression tests.

## Investigation findings

Independent gameplay, rendering, and QA agents investigated the repository; a
gameplay agent also reviewed the implementation. Source review is not a gameplay
test. No game assets were added or downloaded.

| Area | Source entry points | Findings and implications |
| --- | --- | --- |
| Gameplay | `engine/target/board/{board-h,board-states,board-util,board-part,target-board}.gc` under `goal_src/jak3` | Board structures, tuning, lifecycle, tricks, and movement already exist. Extend a guarded movement stage before changing state machines. |
| Input | `engine/target/logic-target.gc`, `read-pad` | Camera-relative stick direction and processed magnitude already exist. Coast uses the existing dead zone. |
| Collision | `target-board-collision`; `engine/collide/{collide-shape,collide-cache,collide-edge-grab}.gc` | Board integration, wall glances, surface reactions, and grind edges are coupled. Preserve the solver for the first experiment. |
| Camera | `logic-target.gc`, `board-states.gc`, `engine/camera/{cam-master,cam-combiner,cam-states,cam-update}.gc` | Tracking, collision probes, halfpipe offsets, interpolation, and FOV have existing behavior. Any later speed-sensitive FOV must preserve fog/aspect correction. |
| Renderer | `game/graphics/opengl_renderer/{OpenGLRenderer,BlitDisplays,Shader}.cpp`, `shaders/{post_processing.frag,tfrag3.vert,tfrag3.frag,merc2.vert}` | Shared OpenGL renderer preserves PS2 effects. Final post-processing is a practical first optional visual extension. |
| Asset pipeline | `decompiler/level_extractor/{extract_level,merc_replacement,fr3_to_gltf}.cpp`, `custom_assets/README.md` | Replacement textures and character models already have integration paths. Future assets must be original or explicitly licensed. |

## Graphics decision

The first proposed visual upgrade is bounded sharpening in the existing final
post-processing pass, with strength `0` by default and a Jak 3 gate. It is **planned,
not implemented** in this change. High internal resolutions, MSAA, and maximum
anisotropic filtering already exist and are not new lab features.

HDR, PBR, and SSAO need deeper work: color targets are currently `GL_RGBA8`, depth
uses a depth/stencil renderbuffer, terrain lacks general normal/material buffers,
and character lighting is currently computed per vertex. See [ROADMAP.md](ROADMAP.md)
for implementation and validation gates.

## Verification and continuation

The previously reported build and 1,669-test pass are historical context. Their
toolchain, binaries, and logs were absent after environment startup, so this task
restores an isolated Clang toolchain and records new results in
[TEST_RESULTS.md](TEST_RESULTS.md). That file is the authority for executed checks.
Fresh verification passed the full native build, 1,672 non-manual tests, eight
focused Jak 3 tests, compiler kernel smoke, and the real board dependency-chain compile.
The cloud checkout has no extracted Jak 3 assets; local gameplay feel, state
transitions, mission compatibility, Windows builds, and GPU behavior remain unverified.

Use [DESKTOP_HANDOFF.md](DESKTOP_HANDOFF.md) to continue on Windows. Keep all work
on the feature branch and target pull requests at the user's fork. The repository's
requested AI disclosure is in [AI_DISCLOSURE.md](AI_DISCLOSURE.md).
