/*
 * PARTIKELSPAWN - the particle spawner (triggers/Trigger_PartikelSpawner.cpp): Value1 the kind,
 * Value2 the delay (or a parameter):
 *   0 rain, 1 snow, 2 smoke, 3 sparks (with sound), 4 bubbles, 5 rising smoke, 6 rising smoke
 *   with pauses, 7 waterfall spray, 8 drops from the ceiling, 9 lightning and thunder,
 *   10 the auto-scroll speed, 11 fog, 12 sparks without sound, 13 flickering light,
 *   14 spider parts, 15 leaves, 16 dust, 17 maggots, 18 lava sparks, 19 rising smoke with lift.
 * Own members: f[0] = Aussetzer (the pause timer of 6). AnimCount is the spawn timer.
 *
 * Few effects (the 32X's drawing time, and the wish for a calm screen): the weather and
 * ambience particles are not pushed at all - rain (0), snow (1), bubbles (4), the rising
 * SMOKE3 (5, 6, 19), waterfall spray (7), ceiling drops (8), fog (11), leaves / dust (15, 16),
 * lava dust (18), the sparks' LASERFLAME / FUNKE / LASERFUNKE2 (3, 12) - they are dropped
 * types of the particle port anyway. Kept: the updraft of the smoke (5, 6, 19: it lifts the
 * jumping player), SMOKE (2), SPIDERPARTS (14), the SMOKEBIG of 19, the sounds (3, 9, 13),
 * MADE (17), the scroll speed (10).
 * Left out: the lightning / flicker colour (SetThunderColor; no screen colour fades).
 *
 * The scroll speed (10): Hurrican's Player[0].AutoScrollspeed. It moves the player and the
 * screen only while surfing (not ported) and is read by the elevator (Trigger_FahrStuhl's
 * speed with Value1 == 0). No engine global holds it, so it is kept here:
 *   partikelspawner_AutoScrollspeed (fx, units per 1/10 s; set to 0 when a stage is loaded).
 */
static fx partikelspawner_AutoScrollspeed;


static void partikelspawner_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Destroyable = 0;
	e->DontMove = 1;
	e->Energy = FX(100);
	e->f[0] = 0;
	e->AnimCount = FX(e->Value2);
	if (e->Value1 == 8) e->AnimCount = FX(GetRandom(e->Value2 > 0 ? e->Value2 : 1));
	if (e->Value1 == 18) e->AnimCount = FX(3);
	if (e->Value1 == 11 || e->Value1 == 19) e->AnimCount = FX(0.5);
	e->Active = 1;                       /* spawns before it is on the screen */
}

static void partikelspawner_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	const int v1 = e->Value1;

	/* the smoke lifts the jumping player (straight up: Value2 == 0) */
	if (v1 == 19 || ((v1 == 5 || (v1 == 6 && e->f[0] > FX(6))) && e->Value2 == 0))
	{
		if (pl.yspeed != 0 && FINT(pl.xpos) + pl.CollideRect.l < FINT(e->xPos) + 40 &&
		    FINT(e->xPos) < FINT(pl.xpos) + pl.CollideRect.r &&
		    FINT(pl.ypos) + pl.CollideRect.t < FINT(e->yPos) + 40 &&
		    FINT(e->yPos) - 300 < FINT(pl.ypos) + pl.CollideRect.b)
		{
			if (v1 == 19) pl.yspeed -= SYNC(FX(e->Value2));
			else pl.yspeed -= SYNC(FX(6));
		}
	}

	e->f[0] += SYNC(FX(0.2));
	if (e->f[0] > FX(10)) e->f[0] = 0;

	e->AnimCount -= SYNC(FX(1));
	if (e->AnimCount >= 0) return;
	e->AnimCount = FX(e->Value2);
	if (v1 == 18) e->AnimCount = FX(3);
	if (v1 == 11) e->AnimCount = FX(2);

	/* the ones spawned near the player, on the screen or not */
	if (PlayerAbstand(e) < 1000)
	{
		switch (v1)
		{
		case 0:                          /* rain (REGENTROPFEN: not pushed) */
			e->AnimCount = FX(e->Value2) / 4;
			if (e->AnimCount == 0) e->AnimCount = FX(1);
			break;
		case 1:                          /* snow (SCHNEEFLOCKE: not pushed) */
			e->AnimCount = FX(0.5);
			break;
		case 5: case 6:                  /* rising smoke (SMOKE3: not pushed) */
			e->AnimCount = FX(0.25);
			break;
		case 8:                          /* ceiling drops (SPAWNDROP: not pushed) */
			e->AnimCount -= FX(GetRandom(5));
			break;
		case 9:                          /* thunder */
			snd(SOUND_THUNDER);
			e->Value2 = (int16_t)(GetRandom(80) + 10);
			break;
		case 13:                         /* flickering light */
			snd(SOUND_FUNKE + GetRandom(4));
			e->Value2 = (int16_t)(GetRandom(20) + 10);
			break;
		case 14:                         /* spider parts */
			part_push(e->xPos + FX(4), e->yPos + FX(4), SPIDERPARTS);
			e->AnimCount = FX(GetRandom(5) + 10);
			break;
		case 15:                         /* leaves / dust (BLATT2, DUST: not pushed) */
			if (e->Value2 == 0) e->AnimCount = FX(GetRandom(5) + 1) / 5;
			else e->AnimCount = FX(GetRandom(5) + 1) / 2;
			break;
		case 16:                         /* dust (not pushed) */
			e->AnimCount = FX(GetRandom(5) + 1) / 5;
			break;
		case 18:                         /* lava dust (not pushed) */
			e->AnimCount = FX(3);
			break;
		case 19:                         /* rising smoke with lift */
			if (GetRandom(3) == 0) part_push(e->xPos - FX(10), e->yPos - FX(20), SMOKEBIG);
			e->AnimCount = FX(0.25);
			break;
		}
	}

	/* the ones spawned only while the trigger is on the screen */
	if (e->xPos + FX(40) > cam_xoff() && e->xPos < cam_xoff() + FX(RENDERWIDTH) &&
	    e->yPos + FX(40) > cam_yoff() && e->yPos < cam_yoff() + FX(RENDERHEIGHT))
	{
		switch (v1)
		{
		case 2:                          /* smoke */
			part_push(e->xPos + FX(GetRandom(5)), e->yPos + FX(24), SMOKE);
			break;
		case 3:                          /* sparks (the particles: not pushed) */
		case 12:
			if (GetRandom(4) == 0 && v1 == 3) snd(SOUND_FUNKE);
			break;
		case 10:                         /* the scroll speed, then the trigger goes */
			partikelspawner_AutoScrollspeed = FX(e->Value2);
			e->Energy = 0;
			break;
		case 17:                         /* maggots */
			gegner_push(e->xPos + FX(GetRandom(20) - 10), e->yPos + FX(16), MADE, 0, 0);
			break;
		}
	}
}

static void partikelspawner_explode(Ent *e) { (void)e; }
static void partikelspawner_load(void) { partikelspawner_AutoScrollspeed = 0; }

TYPEDEF(PARTIKELSPAWN) = { partikelspawner_init, partikelspawner_ki, partikelspawner_explode, 0, partikelspawner_load };
