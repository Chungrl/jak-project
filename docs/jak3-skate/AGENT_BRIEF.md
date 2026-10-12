# Brief for agents working on the Jak 3 skate project (AI-assisted)

Read this first. It is the shared context for every delegated task.

## What this is

Jak 3 on the OpenGOAL PC port, with (1) a bridge that lets an external Skate 3
gameplay engine drive Jak while he is on the jetboard, and (2) an optional
renderer enhancement (ambient occlusion, character shadows, bloom, grading).

- The repository: C++ runtime in `game/`, game code in
  a Lisp called GOAL in `goal_src/jak3/`.
- The work folder, outside the repository, at the path in `JAK3_SKATE_WORK`:
  engine sources in `repos/`, the built `skate_ffi.dll` in `target-ffi/`,
  converted Skate 3 data in `skate-data-mashup/assets`, logs in `run/`.
- Helper scripts: `scripts/jak3-skate/` in the repository.
- Notes: `docs/jak3-skate/skate3-transplant/README.md` and `PLAYING.md`,
  `docs/jak3-skate/graphics-enhancement.md`.

Key files:
- `game/kernel/jak3/skate3_bridge.{h,cpp}`: the C++ side of the bridge.
  `BridgeIO` mirrors the GOAL type `skate3-bridge-io`; commands are numbered.
- `goal_src/jak3/engine/target/board/skate3-bridge.gc`: the GOAL side
  (pad translation, world gathering, Jak's post while skating, the text HUD).
- `game/graphics/opengl_renderer/PostFx.{h,cpp}` and `shaders/postfx_*.frag`,
  `shaders/post_processing.frag`: the image enhancement.
- `goal_src/jak3/pc/debug/skate3-bridge-*.gc`, `gfx-enhance-compare.gc`:
  manual checks, loaded through the REPL, not part of any build.
- The engine's C interface: `repos/GTA-San-AnSkateas/skate-ffi/src/lib.rs`
  (header `skate_ffi.h` beside it). It has local additions. It is not part of
  the Jak repository and must never be copied into it.

## Hard rules

- An agent leaves its changes uncommitted in the working tree; the orchestrator
  verifies, commits and opens the pull request.
- Never copy Skate 3 data, engine source, or anything from the work folder into
  the repository.
- Do not touch files outside the task. Do not reformat files you did not
  change. Match the surrounding code's style and comment density.
- Every new comment block or file header you add says `(AI-assisted)`.
- Comments describe what the code does and why, in plain words. No history of
  how you got there.
- Only one copy of the game can run and only one build can run at a time. If
  the task says you may build and run, nobody else is; if it does not say so,
  do not build or run anything.

## Building and running (only when the task allows it)

Scripted checks hide real controllers; set `KEEP_PAD=1` to turn this off.

All from Git Bash:

- `bash scripts/jak3-skate/build.sh cpp|goal|all`
  rebuilds the runtime and/or the game code. It closes any running game.
  Prints `cpp build exit N`, `goal build exit N` and compile errors.
- `bash scripts/jak3-skate/check-and-run.sh <test.gc> [max seconds]`
  compile-checks a GOAL test file, then starts the game, loads the file
  through the REPL, waits for a line `SK3TEST done`, closes the game and
  prints every `SK3TEST` and `[skate3]` line. Set `NO_SKATE=1` in front to run
  without the skate bridge (faster start).
- The game's full log of the last run: `run/gk-auto.log` in the work folder.
- Screenshots land in `%APPDATA%\OpenGOAL\jak3\screenshots`. Files named
  `sk3-*.png` are deleted at the start of each run. `(pc-screen-shot-window)`
  saves what the window shows; set the name first with
  `(copyn-charp<-string (-> *screen-shot-settings* name) "sk3-name" 244)`.

Writing a GOAL test file (copy the shape of
`goal_src/jak3/pc/debug/skate3-bridge-spin.gc`):
- Print with `(format 0 "SK3TEST ...~%")`. `format` takes at most 8
  parameters in total, so 6 values after the destination and the string.
  Split long lines over several calls.
- A test is a step function called once per frame from a tiny spawned process.
  Never suspend inside nested code: the process has almost no stack.
- End with `(format 0 "SK3TEST done~%")`.
- `(-> io buttons)` bits: A #x1000, B #x2000, X #x4000, D-pad 1/2/4/8,
  LB #x100, RB #x200. Sticks are -1..1.

## Reporting

End with a short report: what you changed (files), what you ran and what it
printed, what you could not verify. If something does not work, say so; do not
paper over it.

## Looking at the picture

- `bash scripts/jak3-skate/shoot.sh` runs
  `goal_src/jak3/pc/debug/gfx-enhance-compare.gc` (five start points: desert,
  Haven industrial, forest, sewer, Haven port) and builds sheets in
  `run/gfx-view/` in the work folder: `<n>-<level>-ab.png` (enhancement off | on, half
  size), `-c.png` (the added shading alone | distance bands) and `-crop.png`
  (full-size crop around Jak, off | on). The full-size captures are in
  `%APPDATA%\OpenGOAL\jak3\screenshots\sk3-gfx-*.png` (a off, b on, c shading
  alone, d distance, e pause menu on the last level).
- Shaders are read from `game/graphics/opengl_renderer/shaders/` when the game
  starts, so a shader-only change needs no rebuild; C++ changes need
  `build.sh cpp`, GOAL changes `build.sh goal`.
- `(pc-set-post-param <id> <thousandths>)` sets a part of the enhancement from
  a test (ids in `goal_src/jak3/kernel-defs.gc`); `(pc-set-post-enhance 0)`
  turns it all off.
- Look at the images you produce. If you cannot view images, measure them
  with Python and Pillow (`python -I`), and say in your report that you could
  not look.
