/*
 * AUGE - the eye extra (enemies/Gegner_Auge.cpp): flies in a sine wave through the level,
 * bounces off walls, leaves the extra Value1 when shot.
 * PushGegner: AUGE with Value2 == 1 is an EXTRAS (GegnerExtras) instead - done in auge_init
 * (the object becomes an EXTRAS and runs its constructor, when that type is ported).
 * Own members: f[0] = smokedelay.
 * Left out: the SMOKE2 trail (a dropped particle type; its timer is kept).
 */

/* PC test hook of group E5's objects (not on the 32X), run once a step from the step of any
 * E5 object (E5_TEST_HOOK in their DoKI): E5_SPAWN="art,dx,dy[,v1,v2[,step]]" pushes one object
 * at the player's position + (dx, dy) once (at that step), E5_LIFT=y puts a still elevator floor at y
 * (g_Fahrstuhl_yPos; STELZSACK / POKENUKE without the elevator), E5_TRACE=art[,every] prints the objects of a
 * type, E5_KILL=art,step takes their energy at that step. */
#ifndef OJ32X_MARS
static void auge_test_hook(Ent *e)
{
	static int done, step, last = MAX_LIVE;
	/* once a step: the objects run in slot order, so a slot not after the last caller's
	 * starts a new step */
	const int me = (int)(e - ents);
	const int newstep = me <= last;
	last = me;
	if (!newstep) return;
	step++;
	const char *s = getenv("E5_SPAWN");
	if (s && !done)
	{
		int a = 0, dx = 0, dy = 0, v1 = 0, v2 = 0, at = 0;
		const int n = sscanf(s, "%d,%d,%d,%d,%d,%d", &a, &dx, &dy, &v1, &v2, &at);
		if (step >= at)
		{
			if (n >= 3) gegner_push(pl.xpos + FX(dx), pl.ypos + FX(dy), a, v1, v2);
			done = 1;
		}
	}
	if ((s = getenv("E5_LIFT")) != 0)      /* a still elevator floor at y; the player on it */
	{
		g_Fahrstuhl_yPos = FX(atoi(s));
		if (pl.ypos + FX(pl.CollideRect.b) > g_Fahrstuhl_yPos)
		{
			pl.ypos = g_Fahrstuhl_yPos - FX(pl.CollideRect.b);
			pl.yspeed = 0;
		}
	}
	/* E5_KILL=art,step: the objects of that type lose all energy at that step */
	if ((s = getenv("E5_KILL")) != 0)
	{
		int art = -1, at = 0;
		sscanf(s, "%d,%d", &art, &at);
		if (step == at)
			for (int k = 0; k < MAX_LIVE; k++)
				if (ents[k].GegnerArt == art) ents[k].Energy = 0;
	}
	/* E5_TRACE=art[,every]: the objects of that type */
	if ((s = getenv("E5_TRACE")) != 0)
	{
		int art = -1, every = 10;
		sscanf(s, "%d,%d", &art, &every);
		if (every > 0 && step % every == 0)
			for (int k = 0; k < MAX_LIVE; k++)
			{
				const Ent *o = &ents[k];
				if (o->GegnerArt != art) continue;
				printf("e5 %d: #%d art %d act %d h %d pos %d,%d spd %d,%d E %d anim %d v %d,%d f0 %d i0 %d pl %d,%d\n", step, k, art,
				       o->Active, o->Handlung, FINT(o->xPos), FINT(o->yPos), FINT(o->xSpeed), FINT(o->ySpeed),
				       FINT(o->Energy), o->AnimPhase, o->Value1, o->Value2, FINT(o->f[0]), (int)o->i[0], FINT(pl.xpos), FINT(pl.ypos));
			}
	}
}
#define E5_TEST_HOOK(e) auge_test_hook(e)
#else
#define E5_TEST_HOOK(e) ((void)(e))
#endif

static void auge_init(Ent *e)
{
	if (e->Value2 == 1 && gegner_known(EXTRAS))
	{
		e->GegnerArt = EXTRAS;
		gegner_types[EXTRAS]->init(e);
		return;
	}
	if (e->Value1 < 0 || e->Value1 > 11) e->Value1 = 7;
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(1);
	e->f[0] = FX(0.2);
	e->Destroyable = 1;
	e->xSpeed = FX(12);
	e->ySpeed = 0;
	e->yAcc = FX(4);
	e->AnimSpeed = FX(0.5);
}

static void auge_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	/* smoke (SMOKE2: dropped type, not pushed) */
	e->f[0] -= SYNC(FX(0.25));
	if (e->f[0] < 0) e->f[0] = FX(0.1);

	/* turning animation only */
	if (e->Handlung != GEGNER_STEHEN)
	{
		e->AnimCount += SPEEDFACTOR;
		if (e->Handlung == GEGNER_DREHEN)
		{
			if (e->AnimCount > e->AnimSpeed)
			{
				e->AnimCount = 0;
				e->AnimPhase++;
				if (e->AnimPhase >= e->AnimEnde)
				{
					e->AnimCount = 0;
					e->Handlung = GEGNER_DREHEN2;
					e->blick = (int8_t)-e->blick;
				}
			}
		}
		else if (e->Handlung == GEGNER_DREHEN2)
		{
			if (e->AnimCount > e->AnimSpeed)
			{
				e->AnimCount = 0;
				e->AnimPhase--;
				if (e->AnimPhase <= 0)
				{
					e->AnimPhase = 0;
					e->AnimEnde = 0;
					e->Handlung = GEGNER_LAUFEN;
				}
			}
		}
	}

	if (e->ySpeed > FX(12)) e->yAcc = FX(-4);
	else if (e->ySpeed < FX(-12)) e->yAcc = FX(4);

	/* bounce off ceiling and floor */
	if ((e->blocko & B_WAND) || (e->blocko & B_GEGNERWAND)) { e->yAcc = -e->yAcc; e->ySpeed = -e->ySpeed; }
	if ((e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND)) { e->yAcc = -e->yAcc; e->ySpeed = -e->ySpeed; }

	/* off a wall left or right */
	if ((e->xSpeed < 0 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))) ||
	    (e->xSpeed > 0 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND))))
	{
		e->Handlung = GEGNER_DREHEN;
		e->AnimPhase = 0;
		e->AnimCount = 0;
		e->AnimEnde = 5;
		e->xSpeed = -e->xSpeed;
	}
}

static void auge_explode(Ent *e)
{
	for (int i = 0; i < 5; i++)
		part_push(e->xPos - FX(15) + FX(GetRandom(20)), e->yPos - FX(15) + FX(GetRandom(40)), EXPLOSION_MEDIUM2);
	snd(SOUND_EXPLOSION1);
	pl.Score += 100;
	gegner_push(e->xPos + FX(12), e->yPos + FX(12), EXTRAS, e->Value1, 0);
}

TYPEDEF(AUGE) = { auge_init, auge_ki, auge_explode, 0, 0 };
