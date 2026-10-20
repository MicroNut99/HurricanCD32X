/*
 * FLUGSACK - the flying fat sack (enemies/Gegner_Flugsack.cpp): floats around the player and
 * fires homing shots; shot down it falls burning and explodes at a wall.
 * Own members: f[0] = ShotCount, f[1] = SmokeCount.
 */
static void flugsack_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(50);
	e->Destroyable = 1;
	e->xSpeed = 0;
	e->ySpeed = 0;
	e->xAcc = FX(2);
	e->yAcc = FX(2);
	e->AnimSpeed = FX(1.5);
	e->AnimEnde = 10;
	e->f[0] = FX(1);
	e->f[1] = FX(0.2);
	if (e->Value1 == 99) e->Energy = 0;
}

static void flugsack_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	SimpleAnimation(e, 0);

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:   /* fly and shoot now and then */
		if (pl.xpos + FX(45) < e->xPos + FX(rect->l)) e->blick = -1;
		if (pl.xpos + FX(45) > e->xPos + FX(rect->r)) e->blick = 1;

		/* float around */
		if (e->ySpeed > FX(6)) e->yAcc = FX(-2);
		if (e->ySpeed < FX(-6)) e->yAcc = FX(2);
		if (e->xSpeed > FX(5)) e->xAcc = FX(-2);
		if (e->xSpeed < FX(-5)) e->xAcc = FX(2);

		/* follow the player */
		if (pl.xpos + FX(45) < e->xPos + FX(40) && !(e->blockl & B_WAND)) e->xPos -= SYNC(FX(4));
		if (pl.xpos + FX(45) > e->xPos + FX(40) && !(e->blockr & B_WAND)) e->xPos += SYNC(FX(4));
		if (pl.ypos + FX(45) < e->yPos + FX(45) && !(e->blocko & B_WAND)) e->yPos -= SYNC(FX(4));
		if (pl.ypos + FX(45) > e->yPos + FX(45) && !(e->blocku & B_WAND)) e->yPos += SYNC(FX(4));

		if ((e->blocko & B_WAND) || (e->blocko & B_GEGNERWAND)) e->ySpeed = FX(4);
		if ((e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND)) e->ySpeed = FX(-4);
		if ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND)) e->xSpeed = FX(4);
		if ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND)) e->xSpeed = FX(-4);

		/* smoke */
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0)
		{
			e->f[1] += FX(0.1);
			if (e->blick == -1) part_push(e->xPos + FX(66), e->yPos + FX(50), FLUGSACKSMOKE2);
			else part_push(e->xPos, e->yPos + FX(50), FLUGSACKSMOKE);
		}

		/* shoot when near enough */
		if (PlayerAbstand(e) <= 400 && ((e->blick == -1 && pl.xpos + FX(45) <= e->xPos) ||
		                                (e->blick == 1 && pl.xpos - FX(45) >= e->xPos)))
		{
			e->f[0] -= SYNC(FX(1));
			if (e->f[0] < 0)
			{
				snd(SOUND_CANON);
				if (e->blick == -1)
				{
					part_push(e->xPos + FX(10), e->yPos + FX(2), SMOKE);
					proj_push(e->xPos + FX(17), e->yPos + FX(10), SUCHSCHUSS);
				}
				else
				{
					part_push(e->xPos + FX(45), e->yPos + FX(2), SMOKE);
					proj_push(e->xPos + FX(53), e->yPos + FX(10), SUCHSCHUSS);
				}
				switch (SKILL)
				{
				case 0: e->f[0] += FX(20); break;
				case 1: e->f[0] += FX(15); break;
				case 2: e->f[0] += FX(12); break;
				default: e->f[0] += FX(8); break;
				}
			}
		}
		break;
	case GEGNER_FALLEN:   /* crashing */
		if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocko & B_WAND) ||
		    (e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
			e->Energy = 0;
		if (e->ySpeed > FX(35)) e->yAcc = 0;
		if (e->AnimCount == 0)
			part_push(e->xPos + FX(20 + GetRandom(40)), e->yPos + FX(20 + GetRandom(30)), SMOKE);
		if (PlayerAbstand(e) <= 600 && e->AnimCount == 0 && e->AnimPhase % 2 == 0 && GetRandom(2) == 0)
		{
			part_push(e->xPos - FX(30) + FX(GetRandom(80)), e->yPos - FX(30) + FX(GetRandom(70)), EXPLOSION_MEDIUM2);
			snd(SOUND_EXPLOSION1);
		}
		break;
	default:
		break;
	}

	/* shot down: it falls */
	if (e->Energy <= 0 && e->Handlung != GEGNER_FALLEN)
	{
		e->Handlung = GEGNER_FALLEN;
		e->Energy = FX(40);
		e->ySpeed = FX(3);
		e->yAcc = FX(2);
		e->xAcc = 0;
		e->AnimSpeed = FX(0.5);
		e->xSpeed = e->blick == -1 ? FX(-5) : FX(5);
		if (e->Value1 == 99) e->xSpeed = 0;
	}

	if (e->Handlung != GEGNER_FALLEN) TestDamagePlayers(e, SYNC(FX(4)), 0);
}

static void flugsack_explode(Ent *e)
{
	for (int i = 0; i < 10; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(80)), e->yPos - FX(30) + FX(GetRandom(70)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 5; i++)
		part_push(e->xPos + FX(20) + FX(GetRandom(40)), e->yPos + FX(20) + FX(GetRandom(30)), SPLITTER);
	snd(SOUND_EXPLOSION4);
	pl.Score += 225;
}

TYPEDEF(FLUGSACK) = { flugsack_init, flugsack_ki, flugsack_explode, 0, 0 };
