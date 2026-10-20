/*
 * STAHLMUECKE - the steel mosquito (enemies/Gegner_Stahlmuecke.cpp): flies at the player once
 * he is near; shot, it falls smoking and explodes at the next wall / floor.
 * Own members: none (Value1 / Value2 = the flight target, as Hurrican).
 * Left out: the red tint while falling.
 */
static void stahlmuecke_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->AnimStart = 0;
	e->AnimEnde = 8;
	e->AnimSpeed = FX(0.5);
	e->Energy = FX(10);
	e->Value1 = (int16_t)(FINT(pl.xpos) - 100 + GetRandom(200));
	e->Value2 = (int16_t)(FINT(pl.ypos) - 100 + GetRandom(200));
	e->Destroyable = 1;
	e->OwnDraw = 1;
}

static void stahlmuecke_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, e->blick == 1);
}

static void stahlmuecke_target(Ent *e, int dx, int dy)
{
	e->Value1 = (int16_t)(FINT(pl.xpos) + dx + GetRandom(100));
	e->Value2 = (int16_t)(FINT(pl.ypos) + dy + GetRandom(100));
}

static void stahlmuecke_ki(Ent *e)
{
	SimpleAnimation(e, 0);
	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:      /* waits for the player */
		if ((e->xSpeed < 0 && (e->blockl & B_WAND)) || (e->xSpeed > 0 && (e->blockr & B_WAND))) e->xSpeed = -e->xSpeed;
		if (PlayerAbstand(e) < 500)
		{
			stahlmuecke_target(e, 35 - 50, 40 - 50);
			e->Handlung = GEGNER_VERFOLGEN;
		}
		break;
	case GEGNER_VERFOLGEN:   /* follows the player */
	{
		if (FX(e->Value1) < e->xPos && !(e->blockl & B_WAND)) { e->xAcc = FX(-3); e->blick = -1; }
		if (FX(e->Value1) > e->xPos && !(e->blockr & B_WAND)) { e->xAcc = FX(3); e->blick = 1; }
		if (FX(e->Value2) < e->yPos)
		{
			if (!(e->blocko & B_WAND)) e->yAcc = FX(-3);
			else { e->Handlung = GEGNER_VERFOLGEN; e->ySpeed = FX(20); }
		}
		if (FX(e->Value2) > e->yPos)
		{
			if (!(e->blocku & B_WAND)) e->yAcc = FX(3);
			else { e->ySpeed = FX(-10); e->yAcc = 0; }
		}
		if (e->xSpeed < FX(-10)) e->xSpeed = FX(-10);
		if (e->xSpeed > FX(10)) e->xSpeed = FX(10);
		if (e->ySpeed < FX(-10)) e->ySpeed = FX(-10);
		if (e->ySpeed > FX(10)) e->ySpeed = FX(10);

		const int dx = abs(FINT(e->xPos) - e->Value1), dy = abs(FINT(e->yPos) - e->Value2);
		/* target reached or too far: a new one */
		if (PlayerAbstand(e) > 400 || dx * dx + dy * dy < 20 * 20) stahlmuecke_target(e, -50, -50);
		/* at a wall: a new one too */
		if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocko & B_WAND) ||
		    (e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
			stahlmuecke_target(e, 35 - 50, 40 - 50);
		/* not into the water */
		if ((e->blockl & B_LIQUID) || (e->blockr & B_LIQUID) || (e->blocko & B_LIQUID) || (e->blocku & B_LIQUID))
		{
			e->Handlung = GEGNER_LAUFEN;
			if (e->ySpeed > 0) e->ySpeed = -e->ySpeed;
			e->xAcc = 0;
			e->yAcc = 0;
		}
		break;
	}
	case GEGNER_FALLEN:      /* crashing */
		if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocko & B_WAND) ||
		    (e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
			e->Energy = 0;
		if (e->ySpeed > FX(35)) e->yAcc = 0;
		if (e->AnimCount == 0 && e->AnimPhase % 2 == 0) part_push(e->xPos + FX(12), e->yPos + FX(5), SMOKE);
		break;
	default:
		break;
	}

	if (e->Handlung != GEGNER_FALLEN) TestDamagePlayers(e, SYNC(FX(1.5)), 0);

	/* shot: it falls */
	if (e->Energy <= 0 && e->Handlung != GEGNER_FALLEN)
	{
		e->Handlung = GEGNER_FALLEN;
		e->Energy = FX(30);
		e->ySpeed = FX(3);
		e->xAcc = 0;
		e->yAcc = FX(2);
		part_push(e->xPos + FX(2), e->yPos - FX(10), EXPLOSION_MEDIUM3);
		for (int i = 0; i < 5; i++)
			part_push(e->xPos + FX(2 + GetRandom(20)), e->yPos - FX(10) + FX(GetRandom(10)), SMOKE2);
		e->xSpeed = e->blick == -1 ? FX(-5) : FX(5);
	}
}

static void stahlmuecke_explode(Ent *e)
{
	part_push(e->xPos + FX(2), e->yPos - FX(10), EXPLOSION_MEDIUM3);
	for (int i = 0; i < 3; i++) part_push(e->xPos + FX(15), e->yPos + FX(20), SPLITTER);
	snd(SOUND_EXPLOSION1);
	pl.Score += 200;
}

TYPEDEF(STAHLMUECKE) = { stahlmuecke_init, stahlmuecke_ki, stahlmuecke_explode, stahlmuecke_draw, 0 };
