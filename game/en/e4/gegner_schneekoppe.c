/*
 * SCHNEEKOPPE - the snow king's shot (enemies/Gegner_Schneekoppe.cpp): flies at angle Value1
 * with speed Value2 under gravity, bursts on walls or on the player.
 * Own members: i[0] = isHit.
 * Drawing: Hurrican rotates it along its flight; here the plain frame.
 */
static void schneekoppe_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(600);
	e->Destroyable = 1;
	e->xSpeed = sin_d(e->Value1) * e->Value2;
	e->ySpeed = cos_d(e->Value1) * e->Value2;
	e->yAcc = FX(6);
	e->i[0] = 0;
	e->OwnDraw = 1;
}

static void schneekoppe_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void schneekoppe_ki(Ent *e)
{
	e->blick = 1;
	SimpleAnimation(e, 0);
	if (e->DamageTaken > 0) e->i[0] = 1;

	/* hit a wall? */
	if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocko & B_WAND) || (e->blocku & B_WAND) ||
	    (e->blocku & B_PLATTFORM))
		e->Energy = 0;

	TestDamagePlayers(e, FX(20), 1);
}

static void schneekoppe_explode(Ent *e)
{
	int i;
	for (i = 0; i < 10; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(20)), e->yPos - FX(10) + FX(GetRandom(20)), BLUE_EXPLOSION);
	snd(SOUND_EXPLOSION1);
	gegner_default_explode(e);
}

TYPEDEF(SCHNEEKOPPE) = { schneekoppe_init, schneekoppe_ki, schneekoppe_explode, schneekoppe_draw, 0 };
