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
- **Riding** on the game's own collision, following slopes and stopping at
  walls, in Spargus, Haven City's port and industrial section, the desert, the
  forest, the sewer, the stadium and the mine (`skate3-bridge-levels.gc`: 3 to
  55 m ridden in four seconds of pushing at each, the short ones ending at a
  wall or in a bail).
- **Not floating.** With Jak on the skateboard his lower foot joint measured
  0.09 to 0.28 m above the ground under him across those levels; the jetboard
  carries him about a metre up.
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
  and dying and respawning, all leave the game in a normal state. When a
  cutscene or script grabs Jak the bridge lets go and picks up again afterwards.
- **Left and right are not mirrored.** Holding the left stick right turns Jak
  towards his own right-hand side, and his left foot leads on the board, the
  regular stance. A third of the stick turned him less than the full stick,
  and a Flickit ollie with the stick only three quarters of the way out still
  popped.

## Known limits

- **No physical controller has been used.** Key presses reach the same pad
  code a controller does, but they are all-or-nothing, so partial stick
  deflection, and how the Flickit gestures feel by hand, are untested.
- **Collision is the level plus solid objects.** The engine is given the
  background and the meshes of obstacles, platforms and pushers around Jak,
  refreshed every three quarters of a second while any are near. Riding on a
  moving platform, and doors, have not been tried. Vehicles and characters are
  not obstacles to the skateboard. If Jak drops 30 m below where the board last
  touched down he is put back there and given the jetboard.
- **Jumping between levels by script is rough.** When the level check reached
  the mine straight from a bail in the forest, Jak did not get on the board or
  the bridge switched off; loaded on its own the mine rode normally, twice.
  When the bridge switches itself off it now prints the reason to the console.
- **The engine's own walking mode is not used.** If the engine takes the skater
  off the board by itself, Jak gets off the board.
- **Jak can still touch things.** Triggers, pickups and characters react to him
  as usual. The jetboard damages anything it touches, which in Spargus turns
  the citizens hostile and gets Jak shot; while the bridge drives the board
  that damage is switched off (`*jak3-skate3-bridge-harmless*`), and a
  50-second ride past them then cost him no health.
- **Grinding from an ollie is hard to hit by script.** Most of the grinds above
  started from a restart placed over the rail. Rolling at a rail and doing an
  ollie by script, without knowing which side of it is the low one, ended in a
  grind in 2 of 184 approaches (an over-willy for 6 frames and a 50-50 for 47);
  whether a person finds it easier is untested.
- **No skateboard sounds.** The jetboard's sounds are silenced while skating and
  nothing replaces them.
- **The camera is Jak's own**, not Skate 3's, and the right stick no longer
  turns it while skating.
- **Jak's head and fingers do not move**; they ride on his chest and hands.
- **Missions and real cutscenes have not been tried.** Only a scripted grab
  and release, as cutscenes do to Jak, was. Pausing and unpausing mid-ride with
  the Start key left Jak where he was, at the same speed, with the bridge
  still driving.
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
(ml "goal_src/jak3/pc/debug/skate3-bridge-steer.gc")  ; mirror check, partial stick, cutscene grab
(ml "goal_src/jak3/pc/debug/skate3-bridge-ollie-grind.gc") ; roll at rails and ollie onto them
(ml "goal_src/jak3/pc/debug/skate3-bridge-levels.gc") ; six more levels, with foot height above ground
```

`skate3-bridge-grind.gc` and `skate3-bridge-ollie-grind.gc` need an engine library that also exports
`sk_debug_rail_count` and `sk_debug_rail` (a small local addition to
`skate-ffi`); with a stock library it reports that and stops.

Each prints `SK3TEST` lines to the `gk` console; the last two also save
`sk3-*.png` to the user screenshots folder.
