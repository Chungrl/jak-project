# Windows texture-pack handoff (AI-assisted)

Use the existing launcher for the pack. This Git branch adds analysis tooling and
documentation; no custom OpenGOAL build is needed for installation. Keep PR #1's
jetboard installation separate when benchmarking.

## Get this work without disturbing the lab

In your existing fork clone, from PowerShell:

```powershell
git fetch origin master jak3-texture-overhaul-workflow
git worktree add -b jak3-texture-overhaul-desktop ../jak3-texture-overhaul-desktop origin/jak3-texture-overhaul-workflow
Set-Location ../jak3-texture-overhaul-desktop
py -3 -m unittest discover -s scripts/nextgen -p "test_*.py" -v
```

Use another unused worktree/branch name if that name already exists. No reset,
merge into master or cherry-pick from PR #1 is required. Python 3.9+ is sufficient
for the analysis script; the cloud tests used Python 3.11. A source build is
optional and does not replace the launcher's active installation automatically.

## Prepare a recoverable install

1. Start a working launcher-managed Jak 3 installation from your legally obtained
   game data. Record launcher and installed OpenGOAL/tooling version, GPU/driver,
   and current texture-pack list/order. Back up saves and settings with the game
   closed. Locate the actual launcher installation directory through its settings;
   do not assume a fixed AppData path.
2. Capture an original-texture reference first: disable all Jak 3 texture packs,
   **Apply Texture Changes**, wait for successful decompilation, then launch anew.
   Save a lossless 1440p screenshot of a recognizable textured wall.
3. Obtain version 1.0.0 manually on your PC from
   [Cioscos' page](https://www.nexusmods.com/opengoal/mods/31). The cloud agent did
   not download the 4.1 GB ZIP. Keep it outside the Git checkout. Record your local
   filename/version and hash; there is no agent-supplied archive hash to compare:

   ```powershell
   Get-FileHash -Algorithm SHA256 "D:/Downloads/your-pack.zip"
   ```

4. Allow room for the ZIP, extracted stored pack, a second active replacement
   copy and regenerated game data, plus temporary work. Archive size is not the
   installed footprint. About 20 GB of additional free space is a planning buffer,
   not a measured guarantee; check actual sizes after import and keep more if needed.

## Install / enable

Close Jak 3. Open **Launcher → Jak 3 → Features → Texture Packs → Add New Pack**,
select the unmodified ZIP, wait for import, enable its card and click
**Apply Texture Changes**. Wait for every job, including decompilation, to succeed.
Then launch a fresh game process, load the same save and reproduce the wall view.
For the initial comparison enable only this pack. The top enabled pack wins if
multiple packs replace a file; record ordering when adding another pack later.

This is an extraction-time replacement, not a runtime hot toggle. Changing a
checkbox without applying and relaunching is insufficient. No Vortex setup,
manual shader edit, texture conversion or second loader is required.

## Disable / remove / recover

- **Disable:** close the game, switch the card off, Apply Texture Changes, wait for
  successful regeneration, relaunch and check the original wall. Disable every
  pack for a genuinely original-texture baseline. An inactive card alone does not
  establish that the generated data was restored.
- **Remove:** first disable and verify restoration. Mark the disabled card for
  deletion and Apply Texture Changes again. Keep the original ZIP on your PC if
  you want to reinstall. Imported packs and active assets are separate copies.
- **Failed import or apply:** save the launcher/decompiler logs and the error,
  check free disk space, and avoid launching a partially regenerated install.
  Try the same disable/apply restoration first. If it fails, use the launcher's
  normal reinstall/setup flow from your own game data, then restore backed-up
  saves/settings. Do not delete arbitrary AppData or mix extracted assets between
  different OpenGOAL versions. If the Texture Packs entry is missing, use a launcher
  release that supports Jak 3 texture packs and record the changed version.

Diagnostic paths relative to the launcher-selected install directory, established
by the inspected launcher source (release layouts can differ):

```text
features/jak3/texture-packs/<zip-stem>/custom_assets/jak3/texture_replacements
active/jak3/data/custom_assets/jak3/texture_replacements
active/jak3/data/out/jak3/fr3
```

The launcher manages these paths. Do not copy the pack into the source checkout
expecting it to alter a different launcher installation.

## Measure and decide

Follow [TEXTURE_BENCHMARK.md](docs/jak3-nextgen/TEXTURE_BENCHMARK.md) for baseline
versus 4x at 2560x1440, PresentMon frame-time captures, VRAM logging and the exact
Python summary command. Raw captures and screenshots stay outside Git.
All desktop import/rollback, visual, frame-time and VRAM checks are still pending.
The cloud tests verify tooling and existing automated regressions, not the pack.

Keep sharpening off, including driver/ReShade sharpening, for this comparison.
Review the pack before considering any new graphics feature. Return the run
metadata, summary JSON files, raw capture logs and paired screenshots for the
next engineering decision; do not upload or redistribute the pack itself.
