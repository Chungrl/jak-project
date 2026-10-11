# Jak 3: Next Generation — texture workflow (AI-assisted)

Date: 2026-10-11. Fork: https://github.com/Chungrl/jak-project.
Branch: `jak3-texture-overhaul-workflow`, based directly on fork master
`efb21c3e8c5a40a74f2e86510f915da55eaeba34`.
[PR #1](https://github.com/Chungrl/jak-project/pull/1) and its jetboard changes
remain on `jak3-nextgen-lab`; this branch contains no gameplay or shader changes.

## Decision

Use the existing OpenGOAL launcher Texture Packs feature. No additional engine
integration is indicated by the published pack instructions or inspected code.
The practical first visual upgrade is to install and evaluate this pack locally.
Bounded sharpening is deferred; no sharpening, anisotropy or texture loader feature
has been added. See [ROADMAP.md](ROADMAP.md) for the decision gate.

## Pack facts and limits

[Cioscos' Nexus description](https://www.nexusmods.com/opengoal/mods/31) reports
version 1.0.0, 15,920 replacements, 4x upscaling with a 2048-pixel cap, preserved
alpha, and unchanged UI, skies/clouds, eyes and animated textures.
The [Files page](https://www.nexusmods.com/opengoal/mods/31?tab=files) lists 4.1 GB.
These are author claims, not archive validation. The archive was not downloaded,
opened or redistributed. No game assets are included in this branch or handoff.

## Integration evidence

OpenGOAL source at the baseline above:

- `decompiler/decompilation_process.cpp`: registers
  `custom_assets/jak3/texture_replacements` before level extraction; replacements
  take priority over texture merges.
- `decompiler/data/TextureDB.cpp`, `replace_texture` / `resolve_texture`: accepts
  page-specific PNGs, falls back to `_all/<name>.png`, and uses replacement width,
  height and RGBA data. No 4x-specific switch is necessary.
- `game/graphics/opengl_renderer/loader/LoaderStages.cpp`, `add_texture`: uploads
  actual dimensions, generates mipmaps and requests available maximum anisotropy.
  Texture upload work has per-frame budgets; one large upload can exceed the byte
  budget. Check traversal stalls before considering any budget change.
- `game/graphics/texture/TexturePool.cpp`: existing RGBA upload/filtering support.

External launcher source was inspected read-only at
[`a32643d9676e0cc4d61df930c27bdf6c83fc14d1`](https://github.com/open-goal/launcher/tree/a32643d9676e0cc4d61df930c27bdf6c83fc14d1),
whose package declares 2.11.2. This is a source snapshot, not a tested Windows release.

- `src-tauri/src/commands/features/texture_packs.rs`: checks the ZIP's expected
  `custom_assets/jak3/texture_replacements` prefix, stores packs under
  `<install>/features/jak3/texture-packs`, and resets the active replacement
  directory before copying enabled packs. Earlier packs in the list win conflicts.
- `src/lib/job/texturePackJob.ts`: Apply saves selection, rebuilds replacements,
  then runs the decompiler. An empty enabled list also regenerates original data.

Conclusion: source compatibility is supported; successful import, full coverage,
visual quality and GPU performance require the Windows checks in
[DESKTOP_HANDOFF.md](DESKTOP_HANDOFF.md) and
[the benchmark protocol](docs/jak3-nextgen/TEXTURE_BENCHMARK.md).

## Delivered

A Windows procedure and an asset-free Python CSV summarizer with regression tests.
[TEST_RESULTS.md](TEST_RESULTS.md) records what was actually run. Frame time, VRAM
and screenshot results remain pending; this investigation supplies no measured
claim that the pack is faster, fits a particular GPU, or eliminates blur.
