/*
 * SCHABE - the cockroach (enemies/Gegner_Schabe.cpp): crawls along walls, floors and ceilings
 * around corners and fires homing shots.
 * Own members: i[0] = winkel (degrees: 0, 90, 180, 270), i[1] = mirror, f[0] = ShotDelay.
 * Drawing: no rotated sprites - at 180 degrees (on the ceiling) drawn upside down, at 90 / 270
 * (on a wall) the plain frame.
 */
#define SCHABE_SPEED FX(5)

static void schabe_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(10);
	e->Destroyable = 1;
	e->AnimSpeed = FX(0.5);
	e->AnimEnde = 10;
	e->ySpeed = 0;
	e->i[0] = 0;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	e->f[0] = FX(5 + GetRandom(5));
	if (e->Value1 != 0) { e->xSpeed = -SCHABE_SPEED; e->i[1] = 1; }
	else { e->xSpeed = SCHABE_SPEED; e->i[1] = 0; }
}

static int schabe_w(int x, int y) { return TileBlock(x, y) & B_WAND; }

static void schabe_snap(Ent *e, int xl, int yl)
{
	e->xPos = FX(xl * 20);
	e->yPos = FX(yl * 20);
}

static void schabe_ki(Ent *e)
{
	/* shoot */
	if (PlayerAbstand(e) < 600) e->f[0] -= SYNC(FX(1));
	if (e->f[0] < 0)
	{
		e->f[0] = FX(8 + (4 - SKILL) * 2);
		snd(SOUND_CANON);
		proj_push(e->xPos + FX(5), e->yPos + FX(5), SUCHSCHUSS);
	}

	SimpleAnimation(e, 0);

	/* crawl along the walls */
	const int xl = FINT(e->xPos + FX(10)) / TILESIZE;
	const int yl = FINT(e->yPos + FX(10)) / TILESIZE;

	if (e->xSpeed < 0)   /* to the left */
	{
		if (!schabe_w(xl, yl + 1) && !schabe_w(xl - 1, yl - 1) && !schabe_w(xl + 1, yl - 1) &&
		    !schabe_w(xl - 1, yl + 1) && schabe_w(xl + 1, yl + 1))
		{
			e->xSpeed = 0; e->ySpeed = SCHABE_SPEED; schabe_snap(e, xl, yl);
		}
		if (!schabe_w(xl, yl - 1) && !schabe_w(xl - 1, yl - 1) && schabe_w(xl + 1, yl - 1) &&
		    !schabe_w(xl - 1, yl + 1) && !schabe_w(xl + 1, yl + 1))
		{
			e->xSpeed = 0; e->ySpeed = -SCHABE_SPEED; schabe_snap(e, xl, yl);
		}
	}
	if (e->xSpeed > 0)   /* to the right */
	{
		if (!schabe_w(xl, yl + 1) && !schabe_w(xl - 1, yl - 1) && !schabe_w(xl + 1, yl - 1) &&
		    schabe_w(xl - 1, yl + 1) && !schabe_w(xl + 1, yl + 1))
		{
			e->xSpeed = 0; e->ySpeed = SCHABE_SPEED; schabe_snap(e, xl, yl);
		}
		if (!schabe_w(xl, yl - 1) && schabe_w(xl - 1, yl - 1) && !schabe_w(xl + 1, yl - 1) &&
		    !schabe_w(xl - 1, yl + 1) && !schabe_w(xl + 1, yl + 1))
		{
			e->xSpeed = 0; e->ySpeed = -SCHABE_SPEED; schabe_snap(e, xl, yl);
		}
	}
	if (e->ySpeed < 0)   /* up */
	{
		if (!schabe_w(xl + 1, yl) && !schabe_w(xl - 1, yl - 1) && schabe_w(xl + 1, yl + 1) &&
		    !schabe_w(xl - 1, yl + 1) && !schabe_w(xl + 1, yl - 1))
		{
			e->xSpeed = SCHABE_SPEED; e->ySpeed = 0; schabe_snap(e, xl, yl);
		}
		if (!schabe_w(xl - 1, yl) && !schabe_w(xl - 1, yl - 1) && !schabe_w(xl + 1, yl - 1) &&
		    schabe_w(xl - 1, yl + 1) && !schabe_w(xl + 1, yl + 1))
		{
			e->xSpeed = -SCHABE_SPEED; e->ySpeed = 0; schabe_snap(e, xl, yl);
		}
	}
	if (e->ySpeed > 0)   /* down */
	{
		if (!schabe_w(xl + 1, yl) && !schabe_w(xl - 1, yl - 1) && schabe_w(xl + 1, yl - 1) &&
		    !schabe_w(xl - 1, yl + 1) && !schabe_w(xl + 1, yl + 1))
		{
			e->xSpeed = SCHABE_SPEED; e->ySpeed = 0; schabe_snap(e, xl, yl);
		}
		if (!schabe_w(xl - 1, yl) && schabe_w(xl - 1, yl - 1) && !schabe_w(xl + 1, yl - 1) &&
		    !schabe_w(xl - 1, yl + 1) && !schabe_w(xl + 1, yl + 1))
		{
			e->xSpeed = -SCHABE_SPEED; e->ySpeed = 0; schabe_snap(e, xl, yl);
		}
	}

	/* against a wall? */
	if ((e->xSpeed < 0 && schabe_w(xl - 1, yl)) || (e->xSpeed > 0 && schabe_w(xl + 1, yl)))
	{
		e->xSpeed = 0;
		schabe_snap(e, xl, yl);
		e->ySpeed = schabe_w(xl, yl - 1) ? SCHABE_SPEED : -SCHABE_SPEED;
	}
	/* against the ceiling / the floor? */
	else if ((e->ySpeed > 0 && schabe_w(xl, yl + 1)) || (e->ySpeed < 0 && schabe_w(xl, yl - 1)))
	{
		e->ySpeed = 0;
		schabe_snap(e, xl, yl);
		e->xSpeed = schabe_w(xl - 1, yl) ? SCHABE_SPEED : -SCHABE_SPEED;
	}

	/* which way it is turned */
	if (e->ySpeed < 0)
	{
		e->i[0] = 90;
		e->i[1] = schabe_w(xl - 1, yl - 1) || schabe_w(xl - 1, yl) || schabe_w(xl - 1, yl + 1);
		if (TileBlock(xl, yl - 1) & B_WENDEPUNKT) e->ySpeed = SCHABE_SPEED;
	}
	else if (e->ySpeed > 0)
	{
		e->i[0] = 270;
		e->i[1] = !(schabe_w(xl - 1, yl - 1) || schabe_w(xl - 1, yl) || schabe_w(xl - 1, yl + 1));
		if (TileBlock(xl, yl + 1) & B_WENDEPUNKT) e->ySpeed = -SCHABE_SPEED;
	}
	if (e->xSpeed > 0)
	{
		if (schabe_w(xl - 1, yl + 1) || schabe_w(xl, yl + 1) || schabe_w(xl + 1, yl + 1)) { e->i[0] = 0; e->i[1] = 0; }
		else { e->i[0] = 180; e->i[1] = 1; }
		if (TileBlock(xl + 1, yl) & B_WENDEPUNKT) e->xSpeed = -SCHABE_SPEED;
	}
	else if (e->xSpeed < 0)
	{
		if (schabe_w(xl - 1, yl + 1) || schabe_w(xl, yl + 1) || schabe_w(xl + 1, yl + 1)) { e->i[0] = 0; e->i[1] = 1; }
		else { e->i[0] = 180; e->i[1] = 0; }
		if (TileBlock(xl - 1, yl) & B_WENDEPUNKT) e->xSpeed = SCHABE_SPEED;
	}

	TestDamagePlayers(e, SYNC(FX(2)), 0);
}

static void schabe_draw(Ent *e)
{
	if (e->i[0] == 180) draw_sprite_flipv(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, !e->i[1]);
	else draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, e->i[1]);
}

static void schabe_explode(Ent *e)
{
	for (int i = 0; i < 2; i++)
	{
		part_push(e->xPos - FX(20) + FX(GetRandom(10)), e->yPos - FX(20) + FX(GetRandom(10)), EXPLOSION_MEDIUM2);
		part_push(e->xPos + FX(5) + FX(GetRandom(10)), e->yPos + FX(5) + FX(GetRandom(10)), FUNKE);
		part_push(e->xPos + FX(5) + FX(GetRandom(10)), e->yPos + FX(5) + FX(GetRandom(10)), SPIDERSPLITTER);
	}
	snd(SOUND_EXPLOSION1);
	pl.Score += 10;
}

TYPEDEF(SCHABE) = { schabe_init, schabe_ki, schabe_explode, schabe_draw, 0 };
