/*
 * SWIMWALKER - the swimming walker (enemies/Gegner_SwimWalker.cpp): swims up and down, turns
 * at walls and fires torpedoes at the player now and then.
 * Own members: none.
 */
static void swimwalker_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(25);
	e->Destroyable = 1;
	e->xSpeed = FX(7);
	e->ySpeed = 0;
	e->yAcc = FX(2);
	e->AnimSpeed = FX(1.5);
	e->AnimEnde = 10;
}

static void swimwalker_ki(Ent *e)
{
	SimpleAnimation(e, 0);

	/* turn at walls */
	if (e->blick == -1 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))) { e->blick = 1; e->xSpeed = FX(7); }
	if (e->blick == 1 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND))) { e->blick = -1; e->xSpeed = FX(-7); }

	/* swim up and down */
	if (e->ySpeed > FX(9)) e->yAcc = FX(-2);
	if (e->ySpeed < FX(-9)) e->yAcc = FX(2);

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:   /* swim, now and then a torpedo */
		if (PlayerAbstand(e) <= 250 && GetRandom(200) == 0 &&
		    ((e->blick == -1 && pl.xpos + FX(45) <= e->xPos) || (e->blick == 1 && pl.xpos - FX(45) >= e->xPos)))
		{
			snd(SOUND_ROCKET);
			if (e->blick == -1) proj_push(e->xPos - FX(25), e->yPos + FX(10), TORPEDO);
			else proj_push(e->xPos + FX(55), e->yPos + FX(10), TORPEDO);
			e->Handlung = GEGNER_SCHIESSEN;
		}
		break;
	case GEGNER_SCHIESSEN:
		if (e->AnimPhase > e->AnimEnde - 2) e->Handlung = GEGNER_LAUFEN;
		break;
	default:
		break;
	}
	TestDamagePlayers(e, SYNC(FX(4)), 0);
}

static void swimwalker_explode(Ent *e)
{
	part_push(e->xPos + FX(GetRandom(20)), e->yPos + FX(GetRandom(20)), EXPLOSION_MEDIUM3);
	part_push(e->xPos, e->yPos, EXPLOSION_MEDIUM2);
	for (int i = 0; i < 30; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(85)), e->yPos - FX(10) + FX(GetRandom(65)), BUBBLE);
	snd(SOUND_EXPLOSION1);
	pl.Score += 100;
}

TYPEDEF(SWIMWALKER) = { swimwalker_init, swimwalker_ki, swimwalker_explode, 0, 0 };
