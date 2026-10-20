/*
 * NEUFISCH - the new fish (enemies/Gegner_NeuFisch.cpp): swims in the water towards the
 * player, bites on and hangs on him (GegnerDran) until he rolls as the wheel.
 * Own members: f[0] = MoveSpeed, i[0] = pFest (1: hanging on the player).
 * While hanging Value1 / Value2 hold the offset to the player (as Hurrican).
 * Left out: the BUBBLEs of the explosion (a dropped particle type).
 */
static void neufisch_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->HitSound = 1;
	e->Energy = FX(60);
	e->AnimEnde = 9;
	e->AnimSpeed = FX(0.5);
	e->AnimCount = 0;
	e->Destroyable = 1;
	e->i[0] = 0;
	e->f[0] = FX(5) + FX(GetRandom(50)) / 10;
}

static void neufisch_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	e->AnimCount += SPEEDFACTOR;
	if (e->Handlung == GEGNER_LAUFEN || e->Handlung == GEGNER_SPECIAL)
	{
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			if (++e->AnimPhase >= e->AnimEnde) e->AnimPhase = e->AnimStart;
		}
	}
	else if (e->Handlung == GEGNER_DREHEN)
	{
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			if (++e->AnimPhase >= e->AnimEnde)
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
			if (--e->AnimPhase <= 9)
			{
				e->AnimPhase = e->AnimStart;
				e->AnimEnde = 9;
				e->Handlung = GEGNER_LAUFEN;
				e->AnimSpeed = FX(0.5);
				e->xSpeed = e->blick * e->f[0];
			}
		}
	}

	/* follow the player */
	e->ySpeed = 0;
	if (e->Handlung != GEGNER_SPECIAL)
	{
		e->xSpeed = e->blick * e->f[0];
		if (pl.InLiquid)
		{
			if (e->yPos < pl.ypos - FX(8) && (e->blocku & B_LIQUID)) e->ySpeed = FX(GetRandom(50) + 50) / 10;
			if (e->yPos > pl.ypos - FX(8) && (e->blocko & B_LIQUID)) e->ySpeed = -FX(GetRandom(50) + 50) / 10;
		}
	}

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
	{
		const int onWall = (e->blick == -1 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))) ||
		                   (e->blick == 1 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND)));
		if (onWall || (pl.InLiquid && ((e->xPos + FX(30) < pl.xpos + FX(35) && e->blick == -1) ||
		                               (e->xPos + FX(30) > pl.xpos + FX(35) && e->blick == 1))))
		{
			e->Handlung = GEGNER_DREHEN;
			e->AnimPhase = 9;
			e->AnimCount = 0;
			e->AnimEnde = 14;
			e->AnimSpeed = FX(0.3);
			e->xSpeed = 0;
			e->ySpeed = 0;
		}
		e->blockl = e->blockr = 0;
		Box rect;
		rect.t = 32;
		rect.b = 40;
		if (e->blick == -1) { rect.l = 10; rect.r = 20; }
		else { rect.l = 70; rect.r = 80; }
		/* bitten on? */
		if (SpriteCollision(pl.xpos, pl.ypos, &pl.CollideRect, e->xPos, e->yPos, &rect))
		{
			e->i[0] = 1;
			e->TestBlock = 0;
			e->Handlung = GEGNER_SPECIAL;
			e->AnimSpeed = FX(0.3);
			e->Value1 = (int16_t)FINT(pl.xpos - e->xPos);
			e->Value2 = (int16_t)FINT(pl.ypos - e->yPos);
			e->AnimPhase = 0;
			e->AnimEnde = 9;
			e->AnimSpeed = FX(0.5);
		}
		break;
	}

	case GEGNER_SPECIAL:              /* hanging on the player */
		pl.GegnerDran = 1;
		e->AnimPhase = 0;
		TestDamagePlayers(e, SYNC(FX(1.5)), 0);
		e->xPos = pl.xpos - FX(e->Value1);
		e->yPos = pl.ypos - FX(e->Value2);
		if (pl.Handlung == PA_RADELN || pl.Handlung == PA_RADELN_FALL)
		{
			e->Handlung = GEGNER_LAUFEN;
			e->AnimEnde = 9;
			e->AnimSpeed = FX(0.5);
			e->AnimCount = 0;
			e->TestBlock = 1;
			if (!pl.InLiquid) e->Energy = 0;
		}
		break;
	}
}

static void neufisch_explode(Ent *e)
{
	snd(SOUND_EXPLOSION1);
	for (int i = 0; i < 10; i++)
		part_push(e->xPos - FX(20) + FX(GetRandom(90)), e->yPos - FX(5) + FX(GetRandom(70)), PIRANHATEILE);
	for (int i = 0; i < 5; i++)
		part_push(e->xPos + FX(GetRandom(60)), e->yPos + FX(GetRandom(40)), PIRANHABLUT);
	pl.Score += 250;
}

TYPEDEF(NEUFISCH) = { neufisch_init, neufisch_ki, neufisch_explode, 0, 0 };
