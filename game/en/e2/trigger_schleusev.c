/*
 * SCHLEUSEV - the vertical lock gate (triggers/Trigger_SchleuseV.cpp): blocks the level (pushes
 * the player and the enemies out); a switch sets Handlung GEGNER_OEFFNEN and it slides up 200
 * pixels.
 * Own members: f[0] = the closed yPos (Hurrican keeps it in Value1).
 * Left out: stopping the DOOR sound (StopWave).
 */
static void schleusev_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(100);
	e->Destroyable = 1;
	e->TestBlock = 0;
}

static void schleusev_ki(Ent *e)
{
	E2DBG(e);
	const Box *r = &GegnerRect[e->GegnerArt];
	e->Energy = FX(100);
	e->DamageTaken = 0;

	/* push the player out */
	if (SpriteCollision(e->xPos, e->yPos, r, pl.xpos, pl.ypos, &pl.CollideRect))
	{
		if (pl.xpos < e->xPos) pl.xpos = e->xPos + FX(r->l - pl.CollideRect.r);
		if (pl.xpos > e->xPos) pl.xpos = e->xPos + FX(r->r - pl.CollideRect.l);
	}

	/* and the enemies */
	for (int k = 0; k < MAX_LIVE; k++)
	{
		Ent *o = &ents[k];
		if (o->GegnerArt < 0 || o == e || !o->Active) continue;
		const Box *orr = &GegnerRect[o->GegnerArt];
		if (SpriteCollision(e->xPos, e->yPos, r, o->xPos, o->yPos, orr))
		{
			if (o->xPos < e->xPos) o->xPos = e->xPos + FX(r->l - orr->r);
			if (o->xPos > e->xPos) o->xPos = e->xPos + FX(r->r - orr->l);
		}
	}

	switch (e->Handlung)
	{
	case GEGNER_OEFFNEN:               /* opening */
		e->Handlung = GEGNER_LAUFEN;
		e->f[0] = FX(FINT(e->yPos));
		e->yAcc = FX(-0.5);
		if (!snd_playing(SOUND_DOOR)) snd(SOUND_DOOR);
		break;

	case GEGNER_LAUFEN:                /* opens */
		if (e->ySpeed < FX(-5))
		{
			e->ySpeed = FX(-5);
			e->yAcc = 0;
		}
		if (e->yPos < e->f[0] - FX(201))   /* fully open */
		{
			e->ySpeed = 0;
			e->yAcc = 0;
			e->Handlung = GEGNER_STEHEN;
			snd(SOUND_DOORSTOP);
		}
		break;

	default:
		break;
	}
}

static void schleusev_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(SCHLEUSEV) = { schleusev_init, schleusev_ki, schleusev_explode, 0, 0 };
