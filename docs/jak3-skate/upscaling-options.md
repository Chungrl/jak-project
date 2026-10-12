# Upscaling options for Jak 3 (AI-assisted)

Written 2026-10-12 for ticket gfx-06. No code comes with this note. It answers
three questions: what rendering below the window's resolution and scaling up
would take in this renderer, what DLSS would need, and whether any of it is
worth building.

## The short answer

- Speed is not this game's problem on a modern graphics card, so the reason to
  build any of this is a steadier, cleaner picture, not a higher frame rate.
- The cheapest real gain is the opposite of upscaling: draw the game larger
  than the window and shrink it. The renderer can already do that; it needs a
  better shrinking filter and a setting.
- DLSS cannot attach to OpenGL. It can still be reached by sharing images
  with a small Vulkan side, but the hard part is not the attaching: DLSS needs
  to be told how every pixel moved since the last frame, and this renderer
  does not know. Building that knowledge is most of the work, and the same
  work gives a home-made temporal anti-aliasing that runs on every card.
- Recommended order: measure, then supersampling with a good filter, then
  temporal anti-aliasing of our own, and DLSS only if that is not good enough.

## What the renderer does today

- OpenGL 4.3 core (4.1 on macOS). 52 vertex shaders.
- The game is drawn into its own buffer at the "game resolution"
  (`game_res_w`/`game_res_h`, set from game code with
  `pc-set-game-resolution`), with multisampling if asked for, then resolved to
  a single-sample buffer.
- The final pass (`post_processing.frag`) reads that buffer and writes the
  window's draw region. The two sizes are already independent, so drawing at
  half size and stretching, or at double size and shrinking, works today. The
  stretch is plain bilinear followed by the sharpening in the grading.
- The HUD and text are drawn into the same buffer as the world, late in the
  frame (from bucket `TEX_HUD_HUD_ALPHA` on).
- Nothing is kept from the previous frame: no previous camera, no previous
  bone positions, no per-pixel motion.
- Vertices reach the screen by several routes. Level geometry (tfrag, tie,
  shrub) and characters (merc) are transformed in shaders from a camera matrix
  and, for characters, bone matrices. Particles, the ocean, the sky, "generic"
  draws, text and the HUD arrive already in screen positions, computed by the
  game code or by C++ that imitates the PlayStation 2's vector unit.

## What was measured, and what was not

- The image enhancement costs 1.76 ms for the shading and 0.24 ms for the
  bloom per frame, at 2560x1440 with 16x multisampling on an RTX 5070.
- The total time the graphics card spends on a frame was never measured, nor
  how it changes with the multisampling level or the game resolution. Without
  those two numbers, any claim about what upscaling would save is a guess.
  Measuring them is the first ticket below.

For scale: a frame at 60 per second is 16.7 ms. The enhancement is about an
eighth of that. The game being drawn is from 2004.

## The options

### 1. Supersampling: draw larger, shrink well

- **What it is:** set the game resolution above the window's (1.5x to 2x per
  side) and shrink in the final pass.
- **Needs:** a shrinking filter that averages properly at any ratio (today's
  bilinear is only right at exactly 2x), a "render scale" setting, and the
  enhancement's radii and thresholds checked at the larger size.
- **Cost to build:** small. One ticket, final pass and settings only.
- **Cost to run:** roughly proportional to the pixel count for the world's
  shading and for the enhancement (about 4.5 ms at 1.5x per side and 8 ms
  at 2x for the enhancement, if it scales with pixels). To be measured.
- **Gain:** removes the shimmer multisampling cannot: alpha-tested leaves and
  fences, texture sparkle, and the edges of the relief and shine effect. It
  can replace part of the multisampling (4x plus supersampling in place of
  16x).

### 2. NVIDIA's DLDSR: the same idea, done by the driver

- **What it is:** a driver setting. The desktop offers resolutions above the
  monitor's (1.78x and 2.25x the pixel count) and the driver shrinks the
  picture with a neural filter. It works at the display, so it does not care
  that the game uses OpenGL.
- **Needs:** an RTX card and a switch in the NVIDIA settings; the game then
  runs fullscreen at the larger resolution. No code.
- **Cost to build:** none. A paragraph in `PLAYING.md`.
- **Gain:** the closest thing to "DLSS quality" available today on the owner's
  card. Worth trying before anything else is built, because it shows how much
  a steadier picture is worth on this game.
- **Limits:** NVIDIA only; the whole screen, HUD included, goes through it.

### 3. Spatial upscaling: draw smaller, stretch cleverly

- **What it is:** game resolution below the window's, with an edge-aware
  stretch in the final pass (AMD's FSR 1 is two shader passes under the MIT
  licence and needs nothing but the picture).
- **Needs:** the two passes in the final pass, the render scale setting from
  option 1, and the HUD drawn after the stretch if it is to stay sharp (see
  "The HUD" below).
- **Cost to build:** small without the HUD change, medium with it.
- **Gain:** frame rate on weak or integrated graphics, at a visibly softer
  picture than native with multisampling. Nothing on the owner's machine.
- **Verdict:** only worth it as a by-product of option 1's setting, for
  players on laptops.

### 4. Temporal anti-aliasing and upscaling of our own

- **What it is:** each frame is drawn shifted by a fraction of a pixel, and
  blended with the previous frames moved to where they are now. Over several
  frames every pixel has been sampled many times. Drawn below the window's
  resolution, the same method upscales.
- **Needs:**
  1. *The shift.* A tiny offset added to the final position in every vertex
     shader. It is the same line in each of the 52, from one shared value.
  2. *Where each pixel was.* For anything that does not move by itself, this
     comes from the depth buffer and last frame's camera (the enhancement
     already rebuilds positions from depth). That covers the level.
  3. *Motion of things that move.* Characters need last frame's bones sent
     alongside this frame's and a second output from their shader. Particles,
     the ocean and other screen-position draws have no previous position to
     give; they are either left out of the blend (drawn after it) or accepted
     with some smearing.
  4. *The blend itself:* history buffer, rejection of stale history
     (neighbourhood clamp), and the texture detail bias that sharper sampling
     wants.
  5. *The HUD after the blend* (see below).
- **Cost to build:** medium to large: four or five tickets, and the most
  tuning of anything here, because the failure is ghost trails behind moving
  things and those have to be hunted scene by scene.
- **Cost to run:** a few tenths of a millisecond to about a millisecond; it
  would replace 16x multisampling, which is likely to cost more than that.
- **Gain:** steadier picture than multisampling on leaves, fences, thin wires
  and shader effects; works on every card and on every platform the port
  supports. It is also everything DLSS needs as input.
- **Other ready-made temporal upscalers:** AMD's FSR 2 and later and Intel's
  XeSS ship for Direct3D and Vulkan only. An unofficial OpenGL port of FSR 2
  exists; taking it would trade writing the blend for owning a large foreign
  code base. The inputs (1 to 3 and 5) are needed either way.

### 5. DLSS, or DLAA

- **What it is:** NVIDIA's neural version of option 4's blend. DLAA is the
  same thing at full resolution, used purely as anti-aliasing, which is the
  form that suits this game.
- **Needs:**
  - Everything in option 4 except the blend itself: the shift, per-pixel
    motion, depth, and the HUD drawn afterwards.
  - A way in. The DLSS library accepts Direct3D 11, Direct3D 12 and Vulkan.
    From OpenGL the route is a second, small Vulkan device that shares the
    colour, depth and motion images with OpenGL through the memory-sharing
    extensions (`GL_EXT_memory_object`, `GL_EXT_semaphore` and their Windows
    forms, which NVIDIA's drivers have), runs DLSS there, and hands the result
    back. The other route, moving the whole renderer to Vulkan, is a rewrite
    and is not proposed.
  - NVIDIA's library and its licence terms. It is a closed binary. Like the
    Skate engine, it would have to stay outside the repository and be loaded
    only when present, and it could never be part of the upstream project.
  - An RTX card. Everyone else needs option 4 anyway.
- **Cost to build:** large, and only after option 4's inputs exist: the
  Vulkan side and image sharing, the library's set-up, and its own tuning.
  Hard to test for anyone without the card.
- **Gain over option 4:** a better blend, especially on thin and fast-moving
  things, and less tuning of ghosting by hand. No frame rate gain that
  matters here.

## The HUD

Every option that changes the picture's size or blends frames wants the HUD
and text drawn afterwards at the window's resolution, or they come out soft or
smeared. Today they share the world's buffer. The split point already exists
(the enhancement gathers its bloom just before the HUD buckets), but the HUD's
draws assume the game buffer's size, so the rest of the frame has to be
pointed at a second buffer with its viewport and scissor rescaled. One medium
ticket, shared by options 3, 4 and 5.

## Is any of it worth doing?

| Option | Build | Who gains | Worth it |
| --- | --- | --- | --- |
| 1 Supersampling | small | anyone with a spare graphics card | yes, first |
| 2 DLDSR | none | RTX owners | yes, try it today |
| 3 Spatial upscaling | small | weak graphics only | as a by-product of 1 |
| 4 Own temporal anti-aliasing | medium to large | everyone | yes, if shimmer still bothers after 1 |
| 5 DLSS / DLAA | large, after 4 | RTX owners | only if 4's blend is not good enough |

The honest case for DLSS on this game is the name. What it would add over a
well-tuned option 4 is real but small, it costs the most, and it helps one
brand of card. What makes the picture look current is steadier edges and no
shimmer, and options 1, 2 and 4 get most of that.

## Tickets this turns into

In order. None is created yet.

1. **Frame cost by setting** (test). GPU time for the whole frame, by
   multisampling level (1, 4, 16) and game resolution (1x, 1.5x, 2x per side)
   at the five comparison spots. A table in this note.
2. **Render scale** (code, small). A setting from 50% to 200%, a proper
   shrinking filter in the final pass, the enhancement checked at each size.
   A paragraph on DLDSR in `PLAYING.md`.
3. **Edge-aware stretch below 100%** (code, small). FSR 1's two passes.
4. **HUD after the final scaling** (code, medium).
5. **The shift and camera reprojection** (code, medium). The offset in every
   vertex shader, last frame's camera kept, a temporal blend for the level
   with moving things left out of it.
6. **Character motion** (code, medium). Last frame's bones and a motion
   output for merc.
7. **Tuning sweep for ghosting** (test). Particles, water, fast camera turns,
   cutscenes.
8. **DLSS through Vulkan** (code, large; needs the owner's decision first).
