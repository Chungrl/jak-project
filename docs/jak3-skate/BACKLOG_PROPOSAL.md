# Jak 3 skate: baseline and proposed ticket backlog (AI-assisted)

Written 2026-10-12 by the planning session. Nothing here has been created on
GitHub. It is a proposal for review.

## Baseline

| | |
| --- | --- |
| Repository | `C:\Users\Alan\Projects\jak3-opus\jak-project` (git worktree) |
| Branch | `experiment/opus-skate-review` |
| Last verified code | `b8dbedaad` |
| On GitHub | No. The branch tracks `origin/jak3-skate-controls` (`6b30da8e8`) and is 24 commits ahead of it, 0 behind. No remote branch contains `b8dbedaad`. |
| Unverified work | Level shadows, first version, on its own branch `wip/level-shadows` (`58aea2d45`, one commit on top of `a60c7031c`): 11 files changed and 2 new shaders under `game/graphics/` plus `kmachine.cpp`, `kernel-defs.gc`, `pckernel.gc`. It compiles and has never been run. |
| Outside git | `C:\Users\Alan\Projects\skate3-work`: the engine sources with local additions (rail debug exports, spin naming), the built `skate_ffi.dll`, the converted Skate 3 data, the test scripts (`tools\`), the agents' brief (`AGENT_BRIEF.md`). None of it is version controlled. |

How `b8dbedaad` was verified, all on one machine (RTX 5070, Jak 3 NTSC):

- `skate3-bridge-spin.gc` (scripted): finished, bridge active after 1 frame,
  names OLLIE, OLLIE 180, HEELFLIP, GRIND BS 5-0, GRIND BS SALAD, no bails.
- Real key presses (`run-skate-keys.sh play`): mounted several times with no
  hold over 30 ms, pushed to 16-19 m/s, names OLLIE 180, OLLIE 360, FS 180,
  LATE KICKFLIP 360, BS POWERSLIDE, dismounted and remounted.

Run again on 2026-10-12 from a clean build of `a60c7031c` (the same code as
`b8dbedaad`):

- `skate3-bridge-test.gc`: finished with the bridge active, 4 pops, 6
  landings, no bails.
- `skate3-bridge-spin.gc`: finished with the same names as above.
- `gfx-enhance-compare.gc`: completed at all five start points with no shader
  errors; GPU cost 1.76 ms for the shading and 0.24 ms for the bloom. One of
  the sheets was looked at; the rest were looked at when the enhancement was
  committed (`64770152a`), and the committed renderer has not changed since.

## What exists

Verified in game (scripted or key input, see `skate3-transplant/PLAYING.md`):
riding on level and object collision in eight levels; Jak in the skater's
poses on the skateboard model; ollie, kickflip, spins with names; Skate 3's
score as text; grinds when started above a rail; bails and recovery; hand-overs
for water, falling out of the world, grabs, pause, death; engine started in the
background; world scale and time scale. Image enhancement: ambient occlusion,
character shadows, surface relief and shine, rim light, bloom, grading.

Partly there:
- Grinds from a normal approach: 2 of 264 in the one scripted attempt; not
  tried by hand with a report.
- Collision with moving platforms, doors, vehicles and characters.
- The HUD is debug text. The camera is Jak's. Head and fingers do not move.
- Trick names lack body flips. The engine copy in use is older than the one in
  the BullySkate repository; only its naming rules were carried over.
- Nothing set at run time is saved (enhancement strength, scales).
- Level shadows: written, never run.

Never tested: manuals, grabs, reverts, the session marker; cutscenes and
missions with the bridge on; the enhancement at night, underwater, with dark
or light Jak, in vehicles; any other hardware; long sessions (two game exits
reported by the test harness earlier were never explained or reproduced).

Missing: skateboard sounds; Skate's camera; a styled HUD; menu entries;
anything to do on the board; light shafts and haze; per-pixel character
lighting; upscaling (DLSS cannot attach to an OpenGL renderer).

## Stalled work

Two Codex tasks were launched before this proposal: skate audio research
(read-only) and testing the level shadows. Both blocked at start waiting for
standard input and did nothing, and were closed with the owner's approval.
They appear below as skate-01 and gfx-01; their prompts are in the work
folder under `run/codex/`. (`codex exec` reads standard input when it is not
a terminal: launch it with `< /dev/null`.)

## Ticket shape

Each ticket is one issue:

- **Title**: `[area-nn] what changes`, area one of `infra`, `gfx`, `skate`.
- **Labels**: `agent:codex` or `agent:claude`; `loop:ready` or `loop:blocked`;
  `needs:human` where a person must act or decide; `kind:test` for tickets
  that only measure and report.
- **Body**: Scope (and what is out of scope), Files, Depends on, Acceptance
  (observable, each one checkable), Verify (commands), Context (what the
  worker must read first), Assumptions.
- One ticket, one pull request, one concern. A ticket that needs someone to
  look at pictures says so and attaches them; the planning session reviews
  those before merge.

`VERIFY` below is the command ticket infra-01 creates:
`bash scripts/jak3-skate/verify.sh`, which builds the runtime and game code and
runs the bridge check, failing unless it prints `SK3TEST done` and
`bridge active #t`. Until infra-01 lands the same scripts are in
`skate3-work\tools\` with this checkout's path written into them.

Only one game and one build can run at a time on this machine, so tickets run
one at a time.

## Proposed tickets

### Infrastructure

**[infra-01] Test scripts in the repository, without fixed paths** (`agent:codex`, ready)
- Scope: copy `build.sh`, `run-skate-test.sh`, `check-and-run.sh`, `run-skate-keys.sh`, `keys.py`, `shoot.sh`, `montage.py` from `skate3-work\tools` to `scripts/jak3-skate/`; take the repository from the script's own location and the work folder from `JAK3_SKATE_WORK`; add `verify.sh`; move the agents' brief to `docs/jak3-skate/AGENT_BRIEF.md`.
- Acceptance: no user-specific path in any of them; `verify.sh` passes in this checkout; `shoot.sh` writes its sheets under the work folder.
- Verify: `bash scripts/jak3-skate/verify.sh`; `grep -rn "Users/Alan" scripts/jak3-skate` prints nothing.

**[infra-02] Stand up the isolated clone** (`needs:human`)
- A full build, the extracted game, the texture pack and the two environment variables in `C:\Users\Alan\Projects\jak3-loop\jak-project`. Not a Codex ticket; listed because every other ticket's verification depends on it.
- Acceptance: `VERIFY` passes there.

**[infra-03] Local engine additions as patch files** (`agent:codex`, ready)
- Scope: turn the two local additions to the engine sources (rail debug exports in `skate-ffi/src/lib.rs`; spin naming, already scripted in `tools/patch_engine_spin.py`) into patches plus one script that applies them to a clean `GTA-San-AnSkateas` at `fde499b` and builds the DLL. Lives in the work folder, not the repository. Update `skate3-transplant/README.md`.
- Acceptance: from a fresh clone the script produces a DLL exporting `sk_debug_rail`, and `skate3-bridge-spin.gc` prints a name ending in `180`.
- Verify: the script; `check-and-run.sh goal_src/jak3/pc/debug/skate3-bridge-spin.gc`.

**[infra-04] Save the enhancement and skate settings** (`agent:codex`, blocked on infra-01)
- Scope: enhancement strength, skate world scale, time scale, HUD on/off in `pc-settings-jak3` (read, write, apply). No menu.
- Files: `goal_src/jak3/pc/pckernel-impl.gc`, `pckernel.gc`, `skate3-bridge.gc`.
- Acceptance: a new check sets each value, saves, and after a restart reads the same values back; an old settings file without them loads with the defaults.
- Verify: `VERIFY`; the new check, run twice.

**[infra-05] Menu entries for them** (`agent:codex`, blocked on infra-04)
- Files: `goal_src/jak3/pc/progress/progress-static-pc.gc`, text files.
- Acceptance: entries present and working; a capture of the menu attached.
- Verify: `VERIFY`; `task fix-translations`.

### Graphics

**[gfx-01] Level shadows: test, fix, tune** (`agent:codex`, ready; pictures reviewed before merge)
- Scope: the first version on branch `wip/level-shadows`. Prompt already written, in the work folder: `run/codex/shadows.prompt.md`.
- Acceptance: outdoors, shadows of buildings, rocks and walls on the side away from the sun, attached at the foot, stable as the camera turns; no speckle on lit ground; sewer and pause menu unchanged; GPU cost reported; `VERIFY` passes.
- Verify: `VERIFY`; `bash scripts/jak3-skate/shoot.sh`, sheets attached.

**[gfx-02] Light shafts through the shadow map** (`agent:codex`, blocked on gfx-01)
- Scope: half-resolution march of the far shadow map, blurred, added in `postfx_light.frag`; a strength parameter; outdoors only.
- Acceptance: visible between occluders when looking towards the sun; nothing indoors; no banding; cost under 1.5 ms at 1440p.
- Verify: as gfx-01.

**[gfx-03] Distance haze** (`agent:codex`, blocked on infra-01)
- Scope: in `postfx_light.frag`, tint by distance towards the game's fog colour; a strength parameter, default subtle.
- Acceptance: near field unchanged in the off|on sheets; no change at strength 0.

**[gfx-04] Per-pixel lighting on characters** (`agent:codex`, blocked on infra-01)
- Files: `shaders/merc2.{vert,frag}`, `emerc.{vert,frag}`, `foreground/Merc2.cpp`.
- Acceptance: with the enhancement at 0 the captures match the originals pixel for pixel; at 100, smooth shading and a highlight on Jak, no change to transparent or glowing models.

**[gfx-05] Capture sweep beyond the five daytime spots** (`agent:codex`, `kind:test`, blocked on infra-01)
- Scope: extend `gfx-enhance-compare.gc` with night, a wet level, a vehicle, a cutscene start; report artifacts with pictures. No fixes.

**[gfx-06] Upscaling options** (`agent:claude`, research)
- What a render scale below 100% with a spatial or temporal upscaler would take in this OpenGL renderer, and what DLSS would need. A written answer, no code.

**[gfx-07] Light the skateboard like the characters** (`agent:codex`, blocked on gfx-04)

### Skate

**[skate-01] Skate audio research** (`agent:codex`, ready, read-only)
- Prompt already written, in the work folder: `run/codex/audio-research.prompt.md`. Output: `skate3-transplant/codex-audits/skate-audio.md`.

**[skate-02] Skateboard sounds** (`agent:codex`, blocked on skate-01 and on a decision, `needs:human`)
- Which of the three routes in skate-01's report to take is a product decision (Skate 3's own sounds from the user's data, or Jak's).

**[skate-03] Skate's camera** (`agent:codex`, blocked on infra-01)
- Scope: use the camera the engine reports each step (`SkPose`), behind a switch, default off until looked at.
- Acceptance: captures while riding; Jak's camera back within a second of getting off; switch works.

**[skate-04] Styled trick and score display** (`agent:codex`, blocked on infra-01)
- Files: `skate3-bridge.gc` (`skate3-bridge-draw-hud`).
- Acceptance: trick name, running score with multiplier, landed total, shown with the game's HUD fonts; captures attached; nothing drawn off the board.

**[skate-05] Head and neck follow the skater** (`agent:codex`, blocked on infra-01)
- Files: `skate3_bridge.cpp` (`kRigMap`).

**[skate-06] Engine copies compared** (`agent:codex`, `kind:test`, ready, read-only)
- What `BullySkate/skate-runtime` has that `mashup/skate/crates/skate-host` lacks, by module, and what moving the DLL onto it would take.

**[skate-07] Body-flip names** (`agent:codex`, blocked on skate-06)

**[skate-08] Grinds from a normal approach** (`agent:codex`, `kind:test`, blocked on infra-01)
- Scope: ride at N rails the engine found from several angles and speeds and ollie; report the hit rate and what the misses have in common.

**[skate-09] Manuals, grabs, reverts** (`agent:codex`, `kind:test`, blocked on infra-01)
- Scope: a scripted check per mechanic that prints the names announced.

**[skate-10] Long ride** (`agent:codex`, `kind:test`, blocked on infra-01)
- Scope: twenty minutes of scripted riding across levels, to reproduce or rule out the two unexplained exits.

**[skate-11] Cutscenes and missions with the bridge on** (`agent:codex`, `kind:test`, blocked on infra-01)

**[skate-12] Score session** (`agent:codex`, blocked on skate-04 and infra-04)
- Scope: a timed run started from the board, three score thresholds, best per level saved.

**[skate-13] Moving collision** (`agent:claude` to design first)
- Platforms, doors, vehicles. Needs a design before it is a Codex ticket.

## Order

infra-01 first: every verification command depends on it. Then gfx-01 and
skate-01, which are already specified. skate-06, skate-08 to skate-11 and
gfx-05 only measure, and tell us which of the remaining tickets matter most.

## Decisions

Answered by the owner on 2026-10-12:

1. `AGENTS.md` said agents never create an issue or a pull request. Allowed on
   the fork only; the file now says so.
2. Push the work to GitHub: yes.
3. The two stalled Codex tasks: closed; they become tickets.
4. Enable Issues on the fork and create this backlog: yes. No ticket is to be
   started until the owner says so.

Still open: whether pictures are reviewed by the planning session before
every graphics merge (proposed: yes).
