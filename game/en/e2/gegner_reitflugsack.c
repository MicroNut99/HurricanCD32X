/*
 * REITFLUGSACK - the flying fat sack to ride (enemies/Gegner_ReitFlugsack.cpp): flies around
 * until shot down, then lies on the floor (an arrow blinking above it) until the player jumps on
 * it: the player rides it from then on (PA_SACKREITEN). Value1 1: the screen scrolls up by itself
 * (the player port reads Value1 when the player mounts it).
 * Own members: f[0] = ShotCount (unused, as in Hurrican), f[1] = SmokeCount, f[2] = ArrowCount,
 * i[0] = mounted (see below).
 * Mounting: Hurrican sets Energy 0 at once. Here the sack stays one more step (not drawn), so
 * the player port, which looks at pl.AufPlattform when riding starts, still finds it there
 * (gegner_free clears pl.AufPlattform).
 * Left out: the FLUGSACK music (no such track), Player.SmokeCount = 1 (the player's own),
 * the arrow's green tint (hudarrow.png is drawn plain); ArrowCount counts in DoKI (Hurrican:
 * in DoDraw).
 */
static Pic reitflugsack_arrow;

static void reitflugsack_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(50);
	e->Destroyable = 1;
	e->xSpeed = 0;
	e->ySpeed = 0;
	e->xAcc = FX(2);
	e->yAcc = FX(2);
	e->AnimSpeed = FX(1.5);
	e->AnimEnde = 10;
	e->f[0] = FX(1);
	e->f[1] = FX(0.2);
	e->OwnDraw = 1;
}

static void reitflugsack_load(void)
{
	reitflugsack_arrow = pic("hudarrow.png");
}

static void reitflugsack_draw(Ent *e)
{
	if (e->i[0]) return;
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, e->blick != -1);
	if (e->Handlung == GEGNER_SPECIAL && FINT(e->f[2]) % 2 == 0)
		draw_sprite_flipv(&reitflugsack_arrow, 0, e->xPos + FX(35), e->yPos - FX(40), 0);
}

static void reitflugsack_ki(Ent *e)
{
	E2DBG(e);
	/* mounted in the last step: gone now */
	if (e->i[0]) { e->Energy = 0; return; }

	SimpleAnimation(e, 0);

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:                /* fly around */
		e->blick = (int8_t)(pl.xpos + FX(45) <= e->xPos + FX(40) ? -1 : 1);
		if (e->ySpeed > FX(6)) e->yAcc = FX(-2);
		if (e->ySpeed < FX(-6)) e->yAcc = FX(2);
		if (e->xSpeed > FX(5)) e->xAcc = FX(-2);
		if (e->xSpeed < FX(-5)) e->xAcc = FX(2);

		/* follow the player */
		if (pl.ypos + FX(45) < e->yPos + FX(45) && !(e->blocko & B_WAND)) e->yPos -= SYNC(FX(4));
		if (pl.ypos + FX(45) > e->yPos + FX(45) && !(e->blocku & B_WAND)) e->yPos += SYNC(FX(4));

		if ((e->blocko & B_WAND) || (e->blocko & B_GEGNERWAND)) e->ySpeed = FX(4);
		if ((e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND)) e->ySpeed = FX(-4);
		if ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND)) e->xSpeed = FX(4);
		if ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND)) e->xSpeed = FX(-4);

		/* smoke */
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0)
		{
			e->f[1] += FX(0.1);
			if (e->blick == -1) part_push(e->xPos + FX(66), e->yPos + FX(50), FLUGSACKSMOKE2);
			else part_push(e->xPos, e->yPos + FX(50), FLUGSACKSMOKE);
		}
		break;

	case GEGNER_FALLEN:                /* crashing */
		if (e->ySpeed > 0 && ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM)))
		{
			e->ySpeed = FMUL(e->ySpeed, FX(-0.3));
			if (e->ySpeed > FX(-1))    /* lies still: waits for the player */
			{
				e->ySpeed = 0;
				e->yAcc = 0;
				e->Handlung = GEGNER_SPECIAL;
				e->f[2] = FX(2);
			}
		}
		if (e->ySpeed > FX(35)) e->ySpeed = FX(35);
		if (e->AnimCount == 0)
			part_push(e->xPos + FX(20 + GetRandom(40)), e->yPos + FX(20 + GetRandom(30)), SMOKE);
		if (e->AnimCount == 0 && e->AnimPhase % 2 == 0 && GetRandom(2) == 0)
		{
			part_push(e->xPos + FX(GetRandom(80) - 30), e->yPos + FX(GetRandom(70) - 30), EXPLOSION_MEDIUM2);
			snd(SOUND_EXPLOSION1);
		}
		break;

	case GEGNER_SPECIAL:
		/* the arrow blinks (Hurrican: in DoDraw) */
		e->f[2] -= SYNC(FX(0.2));
		if (e->f[2] < 0) e->f[2] = FX(2);

		if (e->AnimCount == 0)
			part_push(e->xPos + FX(20 + GetRandom(40)), e->yPos + FX(20 + GetRandom(30)), SMOKE);

		PlattformTest(e, &GegnerRect[e->GegnerArt]);

		/* the player stands on it? */
		if (pl.AufPlattform == e && pl.Handlung != PA_SACKREITEN && pl.Handlung != PA_DREHEN)
		{
			pl.Handlung = PA_SACKREITEN;
			pl.CollideRect.l = 15;
			pl.CollideRect.r = 75;
			pl.CollideRect.t = 10;
			pl.CollideRect.b = 80;
			e->i[0] = 1;               /* Energy = 0 in the next step (see above) */
			e->Destroyable = 0;
		}
		break;

	default:
		break;
	}

	/* shot down? */
	if (e->Energy <= 0 && e->Handlung != GEGNER_FALLEN && e->Handlung != GEGNER_SPECIAL)
	{
		e->Destroyable = 0;
		e->Handlung = GEGNER_FALLEN;
		e->Energy = FX(40);
		e->xSpeed = 0;
		e->ySpeed = FX(3);
		e->xAcc = 0;
		e->yAcc = FX(2);
		e->AnimSpeed = FX(0.5);
	}
}

static void reitflugsack_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(REITFLUGSACK) = { reitflugsack_init, reitflugsack_ki, reitflugsack_explode, reitflugsack_draw, reitflugsack_load };
