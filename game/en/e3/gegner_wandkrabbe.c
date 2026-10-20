/*
 * WANDKRABBE - the wall crab (enemies/Gegner_WandKrabbe.cpp): crawls up and down a wall in the
 * flying level, leaps at the player and clings to him (shaken off by pressing left / right in
 * turns), falls burning when it dies or hits the ground.
 * Own members: f[0] = rot (the spin while flying), i[0] = lastPlayerAction (-1 left, 1 right).
 * Value1 / Value2 hold its place on the player while it clings (as in Hurrican).
 * Drawn: while flying Hurrican spins it (RenderSpriteRotated): here the plain frame; on the wall
 * mirrored and, going up, upside down (RenderMirroredSprite).
 * Hurrican's Aktion[AKTION_LINKS / RECHTS] (the shaking) are the pad's LEFT / RIGHT.
 */
static void wandkrabbe_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(100);
	e->Destroyable = 1;
	e->OwnDraw = 1;
	e->AnimEnde = 10;
	e->AnimStart = 1;
	e->AnimSpeed = FX(0.75);
	e->ySpeed = FX(3);
	e->f[0] = 0;
	e->i[0] = -1;
}

static void wandkrabbe_draw(Ent *e)
{
	const Pic *p = &GegnerPic[e->GegnerArt];
	if (e->Handlung == GEGNER_DREHEN || e->Handlung == GEGNER_FALLEN || e->Handlung == GEGNER_EXPLODIEREN)
		draw_sprite(p, e->AnimPhase, e->xPos, e->yPos, 0);
	else if (e->ySpeed < 0)
		draw_sprite_flipv(p, e->AnimPhase, e->xPos, e->yPos, e->blick != 1);
	else
		draw_sprite(p, e->AnimPhase, e->xPos, e->yPos, e->blick != 1);
}

static void wandkrabbe_spin(Ent *e)
{
	e->f[0] += SYNC(FX(GetRandom(50) + 20));
	while (e->f[0] >= FX(360)) e->f[0] -= FX(360);     /* clampAngle */
}

static void wandkrabbe_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	if (e->Handlung != GEGNER_DREHEN && e->Handlung != GEGNER_EXPLODIEREN) SimpleAnimation(e, 0);

	e->blockl = (uint16_t)BlockLinks(&e->xPos, e->yPos, &e->xPosOld, e->yPosOld, rect, 1);
	e->blockr = (uint16_t)BlockRechts(&e->xPos, e->yPos, &e->xPosOld, e->yPosOld, rect, 1);

	e->blick = (e->blockl & B_WAND) ? 1 : -1;

	/* turn at the top and bottom */
	if ((((e->blocko & B_WAND) || (e->blocko & B_GEGNERWAND)) && e->ySpeed < 0) ||
	    ((((e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND)) && e->ySpeed > 0) && e->Handlung == GEGNER_LAUFEN))
		e->ySpeed = -e->ySpeed;

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:          /* along the wall; the player near: leap */
		if (abs(FINT(pl.ypos - e->yPos)) < 100)
		{
			const fx absx = pl.xpos + FX(35) - (e->xPos + FX(20));
			const fx absy = pl.ypos + FX(40) - (e->yPos + FX(20)) - FX(160);
			e->xSpeed = absx / 10;
			e->ySpeed = absy / 10;
			e->yAcc = FX(3.25);
			e->Handlung = GEGNER_DREHEN;
		}
		break;
	case GEGNER_DREHEN:          /* flies at the player, spinning */
		wandkrabbe_spin(e);
		if (e->ySpeed > FX(20)) e->ySpeed = FX(20);
		break;
	case GEGNER_FALLEN:          /* shaken off */
		wandkrabbe_spin(e);
		if (e->ySpeed > FX(40)) e->ySpeed = FX(40);
		break;
	case GEGNER_EXPLODIEREN:
		wandkrabbe_spin(e);
		if (e->ySpeed > FX(40)) e->ySpeed = FX(40);
		if (((e->blockl & B_WAND) && e->xSpeed < 0) || ((e->blockr & B_WAND) && e->xSpeed > 0)) e->xSpeed = -e->xSpeed;
		e->AnimCount -= SYNC(FX(1));
		while (e->AnimCount <= 0)
		{
			e->AnimCount += FX(0.2);
			part_push(e->xPos + FX(8) + FX(GetRandom(5)), e->yPos + FX(25) + FX(GetRandom(5)), SMOKE3);
			part_push(e->xPos + FX(8) + FX(GetRandom(5)), e->yPos + FX(28) + FX(GetRandom(5)), FUNKE);
		}
		if (e->blocku & B_WAND) e->Energy = 0;
		break;
	case GEGNER_SPECIAL:         /* clings to the player */
		e->xPos = pl.xpos + FX(e->Value1);
		e->yPos = pl.ypos + FX(e->Value2);
		e->blick = e->xPos + FX(15) < pl.xpos + FX(35) ? -1 : 1;
		if (!((pad & PAD_LEFT) && (pad & PAD_RIGHT)))
		{
			if (e->i[0] == -1 && (pad & PAD_RIGHT)) { e->i[0] = 1; e->Value2 += (int16_t)(GetRandom(4) + 2); }
			if (e->i[0] == 1 && (pad & PAD_LEFT)) { e->i[0] = -1; e->Value2 += (int16_t)(GetRandom(4) + 2); }
			if (e->Value2 > 80)
			{
				e->ySpeed = FX(5);
				e->yAcc = FX(3);
				e->Handlung = GEGNER_FALLEN;
			}
		}
		if (pl.Energy <= 0)
		{
			e->ySpeed = FX(5);
			e->yAcc = FX(3);
			e->Handlung = GEGNER_FALLEN;
		}
		break;
	default:
		break;
	}

	/* caught the player: clings to him */
	if (e->Handlung == GEGNER_DREHEN &&
	    SpriteCollision(e->xPos, e->yPos, rect, pl.xpos, pl.ypos, &pl.CollideRect))
	{
		e->f[0] = 0;
		e->xSpeed = 0;
		e->ySpeed = 0;
		e->xAcc = 0;
		e->yAcc = 0;
		e->AnimPhase = 0;
		e->AnimEnde = 0;
		DamagePlayer(FX(10), 0);
		e->Handlung = GEGNER_SPECIAL;
		int v1 = FINT(e->xPos - pl.xpos), v2 = FINT(e->yPos - pl.ypos);
		if (v1 < -10) v1 = -10;
		if (v1 > 50) v1 = 50;
		if (v2 < -30) v2 = -30;
		if (v2 > 80) v2 = 80;
		e->Value1 = (int16_t)v1;
		e->Value2 = (int16_t)v2;
	}

	/* no energy left, or it hit the ground: falls burning */
	if (e->Handlung != GEGNER_EXPLODIEREN && (e->Energy <= 0 || ((e->blocku & B_WAND) && e->Handlung != GEGNER_LAUFEN)))
	{
		e->Energy = FX(100);
		e->AnimEnde = 0;
		e->AnimPhase = 0;
		e->AnimCount = FX(0.1);
		e->ySpeed = FX(-15);
		e->yAcc = FX(3);
		e->xSpeed = e->blick == 1 ? FX(GetRandom(8) + 4) : -FX(GetRandom(8) + 4);
		e->Handlung = GEGNER_EXPLODIEREN;
		snd(SOUND_EXPLOSION1);
		part_push(e->xPos - FX(20), e->yPos, EXPLOSION_MEDIUM2);
	}

	/* out of the screen at the bottom */
	if (e->yPos > cam_yoff() + FX(550)) e->Energy = 0;
}

static void wandkrabbe_explode(Ent *e)
{
	snd(SOUND_EXPLOSION1);
	for (int i = 0; i < 6; i++)
		part_push(e->xPos - FX(20) + FX(GetRandom(10)), e->yPos + FX(GetRandom(30)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 20; i++)
	{
		part_push(e->xPos + FX(GetRandom(30)), e->yPos + FX(GetRandom(64)), FUNKE);
		part_push(e->xPos + FX(GetRandom(25)), e->yPos + FX(GetRandom(50)), SPIDERSPLITTER);
	}
	pl.Score += 100;
	gegner_default_explode(e);
}

TYPEDEF(WANDKRABBE) = { wandkrabbe_init, wandkrabbe_ki, wandkrabbe_explode, wandkrabbe_draw, 0 };
