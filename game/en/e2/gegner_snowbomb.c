/*
 * SNOWBOMBE - the big snow ball (enemies/Gegner_SnowBomb.cpp): rolls off towards the player when
 * it comes onto the screen, falls and bounces, bounces off walls (losing energy there), the player
 * can stand on it; bursts into small snow bombs.
 * Own members: none.
 */
static void snowbomb_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->Energy = FX(30);
	e->Destroyable = 0;
	e->AnimSpeed = FX(0.5);
	e->AnimEnde = 19;
	e->OwnDraw = 1;
}

static void snowbomb_draw(Ent *e)
{
	const int anim = e->xSpeed < 0 ? e->AnimPhase : e->AnimEnde - e->AnimPhase;
	if (e->Handlung != GEGNER_INIT) draw_sprite(&GegnerPic[e->GegnerArt], anim, e->xPos, e->yPos, 0);
}

static void snowbomb_ki(Ent *e)
{
	E2DBG(e);
	const Box *rect = &GegnerRect[e->GegnerArt];
	PlattformTest(e, rect);
	SimpleAnimation(e, 0);

	switch (e->Handlung)
	{
	case GEGNER_INIT:                  /* on the screen: roll off */
		e->xSpeed = pl.xpos > e->xPos ? FX(15) : FX(-15);
		e->Handlung = GEGNER_LAUFEN;
		break;

	case GEGNER_LAUFEN:                /* rolling */
		if (!(e->blocku & B_WAND))
		{
			e->ySpeed = FX(10);
			e->yAcc = FX(5);
			e->Handlung = GEGNER_FALLEN;
		}
		break;

	case GEGNER_FALLEN:                /* falling, bouncing on the floor */
		if (e->ySpeed > 0 && ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM)))
		{
			e->ySpeed = FMUL(e->ySpeed, FX(-0.7));
			if (e->ySpeed > FX(-1))
			{
				e->ySpeed = 0;
				e->yAcc = 0;
				e->Handlung = GEGNER_LAUFEN;
				e->blocku = (uint16_t)BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);
			}
			for (int i = 0; i < 10; i++)
				part_push(e->xPos + FX(GetRandom(50)), e->yPos + FX(GetRandom(20) + 50), WATERFLUSH2);
			snd(SOUND_LANDEN);
		}
		if (e->ySpeed > FX(30)) e->ySpeed = FX(30);
		break;

	default:
		break;
	}

	/* bounce off walls (losing energy) */
	if ((e->xSpeed < 0 && (e->blockl & B_WAND)) || (e->xSpeed > 0 && (e->blockr & B_WAND)))
	{
		e->Energy -= FX(1);
		e->xSpeed = -e->xSpeed;
	}
}

static void snowbomb_explode(Ent *e)
{
	for (int i = 0; i < 8; i++)
		proj_push(e->xPos + FX(GetRandom(60)), e->yPos + FX(GetRandom(50)), SNOWBOMBSMALL);
	for (int i = 0; i < 20; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(70)), e->yPos - FX(10) + FX(GetRandom(70)), SNOWFLUSH);
	snd(SOUND_LANDEN);
	if (pl.AufPlattform == e) pl.AufPlattform = 0;
	gegner_default_explode(e);
}

TYPEDEF(SNOWBOMBE) = { snowbomb_init, snowbomb_ki, snowbomb_explode, snowbomb_draw, 0 };
