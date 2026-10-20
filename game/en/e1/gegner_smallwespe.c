/*
 * SMALLWESPE - the small wasp (enemies/Gegner_SmallWespe.cpp): flies erratically around the
 * player, bounces off him (and hurts him) when it rams him; shot down it falls and explodes.
 * Own members: none (Value1 / Value2 = the flight target, as Hurrican).
 * Shared: smallwespe_maxspeed (Hurrican's static MAXSPEED in DoKI, shared by all wasps).
 * Falling it is red in Hurrican (DamageTaken 192): here it flashes.
 */
static fx smallwespe_maxspeed = FX(15);

static void smallwespe_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->AnimStart = 0;
	e->AnimEnde = 4;
	e->AnimSpeed = FX(0.5);
	e->Energy = FX(50);
	e->Value1 = (int16_t)(FINT(pl.xpos) - 100 + GetRandom(200));
	e->Value2 = (int16_t)(FINT(pl.ypos) - 100 + GetRandom(50));
	e->Destroyable = 1;
}

static void smallwespe_target(Ent *e, int dx, int dy, int rx, int ry)
{
	e->Value1 = (int16_t)(FINT(pl.xpos) + dx + GetRandom(rx));
	e->Value2 = (int16_t)(FINT(pl.ypos) + dy + GetRandom(ry));
}

static void smallwespe_ki(Ent *e)
{
	SimpleAnimation(e, 0);
	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:      /* waits for the player */
		if (PlayerAbstand(e) < 500)
		{
			smallwespe_target(e, 35 - 50, 40 - 50, 100, 30);
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
		const fx m = smallwespe_maxspeed;
		if (e->xSpeed < -m) e->xSpeed = -m;
		if (e->xSpeed > m) e->xSpeed = m;
		if (e->ySpeed < -m) e->ySpeed = -m;
		if (e->ySpeed > m) e->ySpeed = m;

		const int dx = abs(FINT(e->xPos) - e->Value1), dy = abs(FINT(e->yPos) - e->Value2);
		/* near the player: strike */
		smallwespe_maxspeed = PlayerAbstand(e) < 200 ? FX(20) : FX(8);
		/* target reached or too far: a new one */
		if (PlayerAbstand(e) > 400 || dx * dx + dy * dy < 20 * 20) smallwespe_target(e, -50, -50, 100, 100);
		/* at a wall: a new one too */
		if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocko & B_WAND) ||
		    (e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
			smallwespe_target(e, 35 - 50, 40 - 50, 100, 30);
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

	/* rams the player: bounces off */
	if (e->Handlung != GEGNER_FALLEN && pl.Handlung != PA_TOT && pl.Handlung != PA_EXPLODIEREN &&
	    SpriteCollision(e->xPos, e->yPos, &GegnerRect[e->GegnerArt], pl.xpos, pl.ypos, &pl.CollideRect))
	{
		if (e->xPos + FX(25) > pl.xpos + FX(35) && e->blick == -1 && e->xSpeed < FX(-4))
		{
			e->xSpeed = FX(16);
			DamagePlayer(FX(8), 0);
		}
		else if (e->xPos + FX(25) < pl.xpos + FX(35) && e->blick == 1 && e->xSpeed > FX(4))
		{
			e->xSpeed = FX(-16);
			DamagePlayer(FX(8), 0);
		}
	}

	if (e->Handlung == GEGNER_FALLEN) e->DamageTaken = FX(192);

	/* shot down: it falls */
	if (e->Energy <= 0 && e->Handlung != GEGNER_FALLEN)
	{
		e->Handlung = GEGNER_FALLEN;
		e->Energy = FX(30);
		e->ySpeed = FX(3);
		e->xAcc = 0;
		e->yAcc = FX(2);
		part_push(e->xPos + FX(2), e->yPos - FX(10), EXPLOSION_MEDIUM);
		for (int i = 0; i < 5; i++)
			part_push(e->xPos + FX(2 + GetRandom(20)), e->yPos - FX(10) + FX(GetRandom(10)), SMOKE2);
		e->xSpeed = e->blick == -1 ? FX(-5) : FX(5);
	}
}

static void smallwespe_explode(Ent *e)
{
	part_push(e->xPos + FX(2), e->yPos - FX(10), EXPLOSION_MEDIUM);
	for (int i = 0; i < 3; i++) part_push(e->xPos + FX(15), e->yPos + FX(20), SPLITTER);
	snd(SOUND_EXPLOSION1);
	pl.Score += 200;
}

TYPEDEF(SMALLWESPE) = { smallwespe_init, smallwespe_ki, smallwespe_explode, 0, 0 };
