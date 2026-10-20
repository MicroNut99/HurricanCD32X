/*
 * EVILHURRI - the evil Hurrican (bosses/Boss_EvilHurri.cpp): runs in, then rams the player
 * with a smoke trail, runs at him shooting, shoots in a circle, flashes lightning into the sky
 * (EVILBLITZ), runs over the screen shooting arcs / rockets up.
 * Own members: f[0] = BlinkWert, i[0] = BlinkDirection (-1 / 1), f[1] = ActionDelay.
 * Drawing: Hurrican draws the player's run sheet (Player[0].PlayerRun = p1_hurri_laufen.png,
 * 20 frames) tinted dark blue and additive, eight times; here once, plainly. Its AnimPhase
 * goes past the sheet's 20 frames (35..55 for some poses): those frames are past the end of
 * the sheet in Hurrican too; here the sheet's last frame is drawn for them.
 * Left out: the screen flash (RenderRect) of the lightning, screen shaking, the music fades
 * (stop / start instead).
 */
#define EH_FRAMES_RUN 20
static Pic P_eh_run;

static void eh_after_boss(void)          /* ScrolltoPlayeAfterBoss */
{
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
}

static void eh_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->blick = 1;
	e->Energy = FX(6000);
	e->AnimPhase = 3;
	e->AnimStart = 3;
	e->AnimEnde = 21;
	e->AnimSpeed = FX(0.4);
	e->Destroyable = 1;
	e->OwnDraw = 1;
	e->i[0] = 1;
	e->f[0] = 0;
	e->f[1] = 0;
}

static void eh_draw(Ent *e)
{
	draw_sprite(&P_eh_run, e->AnimPhase, e->xPos, e->yPos, e->blick == -1);
}

static void eh_shoot(Ent *e)              /* the running shots (LAUFEN, LAUFEN2) */
{
	e->f[1] -= SYNC(FX(1));
	if (e->f[1] < 0)
	{
		e->f[1] = FX(2.5);
		snd(SOUND_CANON);
		if (e->blick == 1)
		{
			part_push(e->xPos + FX(50), e->yPos + FX(20), SMOKE);
			proj_push(e->xPos + FX(55), e->yPos + FX(30), EVILSHOT);
		}
		else
		{
			part_push(e->xPos + FX(10), e->yPos + FX(20), SMOKE);
			proj_push(e->xPos, e->yPos + FX(30), EVILSHOT2);
		}
	}
}

static void eh_tospecial(Ent *e)          /* in the middle of the screen: the lightning */
{
	if ((e->blick == 1 && e->xPos + FX(35) > FX(e->Value1 + 320)) ||
	    (e->blick == -1 && e->xPos + FX(35) < FX(e->Value1 + 320)))
	{
		e->Handlung = GEGNER_SPECIAL;
		e->AnimPhase = 0;
		e->AnimEnde = 0;
		e->f[1] = 0;
		e->xSpeed = 0;
		snd(SOUND_BLITZSTART);
	}
}

static void eh_smoke(Ent *e)              /* the ramming's smoke trail */
{
	e->f[1] -= SYNC(FX(1));
	if (e->f[1] < 0)
	{
		e->f[1] = FX(1);
		part_push(e->xPos, e->yPos, e->blick == 1 ? EVILSMOKE2 : EVILSMOKE);
	}
}

static void eh_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	SimpleAnimation(e, 0);

	if (e->Handlung != GEGNER_INIT && e->Handlung != GEGNER_EXPLODIEREN)
		boss_bar(e->Energy, FX(6000));

	if (e->Active && tile_zustand() == TS_SCROLLBAR)
	{
		scroll_level(FX(e->Value1), FX(e->Value2), TS_SCROLLTOLOCK);
		music_stop();
	}

	if (e->DamageTaken > 0) e->DamageTaken -= SYNC(FX(100));
	else e->DamageTaken = 0;

	/* the player touches the boss */
	if (pl.Handlung != PA_TOT && pl.Handlung != PA_EXPLODIEREN &&
	    SpriteCollision(e->xPos, e->yPos, rect, pl.xpos, pl.ypos, &pl.CollideRect))
	{
		if (pl.Handlung == PA_RADELN || pl.Handlung == PA_RADELN_FALL)
		{
			if (pl.xpos < e->xPos) pl.Blickrichtung = -1;
			if (pl.xpos > e->xPos) pl.Blickrichtung = 1;
		}
		else
		{
			DamagePlayer(SYNC(FX(10)), 0);
			/* pushed away */
			if (pl.xpos < e->xPos) pl.xpos = e->xPos + FX(rect->l - pl.CollideRect.r);
			if (pl.xpos > e->xPos) pl.xpos = e->xPos + FX(rect->r - pl.CollideRect.l);
		}
	}

	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		shake_screen(5);
		e->AnimPhase = 40;
		e->AnimEnde = 0;
		e->AnimCount = 0;
		e->ySpeed = e->xSpeed = e->yAcc = e->xAcc = 0;
		snd(SOUND_EXPLOSION2);
		e->Handlung = GEGNER_EXPLODIEREN;
		e->f[1] = 0;
		music_stop();
	}

	/* blinking (the value is only for Hurrican's colour) */
	e->f[0] += SYNC(FX(30) * e->i[0]);
	if (e->f[0] <= 0) e->i[0] = 1;
	if (e->f[0] >= FX(128)) e->i[0] = -1;

	/* the screen's edges */
	if (e->Handlung != GEGNER_EINFLIEGEN && e->Handlung != GEGNER_INIT)
	{
		if (e->xPos < FX(e->Value1 + 10)) e->xPos = FX(e->Value1 + 10);
		if (e->xPos > FX(e->Value1 + 550)) e->xPos = FX(e->Value1 + 550);
	}

	switch (e->Handlung)
	{
	case GEGNER_INIT:
		if (tile_zustand() == TS_LOCKED)
		{
			music_boss();
			e->Handlung = GEGNER_EINFLIEGEN;
		}
		break;

	case GEGNER_STEHEN:                  /* count down and choose the next action */
		e->AnimPhase = 0;
		e->AnimStart = 0;
		e->AnimEnde = 0;
		e->xSpeed = 0;
		e->ySpeed = 0;
		if (e->xPos + FX(70) < pl.xpos) e->blick = 1;
		if (e->xPos > pl.xpos + FX(70)) e->blick = -1;
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0)
		{
			e->f[1] = 0;
			if (PlayerAbstand(e) > 300)      /* far away: ram him */
			{
				e->AnimPhase = 48;
				e->f[1] = FX(1);
				e->xSpeed = e->blick == 1 ? FX(40) : FX(-40);
				e->Handlung = GetRandom(2) == 0 ? GEGNER_CRUSHEN : GEGNER_CRUSHEN2;
			}
			else
			{
				if (GetRandom(2) == 0 && e->xPos > FX(e->Value1 + 200) && e->xPos < FX(e->Value1 + 440))
				{
					/* shoot in a circle */
					e->Handlung = GEGNER_BOMBARDIEREN;
					e->f[1] = FX(3);
					e->AnimEnde = 0;
					e->AnimStart = 0;
					e->AnimPhase = 35;
					e->blick = 1;
				}
				else
				{
					/* run at the player shooting */
					e->f[1] = FX(1);
					e->AnimPhase = 3;
					e->AnimStart = 3;
					e->AnimSpeed = FX(0.4);
					e->xSpeed = e->blick == 1 ? FX(20) : FX(-20);
					if (GetRandom(2) == 0) { e->Handlung = GEGNER_LAUFEN; e->AnimEnde = EH_FRAMES_RUN; }
					else { e->Handlung = GEGNER_LAUFEN2; e->AnimEnde = 21; }
				}
			}
		}
		break;

	case GEGNER_EINFLIEGEN:              /* runs into the screen */
		e->Energy = FX(6000);
		e->DamageTaken = 0;
		e->xSpeed = FX(15);
		if (e->xPos >= FX(e->Value1 + 30))
		{
			e->Handlung = GEGNER_STEHEN;
			e->f[1] = FX(10);
		}
		break;

	case GEGNER_CRUSHEN:
		eh_smoke(e);
		if ((e->blick == 1 && e->xPos > pl.xpos + FX(140)) || (e->blick == -1 && e->xPos + FX(140) < pl.xpos))
		{
			e->Handlung = GEGNER_STEHEN;
			e->f[1] = FX(10);
		}
		break;

	case GEGNER_CRUSHEN2:                /* only to the middle, then the lightning */
		eh_smoke(e);
		eh_tospecial(e);
		break;

	case GEGNER_LAUFEN:
		eh_shoot(e);
		/* jump? */
		if (pl.ypos + FX(10) < e->yPos && e->ySpeed == 0 && GetRandom(10) == 0)
		{
			e->ySpeed = FX(-43);
			e->yAcc = FX(8);
			e->yPos -= FX(2);
			e->AnimPhase = 48;
			e->AnimEnde = 55;
			e->AnimSpeed = FX(2);
		}
		if (e->ySpeed != 0 && (e->blocku & B_WAND))
		{
			/* landed */
			e->ySpeed = 0;
			e->yAcc = 0;
			e->AnimPhase = 3;
			e->AnimStart = 3;
			e->AnimEnde = 21;
			e->AnimSpeed = FX(0.4);
			BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);
		}
		if (((e->blick == 1 && e->xPos > pl.xpos + FX(70)) || (e->blick == -1 && e->xPos + FX(70) < pl.xpos)) &&
		    e->ySpeed == 0)
		{
			e->Handlung = GEGNER_STEHEN;
			e->f[1] = FX(10);
		}
		break;

	case GEGNER_LAUFEN2:                 /* to the middle of the screen */
		eh_shoot(e);
		eh_tospecial(e);
		break;

	case GEGNER_BOMBARDIEREN:            /* shoots all around */
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0)
		{
			snd(SOUND_CANON);
			part_push(e->xPos + FX((e->AnimPhase - 36) * 5), e->yPos - FX(23) + FX(abs(e->AnimPhase - 41) * 8), SMOKE);
			proj_push(e->xPos + FX(5) + FX((e->AnimPhase - 36) * 5), e->yPos - FX(10) + FX(abs(e->AnimPhase - 41) * 8),
			          EVILROUND1 + (e->AnimPhase - 35));
			e->f[1] = FX(3);
			e->AnimPhase++;
			if (e->AnimPhase > 46)
			{
				e->Handlung = GEGNER_STEHEN;
				e->AnimPhase = 0;
				e->f[1] = FX(10);
			}
		}
		break;

	case GEGNER_SPECIAL:                 /* lightning into the sky, shots fall from above */
		e->f[1] += SYNC(FX(1));
		if (e->f[1] > FX(5) && e->f[1] < FX(20) && e->AnimPhase == 0) e->AnimPhase = 40;
		if (e->f[1] > FX(25) && e->AnimPhase != 0)
		{
			e->AnimPhase = 0;
			for (int i = 0; i < 30; i++)
				part_push(e->xPos + FX(30 + GetRandom(10)), e->yPos + FX(GetRandom(10)), LASERFUNKE2);
			proj_push(e->xPos, e->yPos - FX(20), EVILBLITZ);
			snd(SOUND_SPIDERGRENADE);
		}
		/* (the screen flash between 25 and 35 is left out) */
		if (e->f[1] > FX(70))
		{
			e->AnimPhase = 3;
			e->AnimStart = 3;
			e->AnimEnde = 21;
			if (GetRandom(2) == 0)
			{
				e->Handlung = GEGNER_AUFRICHTEN;
				e->blick = -1;
				e->xSpeed = FX(-20);
			}
			else
			{
				e->Handlung = GEGNER_AUFRICHTENZWEI;
				e->blick = 1;
				e->xSpeed = FX(20);
			}
		}
		break;

	case GEGNER_AUFRICHTEN:
		if (e->xPos < FX(e->Value1 + 20))
		{
			e->blick = 1;
			e->xSpeed = FX(15);
			e->f[1] = FX(4);
			e->Handlung = GEGNER_AUSSPUCKEN;
		}
		break;

	case GEGNER_AUFRICHTENZWEI:
		if (e->xPos >= FX(e->Value1 + 550))
		{
			e->blick = -1;
			e->xSpeed = FX(-15);
			e->f[1] = FX(4);
			e->Handlung = GEGNER_AUSSPUCKENZWEI;
		}
		break;

	case GEGNER_AUSSPUCKEN:              /* over the screen, shooting arcs up */
		if (e->xPos >= FX(e->Value1 + 550))
		{
			e->xSpeed = 0;
			e->AnimPhase = 0;
			e->f[1] = FX(10);
			e->Handlung = GEGNER_STEHEN;
		}
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0)
		{
			if (e->AnimPhase != 40)
			{
				e->xSpeed = 0;
				e->AnimEnde = 0;
				e->AnimPhase = 40;
				proj_push(e->xPos + FX(20), e->yPos + FX(10), ARCSHOT);
				proj_push(e->xPos + FX(20), e->yPos + FX(10), ARCSHOTLEFT);
				proj_push(e->xPos + FX(20), e->yPos + FX(10), ARCSHOTRIGHT);
				snd(SOUND_SPIDERGRENADE);
				e->f[1] = FX(3);
			}
			else
			{
				e->xSpeed = FX(15);
				e->AnimPhase = 3;
				e->AnimStart = 3;
				e->AnimEnde = 21;
				e->f[1] = FX(7);
			}
		}
		break;

	case GEGNER_AUSSPUCKENZWEI:          /* back over the screen, rockets */
		if (e->xPos <= FX(e->Value1 + 20))
		{
			e->xSpeed = 0;
			e->AnimPhase = 0;
			e->f[1] = FX(10);
			e->Handlung = GEGNER_STEHEN;
		}
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0)
		{
			if (e->AnimPhase != 40)
			{
				e->xSpeed = 0;
				e->AnimEnde = 0;
				e->AnimPhase = 40;
				proj_push(e->xPos, e->yPos + FX(10), ROCKETSPIDER);
				snd(SOUND_ROCKET);
				e->f[1] = FX(4);
			}
			else
			{
				e->xSpeed = FX(-15);
				e->AnimPhase = 3;
				e->AnimStart = 3;
				e->AnimEnde = 21;
				e->f[1] = FX(10);
			}
		}
		break;

	case GEGNER_EXPLODIEREN:
		e->Energy = FX(80);
		e->f[1] += SYNC(FX(1));
		if (e->f[1] >= FX(50)) e->Energy = 0;
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount < 0)
		{
			e->AnimCount = FX(2);
			snd(SOUND_EXPLOSION1);
			part_push(e->xPos - FX(20) + FX(GetRandom(70)), e->yPos + FX(GetRandom(80)), EXPLOSION_MEDIUM);
			part_push(e->xPos - FX(20) + FX(GetRandom(70)), e->yPos + FX(GetRandom(80)), EXPLOSION_MEDIUM2);
			part_push(e->xPos - FX(50) + FX(GetRandom(70)), e->yPos - FX(30) + FX(GetRandom(80)), EXPLOSION_BIG);
		}
		break;

	default:
		break;
	}
}

static void eh_explode(Ent *e)
{
	pl.Score += 9000;
	for (int i = 0; i < 10; i++)
		part_push(e->xPos - FX(20) + FX(GetRandom(70)), e->yPos + FX(GetRandom(80)), SPLITTER);
	for (int i = 0; i < 10; i++)
		part_push(e->xPos - FX(20) + FX(GetRandom(70)), e->yPos + FX(GetRandom(80)), EXPLOSION_MEDIUM2);
	snd(SOUND_EXPLOSION2);
	eh_after_boss();
	gegner_default_explode(e);
}

static void eh_load(void)
{
	P_eh_run = pic("p1_hurri_laufen.png");
}

TYPEDEF(EVILHURRI) = { eh_init, eh_ki, eh_explode, eh_draw, eh_load };
