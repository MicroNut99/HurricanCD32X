/*
 * STARSMALL - the small star of the flying-sack level (enemies/Gegner_StarSmall.cpp): bounces
 * off the walls and the screen's top and bottom.
 * Own members: none.
 * Left out: the LONGFUNKE of the explosion (a dropped particle type).
 * (starsmall.png is in none of the 9 stage packs: nothing is drawn there.)
 */
static void starsmall_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(20);
	e->AnimStart = 0;
	e->AnimEnde = 4;
	e->AnimSpeed = FX(0.8);
	e->AnimCount = 0;
	e->Destroyable = 1;
	e->ySpeed = FX(8);
	e->yAcc = 0;
	e->xAcc = 0;
	/* towards the player (the constructor sees xPos 0, as Hurrican's) */
	if (pl.xpos + FX(80) < e->xPos) e->xSpeed = FX(-8);
	else e->xSpeed = FX(8);
}

static void starsmall_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	e->blick = e->xSpeed > 0 ? 1 : -1;
	SimpleAnimation(e, 0);
	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if ((e->xSpeed < 0 && (e->blockl & B_WAND)) || (e->xSpeed > 0 && (e->blockr & B_WAND))) e->xSpeed = -e->xSpeed;
		if ((e->ySpeed < 0 && (e->blocko & B_WAND)) || (e->ySpeed > 0 && (e->blocku & B_WAND)) ||
		    (e->ySpeed < 0 && e->yPos - cam_yoff() < 0) ||
		    (e->ySpeed > 0 && e->yPos - cam_yoff() > FX(440)))
			e->ySpeed = -e->ySpeed;
		break;
	}
	TestDamagePlayers(e, SYNC(FX(2)), 0);
}

static void starsmall_explode(Ent *e)
{
	for (int i = 0; i < 8; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(40)), e->yPos - FX(30) + FX(GetRandom(40)), EXPLOSION_MEDIUM2);
	snd(SOUND_EXPLOSION1);
	pl.Score += 150;
}

TYPEDEF(STARSMALL) = { starsmall_init, starsmall_ki, starsmall_explode, 0, 0 };
