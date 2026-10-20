/*
 * Hurrican CD32X - the game's interface between the engine (game/p1.c) and the ported parts
 * of Hurrican (the player, the objects - enemies, triggers, bosses -, the projectiles, the
 * particles). Everything is one C file in the end: p1.c includes this header first, then its
 * engine code, then the ported files (game/player.c, game/proj*.c, game/part.c,
 * game/en/ files, game/types.c). So every function here is static.
 *
 * PORTING RULES (Hurrican C++ -> this C):
 *  - Units are Hurrican's own: positions in the PC game's pixels (640 x 480 screen, 20-pixel
 *    tiles), speeds in pixels per 1/10 s, as fixed point "fx" (8 bits of fraction):
 *      float 12.5f           -> FX(12.5)            (compile-time constant)
 *      Timer.sync(v)         -> SYNC(v)             (v an fx: one 60 Hz step of it)
 *      Timer.sync(2.0f)      -> SYNC(FX(2.0))
 *      Timer.getSpeedFactor()-> SPEEDFACTOR         (= FX(1)/6)
 *      a * b (both fx)       -> FMUL(a, b);  a / b -> FDIV(a, b);  fx * int -> a * n
 *      static_cast<int>(f)   -> FINT(f)  (truncates toward zero like C++)
 *    The game runs 60 steps a second (the PC game's SpeedFaktor 1/6 each step).
 *  - Random: GetRandom(n) -> GetRandom(n) (int 0..n-1).
 *  - sin_deg / cos_deg (degrees, float) -> sin_d(int deg) / cos_d(int deg): fx (-256..256).
 *    atan2 in degrees -> atan2_d(dy, dx) (int degrees, -180..180). sqrtf -> isqrt (ints).
 *  - An enemy class becomes a TypeDef (see below) with C functions; its own member variables
 *    go into the Ent's spare fields f[] (fx) and i[] (ints / bools / indices); document which
 *    is which at the top of its file. Hurrican's base members have the same names in Ent.
 *  - BlickRichtung (DirectionEnum::LINKS / RECHTS) -> e->blick = -1 / 1 (Direction::asInt).
 *  - GEGNER::LAUFEN -> GEGNER_LAUFEN; SOUND::X -> SOUND_X; MUSIC::X -> MUSIC_X;
 *    PlayerActionEnum::X -> PA_X; object / projectile / particle numbers keep their names
 *    (WALKER, SPREADSHOT, EXPLOSION_BIG ... game/hur_enums.h).
 *  - BLOCKWERT_WAND etc. -> B_WAND etc. below (only the flags the map keeps).
 *  - Player[0] / pAim / pParent -> pl (one player); NUMPLAYERS loops -> just pl.
 *  - Gegner.PushGegner(x, y, Art, v1, v2, light) -> gegner_push(x, y, Art, v1, v2)
 *    Projectiles.PushProjectile(x, y, Art, pTarget) -> proj_push(x, y, Art)  (WinkelUebergabe
 *    -> the global proj_winkel, in fx degrees)
 *    PartikelSystem.PushPartikel(x, y, Art) -> part_push(x, y, Art)
 *    SoundManager.PlayWave(.., SOUND::X) -> snd(SOUND_X)   (volume / pan / pitch ignored)
 *    SoundManager.PlaySong(MUSIC::BOSS ..) -> music_boss(); stage song -> music_stage();
 *    fade / stop -> music_stop()
 *    TileEngine.XOffset / YOffset -> cam_xoff() / cam_yoff() (fx, the screen's top-left);
 *    TileEngine.ScrollLevel(x, y, state) -> scroll_level(x, y, TS_x); TileEngine.Zustand ->
 *    tile_zustand(); ShakeScreen(n) -> shake_screen(n) (no effect); HUD.ShowBossHUD /
 *    boss energy bar -> boss_bar(energy, max); StageClear() -> stage_clear();
 *    TileEngine.Block* -> the functions below with the same names.
 *  - Drawing: RenderSprite(x - XOffset, y - YOffset, frame, colour, mirrored) ->
 *    draw_sprite(&pic, frame, x, y, mirrored)  (x, y in world fx, NOT screen). Rotation,
 *    scaling, colour and additive modes do not exist: use the unrotated frame (or the frame
 *    nearest to the angle if the sheet has angles) and draw it plainly. Lines / quads (beams,
 *    lightning): draw_line(x0, y0, x1, y1, colour) or leave out. Keep effects few.
 *  - Leave out: lighting (LightValue), glow sprites (HasGlow), screen colour fades, controller
 *    rumble, debug text, multiplayer, the tutorial.
 *  - Strings: none at run time except the HUD.
 *  - Memory: SDRAM is tight. No new big arrays; a boss may keep a small static struct.
 */
#ifndef HUR_H
#define HUR_H

#include "hur_enums.h"

/* ------------------------------------------------------------------ fixed point */
typedef int32_t fx;
#define FX(v)        ((fx)((v) * 256))
#define SYNC(v)      ((fx)(v) / 6)
#define SPEEDFACTOR  (256 / 6)
#define FMUL(a, b)   ((fx)(((int64_t)(a) * (int64_t)(b)) >> 8))
#define FDIV(a, b)   ((fx)(((int64_t)(a) * 256) / (b)))
#define FINT(a)      ((int)((a) / 256))
#define FABS(a)      ((a) < 0 ? -(a) : (a))

/* ------------------------------------------------------------------ boxes, pictures */
typedef struct { int16_t l, t, r, b; } Box;      /* RECT_struct: left, top, right, bottom */
typedef struct Bank Bank;
typedef struct { const Bank *bank; int first, count, fw, fh; } Pic;  /* fw / fh: half size */

/* a picture by its file name (as Hurrican loads it); count 0 if it is not in the packs */
static Pic pic(const char *name);
/* frame `frame` of a picture with its full frame's top-left at world (x, y) (Hurrican's
 * full-size pixels, fx), mirrored (left-right) or not. Clipped to the screen. */
static void draw_sprite(const Pic *p, int frame, fx x, fx y, int mirrored);
/* the same, drawn flipped upside down as well (RenderMirroredSprite vertical) */
static void draw_sprite_flipv(const Pic *p, int frame, fx x, fx y, int mirrored);
/* a 1-pixel line (beams): world fx coordinates, a palette colour (col_* below) */
static void draw_line(fx x0, fx y0, fx x1, fx y1, int colour);
static int col_dark, col_light, col_red, col_yellow, col_green, col_grey, col_blue;

/* ------------------------------------------------------------------ the map */
#define B_WAND          0x001   /* BLOCKWERT_WAND */
#define B_GEGNERWAND    0x002   /* BLOCKWERT_GEGNERWAND */
#define B_PLATTFORM     0x004   /* BLOCKWERT_PLATTFORM */
#define B_SCHADEN       0x008   /* BLOCKWERT_SCHADEN */
#define B_SCHRAEGE_L    0x010
#define B_SCHRAEGE_R    0x020
#define B_LIQUID        0x040   /* BLOCKWERT_LIQUID or BLOCKWERT_WASSER */
#define B_DESTRUCTIBLE  0x080
#define B_WASSERFALL    0x100
#define B_SUMPF         0x200
#define B_WENDEPUNKT    0x400   /* turning point for moving platforms */
#define B_LITFRONT      0x800   /* (drawing only) */
#define TILESIZE        20
static int map_w, map_h;                          /* LEVELSIZE_X / LEVELSIZE_Y in tiles */
#define LEVELSIZE_X map_w
#define LEVELSIZE_Y map_h
/* the block flags of tile (tx, ty) (TileAt(x, y).Block) */
static int TileBlock(int tx, int ty);
/* Hurrican's tests: x, y = position, xo / yo = last position, rect = collision box; with
 * resolve the position is moved out of the wall (as Hurrican's). All fx. */
static int BlockLinks(fx *x, fx y, fx *xo, fx yo, const Box *rect, int resolve);
static int BlockRechts(fx *x, fx y, fx *xo, fx yo, const Box *rect, int resolve);
static int BlockOben(fx x, fx *y, fx xo, fx *yo, const Box *rect, int resolve);
static int BlockUnten(fx x, fx *y, fx xo, fx *yo, const Box *rect, int resolve);
static int BlockUntenNormal(fx x, fx y, fx xo, fx yo, const Box *rect);
static int BlockSlopes(fx x, fx *y, const Box *rect, fx yspeed);
/* the whole rectangle against the flags in mask: any of them in it */
static int BlockIn(fx x, fx y, const Box *rect, int mask);
/* shoot away a destructible wall tile (TileEngine.ExplodeWall); 1 if there was one */
static int ExplodeWall(int tx, int ty);
/* SpriteCollision(x1, y1, r1, x2, y2, r2) */
static int SpriteCollision(fx x1, fx y1, const Box *r1, fx x2, fx y2, const Box *r2);

/* ------------------------------------------------------------------ the camera */
enum { TS_SCROLLBAR, TS_SCROLLTO, TS_SCROLLTOLOCK, TS_SCROLLTOPLAYER, TS_LOCKED };
static fx cam_xoff(void);          /* TileEngine.XOffset (full size, fx) */
static fx cam_yoff(void);          /* TileEngine.YOffset */
#define RENDERWIDTH   640          /* the screen in full-size pixels */
#define RENDERHEIGHT  480          /* Hurrican's window; the 32X shows its lower 400 (game/p1.c CAM_DY) */
static void scroll_level(fx x, fx y, int state);   /* TileEngine.ScrollLevel */
static int tile_zustand(void);                     /* TileEngine.Zustand */
static void shake_screen(int amount);              /* ShakeScreen: no effect here */

/* ------------------------------------------------------------------ sound, music */
static void snd(int sound_id);                     /* SOUND_x */
static int snd_playing(int sound_id);              /* WaveIsPlaying: always 0 here */
static void music_stage(void);
static void music_boss(void);
static void music_stop(void);

/* ------------------------------------------------------------------ math */
static int GetRandom(int n);
static int sin_d(int deg);                          /* fx */
static int cos_d(int deg);
static int atan2_d(int dy, int dx);                 /* degrees */
static int isqrt(int v);

/* ------------------------------------------------------------------ the player */
typedef struct Ent Ent;
typedef struct {
	fx xpos, ypos, xposold, yposold;     /* top-left of the 70 x 80 frame */
	fx xspeed, yspeed;
	fx JumpStart, JumpAdd, JumpySave, JumpxSave;
	int Handlung;                        /* PA_x */
	int Blickrichtung;                   /* -1 left, 1 right */
	Box CollideRect;
	Ent *AufPlattform;                   /* standing on this object (lift, platform ...) */
	int InLiquid, JumpPossible, GegnerDran, CanBeDamaged, WalkLock, WheelMode, GodMode;
	int AnimPhase, AnimEnde;
	fx AnimCount;
	fx Energy, Armour, Shield;           /* 0..100 (MAX_ENERGY), wheel armour, shield time */
	fx ShotDelay, AutoFireCount, AutoFireExtra, RiesenShotExtra;
	fx BlitzStart, BlitzWinkel, BlitzCount;
	int BlitzLength, BlitzAnim;
	fx DamageCounter, BlinkCounter;
	int BlinkColor;
	uint32_t Score;
	int Lives;
	int CollectedDiamonds, DiamondsThisLevel, LivesThisLevel, BlocksThisLevel, SecretThisLevel;
	int SelectedWeapon;                  /* 0 spread, 1 laser, 2 bounce */
	int CurrentWeaponLevel[4];           /* [3] = the lightning's length level */
	int PowerLines, Grenades, SmartBombs;
	int PowerLinePossible;
	int FesteAktion, DoFesteAktion, StageClearRunning;
	fx ExplodingTimer;
	fx AustrittX, AustrittY;             /* where shots leave the gun */
	int AustrittAnim;
	fx FlameTime;
	int FlameAnim;
	fx BronsonCounter;
	fx safe_x, safe_y;                   /* the engine's: where to come back after dying */
	int safe_t;                          /* steps of safety after coming back */
	int dead_t;
} Player;
static Player pl;
#define MAX_ENERGY   FX(100)
#define MAX_ARMOUR   FX(100)
static void DamagePlayer(fx amount, int override_);   /* PlayerClass::DamagePlayer (skill: medium) */
static int player_hit_box(Box *b);                    /* the player's CollideRect now */
static uint16_t pad, pad_prev;                         /* the joypad (PAD_x in port/plat.h) */

/* ------------------------------------------------------------------ shared globals
 * (Hurrican's globals that several objects use; the engine owns them) */
static fx g_Fahrstuhl_yPos, g_Fahrstuhl_Offset, g_Fahrstuhl_Speed;   /* Gegner_Helper.cpp */
static Pic DroneFlame, DroneGun;     /* GegnerListClass's droneflame.png / Zitronestiel.png */
static fx autoscroll_x, autoscroll_y;  /* TileEngine.ScrollSpeedX / Y (auto-scrolling, units) */
static int UsedPowerBlock;           /* TileEngine.GetUsedPowerBlock() (the map's setting) */
static fx water_level;               /* (unused unless a port needs it) */

/* ------------------------------------------------------------------ objects (Gegner) */
struct Ent {
	int16_t GegnerArt;            /* the object type (WALKER ...); -1 = a free slot */
	int16_t obj;                  /* the map object it came from; -1 = made in the game */
	int16_t Handlung, LastAction;
	int16_t AnimPhase, AnimStart, AnimEnde;
	int16_t Value1, Value2;
	int8_t blick;                 /* BlickRichtung: -1 LINKS, 1 RECHTS */
	uint8_t Active, Destroyable, TestBlock, OwnDraw, DontMove, BackGround, HitSound;
	uint8_t ChangeLight, AlreadyDrawn, NoDespawn, pad_;
	fx AnimSpeed, AnimCount;
	fx xPos, yPos, xPosOld, yPosOld, xSpeed, ySpeed, xAcc, yAcc;
	fx Energy, DamageTaken, TurnCount, TimeToChange;
	uint16_t blockl, blockr, blocko, blocku;
	fx f[8];                      /* the class's own float members (see its file) */
	int32_t i[4];                 /* the class's own int / bool members */
};

typedef struct {
	void (*init)(Ent *e);         /* the constructor: Value1 / Value2 are set, the rest zero
	                                 except Active 0, Destroyable 1, TestBlock 1, blick -1,
	                                 Energy 0, AnimSpeed 0 */
	void (*ki)(Ent *e);           /* DoKI (called by the base Run every step while Active) */
	void (*explode)(Ent *e);      /* GegnerExplode (0: the base one) */
	void (*draw)(Ent *e);         /* DoDraw, for OwnDraw objects (0: none) */
	void (*load)(void);           /* after a stage pack is mounted: look up extra pictures */
} TypeDef;
/* each ported file defines its types with TYPEDEF(NAME) = { init, ki, explode, draw, load };
 * game/types.c lists them */
#define TYPEDEF(name) static const TypeDef typedef_##name

static Box GegnerRect[200];          /* as Hurrican's (game/hur_tables.h), may be changed */
static Pic GegnerPic[200];           /* pGegnerGrafix[Art] */
static Ent *gegner_push(fx x, fx y, int art, int value1, int value2);
static void SimpleAnimation(Ent *e, int backward);
static void TestDamagePlayers(Ent *e, fx dam, int destroy);
static void PlattformTest(Ent *e, const Box *rect);
static void Wegschieben(Ent *e, const Box *rect, fx dam);
static void TurnonWall(Ent *e);
static int TurnonShot(Ent *e);
static int IsOnScreen(const Ent *e);
static int PlayerAbstand(const Ent *e);
static int PlayerAbstandHoriz(const Ent *e);
static int PlayerAbstandVert(const Ent *e);
static void gegner_default_explode(Ent *e);   /* the base GegnerExplode (platform release) */
static void gegner_default_draw(Ent *e);      /* the base Render's sprite */
/* the objects in the game (to look through, e.g. bosses finding their parts) */
#define MAX_LIVE 240
static Ent ents[MAX_LIVE];
/* damage the enemies near (x, y) on the screen (DamageEnemiesonScreen: grenades, bombs) */
static void DamageEnemiesonScreen(fx x, fx y, int maxdamage);
static void stage_clear(void);
static void boss_bar(fx energy, fx max);       /* the boss's energy bar this frame */

/* ------------------------------------------------------------------ projectiles
 * game/proj.c (ported from Projectiles.cpp) provides these: */
static fx proj_winkel;                         /* WinkelUebergabe (fx degrees) */
static void proj_push(fx x, fx y, int art);    /* PushProjectile */
static void proj_blitzbeam(int size, fx richtung);   /* PushBlitzBeam (the player's beam) */
static void proj_clear(void);                  /* ClearAll */
static void proj_clear_type(int art);          /* ClearType */
static void proj_step_all(void);               /* one 60 Hz step of all (DoProjectiles' Run) */
static void proj_draw_all(void);               /* draw all */
static void proj_load(void);                   /* after a stage pack is mounted: pictures */

/* ------------------------------------------------------------------ particles
 * game/part.c (ported from Partikelsystem.cpp, fewer of them) provides these: */
static void part_push(fx x, fx y, int art);    /* PushPartikel */
static void part_clear(void);
static void part_step_all(void);
static void part_draw_all(void);
static void part_load(void);

/* ------------------------------------------------------------------ the player
 * game/player.c (ported from Player.cpp) provides these: */
static void player_init_game(void);            /* InitPlayer: a new game (lives set after) */
static void player_init_level(fx x, fx y, int blick);   /* InitNewLevel at the start position */
static void player_step(void);                 /* one 60 Hz step: input (pad), move, shoot */
static void player_draw(void);                 /* the player, his shot flame, shield, beam */
static void player_load(void);                 /* pictures after a stage pack is mounted */
static void player_respawn(void);              /* after losing a life: back at a safe place */
/* the engine provides: the joypad (pad, pad_prev), the pause, the HUD, lives handling after
 * the player's Handlung becomes PA_TOT (pl.dead_t counts the steps since) */

#endif
