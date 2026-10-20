/*
 * THEWALL - the end boss's wall, the last stage of the final boss (bosses/Boss_TheWall.cpp):
 * the machine on the right shoots (golem shots, purple balls, electric floor shots, the big
 * laser if the player stands on the crates), its skull (the SKULL object, which docks on it
 * and sets Value1 = 1) takes the hits; when the skull's energy is gone the skull flies off
 * and the doors of the tunnel on the left open on the beating heart, which can then be hit.
 * Out of energy: the glass breaks, the skull is sent to explode (SKULL's SPECIAL3), the screen
 * goes back to the player - the wall itself never dies (Hurrican's GegnerExplode is empty).
 * Parts (DoDraw): the machine (endbossmaschine.png), its front part (moves when it fires), the
 * skull on it, the turning rings, the heart, the two sliding doors, the frame around them.
 * Own members: the static struct below (one wall).
 * Left out: the heart tank's red soup (an additive rectangle), the doors' shadow (alpha), the
 * tube holder over the heart and the skull on the machine as Hurrican draws them (cut from
 * endbosshirni.png outside its frames - not in the pack; the skull is drawn with skull.png's
 * front frame instead), the ring's rotation (plain frame), the colour tints, the slow motion
 * (Timer.SetMoveSpeed), stopping sounds, the music pause (music_stop), screen shaking.
 * Engine notes: the doors are drawn cut at the tunnel's edges (Tuer.SetRect) with
 * thewall_draw_clip, a copy of p1.c's draw_frame with a left / right clip.
 *
 * PC check (not on the 32X): HB_B3_WALLSKULL=1 - when there is no SKULL type, the wall acts as
 * if the skull docked at once whenever it waits for it (Value1 = 1); with the SKULL type, the
 * skull the Skeletor would have left is pushed near the player when the wall first calls it.
 * HB_B3_TRACE=1 prints the wall's state every second.
 */
#define THEWALL_TOX (-320)               /* TunnelOffx */
#define THEWALL_TOY 140                  /* TunnelOffY */
#define THEWALL_HALF_PI FX(1.57079633)
#define THEWALL_PI      FX(3.14159265)

static struct {
	int ShotCount, HeartAnim, CountOpen;
	fx ShotDelay, SmokeDelay, KringelWinkel, HeartBeat, DoorOffset, OldEnergy, SkullEnergy;
	fx VorneX, VorneY, Laughing, dummy, SkullShotDelay;
} twl;
static Pic thewall_tuer, thewall_kringel[2], thewall_brain, thewall_rahmen, thewall_vorne;

static void thewall_load(void)
{
	thewall_tuer = pic("endbosstuer.png");
	thewall_kringel[0] = pic("endbosskringel.png");
	thewall_kringel[1] = pic("endbosskringel2.png");
	thewall_brain = pic("endbosshirni.png");
	thewall_rahmen = pic("endbossrahmen.png");
	thewall_vorne = pic("endbossmaschine_vorne.png");
}

static void thewall_init(Ent *e)
{
	memset(&twl, 0, sizeof twl);
	twl.HeartAnim = 0;
	twl.CountOpen = 3;
	twl.SkullEnergy = FX(250);
	e->Active = 1;
	e->Handlung = GEGNER_INIT;
	e->blick = -1;
	e->Energy = FX(4000);
	twl.OldEnergy = e->Energy;
	e->Destroyable = 0;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	e->Value1 = -1;
	e->AnimCount = FX(10);
}

/* frame `frame` of p at world (x, y), only its part in [cx0, cx1) (world fx): draw_frame
 * (game/p1.c) with a left / right clip, for the doors' SetRect */
static void thewall_draw_clip(const Pic *p, int frame, fx x, fx y, fx cx0, fx cx1)
{
	if (!p || !p->count) return;
	Frame f;
	frame_of(p, frame, &f);
	if (!f.data) return;
	const int bx = (int)(x >> 9) + f.x0, by = (int)(y >> 9) + f.y0;
	int vx0 = view_x, vx1 = view_x + VIEW_W;
	const int vy0 = view_y, vy1 = view_y + VIEW_H;
	if ((int)(cx0 >> 9) > vx0) vx0 = (int)(cx0 >> 9);
	if ((int)(cx1 >> 9) < vx1) vx1 = (int)(cx1 >> 9);
	if (vx0 >= vx1 || bx >= vx1 || by >= vy1 || bx + f.w <= vx0 || by + f.h <= vy0) return;
	const int d0 = by < vy0 ? vy0 - by : 0;
	const int d1 = by + f.h > vy1 ? vy1 - by : f.h;
	for (int d = d0; d < d1; d++)
	{
		const uint8_t *q = f.data + rd32(f.data + 4 * d);
		int runs = rd16(q);
		q += 2;
		int col = 0;
		while (runs--)
		{
			const int skip = rd16(q), len = rd16(q + 2);
			const uint8_t *src = q + 4;
			q = src + len + (len & 1);
			col += skip;
			int a = bx + col, e = a + len;
			col += len;
			if (e <= vx0 || a >= vx1) continue;
			if (draw_flash)
			{
				for (int i = 0; i < len; i++) rowbuf[i] = flash_lut[src[i]];
				src = rowbuf;
			}
			if (a < vx0) { src += vx0 - a; a = vx0; }
			if (e > vx1) e = vx1;
			fb_put(lin_of(a, by + d), src, e - a, 0);
			px_sprites += (uint32_t)(e - a);
		}
	}
	const int rx0 = bx > vx0 ? bx : vx0, rx1 = bx + f.w < vx1 ? bx + f.w : vx1;
	rect_add(rx0, by, rx1, by + f.h);
	if (over_list_n < MAX_RECTS)
		over_list[over_list_n++] = (Rect){ (int16_t)rx0, (int16_t)by, (int16_t)rx1, (int16_t)(by + f.h) };
}

static void thewall_draw(Ent *e)
{
	if (PlayerAbstand(e) > 800) return;
	const int deg = FINT(FMUL(twl.DoorOffset, FX(57.2957795)));
	const int toff = FINT(sin_d(deg) * 100);
	const fx x = e->xPos, y = e->yPos;

	/* the machine on the right, its front part */
	draw_sprite(&GegnerPic[e->GegnerArt], 0, x, y, 0);
	draw_sprite(&thewall_vorne, 0, x + twl.VorneX + FX(3), y + twl.VorneY + FX(148), 0);

	/* the skull on the machine (Hurrican: a part of endbosshirni.png not in the pack) */
	if (e->Value1 == 1) draw_sprite(&GegnerPic[SKULL], 5, x + FX(3) + twl.VorneX, y + FX(228) + twl.VorneY - FX(4), 0);

	/* the rings behind */
	draw_sprite(&thewall_kringel[1], FINT(twl.KringelWinkel / 3) % 5, x + FX(THEWALL_TOX + 38), y + FX(THEWALL_TOY - 14), 0);

	/* the heart */
	if (e->Handlung != GEGNER_EXPLODIEREN)
		draw_sprite(&thewall_brain, twl.HeartAnim, x + FX(THEWALL_TOX + 116), y + FX(THEWALL_TOY + 59), 0);
	/* (the red soup in the heart's tank: additive, and the tube holder: not in the pack) */

	/* the ring over it (rotated in Hurrican) */
	draw_sprite(&thewall_kringel[0], 0, x + FX(THEWALL_TOX + 37), y + FX(THEWALL_TOY - 16), 0);

	/* the doors: the left one shows its part toff..138 from +10, the right one 138..266-toff
	 * from +128+toff (the shadow: alpha, left out) */
	const fx dy = y + FX(THEWALL_TOY - 10);
	thewall_draw_clip(&thewall_tuer, 0, x + FX(THEWALL_TOX + 10 - toff), dy,
	                  x + FX(THEWALL_TOX + 10), x + FX(THEWALL_TOX + 10 + 138 - toff));
	thewall_draw_clip(&thewall_tuer, 0, x + FX(THEWALL_TOX + 128 + toff - 138), dy,
	                  x + FX(THEWALL_TOX + 128 + toff), x + FX(THEWALL_TOX + 128 + 128));

	/* the frame around */
	draw_sprite(&thewall_rahmen, 0, x + FX(THEWALL_TOX - 8), y + FX(THEWALL_TOY - 26), 0);
}

static void thewall_neue_aktion(Ent *e)
{
	if (twl.CountOpen <= 0)
	{
		/* open again */
		twl.CountOpen = 1;
		e->AnimCount = FX(50);
		e->Handlung = GEGNER_OEFFNEN;
		snd(SOUND_DOOR);
		proj_clear_type(SPIDERLASER);   /* the laser ends */
		e->Value1 = -1;                 /* the skull flies about */
		gegner_push(e->xPos + FX(3), e->yPos + FX(228), SKULL, 99, 0);
	}
	else
	{
		int i = GetRandom(3);
		while (i == e->LastAction) i = GetRandom(3);
		/* a player on the crates? Then the laser, else the electric floor */
		const int DoLaser = pl.ypos < cam_yoff() + FX(RENDERHEIGHT - 20 * 8);
		if (DoLaser) i = 2;
		e->LastAction = (int16_t)i;
		switch (i)
		{
		case 0:
			twl.ShotCount = 20;
			twl.ShotDelay = FX(5);
			e->Handlung = GEGNER_SPECIAL;
			twl.dummy = FX(GetRandom(10) + 15);
			break;
		case 1:
			twl.ShotCount = 16;
			twl.ShotDelay = FX(5);
			e->Handlung = GEGNER_SPECIAL2;
			break;
		case 2:
			twl.ShotCount = 2;
			twl.ShotDelay = FX(20);
			e->Handlung = DoLaser ? GEGNER_BOMBARDIEREN : GEGNER_SPECIAL3;
			break;
		}
	}
	twl.Laughing = FX(10);
	snd(SOUND_LAUGH);
}

/* call the skull back */
static void thewall_return_skull(void)
{
	for (int k = 0; k < MAX_LIVE; k++)
		if (ents[k].GegnerArt == SKULL) ents[k].Handlung = GEGNER_SPECIAL2;
}

static void thewall_ki(Ent *e)
{
#ifndef OJ32X_MARS
	if (getenv("HB_B3_WALLSKULL") && !gegner_known(SKULL) && e->Value1 < 0 &&
	    (e->Handlung == GEGNER_STEHEN || (e->Handlung == GEGNER_INIT && e->Value1 == -2)))
		e->Value1 = 1;
	if (getenv("HB_B3_WALLSKULL") && gegner_known(SKULL) && e->Value1 == -2)
	{
		/* the skull the Skeletor leaves (SKULL 0, 99), called back at once */
		int k;
		for (k = 0; k < MAX_LIVE; k++) if (ents[k].GegnerArt == SKULL) break;
		if (k == MAX_LIVE) gegner_push(pl.xpos, pl.ypos - FX(100), SKULL, 0, 99)->Handlung = GEGNER_SPECIAL2;
	}
	if (getenv("HB_B3_TRACE"))
	{
		static int n;
		if (n++ % 60 == 0)
			fprintf(stderr, "THEWALL: Handlung %d Energy %d SkullEnergy %d Value1 %d CountOpen %d\n", e->Handlung,
			        FINT(e->Energy), FINT(twl.SkullEnergy), e->Value1, twl.CountOpen);
	}
#endif
	/* shoot at the player? */
	if (e->Energy < FX(1500) && e->Value1 == 1)
	{
		twl.SkullShotDelay -= SYNC(FX(1));
		if (twl.SkullShotDelay < 0)
		{
			twl.SkullShotDelay = FX(15);
			proj_push(e->xPos, e->yPos + FX(240), SUCHSCHUSS2);
			snd(SOUND_GOLEMSHOT);
		}
	}

	if (twl.Laughing > 0)
	{
		twl.Laughing -= SYNC(FX(1));
		return;
	}

	if (e->Handlung != GEGNER_EXPLODIEREN)
	{
		/* the front part back */
		if (twl.VorneX > 0) twl.VorneX -= SYNC(FX(1));
		else twl.VorneX = 0;

		/* hit? Then the skull loses the energy */
		if (e->Energy < twl.OldEnergy)
		{
			if (twl.SkullEnergy > 0)
			{
				twl.SkullEnergy -= twl.OldEnergy - e->Energy;
				e->Energy = twl.OldEnergy;
			}
			else if (e->Handlung != GEGNER_OEFFNEN && e->Handlung != GEGNER_SCHLIESSEN)
			{
				twl.CountOpen = 0;
				thewall_neue_aktion(e);
			}
		}

		e->Destroyable = 0;
		/* the skull can be hit */
		if (e->Handlung != GEGNER_OEFFNEN && e->Handlung != GEGNER_SCHLIESSEN && e->Value1 == 1)
		{
			GegnerRect[e->GegnerArt] = (Box){ 5, 232, 5 + 20, 232 + 28 };
			e->Destroyable = 1;
		}
		/* the heart can be hit */
		if (e->Handlung == GEGNER_OEFFNEN || e->Handlung == GEGNER_SCHLIESSEN)
		{
			GegnerRect[e->GegnerArt] = (Box){ THEWALL_TOX + 116, THEWALL_TOY + 59, THEWALL_TOX + 116 + 38, THEWALL_TOY + 59 + 42 };
			e->Destroyable = 1;
		}

		/* the rings turn */
		twl.KringelWinkel += SYNC(FX(10));
		while (twl.KringelWinkel >= FX(360)) twl.KringelWinkel -= FX(360);

		/* the heart beats */
		fx b = FX(1);
		if (e->Energy < FX(3500)) b = FX(1);
		if (e->Energy < FX(3000)) b = FX(2);
		if (e->Energy < FX(2500)) b = FX(3);
		if (e->Energy < FX(2000)) b = FX(4);
		if (e->Energy < FX(1500)) b = FX(5);
		if (e->Energy < FX(1000)) b = FX(6);
		if (e->Energy < FX(500)) b = FX(7);
		if (e->Energy < FX(250)) b = FX(8);
		if (e->Energy < FX(150)) b = FX(9);
		twl.HeartBeat += SYNC(b);
		if (twl.HeartBeat > FX(10))
		{
			const int open = e->Handlung == GEGNER_OEFFNEN || e->Handlung == GEGNER_SCHLIESSEN;
			if (twl.HeartAnim == 0)
			{
				twl.HeartAnim = 1;
				twl.HeartBeat = FX(5);
				if (open) snd(SOUND_HEART2);
			}
			else
			{
				twl.HeartAnim = 0;
				twl.HeartBeat = 0;
				if (open) snd(SOUND_HEART1);
			}
		}

		/* smoke */
		twl.SmokeDelay -= SYNC(FX(1));
		if (twl.SmokeDelay < 0)
		{
			twl.SmokeDelay = FX(0.2);
			if (e->Energy < FX(3500)) part_push(e->xPos + FX(82), e->yPos + FX(260), SMOKE3);
			if (e->Energy < FX(3000)) part_push(e->xPos + FX(180), e->yPos + FX(250), SMOKE3);
			if (e->Energy < FX(2500)) part_push(e->xPos + FX(110), e->yPos + FX(320), SMOKE3);
			if (e->Energy < FX(2000)) part_push(e->xPos + FX(255), e->yPos + FX(130), SMOKE3);
			if (e->Energy < FX(1500) && GetRandom(20) == 0)
			{
				part_push(e->xPos - FX(10) + FX(GetRandom(10)), e->yPos + FX(220) + FX(GetRandom(10)), LASERFLAME);
				for (int i = 0; i < 10; i++)
					part_push(e->xPos + FX(10) + FX(GetRandom(10)), e->yPos + FX(240) + FX(GetRandom(10)), FUNKE);
				for (int i = 0; i < 10; i++)
					part_push(e->xPos + FX(20) + FX(GetRandom(2)), e->yPos + FX(240) + FX(GetRandom(10)), LASERFUNKE2);
			}
		}

		/* the energy bar */
		if (e->Handlung != GEGNER_INIT) boss_bar(e->Energy, FX(4000));

		/* the boss starts: lock the screen */
		if (e->Active && tile_zustand() == TS_SCROLLBAR)
		{
			if (PlayerAbstand(e) < 800 && PlayerAbstandHoriz(e) < 400)
			{
				scroll_level(e->xPos - FX(355), e->yPos, TS_SCROLLTOLOCK);
				music_stop();               /* FadeSong(STAGEMUSIC, -2, 0, pause) */
			}
		}

		/* no energy left: it breaks */
		if (e->Energy < FX(100))
		{
			e->Energy = FX(100);
			e->Destroyable = 0;
			/* the screen back to the player */
			scroll_level(pl.xpos - FX(300), pl.ypos - FX(280), TS_SCROLLTOPLAYER);
			if (pl.Handlung == PA_RADELN || pl.Handlung == PA_RADELN_FALL) pl.Handlung = PA_LAUFEN;
			twl.ShotCount = 1;
			e->Handlung = GEGNER_EXPLODIEREN;
			twl.Laughing = 0;
			proj_clear();                   /* all shots gone */
			music_stop();                   /* PauseSongs */
			snd(SOUND_GLASSBREAK);          /* the glass breaks */
			for (int i = 0; i < 20; i++)
				part_push(e->xPos + FX(THEWALL_TOX + 114 + GetRandom(30)), e->yPos + FX(THEWALL_TOY + 59 + GetRandom(45)), SCHLEIM);
			for (int i = 0; i < 80; i++)
				part_push(e->xPos + FX(THEWALL_TOX + 114 + GetRandom(30)), e->yPos + FX(THEWALL_TOY + 59 + GetRandom(45)), GLASSPLITTER);
			e->AnimCount = FX(100);         /* how long to wait */
			/* the skull goes */
			for (int k = 0; k < MAX_LIVE; k++)
			{
				Ent *o = &ents[k];
				if (o->GegnerArt != SKULL) continue;
				o->Value1 = 0;
				o->AnimCount = FX(0.5);
				o->Handlung = GEGNER_SPECIAL3;
			}
		}
	}

	switch (e->Handlung)
	{
	case GEGNER_INIT:
		if (PlayerAbstandHoriz(e) < 400 && PlayerAbstandVert(e) < 400 && PlayerAbstand(e) < 600)
		{
			/* call the skull back */
			if (e->Value1 == -1)
			{
				e->Value1 = -2;
				thewall_return_skull();
				snd(SOUND_LAUGH);
			}
			/* and start (after a short pause) */
			if (e->Value1 == 1)
			{
				e->AnimCount -= SYNC(FX(1));
				if (e->AnimCount <= 0) thewall_neue_aktion(e);
			}
		}
		break;

	case GEGNER_SPECIAL:                    /* golem shots */
		twl.ShotDelay -= SYNC(FX(1));
		if (twl.ShotDelay < 0)
		{
			proj_winkel = twl.dummy;
			snd(SOUND_GOLEMSHOT);
			proj_push(e->xPos, e->yPos + FX(332), GOLEMSCHUSS);
			part_push(e->xPos - FX(30), e->yPos + FX(295), EXPLOSIONFLARE);
			part_push(e->xPos, e->yPos + FX(320), SMOKEBIG);
			twl.ShotDelay = FX(1.2);
			twl.VorneX = FX(5);
			if (--twl.ShotCount <= 0) thewall_neue_aktion(e);
		}
		break;

	case GEGNER_SPECIAL2:                   /* purple balls */
		twl.ShotDelay -= SYNC(FX(1));
		if (twl.ShotDelay < 0)
		{
			snd(SOUND_LILA);
			proj_push(e->xPos, e->yPos + FX(332), SPIDERSHOT2);
			part_push(e->xPos - FX(30), e->yPos + FX(295), EXPLOSIONFLARE);
			part_push(e->xPos, e->yPos + FX(320), SMOKEBIG);
			twl.ShotDelay = FX(5);
			twl.VorneX = FX(5);
			if (--twl.ShotCount <= 0) thewall_neue_aktion(e);
		}
		break;

	case GEGNER_SPECIAL3:                   /* electric floor shots */
		twl.ShotDelay -= SYNC(FX(1));
		if (twl.ShotDelay < 0)
		{
			if (twl.ShotCount > 1)
			{
				snd(SOUND_LILA);
				snd(SOUND_GOLEMLOAD);
				proj_push(e->xPos + FX(220), e->yPos + FX(210), ELEKTROSCHUSS);
			}
			twl.ShotDelay = FX(30);
			if (--twl.ShotCount <= 0) thewall_neue_aktion(e);
		}
		break;

	case GEGNER_BOMBARDIEREN:               /* the big laser */
		twl.ShotDelay -= SYNC(FX(1));
		if (twl.ShotDelay < 0)
		{
			if (twl.ShotCount > 1)
			{
				proj_push(cam_xoff() - FX(40), e->yPos + FX(240), SPIDERLASER);
				snd(SOUND_BEAMLOAD2);
			}
			twl.ShotDelay = FX(60);
			if (--twl.ShotCount <= 0) thewall_neue_aktion(e);
		}
		break;

	case GEGNER_STEHEN:
		if (e->Value1 == 1) thewall_neue_aktion(e);
		break;

	case GEGNER_OEFFNEN:                    /* the doors open, then wait */
		if (twl.DoorOffset < THEWALL_HALF_PI) twl.DoorOffset += SYNC(FX(0.075));
		else
		{
			twl.DoorOffset = THEWALL_HALF_PI;
			/* (Hurrican: if the door sound still plays, stop it and play STEAM2 - no
			 * WaveIsPlaying here) */
			e->AnimCount -= SYNC(FX(1));
			if (e->AnimCount < 0)
			{
				snd(SOUND_DOOR);
				e->Handlung = GEGNER_SCHLIESSEN;
			}
		}
		break;

	case GEGNER_SCHLIESSEN:                 /* the doors close */
		twl.DoorOffset += SYNC(FX(0.125));
		if (twl.DoorOffset > THEWALL_PI)
		{
			twl.DoorOffset = 0;
			shake_screen(2);
			snd(SOUND_DOORSTOP);
			for (int i = 0; i < 36; i++)
				part_push(e->xPos + FX(THEWALL_TOX + 110 + GetRandom(30)), e->yPos + FX(THEWALL_TOY + GetRandom(10) + i * 5 - 25), SMOKE);
			for (int i = 0; i < 15; i++)
				part_push(e->xPos + FX(THEWALL_TOX + 95 + GetRandom(30)), e->yPos + FX(THEWALL_TOY + GetRandom(10) + i * 10 - 25), SMOKEBIG);
			e->Handlung = GEGNER_STEHEN;
			e->AnimCount = FX(20);
			twl.OldEnergy = e->Energy;
			twl.SkullEnergy = FX(250);
			thewall_return_skull();         /* call the skull back */
		}
		break;

	case GEGNER_EXPLODIEREN:
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount < 0 && twl.ShotCount > 0)
		{
			twl.ShotCount = 0;
			/* ScrolltoPlayeAfterBoss */
			scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
			music_stage();
		}
		break;

	default:
		break;
	}
}

/* (GegnerExplode: empty) */
static void thewall_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(THEWALL) = { thewall_init, thewall_ki, thewall_explode, thewall_draw, thewall_load };
