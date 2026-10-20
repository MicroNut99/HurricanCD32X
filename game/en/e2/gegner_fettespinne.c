/*
 * FETTESPINNE - the fat spider (enemies/Gegner_FetteSpinne.cpp): crawls towards the player on
 * the floor or the ceiling, jumps up to the ceiling when shot at, drops onto the player from it.
 * Own members: f[0] = ShotDelay (unused, as in Hurrican), i[0] = WalkState (0 floor, 1 ceiling).
 * The player's Aktion[AKTION_SHOOT] is read as the shoot button (pad & PAD_B).
 */
static void fettespinne_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(180);
	e->AnimStart = 0;
	e->AnimEnde = 9;
	e->AnimSpeed = FX(0.5);
	e->Destroyable = 1;
	e->f[0] = FX(10);
	e->i[0] = 0;
	e->OwnDraw = 1;
}

static void fettespinne_draw(Ent *e)
{
	/* upside down on the ceiling */
	if (e->i[0] != 0) draw_sprite_flipv(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, e->blick == 1);
	else draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, e->blick == 1);
}

static void fettespinne_crouch(Ent *e, fx count)
{
	e->Handlung = GEGNER_SPECIAL;
	e->AnimPhase = 20;
	e->AnimEnde = 0;
	e->AnimSpeed = 0;
	e->AnimCount = count;
	e->xSpeed = 0;
	e->xAcc = 0;
}

static void fettespinne_ki(Ent *e)
{
	E2DBG(e);
	if (e->AnimEnde > 0)
	{
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			if (++e->AnimPhase >= e->AnimEnde) e->AnimPhase = e->AnimStart;
		}
	}

	/* turn when shot from behind */
	if (e->DamageTaken > 0) e->blick = (int8_t)(pl.xpos + FX(40) < e->xPos + FX(70) ? -1 : 1);

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		e->xAcc = FX(5) * e->blick;
		if (e->xSpeed < FX(-12)) e->xSpeed = FX(-12);
		if (e->xSpeed > FX(12)) e->xSpeed = FX(12);

		/* turn at walls */
		if ((e->xSpeed < 0 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))) ||
		    (e->xSpeed > 0 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND))))
			e->blick = (int8_t)-e->blick;

		/* on the floor: the player shoots at the spider? */
		if (e->i[0] == 0)
			if (((pl.xpos + FX(35) < e->xPos + FX(50) && pl.Blickrichtung == 1 && e->blick == -1) ||
			     (pl.xpos + FX(35) >= e->xPos + FX(50) && pl.Blickrichtung == -1 && e->blick == 1)) &&
			    (pad & PAD_B))
			{
				/* a ceiling above (Hurrican looks at the tiles from its own row on down) */
				int block = 0, a = 0;
				while (a < 10 && !block)
				{
					if (TileBlock(FINT(e->xPos) / TILESIZE, FINT(e->yPos) / TILESIZE + a) & B_WAND) block = 1;
					a++;
				}
				if (block) fettespinne_crouch(e, FX(2));
			}

		/* on the ceiling: drop onto the player */
		if (e->i[0] == 1)
			if (e->yPos < pl.ypos && PlayerAbstandHoriz(e) < 20) fettespinne_crouch(e, 0);

		/* too far from the player? turn */
		if (e->xPos + FX(50) < pl.xpos + FX(35) - FX(200)) e->blick = 1;
		if (e->xPos + FX(50) > pl.xpos + FX(35) + FX(200)) e->blick = -1;
		break;

	case GEGNER_SPECIAL:               /* crouch a moment */
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount < 0)
		{
			e->Handlung = GEGNER_SPRINGEN;
			e->AnimPhase = 10;
			e->AnimEnde = 19;
			e->AnimSpeed = FX(0.5);
			e->AnimStart = 19;
			if (e->i[0] == 0) e->ySpeed = FX(-25);
			else { e->ySpeed = FX(20); e->yAcc = FX(2); }
		}
		break;

	case GEGNER_SPRINGEN:              /* turning in the air */
		if ((e->ySpeed < 0 && (e->blocko & B_WAND)) || (e->ySpeed > 0 && (e->blocku & B_WAND)) ||
		    (e->ySpeed > 0 && (e->blocku & B_PLATTFORM)))
		{
			e->AnimPhase = 0;
			e->AnimStart = 0;
			e->AnimEnde = 9;
			e->AnimSpeed = FX(0.5);
			e->ySpeed = 0;
			e->yAcc = 0;
			e->Handlung = GEGNER_LAUFEN;
			e->i[0] = e->i[0] == 0 ? 1 : 0;
		}
		break;

	default:
		break;
	}
}

static void fettespinne_explode(Ent *e)
{
	for (int i = 0; i < 15; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(170)), e->yPos - FX(30) + FX(GetRandom(72)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 4; i++)
		part_push(e->xPos + FX(GetRandom(100)), e->yPos + FX(GetRandom(30)), SPLITTER);
	for (int i = 0; i < 16; i++)
		part_push(e->xPos + FX(GetRandom(100)), e->yPos + FX(GetRandom(30)), SPIDERSPLITTER);
	snd(SOUND_EXPLOSION3);
	pl.Score += 800;
	gegner_default_explode(e);
}

TYPEDEF(FETTESPINNE) = { fettespinne_init, fettespinne_ki, fettespinne_explode, fettespinne_draw, 0 };
