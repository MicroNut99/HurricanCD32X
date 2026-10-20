/*
 * SWITCH - a wall switch (triggers/Trigger_Switch.cpp): the player touching it and pressing
 * "look up" (UP) flips it; every other object with the same Value2 is opened
 * (Handlung = GEGNER_OEFFNEN).
 * Own members: none.
 * Here only the objects in the game (near the screen) can be opened: Hurrican has all the
 * level's objects at once. (No map of the 9 stages has a switch.)
 */
static void switch_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(100);
	e->Destroyable = 0;
	e->TestBlock = 0;
}

static void switch_ki(Ent *e)
{
	if (e->Handlung == GEGNER_STEHEN && (pad & PAD_UP) &&
	    SpriteCollision(e->xPos, e->yPos, &GegnerRect[e->GegnerArt], pl.xpos, pl.ypos, &pl.CollideRect))
	{
		e->AnimPhase = 1;
		e->Handlung = GEGNER_LAUFEN;
		snd(SOUND_SWITCH);
		for (int k = 0; k < MAX_LIVE; k++)
		{
			Ent *o = &ents[k];
			if (o->GegnerArt < 0 || o == e) continue;
			if (o->Value2 == e->Value2) o->Handlung = GEGNER_OEFFNEN;
		}
	}
}

static void switch_explode(Ent *e) { gegner_default_explode(e); }

TYPEDEF(SWITCH) = { switch_init, switch_ki, switch_explode, 0, 0 };
