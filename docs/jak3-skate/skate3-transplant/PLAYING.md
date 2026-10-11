# Skating in Jak 3 with the Skate 3 bridge (AI-assisted)

Written 2026-10-11. This describes the opt-in bridge that lets an external
Skate 3 gameplay engine drive Jak while he is on the jetboard. Nothing from
that engine or from Skate 3 is in this repository.

## What you need

1. This branch built for Windows (`gk.exe`, `goalc.exe`) and Jak 3 NTSC
   (SCUS-97330) extracted and compiled.
2. `skate_ffi.dll`, built for 64-bit from the `skate-ffi` crate of
   `ryglizzy/GTA-San-AnSkateas` (a C interface over the Skate 3 Rust engine
   carried in `chasmlol/2010-rust-rewrite-mashup`).
3. Skate 3 data converted from your own Xbox 360 copy with the mashup's
   converter (`iw4l_skate_convert.py`), giving an `assets` folder.
4. Two environment variables set before starting `gk`:
   - `OPENGOAL_SKATE3_DLL`: full path to `skate_ffi.dll`
   - `OPENGOAL_SKATE3_ASSETS`: full path to that `assets` folder

With either variable unset the game behaves exactly as before. With both set,
riding the jetboard is skateboarding, and Jak is given the jetboard if he does
not have it yet.

The build and conversion steps used on the development machine are recorded in
[README.md](README.md).

## Controls

| Pad | Does |
| --- | --- |
| R2, on foot | Get on the board (the jetboard's own button) |
| Triangle, on the board | Get off |
| Hold the right stick in, click the left stick | On the board: switch between skateboard and jetboard |
| Left stick | Steer, lean |
| Right stick | Flickit: pull down then flick up to ollie; other flicks are Skate 3's tricks |
| Cross | Push (Skate 3's A button) |
| Square, Circle | Passed through as Skate 3's X and B buttons |
| L2, R2 | Passed through as Skate 3's triggers (its grabs) |
| L1, R1, D-pad | Passed through as Skate 3's bumpers and D-pad (L1 + D-pad is its session marker) |

Only the right-stick ollie and kickflip and Cross to push have been exercised,
and those with scripted input. The other buttons are passed to the engine
unchanged and do whatever Skate 3 does with them.

The first time the skateboard is used in a session the game freezes for about
ten seconds while the engine loads.

Trick names and Skate 3's score are shown as plain text; set
`*jak3-skate3-bridge-hud*` to `#f` in a REPL to hide them.

## What works

Checked on Jak 3 NTSC in Spargus, with scripted input and screenshots:

- Riding, pushing and steering on the level's own collision, following slopes.
- Skate 3's Flickit recognising tricks (ollie, kickflip) and naming them.
- Jak in the Skate 3 skater's poses, on Skate 3's own skateboard model, with
  Daxter on his shoulder.
- Bails: Jak ragdolls, the board tumbles away, and he is put back on it.
- Switching to the jetboard and back, and getting off and on again.

## Known limits

- **Not tried with a real controller.** Everything above was driven by scripted
  input. The pad mapping is written but untested.
- **Only the level's solid background is known to the engine.** Water, moving
  platforms, doors, vehicles and characters are not obstacles to the
  skateboard, and it cannot ride on them. If Jak drops 30 m below where the
  board last touched down the bridge switches off and the jetboard takes over.
- **Jak can still touch things.** Triggers, pickups and characters react to him
  as usual; skating into citizens can make Spargus's guards open fire.
- **Grinds are untested.** The engine finds grindable edges in the level
  (about 80 to 135 around Jak in Spargus) but no grind was performed.
- **No skateboard sounds.** The jetboard's sounds are silenced while skating and
  nothing replaces them.
- **The camera is Jak's own**, not Skate 3's, and the right stick no longer
  turns it while skating.
- **Jak's head and fingers do not move**; they ride on his chest and hands.
- **Missions and scripted sequences have not been tried.**
- **Frame rates other than 60** have only been exercised through the clock path
  at 60; the engine is stepped from game time at its own fixed rate.

## Manual checks

With `gk` running in debug mode and a REPL connected and left open (`goalc`
resets the game when it disconnects):

```lisp
(make-group "iso")
(lt)
(ml "goal_src/jak3/pc/debug/skate3-bridge-test.gc")   ; push and ollie, logged
(ml "goal_src/jak3/pc/debug/skate3-bridge-look.gc")   ; screenshots of the poses
(ml "goal_src/jak3/pc/debug/skate3-bridge-cycle.gc")  ; toggling, kickflip, bail, dismount
```

Each prints `SK3TEST` lines to the `gk` console; the last two also save
`sk3-*.png` to the user screenshots folder.
