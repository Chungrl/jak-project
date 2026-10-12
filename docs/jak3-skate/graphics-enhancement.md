# Image enhancement for Jak 3 (AI-assisted)

Written 2026-10-12. An optional set of renderer effects for Jak 3 on top of the
original picture. It is on by default on this branch and can be switched off
at run time, which gives back the original picture exactly.

## What it adds

| Effect | What you see | How it is made |
| --- | --- | --- |
| Ambient occlusion | Creases, corners and the ground under things are darker, so objects sit in the world | From the depth buffer |
| Character shadows | Jak, NPCs and enemies cast a short shadow away from the sun | By walking the depth buffer towards the sun; only characters cast |
| Surface relief and shine | Textures look less flat under the sun | From the brightness of the picture: darker is treated as deeper |
| Rim light | A light edge on the sunward side of characters | From the depth buffer and the sun direction |
| Bloom | Lamps, eco and other bright things glow | The bright parts of the picture, blurred and added back |
| Grading | A little more contrast and colour, sharper detail, slightly darker corners | In the final pass |

The sun is the strongest directional light of the game's current mood, which
the game code hands to the renderer every frame.

The HUD is drawn after the shading and is left out of the bloom. It does go
through the grading, because that is the last thing done to the whole picture.

## Switching it off and tuning it

From the REPL:

```lisp
(pc-set-post-enhance 0)    ;; off: the original picture
(pc-set-post-enhance 100)  ;; as designed (the default), up to 200
(pc-set-post-param 2 800)  ;; one part, in thousandths
```

Parts for `pc-set-post-param`: 0 ambient occlusion, 1 its radius in metres,
2 bloom, 3 bloom threshold, 4 sharpening, 5 contrast, 6 saturation, 7 vignette,
8 rim light, 9 debug view (1000: the added shading alone, 2000: distance
bands), 10 character shadows, 11 surface relief, 12 surface shine.

The same controls are sliders in the debug build: Alt, then
Debug > Renderer Debug > Image Enhancement. That window also shows the
measured GPU time.

Neither the strength nor the parts are saved between runs yet.

## Cost

Measured with GPU timer queries on the development machine (RTX 5070,
2560x1440, 16x MSAA): 1.77 ms for the shading and 0.25 ms for the bloom per
frame. It has not been measured on any other hardware.

## What was checked

`goal_src/jak3/pc/debug/gfx-enhance-compare.gc` loads five start points
(desert, Haven industrial, forest, sewer, Haven port), and for each saves the
window with the enhancement off, on, the added shading alone, and the distance
the renderer reads back. On the last one it also saves the pause menu. Those
pictures were looked at for every change. Jak on the skateboard in Spargus was
looked at as well, and the skate bridge's own check still passes.

Not checked: cutscenes, night, underwater, dark and light Jak effects,
vehicles, most levels, and anything other than this one machine.

## Known limits

- Shadows come only from characters that are on screen, and reach about 2.6 m.
  The world's own shadows stay the ones painted into it.
- The relief is not real normal mapping. It follows the brightness of the
  picture, so a painted highlight reads as a bump.
- The shading fades out between 70 m and 170 m.
- Where two pieces of ground overlap with a small step, the ambient occlusion
  can still draw a faint line.
- Effects that draw the previous frame back into the picture (heat haze and
  the like) show it with its shading already in. The grading and bloom are
  kept in the final pass so that they are never applied twice.

## Where it lives

- `game/graphics/opengl_renderer/PostFx.{h,cpp}`: the passes.
- `game/graphics/opengl_renderer/shaders/postfx_*.frag`, `post_processing.frag`.
- `OpenGLRenderer.cpp`: the three places in the Jak 3 bucket order where the
  passes run, and the final pass.
- `game/kernel/common/kmachine.cpp`: `pc-set-post-enhance`,
  `pc-set-post-param`, `pc-set-sun-light`, `pc-screen-shot-window`.
- `goal_src/jak3/pc/pckernel.gc`: hands the sun to the renderer each frame.

`pc-screen-shot-window` saves what the window shows, final pass included.
The existing `pc-screen-shot` reads the picture before the final pass, so it
shows the shading but not the bloom or grading.
