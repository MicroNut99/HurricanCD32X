/*
 * BOULDER - the blue boulder (enemies/Gegner_BlueBoulder.cpp): in the lift level it falls from
 * above the screen onto the player (and breaks on the lift); thrown by the Golem it flies with
 * Value1 / Value2 as its speed.
 * Own members: none.
 * Hurrican's constructor takes "Light" as the lift / Golem switch (false: lift): gegner_push has
 * no Light, so the lift's start speed (g_Fahrstuhl_Speed + Value1) is always set - the Golem's
 * throw overwrites it in DoKI's first step anyway (as it does in Hurrican for a map boulder).
 * Hurrican's "g_Fahrstuhl_yPos > -1" (a lift in the level) is "> 0" here (the engine starts every
 * stage with 0, game/player.c / proj.c test it the same way).
 */
static void blueboulder_init(Ent *e)
{
	e->Handlung = GEGNER_NOTVISIBLE;
	e->Energy = FX(10);
	e->ChangeLight = 0;
	e->Destroyable = 0;
	e->AnimEnde = 14;
	e->AnimStart = 0;
	e->AnimSpeed = FX(1);
	/* Hurrican makes the map's boulders when the level loads, before the lift moves (the
	 * engine sets g_Fahrstuhl_Speed to 0 at each stage's start): here they are made when the
	 * screen comes near, so a map boulder takes the stage-start speed 0, as there; the ones
	 * the FahrstuhlBoss throws take the lift's speed now */
	e->ySpeed = (e->obj >= 0 ? 0 : g_Fahrstuhl_Speed) + FX(e->Value1);
}

static void blueboulder_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	SimpleAnimation(e, 0);

	if (e->blocku & B_WAND) e->Energy = 0;

	switch (e->Handlung)
	{
	case GEGNER_NOTVISIBLE:      /* over the player, to fall from there */
		e->Handlung = GEGNER_FALLEN;
		if (g_Fahrstuhl_yPos > 0)
			e->yPos -= FX(RENDERHEIGHT);
		else
		{
			e->xSpeed = FX(e->Value1);
			e->ySpeed = FX(e->Value2);
			e->yAcc = FX(2);
			e->AnimSpeed = FX(0.4);
		}
		break;
	case GEGNER_FALLEN:
		break;
	default:
		break;
	}

	TestDamagePlayers(e, FX(20), 1);

	/* hit the lift */
	if (g_Fahrstuhl_yPos > 0 && e->yPos >= g_Fahrstuhl_yPos - FX(rect->b)) e->Energy = 0;

	/* or a wall */
	if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocku & B_WAND) || (e->blocko & B_WAND)) e->Energy = 0;
}

static void blueboulder_explode(Ent *e)
{
	snd(SOUND_STONEEXPLODE);
	for (int i = 0; i < 16; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(60)), e->yPos + FX(5) + FX(GetRandom(30)), BOULDER_SMALL);
	for (int i = 0; i < 12; i++)
		part_push(e->xPos - FX(25) + FX(GetRandom(60)), e->yPos - FX(25) + FX(GetRandom(50)), SMOKEBIG);
}

TYPEDEF(BOULDER) = { blueboulder_init, blueboulder_ki, blueboulder_explode, 0, 0 };
