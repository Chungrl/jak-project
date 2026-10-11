# Texture-first graphics roadmap (AI-assisted)

This branch is independent of the jetboard lab in PR #1. Keep gameplay changes
out of texture comparison runs so they do not change traversal or camera timing.

1. **Local integration:** use the launcher, record launcher/tooling versions and
   the locally obtained pack checksum, import and apply the ZIP. Verify a visible
   world texture replacement, then disable/apply/restart and verify restoration.
2. **1440p evidence:** run the baseline/4x protocol in
   [TEXTURE_BENCHMARK.md](docs/jak3-nextgen/TEXTURE_BENCHMARK.md). Collect paired
   lossless views, three warm captures per route/condition, separate first-traversal
   captures, frame-time tails, allocated dedicated GPU memory and shared memory.
3. **Adoption:** retain the pack only when its visual benefit, installation success
   and measured frame pacing/memory cost are acceptable on the target Windows PC.
   Investigate regressions first. Leave upload budgets and renderer code unchanged
   until captures identify a specific engine problem.
4. **Sharpening decision:** review matched 100% crops and slow camera pans at native
   2560x1440, with correct internal resolution and consistent MSAA. The upscale
   already adds apparent detail; extra sharpening may duplicate that effect and
   produce ringing, alpha-edge halos and temporal shimmer. It cannot improve
   geometry or restore detail absent from the original textures.

Bounded sharpening remains a possible experiment only if reviewers identify
persistent, reproducible softness after these controls. The existing
`OpenGLRenderer::do_pcrtc_effects` resolves MSAA and performs the final fullscreen
pass; `post_processing.frag` applies brightness/contrast. A change there also
affects HUD/menu pixels and requires GPU validation. Any later prototype should
be Jak 3-specific, opt-in, strength zero by default, clamped against overshoot,
and justified by paired visual and timing captures. No implementation is approved
by evidence from this asset-free cloud investigation.

If the pack looks clear at native 1440p, retire sharpening from the near-term
graphics plan. Existing anisotropic filtering and MSAA already exist; adding a
second implementation would be redundant. HDR, material/normal data and modern
lighting remain separate, larger investigations after texture quality and memory
cost are understood. Physics grappling/portals retain their long-term place in
the gameplay roadmap on separate branches.
