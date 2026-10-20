/*
 * Hurrican CD32X - the player: the full port of Hurrican's PlayerClass (Player.cpp /
 * Player.hpp) and the player's part of Gameplay.cpp (the order of GetPlayerInput, AnimatePlayer,
 * MovePlayer, ScrollFlugsack, DoPlattformStuff, CheckForExplode) and HUD.cpp (BlitzLength).
 * One player; skill medium. Included by game/p1.c (PLAYER_C); the interface is in game/hur.h.
 *
 * CONTROLS (Hurrican's AKTION_x on the 6-button pad):
 *   LEFT / RIGHT     AKTION_LINKS / AKTION_RECHTS (walk; turn the lightning / beam)
 *   UP               AKTION_OBEN (aim up, diagonally with LEFT / RIGHT; fly up on the sack)
 *   DOWN             AKTION_DUCKEN + AKTION_UNTEN (duck; fly down on the sack)
 *   B                AKTION_SHOOT (hold: autofire as Hurrican)
 *   C                AKTION_JUMP (hold for higher jumps; also turns the wheel back)
 *   A                AKTION_BLITZ (hold: the lightning; A + B held: load the beam, letting go
 *                    of either fires it when loaded enough, as Hurrican)
 *   X                AKTION_WAFFEN_CYCLE (next weapon)
 *   Y                AKTION_GRANATE (grenade)
 *   Z                AKTION_POWERLINE (power line)
 *   DOWN + Z         the wheel (Hurrican: AKTION_POWERLINE while AKTION_DUCKEN, on the ground,
 *                    Armour > 5); as the wheel Z fires power lines, B lays bombs, C turns back
 *   UP + Z           AKTION_SMARTBOMB (in Hurrican a key of its own)
 *   START            the engine's pause
 * Hurrican latches its keys itself (JumpPossible, PowerLinePossible, weaponswitchlock), so no
 * pad edges are needed.
 *
 * Ported: walking, running animation, ducking (and standing up only under a free ceiling),
 * looking / aiming up and diagonally (WalkLock: aiming up while standing stops the walk),
 * jumping with Hurrican's physics (JumpStart, JumpAdd, JumpySave, PLAYER_MAXJUMP, the shorter
 * fall after a short jump, the different jump in liquids), slopes, swamp sinking, ceilings,
 * platforms (DoPlattformStuff, the lift FAHRSTUHL), liquids (InLiquid: slower, splashes,
 * bubbles), waterfall drops, damage tiles, GegnerDran (half speed, half jump), the three
 * weapons with all 8 levels each (spread, laser, bounce, normal and RiesenShotExtra "big"
 * shots), autofire / AutoFireExtra / RiesenShotExtra timers, the shot flame (CalcFlamePos /
 * CalcAustrittsPunkt / FlameTime / FlameAnim) and the shell / smoke / spark particles, the
 * lightning (BLITZEN: start delay, turning, its damage to enemies, the SHOOTBUTTON stop,
 * destroyable walls, walls ending it; drawn with the flash sprites and draw_line strands), the
 * beam (BEAMLADEN -> proj_blitzbeam), the wheel (RADELN / RADELN_FALL, Armour, bouncing,
 * turning at walls, bombs), grenades, power lines, smart bombs (also as the wheel), the
 * shield (Shield: counts down; drawn by the SHIELDSPAWNER projectiles as Hurrican, or here with
 * PLF_DRAW_SHIELD 1), riding the flying sack
 * (SACKREITEN / DREHEN, xadd / yadd flying, smoke, the WENDEPUNKT jump-off pushing FLUGSACK),
 * the level-exit walk (FesteAktion / DoFesteAktion / RunPlayerExit / CheckLevelExit),
 * exploding (CheckForExplode: particles, DamageEnemiesonScreen, ExplodingTimer, runExplode's
 * weapon loss) and PA_TOT for the engine's life handling, damage sparks and smoke
 * (DoStuffWhenDamaged), the damage flash (WasDamaged) and power-up blink (BlinkCounter), the
 * idle animations (look around, Bronson / pissen), MovePlayer's level / locked-screen limits.
 *
 * Left out: surfing (SURFEN.. is #if 0 in Hurrican itself: no picture, the SURFBRETT trigger's
 * KI is commented out), ice and conveyor belts and ScreenWinkel (the map keeps no such flags),
 * the camera's look up / down and its player-following edges (the engine's camera owns
 * TileEngine.XOffset / YOffset), lighting colours, glows and flares (SchussFlammeFlare, the
 * scaled Blitzflash glows), the additive "bright" draws (the engine's hit flash is used for
 * WasDamaged / BlinkCounter instead), rumble, the piss text box, the demo recorder, the
 * flame thrower cheat, the tutorial, two players, the Punisher's Vanish on death (the
 * Punisher port can check pl.Handlung == PA_EXPLODIEREN).
 *
 * Engine extras kept from the first version: safe_x / safe_y (= Hurrican's JumpxSave /
 * JumpySave) as the place to come back, safe_t (150 steps of safety, blinking), the HB_GOD
 * host check, falling out of the level kills.
 *
 * Shared with later files (all one translation unit; the objects come after this file):
 *   plf_jumped_off - the platform object the player jumped off in this step (MUSHROOM's
 *                    PlayerJumps needs its own `count`, so the mushroom port does that part)
 *   plf_FlugsackFliesFree - Hurrican's FlugsackFliesFree (set here from the REITFLUGSACK's
 *                    Value1 when the player mounts it)
 *
 * Own state (static, plf_ prefix): the Aktion bits, bl / br / bo / bu, xadd / yadd, the
 * counters Hurrican keeps in the class or in function statics.
 */

/* ------------------------------------------------------------------ constants (Player.hpp) */
#define PLAYER_MOVESPEED      FX(20)
#define PLAYER_ANIMSPEED      FX(0.4)
#define PLAYER_MAXJUMP        FX(124)
#define PLAYER_MAXJUMPSPEED   FX(43)
#define PLAYER_JUMPADDSPEED   FX(12.5)
#define PLAYERANIM_RAD_ENDE   7
#define FRAMES_IDLE           19
#define FRAMES_RUN            20
#define FRAMES_JUMP           12
#define FRAMES_SURROUND       36
#define PLAYER_FLUGSACKSPEED  FX(4)
#define PLAYER_BLITZ_SPEED    FX(0.5)
#define PLAYER_BLITZ_START    FX(4)
#define PLAYER_BEAM_MAX       FX(160)
#define PLAYER_SHOTDELAY      FX(1.8)
#define BLITZ_POWER           FX(4)

/* 0 (Hurrican's way, the default): the shield is drawn by the SHIELDSPAWNER projectiles
 *   (game/proj_full.c) that the extra pushes and that player_respawn pushes;
 * 1: draw it here from pl.Shield (two spawners and their glow trail round the player) and push
 *   no spawners - only for a projectile port without SHIELDSPAWNER */
#ifndef PLF_DRAW_SHIELD
#define PLF_DRAW_SHIELD 0
#endif

/* AKTION_x (Hurrican's numbers: ENDLEVEL's Value2 is one of them) */
enum {
	AK_LINKS = 0, AK_RECHTS = 1, AK_DUCKEN = 2, AK_OBEN = 3, AK_UNTEN = 4, AK_JUMP = 5,
	AK_SHOOT = 6, AK_BLITZ = 7, AK_POWERLINE = 8, AK_GRANATE = 9, AK_SMARTBOMB = 10,
	AK_WAFFEN_CYCLE = 11, AK_MAX = 15
};
static uint16_t plf_ak;                      /* Aktion (bit n = AKTION n) */
#define AK(n) ((plf_ak >> (n)) & 1)

/* ------------------------------------------------------------------ state */
static int plf_bl, plf_br, plf_bo, plf_bu;   /* block values of this step */
static int plf_weaponswitchlock;
static fx plf_xadd, plf_yadd;                /* flying on the sack */
static fx plf_SmokeCount, plf_BeamCount, plf_changecount, plf_BeamX, plf_BeamY;
static fx plf_sparkcount, plf_smokecount;    /* DoStuffWhenDamaged's statics */
static int plf_WasDamaged;                   /* damaged since the last draw */
static int plf_exited;                       /* walked out of the screen at the level's end */
static int plf_FlugsackFliesFree = 1;
static fx plf_ryoff;                         /* the riding screen's YOffset (see SACKREITEN) */
static Ent *plf_jumped_off;
static int plf_dead_ride;                    /* the riding Handlung when he died riding (or -1) */
static int plf_restore;                      /* come back at safe_x / safe_y */
static int plf_blitz_snd;                    /* the lightning's loop sound: steps to the next */
/* the lightning as computed in the step, for drawing */
static int plf_blitz_vis, plf_drawlen;
static fx plf_fx0, plf_fy0, plf_fx1, plf_fy1;    /* the start / end flash (top-left) */
static int8_t plf_jag[3][16];                    /* the strands' random offsets */
static fx plf_shield_ang;                        /* the shield spawners' angle (fx degrees) */

static Pic PP_idle, PP_idle2, PP_kucken, PP_run, PP_diag, PP_oben, PP_crouch, PP_jump, PP_jumpdiag,
           PP_jumpup, PP_blitz, PP_piss, PP_ride, PP_rad, PP_flame[3], PP_flash[4], PP_spawner, PP_shieldglow;

static void player_load(void)
{
	PP_idle = pic("p1_hurri_idle.png");
	PP_kucken = pic("p1_hurri_kucken.png");
	PP_idle2 = pic("p1_hurri_idleumkuck.png");
	PP_run = pic("p1_hurri_laufen.png");
	PP_diag = pic("p1_hurri_shootdiagonal.png");
	PP_oben = pic("p1_hurri_shootup.png");
	PP_crouch = pic("p1_hurri_crouch.png");
	PP_jump = pic("p1_hurri_jump.png");
	PP_jumpdiag = pic("p1_hurri_jumpschraeg.png");
	PP_jumpup = pic("p1_hurri_jumpup.png");
	PP_blitz = pic("p1_hurri_rundumschuss.png");
	PP_piss = pic("p1_hurri_pissen.png");
	PP_ride = pic("p1_hurri_ride.png");
	PP_rad = pic("p1_hurri_rad.png");
	PP_flame[0] = pic("schussflamme.png");
	PP_flame[1] = pic("schussflamme2.png");
	PP_flame[2] = pic("schussflamme3.png");
	PP_flash[0] = pic("blitzflash1.png");
	PP_flash[1] = pic("blitzflash2.png");
	PP_flash[2] = pic("blitzflash3.png");
	PP_flash[3] = pic("blitzflash4.png");
	PP_spawner = pic("shieldspawner.png");
	PP_shieldglow = pic("shield.png");
}

static int plf_riding(void) { return pl.Handlung == PA_SACKREITEN || pl.Handlung == PA_DREHEN; }
static int plf_wheel(void) { return pl.Handlung == PA_RADELN || pl.Handlung == PA_RADELN_FALL; }
static void plf_rect(int l, int t, int r, int b) { pl.CollideRect = (Box){ (int16_t)l, (int16_t)t, (int16_t)r, (int16_t)b }; }
static fx plf_clamp_angle(fx w)
{
	while (w >= FX(360)) w -= FX(360);
	while (w < 0) w += FX(360);
	return w;
}
/* TileEngine.YOffset as the player's code sees it: the riding passage scrolls by itself */
static fx plf_yoff(void) { return plf_riding() && !plf_FlugsackFliesFree ? plf_ryoff : cam_yoff(); }

/* ------------------------------------------------------------------ init */
/* InitNewLevel */
static void plf_init_new_level(void)
{
	pl.BronsonCounter = 0;
	pl.Handlung = PA_STEHEN;
	pl.JumpAdd = 0;
	pl.AnimPhase = 0;
	pl.AnimCount = 0;
	pl.xspeed = pl.yspeed = 0;
	plf_xadd = plf_yadd = 0;
	pl.Blickrichtung = 1;
	pl.JumpPossible = 1;
	plf_rect(20, 12, 50, 80);
	pl.Energy = MAX_ENERGY;
	pl.Armour = MAX_ARMOUR;
	pl.ShotDelay = 0;
	pl.BlitzWinkel = 0;
	pl.BlitzCount = 0;
	pl.BlitzAnim = 0;
	pl.PowerLinePossible = 1;
	pl.AufPlattform = 0;
	pl.InLiquid = 0;
	pl.DamageCounter = 0;
	pl.Shield = 0;
	pl.FlameTime = 0;
	pl.SecretThisLevel = pl.DiamondsThisLevel = pl.LivesThisLevel = pl.BlocksThisLevel = 0;
	pl.CanBeDamaged = 1;
	pl.WalkLock = 1;
	pl.FesteAktion = -1;
	pl.DoFesteAktion = 0;
	pl.StageClearRunning = 0;
	pl.ExplodingTimer = 0;
	pl.GegnerDran = 0;
	pl.BlinkCounter = 0;
	plf_ak = 0;
	plf_bl = plf_br = plf_bo = plf_bu = 0;
	plf_weaponswitchlock = 0;
	plf_SmokeCount = plf_BeamCount = plf_changecount = 0;
	plf_sparkcount = plf_smokecount = 0;
	plf_WasDamaged = 0;
	plf_exited = 0;
	plf_FlugsackFliesFree = 1;
	plf_jumped_off = 0;
	plf_dead_ride = -1;
	plf_restore = 0;
	plf_blitz_snd = 0;
	plf_blitz_vis = 0;
}

/* InitPlayer */
static void player_init_game(void)
{
	memset(&pl, 0, sizeof pl);
	pl.BronsonCounter = 0;
	pl.Score = 0;
	pl.Lives = 3;
	pl.Armour = MAX_ARMOUR;
	pl.Energy = MAX_ENERGY;
	pl.PowerLines = 3;
	pl.Grenades = 3;
	pl.SmartBombs = 1;
	pl.SelectedWeapon = 0;
	pl.CollectedDiamonds = 0;
	pl.CurrentWeaponLevel[0] = 1;
	pl.CurrentWeaponLevel[1] = 0;
	pl.CurrentWeaponLevel[2] = 0;
	pl.CurrentWeaponLevel[3] = 2;
	pl.BlitzLength = 3;
	pl.AutoFireCount = 0;
	pl.AutoFireExtra = 0;
	pl.RiesenShotExtra = 0;
	pl.GodMode = 0;
	pl.WheelMode = 0;
#ifndef OJ32X_MARS
	if (getenv("HB_WEAPONS"))          /* PC check: weapon levels "spread,laser,bounce,blitz" */
	{
		int a = 1, b = 0, c = 0, d = 2;
		if (sscanf(getenv("HB_WEAPONS"), "%d,%d,%d,%d", &a, &b, &c, &d) >= 3)
		{
			pl.CurrentWeaponLevel[0] = a; pl.CurrentWeaponLevel[1] = b;
			pl.CurrentWeaponLevel[2] = c; pl.CurrentWeaponLevel[3] = d;
		}
	}
#endif
	plf_init_new_level();
}

static void player_init_level(fx x, fx y, int blick)
{
	plf_init_new_level();
	pl.xpos = pl.xposold = x;
	pl.ypos = pl.yposold = y;
	pl.Blickrichtung = blick;
	pl.JumpxSave = pl.safe_x = x;
	pl.JumpySave = pl.safe_y = y;
	pl.JumpStart = y;
	pl.safe_t = 0;
	pl.dead_t = 0;
}

static int player_hit_box(Box *b)
{
	*b = pl.CollideRect;
	return pl.Handlung != PA_TOT && pl.Handlung != PA_EXPLODIEREN && !plf_exited;
}

/* ------------------------------------------------------------------ damage, death */
#ifndef OJ32X_MARS
static int host_god = -1;            /* PC check: HB_GOD=1, the player takes no damage */
#endif
static void DamagePlayer(fx amount, int override_)
{
#ifndef OJ32X_MARS
	if (host_god < 0) host_god = getenv("HB_GOD") != 0;
	if (host_god && amount < FX(150)) return;
#endif
	if (pl.Handlung == PA_TOT || pl.Handlung == PA_EXPLODIEREN || plf_exited) return;
	if (pl.GodMode || !pl.CanBeDamaged || (pl.Shield > 0 && !override_)) return;
	if (pl.safe_t > 0 && amount < FX(150)) return;       /* the engine's safety after coming back */
	amount = amount * 6 / 10;                            /* SKILL_MEDIUM */
	if (pl.DamageCounter == 0 && !plf_wheel()) snd(SOUND_ABZUG);
	/* the wheel cannot be hurt */
	if (!plf_wheel() || override_)
	{
		pl.Energy -= amount;
		if (pl.Energy < 0) pl.Energy = 0;
		pl.DamageCounter = FX(5);
		plf_WasDamaged = 1;
	}
}

/* runExplode: the explosion is over */
static void plf_run_explode(void)
{
	pl.ExplodingTimer -= SYNC(FX(1));
	if (pl.ExplodingTimer > 0) return;
	plf_rect(20, 12, 50, 79);
	/* the weapon and the lightning one level down, power lines / grenades at least 1 */
	if (pl.CurrentWeaponLevel[pl.SelectedWeapon] > 1) pl.CurrentWeaponLevel[pl.SelectedWeapon]--;
	if (pl.CurrentWeaponLevel[3] > 2) pl.CurrentWeaponLevel[3]--;
	if (pl.PowerLines < 1) pl.PowerLines = 1;
	if (pl.Grenades < 1) pl.Grenades = 1;
	/* back at the last safe place (JumpxSave / JumpySave) when the screen scrolls freely */
	plf_restore = (tile_zustand() == TS_SCROLLBAR && plf_dead_ride < 0) || FINT(pl.ypos) > map_h * TILESIZE;
	pl.yspeed = 0;
	pl.JumpAdd = 0;
	/* the engine takes the life and calls player_respawn (or ends the game) */
	pl.Handlung = PA_TOT;
	pl.dead_t = 0;
	pl.AufPlattform = 0;
}

/* CheckForExplode */
static void plf_check_for_explode(void)
{
	if (pl.Energy > 0 || pl.Handlung == PA_EXPLODIEREN || pl.Handlung == PA_TOT) return;
	plf_rect(0, 0, 0, 0);
	snd(SOUND_EXPLOSION2);
	DamageEnemiesonScreen(pl.xpos + FX(35), pl.ypos + FX(40), 400);
	pl.BronsonCounter = 0;
	pl.Shield = 0;
	shake_screen(5);
	for (int i = 0; i < 3; i++)
		part_push(pl.xpos + FX(10 + GetRandom(50)), pl.ypos + FX(10 + GetRandom(50)), SPLITTER);
	for (int i = 0; i < 10; i++)
		part_push(pl.xpos + FX(10 + GetRandom(50)), pl.ypos + FX(10 + GetRandom(50)), SPIDERSPLITTER);
	part_push(pl.xpos + FX(35 - 90), pl.ypos + FX(40 - 90), EXPLOSION_GIGA);
	part_push(pl.xpos + FX(35), pl.ypos + FX(40), SHOCKEXPLOSION);
	part_push(pl.xpos - FX(20), pl.ypos - FX(20), EXPLOSIONFLARE);
	part_push(pl.xpos + FX(20), pl.ypos + FX(10), HURRITEILE_KOPF);
	part_push(pl.xpos + FX(20), pl.ypos + FX(10), HURRITEILE_ARM1);
	part_push(pl.xpos + FX(20), pl.ypos + FX(10), HURRITEILE_ARM2);
	part_push(pl.xpos + FX(20), pl.ypos + FX(10), HURRITEILE_BEIN1);
	part_push(pl.xpos + FX(20), pl.ypos + FX(10), HURRITEILE_BEIN2);
	part_push(pl.xpos + FX(20), pl.ypos + FX(10), HURRITEILE_WAFFE);
	part_push(pl.xpos + FX(20), pl.ypos + FX(10), HURRITEILE_TORSO);
	part_push(pl.xpos - FX(88), pl.ypos - FX(88), GRENADEFLARE);
	pl.ExplodingTimer = FX(30);
	plf_dead_ride = -1;
	if (plf_riding())
	{
		/* riding: straight on (Hurrican keeps him on the sack) */
		if (pl.ypos >= plf_yoff() + FX(475)) { pl.ypos = plf_yoff() + FX(470); plf_yadd = FX(-25); }
		plf_dead_ride = pl.Handlung;
		pl.ExplodingTimer = 0;
		snd(SOUND_EXPLOSION2);
		for (int i = 0; i < 15; i++)
			part_push(pl.xpos - FX(10) + FX(GetRandom(80)), pl.ypos - FX(10) + FX(GetRandom(80)), EXPLOSION_MEDIUM2);
		for (int i = 0; i < 40; i++)
			part_push(pl.xpos + FX(GetRandom(90)), pl.ypos + FX(GetRandom(90)), LONGFUNKE);
		plf_run_explode();
	}
	else
		pl.Handlung = PA_EXPLODIEREN;
	pl.InLiquid = 0;
}

/* the second half of runExplode: coming back with a new life */
static void player_respawn(void)
{
	if (plf_restore)
	{
		pl.xpos = pl.xposold = pl.safe_x;
		pl.ypos = pl.yposold = pl.safe_y;
	}
	/* in the lift level: back on the lift */
	if (g_Fahrstuhl_yPos > 0)
	{
		pl.xpos = cam_xoff() + FX(320 - 35);
		pl.ypos = g_Fahrstuhl_yPos - FX(pl.CollideRect.b) - FX(1);
		pl.xposold = pl.xpos; pl.yposold = pl.ypos;
		pl.JumpxSave = pl.safe_x = pl.xpos;
		pl.JumpySave = pl.safe_y = pl.ypos;
	}
#if !PLF_DRAW_SHIELD
	proj_push(pl.xpos, pl.ypos, SHIELDSPAWNER);
	proj_push(pl.xpos, pl.ypos, SHIELDSPAWNER2);
#endif
	pl.Shield += FX(20);
	pl.Energy = MAX_ENERGY;
	pl.Armour = MAX_ARMOUR;
	if (plf_dead_ride >= 0)
	{
		pl.Handlung = plf_dead_ride;
		plf_rect(15, 10, 75, 80);
	}
	else
	{
		pl.Handlung = PA_SPRINGEN;
		plf_rect(20, 12, 50, 79);
	}
	pl.AnimPhase = 0;
	pl.yspeed = 0;
	/* (Hurrican: JumpAdd 0, which leaves him hanging in the air while jump is held) */
	pl.JumpAdd = PLAYER_JUMPADDSPEED;
	pl.JumpStart = pl.ypos;
	pl.JumpPossible = 0;
	pl.AufPlattform = 0;
	pl.DamageCounter = 0;
	pl.safe_t = 150;
	pl.dead_t = 0;
	plf_dead_ride = -1;
	plf_restore = 0;
}

/* DoStuffWhenDamaged: sparks and smoke at low energy */
static void plf_damaged_stuff(void)
{
	if (pl.Handlung == PA_EXPLODIEREN || pl.Energy > MAX_ENERGY / 2) return;
	if (plf_sparkcount > 0) plf_sparkcount -= SYNC(FX(1));
	else
	{
		plf_sparkcount = FX(GetRandom(FINT(pl.Energy / 2) + 2) + 5);
		const int effect = GetRandom(3);
		if (effect < 2)
		{
			const fx x = pl.xpos + FX(20 + GetRandom(40)), y = pl.ypos + FX(20 + GetRandom(40));
			for (int i = 0; i < 5; i++) part_push(x + FX(GetRandom(4)), y + FX(GetRandom(4)), effect == 0 ? FUNKE : LONGFUNKE);
			part_push(x - FX(20), y - FX(20), LASERFLAME);
			snd(SOUND_FUNKE);
		}
	}
	if (plf_smokecount > 0) plf_smokecount -= SYNC(FX(1));
	else
	{
		plf_smokecount = FX(0.8);
		if (pl.Energy < MAX_ENERGY / 3 + FX(5) && GetRandom(2) == 0)
			part_push(pl.xpos + FX(10 + GetRandom(30)), pl.ypos + FX(20 + GetRandom(40)), SMOKE2);
		if (pl.Energy < MAX_ENERGY / 4 + FX(5))
			part_push(pl.xpos + FX(26 + pl.Blickrichtung * 4 + GetRandom(4)), pl.ypos + FX(20 + GetRandom(4)), SMOKE3);
	}
}

/* ------------------------------------------------------------------ the gun */
/* CalcFlamePos (relative to xpos / ypos: Hurrican's minus the screen offset) */
static void plf_calc_flame_pos(void)
{
	fx xoff, yoff;
	const int h = pl.Handlung;
	if (h == PA_SACKREITEN)
	{
		yoff = FX(1);
		xoff = pl.Blickrichtung > 0 ? FX(80) : FX(-35);
	}
	else
	{
		yoff = h == PA_DUCKEN ? FX(23) : 0;
		xoff = pl.Blickrichtung > 0 ? FX(66) : FX(-33);
	}
	if (pl.Blickrichtung > 0)
	{
		if (h == PA_STEHEN || h == PA_SACKREITEN || h == PA_DUCKEN) { pl.AustrittAnim = 0; pl.AustrittX = xoff - FX(10); pl.AustrittY = yoff + FX(20); }
		else if (h == PA_SPRINGEN)
		{
			if (AK(AK_OBEN))
			{
				if (!(AK(AK_LINKS) || AK(AK_RECHTS))) { pl.AustrittAnim = 2; pl.AustrittX = xoff - FX(47); pl.AustrittY = yoff - FX(34); }
				else { pl.AustrittAnim = 1; pl.AustrittX = xoff - FX(21); pl.AustrittY = yoff - FX(12); }
			}
			else { pl.AustrittAnim = 0; pl.AustrittX = xoff - FX(10); pl.AustrittY = yoff + FX(15); }
		}
		else if (h == PA_LAUFEN) { pl.AustrittAnim = 0; pl.AustrittX = xoff - FX(2); pl.AustrittY = yoff + FX(19); }
		else if (h == PA_SCHIESSEN_RO)
		{
			pl.AustrittAnim = 1;
			if (pl.WalkLock) { pl.AustrittX = xoff - FX(25); pl.AustrittY = yoff - FX(8); }
			else { pl.AustrittX = xoff - FX(21); pl.AustrittY = yoff - FX(12); }
		}
		else if (h == PA_SCHIESSEN_O) { pl.AustrittAnim = 2; pl.AustrittX = xoff - FX(49); pl.AustrittY = yoff - FX(33); }
	}
	else
	{
		if (h == PA_STEHEN || h == PA_SACKREITEN || h == PA_DUCKEN) { pl.AustrittAnim = 0; pl.AustrittX = xoff + FX(10); pl.AustrittY = yoff + FX(20); }
		else if (h == PA_SPRINGEN)
		{
			if (AK(AK_OBEN))
			{
				if (!(AK(AK_LINKS) || AK(AK_RECHTS))) { pl.AustrittAnim = 2; pl.AustrittX = xoff + FX(62); pl.AustrittY = yoff - FX(34); }
				else { pl.AustrittAnim = 1; pl.AustrittX = xoff + FX(21); pl.AustrittY = yoff - FX(12); }
			}
			else { pl.AustrittAnim = 0; pl.AustrittX = xoff + FX(6); pl.AustrittY = yoff + FX(15); }
		}
		else if (h == PA_LAUFEN) { pl.AustrittAnim = 0; pl.AustrittX = xoff; pl.AustrittY = yoff + FX(19); }
		else if (h == PA_SCHIESSEN_LO)
		{
			pl.AustrittAnim = 1;
			if (pl.WalkLock) { pl.AustrittX = xoff + FX(21); pl.AustrittY = yoff - FX(8); }
			else { pl.AustrittX = xoff + FX(8); pl.AustrittY = yoff - FX(10); }
		}
		else if (h == PA_SCHIESSEN_O) { pl.AustrittAnim = 2; pl.AustrittX = xoff + FX(62); pl.AustrittY = yoff - FX(33); }
	}
}

/* CalcAustrittsPunkt: where the shots leave the gun (relative to xpos / ypos) */
static void plf_calc_austritt(void)
{
	plf_calc_flame_pos();
	switch (pl.AustrittAnim)
	{
	case 0:
		pl.AustrittX += pl.Blickrichtung < 0 ? FX(34) : FX(4);
		pl.AustrittY += FX(11);
		break;
	case 1:
		pl.AustrittX += pl.Blickrichtung < 0 ? FX(32) : FX(8);
		pl.AustrittY += FX(26);
		break;
	case 2:
		pl.AustrittX += FX(12);
		pl.AustrittY += FX(38);
		break;
	case 3:
		pl.AustrittX += FX(20);
		pl.AustrittY += FX(18);
		break;
	}
}

/* the aim of a shot or grenade (wadd in PlayerShoot / PlayerGrenadeShoot) */
static int plf_wadd(void)
{
	const int h = pl.Handlung;
	if (h == PA_SCHIESSEN_O || (h == PA_SPRINGEN && AK(AK_OBEN) && !AK(AK_LINKS) && !AK(AK_RECHTS)))
		return pl.Blickrichtung < 0 ? 90 : -90;
	if (h == PA_SCHIESSEN_LO || (h == PA_SPRINGEN && AK(AK_OBEN) && AK(AK_LINKS))) return 45;
	if (h == PA_SCHIESSEN_RO || (h == PA_SPRINGEN && AK(AK_OBEN) && AK(AK_RECHTS))) return -45;
	return 0;
}

/* the spread shot at angle a (90 + wadd) */
static void plf_spread(fx x, fx y, int a, int art)
{
	proj_winkel = FX(a);
	proj_push(x, y, art);
}

/* PlayerShoot */
static void plf_shoot(void)
{
	plf_calc_austritt();
	const int yoff = pl.Handlung == PA_DUCKEN ? 23 : 0;
	const int wadd = plf_wadd();
	const int lvl = pl.CurrentWeaponLevel[pl.SelectedWeapon];
	const fx AX = pl.AustrittX, AY = pl.AustrittY, X = pl.xpos, Y = pl.ypos;
	const int big = pl.RiesenShotExtra > 0;

	if (plf_wheel())
	{
		/* the wheel lays bombs */
		if (big) proj_push(X + FX(26), Y + FX(46), BOMBEBIG);
		else proj_push(X + FX(30), Y + FX(56), BOMBE);
		return;
	}

	switch (pl.SelectedWeapon)
	{
	case 0:   /* spread shot */
	{
		const fx ta = big ? FX(16) : FX(8);
		const int s = big ? SPREADSHOTBIG : SPREADSHOT;
		snd(SOUND_SPREADSHOT);
		const fx x0 = X - ta + AX, y0 = Y - ta + AY;
		plf_spread(x0, y0, 90 + wadd, lvl < 5 ? s : s + 1);
		if (lvl > 1) { plf_spread(x0, y0, 82 + wadd, lvl < 6 ? s : s + 1); plf_spread(x0, y0, 98 + wadd, lvl < 6 ? s : s + 1); }
		if (lvl > 2) { plf_spread(x0, y0, 74 + wadd, lvl < 7 ? s : s + 1); plf_spread(x0, y0, 106 + wadd, lvl < 7 ? s : s + 1); }
		if (lvl > 3) { plf_spread(x0, y0, 66 + wadd, lvl < 8 ? s : s + 1); plf_spread(x0, y0, 114 + wadd, lvl < 8 ? s : s + 1); }
	}
	break;

	case 1:   /* laser */
	{
		const fx tx = big ? FX(12) : FX(8), ty = big ? FX(40) : FX(24);
		const int s = big ? LASERSHOTBIG : LASERSHOT;
		const fx m = FMUL(tx, FX(0.875));                 /* tempaddx * mul1 */
		const fx X0 = X - tx + AX, Y0 = Y - ty + AY;
		const int b = pl.Blickrichtung;
		snd(SOUND_LASERSHOT);
		proj_winkel = FX(90 + wadd);
#define LP(x, y, art) proj_push((x), (y), (art))
		if (lvl == 4 || lvl == 6) LP(X0 - FX(4), Y0, s + 1);
		if (lvl == 1 || lvl == 3 || lvl == 5) LP(X0, Y0, s);
		if (lvl == 2)
		{
			if (wadd == 0) { LP(X0, Y0 - m, s); LP(X0, Y0 + m, s); }
			if (wadd == 45) { LP(X0 + m - FX(2), Y0 - m, s); LP(X0 - m + FX(2), Y0 + m, s); }
			if (wadd == -45) { LP(X0 - m + FX(6), Y0 - m, s); LP(X0 + m + FX(2), Y0 + m, s); }
			if (wadd == 90 || wadd == -90) { LP(X0 - m, Y0, s); LP(X0 + m, Y0, s); }
		}
		if (lvl >= 7)
		{
			const fx m75 = FMUL(m, FX(0.75));
			if (wadd == 0) { LP(X0, Y0 - m, s + 1); LP(X0, Y0 + m, s + 1); }
			if (wadd == 45) { LP(X0 + m75, Y0 - m75, s + 1); LP(X0 - m75, Y0 + m75, s + 1); }
			if (wadd == -45) { LP(X0 - m75, Y0 - m75, s + 1); LP(X0 + m75, Y0 + m75, s + 1); }
			if (wadd == 90 || wadd == -90) { LP(X0 - m, Y0, s + 1); LP(X0 + m, Y0, s + 1); }
		}
		if (lvl >= 3 && lvl <= 4)
		{
			const fx m23 = FMUL(m, FX(2.3));
			if (wadd == 0) { LP(X0 - tx * b, Y0 - m23, s); LP(X0 - tx * b, Y0 + m23, s); }
			if (wadd == 90 || wadd == -90) { LP(X0 - m23, Y - ty / 2 + AY, s); LP(X0 + m23, Y - ty / 2 + AY, s); }
			if (wadd == 45) { LP(X + m23 + AX, Y0, s); LP(X + tx - m23 + AX, Y0 + tx * 3, s); }
			if (wadd == -45) { LP(X - m * 4 + AX, Y0, s); LP(X - tx * 2 + m23 + AX, Y0 + tx * 2, s); }
		}
		if (lvl == 5 || lvl == 6)
		{
			if (wadd == 0) { LP(X0 - tx * b * 2, Y0 - m * 2, s + 1); LP(X0 - tx * b * 2, Y0 + m * 2, s + 1); }
			if (wadd == 90 || wadd == -90) { LP(X + m * 2 - tx + AX - FX(4), Y0 + tx, s + 1); LP(X - m * 2 - tx + AX - FX(4), Y0 + tx, s + 1); }
			if (wadd == 45) { LP(X + tx + AX - FX(4), Y0, s + 1); LP(X - tx + AX - FX(4), Y0 + tx * 2, s + 1); }
			if (wadd == -45) { LP(X - tx * 3 + AX - FX(6), Y0, s + 1); LP(X - tx + AX - FX(4), Y0 + tx * 2, s + 1); }
		}
		if (lvl == 7)
		{
			if (wadd == 0) { LP(X0 - tx * b * 2, Y0 - m * 3, s); LP(X0 - tx * b * 2, Y0 + m * 3, s); }
			if (wadd == 45) { LP(X + tx * 2 + AX + FX(6), Y0, s); LP(X - tx * 2 + AX + FX(4), Y0 + tx * 5 / 2, s); }
			if (wadd == -45) { LP(X - tx * 4 + AX + FX(2), Y0, s); LP(X + AX + FX(4), Y0 + tx * 5 / 2, s); }
			if (wadd == 90 || wadd == -90)
			{
				const fx my = FMUL(ty, FX(0.875));
				LP(X - tx - my + AX + FX(3), Y - ty / 2 + AY, s);
				LP(X - tx + my + AX + FX(4), Y - ty / 2 + AY, s);
			}
		}
		if (lvl == 8)
		{
			if (wadd == 0) { LP(X0 - tx * b * 2, Y0 - tx * 3, s + 1); LP(X0 - tx * b * 2, Y0 + tx * 3, s + 1); }
			if (wadd == 45) { LP(X + tx * 3 + AX, Y0, s + 1); LP(X - tx * 2 + AX, Y0 + tx * 3, s + 1); }
			if (wadd == -45) { LP(X - tx * 9 / 2 + AX, Y0, s + 1); LP(X + AX, Y0 + tx * 3, s + 1); }
			if (wadd == 90 || wadd == -90)
			{
				const fx my = FMUL(ty, FX(0.875));
				LP(X - tx - my + AX, Y - ty / 2 + AY, s + 1);
				LP(X - tx + my + AX, Y - ty / 2 + AY, s + 1);
			}
		}
#undef LP
	}
	break;

	case 2:   /* bounce */
	{
		const fx ta = big ? FX(24) : FX(12);
		snd(SOUND_BOUNCESHOT);
		proj_winkel = FX(90 + wadd);
		proj_push(X - ta + AX, Y - ta + AY, big ? BOUNCESHOTBIG1 : BOUNCESHOT1);
	}
	break;
	}

	/* the shell, the laser's smoke, the bounce's sparks */
	if (pl.SelectedWeapon == 0) part_push(X + FX(30), Y + FX(24 + yoff), BULLET);
	else if (pl.SelectedWeapon == 1) part_push(X + FX(30), Y + FX(24 + yoff), SMOKE3);
	else
		for (int i = 0; i < 2; i++) part_push(X + FX(30 + GetRandom(4)), Y + FX(28 + yoff + GetRandom(4)), FUNKE2);

	pl.FlameTime = FX(1);
	pl.FlameAnim = GetRandom(2);
}

/* PlayerGrenadeShoot */
static void plf_grenade(void)
{
	plf_calc_austritt();
	proj_winkel = FX(90 + plf_wadd());
	proj_push(pl.xpos + pl.AustrittX - FX(4), pl.ypos + pl.AustrittY - FX(4), GRENADE);
	snd(SOUND_GRANATE);
	pl.PowerLinePossible = 0;
	pl.Grenades--;
	pl.FlameTime = FX(1);
	pl.FlameAnim = GetRandom(2);
}

static void plf_powerline(void)
{
	snd(SOUND_POWERLINE);
	pl.PowerLinePossible = 0;
	pl.PowerLines--;
	const int top = FINT(cam_yoff()) / 20 * 20;
	for (int i = -1; i < 25; i++)
	{
		proj_push(pl.xpos + FX(20), FX(top + i * 20), POWERLINE);
		proj_push(pl.xpos + FX(20), FX(top + i * 20), POWERLINE2);
	}
}

/* checkShoot */
static void plf_check_shoot(void)
{
	if (AK(AK_SHOOT) && pl.Handlung != PA_BLITZEN && pl.Handlung != PA_BEAMLADEN && pl.Handlung != PA_DREHEN &&
	    pl.AutoFireCount > 0 && pl.ShotDelay <= 0)
	{
		pl.ShotDelay = PLAYER_SHOTDELAY;
		plf_shoot();
		if (pl.Handlung != PA_LAUFEN && pl.Handlung != PA_SPRINGEN && pl.Handlung != PA_SACKREITEN &&
		    !plf_wheel() && pl.WalkLock)
			pl.AnimPhase = 1;
		if (pl.AutoFireExtra > 0) pl.ShotDelay = FDIV(pl.ShotDelay, FX(2.25));
		/* the recoil on the sack */
		if (pl.Handlung == PA_SACKREITEN) plf_xadd += pl.Blickrichtung < 0 ? FX(2) : FX(-2);
	}
	if (AK(AK_GRANATE) && pl.PowerLinePossible && pl.Grenades > 0 && !plf_wheel() &&
	    pl.Handlung != PA_BLITZEN && pl.Handlung != PA_DREHEN && pl.Handlung != PA_BEAMLADEN)
		plf_grenade();
	if (!AK(AK_SHOOT))
	{
		pl.ShotDelay = 0;
		if (pl.AutoFireCount < FX(3)) pl.AutoFireCount = FX(3);
	}
	if (pl.ShotDelay > 0) pl.ShotDelay -= SYNC(FX(1));
	else pl.ShotDelay = 0;
}

/* handleAutoFire */
static void plf_handle_autofire(void)
{
	if (pl.AutoFireExtra > 0) { pl.AutoFireExtra -= SYNC(FX(0.5)); pl.AutoFireCount = FX(3); }
	else pl.AutoFireExtra = 0;
	if (pl.RiesenShotExtra > 0) pl.RiesenShotExtra -= SYNC(FX(0.5));
	else pl.RiesenShotExtra = 0;
}

/* checkWeaponSwitch */
static void plf_check_weapon_switch(void)
{
	if (!AK(AK_WAFFEN_CYCLE)) plf_weaponswitchlock = 0;
	if (AK(AK_WAFFEN_CYCLE) && !plf_weaponswitchlock)
	{
		plf_weaponswitchlock = 1;
		do pl.SelectedWeapon++; while (pl.SelectedWeapon < 3 && pl.CurrentWeaponLevel[pl.SelectedWeapon] == 0);
		if (pl.SelectedWeapon >= 3) pl.SelectedWeapon = 0;
	}
}

/* ------------------------------------------------------------------ input */
static uint16_t plf_pad_aktion(uint16_t p)
{
	uint16_t a = 0;
	if (p & PAD_LEFT) a |= 1u << AK_LINKS;
	if (p & PAD_RIGHT) a |= 1u << AK_RECHTS;
	if (p & PAD_DOWN) a |= 1u << AK_DUCKEN | 1u << AK_UNTEN;
	if (p & PAD_UP) a |= 1u << AK_OBEN;
	if (p & PAD_C) a |= 1u << AK_JUMP;
	if (p & PAD_B) a |= 1u << AK_SHOOT;
	if (p & PAD_A) a |= 1u << AK_BLITZ;
	if (p & PAD_Y) a |= 1u << AK_GRANATE;
	if (p & PAD_X) a |= 1u << AK_WAFFEN_CYCLE;
	if (p & PAD_Z) a |= (p & PAD_UP) ? 1u << AK_SMARTBOMB : 1u << AK_POWERLINE;
	return a;
}

/* CheckLevelExit */
static int plf_check_level_exit(void)
{
	const fx xo = cam_xoff(), yo = cam_yoff();
	return pl.xpos + FX(pl.CollideRect.r) < xo || pl.xpos + FX(pl.CollideRect.l) > xo + FX(RENDERWIDTH) ||
	       pl.ypos + FX(pl.CollideRect.b) < yo || pl.ypos + FX(pl.CollideRect.t) > yo + FX(RENDERHEIGHT);
}

/* GetPlayerInput; 0 when the player does nothing this step */
static int plf_get_input(void)
{
	if (pl.Handlung == PA_TOT) return 0;
	if (pl.Handlung == PA_EXPLODIEREN) { plf_run_explode(); return 0; }
	plf_ak = 0;
	pl.xspeed = 0;                       /* (no ice in the maps) */
	if (pl.DoFesteAktion)
	{
		if (pl.FesteAktion >= 0 && pl.FesteAktion < AK_MAX) plf_ak = (uint16_t)(1u << pl.FesteAktion);
	}
	else
	{
		plf_ak = plf_pad_aktion(pad);
		if (plf_ak) pl.BronsonCounter = 0;
	}
	/* walking out at the level's end */
	if (pl.StageClearRunning && plf_check_level_exit()) { plf_exited = 1; return 0; }
	plf_check_shoot();
	plf_handle_autofire();
	plf_check_weapon_switch();
	if (plf_ak && pl.Handlung == PA_PISSEN) { pl.BronsonCounter = 0; pl.Handlung = PA_STEHEN; }
	if (pl.Handlung == PA_STEHEN || pl.Handlung == PA_PISSEN) pl.BronsonCounter += SYNC(FX(1));
	else
	{
		pl.BronsonCounter = 0;
		if (pl.Handlung == PA_PISSEN) pl.Handlung = PA_STEHEN;
	}
	if (pl.BronsonCounter > FX(200) && pl.Handlung != PA_PISSEN)
	{
		pl.AnimCount = 0;
		pl.AnimPhase = 0;
		pl.Handlung = PA_PISSEN;
	}
	return 1;
}

/* ------------------------------------------------------------------ AnimatePlayer */
static int plf_lightning_phase(void)
{
	fx w = pl.BlitzWinkel - FX(270);           /* 270 looking left = phase 0 */
	if (w < 0) w += FX(360);
	if (pl.Blickrichtung < 0) w = FX(360) - w;
	else w = FX(180) + w;
	int ph = (FINT(w) / 10) % FRAMES_SURROUND;
	return ph < 0 ? 0 : ph;
}

/* the normal mode (not the wheel, not the sack) */
static void plf_animate_normal(void)
{
	const Box *R = &pl.CollideRect;
	int *bl = &plf_bl, *br = &plf_br, *bo = &plf_bo, *bu = &plf_bu;

	if (pl.Handlung == PA_STEHEN)
	{
		pl.AnimCount += SYNC(FX(1));
		if (pl.AnimCount > FX(1))
		{
			pl.AnimCount -= FX(1);
			pl.AnimPhase++;
			if (pl.AnimPhase >= FRAMES_IDLE * 3 + 44 - 1) pl.AnimPhase = 0;
		}
	}
	/* the wheel's energy comes back slowly, faster when standing */
	if (pl.Armour < 0) pl.Armour = 0;
	pl.Armour += SYNC(FX(0.2));
	if (pl.Handlung == PA_STEHEN || pl.Handlung == PA_DUCKEN || pl.Handlung == PA_SCHIESSEN_O) pl.Armour += SYNC(FX(1));
	if (pl.Armour > MAX_ARMOUR) pl.Armour = MAX_ARMOUR;

	/* against the ceiling */
	if ((*bo & B_WAND) || (tile_zustand() == TS_LOCKED && pl.ypos <= cam_yoff()))
	{
		if (pl.yspeed < 0) pl.yspeed = 0;
		pl.JumpAdd = PLAYER_JUMPADDSPEED;
	}

	/* left / right: walk, or turn the lightning / beam */
	for (int d = -1; d <= 1; d += 2)
	{
		const int key = d < 0 ? AK_LINKS : AK_RECHTS, other = d < 0 ? AK_RECHTS : AK_LINKS;
		const int wall = d < 0 ? *bl : *br;
		if (!AK(key) || AK(other)) continue;
		if (pl.Handlung == PA_BLITZEN)
		{
			if (pl.BlitzStart >= PLAYER_BLITZ_START) pl.BlitzWinkel += d * SYNC(FX(20));
		}
		else if (pl.Handlung == PA_BEAMLADEN) pl.BlitzWinkel += d * SYNC(FX(20));
		else if (!(wall & B_WAND))
		{
			pl.Blickrichtung = d;
			if (pl.Handlung == PA_STEHEN) pl.AnimPhase = 0;
			if (pl.Handlung != PA_SPRINGEN && pl.Handlung != PA_DUCKEN) pl.Handlung = PA_LAUFEN;
			if (pl.Handlung != PA_DUCKEN)
			{
				pl.xspeed = !pl.InLiquid ? d * PLAYER_MOVESPEED : d * PLAYER_MOVESPEED * 2 / 3;
				if (pl.GegnerDran) pl.xspeed /= 2;
			}
			if (pl.Handlung != PA_SPRINGEN && AK(AK_OBEN) && pl.WalkLock) pl.xspeed = 0;
		}
	}

	/* a wall in the way: stand */
	if (((AK(AK_LINKS) && (*bl & B_WAND)) || (AK(AK_RECHTS) && (*br & B_WAND))) && !(*bu & (B_SCHRAEGE_L | B_SCHRAEGE_R)))
	{
		if (pl.Handlung == PA_LAUFEN) { pl.Handlung = PA_STEHEN; pl.AnimPhase = 0; }
		pl.xspeed = 0;
	}
	/* both: stand */
	if (AK(AK_LINKS) && AK(AK_RECHTS))
	{
		if ((pl.Handlung != PA_SPRINGEN || pl.JumpPossible) && pl.Handlung != PA_BLITZEN && pl.Handlung != PA_BEAMLADEN)
		{
			pl.AnimPhase = 0;
			pl.Handlung = PA_STEHEN;
		}
	}
	/* no key: stand */
	if (!AK(AK_LINKS) && !AK(AK_RECHTS) && pl.Handlung != PA_SPRINGEN && pl.Handlung != PA_BLITZEN &&
	    pl.Handlung != PA_PISSEN && pl.Handlung != PA_BEAMLADEN && pl.ShotDelay <= FX(0.5))
		pl.Handlung = PA_STEHEN;

	/* sinking into the swamp */
	{
		const int busumpf = BlockUnten(pl.xpos, &pl.ypos, pl.xposold, &pl.yposold, R, pl.yspeed >= 0);
		if ((busumpf & B_SUMPF) || (*bl & B_SUMPF) || (*br & B_SUMPF))
		{
			if (!AK(AK_JUMP)) pl.JumpPossible = 1;
			if (!(busumpf & B_WAND) && pl.yspeed >= 0) pl.ypos += SYNC(FX(4));
			if (pl.Handlung == PA_SPRINGEN && pl.yspeed >= 0) pl.Handlung = PA_LAUFEN;
		}
	}

	/* jump (only from the ground) */
	if (AK(AK_JUMP) && pl.JumpPossible)
	{
		const int save = pl.AnimPhase;
		pl.JumpPossible = 0;
		pl.AnimPhase = 2;
		pl.Handlung = PA_SPRINGEN;
		pl.JumpStart = pl.ypos;
		pl.yspeed = -PLAYER_MAXJUMPSPEED;
		pl.JumpAdd = 0;
		if (pl.GegnerDran) { pl.yspeed /= 2; pl.JumpAdd = PLAYER_JUMPADDSPEED; }
		if (pl.AufPlattform)
		{
			plf_jumped_off = pl.AufPlattform;   /* (MUSHROOM: its PlayerJumps) */
			pl.AufPlattform = 0;
		}
		if (pl.InLiquid) snd(SOUND_DIVE);
		if (*bu & B_SUMPF)
		{
			pl.ypos -= FX(15);
			*bu = BlockSlopes(pl.xpos, &pl.ypos, R, pl.yspeed);
			if (*bu & B_SUMPF) { pl.JumpPossible = 0; pl.AnimPhase = save; }
		}
	}

	/* landed: a jump is possible again */
	if ((*bu & (B_SUMPF | B_WAND | B_PLATTFORM | B_SCHRAEGE_L | B_SCHRAEGE_R)) || pl.AufPlattform)
	{
		if (!(*bu & (B_SCHRAEGE_L | B_SCHRAEGE_R)))
		{
			if (pl.yspeed > 0)
			{
				if (pl.Handlung != PA_BLITZEN) { pl.Handlung = PA_STEHEN; pl.AnimPhase = 0; }
				pl.yspeed = 0;
				snd(SOUND_LANDEN);
				part_push(pl.xpos + FX(20), pl.ypos + FX(60), SMOKE);
			}
		}
		else
			*bu = BlockSlopes(pl.xpos, &pl.ypos, R, pl.yspeed);
		if (!pl.JumpPossible && !AK(AK_JUMP) && pl.yspeed >= 0) pl.JumpPossible = 1;
	}

	/* nothing under him: he falls */
	if (!pl.AufPlattform && pl.Handlung != PA_SPRINGEN && pl.yspeed >= 0 &&
	    !(*bu & (B_WAND | B_PLATTFORM | B_SUMPF | B_SCHRAEGE_L | B_SCHRAEGE_R)) &&
	    !(*bl & (B_SCHRAEGE_L | B_SCHRAEGE_R)) && !(*br & (B_SCHRAEGE_L | B_SCHRAEGE_R)))
	{
		pl.JumpPossible = 0;
		if (pl.Handlung != PA_BLITZEN) { pl.Handlung = PA_SPRINGEN; pl.AnimPhase = 0; }
		if (pl.Handlung == PA_BLITZEN && pl.yspeed == 0) pl.yspeed = FX(0.5);
		pl.JumpAdd = PLAYER_JUMPADDSPEED;
	}

	/* on the ground, not in a swamp or on a damage tile: remember the place */
	if (pl.Handlung != PA_SPRINGEN && !pl.AufPlattform && !((*bo | *bu | *bl | *br) & (B_SUMPF | B_SCHADEN)))
	{
		pl.JumpxSave = pl.safe_x = pl.xpos;
		pl.JumpySave = pl.safe_y = pl.ypos;
	}

	/* stand up from ducking, if there is room above */
	if (pl.Handlung == PA_DUCKEN)
	{
		fx ypos2 = pl.ypos - FX(48);
		const int bo2 = BlockOben(pl.xpos, &ypos2, pl.xposold, &pl.yposold, R, 1);
		if (!(*bo & B_WAND) && !(bo2 & B_WAND)) pl.Handlung = PA_STEHEN;
	}
	pl.CollideRect.t = 12;
	if (AK(AK_DUCKEN) && pl.JumpPossible && pl.Handlung != PA_BEAMLADEN && pl.Handlung != PA_BLITZEN)
	{
		pl.Handlung = PA_DUCKEN;
		pl.CollideRect.t = 32;
	}

	/* the lightning (only on the ground) */
	if (AK(AK_BLITZ) && pl.JumpPossible)
	{
		if (AK(AK_SHOOT) && pl.Handlung != PA_BEAMLADEN)
		{
			/* load the beam */
			snd(SOUND_BEAMLOAD);
			pl.Handlung = PA_BEAMLADEN;
			pl.BlitzStart = FX(5);
			pl.AnimPhase = 0;
			pl.BlitzWinkel = pl.Blickrichtung < 0 ? FX(270) : FX(90);
			if (AK(AK_OBEN)) { pl.Blickrichtung = -pl.Blickrichtung; pl.BlitzWinkel = 0; }
		}
		else if (pl.Handlung != PA_BLITZEN && pl.Handlung != PA_BEAMLADEN)
		{
			pl.BlitzStart = 0;
			plf_BeamCount = FX(0.1);
			pl.AnimPhase = 0;
			pl.Handlung = PA_BLITZEN;
			plf_changecount = 0;
			snd(SOUND_BLITZSTART);
			plf_blitz_snd = 0;
			pl.BlitzWinkel = pl.Blickrichtung < 0 ? FX(270) : FX(90);
		}
	}
	if (!AK(AK_BLITZ) && pl.Handlung == PA_BLITZEN)
	{
		if (pl.yspeed > 0) pl.Handlung = PA_SPRINGEN;
		else
		{
			if (pl.AnimPhase >= 9 && pl.AnimPhase <= 18) pl.Blickrichtung = -pl.Blickrichtung;
			pl.Handlung = PA_STEHEN;
		}
		pl.AnimPhase = 0;
	}

	/* into the wheel */
	if (pl.JumpPossible && AK(AK_POWERLINE) && AK(AK_DUCKEN) && pl.Armour > FX(5))
	{
		pl.Handlung = PA_RADELN;
		pl.AnimPhase = 0;
		pl.AnimCount = 0;
		plf_rect(20, 40, 50, 79);
		pl.PowerLinePossible = 0;
	}

	/* the lightning's loop sound */
	if (pl.Handlung == PA_BLITZEN)
	{
		if (pl.BlitzStart >= PLAYER_BLITZ_START && --plf_blitz_snd <= 0) { snd(SOUND_BLITZ); plf_blitz_snd = 60; }
	}
	else if (plf_blitz_snd > 0) { plf_blitz_snd = 0; snd(SOUND_BLITZENDE); }

	/* turn the player with the lightning */
	if (pl.Handlung == PA_BLITZEN)
	{
		if (pl.BlitzStart < PLAYER_BLITZ_START) pl.BlitzStart += SYNC(FX(1));
		else
		{
			pl.BlitzWinkel = plf_clamp_angle(pl.BlitzWinkel);
			pl.AnimPhase = plf_lightning_phase();
		}
	}

	/* load the beam, fire it */
	if (pl.Handlung == PA_BEAMLADEN)
	{
		if (pl.BlitzStart < PLAYER_BEAM_MAX) pl.BlitzStart += SYNC(FX(pl.CurrentWeaponLevel[3]));
		pl.BlitzWinkel = plf_clamp_angle(pl.BlitzWinkel);
		pl.AnimPhase = plf_lightning_phase();
		if (!AK(AK_BLITZ) || !AK(AK_SHOOT))
		{
			pl.Handlung = PA_STEHEN;
			if (pl.BlitzStart >= FX(20))
			{
				proj_blitzbeam(FINT(pl.BlitzStart), pl.BlitzWinkel);
				snd(SOUND_BLITZENDE);
				snd(SOUND_EXPLOSION3);
			}
			if (pl.AnimPhase >= 12 && pl.AnimPhase <= 24) pl.Blickrichtung = -pl.Blickrichtung;
		}
	}

	/* Bronson */
	if (pl.Handlung == PA_PISSEN)
	{
		pl.AnimCount += SYNC(FX(1));
		if (pl.AnimCount > FX(1))
		{
			pl.AnimCount = 0;
			pl.AnimPhase++;
			if (pl.AnimPhase > 2) pl.AnimCount = FX(-1);
			if (pl.AnimPhase == 11) pl.AnimCount = FX(-2);
		}
		if (pl.AnimPhase == 16) pl.AnimPhase = 3;
	}
}

/* the wheel */
static void plf_animate_wheel(void)
{
	const Box *R = &pl.CollideRect;
	const int bl = plf_bl, br = plf_br, bo = plf_bo, bu = plf_bu;
	if (!pl.WheelMode) pl.Armour -= SYNC(FX(2.5));
	if (pl.Armour < 0) pl.Armour = 0;
	if (AK(AK_POWERLINE) && pl.PowerLinePossible && pl.PowerLines > 0) plf_powerline();
	if (AK(AK_SMARTBOMB) && pl.PowerLinePossible && pl.SmartBombs > 0)
	{
		snd(SOUND_POWERLINE);
		pl.PowerLinePossible = 0;
		pl.SmartBombs--;
		proj_push(pl.xpos + FX(40 - 32), pl.ypos + FX(55 - 32), SMARTBOMB);
	}
	if (!AK(AK_POWERLINE) && !AK(AK_SMARTBOMB)) pl.PowerLinePossible = 1;

	/* roll on */
	if (pl.Blickrichtung < 0 && !(bl & B_WAND)) pl.xspeed = pl.InLiquid ? FX(-30) : FX(-40);
	else if (pl.Blickrichtung > 0 && !(br & B_WAND)) pl.xspeed = pl.InLiquid ? FX(30) : FX(40);
	const int ground = pl.Handlung == PA_RADELN || pl.AufPlattform || (bu & (B_WAND | B_PLATTFORM));
	if (AK(AK_LINKS) && ground) pl.Blickrichtung = -1;
	if (AK(AK_RECHTS) && ground) pl.Blickrichtung = 1;

	if (pl.Handlung != PA_RADELN_FALL && !(bu & (B_SCHRAEGE_R | B_SCHRAEGE_L | B_WAND | B_PLATTFORM)))
	{
		pl.Handlung = PA_RADELN_FALL;
		pl.yspeed = FX(0.5);
		pl.JumpAdd = PLAYER_JUMPADDSPEED;
	}
	/* down again: bounce */
	if (pl.Handlung == PA_RADELN_FALL && (pl.AufPlattform || (bu & (B_WAND | B_PLATTFORM))))
	{
		if (pl.yspeed > FX(2)) part_push(pl.xpos + FX(20), pl.ypos + FX(60), SMOKE);
		if (pl.yspeed > 0)
		{
			pl.yspeed = -pl.yspeed * 2 / 3;
			if (pl.yspeed > FX(-5))
			{
				pl.yspeed = 0;
				pl.JumpAdd = 0;
				pl.Handlung = PA_RADELN;
				BlockUnten(pl.xpos, &pl.ypos, pl.xposold, &pl.yposold, R, 1);
			}
			else
			{
				pl.AufPlattform = 0;
				snd(SOUND_LANDEN);
			}
		}
	}
	/* turn at walls */
	if (!(bu & (B_SCHRAEGE_R | B_SCHRAEGE_L)))
	{
		if (pl.Blickrichtung > 0 && (br & B_WAND)) pl.Blickrichtung = -1;
		if (pl.Blickrichtung < 0 && (bl & B_WAND)) pl.Blickrichtung = 1;
	}
	/* back to the player (jump, or no energy left), if there is room above */
	fx ypos2 = pl.ypos - FX(20);
	const int bo2 = BlockOben(pl.xpos, &ypos2, pl.xposold, &pl.yposold, R, 1);
	if ((pl.Armour <= 0 || AK(AK_JUMP)) && !(bo & B_WAND) && !(bo2 & B_WAND))
	{
		pl.AnimPhase = 0;
		if (pl.Armour <= 0 && (bu & (B_WAND | B_PLATTFORM))) pl.Handlung = PA_STEHEN;
		else pl.Handlung = PA_SPRINGEN;
		plf_rect(20, 12, 50, 79);
		if (pl.Armour < 0) pl.Armour = 0;
	}
}

/* riding the flying sack */
static void plf_animate_ride(void)
{
	const int bl = plf_bl, br = plf_br, bo = plf_bo, bu = plf_bu;
	const int beide_frei = pl.FesteAktion <= -1;
	if (!plf_FlugsackFliesFree && beide_frei) pl.ypos -= SYNC(PLAYER_FLUGSACKSPEED);
	pl.JumpySave = pl.safe_y = pl.ypos;
	pl.JumpxSave = pl.safe_x = pl.xpos;
	/* out at the bottom */
	if (pl.ypos > plf_yoff() + FX(475))
	{
		snd(SOUND_EXPLOSION2);
		pl.xposold = pl.xpos;
		pl.yposold = pl.ypos;
		pl.Energy = 0;
	}
	/* the ceiling */
	if (pl.ypos < plf_yoff() && beide_frei && plf_yadd < 0) plf_yadd = -plf_yadd;
	if ((bl & B_WAND) && plf_xadd < 0) plf_xadd = -plf_xadd / 2;
	if ((br & B_WAND) && plf_xadd > 0) plf_xadd = -plf_xadd / 2;
	if ((bo & B_WAND) && plf_yadd < 0) plf_yadd = -plf_yadd / 2;
	if ((bu & B_WAND) && plf_yadd > 0) plf_yadd = -plf_yadd / 2;

	/* the sack's smoke */
	plf_SmokeCount -= SYNC(FX(1));
	if (plf_SmokeCount < 0)
	{
		plf_SmokeCount += FX(0.1);
		if (pl.Handlung == PA_SACKREITEN)
		{
			if (pl.Blickrichtung < 0) part_push(pl.xpos + FX(72), pl.ypos + FX(100), FLUGSACKSMOKE2);
			else part_push(pl.xpos - FX(2), pl.ypos + FX(100), FLUGSACKSMOKE);
		}
		else
		{
			if (pl.Blickrichtung < 0)
			{
				part_push(pl.xpos + FX(70 - pl.AnimPhase * 10), pl.ypos + FX(100), FLUGSACKSMOKE2);
				part_push(pl.xpos + FX(87 - pl.AnimPhase * 10), pl.ypos + FX(100), FLUGSACKSMOKE);
			}
			else
			{
				part_push(pl.xpos + FX(4 + (pl.AnimPhase - 10) * 10), pl.ypos + FX(100), FLUGSACKSMOKE);
				part_push(pl.xpos + FX(-20 + (pl.AnimPhase - 10) * 10), pl.ypos + FX(100), FLUGSACKSMOKE2);
			}
		}
	}

	/* turning round */
	if (pl.Handlung == PA_DREHEN)
	{
		pl.AnimCount += SYNC(FX(1.2));
		if (pl.AnimCount > FX(0.6))
		{
			pl.AnimCount -= FX(0.6);
			pl.AnimPhase++;
			if (pl.AnimPhase == pl.AnimEnde)
			{
				pl.AnimEnde = 0;
				pl.Handlung = PA_SACKREITEN;
				if (pl.Blickrichtung > 0) { pl.AnimPhase = 0; pl.Blickrichtung = -1; }
				else { pl.AnimPhase = 10; pl.Blickrichtung = 1; }
			}
		}
	}
	if (AK(AK_LINKS) && !AK(AK_RECHTS))
	{
		plf_xadd -= SYNC(FX(10));
		if (!AK(AK_SHOOT) && pl.Handlung == PA_SACKREITEN && pl.Blickrichtung > 0)
		{
			pl.Handlung = PA_DREHEN;
			pl.AnimEnde = 19;
			pl.AnimCount = 0;
			pl.AnimPhase = 10;
		}
	}
	if (AK(AK_RECHTS) && !AK(AK_LINKS))
	{
		plf_xadd += SYNC(FX(10));
		if (!AK(AK_SHOOT) && pl.Handlung == PA_SACKREITEN && pl.Blickrichtung < 0)
		{
			pl.Handlung = PA_DREHEN;
			pl.AnimEnde = 9;
			pl.AnimCount = 0;
			pl.AnimPhase = 0;
		}
	}
	if ((AK(AK_OBEN) || AK(AK_JUMP)) && !AK(AK_DUCKEN)) plf_yadd -= SYNC(FX(10));
	if (AK(AK_DUCKEN) && !AK(AK_OBEN)) plf_yadd += SYNC(FX(10));
	/* slowing down */
	if (!AK(AK_LINKS) && !AK(AK_RECHTS))
	{
		if (plf_xadd < 0) { plf_xadd += SYNC(FX(4)); if (plf_xadd > 0) plf_xadd = 0; }
		if (plf_xadd > 0) { plf_xadd -= SYNC(FX(4)); if (plf_xadd < 0) plf_xadd = 0; }
	}
	if (!AK(AK_OBEN) && !AK(AK_DUCKEN))
	{
		if (plf_yadd < 0) { plf_yadd += SYNC(FX(4)); if (plf_yadd > 0) plf_yadd = 0; }
		if (plf_yadd > 0) { plf_yadd -= SYNC(FX(4)); if (plf_yadd < 0) plf_yadd = 0; }
	}
	if (plf_xadd < FX(-25)) plf_xadd = FX(-25);
	if (plf_xadd > FX(25)) plf_xadd = FX(25);
	if (plf_yadd < FX(-25)) plf_yadd = FX(-25);
	if (plf_yadd > FX(25)) plf_yadd = FX(25);
	pl.xspeed = plf_xadd;
	pl.ypos += SYNC(plf_yadd);

	/* the turning point: jump off, the sack falls */
	fx ytemp = pl.ypos - FX(1);
	plf_bu = BlockUntenNormal(pl.xpos, pl.ypos, pl.xposold, ytemp, &pl.CollideRect);
	if (plf_bu & B_WENDEPUNKT)
	{
		pl.Handlung = PA_SPRINGEN;
		pl.AnimPhase = 0;
		pl.yspeed = -PLAYER_MAXJUMPSPEED;
		pl.JumpAdd = PLAYER_JUMPADDSPEED;
		pl.JumpStart = pl.ypos;
		pl.AufPlattform = 0;
		plf_rect(20, 12, 50, 79);
		gegner_push(pl.xpos, pl.ypos + FX(20), FLUGSACK, 99, 0);
		plf_FlugsackFliesFree = 1;
		music_stage();
	}
}

static void plf_animate(void)
{
	if (pl.Handlung == PA_EXPLODIEREN || pl.Handlung == PA_TOT) return;
	const Box *R = &pl.CollideRect;
	const int res = pl.yspeed >= 0;

	plf_bu = 0;
	plf_bl = BlockLinks(&pl.xpos, pl.ypos, &pl.xposold, pl.yposold, R, res);
	plf_br = BlockRechts(&pl.xpos, pl.ypos, &pl.xposold, pl.yposold, R, res);
	if (pl.yspeed >= 0) plf_bu = BlockSlopes(pl.xpos, &pl.ypos, R, pl.yspeed);
	if (pl.Handlung == PA_RADELN &&
	    ((pl.Blickrichtung < 0 && (plf_bu & B_SCHRAEGE_R)) || (pl.Blickrichtung > 0 && (plf_bu & B_SCHRAEGE_L))))
	{
		plf_bu = BlockUnten(pl.xpos, &pl.ypos, pl.xposold, &pl.yposold, R, pl.yspeed >= 0);
		pl.yspeed = FX(-2);
		pl.ypos -= FX(1);
	}
	if (!(plf_bu & (B_SCHRAEGE_L | B_SCHRAEGE_R)))
	{
		BlockUnten(pl.xpos, &pl.ypos, pl.xposold, &pl.yposold, R, pl.yspeed >= 0);
		plf_bu = BlockUntenNormal(pl.xpos, pl.ypos, pl.xposold, pl.yposold, R);
	}
	else if (pl.Handlung == PA_SPRINGEN)
	{
		pl.Handlung = PA_STEHEN;
		pl.yspeed = 0;
		pl.JumpAdd = 0;
	}
	plf_bo = BlockOben(pl.xpos, &pl.ypos, pl.xposold, &pl.yposold, R, 1);

	/* under a waterfall: drops */
	{
		const int ty = (FINT(pl.ypos) + R->t + 10) / TILESIZE;
		if (FINT(pl.ypos) + R->t + 10 >= 0 && ty < map_h)
			for (int i = FINT(pl.xpos) + R->l + 5; i < FINT(pl.xpos) + R->r - 5; i += 2)
			{
				if (i < 0) continue;
				if (i / TILESIZE >= map_w) break;
				if (TileBlock(i / TILESIZE, ty) & B_WASSERFALL)
				{
					const fx tx = FX(i), tyy = pl.ypos + FX(R->t + 10);
					if (GetRandom(50) == 0) part_push(tx + FX(GetRandom(4)), tyy + FX(GetRandom(4)), WASSERTROPFEN);
					if (GetRandom(200) == 0) part_push(tx + FX(GetRandom(4)), tyy + FX(GetRandom(4)), WATERFUNKE);
					if (GetRandom(200) == 0) part_push(tx + FX(GetRandom(4) - 16), tyy + FX(GetRandom(4) - 16), WATERFLUSH);
				}
			}
	}

	pl.xposold = pl.xpos;
	pl.yposold = pl.ypos;

	if (plf_wheel()) plf_animate_wheel();
	else if (plf_riding()) plf_animate_ride();
	else if (pl.Handlung != PA_SURFEN && pl.Handlung != PA_SURFENCROUCH && pl.Handlung != PA_SURFENJUMP)
		plf_animate_normal();

	/* power line, smart bomb */
	if (AK(AK_POWERLINE) && !AK(AK_DUCKEN) && pl.PowerLinePossible && pl.PowerLines > 0) plf_powerline();
	if (AK(AK_SMARTBOMB) && pl.PowerLinePossible && pl.SmartBombs > 0)
	{
		snd(SOUND_POWERLINE);
		pl.PowerLinePossible = 0;
		pl.SmartBombs--;
		proj_push(pl.xpos + FX(40 - 32), pl.ypos + FX(35 - 32), SMARTBOMB);
	}
	if (!AK(AK_POWERLINE) && !AK(AK_GRANATE) && !AK(AK_SMARTBOMB)) pl.PowerLinePossible = 1;

	/* aim up */
	if (AK(AK_OBEN))
	{
		const int h = pl.Handlung;
		if (h == PA_STEHEN || h == PA_SCHIESSEN_O || h == PA_SCHIESSEN_LO || h == PA_SCHIESSEN_RO || h == PA_LAUFEN)
		{
			if (AK(AK_LINKS)) { pl.Handlung = PA_SCHIESSEN_LO; pl.Blickrichtung = -1; }
			else if (AK(AK_RECHTS)) { pl.Handlung = PA_SCHIESSEN_RO; pl.Blickrichtung = 1; }
			else pl.Handlung = PA_SCHIESSEN_O;
		}
	}

	/* the shield */
	if (pl.Shield > 0) pl.Shield -= SYNC(FX(0.4));

	/* damage tiles */
	if ((plf_bo | plf_bu | plf_bl | plf_br) & B_SCHADEN)
	{
		DamagePlayer(SYNC(FX(20)), 0);
		if (plf_wheel() && !pl.WheelMode) pl.Armour -= SYNC(FX(5));
	}

	/* in a liquid? */
	{
		const int tx = (FINT(pl.xpos) + 35) / TILESIZE, ty = (FINT(pl.ypos) + 40) / TILESIZE;
		const int middle = FINT(pl.xpos) + 35 >= 0 && FINT(pl.ypos) + 40 >= 0 ? TileBlock(tx, ty) : 0;
		if (((plf_bu | plf_br | plf_bl | plf_bo) & B_LIQUID) || (middle & B_LIQUID))
		{
			if (!pl.InLiquid)
			{
				if (pl.Handlung == PA_SPRINGEN || pl.Handlung == PA_RADELN_FALL)
				{
					for (int i = 0; i < 12; i++)
						part_push(pl.xpos + FX(18 + GetRandom(20)), pl.ypos + FX(R->b - 25), WASSER_SPRITZER2);
					proj_winkel = FX(-1);
					for (int i = 0; i < 15; i++)
						part_push(pl.xpos + FX(10 + GetRandom(40)), pl.ypos + FX(R->b + GetRandom(20)), BUBBLE);
				}
				pl.InLiquid = 1;
				snd(SOUND_WATERIN);
			}
		}
		else if (pl.InLiquid)
		{
			if (pl.Handlung == PA_SPRINGEN || pl.Handlung == PA_RADELN_FALL)
				for (int i = 0; i < 12; i++)
					part_push(pl.xpos + FX(10 + GetRandom(20)), pl.ypos + FX(R->b - 25), WASSER_SPRITZER2);
			snd(SOUND_WATEROUT);
			pl.InLiquid = 0;
		}
		if (pl.InLiquid && GetRandom(500) == 0)
		{
			part_push(pl.xpos + FX(30), pl.ypos + FX(20), BUBBLE);
			snd(SOUND_DIVE);
		}
	}

	/* walking diagonally (WalkLock is always set: never) */
	if ((pl.Handlung == PA_SCHIESSEN_LO || pl.Handlung == PA_SCHIESSEN_RO) && !pl.WalkLock)
	{
		pl.AnimCount += SYNC(FX(1));
		while (pl.AnimCount > PLAYER_ANIMSPEED)
		{
			pl.AnimCount -= PLAYER_ANIMSPEED;
			if (++pl.AnimPhase >= FRAMES_RUN) pl.AnimPhase = 0;
		}
	}

	/* run / jump / wheel animation */
	if (pl.Handlung == PA_LAUFEN || plf_wheel() || (pl.Handlung == PA_SPRINGEN && pl.yspeed > FDIV(-PLAYER_MAXJUMPSPEED, FX(1.5))))
	{
		pl.AnimCount += pl.InLiquid ? SYNC(FX(0.5)) : SYNC(FX(1));
		if (plf_wheel()) pl.AnimCount += SYNC(FX(4));
		while (pl.AnimCount > PLAYER_ANIMSPEED)
		{
			pl.AnimCount -= PLAYER_ANIMSPEED;
			switch (pl.Handlung)
			{
			case PA_LAUFEN: if (++pl.AnimPhase >= FRAMES_RUN) pl.AnimPhase = 0; break;
			case PA_SPRINGEN: if (pl.AnimPhase < FRAMES_JUMP - 1) pl.AnimPhase++; break;
			case PA_RADELN:
			case PA_RADELN_FALL: if (pl.AnimPhase < PLAYERANIM_RAD_ENDE) pl.AnimPhase++; else pl.AnimPhase = 0; break;
			default: break;
			}
		}
	}

	/* jumping / falling */
	if (pl.Handlung == PA_SPRINGEN || (pl.Handlung == PA_BLITZEN && pl.yspeed > 0))
	{
		pl.yspeed += SYNC(!pl.InLiquid ? pl.JumpAdd : pl.JumpAdd * 2 / 3);
		pl.ypos += SYNC(!pl.InLiquid ? pl.yspeed : pl.yspeed * 2 / 3);
		if (pl.yspeed > PLAYER_MAXJUMPSPEED) pl.yspeed = PLAYER_MAXJUMPSPEED;
		if (!pl.InLiquid)
		{
			if (pl.ypos < pl.JumpStart - PLAYER_MAXJUMP || !AK(AK_JUMP))
			{
				/* the higher the jump, the slower the fall */
				const int t = FINT((FX(160) - (pl.JumpySave - pl.ypos)) / 10);
				pl.JumpAdd = PLAYER_JUMPADDSPEED + FX(abs(t));
				if (pl.JumpAdd > FX(18)) pl.JumpAdd = FX(18);
			}
		}
		else if (pl.ypos < pl.JumpStart - (PLAYER_MAXJUMP + FX(50)) || !AK(AK_JUMP))
			pl.JumpAdd = PLAYER_JUMPADDSPEED;
	}

	/* the wheel falling */
	if (pl.Handlung == PA_RADELN_FALL)
	{
		pl.yspeed += SYNC(!pl.InLiquid ? pl.JumpAdd : pl.JumpAdd * 2 / 3);
		pl.ypos += SYNC(!pl.InLiquid ? pl.yspeed : pl.yspeed * 2 / 3);
		if (pl.yspeed > PLAYER_MAXJUMPSPEED) pl.yspeed = PLAYER_MAXJUMPSPEED;
		if (plf_bo & B_WAND)
		{
			if (pl.yspeed < 0) pl.yspeed = -pl.yspeed / 3;
			pl.JumpAdd = PLAYER_JUMPADDSPEED;
		}
	}

	/* move, if no wall is in the way */
	if (pl.xspeed != 0)
	{
		if ((pl.xspeed < 0 && !(plf_bl & B_WAND)) || (pl.xspeed > 0 && !(plf_br & B_WAND)))
			pl.xpos += SYNC(pl.xspeed);
	}

	switch (pl.Handlung)
	{
	case PA_RADELN: case PA_RADELN_FALL: if (pl.AnimPhase >= 8) pl.AnimPhase = 0; break;
	case PA_LAUFEN: if (pl.AnimPhase >= FRAMES_RUN) pl.AnimPhase = 0; break;
	case PA_BLITZEN: case PA_BEAMLADEN: if (pl.AnimPhase >= 36) pl.AnimPhase = 0; break;
	case PA_DREHEN: if (pl.AnimPhase >= 20) pl.AnimPhase = 0; break;
	case PA_SPRINGEN: if (pl.AnimPhase >= 12) pl.AnimPhase = 0; break;
	case PA_PISSEN: if (pl.AnimPhase >= 18) pl.AnimPhase = 0; break;
	default: break;
	}
}

/* ------------------------------------------------------------------ MovePlayer */
static void plf_move(void)
{
	if (pl.DoFesteAktion || pl.Handlung == PA_TOT) return;
	if (pl.xpos < 0) pl.xpos = 0;
	if (pl.xpos > FX(map_w * TILESIZE - 80)) pl.xpos = FX(map_w * TILESIZE - 80);
	/* out at the bottom of the level */
	if (FINT(pl.ypos) > map_h * TILESIZE) pl.Energy = 0;
	if (pl.ypos < 0)
	{
		if (pl.yspeed < 0) pl.yspeed = -pl.yspeed * 2 / 3;
		else pl.ypos = 0;
	}
	/* a screen that does not scroll holds the player */
	if (tile_zustand() != TS_SCROLLBAR)
	{
		if (pl.xpos < cam_xoff()) pl.xpos = cam_xoff();
		if (pl.xpos > cam_xoff() + FX(580)) pl.xpos = cam_xoff() + FX(580);
		if (g_Fahrstuhl_yPos > 0)
		{
			/* the lift level: off the screen is death */
			if (pl.ypos + FX(pl.CollideRect.b) < cam_yoff() || pl.ypos > cam_yoff() + FX(RENDERHEIGHT)) pl.Energy = 0;
		}
		else if (pl.ypos < cam_yoff()) pl.ypos = cam_yoff();
	}
}

/* DoPlattformStuff */
static void plf_plattform_stuff(void)
{
	Ent *e = pl.AufPlattform;
	if (!e) return;
	if (e->GegnerArt < 0) { pl.AufPlattform = 0; return; }
	if (e->GegnerArt == REITFLUGSACK) return;
	if (pl.Handlung == PA_SPRINGEN) { pl.Handlung = PA_STEHEN; pl.yspeed = 0; }
	if (!AK(AK_JUMP)) pl.JumpPossible = 1;
	if (plf_bu & (B_WAND | B_PLATTFORM)) pl.AufPlattform = 0;
	else if (e->GegnerArt == FAHRSTUHL)
		pl.ypos = g_Fahrstuhl_yPos + FX(GegnerRect[FAHRSTUHL].t) - FX(pl.CollideRect.b);
	else
		pl.ypos = e->yPos + FX(GegnerRect[e->GegnerArt].t) - FX(pl.CollideRect.b);
}

/* ------------------------------------------------------------------ the lightning (DoLightning's
 * logic part: its damage, walls; computed each step, drawn in player_draw) */
static void plf_lightning_step(void)
{
	static const Box R32 = { 0, 0, 31, 31 };
	plf_blitz_vis = 0;
	if (pl.Handlung != PA_BLITZEN && pl.Handlung != PA_BEAMLADEN) return;
	pl.BlitzWinkel = plf_clamp_angle(pl.BlitzWinkel);
	const int w = FINT(pl.BlitzWinkel) - 90;
	const fx c = cos_d(w), s = sin_d(w);
	pl.BlitzCount += SPEEDFACTOR;
	if (pl.BlitzCount > PLAYER_BLITZ_SPEED)
	{
		pl.BlitzCount = 0;
		if (++pl.BlitzAnim >= 4) pl.BlitzAnim = 0;
	}

	if (pl.Handlung == PA_BEAMLADEN)
	{
		/* LoadBeam: the glow at the gun, the loading particles */
		const fx xstart = pl.xpos + FX(20) + 28 * c, ystart = pl.ypos + FX(21) + 28 * s;
		plf_fx0 = xstart - FX(18);
		plf_fy0 = ystart - FX(18);
		plf_blitz_vis = 1;
		plf_BeamX = xstart + FX(12);
		plf_BeamY = ystart + FX(12);
		if (pl.BlitzStart < PLAYER_BEAM_MAX)
		{
			plf_BeamCount -= SYNC(FX(1));
			if (plf_BeamCount < 0)
			{
				plf_BeamCount = FX(0.1);
				const int j = GetRandom(360);
				part_push(plf_BeamX + sin_d(j) * 50, plf_BeamY + cos_d(j) * 50, BEAMSMOKE2);
			}
		}
		return;
	}

	int DrawLength = pl.BlitzLength - 1;
	fx xstart = pl.xpos + FX(20) + 36 * c, ystart = pl.ypos + FX(21) + 36 * s;
	plf_fx0 = xstart - FX(18);
	plf_fy0 = ystart - FX(18);
	plf_blitz_vis = 1;
	if (pl.BlitzStart < PLAYER_BLITZ_START) return;      /* only the glow while it starts */
	plf_blitz_vis = 2;
	xstart -= 20 * c;
	ystart -= 20 * s;
	const int spark = pl.BlitzCount == 0 && pl.BlitzAnim % 2 == 0;
	for (int i = 0; i < pl.BlitzLength + 1; i++)
	{
		const fx xs = xstart, ys = ystart;
		int stop = 0;
		for (int k = 0; k < MAX_LIVE && !stop; k++)
		{
			Ent *e = &ents[k];
			if (e->GegnerArt < 0 || !e->Active || !e->Destroyable) continue;
			const Box *r = &GegnerRect[e->GegnerArt];
			if (xstart + FX(31) > e->xPos && xstart < e->xPos + FX(r->r) && ystart + FX(31) > e->yPos && ystart < e->yPos + FX(r->b))
			{
				if (spark) part_push(xs + FX(12), ys + FX(12), LASERFUNKE);
				e->DamageTaken = FX(255);
				e->Energy -= SYNC(BLITZ_POWER);
				snd(SOUND_HIT + e->HitSound);
				if (e->GegnerArt == SHOOTBUTTON)
				{
					/* the button of a platform stops the lightning */
					if (spark) part_push(xs + FX(12), ys + FX(12), LASERFUNKE);
					DrawLength = i - 1;
					stop = 1;
				}
			}
		}
		/* destroyable walls */
		{
			const int x0 = FINT(xs) - 1, y0 = FINT(ys) - 1, x1 = FINT(xs) + 32, y1 = FINT(ys) + 32;
			if (x1 >= 0 && y1 >= 0)
				for (int ty = (y0 < 0 ? 0 : y0) / TILESIZE; ty <= y1 / TILESIZE; ty++)
					for (int tx = (x0 < 0 ? 0 : x0) / TILESIZE; tx <= x1 / TILESIZE; tx++)
						if (TileBlock(tx, ty) & B_DESTRUCTIBLE) ExplodeWall(tx, ty);
		}
		/* bubbles in water, end at a wall */
		{
			fx a = xs, b = xs, y = ys, yo = ys;
			const int wl = BlockLinks(&a, ys, &b, ys, &R32, 0);
			a = b = xs;
			const int wr = BlockRechts(&a, ys, &b, ys, &R32, 0);
			const int wo = BlockOben(xs, &y, xs, &yo, &R32, 0);
			y = yo = ys;
			const int wu = BlockUnten(xs, &y, xs, &yo, &R32, 0);
			if ((wl & wr & wo & wu & B_LIQUID) && GetRandom(80) == 0)
				part_push(xs + FX(GetRandom(32)), ys + FX(GetRandom(32)), BUBBLE);
			if ((wl | wr | wo | wu) & B_WAND)
			{
				if (spark) part_push(xs + FX(12), ys + FX(12), LASERFUNKE);
				xstart += 32 * c;
				ystart += 32 * s;
				DrawLength = i - 1;
				break;
			}
		}
		xstart += 32 * c;
		ystart += 32 * s;
		if (stop) break;
	}
	xstart -= 16 * c;
	ystart -= 16 * s;
	plf_fx1 = xstart - FX(18);
	plf_fy1 = ystart - FX(18);
	if (DrawLength < 0) DrawLength = 0;
	plf_drawlen = DrawLength;

	/* new strands now and then (DrawCoolLightning's changecount) */
	plf_changecount -= SYNC(FX(1));
	if (plf_changecount <= 0)
	{
		plf_changecount = FX(GetRandom(10) + 1) / 10;
		for (int n = 0; n < 3; n++)
			for (int i = 0; i < 16; i++) plf_jag[n][i] = (int8_t)(GetRandom(32) - 16);
	}
}

/* ------------------------------------------------------------------ one step */
static void player_step(void)
{
	if (pl.Handlung == PA_TOT) { pl.dead_t++; return; }
	if (plf_exited) return;
	pl.BlitzLength = pl.CurrentWeaponLevel[3] + 1;      /* HUD.DoHUD */
	plf_jumped_off = 0;

	/* the sack mounted (the REITFLUGSACK sets SACKREITEN): Hurrican's FlugsackFliesFree */
	{
		static int was_riding;
		const int riding = plf_riding();
		if (riding && !was_riding)
		{
			Ent *e = pl.AufPlattform;
			plf_FlugsackFliesFree = !(e && e->GegnerArt == REITFLUGSACK && e->Value1 == 1);
			plf_ryoff = cam_yoff();
			plf_xadd = plf_yadd = 0;
		}
		was_riding = riding;
	}

	plf_check_for_explode();                            /* (Gameplay: after the frame) */
	if (pl.Handlung != PA_TOT)
	{
		plf_get_input();
		plf_animate();
		plf_move();
	}
	if (plf_exited || pl.Handlung == PA_TOT) return;
	pl.GegnerDran = 0;
	/* ScrollFlugsack */
	if (plf_riding() && !plf_FlugsackFliesFree && pl.FesteAktion <= -1) plf_ryoff -= SYNC(PLAYER_FLUGSACKSPEED);
	if (pl.AufPlattform) plf_plattform_stuff();
	plf_lightning_step();

	/* DrawPlayer's timers */
	if (pl.DamageCounter > 0) pl.DamageCounter -= SYNC(FX(5));
	else pl.DamageCounter = 0;
	if (pl.DamageCounter < 0) pl.DamageCounter = 0;
	if (pl.FlameTime > 0) pl.FlameTime -= SPEEDFACTOR;
	if (pl.BlinkCounter > 0) pl.BlinkCounter -= SYNC(FX(0.5));
	if (pl.Shield > 0) plf_shield_ang += SYNC(FX(0.5 * 57.29578)) * -pl.Blickrichtung;
	plf_shield_ang = plf_clamp_angle(plf_shield_ang);
	if (pl.Handlung != PA_EXPLODIEREN) plf_damaged_stuff();
	if (pl.safe_t > 0) pl.safe_t--;
}

/* ------------------------------------------------------------------ drawing */
static uint32_t plf_t;                           /* frames drawn */
static void plf_draw_lightning(void)
{
	const Pic *f = &PP_flash[pl.BlitzAnim & 3];
	if (pl.Handlung == PA_BEAMLADEN)
	{
		/* the glow grows while the beam loads */
		if (pl.BlitzStart >= PLAYER_BEAM_MAX / 2 || (plf_t & 1)) draw_sprite(f, 0, plf_fx0, plf_fy0, 0);
		return;
	}
	if (plf_blitz_vis == 1)
	{
		if (pl.BlitzStart >= PLAYER_BLITZ_START / 2 || (plf_t & 1)) draw_sprite(f, 0, plf_fx0, plf_fy0, 0);
		return;
	}
	/* the strands from the start glow to the end glow (DrawCoolLightning, as lines) */
	const fx x0 = plf_fx0 + FX(33), y0 = plf_fy0 + FX(32), x1 = plf_fx1 + FX(33), y1 = plf_fy1 + FX(32);
	int segs = plf_drawlen + 2;
	if (segs > 15) segs = 15;
	const int w = FINT(pl.BlitzWinkel);
	const fx px = cos_d(w), py = sin_d(w);           /* across the lightning */
	static const int *cols[3] = { &col_light, &col_blue, &col_light };
	for (int n = 0; n < 3; n++)
	{
		fx lx = x0, ly = y0;
		for (int i = 1; i <= segs; i++)
		{
			fx nx = x0 + (x1 - x0) * i / segs, ny = y0 + (y1 - y0) * i / segs;
			if (i < segs) { nx += px * plf_jag[n][i]; ny += py * plf_jag[n][i]; }
			draw_line(lx, ly, nx, ny, *cols[n]);
			lx = nx; ly = ny;
		}
	}
	draw_sprite(f, 0, plf_fx0, plf_fy0, 0);
	draw_sprite(&PP_flash[3 - (pl.BlitzAnim & 3)], 0, plf_fx1, plf_fy1, 0);
}

#if PLF_DRAW_SHIELD
static void plf_draw_shield(void)
{
	if (pl.Shield <= 0) return;
	const int wheel = plf_wheel();
	const fx cx = pl.xpos + FX(34), cy = pl.ypos + (wheel ? FX(61) : FX(44));
	const int rad = wheel ? 30 : 40;
	const int a = FINT(plf_shield_ang), back = pl.Blickrichtung < 0 ? -1 : 1;
	for (int k = 0; k < 2; k++)
	{
		const int a0 = a + k * 180;
		/* the glow left behind (SHIELD particles), then the spawner */
		for (int j = 2; j >= 1; j--)
		{
			const int aj = a0 + back * j * 55;
			draw_sprite(&PP_shieldglow, 0, cx + sin_d(aj) * rad - FX(24), cy + cos_d(aj) * rad - FX(24), 0);
		}
		draw_sprite(&PP_spawner, (FINT(pl.Shield) + k * 5) % 10, cx + sin_d(a0) * rad - FX(6), cy + cos_d(a0) * rad - FX(6), 0);
	}
}
#endif

static void player_draw(void)
{
	const uint32_t t = ++plf_t;
	if (pl.Handlung == PA_TOT || pl.Handlung == PA_EXPLODIEREN || plf_exited) { plf_WasDamaged = 0; return; }
	const fx x = FX(FINT(pl.xpos)), y = pl.ypos;
	const int left = pl.Blickrichtung < 0;
	const int shoot = pl.ShotDelay > FX(0.25);
	if (!(pl.safe_t > 0 && (t & 1)))
	{
		/* hit this frame: bright (DrawPlayer(true ..)); a power-up: blink (BlinkCounter) */
		draw_flash = plf_WasDamaged || (pl.BlinkCounter > 0 && pl.BlinkCounter < FX(4) && (t & 2));
		switch (pl.Handlung)
		{
		case PA_RADELN:
		case PA_RADELN_FALL:
			draw_sprite(&PP_rad, pl.AnimPhase, x + FX(17), y + FX(45), !left);
			break;
		case PA_STEHEN:
			if (AK(AK_UNTEN) && !AK(AK_SHOOT) && !AK(AK_GRANATE)) draw_sprite(&PP_kucken, 1, x, y, left);
			else if (pl.AnimPhase < FRAMES_IDLE * 3) draw_sprite(&PP_idle, shoot ? FRAMES_IDLE : pl.AnimPhase % FRAMES_IDLE, x, y, left);
			else draw_sprite(&PP_idle2, pl.AnimPhase - FRAMES_IDLE * 3, x, y, left);
			break;
		case PA_SCHIESSEN_O:
			if (AK(AK_SHOOT) || AK(AK_GRANATE)) draw_sprite(&PP_oben, shoot ? 1 : 0, x, y, left);
			else draw_sprite(&PP_kucken, 0, x, y, left);
			break;
		case PA_SCHIESSEN_LO:
		case PA_SCHIESSEN_RO:
			if (pl.WalkLock) draw_sprite(&PP_diag, shoot ? 1 : 0, x, y, left);
			break;
		case PA_SPRINGEN:
			if (AK(AK_OBEN))
			{
				if (AK(AK_LINKS) || AK(AK_RECHTS)) draw_sprite(&PP_jumpdiag, pl.AnimPhase, x, y, left);
				else draw_sprite(&PP_jumpup, pl.AnimPhase, x, y, left);
			}
			else draw_sprite(&PP_jump, pl.AnimPhase, x, y, left);
			break;
		case PA_LAUFEN:
			draw_sprite(&PP_run, pl.AnimPhase, x, y, left);
			break;
		case PA_PISSEN:
			draw_sprite(&PP_piss, pl.AnimPhase, x, y, left);
			break;
		case PA_BLITZEN:
		case PA_BEAMLADEN:
			draw_sprite(&PP_blitz, pl.AnimPhase, x, y, left);
			break;
		case PA_DUCKEN:
			draw_sprite(&PP_crouch, shoot ? 1 : 0, x, y, left);
			break;
		case PA_SACKREITEN:
			draw_sprite(&PP_ride, left ? 0 : 10, x, y, 0);
			break;
		case PA_DREHEN:
			draw_sprite(&PP_ride, pl.AnimPhase, x, y, 0);
			break;
		default:
			break;
		}
		draw_flash = 0;
	}
	plf_WasDamaged = 0;

	/* the shot flame */
	if (pl.FlameTime > 0)
	{
		plf_calc_flame_pos();
		if (pl.AustrittAnim >= 0 && pl.AustrittAnim < 3)
			draw_sprite(&PP_flame[pl.AustrittAnim], pl.FlameAnim + pl.SelectedWeapon * 2, x + pl.AustrittX, y + pl.AustrittY, left);
		plf_calc_austritt();
	}

	/* the lightning or the loading beam */
	if (plf_blitz_vis) plf_draw_lightning();
#if PLF_DRAW_SHIELD
	plf_draw_shield();
#endif
}
