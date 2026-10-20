/*
 * SIDEROCKET - the rocket hanging on the wall (enemies/Gegner_SideRocket.cpp): when the player
 * comes near on its side its timer (Value2, default 10) runs, then it flies off (Value1 0: to
 * the left, else to the right).
 * Own members: none (AnimCount is the timer, as in Hurrican).
 */
static void siderocket_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->Energy = FX(50);
	e->Destroyable = 1;
	if (e->Value2 == 0) e->Value2 = 10;
}

static void siderocket_ki(Ent *e)
{
	e->blick = e->Value1 == 0 ? -1 : 1;

	switch (e->Handlung)
	{
	/* start the timer when the player comes */
	case GEGNER_INIT:
		e->AnimCount = FX(e->Value2);
		if (PlayerAbstandVert(e) < 100 && PlayerAbstandHoriz(e) < 400 &&
		    ((e->Value1 == 0 && e->xPos > pl.xpos) || (e->Value1 != 0 && e->xPos < pl.xpos)))
			e->Handlung = GEGNER_STEHEN;
		break;

	/* the timer runs */
	case GEGNER_STEHEN:
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount <= 0)
		{
			e->AnimCount = FX(0.1);
			e->Handlung = GEGNER_LAUFEN;
			snd(SOUND_ROCKET);
			if (e->Value1 == 0) { e->xSpeed = FX(-20); e->xAcc = FX(-5); }
			else { e->xSpeed = FX(20); e->xAcc = FX(5); }
		}
		break;

	/* flies */
	case GEGNER_LAUFEN:
		if (e->xSpeed < FX(-30)) e->xSpeed = FX(-30);
		else if (e->xSpeed > FX(30)) e->xSpeed = FX(30);
		e->AnimCount -= SYNC(FX(1));
		while (e->AnimCount <= 0)
		{
			e->AnimCount += FX(0.05);
			if (e->Value1 == 0) part_push(e->xPos + FX(35), e->yPos + FX(3), ROCKETSMOKE);
			else part_push(e->xPos - FX(5), e->yPos + FX(3), ROCKETSMOKE);
		}
		/* into a wall */
		if ((e->Value1 == 0 && (e->blockl & B_WAND)) || (e->Value1 == 1 && (e->blockr & B_WAND))) e->Energy = 0;
		break;
	}

	TestDamagePlayers(e, FX(25), 1);
}

static void siderocket_explode(Ent *e)
{
	int i;
	snd(SOUND_EXPLOSION1);
	part_push(e->xPos - FX(10), e->yPos - FX(20), EXPLOSION_MEDIUM2);
	for (i = 0; i < 10; i++)
	{
		part_push(e->xPos + FX(GetRandom(20)), e->yPos - FX(10) + FX(GetRandom(20)), MINIFLARE);
		part_push(e->xPos + FX(GetRandom(20)), e->yPos - FX(10) + FX(GetRandom(20)), SPIDERSPLITTER);
	}
	pl.Score += 50;
	gegner_default_explode(e);
}

TYPEDEF(SIDEROCKET) = { siderocket_init, siderocket_ki, siderocket_explode, 0, 0 };
