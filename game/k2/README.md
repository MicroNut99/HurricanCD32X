# Added enemy kinds (P1d)

P1b's engine (`game/p1.c`) runs 20 object kinds of its own. The kinds here are added to it the
same lean way: no new pools, no particles, the engine's own shots and effects. (P2 ported all of
Hurrican's object code with its whole framework and ran at 4-5 fps on the console; these do not.)

A group of kinds is two files:

* `<group>_list.h` - one line per kind:

      K2(NAME, id, "picture.png", l, t, r, b, energy, anim_end, anim_per, touch, score, mirror, testblock, shootable)

  `NAME` an upper-case C name (the kind is `E_NAME`, its picture `P_NAME`); `id` the map's object
  number (Hurrican's GegnerClass.cpp PushGegner), or -1 for a kind only spawned by another;
  the picture's name as in `tools/data/hurrican_textures.txt` (only its first 16 letters are
  compared); `l, t, r, b` the collision box (Hurrican's GegnerRect, full-size pixels);
  `energy` Hurrican's Energy; `anim_end` the last frame of the plain loop and `anim_per` steps
  per frame (`AnimSpeed * 6`) - for your own `anim_loop` calls, the engine does not animate;
  `touch` damage per second while the player touches it (Hurrican's `TestDamagePlayers(v)`
  with `Timer.sync(v)`: v x 100, as the engine's own kinds; 0 = none, the step can do it itself); `score`; `mirror` 1 = the
  picture faces left and is mirrored when `e->dir` is 1 (right); `testblock` 1 = the step gets
  the wall flags around it (`bl br bo bu`) and the base movement stops at walls; `shootable`
  1 = the player's shots hit it.

* `<group>.c` - five functions per kind, all `static`:

      static void NAME_load(void)            /* extra pictures: static Pic NAME_p2; NAME_p2 = pic("x.png"); */
      static void NAME_init(Ent *e)          /* the map put it here: e->x/y (8.8, top-left), e->v1/v2 the map's values */
      static int  NAME_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
      static int  NAME_explode(Ent *e)       /* energy 0: 1 = done here (effects, sound), 0 = the default explosion */
      static int  NAME_draw(const Ent *e, uint32_t t)   /* 1 = drawn here, 0 = the default (picture, e->anim, mirror) */

  `NAME_step` is one 60 Hz step of Hurrican's DoKI (x, y = whole pixels; `touching` = the
  player's box meets its box); return 0 and the engine then does the contact damage (`touch`)
  and the base movement (GegnerClass::Run: `xs += xa/6`, `ys += ya/6`, `x += xs/6`, `y += ys/6`,
  walls stop it when `testblock`); return 1 to skip both (the kind moves itself, or is gone:
  `e->type = E_NONE`). Set `e->energy = 0` to make it explode (the engine calls `NAME_explode`).

The Ent fields (p1.c): `type active dir state size hit blocks spare` (bytes), `energy anim animt
t1 t2 v1 v2` (int16), `x y xo yo` (8.8 full-size pixels), `xs ys xa ya` (int16 8.8 Hurrican
units: `FX(10)` is Hurrican's 10.0). `e->dir` 1 = facing right. A kind is activated when it
first comes near the screen (then `e->dir` = towards the player, `xs`/`xa` are flipped to it);
it runs while within about one screen of the view.

What a kind may use from p1.c: `FX SYNC DMG DMG_T`, `rnd(n)`, `isqrt`, `sin_d(deg)`, `cos_d`
(x 256), `blk(tx, ty)` (block flags of tile tx, ty: `B_WALL B_PLATFORM B_DAMAGE B_LIQUID ...`),
`blk_below/blk_top/blk_left/blk_right`, `pl` (the player: `pl.x pl.y` 8.8 top-left, `pl.dir`,
`pl.state` PS_..., box `player_box`), `pcx() pcy()` (his centre), `ecx(e) ecy(e)`,
`player_distance(e)`, `player_damage(DMG(v))`, `aim_at(fromx, fromy, speed, &xs, &ys)`,
`anim_loop(e, first, last, steps)`, `ent_new(E_KIND, x, y)` (returns 0 when full; set `active = 1`),
`spawn_extra(x, y, kind)`, `enemy_shot(SH_BULLET|SH_CANON|SH_LASER|SH_PFLANZE, x, y, xs, ys, DMG(v))`,
`enemy_shot_pic(&P_x, x, y, xs, ys, ya, DMG(v), quiet)` (any picture as a shot; `ya` falls),
`fx_add(&P_expl | &P_explbig | &P_smoke | &P_flare | &P_x, cx, cy, steps_per_frame)` (max 12 at
once), `sfx(SFX_SHOT SFX_EXPLODE SFX_COLLECT SFX_HURT SFX_LAND SFX_ESHOT SFX_BIGEXPLODE SFX_HIT)`,
`ent_platform(e, idx)` (the player can stand on it), `draw_pic(&P_x, frame, x8, y8, mirror)`,
`cam_lock cam_lock_x cam_lock_y` (hold the camera: set every step, full-size top-left), `cam_x cam_y` (the view's top-left in half-size pixels: Hurrican's screen is cam_x*2, cam_y*2,
640 x 400), `skill` (0 easy, 1 normal, 2 hard).

Keep it lean: a few compares and adds per step, no loops over all entities (a kind that needs
another finds it once and keeps its index), at most one picture per kind drawn, no glows,
smoke trails or particles beyond an `fx_add` now and then.

## Bosses (P1k)

A stage boss is a kind like the others (its map number 120-139; it is never held back by the
enemy budget - `Kind.boss`). What a boss may use besides the above:

* `boss_lock(xoff, yoff, speed)` - Hurrican's `TileEngine.ScrollLevel(x, y, ZUSTAND_SCROLLTOLOCK)`:
  the camera goes to Hurrican's window offsets (full-size pixels; this screen shows the window's
  lower 400 lines) at `speed` (Hurrican's scroll speed, default 10) and stays; `boss_unlock()`
  gives it back to the player (ZUSTAND_SCROLLTOPLAYER / SCROLLBAR); `boss_locked_there()` = the
  camera has arrived (ZUSTAND_LOCKED). `cam_xoff()`, `cam_yoff()` - Hurrican's XOffset /
  YOffset now.
* `boss_bar(energy, max)` - the boss bar under the HUD; call it every step while it shows.
* `music_boss()` / `music_stage()` - the stage's boss song (CD track 15-23) / its own song.
* `stage_clear()` - the stage is won (Hurrican's StageClear()).
* the elevator's state for the elevator boss: `temple_lift_on`, `temple_lift_y`, `temple_lift_ys`
  (game/k2/temple.c, included before the boss files).

A boss is big: draw it with as few pictures as it needs (its body and at most a few parts),
no glows, no particles beyond an `fx_add` now and then. Its static data < 1 KB.
