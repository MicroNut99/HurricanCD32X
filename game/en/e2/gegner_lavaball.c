/*
 * LAVABALL - the lava ball (enemies/Gegner_LavaBall.cpp): jumps out of the lava (speed Value1)
 * and falls back in; explodes on the player.
 * Own members: f[0] = yStart, f[1] = SmokeDelay, f[2] = FlareDelay, i[0] = InLava.
 * Left out: the additive LavaFlare glow drawn over it.
 */
static void lavaball_init(Ent *e)
{
	e->Active = 1;
	e->Handlung = GEGNER_EINFLIEGEN;
	e->Energy = FX(100);
	e->AnimStart = 0;
	e->AnimEnde = 19;
	e->AnimSpeed = FX(0.5);
	e->Destroyable = 0;
	e->ySpeed = FX(-e->Value1);
	e->yAcc = FX(4);
	e->f[1] = 0;
	e->f[2] = 0;
	e->i[0] = 1;
	e->OwnDraw = 1;
}

static void lavaball_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void lavaball_ki(Ent *e)
{
	E2DBG(e);
	const Box *rect = &GegnerRect[e->GegnerArt];
	SimpleAnimation(e, 0);

	/* just appeared: remember yPos */
	if (e->Handlung == GEGNER_EINFLIEGEN)
	{
		e->f[0] = e->yPos;
		e->Handlung = GEGNER_LAUFEN;
		return;
	}

	if (BlockUntenNormal(e->xPos, e->yPos, e->xPosOld, e->yPosOld, rect) & B_LIQUID)
	{
		if (!e->i[0])                  /* just fell in */
		{
			for (int i = 0; i < 12; i++)
				part_push(e->xPos + FX(16 + GetRandom(20)), e->yPos + FX(8 + rect->b), LAVA_SPRITZER2);
			snd(SOUND_WATERIN);
		}
		e->i[0] = 1;
	}
	else
	{
		if (e->i[0])                   /* just came out */
		{
			for (int i = 0; i < 12; i++)
				part_push(e->xPos + FX(16 + GetRandom(20)), e->yPos + FX(8 + rect->b), LAVA_SPRITZER);
			snd(SOUND_WATEROUT);
		}
		e->i[0] = 0;
	}

	/* smoke */
	e->f[1] -= SYNC(FX(1));
	if (e->f[1] < 0)
	{
		e->f[1] = FX(0.5);
		if (!e->i[0]) part_push(e->xPos - FX(20) + FX(GetRandom(50)), e->yPos + FX(GetRandom(40)), SMOKEBIG);
	}
	e->f[2] -= SYNC(FX(1));
	if (e->f[2] < 0) e->f[2] = FX(2);

	if (e->ySpeed > FX(50)) e->ySpeed = FX(50);

	/* back in? gone */
	if (e->ySpeed > 0 && e->yPos > e->f[0]) e->Energy = 0;

	/* hit the player: explodes here (falling back into the lava it goes without) */
	if (SpriteCollision(e->xPos, e->yPos, rect, pl.xpos, pl.ypos, &pl.CollideRect))
	{
		DamagePlayer(FX(50), 0);
		e->Energy = 0;
		snd(SOUND_EXPLOSION1);
		part_push(e->xPos - FX(10), e->yPos - FX(30), EXPLOSION_GIANT);
		for (int j = 0; j < 4; j++)
			part_push(e->xPos - FX(10) + FX(GetRandom(40)), e->yPos - FX(10) + FX(GetRandom(40)), EXPLOSION_MEDIUM2);
		for (int j = 0; j < 20; j++)
			part_push(e->xPos - FX(10) + FX(GetRandom(50)), e->yPos - FX(16) + FX(GetRandom(56)), MINIFLARE);
		for (int j = 0; j < 20; j++)
			part_push(e->xPos + FX(GetRandom(40)), e->yPos + FX(GetRandom(40)), SMOKE2);
	}
}

static void lavaball_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(LAVABALL) = { lavaball_init, lavaball_ki, lavaball_explode, lavaball_draw, 0 };
