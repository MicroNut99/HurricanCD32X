/*
 * MINIDRAGON - the extending mini dragon (enemies/Gegner_Minidragon.cpp, class GegnerMiniDragon):
 * waits at its place, shoots out towards the player when he comes near, wobbles at his height
 * on its neck and spits fireballs; shot, its segments explode one by one. Value2 = 99: thrown
 * by the dragon boss (falls, lands, then behaves as the others).
 * Own members: i[0] = Segments, f[0] = NewX, f[1] = NewY, f[2] = OldX, f[3] = OldY,
 *              f[4] = ShotDelay.
 * Drawing: the neck segments of a thrown one are not drawn while it falls (Hurrican draws them
 * from uninitialised OldX / OldY there).
 */
static void minidragon_init(Ent *e)
{
	e->i[0] = 6;
	e->f[4] = FX(4);
	e->Energy = FX(200);
	e->Destroyable = 0;
	e->OwnDraw = 1;

	/* thrown by the dragon? */
	if (e->Value2 == 99)
	{
		e->ySpeed = -(FX(13) + FX(GetRandom(4)));
		e->yAcc = FX(3);
		e->xSpeed = FX(GetRandom(10) - 5) / 2;
		e->Handlung = GEGNER_FALLEN;
		e->AnimCount = 0;
		e->AnimStart = 5;
		e->AnimPhase = 5;
		e->AnimEnde = 15;
		e->AnimSpeed = FX(0.5);
	}
	else
	{
		e->Handlung = GEGNER_INIT;
		e->AnimEnde = 3;
		e->AnimSpeed = FX(1);
	}
}

static void minidragon_draw(Ent *e)
{
	const Pic *p = &GegnerPic[e->GegnerArt];
	const fx muly = (e->f[3] - e->yPos) / 6;
	const fx mulx = (e->f[2] - e->xPos) / 6;
	int mirrored = pl.xpos + FX(35) < e->xPos + FX(30);
	int i, a;

	if (e->Handlung != GEGNER_FALLEN)
		for (i = 0; i < e->i[0]; i++)
			draw_sprite(p, 4, e->xPos + (6 - i) * mulx, e->yPos + (6 - i) * muly, 0);

	/* the head */
	e->blick = mirrored ? -1 : 1;
	switch (FINT(e->f[4]))
	{
	case 0: a = 1; break;
	case 8:
	case 7: a = 2; break;
	case 6: a = 1; break;
	default: a = 0;
	}
	if (e->Handlung == GEGNER_FALLEN)
	{
		a = e->AnimPhase;
		mirrored = e->xSpeed > 0;
		e->blick = -1;
	}
	if (e->i[0] == 6) draw_sprite(p, a, e->xPos + FX(15) * e->blick, e->yPos, mirrored);
}

static void minidragon_ki(Ent *e)
{
	if (((e->blocko & B_WAND) && e->ySpeed < 0) || ((e->blocku & B_WAND) && e->ySpeed > 0)) e->ySpeed = -e->ySpeed;
	if (((e->blockl & B_WAND) && e->xSpeed < 0) || ((e->blockr & B_WAND) && e->xSpeed > 0)) e->xSpeed = -e->xSpeed;

	switch (e->Handlung)
	{
	case GEGNER_FALLEN:
		SimpleAnimation(e, 0);
		if (e->ySpeed > FX(40)) e->ySpeed = FX(40);
		/* landed? */
		e->blocku = (uint16_t)BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, &GegnerRect[e->GegnerArt], 0);
		if (e->blocku & B_WAND)
		{
			int i;
			e->xSpeed = 0;
			e->ySpeed = 0;
			e->xAcc = 0;
			e->yAcc = 0;
			e->Handlung = GEGNER_INIT;
			e->AnimEnde = 0;
			e->AnimPhase = 0;
			snd(SOUND_EXPLOSION1);
			for (i = 0; i < 10; i++)
				part_push(e->xPos + FX(10) + FX(GetRandom(30)), e->yPos + FX(GetRandom(10)), SPIDERSPLITTER);
			for (i = 0; i < 3; i++)
				part_push(e->xPos - FX(10) + FX(GetRandom(30)), e->yPos - FX(20) + FX(GetRandom(20)), EXPLOSION_MEDIUM3);
		}
		break;

	/* keep the place */
	case GEGNER_INIT:
		e->f[2] = e->xPos;
		e->f[3] = e->yPos;
		/* the player near enough? */
		if (PlayerAbstandHoriz(e) < 300 && PlayerAbstandVert(e) < 300)
		{
			e->Handlung = GEGNER_INIT2;
			if (pl.xpos + FX(35) < e->xPos + FX(30)) { e->xSpeed = FX(10); e->xAcc = FX(-2); }
			else { e->xSpeed = FX(-10); e->xAcc = FX(2); }
			if (pl.ypos + FX(40) < e->yPos + FX(20)) { e->ySpeed = FX(10); e->yAcc = FX(-2); }
			else { e->ySpeed = FX(-10); e->yAcc = FX(2); }
			if (!snd_playing(SOUND_STEAM) && !snd_playing(SOUND_SPIDERSCREAM))
			{
				snd(SOUND_STEAM);
				snd(SOUND_SPIDERSCREAM);
			}
			e->Destroyable = 1;
			e->AnimCount = FX(5);
		}
		break;

	/* a short stab out */
	case GEGNER_INIT2:
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount <= 0)
		{
			e->AnimCount = 0;
			e->Handlung = GEGNER_STEHEN;
		}
		break;

	/* wobble at the player's height */
	case GEGNER_STEHEN:
	{
		fx xdiv, ydiv;
		if (PlayerAbstand(e) < 600) e->f[4] -= SYNC(FX(0.8));
		if (e->f[4] < 0)
		{
			e->f[4] = FX(9);
			snd(SOUND_FIREBALL);
			proj_winkel = FX(90) * e->blick;
			proj_push(e->xPos + FX(e->blick * 10), e->yPos, FIREBALL);
		}
		xdiv = (pl.xpos + FX(30)) - (e->f[2] + FX(30));
		ydiv = (pl.ypos + FX(35)) - (e->f[3] + FX(20));
		if (xdiv < FX(-80)) xdiv = FX(-80);
		if (xdiv > FX(80)) xdiv = FX(80);
		if (ydiv < FX(-80)) ydiv = FX(-80);
		if (ydiv > FX(80)) ydiv = FX(80);
		e->f[0] = e->f[2] + xdiv;
		e->f[1] = e->f[3] + ydiv;
		e->xAcc = e->xPos < e->f[0] ? FX(2) : FX(-2);
		e->yAcc = e->yPos < e->f[1] ? FX(2) : FX(-2);
		if (e->xSpeed < FX(-8)) e->xSpeed = FX(-8);
		if (e->xSpeed > FX(8)) e->xSpeed = FX(8);
		if (e->ySpeed < FX(-8)) e->ySpeed = FX(-8);
		if (e->ySpeed > FX(8)) e->ySpeed = FX(8);
	} break;

	/* the segments explode one after the other */
	case GEGNER_EXPLODIEREN:
		e->Energy = FX(100);
		e->Destroyable = 0;
		e->f[4] -= SYNC(FX(1));
		if (e->f[4] < 0)
		{
			const fx muly = (e->f[3] - e->yPos) / 6;
			const fx mulx = (e->f[2] - e->xPos) / 6;
			const fx sx = e->xPos + (6 - e->i[0]) * mulx, sy = e->yPos + (6 - e->i[0]) * muly;
			int i;
			snd(SOUND_EXPLOSION1);
			part_push(sx, sy, EXPLOSION_MEDIUM2);
			for (i = 0; i < 3; i++) part_push(sx + FX(GetRandom(40)), sy + FX(GetRandom(30)), SPIDERSPLITTER);
			for (i = 0; i < 10; i++) part_push(sx + FX(GetRandom(40)), sy + FX(GetRandom(30)), FUNKE);
			e->f[4] = FX(2);
			e->i[0]--;
			if (e->i[0] == 0) e->Energy = 0;
		}
		break;
	}

	if (e->Energy < FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Handlung = GEGNER_EXPLODIEREN;
		e->Energy = FX(1);
		e->f[4] = 0;
		e->xSpeed = 0;
		e->ySpeed = 0;
		e->xAcc = 0;
		e->yAcc = 0;
		snd(SOUND_EXPLOSION3);
		part_push(e->xPos - FX(26), e->yPos - FX(22), EXPLOSION_BIG);
	}

	if (e->Handlung != GEGNER_EXPLODIEREN) TestDamagePlayers(e, SYNC(FX(4)), 0);
}

static void minidragon_explode(Ent *e)
{
	pl.Score += 250;
	gegner_default_explode(e);
}

TYPEDEF(MINIDRAGON) = { minidragon_init, minidragon_ki, minidragon_explode, minidragon_draw, 0 };
