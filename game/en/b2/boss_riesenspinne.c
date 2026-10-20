/*
 * RIESENSPINNE - the giant spider, end boss of the spider factory (bosses/Boss_RiesenSpinne.cpp):
 * walks to and fro on eight legs, fires rockets, purple shots and a laser, lowers its body,
 * charges to the left and drops spider bombs; barrels (other group) hurt it most.
 * Own members: f[0] = HeadWinkel (degrees), f[1] = HeadXOffset (radians), f[2] = WalkDir,
 *   f[3] = WalkCount (radians), f[4] = DamageWackel (radians), f[5] = LastEnergy,
 *   f[6] = ShotDelay, f[7] = yBody; i[0] = ShotCount, i[1] = ShotMode.
 *   LegsAnim[i] is WalkCount + i * PI / 3 (Hurrican sets it so whenever WalkCount changes), so it
 *   is worked out when drawn instead of kept.
 * Drawing: Hurrican draws each leg's two parts as quads skewed between the body and the knee /
 * foot, and the head rotated; here every part is its plain frame, the leg parts upright in the
 * middle of their quads, the head at its place. Left out: the purple glows in front of the head
 * (additive LavaFlare), screen shaking.
 * Hurrican fades the music: here it stops / starts.
 */
static Pic riesenspinne_head, riesenspinne_legs[3];
static int riesenspinne_last = -1;          /* RandomHandlung's static */
static fx riesenspinne_headdir = FX(1);     /* the SPECIAL case's static */

#define RSP_PI     FX(3.14159265)
#define RSP_TWOPI  FX(6.2831853)

static int riesenspinne_sin(fx rad)          /* sin of radians, fx */
{
	return sin_d(FINT(FMUL(rad, FX(57.29578))));
}

static void riesenspinne_init(Ent *e)
{
	e->i[0] = 0;
	e->i[1] = 0;
	e->f[0] = 0;
	e->f[1] = 0;
	e->f[2] = FX(-5);
	e->f[3] = 0;
	e->f[4] = 0;
	e->f[7] = 0;
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = -1;
	e->Energy = FX(100000);
	e->f[5] = FX(100000);
	e->Destroyable = 1;
	e->OwnDraw = 1;
	e->TestBlock = 0;
}

/* one leg: (x, y) its place, winkel its phase (radians), anim the leg (0..3), off 0 (in
 * front) or 10 (behind) */
static void riesenspinne_drawleg(Ent *e, fx x, fx y, fx winkel, int anim, int off)
{
	static const int16_t top1[4] = { 40, 24, -10, -20 }, bot1[4] = { 120, 40, -40, -120 };
	static const int16_t top2[4] = { 40, 20, -20, -40 };
	const fx yBody = e->f[7];
	fx xp, yp;
	x += FX(120);
	if (winkel < RSP_PI)
	{
		xp = x + 18 * riesenspinne_sin(winkel + RSP_PI / 2);
		yp = y + 30 * riesenspinne_sin(winkel + RSP_PI);   /* cos(w + PI/2) */
	}
	else
	{
		xp = x + (winkel - RSP_PI) * 10 - FX(16);
		yp = y;
	}
	const fx tempdamage = 10 * riesenspinne_sin(FMUL(e->f[4], RSP_TWOPI));
	const fx yoff = riesenspinne_sin(e->f[3] * 2);
	const fx yoff2 = 5 * riesenspinne_sin(winkel - RSP_PI - FX(0.5));
	const fx temp = 10 * riesenspinne_sin(winkel);
	const int fr = anim * 3 + off;
	const Pic *up = &riesenspinne_legs[2], *lo = &riesenspinne_legs[1];

	/* the upper part: from (temp + x + top, o) to (x + bottom, u) */
	{
		const fx o = y + FX(55) - FX(up->fh * 2) + yoff2 + yBody + yoff;
		const fx u = y + FX(30) + yoff + tempdamage + yBody;
		const fx mx = (temp + x + FX(top1[anim]) + x + FX(bot1[anim])) / 2;
		draw_sprite(up, fr, mx, (o + u) / 2 - FX(up->fh), 0);
	}
	/* the lower part: from (temp + x + top, o) to (xp (+ 20), yp + 20) */
	{
		const fx o = y + FX(20) - FX(lo->fh * 2) + yoff2 + yBody;
		const fx u = yp + FX(20);
		const fx bx = xp + (anim < 2 ? FX(20) : 0);
		const fx mx = (temp + x + FX(top2[anim]) + bx) / 2;
		draw_sprite(lo, fr, mx, (o + u) / 2 - FX(lo->fh), 0);
	}
	/* the foot */
	draw_sprite(&riesenspinne_legs[0], fr, xp, yp, 0);
}

static void riesenspinne_draw(Ent *e)
{
	static const int8_t order[4] = { 0, 1, 3, 2 };
	int i;
	/* the legs behind the body */
	for (i = 0; i < 4; i++)
	{
		const int a = order[i];
		fx w = e->f[3] + RSP_PI / 3 * a;
		while (w > RSP_TWOPI) w -= RSP_TWOPI;
		w += RSP_PI;
		if (w > RSP_TWOPI) w -= RSP_TWOPI;
		riesenspinne_drawleg(e, e->xPos - FX(70) + FX(a * 110), e->yPos + FX(130), w, a, 10);
	}
	const fx tempdamage = 10 * riesenspinne_sin(FMUL(e->f[4], RSP_TWOPI));
	const fx bob = 2 * riesenspinne_sin(e->f[3]);
	/* the body */
	draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos + bob + tempdamage + e->f[7], 0);
	/* the head (Hurrican turns it by HeadWinkel) */
	draw_sprite(&riesenspinne_head, 0, e->xPos - FX(90) + FX(120) + 10 * riesenspinne_sin(e->f[1]),
	            e->yPos + FX(60) + bob + tempdamage + e->f[7], 0);
	/* the legs in front */
	for (i = 0; i < 4; i++)
	{
		const int a = order[i];
		fx w = e->f[3] + RSP_PI / 3 * a;
		while (w > RSP_TWOPI) w -= RSP_TWOPI;
		riesenspinne_drawleg(e, e->xPos - FX(70) + FX(a * 110), e->yPos + FX(130), w, a, 0);
	}
}

/* a new action, when it stands on the left */
static void riesenspinne_random(Ent *e)
{
	int j;
	e->f[6] = FX(4);
	e->xSpeed = 0;
	do j = GetRandom(6); while (j == riesenspinne_last);
	riesenspinne_last = j;
	switch (j)
	{
	case 0: e->i[0] = 10 + GetRandom(10); e->Handlung = GEGNER_SCHIESSEN; break;
	case 1: e->Handlung = GEGNER_ABSENKEN; break;
	case 2: e->i[0] = 20 + GetRandom(10); e->Handlung = GEGNER_SPECIAL; break;
	case 3: e->Handlung = GEGNER_ABSENKENZWEI; break;
	case 4: e->Handlung = GEGNER_LAUFEN_LINKS; e->f[6] = FX(1); e->i[1] = GetRandom(2); break;
	case 5:
		e->Handlung = GEGNER_SPECIAL2;
		proj_push(FX(e->Value1), e->yPos + e->f[7] + FX(110), SPIDERLASER);
		snd(SOUND_BEAMLOAD2);
		e->AnimCount = FX(140);
		e->f[6] = 0;
		break;
	}
}

static void riesenspinne_to_left(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN_LINKS;
	e->i[1] = GetRandom(2);
	e->f[6] = FX(5);
}

static void riesenspinne_ki(Ent *e)
{
	fx *HeadWinkel = &e->f[0], *HeadXOffset = &e->f[1], *WalkDir = &e->f[2], *WalkCount = &e->f[3];
	fx *DamageWackel = &e->f[4], *LastEnergy = &e->f[5], *ShotDelay = &e->f[6], *yBody = &e->f[7];
	int i;

	if (e->Handlung != GEGNER_NOTVISIBLE && e->Handlung != GEGNER_EXPLODIEREN)
	{
		b2test_hurt(e);
		boss_bar(e->Energy, FX(100000));
	}

	/* lock the screen on the spider as soon as it is seen */
	if (e->Active && tile_zustand() == TS_SCROLLBAR)
	{
		scroll_level(FX(e->Value1), FX(e->Value2), TS_SCROLLTOLOCK);
		music_stop();
	}

	switch (e->Handlung)
	{
	case GEGNER_NOTVISIBLE:        /* wait until the screen is in place */
		if (tile_zustand() == TS_LOCKED)
		{
			music_boss();
			e->Handlung = GEGNER_EINFLIEGEN;
		}
		break;

	case GEGNER_EINFLIEGEN:        /* walks into the level */
		e->yPos -= FX(55);
		e->Energy = FX(100000);
		e->DamageTaken = 0;
		snd(SOUND_SPIDERSCREAM);
		e->Handlung = GEGNER_LAUFEN_LINKS;
		*ShotDelay = FX(1);
		e->i[1] = GetRandom(2);
		break;

	case GEGNER_CRUSHEN:           /* walking left or right */
	case GEGNER_LAUFEN_LINKS:
	case GEGNER_LAUFEN_RECHTS:
	case GEGNER_EXPLODIEREN:
		/* the head straight */
		if (*HeadWinkel > 0) *HeadWinkel -= SYNC(FX(1));
		if (*HeadWinkel < 0) *HeadWinkel += SYNC(FX(1));
		e->xSpeed = *WalkDir;
		*WalkCount -= SYNC(*WalkDir / 10);
		if (*WalkCount > RSP_TWOPI) *WalkCount -= RSP_TWOPI;
		if (*WalkCount < 0) *WalkCount += RSP_TWOPI;

		if (e->Handlung == GEGNER_CRUSHEN)
		{
			if (*HeadWinkel < FX(45)) *HeadWinkel += SYNC(FX(1));
			*WalkDir -= SYNC(FX(10));
			if (*WalkDir < FX(-35)) *WalkDir = FX(-35);
			if (e->xPos < FX(e->Value1 - 50))
			{
				e->Handlung = GEGNER_LAUFEN_RECHTS;
				*ShotDelay = FX(15);
			}
			/* push the player away */
			const Box coll = { 50, (int16_t)(40 + FINT(*yBody)), 120, (int16_t)(140 + FINT(*yBody)) };
			Wegschieben(e, &coll, FX(50));
		}

		if (e->Handlung == GEGNER_LAUFEN_LINKS)
		{
			*WalkDir -= SYNC(FX(1));
			if (*WalkDir < FX(-8)) *WalkDir = FX(-8);
			if (e->xPos < FX(e->Value1 + 100))
			{
				e->Handlung = GEGNER_LAUFEN_RECHTS;
				*ShotDelay = FX(15);
			}
			/* maybe shoot */
			if (e->Energy < FX(75000))
			{
				*ShotDelay -= SYNC(FX(1));
				if (*DamageWackel > 0) *ShotDelay = FX(15);
				if (*ShotDelay < 0)
				{
					*ShotDelay = FX(12);
					snd(SOUND_LILA);
					part_push(e->xPos + FX(30) - FX(50), e->yPos + *yBody + FX(85) - FX(35), EXPLOSIONFLARE2);
					if (e->i[1] == 0) proj_push(e->xPos + FX(30), e->yPos + *yBody + FX(85), SPIDERSLOW);
					else
						for (i = 0; i < 5; i++)
						{
							proj_winkel = FX(212 - i * 15);
							proj_push(e->xPos, e->yPos + *yBody + FX(85), SPIDERSHOTLANGSAM);
						}
				}
			}
		}

		if (e->Handlung == GEGNER_LAUFEN_RECHTS || e->Handlung == GEGNER_EXPLODIEREN)
		{
			*WalkDir += SYNC(FX(1));
			/* the body up, if it is down */
			if (*yBody > 0)
			{
				*yBody -= SYNC(FX(1));
				if (*yBody < 0) *yBody -= 0;
			}
			if (e->Handlung == GEGNER_LAUFEN_RECHTS)
			{
				/* spider bombs out at the back */
				*ShotDelay -= SYNC(FX(1));
				if (*ShotDelay < 0)
				{
					*ShotDelay = FX(15);
					gegner_push(e->xPos + FX(400), e->yPos + FX(100) + *yBody, SPIDERBOMB, 0, 0);
				}
			}
			/* faster for a while after charging */
			if (e->xPos < FX(e->Value1 - 550)) *WalkDir = FX(30);
			else
			{
				if (e->Handlung == GEGNER_LAUFEN_RECHTS && *WalkDir > FX(8)) *WalkDir = FX(8);
				if (e->Handlung == GEGNER_EXPLODIEREN && *WalkDir > FX(4)) *WalkDir = FX(4);
			}
			if (e->Handlung == GEGNER_LAUFEN_RECHTS && e->xPos > FX(e->Value1 + 400)) riesenspinne_random(e);
		}

		if (e->Handlung == GEGNER_EXPLODIEREN)
		{
			e->Energy = FX(100);
			e->AnimCount -= SYNC(FX(1));
			if (e->AnimCount < 0)
			{
				e->AnimCount = FX(0.4);
				part_push(e->xPos + FX(GetRandom(400)), e->yPos + FX(GetRandom(100)), EXPLOSION_MEDIUM + GetRandom(2));
				part_push(e->xPos + FX(GetRandom(400)), e->yPos + FX(GetRandom(100)), SMOKEBIG);
				if (GetRandom(5) == 0) snd(SOUND_EXPLOSION3 + GetRandom(2));
			}
			if (e->xPos > FX(e->Value1 + RENDERWIDTH)) e->Energy = 0;
		}
		break;

	case GEGNER_SCHIESSEN:         /* rockets */
		*ShotDelay -= SYNC(FX(1));
		if (*ShotDelay < 0)
		{
			*ShotDelay = FX(4);
			snd(SOUND_ROCKET);
			gegner_push(e->xPos + FX(35), e->yPos + FX(95), FETTERAKETE, GetRandom(60) - 130, 0);
			*HeadXOffset = RSP_TWOPI;              /* the head jerks back */
			e->i[0]--;
			if (e->i[0] == 0 || pl.xpos > e->xPos - FX(50)) riesenspinne_to_left(e);
		}
		break;

	case GEGNER_SPECIAL:           /* move the head and shoot */
		*HeadWinkel += SYNC(riesenspinne_headdir);
		if (*HeadWinkel > FX(15)) riesenspinne_headdir = FX(-1);
		if (*HeadWinkel < FX(-10)) riesenspinne_headdir = FX(1);
		*ShotDelay -= SYNC(FX(1));
		if (*ShotDelay < 0)
		{
			*ShotDelay = FX(3.5);
			snd(SOUND_LILA);
			proj_winkel = FX(180) - *HeadWinkel * 3 / 2;
			proj_push(e->xPos - 10 * cos_d(FINT(*HeadWinkel)), e->yPos + 140 * sin_d(FINT(*HeadWinkel)) + FX(90) + *yBody,
			          SPIDERSHOT);
			*HeadXOffset = RSP_TWOPI;
			e->i[0]--;
			if (e->i[0] == 0) riesenspinne_to_left(e);
		}
		break;

	case GEGNER_SPECIAL2:          /* the laser */
		if (*ShotDelay < FX(255)) *ShotDelay += SYNC(FX(5.8));
		else *ShotDelay = FX(255);
		e->AnimCount -= SYNC(FX(2.5));
		if (e->AnimCount < 0) riesenspinne_to_left(e);
		break;

	case GEGNER_ABSENKEN:          /* lower the body and shoot */
		*yBody += SYNC(FX(1));
		if (*yBody > FX(50)) e->Handlung = GEGNER_AUFRICHTEN;
		*ShotDelay -= SYNC(FX(1));
		if (*ShotDelay < 0)
		{
			*ShotDelay = FX(10);
			snd(SOUND_LILA);
			proj_push(e->xPos + FX(20), e->yPos + FX(80) + *yBody, SPIDERSHOT2);
			proj_push(e->xPos + FX(20), e->yPos + FX(100) + *yBody, SPIDERSHOT2);
			*HeadXOffset = RSP_PI * 3;
		}
		break;

	case GEGNER_ABSENKENZWEI:      /* down fast, then charge to the left */
		*yBody += SYNC(FX(8));
		if (*yBody > FX(50))
		{
			e->Handlung = GEGNER_CRUSHEN;
			snd(SOUND_SPIDERSCREAM);
		}
		break;

	case GEGNER_AUFRICHTEN:        /* up again and shoot */
		*yBody -= SYNC(FX(1));
		if (*yBody <= 0)
		{
			*yBody = 0;
			riesenspinne_to_left(e);
		}
		*ShotDelay -= SYNC(FX(1));
		if (*ShotDelay < 0)
		{
			*ShotDelay = FX(10);
			snd(SOUND_LILA);
			proj_winkel = FX(PlayerAbstandHoriz(e)) / 10;
			proj_push(e->xPos + FX(20), e->yPos + FX(80) + *yBody, SPIDERSHOT2);
			proj_push(e->xPos + FX(20), e->yPos + FX(100) + *yBody, SPIDERSHOT2);
			*HeadXOffset = RSP_PI * 3;
		}
		break;
	}

	/* the head back after jerking */
	if (*HeadXOffset > RSP_PI) *HeadXOffset -= SYNC(FX(0.8));
	else *HeadXOffset = RSP_PI;

	/* hurt? */
	if (e->DamageTaken > 0)
	{
		/* a barrel? then the head goes up */
		if (*LastEnergy - e->Energy > FX(5000))
		{
			if (e->Handlung != GEGNER_SPECIAL2) *DamageWackel = RSP_PI;
			snd(SOUND_SPIDERSCREAM);
			if (e->Handlung == GEGNER_SCHIESSEN || e->Handlung == GEGNER_SPECIAL || e->Handlung == GEGNER_ABSENKEN ||
			    e->Handlung == GEGNER_AUFRICHTEN)
				e->Handlung = GEGNER_LAUFEN_LINKS;
		}
		else e->DamageTaken = 0;
		*LastEnergy = e->Energy;
	}

	if (*DamageWackel > 0) *DamageWackel -= SYNC(FX(0.2));
	else *DamageWackel = 0;

	GegnerRect[RIESENSPINNE].l = 60;
	GegnerRect[RIESENSPINNE].r = 360;
	GegnerRect[RIESENSPINNE].t = (int16_t)(40 + FINT(*yBody));
	GegnerRect[RIESENSPINNE].b = (int16_t)(140 + FINT(*yBody));

	TestDamagePlayers(e, SYNC(FX(4)), 0);

	/* no energy left? then it explodes */
	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Energy = FX(100);
		e->Handlung = GEGNER_EXPLODIEREN;
		e->AnimCount = FX(1);
		proj_clear_type(SPIDERLASER);
		music_stop();
	}
}

static void riesenspinne_explode(Ent *e)
{
	pl.Score += 15000;
	snd(SOUND_EXPLOSION2);
	shake_screen(5);
	/* ScrolltoPlayeAfterBoss */
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
	gegner_default_explode(e);
}

static void riesenspinne_load(void)
{
	riesenspinne_head = pic("spiderboss_kopf.png");
	riesenspinne_legs[0] = pic("spiderboss_foot.png");
	riesenspinne_legs[1] = pic("spiderboss_leg2.png");
	riesenspinne_legs[2] = pic("spiderboss_leg1.png");
}

TYPEDEF(RIESENSPINNE) = { riesenspinne_init, riesenspinne_ki, riesenspinne_explode, riesenspinne_draw, riesenspinne_load };
