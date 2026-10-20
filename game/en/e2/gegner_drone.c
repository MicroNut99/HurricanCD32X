/*
 * DRONE - the drone (enemies/Gegner_Drone.cpp): buzzes around above the player, turns to him and
 * fires bursts; shot down it falls smoking and explodes on hitting a wall. Value1 99: spat out
 * by the spider machine (flies up first).
 * Own members: f[0] = ShotDelay, f[1] = ActionDelay, i[0] = ShotCount.
 * Hurrican draws the muzzle flame (Gegner.DroneFlame) from DoKI; here it is OwnDraw: the flame
 * (plain, not additive) under the body, drawn as the base Render would.
 * Left out: the walls' "ySpeed *= 1.0f" lines (they do nothing).
 */
static void drone_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(125);
	e->AnimEnde = 7;
	e->AnimSpeed = FX(0.75);
	e->AnimCount = 0;
	e->Destroyable = 1;
	e->OwnDraw = 1;
	e->i[0] = 0;
	e->f[0] = 0;
	e->f[1] = FX(5);
	if (e->Value1 == 99) e->ySpeed = FX(-22);   /* spat out by the spider machine */
}

static void drone_shoot(Ent *e, fx px, fx py)
{
	snd(SOUND_DRONE);
	part_push(e->xPos + px, e->yPos + py, BULLET);
	if (e->blick == 1) proj_push(e->xPos + FX(58), e->yPos + FX(68), DRONEBULLET);
	else proj_push(e->xPos - FX(10), e->yPos + FX(68), DRONEBULLET2);
}

static void drone_turn_check(Ent *e)
{
	if ((e->xPos + FX(30) < pl.xpos + FX(35) && e->blick == -1) ||
	    (e->xPos + FX(30) > pl.xpos + FX(35) && e->blick == 1))
	{
		e->Handlung = GEGNER_DREHEN;
		e->AnimPhase = 6;
		e->AnimCount = 0;
		e->AnimEnde = 11;
	}
}

static void drone_ki(Ent *e)
{
	E2DBG(e);
	/* animation */
	e->AnimCount += SPEEDFACTOR;
	if (e->Handlung == GEGNER_LAUFEN)
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
			if (--e->AnimPhase <= 6)
			{
				e->AnimPhase = e->AnimStart;
				e->AnimEnde = 7;
				e->Handlung = GEGNER_LAUFEN;
				e->f[1] = FX(1);
			}
		}
	}

	/* fly towards the player */
	if (e->Handlung != GEGNER_FALLEN)
	{
		if (e->xPos + FX(30) < pl.xpos + FX(35)) e->xAcc = FX(1.5);
		if (e->xPos + FX(30) > pl.xpos + FX(35)) e->xAcc = FX(-1.5);
		if (e->yPos < pl.ypos - FX(60)) e->yAcc = FX(1.5);
		if (e->yPos > pl.ypos - FX(60)) e->yAcc = FX(-1.5);
		if (e->xSpeed < FX(-20)) e->xSpeed = FX(-20);
		if (e->xSpeed > FX(20)) e->xSpeed = FX(20);
		if (e->ySpeed > FX(10)) e->ySpeed = FX(10);
		if (e->ySpeed < FX(-10) && e->Value1 != 99) e->ySpeed = FX(-10);
	}

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:                /* hover */
		drone_turn_check(e);
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] <= 0 && PlayerAbstand(e) < 600)
		{
			e->Handlung = GEGNER_SCHIESSEN;
			e->i[0] = 6;               /* SKILL_MEDIUM */
			e->f[0] = FX(20);
			drone_shoot(e, FX(33), FX(60));
		}
		break;

	case GEGNER_SCHIESSEN:             /* fire at the player */
		drone_turn_check(e);
		e->f[0] -= SYNC(FX(12));
		if (e->i[0] <= 0 && e->f[0] < FX(12))
		{
			e->f[1] = FX(10);
			e->Handlung = GEGNER_LAUFEN;
		}
		if (e->f[0] <= 0 && PlayerAbstand(e) < 600)
		{
			e->f[0] = FX(20);
			e->i[0]--;
			drone_shoot(e, FX(36), FX(57));
		}
		break;

	case GEGNER_FALLEN:                /* crashing */
		if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocko & B_WAND) ||
		    (e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
			e->Energy = 0;
		if (e->ySpeed > FX(35)) e->yAcc = 0;
		if (e->AnimCount >= FX(0.5))
		{
			e->AnimCount = 0;
			part_push(e->xPos + FX(20 + GetRandom(30)), e->yPos + FX(10 + GetRandom(40)), SMOKE);
			part_push(e->xPos + FX(20 + GetRandom(30)), e->yPos + FX(10 + GetRandom(40)), SMOKE3);
		}
		break;

	default:
		break;
	}

	/* shot down? */
	if (e->Energy <= 0 && e->Handlung != GEGNER_FALLEN)
	{
		e->Handlung = GEGNER_FALLEN;
		e->AnimCount = 0;
		e->Energy = FX(50);
		e->ySpeed = FX(1);
		e->yAcc = FX(2);
		e->xAcc = 0;
	}
}

static void drone_draw(Ent *e)
{
	/* the muzzle flame (Hurrican: drawn in DoKI while ShotDelay >= 12) */
	if (e->Handlung == GEGNER_SCHIESSEN && FINT(e->f[0]) >= 12)
		draw_sprite(&DroneFlame, e->blick == -1 ? 0 : 1, e->xPos - FX(5) + FX(e->blick * 56), e->yPos + FX(60), 0);
	gegner_default_draw(e);
}

static void drone_explode(Ent *e)
{
	snd(SOUND_EXPLOSION3);
	for (int i = 0; i < 5; i++)
	{
		part_push(e->xPos - FX(30) + FX(GetRandom(70)), e->yPos - FX(30) + FX(GetRandom(80)), EXPLOSION_MEDIUM2);
		part_push(e->xPos - FX(30) + FX(GetRandom(70)), e->yPos - FX(30) + FX(GetRandom(80)), SPIDERSPLITTER);
	}
	for (int i = 0; i < 10; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(70)), e->yPos - FX(30) + FX(GetRandom(80)), SPLITTER);
	part_push(e->xPos - FX(24), e->yPos - FX(16), EXPLOSION_GIANT);
	pl.Score += 300;
	gegner_default_explode(e);
}

TYPEDEF(DRONE) = { drone_init, drone_ki, drone_explode, drone_draw, 0 };
