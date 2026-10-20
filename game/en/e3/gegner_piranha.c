/*
 * PIRANHA - the piranha (enemies/Gegner_Piranha.cpp): swims from one side of its pool to the
 * other; with Value1 == 99 it attacks the player in the water (Value2 == 1: from any distance).
 * Value1 == 98: spat out by the BigFish (flies out, slowing down, then attacks).
 * Own members: f[0] = xsave, f[1] = ysave.
 */
static void piranha_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->HitSound = 1;
	e->AnimStart = 0;
	e->AnimEnde = 10;
	e->AnimSpeed = FX(0.75);
	e->blick = GetRandom(2) == 0 ? -1 : 1;
	e->xSpeed = FX(-8);
	e->ySpeed = 0;
	e->xAcc = 0;
	e->yAcc = 0;
	e->Energy = FX(30);
	if (e->Value1 == 98)       /* spat out by the BigFish */
	{
		const int wert2 = e->Value2;
		e->Value1 = 99;
		e->Value2 = 0;
		e->Handlung = GEGNER_SPECIAL;
		e->xSpeed = FX(-20 - GetRandom(20));
		e->f[0] = e->xSpeed;
		e->ySpeed = FX(wert2) * 3;
		e->f[1] = e->ySpeed;
		e->AnimCount = FX(1);
	}
	else if (e->Value2 != 1)
		e->Value2 = (int16_t)(GetRandom(20) + 2);
	e->Destroyable = 1;
	e->OwnDraw = 1;
}

static void piranha_draw(Ent *e)
{
	/* attacking: frames 0..; only swimming around: frames 20.. */
	const int off = e->Value1 == 99 ? 0 : 20;
	draw_sprite(&GegnerPic[e->GegnerArt], off + e->AnimPhase, e->xPos, e->yPos, e->blick != -1);
}

static void piranha_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	if (e->Handlung != GEGNER_SPECIAL) SimpleAnimation(e, 0);

	/* turn at walls */
	if ((e->xSpeed < 0 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))) ||
	    (e->xSpeed > 0 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND))))
	{
		e->xSpeed = 0;
		e->Handlung = GEGNER_DREHEN;
		e->AnimPhase = 10;
		e->AnimStart = 0;
		e->AnimEnde = 15;
	}

	/* the player in the water and near: up or down to him */
	if (pl.InLiquid && e->Value1 == 99 && (PlayerAbstand(e) <= 150 || e->Value2 == 1))
	{
		if (pl.ypos + FX(4 + e->Value2) < e->yPos &&
		    !(e->blocko & B_WAND) && !(e->blocko & B_GEGNERWAND) && (e->blocko & B_LIQUID))
			e->yPos -= SYNC(FX(4));
		if (pl.ypos + FX(4 + e->Value2) > e->yPos)
		{
			const int bu = BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);
			if (!(bu & B_WAND) && !(bu & B_GEGNERWAND)) e->yPos += SYNC(FX(4));
		}
	}

	switch (e->Handlung)
	{
	case GEGNER_SPECIAL:
		e->AnimCount -= SYNC(FX(0.1));
		e->xSpeed = FMUL(e->f[0], e->AnimCount);
		e->ySpeed = FMUL(e->f[1], e->AnimCount);
		if (e->AnimCount < 0)
		{
			e->xSpeed = 0;
			e->ySpeed = 0;
			e->Value2 = 1;
			e->Value1 = 99;
			e->Handlung = GEGNER_LAUFEN;
		}
		break;
	case GEGNER_LAUFEN:
		if (pl.InLiquid && e->Value1 == 99 && (PlayerAbstand(e) <= 150 || e->Value2 == 1))
		{
			e->xSpeed = e->blick == -1 ? FX(-14) : FX(14);
			/* past the player? then turn */
			if ((e->blick == -1 && pl.xpos > e->xPos + FX(rect->r - 20)) ||
			    (e->blick == 1 && pl.xpos + FX(pl.CollideRect.r) < e->xPos))
			{
				e->xSpeed = 0;
				e->Handlung = GEGNER_DREHEN;
				e->AnimPhase = 10;
				e->AnimStart = 0;
				e->AnimEnde = 15;
			}
		}
		else
			e->xSpeed = e->blick == -1 ? -FX(GetRandom(10) + 4) : FX(GetRandom(10) + 4);
		break;
	case GEGNER_DREHEN:
		if (e->AnimPhase == e->AnimStart)
		{
			e->Handlung = GEGNER_LAUFEN;
			e->AnimEnde = 10;
			e->AnimStart = 0;
			e->AnimPhase = 0;
			if (e->blick == -1) { e->blick = 1; e->xSpeed = FX(8); }
			else { e->blick = -1; e->xSpeed = FX(-8); }
		}
		break;
	default:
		break;
	}

	TestDamagePlayers(e, SYNC(FX(1)), 0);
}

static void piranha_explode(Ent *e)
{
	for (int i = 0; i < 3; i++)
		part_push(e->xPos - FX(20) + FX(GetRandom(45)), e->yPos - FX(5) + FX(GetRandom(30)), PIRANHATEILE);
	for (int i = 0; i < 3; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(45)), e->yPos + FX(10) + FX(GetRandom(30)), BUBBLE);
	part_push(e->xPos + FX(2), e->yPos - FX(5), PIRANHABLUT);
	snd(SOUND_EXPLOSION1);
	pl.Score += 200;
	gegner_default_explode(e);
}

TYPEDEF(PIRANHA) = { piranha_init, piranha_ki, piranha_explode, piranha_draw, 0 };
