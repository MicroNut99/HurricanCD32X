/*
 * PRESSWURST - the press (triggers/Trigger_Presswurst.cpp): comes down when the player is below,
 * crushes him, steams, plants a spider bomb (SPIDERBOMB) and goes up again.
 * Own members: f[0] = oldy, f[1] = SmokeCount.
 * Drawing: Hurrican draws only the part below its resting place (SetRect from row
 * oldy - yPos + 251); here the whole picture at yPos - the wall tiles drawn over the sprites
 * hide what is in the ceiling.
 */
static void presswurst_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->blick = -1;
	e->Energy = FX(100);
	e->AnimPhase = 0;
	e->Destroyable = 0;
	e->Active = 1;
	e->OwnDraw = 1;
	e->TestBlock = 0;
	e->f[0] = 0;
	e->f[1] = 0;
}

static void presswurst_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos, 0);
}

static void presswurst_ki(Ent *e)
{
	Box *rect = &GegnerRect[e->GegnerArt];

	/* the player cannot jump through it from below */
	if (SpriteCollision(pl.xpos, pl.ypos, &pl.CollideRect, e->xPos, e->yPos, rect) && e->yPos < pl.ypos && pl.yspeed < 0)
		pl.yspeed = -FMUL(pl.yspeed, FX(0.25));

	/* the whole rectangle pushes the player aside */
	rect->t = 0;
	Wegschieben(e, rect, 0);

	/* the lower part's rectangle */
	rect->t = 251;

	e->blocko = (uint16_t)BlockOben(e->xPos, &e->yPos, e->yPosOld, &e->yPosOld, rect, 0);
	e->blocku = (uint16_t)BlockUntenNormal(e->xPos, e->yPos, e->yPosOld, e->yPosOld, rect);

	switch (e->Handlung)
	{
	/* once at the start */
	case GEGNER_INIT:
		e->yPos -= FX(rect->t);
		e->f[0] = e->yPos;
		e->Handlung = GEGNER_INIT2;
		break;

	/* waits for the player */
	case GEGNER_INIT2:
		if (pl.ypos > e->yPos && PlayerAbstandHoriz(e) < 200 && PlayerAbstandVert(e) < 500)
		{
			e->Handlung = GEGNER_LAUFEN;
			e->ySpeed = FX(10);
			e->yAcc = FX(2.5);
			if (PlayerAbstand(e) < 600) snd(SOUND_PRESSE);
		}
		break;

	/* presses */
	case GEGNER_LAUFEN:
		/* touched while it comes down: he dies */
		if (SpriteCollision(pl.xpos, pl.ypos, &pl.CollideRect, e->xPos, e->yPos, rect) && pl.ypos > e->yPos)
		{
			/* standing: crushed at once */
			if (pl.Handlung != PA_SPRINGEN) DamagePlayer(FX(500), 0);
			/* jumping: pressed down */
			else pl.ypos = e->yPos + FX(GegnerRect[PRESSWURST].b);
		}

		/* on the ground? particles, and then it produces */
		if (e->ySpeed > 0 && ((e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND)))
		{
			if (SpriteCollision(pl.xpos, pl.ypos, &pl.CollideRect, e->xPos, e->yPos, rect) && pl.ypos > e->yPos)
				DamagePlayer(FX(500), 0);
			e->ySpeed = 0;
			e->yAcc = 0;
			e->f[1] = FX(5);
			if (PlayerAbstand(e) < 600) snd(SOUND_DOORSTOP);
			shake_screen(2);
			e->Handlung = GEGNER_SPECIAL;
		}

		/* at the ceiling? waits again */
		if (e->ySpeed < 0 && ((e->blocko & B_WAND) || (e->blocko & B_GEGNERWAND)))
		{
			e->ySpeed = 0;
			e->yAcc = 0;
			e->Handlung = GEGNER_INIT2;
		}
		break;

	/* waits below, then steams */
	case GEGNER_SPECIAL:
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] <= 0)
		{
			e->Handlung = GEGNER_SPECIAL2;
			e->AnimCount = FX(28);
			if (PlayerAbstand(e) < 600) snd(SOUND_STEAM2);
		}
		break;

	/* steams */
	case GEGNER_SPECIAL2:
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] <= 0)
		{
			e->f[1] = FX(0.2);
			part_push(e->xPos + FX(30), e->yPos + FX(300), SMOKE3_LU);
			part_push(e->xPos + FX(135), e->yPos + FX(300), SMOKE3_RU);
		}
		/* the spider */
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount < 0)
		{
			gegner_push(e->xPos + FX(60), e->yPos + FX(357) - FX(GegnerRect[SPIDERBOMB].b), SPIDERBOMB, 0, 0);
			/* up again */
			e->Handlung = GEGNER_LAUFEN;
			e->ySpeed = FX(-15);
			e->yAcc = FX(0.2);
			if (PlayerAbstand(e) < 600) snd(SOUND_PRESSE);
		}
		break;
	}
}

static void presswurst_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(PRESSWURST) = { presswurst_init, presswurst_ki, presswurst_explode, presswurst_draw, 0 };
