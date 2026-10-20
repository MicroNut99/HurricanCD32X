/*
 * KETTENGLIED - a link of the Rollmops boss's chain (enemies/Gegner_Kettenglied.cpp): passes the
 * damage it takes on to its parent (the Rollmops), and flies off (GEGNER_SPECIAL, set by the
 * parent) when the chain breaks, fading out.
 * Own members: i[0] = pParent (the parent's slot in ents[], set by the Rollmops; when that slot
 * does not hold a ROLLMOPS, the first ROLLMOPS in the list is used), i[1] = the PC check
 * HB_E3_KETTE's step count. AnimCount is the rotation
 * angle and AnimSpeed the fade (as in Hurrican).
 * Drawn: the plain frame (no rotation, no fading: the link just goes when its fade is over).
 */
static Ent *kettenglied_parent(Ent *e)
{
	const int k = e->i[0];
	if (k >= 0 && k < MAX_LIVE && ents[k].GegnerArt == ROLLMOPS) return &ents[k];
	for (int j = 0; j < MAX_LIVE; j++)
		if (ents[j].GegnerArt == ROLLMOPS) { e->i[0] = j; return &ents[j]; }
	return 0;
}

static void kettenglied_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(1000);
	e->Destroyable = 1;
	e->OwnDraw = 1;
	e->AnimCount = FX(255);
	e->AnimSpeed = FX(-1);
	e->i[0] = -1;
}

static void kettenglied_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void kettenglied_ki(Ent *e)
{
#ifndef OJ32X_MARS
	{   /* PC check (HB_E3_KETTE=n): a link without its Rollmops flies off after n steps */
		const int n = e3test_env("HB_E3_KETTE");
		if (n >= 0 && e->Handlung == GEGNER_STEHEN && !kettenglied_parent(e) && ++e->i[1] > n) e->Handlung = GEGNER_SPECIAL;
	}
#endif
	switch (e->Handlung)
	{
	case GEGNER_STEHEN:          /* damage goes to the parent */
		if (e->Energy < FX(1000))
		{
			Ent *p = kettenglied_parent(e);
			if (p && p->AnimCount <= 0)
			{
				p->AnimCount = FX(0.1);
				p->Energy -= FX(1000) - e->Energy;
			}
			e->Energy = FX(1000);
		}
		break;
	case GEGNER_SPECIAL:         /* flies off */
		e->AnimCount = FX(GetRandom(360));
		e->Destroyable = 0;
		e->xSpeed = FX(GetRandom(40) - 20);
		e->ySpeed = -FX(GetRandom(40) - 20);
		e->yAcc = FX(5);
		e->Handlung = GEGNER_SPECIAL2;
		e->AnimSpeed = FX(255);
		break;
	case GEGNER_SPECIAL2:
	{
		const int bu = BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, &GegnerRect[e->GegnerArt], 0);
		if ((bu & B_WAND) && e->ySpeed > 0)
		{
			e->ySpeed = FMUL(e->ySpeed, FX(-0.3));
			if (e->ySpeed > FX(-0.1)) e->ySpeed = 0;
		}
		e->AnimSpeed -= SYNC(FX(5));
		if (e->AnimSpeed < 0) e->Energy = 0;
		if (e->ySpeed != 0) e->AnimCount += SYNC(FX(50));
		if (e->AnimCount > FX(360)) e->AnimCount = 0;
	} break;
	default:
		break;
	}
}

static void kettenglied_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(KETTENGLIED) = { kettenglied_init, kettenglied_ki, kettenglied_explode, kettenglied_draw, 0 };
