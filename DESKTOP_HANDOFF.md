# Windows desktop handoff (AI-assisted)

## Current handoff: recovered skating work

The current branch is **`jak3-skate-controls`**, with its draft PR targeting
**`Chungrl/jak-project:jak3-nextgen-lab`**. This is an interrupted, incomplete
snapshot with a known GOAL reader error; it is not ready for gameplay testing.
Read [RECOVERY_STATUS.md](RECOVERY_STATUS.md) for exact surviving work, test
evidence and known issues. Do not rerun `work/integrate-skate-a.py`.

To inspect it without disturbing an existing lab checkout, use an unused worktree
and local branch name in PowerShell:

```powershell
git fetch origin jak3-nextgen-lab jak3-skate-controls
git worktree add -b jak3-skate-recovery-desktop ../jak3-skate-recovery-desktop origin/jak3-skate-controls
```

No recovery build or gameplay launch was performed. The next engineering owner
must first resolve the documented syntax and input-ownership issues and rerun
the model, integration and regression checks. Windows validation must then cover
controllers and frame rates, original-profile missions/RC/scripted animation,
ground/pop/collision/rail/halfpipe traversal, and deck/rider animation isolation.
No archive or ISO should be committed or attached to the PR.

The instructions below belong to **the earlier coasting foundation**; their
branch, build/test results and feature descriptions do not verify skating.

## Historical lab handoff

Updated: 2026-10-11. Repository: https://github.com/Chungrl/jak-project.
Feature branch: `jak3-nextgen-lab`. PR base: **`Chungrl/jak-project:master`**.
Do not target upstream `open-goal/jak-project`.

## Get the branch

For a fresh checkout in PowerShell:

```powershell
git clone --branch jak3-nextgen-lab https://github.com/Chungrl/jak-project.git
cd jak-project
git status --short --branch
```

For an existing clone of this fork with a clean worktree:

```powershell
git fetch origin
git switch --track origin/jak3-nextgen-lab
```

If the branch already exists locally, use `git switch jak3-nextgen-lab`, then
`git pull --ff-only`. Preserve any unrelated local work before switching.
`git remote -v` should identify the fork before any push or PR operation.
Keep the checkout directory named `jak-project`: automatic project discovery in
the test executable depends on that name or a correctly packaged `data` directory.

## Build and automated verification

Follow [Windows setup](docs/setup/system/windows.md). Install Visual Studio 2022
with **Desktop development with C++**, including the Windows SDK. Install the
repository's other tools with Scoop:

```powershell
scoop install git llvm nasm python task ninja cmake
```

Use an x64 Visual Studio developer shell so the compiler can locate the SDK:

```powershell
cmake --preset Release-windows-clang
cmake --build out/build/Release --parallel 3
task set-game-jak3
& ./out/build/Release/bin/goalc-test.exe --gtest_filter='Jak3KernelTest.*:Jak3TypeConsistency.TypeConsistency'
& ./out/build/Release/bin/goalc-test.exe --gtest_filter='-*MANUAL_TEST*'
& ./out/build/Release/bin/goalc.exe --game jak3 --cmd '(build-kernel)'
```

These Windows commands are a handoff, not a claim of a Windows pass. The new tests
execute the actual production helper in the Jak 3 kernel. To check integration
with the board's complete compilation dependency chain, start `task repl` and enter
this directly in the GOAL REPL (this avoids PowerShell's native quote handling):

```lisp
(make "$OUT/obj/target-board.o" :force #t)
```

This source-only check does not require extracted assets. Invoke
`goalc-test` directly: CTest's test discovery is disabled in this repository.

## Boot with your own extracted game data

Use your legally obtained Jak 3 data and the repository's [extraction and build
instructions](README.md). No ISO, textures, models, music, or other copyrighted
assets are supplied by this branch. After extraction, in one terminal:

```powershell
task repl
```

In the GOAL REPL, build the game output (this step requires your extracted data):

```lisp
(mi)
```

In a second terminal from the repository root:

```powershell
task boot-game
```

Connect the first terminal to the running debug game and inspect the default:

```lisp
(lt)
*jak3-nextgen-board-coast*
```

Expected value is `#f`. Mount the board in an ordinary ground area, then enable:

```lisp
(set! *jak3-nextgen-board-coast* #t)
```

Accelerate with the stick, then release it. Powered forward thrust should stop
and the existing drag should slow the board. Any nonzero processed stick input
restores the original thrust calculation; this patch adds no new dead zone.
Toggle back for a direct comparison:

```lisp
(set! *jak3-nextgen-board-coast* #f)
```

Prefer a full `(mi)` and fresh boot after updating this branch; GAME.CGO includes
a new helper object. For live edits to existing lab code, load the helper before
the board implementation:

```lisp
(ml "goal_src/jak3/engine/target/board/board-nextgen.gc")
(ml "goal_src/jak3/engine/target/board/target-board.gc")
```

Reloading the helper resets the toggle to `#f`; enable it again deliberately.
The helper alone does not install the movement call site into an old game build.

## Required local gameplay checks

All items below remain **not run** in the cloud. Test on a spare save; record
Windows version, CPU/GPU/driver, controller, resolution, frame cap, location,
feature state, and observations in TEST_RESULTS.md.

| Check | Expected result / observation to record |
| --- | --- |
| Feature off and fresh boot | Original acceleration, release behavior, tricks, and missions; toggle starts `#f`. |
| Flat ground release | Feature on produces smooth deceleration; no abrupt stop, reversed motion, or idle creep. Compare on/off over the same route. |
| Active input | Full and slight stick input retain original thrust. Existing dead-zone boundary stays unchanged; assess the thrust transition from neutral to slight input. |
| 30/60/120 FPS | Check feel and release distance at each supported frame cap. Exact frame invariance is not promised. |
| Slopes and surface transitions | Check uphill/downhill, edges, landing, dismount/remount, and starting from rest. Gravity still acts; coasting does not hold the board on a slope. |
| Collision recovery | Head-on and glancing walls, actor contact, and getting unstuck remain usable. Original wall-recovery logic still runs. |
| Specialized modes | Duck, air tricks, spins, boosts, rails, halfpipes, water/hazards, RC controls: no unintended change. |
| Mission/camera | Check a board mission/race, scripted control, camera tracking, and halfpipe camera offsets. |
| Immediate rollback | Set flag `#f` while moving; original movement resumes without restart or save migration. |

## Reversal and next work

Disable the runtime flag for immediate reversal. To remove the source change from
the feature branch, revert its implementation commit, rebuild `(mi)`, and restart
the game. To return to the fork baseline, switch to `master`, rebuild, and restart.
Do not reuse a previously generated GAME.CGO across code versions.

Review [PROJECT_STATUS.md](PROJECT_STATUS.md), [TEST_RESULTS.md](TEST_RESULTS.md),
and [ROADMAP.md](ROADMAP.md) before extending physics or rendering. The first visual
upgrade remains a proposal; this branch has not modified GLSL or claimed improved
graphics quality. Keep the pull request draft until local gameplay checks are recorded.
