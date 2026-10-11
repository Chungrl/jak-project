# Verification results (AI-assisted)

Date: 2026-10-11. Branch: `jak3-nextgen-lab`.
Baseline: `efb21c3e8c5a40a74f2e86510f915da55eaeba34`.

## Environment and evidence

The user's earlier Clang build, 1,669-test pass, and compiler smoke pass are
historical reports. No prior binaries, build cache, reports, or Docker images
were present in the resumed workspace. This task restores the documented Linux
dependencies in an isolated Debian 12 Docker toolchain and runs fresh checks.
The checkout is bind-mounted as `/repo`; commands below run from that directory.
No extracted game assets are present or required for these source/kernel checks.

Clang 14.0.6, CMake 3.25.1, Ninja 1.11.1, and LLD are used for a Release build with three
parallel jobs. The container runs compilation and tests as UID/GID 1000.
Configuration:

```sh
cmake -S /repo -B /repo/build/nextgen -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld \
  -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld
cmake --build build/nextgen --parallel 3
```

The first test launch aborted before executing tests: mounting the checkout as
`/repo` defeated the existing executable-path discovery, which searches for
`jak-project` or an adjacent `data` directory. This was corrected without source
changes by adding the supported data-directory alias:

```sh
ln -s /repo /repo/build/nextgen/data
```

The compiler CLI checks also use `--proj-path /repo`. In a normal checkout named
`jak-project`, the alias is unnecessary. The initial failure log is retained in
the handoff evidence. Native build completion and a subsequent no-work build
check returned exit code 0.

The first full-suite attempt then aborted during game-code initialization because
the container's non-root user had no writable default configuration directory.
The full suite was rerun successfully with a task-local `XDG_CONFIG_HOME` under
`work/nextgen-config`; its initial failure log is also retained. No gameplay source changes were needed
for either environment correction. Headless kernel runs emit nonfatal `Cubeb init
failed` messages because there is no audio device; physical audio playback is not
tested.

The XML reports confirm zero failures, zero errors, and zero disabled tests in
both successful runs. The focused run took 48.701 seconds; the full run took
123.545 seconds. The full count is the reported baseline's 1,669 tests plus the
three new lab regressions. All manual tests were excluded by the existing filter.

| Check | Current result |
| --- | --- |
| CMake configuration | PASS |
| Full native Clang Release build | PASS — all 1,484 steps completed; final exit code 0 |
| Focused Jak 3 kernel and source consistency tests | PASS — 8 tests, including all 3 new regressions |
| Full non-manual automated test suite | PASS — 1,672 tests across 75 suites, 0 failures/errors |
| Jak 3 compiler kernel smoke | PASS — 9 build targets, exit code 0 |
| Actual `target-board.o` dependency-chain compile | PASS — 354 build targets, including the new helper and actual board implementation |
| Diff whitespace check | PASS |

Evidence is retained in the downloadable handoff: `jak3-focused.xml`,
`full-suite.xml`, successful run logs, native configure/build logs, toolchain
versions, native build exit status, and both initial environment failure logs.
The final source patch and an incremental Git bundle accompany the documents.

## Automated test scope

Three new `Jak3KernelTest.BoardNextgen*` tests load the **production**
`board-nextgen.gc` into the actual compiled Jak 3 GOAL kernel:

- Default flag is false; positive, negative, and zero thrust values are retained.
- Enabled coasting returns zero thrust for eligible neutral input; nonzero input
  (including a tiny positive value) and ineligible ground retain original thrust.
  Disabling the flag restores the original result immediately.
- A flat-ground scalar update applies the production helper followed by the
  existing longitudinal drag equation. Positive speed decreases monotonically
  over one second at 30, 60, and 120 Hz. This is an isolated calculation, not a
  full target/collision simulation or proof of exact frame invariance.

The source integration is compiled in the complete board dependency chain.
The call-site state guards were independently source-reviewed; helper tests do
not execute those guards through a live game target. Full Jak 3 type consistency
also compiles game sources without ISO assets.

Reproduction commands after building:

```sh
build/nextgen/goalc-test \
  --gtest_filter='Jak3KernelTest.*:Jak3TypeConsistency.TypeConsistency' \
  --gtest_output=xml:work/jak3-focused.xml
mkdir -p work/nextgen-config
XDG_CONFIG_HOME=/repo/work/nextgen-config build/nextgen/goalc-test \
  --gtest_filter='-*MANUAL_TEST*' \
  --gtest_output=xml:work/full-suite.xml
build/nextgen/goalc/goalc --proj-path /repo --game jak3 --cmd '(build-kernel)'
build/nextgen/goalc/goalc --proj-path /repo --game jak3 \
  --cmd '(make "$OUT/obj/target-board.o" :force #t)'
git diff --check
```

Invoke the test executable directly; `gtest_discover_tests` is disabled here,
so a CTest invocation alone would not be evidence of a passed suite.

## Not verified automatically

Windows compilation, controller/keyboard feel, actual surface/state gating,
slopes, wall recovery, landing/grind/halfpipe transitions, scripted missions,
camera comfort, save compatibility in gameplay, and GPU rendering require local
testing. These are **not run** in this asset-free cloud task. Existing gravity
can still move the board on slopes. Enabled thrust switches back to its original
minimum for any nonzero input; evaluate that transition locally.

No rendering code or GLSL was modified. Bounded sharpening is a future proposal,
not a tested graphics upgrade. A native build cannot verify runtime GLSL compilation
or visual quality. See [DESKTOP_HANDOFF.md](DESKTOP_HANDOFF.md) for local checks.
