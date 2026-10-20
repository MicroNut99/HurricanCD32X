/*
 * MUSHROOM - the mushroom head (triggers/Trigger_Mushroom.cpp): a trampoline: it squashes
 * while the player stands on it, and a jump off it goes higher the sooner it comes.
 * Own members: f[0] = ySize (61 .. 55), f[1] = count.
 * PlayerJumps (called by Hurrican's player when jumping off a MUSHROOM): game/player.c marks
 * the platform the player jumped off in plf_jumped_off; mushroom_ki does PlayerJumps then
 * (in the same step, after the player's move). Its `yadd = PLAYER_JUMPADDSPEED` (the flying
 * sack's speed, no effect on foot) is left out.
 * Simplified: Hurrican draws the head scaled to ySize; here the plain frame, moved down by the
 * squash as Hurrican's top edge.
 */
static void mushroom_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(100);
	e->Destroyable = 0;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	e->f[0] = FX(61);
	e->f[1] = 0;
}

static void mushroom_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos + (FX(61) - e->f[0]) * 2, 0);
}

static void mushroom_player_jumps(Ent *e)
{
	if (e->f[1] > 0)
	{
		pl.AufPlattform = 0;
		pl.yspeed = -FMUL(FX(43), FX(0.5) + e->f[1]);      /* PLAYER_MAXJUMPSPEED */
		if (pl.yspeed > FX(-43)) pl.yspeed = FX(-43);
		pl.Handlung = PA_SPRINGEN;
		snd(SOUND_MUSHROOMJUMP);
	}
}

static void mushroom_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	e->blick = -1;
	if (plf_jumped_off == e) { mushroom_player_jumps(e); plf_jumped_off = 0; }
	switch (e->Handlung)
	{
	case GEGNER_STEHEN:
		e->f[0] += SYNC(FX(10));
		if (e->f[0] > FX(61)) e->f[0] = FX(61);
		if (pl.AufPlattform == e) { e->Handlung = GEGNER_SPRINGEN; e->f[1] = FX(2); }
		break;
	case GEGNER_SPRINGEN:
		e->f[0] -= SYNC(FX(20));
		if (e->f[0] < FX(55)) e->f[0] = FX(55);
		e->f[1] -= SYNC(FX(1));
		if (pl.AufPlattform != e) e->Handlung = GEGNER_STEHEN;
		break;
	}
	PlattformTest(e, &GegnerRect[e->GegnerArt]);
}

static void mushroom_explode(Ent *e) { (void)e; }

TYPEDEF(MUSHROOM) = { mushroom_init, mushroom_ki, mushroom_explode, mushroom_draw, 0 };
