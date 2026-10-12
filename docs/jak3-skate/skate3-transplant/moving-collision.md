# Moving collision for the skateboard: a design (AI-assisted)

Written 2026-10-12 for ticket skate-13. No code comes with this note.

Platforms, doors, vehicles and characters are not obstacles the skateboard
understands. This says what the engine can take, how each kind of object
should be handled, and the tickets that follow from it.

Statements marked *not checked* are what the design assumes; the first two
tickets exist to check them before anything is built on them.

## What the engine can take

From its C interface (`skate_ffi.h` in the work folder):

- **One world of triangles that do not move.** `sk_install_world` puts a
  triangle list in and blocks until it is ready; `sk_queue_world` builds it in
  the background and swaps it in on a later step, a newer request replacing an
  older one. Grind rails are found in the triangles at that moment. There is
  no moving body, no way to name a triangle or say whose it is, and no way to
  say the ground under the skater is moving.
- **A push.** `sk_set_velocity` gives the board and skater a velocity.
- **A knock.** `sk_knock` is Skate's own hit-by-a-vehicle bail: a velocity, an
  optional spin and a lift (onto a hood). It was written for a host game with
  traffic.
- **A restart.** `sk_activate` puts the skater somewhere, standing still.
- **Readback.** Position, velocity, state (riding, air, bail, grind), wheels
  on the ground.

Inside the engine the world is reached through one small interface
(`ExternalQueries`: a line sweep and "triangles near here"). That is where a
second, moving set of triangles could be added. *Not checked:* whether two
sources can answer together, and where the skater's own state would have to be
shifted to carry him.

## What the bridge does today

`skate3-bridge-gather-world` fills the game's collide cache for a square of
5 x 5 cells of 8 m around Jak and hands every triangle to the engine. It asks
for the level's background and for the meshes of obstacles, platforms and
pushers. It gathers again when Jak has moved 8 m, or 6 m up or down, or every
three quarters of a second while any object was in the last gather.

So today:

- A thing that moves is a statue that jumps to its new place up to 0.75 s
  late, plus however long the engine takes to build the world. A lift carries
  the skater in steps, if at all.
- Every one of those refreshes rebuilds the whole world, level included, and
  finds the rails again.
- Vehicles and characters are not asked for, so the skater passes through
  them. Their touching Jak still reaches the game (that is why the bridge has
  a switch that makes the board harmless to bystanders).

Each gathered triangle records which collision shape it came from
(`collide-cache-tri` has `collide-ptr` and `prim-index`), so the bridge can
tell the level's triangles from an object's, and one object's from another's.
It does not use that yet.

## The design, by kind of object

### Doors, gates, crates: still most of the time

Keep them in the one static world, but refresh on change, not on a timer.

- Split the gathered triangles into the level's and the objects'.
- Keep a cheap fingerprint of the objects' triangles (a sum over their vertex
  words). Gather the objects on the same 0.75 s beat, but queue a new world
  only when the fingerprint has changed.
- While it keeps changing (a door in mid swing), refresh as fast as the engine
  finishes building.
- A door that closes on the skater leaves him inside something solid. The
  bridge already puts him back at the last place his wheels were down when the
  engine loses him; a check has to show that this case ends there.

This needs no change to the engine and removes the constant rebuilding near
any object, which is worth having by itself.

### Platforms and lifts: things that carry him

Snapshots cannot do this. However often the world is replaced, the ground
arrives late and in jumps, and the skater has to be moved with it by the same
amount at the same instant.

The design is a small addition to the engine: **moving groups**.

- New calls, roughly: `sk_group_set(session, group, triangles, count)` with
  the triangles in the object's own frame, `sk_group_move(session, group,
  matrix)` once a step, `sk_group_clear(session, group)`. A handful of groups,
  a few hundred triangles each.
- The engine answers its sweeps and nearby-triangle questions from the static
  world and from each group at its current place.
- When the board's contact is with a group's triangle, the engine moves the
  skater and board by that group's movement since the last step (position and
  turn). That is the carrying.
- Groups have no grind rails. Rails are found when a world is installed, and
  a rail that moves is not worth the trouble.
- The bridge gathers each moving object once, turns its triangles into the
  object's frame using its position and rotation at that moment, and from
  then on sends only the matrix each frame. An object whose shape itself
  changes (several parts moving separately) is sent again in full each frame;
  the counts are small.
- Objects that update after Jak in the game's frame would arrive one frame
  late. The bridge sends where the object will be (position plus velocity
  times the step) for those.

The addition lives in the work folder as a patch to the engine, like the spin
names, and never enters the repository.

If the engine cannot take it (the first research ticket says so), the
fallback is to refresh a small world, 3 x 3 cells, every frame with the
blocking call while the board stands on an object, and to shift the skater by
restarting him with his velocity kept. The measurements ticket says whether a
blocking install is fast enough for that. It is the worse design: it loses
his pose for a frame at every shift.

### Pushers and hazards: things that shove him

As geometry they are moving groups, like platforms. A wall that moves into
the skater puts him inside it, and what the engine does then is *not
checked*. So the shove itself comes from the game: when the game sends Jak
its own shove or attack while the bridge drives him, the bridge passes it to
the engine as a knock with the game's direction and strength, and the game's
own rules decide the damage. What reaches Jak in that state today is *not
checked*; the measurements ticket finds out.

### Vehicles

- **Moving:** not geometry. When a vehicle touches Jak faster than a walking
  pace relative to him, `sk_knock` with the vehicle's velocity and a lift
  about the height of its hood. This is the case the call was written for.
- **Parked or crawling:** a moving group made from the vehicle's collision
  mesh, so he can ride up to it, ollie onto it and grind nothing. *Not
  checked:* that vehicles have a mesh the collide cache will hand over (the
  game has a `vehicle-mesh` kind of collision, which suggests so).
- The switch between the two is the vehicle's speed, with a gap between the
  thresholds so it does not flicker.

### Characters: bystanders, guards, enemies

They are spheres to the game and should not become geometry.

- Rolling into a bystander above a set speed: the bystander gets the game's
  own "bumped" reaction without damage, and the skater loses about half his
  speed (`sk_set_velocity`). Above a higher speed he is knocked off
  (`sk_knock`), as in Skate.
- An enemy's attack: the game's damage as usual, and a knock in the attack's
  direction.
- Below the lower speed nothing happens, as now.

## What it turns into

In order. Each is small enough for one Codex ticket. None is created yet.

1. **Measurements** (test). In the forest, the mine, the sewer and Haven's
   port: triangles per gather and how many are objects'; how long a queued
   world takes to arrive and how long a blocking install of 3 x 3 cells takes;
   what kinds of collision vehicles and characters carry; which events reach
   Jak when a vehicle touches him, a bystander is run into and an enemy
   attacks while the bridge is on. A note with the numbers.
2. **Engine research** (read-only). Can the world interface answer from two
   sources; where the skater's state would be shifted to carry him; what the
   engine does when geometry appears around him. A note; no code.
3. **Refresh on change** (bridge). Split level from objects, fingerprint the
   objects, queue a world only when it changed. Check with a door.
4. **Moving groups in the engine** (engine patch and its C calls). Depends
   on 2.
5. **Platforms through moving groups** (bridge). Gather once per object, send
   matrices, drop those objects from the static world. Check: ride a lift up
   and roll off it at the top.
6. **Shoves and attacks become knocks** (bridge). Depends on 1.
7. **Vehicles** (bridge). Knock when moving, group when parked. Depends on 1,
   4.
8. **Characters** (bridge). Bump, slow, knock off. Depends on 1.
9. **Sweep** (test). A scripted ride past each kind in two levels, and a long
   ride in Haven's traffic.

Tickets 3 and 6 need nothing from the engine and can go first.

## Risks

- The collide cache holds 461 triangles per cell. Cells with many objects
  already overflow in places; taking objects out of the static gather helps.
- The engine remembers places to put the skater back after a bail. One on a
  platform that has since left is in mid-air. The bridge's own "last safe
  spot" has the same weakness and should prefer level ground.
- Knocks hurt. Haven's traffic is dense, and being knocked off every few
  seconds would make the city unplayable on the board; the thresholds need a
  person's hands, not only a script.
