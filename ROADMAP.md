# Jak 3: Next Generation roadmap (AI-assisted)

Status date: 2026-10-11. All experiments stay on `jak3-nextgen-lab` and are
disabled by default. Implemented work and future proposals are distinguished
below; automated correctness does not establish gameplay feel or image quality.

## Phase 1 — Reversible movement foundation

Implemented: opt-in neutral-input jetboard coasting via
`*jak3-nextgen-board-coast*`. On ordinary ground, releasing the stick removes
powered thrust while retaining the existing drag. Nonzero input and other
surfaces retain their existing thrust. Local gameplay feel remains unverified;
see `TEST_RESULTS.md` for automated evidence.

Acceptance gate: compiler/runtime regression checks pass; Windows gameplay
confirms the default mode matches the baseline and the enabled mode coasts
predictably on flat ground, slopes and near obstacles. Check mounting,
dismounting, jumps, halfpipes, grinding, missions and the existing turbo cheat.
Record routes, controller settings and frame rate before tuning values.

Next: add separate default-off analog acceleration, progressive braking and
steering experiments. Preserve the current collision/state machine, bound
parameters and compare identical routes. Add tricks and grind entry/exit
changes only after acceleration, stopping distance and turning feel are stable.
Keep each behavior independently switchable and reversible.

## Phase 2 — First visual experiment

Planned, not implemented: bounded sharpening, with strength zero by default and
a Jak 3 gate, in the existing final presentation pass:

- `game/graphics/opengl_renderer/OpenGLRenderer.cpp`, `do_pcrtc_effects`, already
  resolves MSAA and samples the final color framebuffer.
- `game/graphics/opengl_renderer/shaders/post_processing.frag` currently applies
  brightness and contrast. Add a small clamped sharpening strength, sample
  offsets derived from texture size, and neighborhood bounds to reduce halos.
  Preserve the original sampling path when strength is zero.
- Plumb the option through graphics settings and render options. Verify its
  interaction with brightness/contrast, existing color filters and blackouts.

MSAA, high internal resolution, mipmaps and maximum-device anisotropic filtering
already exist. Use them to establish a visual baseline; they are not new
Next Generation features. The final pass includes HUD and menus, so assess text
artifacts before considering a scene-only pass ahead of HUD rendering.

Acceptance gate: real Windows OpenGL shader compilation/linking succeeds;
feature-off captures match the baseline; enabled captures show useful detail
without distracting halos or flicker. Test desert, interiors, water, particles,
slow-time, HUD, menus, letterboxing, resizing and screenshots. Measure frame
times at 1080p, 1440p and 4K with MSAA off/on. GLSL compiles at runtime in
`Shader.cpp`; a C++ build or CPU test alone cannot verify shader compatibility
or visual quality.

## Phase 3 — Rendering architecture and original art

Inventory frame costs and scene data before choosing deeper upgrades. Main
framebuffers currently use RGBA8 color and depth/stencil renderbuffers;
terrain shaders expose color, UV and fog rather than general normal/material
buffers. Character normals support vertex lighting, not a complete PBR pipeline.

Prototype sampled depth with correct projection/depth reconstruction before
SSAO. Define linear color, HDR targets, exposure and blending compatibility
before HDR/bloom work. Establish terrain normals, material metadata and a
lighting interface before per-pixel lighting or PBR. Preserve original fog,
time-of-day colors, alpha tests, transparency and special effects.

Acceptance gate: representative scene captures, GPU timings and memory budgets
show a measurable benefit; baseline mode remains available and compatible.
Use synthetic geometry and original or explicitly licensed art for tests.
Existing texture replacement and GLB character replacement paths under
`custom_assets/jak3/` provide useful tooling, but do not imply complete material
or terrain replacement support.

## Phase 4 — Grappling

Build a default-off tether experiment using the existing collision query/cache
system. Establish anchor eligibility, rope constraint stability, swing input,
detach velocity, moving anchors and collision response. Integrate camera
occlusion and mission/state transitions before expanding traversal.

Acceptance gate: deterministic constraint tests and Windows play sessions cover
walls, corners, slopes, moving objects, high speed, death and reloads. Avoid
tunneling and camera clipping; keep existing routes and missions playable.

## Phase 5 — Portal traversal

Prerequisites: tested position/orientation/velocity transforms, collision-safe
exit placement and camera transitions. Rendering additionally needs alternate
cameras, portal clipping, visibility/streaming decisions, render targets and a
bounded recursion policy. Separate traversal correctness from portal visuals.

Acceptance gate: round-trip transform tests pass; local gameplay verifies
momentum, exit clearance, camera continuity, streaming and mission state.
GPU captures and timings establish an acceptable recursion and memory budget.

## Asset and publication policy

Never commit or distribute copyrighted game assets, extracted assets, ISOs or
save files. Local users supply their own lawful game data. Commit source,
documentation, tests and original/licensed test material only. Keep changes on
the feature branch and direct any pull request to the user's fork, with clear
AI disclosure and a distinction between automated verification and local testing.
# Recovery handoff takes precedence (AI-assisted)

Skating work is preserved on `jak3-skate-controls`, stacked on `jak3-nextgen-lab`.
Read [RECOVERY_STATUS.md](RECOVERY_STATUS.md), including the complete feature-area
checklist and known compile/ownership issues, before planning further work.
This recovery task authorizes no new feature implementation. The roadmap below
is historical context from the earlier lab, not skating completion evidence.

