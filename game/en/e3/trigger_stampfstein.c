/*
 * STAMPFSTEIN - the stamping stone on its chain (triggers/Trigger_Stampfstein.cpp): falls when the
 * player comes under it (crushing him and the enemies under it), waits, and is pulled back up
 * on its chain; the player can ride it up (and is crushed against the ceiling). Its spikes hurt
 * from below; it cannot be walked or jumped through.
 * Own members: f[0] = oldy (its rest height).
 * Drawn as Hurrican: the picture is the chain with the stone at its bottom; only the part below
 * the rest height (the stone, and the chain that came out while it fell) shows
 * (SetRect(0, size, 80, 256)). The engine has no clipped drawing, so stampfstein_draw_crop
 * clips the picture's top by moving the engine's view top (view_y in game/p1.c) for the call.
 * Hurrican's chain sound channel (stopped when it is up) is left out: sounds cannot be stopped.
 */
static void stampfstein_init(Ent *e)
{
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = -1;
	e->Energy = FX(5000);
	e->Destroyable = 1;
	e->OwnDraw = 1;
	e->f[0] = 0;
}

/* frame `frame` of p at world (x, y), its rows above world y `top` not drawn */
static void stampfstein_draw_crop(const Pic *p, int frame, fx x, fx y, fx top)
{
	if (!p->count) return;
	Frame f;
	frame_of(p, frame, &f);
	const int ty = (int)(top >> 9), sy = (int)(y >> 9);
	const int vy = view_y, vbottom = view_y + VIEW_H;
	if (ty > vy)
	{
		/* the view's top at the cut; the frame cut at the old bottom so the view's bottom
		 * (view_y + VIEW_H) moving down does not matter */
		const int by = sy + f.y0;
		if (by + f.h > vbottom) f.h = vbottom - by;
		if (f.h <= 0 || ty >= vbottom) return;
		view_y = ty;
	}
	draw_frame(&f, p->fw, p->fh, (int)(x >> 9), sy, 0, 1);
	view_y = vy;
}

static void stampfstein_draw(Ent *e)
{
	const int size = FINT(e->f[0] - e->yPos) + 165;
	stampfstein_draw_crop(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos, e->yPos + FX(size));
}

static void stampfstein_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	e3test_step();               /* (PC check only) */
	e->Energy = FX(5000);

	/* the player cannot walk through it */
	Wegschieben(e, rect, 0);

	/* nor jump through it from below */
	if (SpriteCollision(pl.xpos, pl.ypos, &pl.CollideRect, e->xPos, e->yPos, rect) &&
	    e->yPos < pl.ypos && pl.yspeed < 0)
		pl.yspeed = FMUL(pl.yspeed, FX(-0.25));

	switch (e->Handlung)
	{
	case GEGNER_NOTVISIBLE:
		e->f[0] = e->yPos;
		e->Handlung = GEGNER_STEHEN;
		break;
	case GEGNER_STEHEN:          /* waiting for the player */
		e->f[0] = e->yPos;
		if (PlayerAbstandHoriz(e) < 50 && PlayerAbstandVert(e) < 600 && pl.ypos > e->yPos)
		{
			e->Handlung = GEGNER_FALLEN;
			e->ySpeed = FX(20);
			e->yAcc = FX(15);
			snd(SOUND_STONEFALL);
		}
		break;
	case GEGNER_FALLEN:          /* falling */
		if (e->ySpeed > FX(40)) e->ySpeed = FX(40);
		/* touched while falling: the player dies */
		if (pl.AufPlattform != e &&
		    SpriteCollision(pl.xpos, pl.ypos, &pl.CollideRect, e->xPos, e->yPos, rect) &&
		    pl.ypos + FX(pl.CollideRect.b) >= e->yPos)
			TestDamagePlayers(e, FX(500), 0);
		/* on the ground: stamp, wait and go back up */
		if ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
		{
			for (int i = 0; i < 20; i++)
				part_push(e->xPos + FX(i * 5 - 30), e->yPos + FX(220), SMOKEBIG);
			for (int i = 0; i < 20; i++)
				part_push(e->xPos + FX(i * 5 - 10), e->yPos + FX(236), ROCKSPLITTER + GetRandom(2));
			snd(SOUND_PHARAORAMM);
			e->ySpeed = 0;
			e->yAcc = 0;
			e->AnimCount = FX(20);
			e->Handlung = GEGNER_INIT;
			/* the enemies under it */
			for (int k = 0; k < MAX_LIVE; k++)
			{
				Ent *o = &ents[k];
				if (o->GegnerArt < 0 || o == e || !o->Destroyable) continue;
				if (SpriteCollision(e->xPos, e->yPos, rect, o->xPos, o->yPos, &GegnerRect[o->GegnerArt])) o->Energy = 0;
			}
		}
		break;
	case GEGNER_INIT:            /* waits a moment, then is pulled up */
		PlattformTest(e, rect);
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount < 0)
		{
			e->ySpeed = FX(-4);
			e->Handlung = GEGNER_SPRINGEN;
			snd(SOUND_CHAIN);
		}
		break;
	case GEGNER_SPRINGEN:        /* being pulled up */
		PlattformTest(e, rect);
		if (e->blocko & B_WAND)
		{
			e->Handlung = GEGNER_STEHEN;
			e->ySpeed = 0;
		}
		/* the player caught between the ceiling and the stone? */
		if (pl.AufPlattform == e &&
		    (BlockOben(pl.xpos, &pl.ypos, pl.xposold, &pl.yposold, &pl.CollideRect, 0) & B_WAND))
		{
			DamagePlayer(FX(500), 1);
			pl.AufPlattform = 0;
			pl.ypos += FX(10);
			pl.yspeed = FX(10);
		}
		break;
	}

	/* the spikes from below */
	if (SpriteCollision(pl.xpos, pl.ypos, &pl.CollideRect, e->xPos, e->yPos, rect) && pl.ypos > e->yPos)
		DamagePlayer(SYNC(FX(20)), 0);
}

static void stampfstein_explode(Ent *e)
{
	(void)e;
}

TYPEDEF(STAMPFSTEIN) = { stampfstein_init, stampfstein_ki, stampfstein_explode, stampfstein_draw, 0 };
