/*
 * SCHLEIMBOLLER - the slime ball (enemies/Gegner_SchleimBoller.cpp): hops towards the player;
 * destroyed it splits into two smaller ones until it is smaller than 30. Value1 = size (0: 60),
 * Value2 = the new xSpeed (spawned halves).
 * Own members: i[0] = Size.
 * Hurrican draws it scaled to Size x Size: here the plain frame, bottom-centred where the
 * scaled one stands. Left out: the additive LavaFlare glow.
 */
static void schleimboller_init(Ent *e)
{
	e->Handlung = GEGNER_FALLEN;
	e->HitSound = 1;
	e->Destroyable = 1;
	e->i[0] = e->Value1 != 0 ? e->Value1 : 60;
	e->Energy = FX(e->i[0]);
	e->xSpeed = FX(e->Value2);
	e->OwnDraw = 1;
	e->ySpeed = e->Value2 == 0 ? 0 : FX(-40);
	e->AnimSpeed = FX(2);
	e->AnimEnde = 2;
	e->yAcc = FX(8);
}

static void schleimboller_draw(Ent *e)
{
	const Pic *p = &GegnerPic[e->GegnerArt];
	draw_sprite(p, e->AnimPhase, e->xPos + FX(30 - p->fw), e->yPos + FX(60 - 2 * p->fh), 0);
}

static void schleimboller_ki(Ent *e)
{
	E2DBG(e);
	e->blick = -1;

	if (e->Handlung == GEGNER_FALLEN) SimpleAnimation(e, 0);
	else                               /* after the animation on the floor: jump off again */
	{
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			if (++e->AnimPhase >= e->AnimEnde)
			{
				e->Handlung = GEGNER_FALLEN;
				e->ySpeed = -FX(GetRandom(10) + 30);
				e->yAcc = FX(8);
				e->AnimPhase = 0;
				e->AnimEnde = 2;
				e->AnimSpeed = FX(2);
				if (pl.xpos + FX(35) > e->xPos + FX(30)) e->xSpeed = FX(GetRandom(8) + 6);
				else e->xSpeed = -FX(GetRandom(8) + 6);
			}
		}
	}

	switch (e->Handlung)
	{
	case GEGNER_FALLEN:                /* hopping */
		if (e->ySpeed > FX(50))
		{
			e->ySpeed = FX(50);
			e->yAcc = 0;
		}
		if (e->ySpeed > 0 && ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM)))
		{
			e->Handlung = GEGNER_STEHEN;
			e->AnimEnde = 12;
			e->AnimSpeed = FX(0.5);
			e->ySpeed = 0;
			e->yAcc = 0;
			e->xSpeed = 0;
		}
		if (e->ySpeed < 0 && ((e->blocko & B_WAND) || (e->blocko & B_GEGNERWAND))) e->ySpeed = -e->ySpeed;
		break;
	default:
		break;
	}

	TestDamagePlayers(e, SYNC(FX(2)), 0);
	Wegschieben(e, &GegnerRect[e->GegnerArt], FX(2));
}

static void schleimboller_explode(Ent *e)
{
	const int size = e->i[0];
	for (int i = 0; i < 3 + size / 8; i++)
		part_push(e->xPos + FX(15 + GetRandom(20)), e->yPos + FX(15 + GetRandom(20)), SCHLEIM);
	if (!snd_playing(SOUND_SCHLEIM)) snd(SOUND_SCHLEIM);
	pl.Score += 150;

	/* two smaller ones, if still big enough */
	if (size >= 30)
	{
		int mul = 1;
		if (e->xPos + FX(30 - size / 2) > pl.xpos + FX(35)) mul = -1;
		gegner_push(e->xPos + FX(20 * mul), e->yPos, SCHLEIMBOLLER, size - 8, 5);
		gegner_push(e->xPos - FX(20 * mul), e->yPos, SCHLEIMBOLLER, size - 8, -5);
	}
	gegner_default_explode(e);
}

TYPEDEF(SCHLEIMBOLLER) = { schleimboller_init, schleimboller_ki, schleimboller_explode, schleimboller_draw, 0 };
