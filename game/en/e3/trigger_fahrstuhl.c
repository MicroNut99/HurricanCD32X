/*
 * FAHRSTUHL - the lift (triggers/Trigger_FahrStuhl.cpp): goes down once the player stands on it.
 *  Value1 == 0: the lift of the lift level (stage 3): the screen rides with it (XOffset fixed to
 *               the lift, YOffset = yPos - 320 - g_Fahrstuhl_Offset), its speed follows the
 *               player's AutoScrollspeed (set by the level's PARTIKELSPAWN Value1 == 10 triggers)
 *               and it explodes at yPos >= Value2 (the player jumps off).
 *  Value1 != 0: a lift that just goes down at speed 8 (the temple's exit).
 * It sets the shared g_Fahrstuhl_yPos / g_Fahrstuhl_Speed (hur.h) every step and
 * g_Fahrstuhl_Offset = 70 when made (the FahrstuhlBoss changes it later).
 * Own members: f[0] = new_ySpeed.
 *
 * Engine notes:
 *  - TileEngine.XOffset = .. and TileEngine.NewYOffset = .. (the screen forced to the lift) are
 *    written to the engine's camera (hur_xoff / hur_yoff in game/p1.c: hur.h has no setter;
 *    with the screen LOCKED the engine's camera_step leaves them alone).
 *    TileEngine.Zustand = LOCKED / SCROLLBAR -> scroll_level(cam_xoff(), cam_yoff(), TS_x).
 *  - Player[0].AutoScrollspeed has no field in the engine's Player: Hurrican sets it from the
 *    PARTIKELSPAWN triggers with Value1 == 10 (Trigger_PartikelSpawner.cpp case 10: each one,
 *    running from the level's start, counts down Value2 tenths of a second again and again
 *    and, the first time it runs out while the player is less than 1000 pixels away, sets
 *    AutoScrollspeed = Value2 and removes itself). Those triggers are emulated here from the
 *    map's objects (fahrstuhl_AutoScrollspeed, fahrstuhl_spawn_*), so the lift does not depend
 *    on the PARTIKELSPAWN port.
 *  - The lift's shadow (a scaled 0x99000000 copy) and the vertical parallax layer
 *    (fahrstuhl_parallax.png, TileEngine.IsElevatorLevel) are left out; IsElevatorLevel is
 *    g_Fahrstuhl_yPos > 0 for the engine (game/player.c, proj.c). ShakeScreen has no effect.
 */
#define FAHRSTUHL_NSPAWN 16
static fx fahrstuhl_AutoScrollspeed;               /* Player[0].AutoScrollspeed */
static int fahrstuhl_nspawn;
static int16_t fahrstuhl_spawn_obj[FAHRSTUHL_NSPAWN];   /* map object index, -1: removed */
static fx fahrstuhl_spawn_t[FAHRSTUHL_NSPAWN];          /* its AnimCount */
static fx fahrstuhl_sparkcount;                    /* DoKI's static sparkcount */

static void fahrstuhl_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(100);
	e->AnimPhase = 0;
	e->Destroyable = 0;
	g_Fahrstuhl_Offset = FX(70);
	e->f[0] = 0;
	e->TestBlock = 0;
	e->OwnDraw = 1;

	if (e->Value1 == 0)
	{
		/* the level's scroll speed triggers (PARTIKELSPAWN, Value1 10), from the level's start */
		fahrstuhl_AutoScrollspeed = 0;
		fahrstuhl_nspawn = 0;
		for (int i = 0; i < nobjs && fahrstuhl_nspawn < FAHRSTUHL_NSPAWN; i++)
			if (OBJ_ID(i) == PARTIKELSPAWN && OBJ_V1(i) == 10 && OBJ_SKILL(i) <= SKILL)
			{
				fahrstuhl_spawn_obj[fahrstuhl_nspawn] = (int16_t)i;
				fahrstuhl_spawn_t[fahrstuhl_nspawn] = FX(OBJ_V2(i));
				fahrstuhl_nspawn++;
			}
#ifndef OJ32X_MARS
		if (e3test_env("HB_E3_LIFTSPEED") > 0) fahrstuhl_AutoScrollspeed = FX(e3test_env("HB_E3_LIFTSPEED"));   /* PC check */
#endif
	}
}

/* Trigger_PartikelSpawner's case 10, for each of the level's scroll speed triggers */
static void fahrstuhl_scrollspeed_triggers(void)
{
	const Box *r = &GegnerRect[PARTIKELSPAWN];
	for (int k = 0; k < fahrstuhl_nspawn; k++)
	{
		const int i = fahrstuhl_spawn_obj[k];
		if (i < 0) continue;
		fahrstuhl_spawn_t[k] -= SYNC(FX(1));
		if (fahrstuhl_spawn_t[k] >= 0) continue;
		fahrstuhl_spawn_t[k] = FX(OBJ_V2(i));
		/* PlayerAbstand() < 1000 */
		const int dx = FINT(pl.xpos) + 35 - (OBJ_X(i) + r->r / 2);
		const int dy = FINT(pl.ypos) + 40 - (OBJ_Y(i) + r->b / 2);
		if (abs(dx) < 1000 && abs(dy) < 1000 && isqrt(dx * dx + dy * dy) < 1000)
		{
			fahrstuhl_AutoScrollspeed = FX(OBJ_V2(i));
			fahrstuhl_spawn_obj[k] = -1;               /* Energy = 0: the trigger is gone */
		}
	}
}

static void fahrstuhl_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, g_Fahrstuhl_yPos, 0);
}

static void fahrstuhl_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];

	if (e->Value1 == 0)
	{
		fahrstuhl_scrollspeed_triggers();
		hur_xoff = e->xPos + FX(200 - 320);       /* TileEngine.XOffset */
	}

	/* the player on the lift? */
	PlattformTest(e, rect);
	if (pl.AufPlattform == e)
	{
		if (e->Handlung != GEGNER_FALLEN)
		{
			if (e->Value1 == 0)
				scroll_level(cam_xoff(), cam_yoff(), TS_LOCKED);
			else if (tile_zustand() != TS_SCROLLTOPLAYER)
				scroll_level(e->xPos - FX(50), e->yPos - FX(400), TS_SCROLLTOLOCK);
		}
		e->Handlung = GEGNER_FALLEN;               /* off it goes */
	}

	switch (e->Handlung)
	{
	case GEGNER_STEHEN:          /* waiting for the player */
		break;
	case GEGNER_FALLEN:          /* going down */
	{
		if (e->Value1 == 0)
			hur_yoff = e->yPos - FX(320) - g_Fahrstuhl_Offset;    /* TileEngine.NewYOffset */

		if (e->Value1 != 0)
		{
			pl.JumpPossible = 0;
			pl.ypos = e->yPos - FX(pl.CollideRect.b);
			if (tile_zustand() == TS_LOCKED) e->f[0] = FX(8);
		}
		else
			e->f[0] = fahrstuhl_AutoScrollspeed;

		if (e->ySpeed < e->f[0]) e->ySpeed += SYNC(FX(0.3));
		if (e->ySpeed > e->f[0]) e->ySpeed -= SYNC(FX(0.3));

		/* sparks */
		fahrstuhl_sparkcount -= SYNC(FX(1));
		if (fahrstuhl_sparkcount < 0)
		{
			fahrstuhl_sparkcount = FX(0.05);
			part_push(e->xPos + FX(GetRandom(5)), e->yPos + FX(GetRandom(5)), FUNKE);
			part_push(e->xPos + FX(GetRandom(5)) + FX(rect->r), e->yPos + FX(GetRandom(5)), FUNKE);
			if (GetRandom(20) == 0) part_push(e->xPos - FX(20), e->yPos - FX(10), SMOKEBIG);
			if (GetRandom(20) == 0) part_push(e->xPos - FX(20) + FX(rect->r), e->yPos - FX(10), SMOKEBIG);
		}
	} break;
	default:
		break;
	}

	/* at the end (lift level only): explode */
	if (e->Value1 == 0 && e->yPos >= FX(e->Value2)) e->Energy = 0;

	g_Fahrstuhl_yPos = e->yPos;
	g_Fahrstuhl_Speed = e->ySpeed;
}

static void fahrstuhl_explode(Ent *e)
{
	shake_screen(10);
	snd(SOUND_EXPLOSION2);
	for (int j = 0; j < 100; j++)
		part_push(e->xPos + FX(GetRandom(400)), e->yPos + FX(GetRandom(100)), SPIDERSPLITTER);
	for (int j = 0; j < 50; j++)
	{
		part_push(e->xPos + FX(GetRandom(400)), e->yPos + FX(GetRandom(100)), ROCKETSMOKE);
		part_push(e->xPos - FX(30) + FX(GetRandom(400)), e->yPos - FX(30) + FX(GetRandom(100)), EXPLOSION_MEDIUM2);
	}
	for (int j = 0; j < 20; j++)
		part_push(e->xPos + FX(GetRandom(400)), e->yPos + FX(GetRandom(100)), SPLITTER);

	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLBAR);

	/* the player jumps off */
	pl.JumpPossible = 0;
	pl.AnimPhase = 0;
	pl.Handlung = PA_SPRINGEN;
	pl.JumpStart = pl.ypos;
	pl.yspeed = -FX(43);                           /* PLAYER_MAXJUMPSPEED */
	pl.JumpAdd = 0;
	pl.AufPlattform = 0;
}

TYPEDEF(FAHRSTUHL) = { fahrstuhl_init, fahrstuhl_ki, fahrstuhl_explode, fahrstuhl_draw, 0 };
