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

Checked on Jak 3 NTSC (SCUS-97330) through the manual checks listed below,
watching the game's log and screenshots:

- **Real input.** Key presses sent to the game window, which the game binds to
  the pad, mounted the board (R2), pushed to about 11.9 m/s (Cross), steered
  (left stick), did an ollie and a kickflip from right-stick gestures, switched
  to the jetboard and back, and dismounted (Triangle), twice over.
- **Riding** on the level's own collision in Spargus and on Haven City's port,
  following slopes and stopping at walls.
- **Tricks** recognised, named and scored by Skate 3's own logic: ollie,
  kickflip.
- **Grinds.** Restarted above grind rails the engine had found in Spargus, the
  skater ground 9 or 10 of 30 of them in most runs (2 of 30 in one), named by
  the engine as 50-50, 5-0, feeble, nose, boardslide and blunt slide, the
  longest lasting 134 frames.
- **The look.** Jak is in the Skate 3 skater's poses, on Skate 3's own
  skateboard model, with Daxter on his shoulder.
- **Bails.** Jak ragdolls, the board tumbles away, and he is put back on it at
  the last place the wheels were down.
- **Water.** Riding off a pier in Haven City's port, the bridge handed over to
  the jetboard at the water's surface and Jak rode on across it.
- **Hand-overs.** Switching to the jetboard and back, getting off and on again,
  and dying and respawning, all leave the game in a normal state.

## Known limits

- **No physical controller has been used.** Key presses reach the same pad
  code a controller does, but they are all-or-nothing, so partial stick
  deflection, and how the Flickit gestures feel by hand, are untested.
- **Only the level's solid background is known to the engine.** Moving
  platforms, doors, vehicles and characters are not obstacles to the
  skateboard, and it cannot ride on them. If Jak drops 30 m below where the
  board last touched down the bridge switches off and the jetboard takes over.
- **Jak can still touch things.** Triggers, pickups and characters react to him
  as usual; skating into citizens can make Spargus's guards open fire.
- **Grinding by hand is untested.** The grinds above started from a restart
  placed over the rail, not from an ollie onto it.
- **No skateboard sounds.** The jetboard's sounds are silenced while skating and
  nothing replaces them.
- **The camera is Jak's own**, not Skate 3's, and the right stick no longer
  turns it while skating.
- **Jak's head and fingers do not move**; they ride on his chest and hands.
- **Missions, cutscenes and levels other than Spargus and the port have not
  been tried.**
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
(ml "goal_src/jak3/pc/debug/skate3-bridge-pad.gc")    ; pad translation
(ml "goal_src/jak3/pc/debug/skate3-bridge-grind.gc")  ; grinds on the engine's rails
(ml "goal_src/jak3/pc/debug/skate3-bridge-explore.gc"); Spargus slope, then Haven's port and its water
(ml "goal_src/jak3/pc/debug/skate3-bridge-observe.gc"); watches while you play
```

`skate3-bridge-grind.gc` needs an engine library that also exports
`sk_debug_rail_count` and `sk_debug_rail` (a small local addition to
`skate-ffi`); with a stock library it reports that and stops.

Each prints `SK3TEST` lines to the `gk` console; the last two also save
`sk3-*.png` to the user screenshots folder.
