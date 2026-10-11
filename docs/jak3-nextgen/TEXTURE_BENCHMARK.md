# Jak 3 baseline versus 4x at 1440p (AI-assisted)

This is a procedure, not a performance result. A = all texture packs disabled and
applied; B = only Cioscos' pack enabled and applied. Use the same installed
OpenGOAL version, saves, hardware and settings throughout. Applying either
condition must finish before restarting the game. Complete the rollback check in
[DESKTOP_HANDOFF.md](../../DESKTOP_HANDOFF.md).

## Fixed controls and run metadata

- Windows desktop and game output: **2560x1440**, 16:9. Prefer fullscreen with
  game resolution explicitly 2560x1440. In this Jak 3 menu the resolution entry
  is labeled Game Resolution in fullscreen and Window Size in windowed mode.
  Do not use Auto, a lower internal resolution, supersampling or dynamic scaling.
  Record the requested internal render dimensions and check the runtime log's
  `FBO Setup: requested 2560x1440` line when available. Save settings screenshots.
- MSAA off (1 sample) for the primary comparison. Existing anisotropy stays
  unchanged. Keep all PS2 graphics/LOD/shadow/cloud options, brightness, contrast,
  FOV/camera mode, display refresh rate, Windows scaling and driver settings fixed.
  Driver upscaling, sharpening, ReShade and unrelated overlays/mods stay off.
- Primary gameplay test: 60 FPS target and VSync off, identical in A/B. This cap
  can hide spare throughput, so prioritize tails and stalls over average FPS.
  Optionally repeat both conditions with the same supported higher target such as
  120 and VSync off; keep those results separate. Do not describe a capped 60 FPS
  result as proof of unchanged GPU workload or unlimited headroom.
- Record CPU, RAM, GPU and physical VRAM capacity, driver/OS, storage, launcher
  and tooling versions, pack version/local SHA256, all settings, capture tools and
  versions, sensor identity/units/scope, ambient background apps and capture order.
  Keep power mode and temperatures comparable; let thermal conditions settle.

## Repeatable scenes and captures

Prepare saves for three accessible representative routes. Write down exact save,
start/end landmarks, camera direction, in-game time/weather and route/input notes;
these routes are manual, not deterministic replays. Load the same save every run.
Use only scenes actually unlocked on your PC; don't claim these routes were tested.

| Scene | 60-second warm route | Separate first-traversal observation |
| --- | --- | --- |
| Spargus | Fixed wall/ground view, then a repeatable alley loop | Enter from an adjacent area and pan over props/characters |
| Haven City | Fixed masonry/metal view, then the same street loop | Cross a chosen district boundary and record streaming spikes |
| Wasteland | Vehicle/character close view, then a fixed desert route | First crossing into another loaded area; note dust/alpha/sky artifacts |

For each condition, launch freshly, log idle GPU allocation, load the chosen save,
and record a separate 60-second first-traversal capture including streaming. This
is a **fresh-process first traversal**, not guaranteed cold OS/disk/shader caches.
Do not flush caches for just one condition. Measure boot/load/apply duration
separately with a stopwatch; do not put those intervals in steady gameplay metrics.

Warm the route for at least 60 seconds, then record three 60-second gameplay runs
for each scene/condition, resetting the save/start position consistently. Compare
the median of the three run summaries for each statistic; retain the raw runs.
Use A → B → A when feasible: a restored A detects drift and verifies rollback.
No loading screens, menus, focus loss or capture hotkey delay in warm windows.
If events differ substantially, retain the observation but repeat the comparison.

## Frame-time capture on Windows

Use the official [PresentMon console release](https://github.com/GameTechDev/PresentMon/releases).
Its [console documentation](https://github.com/GameTechDev/PresentMon/blob/main/README-ConsoleApplication.md)
defines the flags below. Use a build supporting `--v1_metrics`; record the actual
version and inspect its help/header. This makes the expected presentation-interval
column `MsBetweenPresents` explicit across releases.

Open PowerShell from your tooling folder with Jak 3 running and find its PID:

```powershell
Get-Process gk | Select-Object Id, Path
$jakProcessId = 12345 # replace with the actual selected game PID each launch
New-Item -ItemType Directory -Force D:/Jak3Bench | Out-Null
./PresentMon.exe --process_id $jakProcessId --v1_metrics --delay 5 --timed 60 --terminate_after_timed --output_file D:/Jak3Bench/baseline-spargus-warm-r1.csv
```

Return focus to the game during the delay; capture only the intended route. Repeat
with descriptive B/scene/run filenames and fresh PIDs after each restart. If ETW
capture reports access denied, follow PresentMon's documented Performance Log
Users setup or run the capture console elevated. Check that rows actually belong
to `gk.exe` and that a meaningful game swap chain was captured. Do not use
`--exclude_dropped` to remove poor frames. OpenGL ETW has less instrumentation
than DXGI: treat these as presentation intervals, not exact GPU render duration
or engine simulation times. An unsupported/empty capture is a failed measurement,
not zero cost. CapFrameX may be used instead if its raw export preserves these
same columns; otherwise use its analysis and state its metric definitions.

## VRAM capture and interpretation

In HWiNFO Sensors-only mode or GPU-Z, log the **dedicated GPU memory allocated /
memory used** sensor for the game GPU at a fixed 200–500 ms interval (use the same
rate in A/B). Record the tool/version, exact sensor name, unit and adapter.
Start before the PresentMon delay, note wall-clock start/stop, and retain only
samples overlapping the 60-second game capture for the summary. Export this
window as comma-separated UTF-8 CSV with dot decimals and unique column names.
Keep the original logger file and timestamp trimming notes as evidence.

Also log shared GPU memory if available, total physical dedicated capacity,
idle desktop allocation and post-load allocation. Report median and peak dedicated
allocation per run, A/B delta, and nominal headroom = capacity minus observed
peak. Device-wide sensors include Windows and other apps; keep those apps fixed
and label scope clearly. Allocated memory is not exact physical residency, and
sampled peaks can miss short spikes. Shared-memory growth or stalls suggest
pressure but do not prove eviction. Never infer VRAM use from the 4.1 GB ZIP.

An uncapped 4x increase in both texture dimensions means 16x base texels; mipmaps
add approximately one third for a full chain. This is a sizing model, not a measured
pack requirement: caps, unchanged textures, level residency and driver allocation
affect actual use. Record GPU sensor data rather than assuming all textures reside
simultaneously or that a specific VRAM capacity is sufficient.

## Summarize without third-party Python packages

From this branch's worktree, after trimming a matching sensor window:

```powershell
py -3 scripts/nextgen/summarize_texture_benchmark.py D:/Jak3Bench/baseline-spargus-warm-r1.csv --process-id 12345 --label baseline-spargus-warm-r1 --vram-csv D:/Jak3Bench/baseline-spargus-warm-r1-vram.csv --vram-column "GPU Memory Allocated [MB]" --vram-unit MB --output D:/Jak3Bench/baseline-spargus-warm-r1.json
```

Replace the sensor header and unit with your actual logger data. MB here means
decimal MB; use MiB if the logger uses binary units. Supported units: MB, MiB,
GiB, bytes. Omit all three VRAM options if no valid memory log exists and report
VRAM as **unmeasured**. Do not fill missing measurements with zero.

The tool rejects multiple swap chains unless you select the game stream using
`--swapchain <exact-address>`. It filters by PID, allows a single initial zero
interval, rejects invalid/nonfinite data, and writes UTF-8 JSON. It does not trim
timestamps, capture telemetry, join logs or decide sensor scope. Inspect the
capture window and chosen stream before analysis. Output includes mean/median,
p95/p99/max milliseconds, interval-derived mean FPS, a defined slowest-1% mean
FPS, counts above 16.667/33.333/50 ms, and optional memory median/peak in MiB.
Small captures make tail statistics weak; retain full 60-second captures.

For each scene report A/B median-of-three values and deltas for mean/p95/p99,
slowest-1% FPS, counts/rates of stalls and peak dedicated allocation. Also report
individual run ranges, shared-memory peak and separate first-traversal results.
Do not pool different scenes, caps or warm/first-traversal runs into one number.

## Visual comparisons and sharpening gate

Take separate **lossless 2560x1440 PNGs** after warming the exact same save/view in
A and B. Avoid capture overlays, motion blur from moving cameras and differently
timed exposure/lighting. Keep HUD status the same. Name pairs by scene, landmark,
condition and version; retain full images plus identically positioned 100% crops.
Compare wall/ground detail, character/vehicle materials, alpha fences/vegetation,
distant oblique surfaces, UV seams, skies/eyes and HUD/menu legibility. The pack's
claimed exclusions are expectations to verify, not coverage guarantees.

Record slow camera pans for shimmer, aliasing, mip transitions and halos; stills
alone cannot establish temporal quality. Note missing/broken textures and any
obvious image-generation artifacts. Screenshots from different viewpoints are
illustrations, not controlled A/B evidence. Keep captures local unless you choose
to share them; the handoff contains no game imagery or mod assets.

Judge clarity at native 1440p with sharpening off. If B is clear and stable, drop
bounded sharpening from the next milestone. If residual softness survives correct
resolution/filtering and matched views, document the offending crops first and
review a separate opt-in experiment using [ROADMAP.md](../../ROADMAP.md). Never
enable driver sharpening just for B or change MSAA to make one condition look better.
