/*
 * ENDLEVEL - the level's exit (triggers/Trigger_EndLevel.cpp).
 * Own members: none.
 */
static void endlevel_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Destroyable = 0;
	e->Energy = FX(100);
	e->DontMove = 1;
}

static void endlevel_ki(Ent *e)
{
	if (e->Energy > 0 &&
	    e->xPos + FX(120) > cam_xoff() && e->xPos < cam_xoff() + FX(RENDERWIDTH) &&
	    e->yPos + FX(120) > cam_yoff() && e->yPos < cam_yoff() + FX(RENDERHEIGHT) &&
	    SpriteCollision(e->xPos, e->yPos, &GegnerRect[e->GegnerArt], pl.xpos, pl.ypos, &pl.CollideRect))
	{
		scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOLOCK);
		e->Energy = 0;
		pl.FesteAktion = e->Value2;       /* how the player leaves the screen */
		pl.DoFesteAktion = 1;
		stage_clear();
	}
}

TYPEDEF(ENDLEVEL) = { endlevel_init, endlevel_ki, 0, 0, 0 };
