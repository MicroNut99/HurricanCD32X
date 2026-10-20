/*
 * SKULL - the skull of the end boss machine (bosses/Boss_Skull.cpp). Spawned by THEWALL and
 * SKELETOR, which set its Handlung (SPECIAL: fly around the player, SPECIAL2: back onto the
 * machine, SPECIAL3: to the middle and explode) and Value1 (99: shoots at the player).
 * Own members: i[0] = pMachine (slot index of the THEWALL, -1 none), f[0] = mxPos,
 * f[1] = myPos, f[2] = SmokeDelay, f[3] = ShotDelay, f[4] = Disappear. (ShotCount unused.)
 * Left out: Timer.SetMoveSpeed (the slow motion while it explodes), the sound pitches.
 *
 * Also the B1 PC test hook (skull_load, not on the 32X): HB_B1_SPAWN=<id>[,<v1>,<v2>] turns the
 * map object nearest to HB_WARP (or the player's start) into object <id> - for the objects no
 * map places (RIESENQUALLE, SKULL).
 */
static void skull_init(Ent *e)
{
	e->i[0] = -1;
	e->i[1] = 0;
	e->f[2] = 0;
	e->f[3] = FX(10.0);
	e->f[4] = FX(20.0);
	e->Energy = FX(100000);
	e->Destroyable = 1;
	e->TestBlock = 0;
	e->AnimCount = 0;
	if (e->Value2 == 99) e->AnimCount = FX(20.0);
	e->Handlung = GEGNER_SPECIAL;
	e->NoDespawn = 1;                      /* it carries the ending: never goes away */
	/* the end boss wall's position */
	for (int k = 0; k < MAX_LIVE; k++)
	{
		Ent *o = &ents[k];
		if (o->GegnerArt == THEWALL)
		{
			e->f[0] = o->xPos + FX(5);
			e->f[1] = o->yPos + FX(229);
			e->i[0] = k;
			break;
		}
	}
	/* not in the game yet: its place from the map */
	if (e->i[0] < 0)
		for (int k = 0; k < nobjs; k++)
			if (OBJ_ID(k) == THEWALL) { e->f[0] = FX(OBJ_X(k)) + FX(5); e->f[1] = FX(OBJ_Y(k)) + FX(229); break; }
}

static Ent *skull_machine(Ent *e)
{
	if (e->i[0] < 0 || e->i[0] >= MAX_LIVE || ents[e->i[0]].GegnerArt != THEWALL)
	{
		/* (here objects come into the game only near the screen: the wall may have come later
		 * than the skull - look again) */
		e->i[0] = -1;
		for (int k = 0; k < MAX_LIVE; k++)
			if (ents[k].GegnerArt == THEWALL) { e->i[0] = k; e->f[0] = ents[k].xPos + FX(5); e->f[1] = ents[k].yPos + FX(229); break; }
		if (e->i[0] < 0) return 0;
	}
	return &ents[e->i[0]];
}

static fx skull_clamp(fx v, fx lo, fx hi) { return v < lo ? lo : v > hi ? hi : v; }

#ifndef OJ32X_MARS
/* PC check: HB_B1LOG=1 prints the state every half second */
static void skull_log(const Ent *e)
{
	static int n;
	if (getenv("HB_B1LOG") && n++ % 30 == 0)
		fprintf(stderr, "skull: H %d E %d pos %d,%d speed %d,%d anim %d dest %d cam %d,%d state %d player %d,%d\n", e->Handlung,
		        FINT(e->Energy), FINT(e->xPos), FINT(e->yPos), FINT(e->xSpeed), FINT(e->ySpeed), e->AnimPhase, e->Destroyable,
		        FINT(cam_xoff()), FINT(cam_yoff()), tile_zustand(), FINT(pl.xpos), FINT(pl.ypos));
}
#else
#define skull_log(e) ((void)0)
#endif

static void skull_ki(Ent *e)
{
	skull_log(e);
	int i;
	Ent *m = skull_machine(e);
	e->Energy = FX(100000);
	e->blick = -1;

	/* the frame by the position to the player */
	{
		const fx dx = (pl.xpos + FX(35)) - (e->xPos + FX(16));
		int a = FINT(dx / 30);
		if (a < -5) a = -5;
		if (a > 9) a = 9;
		a += 5;
		if (e->xSpeed < 0) a = 20 - a;
		if (a < 0) a = 0;
		if (a > 19) a = 19;
		e->AnimPhase = (int16_t)a;
	}

	/* the drive */
	e->f[2] -= SYNC(FX(1.0));
	if (e->f[2] <= 0)
	{
		e->f[2] = FX(5.0);
		if (GetRandom(20) == 0)
			for (i = 0; i < 10; i++) part_push(e->xPos + FX(10), e->yPos + FX(40), FUNKE);
	}

	switch (e->Handlung)
	{
	case GEGNER_SPECIAL3:                  /* to the middle and explode */
		e->f[4] -= SYNC(FX(1.0));
		if (e->AnimCount > 0) e->AnimCount -= SYNC(FX(1.0));
		else
		{
			e->AnimCount = FX(0.8);
			snd(SOUND_EXPLOSION2);
			part_push(e->xPos + FX(GetRandom(30) - 30), e->yPos + FX(GetRandom(30) - 30), EXPLOSION_MEDIUM2);
			for (i = 0; i < 3; i++)
				part_push(e->xPos + FX(GetRandom(30) - 10), e->yPos + FX(GetRandom(20) - 10), SPIDERSPLITTER);
		}
		e->xAcc = e->xPos + FX(16) > cam_xoff() + FX(320) ? FX(-4.0) : FX(4.0);
		e->yAcc = e->yPos + FX(20) > cam_yoff() + FX(200) ? FX(-4.0) : FX(4.0);
		e->xSpeed = skull_clamp(e->xSpeed, FX(-25), FX(25));
		e->ySpeed = skull_clamp(e->ySpeed, FX(-15), FX(15));
		if (e->f[4] < 0)
		{
			snd(SOUND_EXPLOSION2);
			snd(SOUND_EXPLOSION3);
			snd(SOUND_EXPLOSION1);
			shake_screen(10);
			e->Energy = 0;
			snd(SOUND_MUTANT);
			part_push(e->xPos + FX(20), e->yPos + FX(20), SHOCKEXPLOSION);
			part_push(e->xPos + FX(20), e->yPos + FX(20), SHOCKEXPLOSION);
			part_push(e->xPos + FX(20), e->yPos + FX(20), SHOCKEXPLOSION);
			for (i = 0; i < 20; i++) part_push(e->xPos + FX(GetRandom(30)), e->yPos + FX(GetRandom(30)), SPIDERSPLITTER);
			for (i = 0; i < 20; i++) part_push(e->xPos + FX(GetRandom(100) - 50), e->yPos + FX(GetRandom(100) - 50), EXPLOSION_BIG);
			for (i = 0; i < 20; i++) part_push(e->xPos + FX(GetRandom(100) - 50), e->yPos + FX(GetRandom(100) - 50), SPLITTER);
			for (i = 0; i < 20; i++) part_push(e->xPos + FX(GetRandom(100) - 50), e->yPos + FX(GetRandom(100) - 50), SCHROTT1);
			part_push(e->xPos - FX(80), e->yPos - FX(80), EXPLOSION_GIGA);
			/* the flying sacks to ride on */
			gegner_push(cam_xoff() + FX(200), cam_yoff() - FX(40), REITFLUGSACK, 1, 0);
		}
		break;

	case GEGNER_SPECIAL:                   /* around the player, kept on the screen */
		if (e->AnimCount > 0) e->AnimCount -= SYNC(FX(1.0));
		else
		{
			e->xAcc = e->xPos + FX(16) > pl.xpos + FX(35) ? FX(-2.0) : FX(2.0);
			e->yAcc = e->yPos + FX(20) > pl.ypos + FX(10) ? FX(-2.0) : FX(2.0);
			e->xSpeed = skull_clamp(e->xSpeed, FX(-25), FX(25));
			e->ySpeed = skull_clamp(e->ySpeed, FX(-15), FX(15));
			if ((e->xSpeed < 0 && e->xPos < cam_xoff()) || (e->xSpeed > 0 && e->xPos > cam_xoff() + FX(RENDERWIDTH)))
				e->xSpeed = -e->xSpeed;
			/* (Hurrican compares with RENDERWIDTH here too) */
			if ((e->ySpeed < 0 && e->yPos < cam_yoff()) || (e->ySpeed > 0 && e->yPos > cam_yoff() + FX(RENDERWIDTH)))
				e->ySpeed = -e->ySpeed;
		}
		break;

	case GEGNER_SPECIAL2:                  /* back onto the machine */
		e->xAcc = 0;
		e->yAcc = 0;
		if (e->xSpeed < 0) e->xSpeed += SYNC(FX(1.0));
		if (e->xSpeed > 0) e->xSpeed -= SYNC(FX(1.0));
		if (e->ySpeed < 0) e->ySpeed += SYNC(FX(1.0));
		if (e->ySpeed > 0) e->ySpeed -= SYNC(FX(1.0));
		if (e->xPos < e->f[0]) { e->xPos += SYNC(FX(10.0)); if (e->xPos > e->f[0]) e->xPos = e->f[0]; }
		if (e->xPos > e->f[0]) { e->xPos -= SYNC(FX(10.0)); if (e->xPos < e->f[0]) e->xPos = e->f[0]; }
		if (e->yPos < e->f[1]) { e->yPos += SYNC(FX(10.0)); if (e->yPos > e->f[1]) e->yPos = e->f[1]; }
		if (e->yPos > e->f[1]) { e->yPos -= SYNC(FX(10.0)); if (e->yPos < e->f[1]) e->yPos = e->f[1]; }
		if (e->xPos == e->f[0] && e->yPos == e->f[1])
		{
			e->Energy = 0;
			if (m) m->Value1 = 1;
			snd(SOUND_KLONG);
			music_boss();
		}
		break;

	default:
		break;
	}

	/* shoot at the player? */
	if (e->Value1 == 99)
	{
		if (m && m->Energy < FX(1500)) e->f[3] -= SYNC(FX(1.0));
		if (e->f[3] < 0)
		{
			e->f[3] = FX(15.0);
			proj_push(e->xPos - FX(10), e->yPos, SUCHSCHUSS2);
			snd(SOUND_GOLEMSHOT);
		}
	}

	if (e->Handlung != GEGNER_SPECIAL3) TestDamagePlayers(e, SYNC(FX(1.0)), 0);
}

static void skull_explode(Ent *e)
{
	gegner_default_explode(e);
}

#ifndef OJ32X_MARS
/* the B1 PC test hook (see the top) */
static void skull_load(void)
{
	const char *s = getenv("HB_B1_SPAWN");
	if (!s || !objs) return;
	int id = 0, v1 = 0, v2 = 0;
	if (sscanf(s, "%d,%d,%d", &id, &v1, &v2) < 1 || !gegner_known(id)) return;
	fx wx = FX(400), wy = FX(400);
	int b = 1;
	gegner_player_start(&wx, &wy, &b);
	if (getenv("HB_WARP"))
	{
		int x = 0, y = 0;
		if (sscanf(getenv("HB_WARP"), "%d,%d", &x, &y) == 2) { wx = FX(x); wy = FX(y); }
	}
	int best = -1, bd = 0x7fffffff;
	for (int i = 0; i < nobjs; i++)
	{
		if (OBJ_ID(i) == 0) continue;
		const int dx = OBJ_X(i) - FINT(wx), dy = OBJ_Y(i) - FINT(wy);
		if (abs(dx) > 30000 || abs(dy) > 30000) continue;
		const int d = abs(dx) + abs(dy);
		if (d < bd) { bd = d; best = i; }
	}
	if (best < 0) return;
	uint8_t *o = (uint8_t *)(uintptr_t)(objs + 12 * best);
	o[0] = (uint8_t)(id >> 8); o[1] = (uint8_t)id;
	o[6] = (uint8_t)(v1 >> 8); o[7] = (uint8_t)v1;
	o[8] = (uint8_t)(v2 >> 8); o[9] = (uint8_t)v2;
	o[10] = 0; o[11] = 0;
	fprintf(stderr, "B1 test: object %d at %d,%d is now %d (%d,%d)\n", best, OBJ_X(best), OBJ_Y(best), id, v1, v2);
}
#else
#define skull_load 0
#endif

TYPEDEF(SKULL) = { skull_init, skull_ki, skull_explode, 0, skull_load };
