/*
 * Hurrican CD32X - the projectiles: a port of Hurrican's Projectiles.cpp / .hpp
 * (ProjectileClass::CreateShot / Run / CheckCollision / ExplodeShot / Render and
 * ProjectileListClass::LoadSprites / PushProjectile / PushBlitzBeam / ClearAll / ClearType /
 * DoProjectiles). Included by game/p1.c in place of game/proj.c (PROJ_C="proj_full.c").
 *
 * Every projectile type of Projectiles.hpp works as Hurrican's: speeds, accelerations,
 * animation, aiming at the player, bouncing, wall tests (CheckBlock), destructible walls shot
 * away by the player's shots (BlockDestroy* / CheckDestroyableWalls / ExplodeWalls on top of
 * TileBlock and the engine's ExplodeWall), damage to the enemies (CheckCollision with its
 * special cases: bosses take 70 % from grenades / powerlines / smartbombs, the powerline kills
 * what has less than 40 energy, Schneekoppe / Skull / Punisher are pushed, Schleusen are hit
 * by enemy shots, wall flames burn enemies) and to the player (DamagePlayer), ExplodeShot's
 * particles and sounds, the bounce shots splitting, bombs / grenades / the beam hurting what
 * is near.
 *
 * Differences (the 32X):
 *  - at most PJ_MAX (96) projectiles; PushProjectile fails silently when they are all used.
 *  - particles: ExplodeShot's big showers are fewer (PN(): up to 6 as they are, then one in
 *    8 more, at most 12); the per-frame trails Hurrican makes "200 * elapsed + 1" a frame
 *    (Pharao laser, rockets, Evil blitz) push one a step.
 *  - drawing: no glows (HasGlow, LaserSmoke, LavaFlare, PowerlineSmoke, SpreadShotSmoke), no
 *    additive / alpha / colour. Rotated sprites: the thin ones (the player's lasers, the
 *    Skeletor shot, the flying cannon's laser, the icicle) are drawn as a few lines along their
 *    flight unless they fly (nearly) straight up / down (then their frame, flipped for down);
 *    the others use their plain frame. Scaled ones: the flames (flame.png scaled from 20 to 84
 *    pixels) use flamme.png's frames while they are small and flame.png's after, centred where
 *    Hurrican's scaled picture is; the smart bomb's wave is a circle of lines; the beam ball
 *    and the bounce shot their plain frame centred; the spider boss's big laser its frame
 *    repeated along the beam (a line while it charges).
 *  - the screen flash of the beam (SetThunderColor) and the controller rumble are left out.
 *  - Hurrican's quirks that run ExplodeShot twice for one shot (a shot that dies at a wall
 *    and also touches the player / a second enemy in the same frame) explode once here.
 *  - TileEngine.IsElevatorLevel (bombs land on the elevator): g_Fahrstuhl_yPos > 0.
 *  - the Skeletor grenade's turning (Winkel from atan) is only for drawing: left out.
 *  - pParent / pAim / ChooseAim() -> pl (one player).
 *
 * PC check (not on the 32X): HB_PROJ=art[,every[,dx,dy[,level]]] pushes a projectile of that
 * type every `every` steps at the player + (dx, dy); with level the player's weapon levels are
 * set to it (pl.SelectedWeapon from HB_PROJW).
 */

#define PJ_MAX 96
#define PJ_ARTS 264
#define PJ_NOWINKEL FX(-10000)

typedef struct {
	int16_t ShotArt;                  /* -1 = a free slot */
	int16_t AnimPhase, AnimEnde;
	int16_t Damage;
	int16_t xAcc, yAcc, AnimSpeed;    /* fx (small values) */
	uint8_t CheckBlock, BounceWalls, ExplodeOnImpact, DamagePlayer, OwnDraw, fresh;
	fx xPos, yPos, xPosOld, yPosOld;
	fx xSpeed, ySpeed;
	fx AnimCount, Winkel, Counter;
} ProjectileClass;

static ProjectileClass pj[PJ_MAX];
static int pj_in_step, pj_nfresh;
static int bo, bu, bl, br;            /* the block values around the shot (Projectiles.cpp's globals) */

/* ------------------------------------------------------------------ LoadSprites:
 * ProjectileGrafix / ShotRect as a small table (the types that have a picture) */
typedef struct { int16_t art; const char *file; Box r; } PjDef;
static const PjDef pj_def[] = {
	{ DIAMONDSHOT, "diamant.png", { 5, 5, 24, 24 } },
	{ SKELETOR_SHOT, "skeletor_shot.png", { 10, 10, 30, 30 } },
	{ SPREADSHOT, "spreadshot.png", { 4, 4, 12, 12 } },
	{ SPREADSHOT2, "spreadshot2.png", { 2, 2, 16, 16 } },
	{ SPREADSHOTBIG, "spreadshot_big.png", { 4, 4, 28, 28 } },
	{ SPREADSHOTBIG2, "spreadshot_big2.png", { 6, 6, 34, 34 } },
	{ BOUNCESHOT1, "bounce1.png", { 4, 4, 20, 20 } },
	{ BOUNCESHOT2, "bounce2.png", { 4, 4, 12, 12 } },
	{ BOUNCESHOT3, "bounce3.png", { 0, 0, 10, 10 } },
	{ BOUNCESHOTBIG1, "bounce_big1.png", { 8, 8, 40, 40 } },
	{ BOUNCESHOTBIG2, "bounce_big2.png", { 4, 4, 28, 28 } },
	{ BOUNCESHOTBIG3, "bounce_big3.png", { 2, 2, 18, 18 } },
	{ LASERSHOT, "laser.png", { -4, 18, 15, 30 } },
	{ LASERSHOT2, "laser2.png", { 0, 12, 20, 38 } },
	{ LASERSHOTBIG, "laserbig.png", { 0, 30, 30, 50 } },
	{ LASERSHOTBIG2, "laserbig2.png", { 0, 30, 30, 50 } },
	{ WALKER_LASER, "walker-laser.png", { 0, 0, 32, 3 } },
	{ CANONBALL, "canonball.png", { 0, 0, 8, 8 } },
	{ POWERLINE, "powerline.png", { 0, 0, 20, 20 } },
	{ SUCHSCHUSS, "suchschuss.png", { 0, 0, 8, 8 } },
	{ SUCHSCHUSS2, "suchschuss2.png", { 1, 1, 15, 15 } },
	{ GOLEMSCHUSS, "golemschuss.png", { 2, 2, 18, 18 } },
	{ TURRIEXTRAWURST, "golemschuss.png", { 2, 2, 18, 18 } },
	{ SPIDERSLOW, "spiderslow.png", { 5, 5, 35, 35 } },
	{ SPIDERLASER, "fetterspinnenlaser.png", { 0, 0, 500, 10 } },
	{ SPIDERSHOT, "spidershot.png", { 24, 12, 72, 36 } },
	{ SPIDERSHOT2, "spidershot2.png", { 16, 16, 32, 32 } },
	{ BOMBE, "bombe_small.png", { 0, 0, 16, 12 } },
	{ BOMBEBIG, "bombe_big.png", { 0, 0, 24, 20 } },
	{ FLAMME, "flamme.png", { 16, 16, 32, 32 } },
	{ FEUERFALLE, "flame.png", { 30, 30, 54, 70 } },
	{ FIREBALL, "fireball.png", { 0, 12, 24, 36 } },
	{ FIREBALL_BIG, "fireball_big.png", { 8, 8, 52, 46 } },
	{ FIREBALL_BOMB, "fireball_big.png", { 8, 8, 52, 46 } },
	{ ROTZSHOT, "rotzshot.png", { 2, 2, 14, 14 } },
	{ GOLEMSAEULE, "flame.png", { 30, 30, 54, 70 } },
	{ FEUERFALLE_LAVAMANN, "flame.png", { 30, 30, 54, 70 } },
	{ PLAYERFIRE, "flame.png", { 30, 30, 54, 70 } },
	{ SPIDERFIRE, "flame.png", { 30, 30, 54, 70 } },
	{ KRABBLERLASER1, "krabblerlaser1.png", { 0, 0, 4, 48 } },
	{ KRABBLERLASER2, "krabblerlaser2.png", { 0, 0, 48, 4 } },
	{ KRABBLERLASER3, "krabblerlaser2.png", { 0, 0, 48, 4 } },
	{ PHARAOLASER, "pharaolaser.png", { 0, 0, 48, 48 } },
	{ GRENADE, "grenade.png", { 0, 0, 11, 14 } },
	{ ROCKET, "rocket.png", { 20, 20, 55, 55 } },
	{ ROCKETSPIDER, "rocket.png", { 20, 20, 55, 55 } },
	{ ROCKETWERFER, "rocketwerfer.png", { 0, 0, 45, 12 } },
	{ TORPEDO, "torpedo.png", { 0, 0, 33, 10 } },
	{ EVILSHOT, "evilshot.png", { 0, 0, 32, 16 } },
	{ EVILSHOT2, "evilshot2.png", { 0, 0, 32, 16 } },
	{ EVILBLITZ, "evilblitz.png", { 0, 0, 70, 100 } },
	{ BLITZBEAM, "blitzbeam.png", { 40, 40, 120, 120 } },
	{ UFOLASER, "ufolaser.png", { 5, 10, 29, 90 } },
	{ STELZLASER, "stelzlaser.png", { 5, 5, 55, 38 } },
	{ PFLANZESHOT, "pflanzeschuss.png", { 1, 1, 15, 15 } },
	{ BRATKLOPSSHOT, "bratklopsshot.png", { 10, 10, 50, 50 } },
	{ DRONEBULLET, "dronebullet.png", { 2, 2, 10, 10 } },
	{ SCHLEIMSHOT, "schleimshot.png", { 2, 2, 10, 10 } },
	{ LAFASSSHOT, "lafass_fall.png", { 10, 10, 56, 73 } },
	{ EVILBLITZ2, "evilblitz2.png", { 0, 0, 70, 400 } },
	{ FLUGLASER, "flugkanonelaser.png", { 0, 0, 8, 32 } },
	{ EISZAPFENSHOT, "eiszapfenshot.png", { 2, 0, 10, 48 } },
	{ ARCSHOT, "arcshot.png", { 0, 0, 8, 8 } },
	{ SNOWBOMB, "snowbomb.png", { 10, 10, 70, 70 } },
	{ SNOWBOMBSMALL, "snowbombsmall.png", { 5, 5, 15, 15 } },
	{ FETTESPINNESHOT, "fettespinneshot.png", { 5, 5, 35, 45 } },
	{ FETTESPINNESHOT2, "fettespinneshot2.png", { 5, 5, 35, 45 } },
	{ EIERBOMBE, "eierbombe.png", { 5, 5, 20, 20 } },
	{ EVILROUND1, "evilround.png", { 0, 0, 10, 10 } },
	{ SMARTBOMB, "druckwelle.png", { 0, 0, 256, 256 } },
	{ SHIELDSPAWNER, "shieldspawner.png", { 0, 0, 12, 12 } },
	{ BLUEBOMB, "blauebombe.png", { 4, 4, 26, 26 } },
	{ SKELETORGRANATE, "skeletor_granate.png", { 4, 4, 16, 15 } },
	{ ELEKTROPAMPE, "elektropampe.png", { 0, 0, 640, 128 } },
	{ ELEKTROSCHUSS, "spidershot2.png", { 16, 16, 32, 32 } },
};
#define PJ_NDEF ((int)(sizeof pj_def / sizeof pj_def[0]))
static Pic pj_pic[PJ_NDEF];
static uint8_t pj_slot[PJ_ARTS];      /* art -> pj_def index + 1 (0: no picture, rect 0) */
static Pic pj_flamme;                 /* small flames (see the header) */
static const Box pj_nobox = { 0, 0, 0, 0 };

static void proj_load(void)
{
	memset(pj_slot, 0, sizeof pj_slot);
	for (int i = 0; i < PJ_NDEF; i++)
	{
		pj_pic[i] = pic(pj_def[i].file);
		pj_slot[pj_def[i].art] = (uint8_t)(i + 1);
	}
	pj_flamme = pic("flamme.png");
}

static const Pic *pj_pic_of(int art)
{
	const int s = art >= 0 && art < PJ_ARTS ? pj_slot[art] : 0;
	return s ? &pj_pic[s - 1] : 0;
}

/* ShotRect[ShotArt] (the beam's, the smart bomb's and the spider laser's change with the shot) */
static Box pj_rect(const ProjectileClass *p)
{
	const int a = p->ShotArt;
	if (a == BLITZBEAM)
	{
		const int l = p->Damage / 4;
		return (Box){ (int16_t)l, (int16_t)l, (int16_t)(l + p->Damage / 2), (int16_t)(l + p->Damage / 2) };
	}
	if (a == SMARTBOMB)
	{
		const int s = (255 - p->Damage / 2) * 2;
		return (Box){ 0, 0, (int16_t)s, (int16_t)s };
	}
	if (a == SPIDERLASER)
		return p->AnimCount < FX(45) ? pj_nobox : (Box){ 0, -100, 500, 100 };
	const int s = a >= 0 && a < PJ_ARTS ? pj_slot[a] : 0;
	return s ? pj_def[s - 1].r : pj_nobox;
}

/* the many particles of an explosion: fewer here */
#define PN(n) ((n) <= 6 ? (n) : (6 + ((n) - 6) / 8 > 12 ? 12 : 6 + ((n) - 6) / 8))
#define PP(dx, dy, art) part_push(p->xPos + (dx), p->yPos + (dy), (art))
#define R(n) FX(GetRandom(n))

/* ------------------------------------------------------------------ the tile engine's
 * destructible walls (Tileengine.cpp) */
static void pj_explode_wall(int tx, int ty)
{
	/* TileEngine.ExplodeWall: the engine's breaks the tile (smoke, sound); the splinters here */
	if (!ExplodeWall(tx, ty)) return;
	for (int i = 0; i < 2; i++)
		part_push(FX(tx * TILESIZE + GetRandom(10)), FX(ty * TILESIZE + GetRandom(10)), ROCKSPLITTERSMALL);
	for (int k = 0; k < 4; k++)
		part_push(FX(tx * TILESIZE + 8), FX(ty * TILESIZE + 8), FUNKE);
}

static void pj_explode_walls(int x, int y)            /* ExplodeWalls: a round hole */
{
	static const int8_t hole[21][2] = {
		{ -1, -2 }, { 0, -2 }, { 1, -2 },
		{ -2, -1 }, { -1, -1 }, { 0, -1 }, { 1, -1 }, { 2, -1 },
		{ -2, 0 }, { -1, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 },
		{ -2, 1 }, { -1, 1 }, { 0, 1 }, { 1, 1 }, { 2, 1 },
		{ -1, 2 }, { 0, 2 }, { 1, 2 } };
	for (int i = 0; i < 21; i++) pj_explode_wall(x + hole[i][0], y + hole[i][1]);
}

/* a destructible tile in column xlev, rows of the rectangle (BlockDestroyRechts / Links) */
static int pj_destroy_col(int xlev, fx y, const Box *r)
{
	if (xlev < 0 || xlev >= map_w) return 0;
	for (int j = r->t; j < r->b; j += TILESIZE)
	{
		const int yy = FINT(y + FX(j));
		if (yy < 0) continue;
		const int ylev = yy / TILESIZE;
		if (ylev >= map_h) break;
		if (TileBlock(xlev, ylev) & B_DESTRUCTIBLE) { pj_explode_wall(xlev, ylev); return 1; }
	}
	return 0;
}
static int pj_destroy_row(int ylev, fx x, const Box *r)   /* BlockDestroyOben / Unten */
{
	if (ylev < 0 || ylev >= map_h) return 0;
	for (int i = r->l; i < r->r; i += TILESIZE)
	{
		const int xx = FINT(x + FX(i));
		if (xx < 0) continue;
		const int xlev = xx / TILESIZE;
		if (xlev >= map_w) break;
		if (TileBlock(xlev, ylev) & B_DESTRUCTIBLE) { pj_explode_wall(xlev, ylev); return 1; }
	}
	return 0;
}
static int pj_block_destroy(const ProjectileClass *p, const Box *r)
{
	const fx x = p->xPos, y = p->yPos;
	if (p->xPosOld <= x && pj_destroy_col(FINT(x + FX(r->r + 1)) / TILESIZE, y, r)) return 1;
	if (p->xPosOld >= x && pj_destroy_col(FINT(x + FX(r->l - 1)) / TILESIZE, y, r)) return 1;
	if (p->yPosOld >= y && pj_destroy_row(FINT(y + FX(r->t - 1)) / TILESIZE, x, r)) return 1;
	if (p->yPosOld <= y && pj_destroy_row(FINT(y + FX(r->b + 1)) / TILESIZE, x, r)) return 1;
	return 0;
}

static int pj_check_destroyable_walls(fx x, fx y, fx xs, fx ys, const Box *r)
{
	if (x < 0 || x > FX(map_w * TILESIZE) || y < 0 || y > FX(map_h * TILESIZE)) return 0;
	int xstart = FINT(x + SYNC(xs)) / TILESIZE, ystart = FINT(y + SYNC(ys)) / TILESIZE;
	const int xl = r->r / TILESIZE + 2, yl = r->b / TILESIZE + 2;
	if (xstart > map_w - xl) xstart = map_w - xl;
	if (xstart < 1) xstart = 1;
	if (ystart > map_h - yl) ystart = map_h - yl;
	if (ystart < 0) ystart = 0;
	for (int i = xstart - 1; i < xstart + xl; i++)
		for (int j = ystart; j < ystart + yl; j++)
			if (TileBlock(i, j) & B_DESTRUCTIBLE) { pj_explode_wall(i, j); return 1; }
	return 0;
}

/* ------------------------------------------------------------------ helpers */
static void pj_clamp_angle(fx *a)
{
	while (*a < 0) *a += FX(360);
	while (*a >= FX(360)) *a -= FX(360);
}

/* Winkel = WinkelUebergabe, turned round when the player looks left (flip); the shot's
 * speed along it (w = Winkel - 90) */
static int pj_winkel(ProjectileClass *p, int flip)
{
	int W = FINT(proj_winkel);
	p->Winkel = proj_winkel;
	if (flip && pl.Blickrichtung == -1) { W = (W + 180) % 360; p->Winkel = FX(W); }
	return (W - 90) % 360;
}
static void pj_angle_speed(ProjectileClass *p, int w, int speed)
{
	p->xSpeed = cos_d(w) * speed;
	p->ySpeed = sin_d(w) * speed;
}

/* the speed `speed` from (0, 0) towards (dx, dy) (pixels) */
static void pj_aim(ProjectileClass *p, fx dxf, fx dyf, int speed)
{
	int dx = FINT(dxf), dy = FINT(dyf);
	if (dx > 20000) dx = 20000;
	if (dx < -20000) dx = -20000;
	if (dy > 20000) dy = 20000;
	if (dy < -20000) dy = -20000;
	int d = isqrt(dx * dx + dy * dy);
	if (d <= 0) d = 1;
	p->xSpeed = FX(speed) * dx / d;
	p->ySpeed = FX(speed) * dy / d;
}

static ProjectileClass *pj_alloc(void)
{
	for (int i = 0; i < PJ_MAX; i++)
		if (pj[i].ShotArt < 0) return &pj[i];
	return 0;
}

static void proj_clear(void)
{
	for (int i = 0; i < PJ_MAX; i++) pj[i].ShotArt = -1;
	pj_nfresh = 0;
}

static void proj_clear_type(int art)
{
	for (int i = 0; i < PJ_MAX; i++) if (pj[i].ShotArt == art) pj[i].ShotArt = -1;
}

/* ------------------------------------------------------------------ CreateShot */
static void pj_create(ProjectileClass *p, fx x, fx y, int Art)
{
	memset(p, 0, sizeof *p);
	p->xPos = p->xPosOld = x;
	p->yPos = p->yPosOld = y;
	p->ShotArt = (int16_t)Art;
	p->CheckBlock = 1;
	p->ExplodeOnImpact = 1;
	p->Winkel = PJ_NOWINKEL;
	pj_clamp_angle(&proj_winkel);
	int w;

	switch (Art)
	{
	case DIAMONDSHOT:
	{
		Ent *aim = 0;
		for (int k = 0; k < MAX_LIVE; k++) if (ents[k].GegnerArt == PUNISHER) { aim = &ents[k]; break; }
		if (!aim) { p->ShotArt = -1; return; }
		pj_aim(p, aim->xPos + FX(85) - p->xPos, aim->yPos + FX(40) - p->yPos, 85);
		p->AnimEnde = 8;
		p->AnimSpeed = FX(0.75);
		p->Damage = 1000;
	} break;

	case SKELETOR_SHOT:
		p->Damage = 4;
		p->DamagePlayer = 1;
		w = pj_winkel(p, 0);
		pj_angle_speed(p, w, 100);
		break;

	case SPREADSHOT:
		p->Damage = (int16_t)(6 - (pl.CurrentWeaponLevel[pl.SelectedWeapon & 3] * 3 / 4));
		w = pj_winkel(p, 1);
		pj_angle_speed(p, w, 50);
		if (w == -90) p->xSpeed = 0;
		break;
	case SPREADSHOT2:
		p->xPos -= FX(2); p->yPos -= FX(2);
		p->Damage = 6;
		w = pj_winkel(p, 1);
		pj_angle_speed(p, w, 50);
		if (w == -90) p->xSpeed = 0;
		break;
	case SPREADSHOTBIG:
		p->Damage = 12;
		w = pj_winkel(p, 1);
		pj_angle_speed(p, w, 50);
		if (w == -90) p->xSpeed = 0;
		break;
	case SPREADSHOTBIG2:
		p->xPos -= FX(4); p->yPos -= FX(4);
		p->Damage = 16;
		w = pj_winkel(p, 1);
		pj_angle_speed(p, w, 50);
		if (w == -90) p->xSpeed = 0;
		break;

	case LASERSHOT: case LASERSHOT2: case LASERSHOTBIG: case LASERSHOTBIG2:
		p->Damage = Art == LASERSHOT ? 8 : Art == LASERSHOT2 ? 10 : Art == LASERSHOTBIG ? 12 : 16;
		w = pj_winkel(p, 1);
		pj_angle_speed(p, w, 60);
		if (w == -90) p->xSpeed = 0;
		if (Art == LASERSHOT2 || Art == LASERSHOTBIG2) { p->AnimEnde = 9; p->AnimSpeed = FX(0.25); }
		p->ExplodeOnImpact = 0;
		break;

	case BOUNCESHOT1:
		w = pj_winkel(p, 1);
		pj_angle_speed(p, w, 65);
		if (w == -90) p->xSpeed = FX(0.01);
		p->Damage = 8;
		if (pl.CurrentWeaponLevel[2] == 2) p->BounceWalls = 1;
		p->OwnDraw = 1;
		break;
	case BOUNCESHOT2:
		p->Damage = (int16_t)(pl.CurrentWeaponLevel[pl.SelectedWeapon & 3] * 2 - 3);
		if (p->Damage > 8) p->Damage = 8;
		w = pj_winkel(p, 1);
		pj_angle_speed(p, w, 65);
		if (w == -90) p->xSpeed = FX(0.01);
		if (pl.CurrentWeaponLevel[pl.SelectedWeapon & 3] < 5) p->BounceWalls = 1;
		break;
	case BOUNCESHOT3:
		w = pj_winkel(p, 1);
		pj_angle_speed(p, w, 65);
		if (w == -90) p->xSpeed = FX(0.01);
		p->Damage = (int16_t)(pl.CurrentWeaponLevel[pl.SelectedWeapon & 3] - 3);
		if (p->Damage > 6) p->Damage = 6;
		p->BounceWalls = 1;
		break;
	case BOUNCESHOTBIG1:
		w = pj_winkel(p, 1);
		pj_angle_speed(p, w, 65);
		if (w == -90) p->xSpeed = FX(0.01);
		p->Damage = 20;
		if (pl.CurrentWeaponLevel[2] == 2) p->BounceWalls = 1;
		break;
	case BOUNCESHOTBIG2:
		w = pj_winkel(p, 1);
		pj_angle_speed(p, w, 65);
		if (w == -90) p->xSpeed = FX(0.01);
		p->Damage = 14;
		if (pl.CurrentWeaponLevel[pl.SelectedWeapon & 3] < 5) p->BounceWalls = 1;
		break;
	case BOUNCESHOTBIG3:
		w = pj_winkel(p, 1);
		pj_angle_speed(p, w, 65);
		if (w == -90) p->xSpeed = FX(0.01);
		p->Damage = 12;
		p->BounceWalls = 1;
		break;

	case WALKER_LASER:
		p->xSpeed = FX(-30); p->Damage = 2; p->DamagePlayer = 1; p->ExplodeOnImpact = 0;
		break;
	case WALKER_LASER2:
		p->xSpeed = FX(30); p->Damage = 2; p->DamagePlayer = 1; p->ExplodeOnImpact = 0;
		p->ShotArt = WALKER_LASER;
		break;
	case SPIDER_LASER:
		p->xSpeed = FX(-60); p->Damage = 10; p->DamagePlayer = 1;
		p->ShotArt = WALKER_LASER;
		break;
	case CANONBALL:
		p->xSpeed = FX(-30); p->Damage = 8; p->DamagePlayer = 1;
		break;
	case CANONBALL2:
		p->ShotArt = CANONBALL; p->xSpeed = FX(30); p->Damage = 8; p->DamagePlayer = 1;
		break;

	case BOMBE:
		p->AnimEnde = 15; p->AnimSpeed = FX(0.5); p->Damage = 30;
		break;
	case BOMBEBIG:
		p->AnimEnde = 15; p->AnimSpeed = FX(0.5); p->Damage = 90;
		break;

	case SUCHSCHUSS:
		pj_aim(p, pl.xpos + FX(35) - p->xPos, pl.ypos + FX(40) - p->yPos, 20);
		p->Damage = 8; p->DamagePlayer = 1;
		break;
	case STRAIGHTSCHUSS:
		p->ySpeed = FX(-20); p->Damage = 8; p->DamagePlayer = 1; p->ShotArt = SUCHSCHUSS;
		break;
	case STRAIGHTSCHUSS2:
		p->ySpeed = FX(20); p->Damage = 8; p->DamagePlayer = 1; p->ShotArt = SUCHSCHUSS;
		break;
	case SPITTERBOMBESHOTLO: case SPITTERBOMBESHOTLM: case SPITTERBOMBESHOTLU:
	case SPITTERBOMBESHOTRO: case SPITTERBOMBESHOTRM: case SPITTERBOMBESHOTRU:
	{
		const int k = Art - SPITTERBOMBESHOTLO;           /* lo lm lu ro rm ru */
		const int s = k < 3 ? -1 : 1, v = k % 3;
		p->ySpeed = v == 0 ? FX(-8) : v == 2 ? FX(8) : 0;
		p->xSpeed = s * (v == 1 ? FX(25) : FX(20));
		p->Damage = 8; p->DamagePlayer = 1; p->ShotArt = SUCHSCHUSS;
	} break;

	case SHIELDSPAWNER:
	case SHIELDSPAWNER2:
		/* ySpeed: the turn round the player (degrees here, radians in Hurrican), xSpeed: the
		 * particle counter */
		p->Damage = 10;
		p->ySpeed = Art == SHIELDSPAWNER2 ? FX(180) : 0;
		p->xSpeed = FX(1);
		p->ShotArt = SHIELDSPAWNER;
		p->AnimEnde = 9; p->AnimSpeed = FX(1);
		p->ExplodeOnImpact = 0;
		break;

	case SUCHSCHUSS2:
		pj_aim(p, pl.xpos + FX(35) - p->xPos - FX(20), pl.ypos + FX(40) - p->yPos - FX(20), 20);
		p->Damage = 12; p->DamagePlayer = 1;
		p->AnimEnde = 3; p->AnimSpeed = FX(0.5);
		break;

	case TURRIEXTRAWURST:
		p->Counter = proj_winkel;
		p->ySpeed = FX(-16); p->yAcc = FX(1);
		p->Damage = 50; p->DamagePlayer = 1;
		p->AnimEnde = 24; p->AnimSpeed = FX(0.5);
		break;
	case GOLEMSCHUSS:
		p->Winkel = proj_winkel;
		p->xSpeed = -proj_winkel;
		p->ySpeed = FX(-40); p->yAcc = FX(10);
		p->Counter = -p->ySpeed;
		if (FINT(p->xSpeed) % 2 == 0) { p->ySpeed = -p->ySpeed; p->yAcc = (int16_t)-p->yAcc; }
		p->Damage = 50; p->DamagePlayer = 1;
		p->AnimEnde = 24; p->AnimSpeed = FX(0.5);
		break;

	case SPIDERSLOW:
		pj_aim(p, pl.xpos + FX(35) - p->xPos - FX(20), pl.ypos + FX(40) - p->yPos - FX(20), 20);
		p->Damage = 30; p->DamagePlayer = 1;
		p->AnimEnde = 24; p->AnimSpeed = FX(0.5);
		break;
	case SPIDERLASER:
		p->Damage = 10; p->DamagePlayer = 1;
		p->AnimPhase = 1;
		p->ExplodeOnImpact = 0; p->OwnDraw = 1;
		break;
	case SPIDERSHOT:
		w = pj_winkel(p, 0);
		p->xSpeed = -sin_d(w) * 50; p->ySpeed = cos_d(w) * 50;
		p->Damage = 10; p->DamagePlayer = 1; p->ExplodeOnImpact = 0;
		break;
	case SPIDERSHOTLANGSAM:
		w = pj_winkel(p, 0);
		p->xSpeed = -sin_d(w) * 25; p->ySpeed = cos_d(w) * 25;
		p->Damage = 10; p->DamagePlayer = 1;
		p->ShotArt = SPIDERSHOT;
		break;
	case SPIDERSHOT2:
		p->xSpeed = FX(-20);
		p->ySpeed = -FX(GetRandom(12) + 5);
		p->yAcc = (int16_t)(FX(GetRandom(10) + 5) / 5);
		if (GetRandom(2) == 0) { p->ySpeed = -p->ySpeed; p->yAcc = (int16_t)-p->yAcc; }
		p->Damage = 10; p->DamagePlayer = 1; p->BounceWalls = 1;
		p->AnimCount = FX(1);
		break;
	case SPIDERSHOT3:
		p->ySpeed = FX(-40); p->Damage = 10; p->DamagePlayer = 1; p->AnimCount = FX(1);
		p->ShotArt = SPIDERSHOT2;
		break;

	case PHARAOLASER:
		pj_aim(p, pl.xpos + FX(35) - (p->xPos + FX(24)), pl.ypos + FX(40) - (p->yPos + FX(24)), 40);
		p->Damage = 40; p->DamagePlayer = 1;
		p->AnimEnde = 3; p->AnimSpeed = FX(0.5);
		break;

	case FLAMEWALL:
		p->xSpeed = FX(40); p->ySpeed = FX(-20); p->yAcc = FX(2);
		p->Damage = 40; p->DamagePlayer = 1;
		p->AnimEnde = 6; p->AnimSpeed = FX(1);
		snd(SOUND_EXPLOSION1);
		p->CheckBlock = 0; p->ExplodeOnImpact = 0;
		break;

	case POWERLINE:
		p->xSpeed = FX(-80); p->Damage = 20; p->ExplodeOnImpact = 0;
		break;
	case POWERLINE2:
		p->ShotArt = POWERLINE; p->xSpeed = FX(80); p->AnimPhase = 1; p->Damage = 12; p->ExplodeOnImpact = 0;
		break;

	case FLAMME:
		p->xSpeed = FX(GetRandom(120) - 60) / 3;
		p->ySpeed = -FX(GetRandom(60) + 30) / 2;
		p->yAcc = FX(4);
		p->Damage = 50;
		p->AnimEnde = 7;
		p->AnimSpeed = (int16_t)(FX(GetRandom(20) + 20) / 100);
		break;

	case FEUERFALLE_LAVAMANN:
		p->AnimEnde = 29;
		p->AnimPhase = (int16_t)(6 + GetRandom(3));
		p->AnimSpeed = (int16_t)(FX(GetRandom(3) + 10) / 20);
		p->Damage = 1; p->ySpeed = FX(1); p->DamagePlayer = 1;
		p->xSpeed = FX(GetRandom(20) - 10) / 8;
		p->CheckBlock = 0; p->ExplodeOnImpact = 0; p->OwnDraw = 1;
		break;
	case FEUERFALLE:
		p->AnimEnde = 29;
		p->AnimSpeed = (int16_t)(FX(GetRandom(3) + 10) / 20);
		p->Damage = 1; p->DamagePlayer = 1;
		p->ySpeed = FX(-20);
		p->xSpeed = FX(GetRandom(20) - 10) / 8;
		p->yPos -= FX(40); p->xPos += FX(5);
		p->ExplodeOnImpact = 0;
		p->Winkel = R(360);
		p->OwnDraw = 1;
		break;
	case FEUERFALLE2:
		p->ShotArt = FEUERFALLE;
		p->AnimEnde = 29;
		p->AnimSpeed = (int16_t)(FX(GetRandom(3) + 10) / 20);
		p->Damage = 1; p->DamagePlayer = 1;
		p->ySpeed = FX(GetRandom(20) - 10) / 8 - FX(1);
		p->xSpeed = FX(25);
		p->yPos -= FX(7) + FX(GetRandom(20)) / 10;
		p->xPos += FX(33);
		p->ExplodeOnImpact = 0;
		p->Winkel = R(360);
		p->OwnDraw = 1;
		break;
	case FEUERFALLE3:
		p->ShotArt = FEUERFALLE;
		p->AnimEnde = 29;
		p->AnimSpeed = (int16_t)(FX(GetRandom(3) + 10) / 20);
		p->Damage = 1; p->DamagePlayer = 1;
		p->ySpeed = FX(20);
		p->xSpeed = FX(GetRandom(20) - 10) / 8;
		p->yPos += FX(30);
		p->ExplodeOnImpact = 0;
		p->Winkel = R(360);
		p->OwnDraw = 1;
		break;
	case FEUERFALLE4:
		p->ShotArt = FEUERFALLE;
		p->AnimEnde = 29;
		p->AnimSpeed = (int16_t)(FX(GetRandom(3) + 10) / 20);
		p->Damage = 1; p->DamagePlayer = 1;
		p->ySpeed = FX(GetRandom(20) - 10) / 8 - FX(1);
		p->xSpeed = FX(-25);
		p->yPos -= FX(15) + FX(GetRandom(20)) / 10;
		p->xPos -= FX(28);
		p->ExplodeOnImpact = 0;
		p->Winkel = R(360);
		p->OwnDraw = 1;
		break;

	case FIREBALL:
	case FIREBALL_BIG:
		p->Damage = Art == FIREBALL ? 15 : 30;
		p->DamagePlayer = 1;
		p->AnimCount = FX(0.1);
		w = pj_winkel(p, 0);
		pj_angle_speed(p, w, 50);
		break;
	case FIREBALL_BOMB:
		p->DamagePlayer = 1;
		p->xSpeed = (pl.xpos + FX(35) - (p->xPos + FX(50))) / 10;
		p->ySpeed = (pl.ypos + FX(40) - (p->yPos + FX(180))) / 10;
		p->Damage = 10;
		p->yAcc = FX(3);
		p->Winkel = R(360);
		break;

	case ROTZSHOT:
		p->Damage = 12; p->DamagePlayer = 1;
		w = pj_winkel(p, 0);
		p->xSpeed = cos_d(w) * 40;
		p->ySpeed = sin_d(w) * 30;
		p->yAcc = FX(5);
		p->AnimEnde = 24; p->AnimSpeed = FX(0.2);
		p->Counter = FX(0.2);
		break;

	case GOLEMSAEULE:
		p->Damage = 1; p->DamagePlayer = 1;
		p->xSpeed = -FX(GetRandom(10) + 5);
		p->CheckBlock = 0;
		break;

	case SPIDERFIRE:
		p->AnimEnde = 29;
		p->AnimSpeed = (int16_t)(FX(GetRandom(3) + 10) / 30);
		p->DamagePlayer = 1;
		pj_aim(p, pl.xpos + FX(35) - p->xPos, pl.ypos + FX(40) - p->yPos, 20);
		p->Damage = 4;
		p->CheckBlock = 0; p->ExplodeOnImpact = 0;
		p->Winkel = R(360);
		p->OwnDraw = 1;
		break;
	case WALKERFIRE:
		p->AnimEnde = 29;
		p->AnimSpeed = (int16_t)(FX(GetRandom(3) + 10) / 30);
		p->DamagePlayer = 1;
		pj_aim(p, pl.xpos + FX(35) - p->xPos, pl.ypos + FX(5) - p->yPos, 22);
		if (p->ySpeed > FX(5)) p->ySpeed = FX(5);
		if (p->ySpeed < FX(-5)) p->ySpeed = FX(-5);
		if (p->xSpeed > 0 && p->xSpeed < FX(5)) p->xSpeed = FX(5);
		if (p->xSpeed < 0 && p->xSpeed > FX(-5)) p->xSpeed = FX(-5);
		p->Damage = 4;
		p->CheckBlock = 0;
		p->Winkel = R(360);
		p->OwnDraw = 1;
		p->ExplodeOnImpact = 0;
		p->ShotArt = SPIDERFIRE;
		break;

	case ELEKTROSCHUSS:
		p->Counter = FX(1000);
		p->DamagePlayer = 1; p->Damage = 1000; p->ExplodeOnImpact = 0;
		p->ySpeed = FX(25); p->xSpeed = FX(-3);
		break;
	case ELEKTROPAMPE:
		p->Counter = FX(1500);
		p->AnimEnde = 4; p->AnimSpeed = FX(0.5);
		p->DamagePlayer = 1; p->Damage = 20; p->ExplodeOnImpact = 0;
		break;

	case PLAYERFIRE:
	{
		p->Counter = FX(255);
		p->AnimEnde = 29;
		p->AnimSpeed = (int16_t)(FX(GetRandom(3) + 10) / 40);
		p->Damage = 20;
		int W = FINT(proj_winkel - FX(5) + R(10));
		if (pl.Blickrichtung == -1) W = (W + 180) % 360;
		w = (W - 90) % 360;
		pj_angle_speed(p, w, 35);
		p->Winkel = R(360);
		if (w == -90) p->xSpeed = 0;
		p->ExplodeOnImpact = 0; p->OwnDraw = 1;
	} break;

	case KRABBLERLASER1:
		p->Damage = 8; p->DamagePlayer = 1; p->ySpeed = FX(30); p->ExplodeOnImpact = 0;
		break;
	case KRABBLERLASER2:
		p->Damage = 8; p->DamagePlayer = 1; p->xSpeed = FX(30); p->ExplodeOnImpact = 0;
		break;
	case KRABBLERLASER3:
		p->Damage = 8; p->DamagePlayer = 1; p->xSpeed = FX(-30); p->ExplodeOnImpact = 0;
		break;

	case GRENADE:
		w = pj_winkel(p, 1);
		p->xSpeed = cos_d(w) * 60;
		p->ySpeed = sin_d(w) * 60 - FX(6);
		p->yPos -= FX(2);
		p->yAcc = FX(5);
		p->Damage = 300;
		break;

	case ROCKET:
		p->xSpeed = FX(-50); p->Damage = 100; p->DamagePlayer = 1;
		break;
	case ROCKETSPIDER:
		p->AnimCount = FX(5); p->ySpeed = FX(-60); p->Damage = 40; p->DamagePlayer = 1;
		break;
	case ROCKETWERFER:
		if (pl.xpos > x) p->AnimPhase = 1;
		p->ySpeed = pl.ypos + FX(60) < y ? FX(-20) : FX(20);
		p->Damage = 40; p->DamagePlayer = 1;
		break;
	case ROBOROCKET:
		p->xSpeed = FX(-30);
		if (pl.xpos > x) { p->AnimPhase = 1; p->xSpeed = FX(30); }
		p->Damage = 40; p->DamagePlayer = 1;
		p->ShotArt = ROCKETWERFER;
		break;

	case ARCSHOT:
		p->ySpeed = FX(-30); p->yAcc = FX(4); p->Damage = 10; p->DamagePlayer = 1;
		break;
	case ARCSHOTLEFT:
	case ARCSHOTRIGHT:
		p->ShotArt = ARCSHOT;
		p->ySpeed = FX(-30);
		p->xSpeed = (Art == ARCSHOTLEFT ? -8 : 8) * proj_winkel;
		p->yAcc = FX(4); p->Damage = 10; p->DamagePlayer = 1;
		break;

	case EVILROUND1: case EVILROUND2: case EVILROUND3: case EVILROUND4: case EVILROUND5: case EVILROUND6:
	case EVILROUND7: case EVILROUND8: case EVILROUND9: case EVILROUNDA: case EVILROUNDB: case EVILROUNDC:
	{
		static const int8_t xs[12] = { -50, -45, -40, -30, -20, -10, 10, 20, 30, 40, 45, 50 };
		static const int8_t ys[12] = { 0, -8, -16, -24, -32, -40, -40, -32, -24, -16, 0, 0 };
		const int k = Art - EVILROUND1;
		p->ShotArt = EVILROUND1;
		p->xSpeed = FX(xs[k]); p->ySpeed = FX(ys[k]);
		p->yAcc = Art == EVILROUNDB ? FX(8) : FX(5);
		p->Damage = 20; p->DamagePlayer = 1;
	} break;

	case SNOWBOMB:
		p->xSpeed = FX(-8); p->ySpeed = FX(26); p->Damage = 40; p->DamagePlayer = 1;
		p->AnimEnde = 19; p->AnimSpeed = FX(0.7);
		break;
	case FETTESPINNESHOT:
	case FETTESPINNESHOT2:
		p->xSpeed = Art == FETTESPINNESHOT ? FX(30) : FX(-30);
		p->Damage = 20; p->DamagePlayer = 1;
		p->AnimEnde = 3; p->AnimSpeed = FX(0.5);
		break;
	case EIERBOMBE:
		p->xSpeed = pl.xpos + FX(40) < p->xPos + FX(20) ? FX(-18) : FX(18);
		p->Damage = 40; p->DamagePlayer = 1; p->AnimSpeed = FX(0.5);
		p->AnimPhase = p->xSpeed < 0 ? 18 : 0;
		p->AnimEnde = 19;
		break;
	case SNOWBOMBSMALL:
		p->xSpeed = FX(GetRandom(80) - 40) / 5;
		p->ySpeed = -FX(GetRandom(40) + 80) / 5;
		p->yAcc = FX(5); p->Damage = 15; p->DamagePlayer = 1;
		p->AnimEnde = 19; p->AnimSpeed = FX(0.7);
		break;
	case TORPEDO:
		if (pl.xpos > x) { p->AnimPhase = 1; p->xSpeed = FX(20); }
		else { p->AnimPhase = 0; p->xSpeed = FX(-20); }
		p->AnimCount = FX(2);
		p->Damage = 20; p->DamagePlayer = 1;
		break;
	case EVILSHOT:
		p->xSpeed = FX(65); p->Damage = 10; p->DamagePlayer = 1;
		break;
	case EVILSHOT2:
		p->xSpeed = FX(-65); p->Damage = 10; p->DamagePlayer = 1;
		break;
	case EVILBLITZ:
		p->AnimEnde = 1; p->AnimSpeed = FX(0.7); p->Damage = 50;
		p->ySpeed = FX(-20); p->yAcc = FX(-1);
		p->DamagePlayer = 1; p->ExplodeOnImpact = 0;
		break;
	case EVILBLITZ2:
		p->Damage = 100; p->AnimSpeed = FX(0.5); p->AnimEnde = 1; p->ySpeed = FX(30);
		p->DamagePlayer = 1; p->ExplodeOnImpact = 0;
		break;
	case UFOLASER:
		p->AnimEnde = 9; p->AnimSpeed = FX(0.25); p->Damage = 20;
		p->ySpeed = FX(20); p->yAcc = FX(10);
		p->DamagePlayer = 1; p->ExplodeOnImpact = 0;
		break;
	case STELZLASER:
		p->AnimPhase = 1; p->xSpeed = FX(-30); p->Damage = 8;
		p->ySpeed = FX(15) + g_Fahrstuhl_Speed;
		p->DamagePlayer = 1; p->ExplodeOnImpact = 0;
		break;
	case STELZLASER2:
		p->ShotArt = STELZLASER;
		p->xSpeed = FX(30); p->Damage = 8;
		p->ySpeed = FX(15) + g_Fahrstuhl_Speed;
		p->DamagePlayer = 1;
		break;
	case PFLANZESHOT:
		p->xSpeed = (pl.xpos + FX(35) - (p->xPos + FX(20))) / 10;
		p->ySpeed = (pl.ypos + FX(40) - (p->yPos + FX(20)) - FX(160)) / 10;
		p->Damage = 10; p->yAcc = FX(3.25); p->DamagePlayer = 1;
		p->AnimEnde = 3; p->AnimSpeed = FX(0.25);
		break;
	case BRATKLOPSSHOT:
		pj_aim(p, pl.xpos + FX(35) - (p->xPos + FX(30)), pl.ypos + FX(40) - (p->yPos + FX(30)), 20);
		p->Damage = 40; p->DamagePlayer = 1; p->OwnDraw = 1;
		break;
	case DRONEBULLET:
	case DRONEBULLET2:
		p->xSpeed = Art == DRONEBULLET ? FX(40) : FX(-40);
		p->ySpeed = FX(9); p->Damage = 6; p->DamagePlayer = 1;
		p->AnimEnde = 3; p->AnimSpeed = FX(0.2);
		p->ShotArt = DRONEBULLET;
		break;
	case SCHLEIMSHOT:
		pj_aim(p, pl.xpos + FX(35) - p->xPos, pl.ypos + FX(40) - p->yPos, 20);
		p->Damage = 8; p->DamagePlayer = 1;
		break;
	case LAFASSSHOT:
		p->xSpeed = proj_winkel;
		p->yAcc = FX(5);
		p->AnimEnde = 14; p->AnimSpeed = FX(1);
		p->Damage = 8000;
		break;
	case FLUGLASER:
	case EISZAPFENSHOT:
		p->Damage = Art == FLUGLASER ? 15 : 12;
		p->DamagePlayer = 1;
		w = pj_winkel(p, 0);
		pj_angle_speed(p, w, 40);
		break;
	case SMARTBOMB:
		p->AnimCount = FX(32);
		p->CheckBlock = 0; p->ExplodeOnImpact = 0;
		p->Damage = (255 - 32) * 2;
		p->OwnDraw = 1;
		break;
	case BLUEBOMB:
		p->AnimSpeed = FX(0.5); p->DamagePlayer = 1; p->Damage = 20; p->AnimEnde = 19;
		p->ySpeed = FX(10); p->yAcc = FX(10);
		break;
	case SKELETORGRANATE:
		p->AnimEnde = 9; p->AnimSpeed = FX(0.5); p->Damage = 40; p->Counter = FX(1);
		if (proj_winkel > FX(180)) proj_winkel = -(FX(360) - proj_winkel);
		p->xSpeed = FMUL(proj_winkel, FX(1.8));
		p->ySpeed = FX(-18);
		p->yAcc = FX(6.2);
		break;

	default:
		/* (BOUNCESHOT2_LONG / BOUNCESHOT3_LONG: never made by the game) */
		p->ShotArt = -1;
		break;
	}
}

static void proj_push(fx x, fx y, int art)
{
	ProjectileClass *p = pj_alloc();
	if (!p) return;
	pj_create(p, x, y, art);
	if (p->ShotArt < 0) return;
	if (pj_in_step) { p->fresh = 1; pj_nfresh++; }
}

/* PushBlitzBeam: the player's beam of size `size` in direction `richtung` (fx degrees) */
static void proj_blitzbeam(int size, fx richtung)
{
	ProjectileClass *p = pj_alloc();
	if (!p) return;
	memset(p, 0, sizeof *p);
	p->ShotArt = BLITZBEAM;
	p->xPos = p->xPosOld = pl.xpos - FX(size) / 2 + FX(20);
	p->yPos = p->yPosOld = pl.ypos - FX(size) / 2 + FX(32);
	p->AnimEnde = 3;
	p->AnimSpeed = FX(0.5);
	p->Damage = (int16_t)size;
	p->CheckBlock = 1;
	p->OwnDraw = 1;
	p->Winkel = PJ_NOWINKEL;
	richtung += FX(180);
	pj_clamp_angle(&richtung);
	const int r = FINT(richtung);
	p->xSpeed = -40 * sin_d(r);
	p->ySpeed = 40 * cos_d(r);
	if (pj_in_step) { p->fresh = 1; pj_nfresh++; }
}

/* ------------------------------------------------------------------ ExplodeShot */
static void pj_split(ProjectileClass *p, int art, int off)
{
	const fx x = p->xPos + FX(off) - SYNC(p->xSpeed), y = p->yPos + FX(off) - SYNC(p->ySpeed);
	int a1, a2;
	if (bl & B_WAND) { a1 = 45; a2 = 135; }
	else if (br & B_WAND) { a1 = 315; a2 = 225; }
	else if (bo & B_WAND) { a1 = 225; a2 = 135; }
	else { a1 = 315; a2 = 45; }
	proj_winkel = FX(a1);
	proj_push(x, y, art);
	proj_winkel = FX(a2);
	proj_push(x, y, art);
}

static void pj_explode(ProjectileClass *p)
{
	int i;
	switch (p->ShotArt)
	{
	case FIREBALL:
	case FIREBALL_BIG:
		snd(SOUND_EXPLOSION1);
		PP(0, 0, SHOTFLARE);
		for (i = 0; i < 4; i++)
		{
			PP(FX(5) + R(20), FX(5) + R(10), FUNKE);
			PP(R(20), FX(-5) + R(10), p->ShotArt == FIREBALL ? SMOKE : SMOKEBIG);
			PP(FX(5) + R(20), FX(5) + R(10), LASERFUNKE2);
		}
		if (p->ShotArt == FIREBALL) PP(FX(-10), 0, EXPLOSION_MEDIUM2);
		else PP(FX(-10), FX(-20), EXPLOSION_BIG);
		break;

	case FIREBALL_BOMB:
		snd(SOUND_EXPLOSION1);
		for (i = 0; i < 2; i++) PP(FX(-30), FX(-20), EXPLOSIONFLARE);
		for (i = 0; i < 4; i++) PP(FX(-10) + R(20), FX(-5) + R(10), SMOKEBIG);
		for (i = 0; i < 8; i++) proj_push(p->xPos - FX(20) + R(30), p->yPos + FX(20) + R(30), FEUERFALLE);
		break;

	case ROTZSHOT:
		snd(SOUND_SPREADHIT);
		PP(0, 0, SHOTFLARE);
		for (i = 0; i < 4; i++)
		{
			PP(FX(-5) + R(10), FX(-5) + R(10), FUNKE);
			PP(FX(-5) + R(10), FX(-5) + R(10), LASERFUNKE2);
		}
		break;

	case DIAMONDSHOT:
		PP(0, 0, DIAMANTCOLLECTED);
		snd(SOUND_EXPLOSION1);
		snd(SOUND_COLLECT);
		break;

	case SKELETOR_SHOT:
		PP(FX(5), FX(5), SHOTFLARE);
		for (i = 0; i < 4; i++) PP(FX(10), FX(10), FUNKE);
		if (GetRandom(3) == 0)
			for (i = 0; i < 3; i++) if (GetRandom(2) == 0) PP(FX(10), FX(5), LONGFUNKE);
		break;

	case SPREADSHOT:
	case SPREADSHOT2:
		PP(0, 0, SHOTFLARE);
		for (i = 0; i < 4; i++) PP(FX(5), FX(5), FUNKE);
		if (GetRandom(3) == 0)
			for (i = 0; i < 3; i++) if (GetRandom(2) == 0) PP(FX(5), FX(5), LONGFUNKE);
		break;

	case SPREADSHOTBIG:
	case SPREADSHOTBIG2:
		snd(SOUND_SPREADHIT);
		for (i = 0; i < 8; i++) PP(FX(5), FX(5), FUNKE);
		for (i = 0; i < 3; i++) PP(FX(5), FX(5), LONGFUNKE);
		break;

	case LASERSHOT:
	case LASERSHOT2:
		PP(FX(-50), FX(-30), LASERFLARE);
		for (i = 0; i < 3; i++) PP(FX(10), FX(5), LASERFUNKE);
		if (p->xSpeed > 0) PP(FX(25), FX(8), BLUE_EXPLOSION);
		else PP(FX(-5), FX(8), BLUE_EXPLOSION);
		break;

	case LASERSHOTBIG:
	case LASERSHOTBIG2:
		for (i = 0; i < 8; i++) PP(FX(10) + R(70), FX(5), LASERFUNKE);
		for (i = 0; i < 5; i++)
			if (p->xSpeed > 0) PP(FX(10) + R(70), FX(-10) + R(20), BLUE_EXPLOSION);
			else PP(FX(-20) + R(20), FX(-10) + R(20), BLUE_EXPLOSION);
		break;

	case BOUNCESHOT1:
		PP(FX(4), FX(4), SHOTFLARE2);
		for (i = 0; i < 4; i++) PP(FX(8), FX(8), FUNKE2);
		if (pl.CurrentWeaponLevel[pl.SelectedWeapon & 3] > 2) pj_split(p, BOUNCESHOT2, 4);
		break;

	case BOUNCESHOT2:
		snd(SOUND_BOUNCESHOT);
		PP(0, 0, SHOTFLARE2);
		for (i = 0; i < 2; i++) PP(FX(8), FX(8), FUNKE2);
		if (pl.CurrentWeaponLevel[pl.SelectedWeapon & 3] >= 5) pj_split(p, BOUNCESHOT3, 3);
		break;

	case BOUNCESHOT3:
		snd(SOUND_BOUNCESHOT);
		for (i = 0; i < 3; i++) PP(FX(4), FX(4), FUNKE2);
		break;

	case BOUNCESHOTBIG1:
	case BOUNCESHOTBIG2:
		snd(SOUND_BOUNCESHOT);
		PP(FX(-32), FX(-32), EXPLOSIONFLARE);
		for (i = 0; i < 4; i++) PP(FX(8), FX(8), FUNKE2);
		if (p->ShotArt == BOUNCESHOTBIG1 ? pl.CurrentWeaponLevel[pl.SelectedWeapon & 3] > 2
		                                 : pl.CurrentWeaponLevel[pl.SelectedWeapon & 3] >= 5)
			pj_split(p, p->ShotArt == BOUNCESHOTBIG1 ? BOUNCESHOTBIG2 : BOUNCESHOTBIG3, 8);
		break;

	case CANONBALL:
	case SUCHSCHUSS:
		snd(SOUND_SPREADHIT);
		for (i = 0; i < 4; i++) PP(FX(5), FX(5), FUNKE);
		PP(FX(-10), FX(-10), SMOKE);
		break;

	case SPIDERSHOT:
	case SPIDERSHOT2:
	case SPIDERSLOW:
		for (i = 0; i < PN(10); i++) PP(FX(-10) + R(40), FX(-10) + R(40), LILA2);
		PP(FX(-40), FX(-40), EXPLOSIONFLARE2);
		break;

	case ELEKTROSCHUSS:
		snd(SOUND_BLITZENDE);
		proj_push(cam_xoff() - FX(60), cam_yoff() + FX(RENDERHEIGHT - 128), ELEKTROPAMPE);
		break;

	case SUCHSCHUSS2:
		snd(SOUND_FUNKE2);
		snd(SOUND_FUNKE3);
		PP(FX(-50), FX(-50), LASERFLARE);
		for (i = 0; i < PN(10); i++) PP(R(12), R(12), LASERFUNKE);
		for (i = 0; i < 4; i++) PP(FX(-4) + R(8), FX(-4) + R(8), SMOKE3);
		break;

	case TURRIEXTRAWURST:
	case GOLEMSCHUSS:
		snd(SOUND_EXPLOSION3);
		for (i = 0; i < PN(40); i++) PP(R(35), R(35), FUNKE2);
		break;

	case WALKER_LASER:
	case SPIDER_LASER:
		snd(SOUND_SPREADHIT);
		for (i = 0; i < 8; i++) PP(FX(10), 0, FUNKE);
		break;

	case PHARAOLASER:
		for (i = 0; i < PN(50); i++) PP(R(48) - FX(6), R(48) - FX(6), PHARAOSMOKE);
		break;

	case ROCKET:
	case ROCKETSPIDER:
		PP(FX(8), FX(8), EXPLOSION_MEDIUM2);
		for (i = 0; i < PN(100); i++)
		{
			PP(R(55) + FX(4), R(55) + FX(4), ROCKETSMOKE);
			PP(R(35) + FX(14), R(35) + FX(14), ROCKETSMOKE);
		}
		snd(SOUND_EXPLOSION1);
		break;

	case ROCKETWERFER:
		PP(FX(8), FX(-20), EXPLOSION_MEDIUM2);
		for (i = 0; i < PN(75); i++)
		{
			PP(R(40) + FX(18), R(40) - FX(20), ROCKETSMOKE);
			PP(R(20) + FX(26), R(20) - FX(10), ROCKETSMOKE);
		}
		snd(SOUND_EXPLOSION1);
		break;

	case TORPEDO:
		PP(FX(8), FX(-20), EXPLOSION_MEDIUM2);
		for (i = 0; i < PN(20); i++) PP(R(33), R(10), BUBBLE);
		snd(SOUND_EXPLOSION1);
		break;

	case EVILSHOT:
	case EVILSHOT2:
		PP(0, FX(-20), EXPLOSION_MEDIUM2);
		snd(SOUND_EXPLOSION1);
		break;

	case FLUGLASER:
		for (i = 0; i < PN(12); i++) PP(R(4), FX(10) + R(14), FUNKE);
		for (i = 0; i < 6; i++) PP(R(4), FX(10) + R(14), LASERFUNKE2);
		snd(SOUND_SPREADHIT);
		break;

	case EISZAPFENSHOT:
		PP(FX(-10), FX(8), SNOWFLUSH);
		snd(SOUND_SPREADHIT);
		break;

	case EVILBLITZ:
		proj_push(p->xPos - FX(290), p->yPos - FX(300), EVILBLITZ2);
		proj_push(p->xPos - FX(100), p->yPos - FX(200), EVILBLITZ2);
		proj_push(p->xPos + FX(100), p->yPos - FX(100), EVILBLITZ2);
		proj_push(p->xPos + FX(290), p->yPos, EVILBLITZ2);
		snd(SOUND_EXPLOSION1);
		break;

	case BLITZBEAM:
	{
		const fx h = FX(p->Damage) / 2;
		DamageEnemiesonScreen(p->xPos, p->yPos, p->Damage);
		for (i = 0; i < PN(300); i++) PP(h, h, BEAMSMOKE3);
		for (i = 0; i < PN(100); i++) PP(h - FX(12), h - FX(12), BEAMSMOKE4);
		PP(FX(-32) + FX(p->Damage / 2), FX(-32) + FX(p->Damage / 2), BEAMSMOKE5);
		PP(FX(-31) + FX(p->Damage / 2), FX(-32) + FX(p->Damage / 2), BEAMSMOKE5);
		p->Damage = 0;
		snd(SOUND_BLITZENDE);
		snd(SOUND_EXPLOSION1);
	} break;

	case STELZLASER:
		for (i = 0; i < PN(50); i++) PP(R(60) - FX(6), R(35) + FX(5), PHARAOSMOKE);
		snd(SOUND_SPREADHIT);
		break;

	case PFLANZESHOT:
		for (i = 0; i < PN(20); i++) PP(R(16), R(16), FUNKE);
		snd(SOUND_SPREADHIT);
		break;

	case BRATKLOPSSHOT:
		for (i = 0; i < 5; i++) PP(FX(-20) + R(40), FX(-20) + R(40), EXPLOSION_GREEN);
		for (i = 0; i < PN(100); i++) PP(FX(10) + R(40), FX(10) + R(40), FUNKE2);
		snd(SOUND_EXPLOSION1);
		break;

	case DRONEBULLET:
		snd(SOUND_SPREADHIT);
		for (i = 0; i < PN(12); i++) PP(FX(2), FX(2), FUNKE);
		PP(0, 0, SMOKE3);
		if (GetRandom(3) == 0)
			for (i = 0; i < 3; i++) if (GetRandom(2) == 0) PP(FX(2), FX(-12), LONGFUNKE);
		break;

	case SCHLEIMSHOT:
		snd(SOUND_MADE);
		PP(0, 0, MADEBLUT);
		break;

	case SKELETORGRANATE:
		snd(SOUND_EXPLOSION3);
		PP(FX(-20), FX(-20), EXPLOSION_MEDIUM2);
		for (i = 0; i < 2; i++) PP(FX(-10) + R(5), FX(-10), SPIDERSPLITTER);
		for (i = 0; i < 15; i++) proj_push(p->xPos - FX(20), p->yPos + FX(i * 9), FEUERFALLE);
		break;

	case BLUEBOMB:
		snd(SOUND_EXPLOSION3);
		for (i = 0; i < PN(10); i++) PP(FX(-10) + R(20), FX(-10) + R(20), BLUE_EXPLOSION);
		for (i = 0; i < PN(15); i++) PP(FX(-10) + R(20), FX(-10) + R(20), WATERFLUSH_HIGH2);
		break;

	case LAFASSSHOT:
		snd(SOUND_EXPLOSION3);
		PP(0, 0, EXPLOSION_TRACE);
		for (i = 0; i < PN(20); i++) PP(FX(10) + R(40), FX(10) + R(60), SPIDERSPLITTER);
		for (i = 0; i < 5; i++) PP(FX(-30) + R(66), FX(-30) + R(83), EXPLOSION_MEDIUM2);
		break;

	case ARCSHOT:
		for (i = 0; i < PN(100); i++) PP(R(20) - FX(10), R(20) - FX(10), ROCKETSMOKEBLUE);
		snd(SOUND_EXPLOSION1);
		break;

	case SNOWBOMB:
		for (i = 0; i < 8; i++) proj_push(p->xPos + R(60), p->yPos + R(50), SNOWBOMBSMALL);
		for (i = 0; i < PN(20); i++) PP(R(70) - FX(10), R(70) - FX(10), SNOWFLUSH);
		snd(SOUND_LANDEN);
		break;

	case SNOWBOMBSMALL:
		for (i = 0; i < 4; i++) PP(FX(-25) + R(20), FX(-25) + R(20), SNOWFLUSH);
		snd(SOUND_LANDEN);
		break;

	case FETTESPINNESHOT:
	case FETTESPINNESHOT2:
		snd(SOUND_SPREADHIT);
		for (i = 0; i < PN(100); i++) PP(FX(-5) + R(40), FX(-5) + R(50), SMOKE2);
		for (i = 0; i < PN(30); i++) PP(R(40), R(50), FUNKE);
		for (i = 0; i < PN(40); i++) PP(R(20) + FX(10), R(30) + FX(10), PHARAOSMOKE);
		break;

	case EIERBOMBE:
		snd(SOUND_EXPLOSION1);
		for (i = 0; i < PN(30); i++) PP(FX(-20) + R(60), FX(-20) + R(60), SMOKE3);
		PP(FX(-40), FX(-40), EXPLOSION_GIANT);
		for (i = 0; i < 4; i++) PP(FX(-30) + R(40), FX(-30) + R(40), EXPLOSION_MEDIUM2);
		break;

	case EVILROUND1:
		for (i = 0; i < PN(50); i++) PP(R(20) - FX(10), R(20) - FX(10), EVILROUNDSMOKE);
		snd(SOUND_EXPLOSION1);
		break;

	case BOMBE:
	case BOMBEBIG:
	{
		const Box r = pj_rect(p);
		pj_explode_walls(FINT(p->xPos) / TILESIZE, FINT(p->yPos) / TILESIZE);
		snd(SOUND_EXPLOSION1);
		snd(SOUND_EXPLOSION3);
		const int g = p->ShotArt == BOMBEBIG ? 5 : 2;
		for (i = 0; i < g; i++)
		{
			PP(FX(-26), FX(-26), SMOKEBIG);
			PP(R(4), R(4), SPIDERSPLITTER);
		}
		PP(FX(-24), FX(-24), EXPLOSION_MEDIUM2);
		if (p->ShotArt == BOMBEBIG) PP(FX(-55), FX(-55), EXPLOSION_GIANT);
		/* the enemies near lose energy by the distance */
		const int schaden = p->ShotArt == BOMBEBIG ? 250 : 80;
		const int cx = FINT(p->xPos) + r.l + (r.r - r.l) / 2, cy = FINT(p->yPos) + r.t + (r.b - r.t) / 2;
		for (int k = 0; k < MAX_LIVE; k++)
		{
			Ent *e = &ents[k];
			if (e->GegnerArt < 0 || !e->Active || !e->Destroyable ||
			    e->GegnerArt == EXTRAS || e->GegnerArt == ONEUP || e->GegnerArt == DIAMANT) continue;
			const int dx = cx - (FINT(e->xPos) + GegnerRect[e->GegnerArt].r / 2);
			const int dy = cy - (FINT(e->yPos) + GegnerRect[e->GegnerArt].b / 2);
			if (dx > schaden || dx < -schaden || dy > schaden || dy < -schaden) continue;
			const int d = isqrt(dx * dx + dy * dy);
			if (d > schaden) continue;
			if (e->DamageTaken <= 0) e->DamageTaken = FX(255);
			e->Energy -= FX(schaden - d) * 3 / 2;
		}
		/* the player as a wheel near: thrown up */
		{
			const int dx = cx - (FINT(pl.xpos) + pl.CollideRect.r / 2);
			const int dy = cy - (FINT(pl.ypos) + pl.CollideRect.b / 2);
			const int d = (dx > 200 || dx < -200 || dy > 200 || dy < -200) ? 999 : isqrt(dx * dx + dy * dy);
			if (((p->ShotArt == BOMBE && d <= 60) || (p->ShotArt == BOMBEBIG && d <= 80)) &&
			    (pl.Handlung == PA_RADELN || pl.Handlung == PA_RADELN_FALL))
			{
				pl.yspeed -= p->ShotArt == BOMBE ? FX(45) : FX(75);
				pl.JumpAdd = FX(12.5);              /* PLAYER_JUMPADDSPEED */
				pl.Handlung = PA_RADELN_FALL;
			}
		}
	} break;

	case FLAMME:
		snd(SOUND_EXPLOSION1);
		PP(FX(-16), FX(-16), EXPLOSION_MEDIUM2);
		for (i = 0; i < 8; i++) PP(FX(4), FX(4), FUNKE);
		break;

	case KRABBLERLASER1:
		snd(SOUND_SPREADHIT);
		for (i = 0; i < PN(24); i++) PP(FX(-1) + R(2), FX(i * 2) + R(2), LASERFUNKE2);
		break;
	case KRABBLERLASER2:
		snd(SOUND_SPREADHIT);
		for (i = 0; i < 8; i++) PP(FX(36), 0, LASERFUNKE2);
		break;
	case KRABBLERLASER3:
		snd(SOUND_SPREADHIT);
		for (i = 0; i < 8; i++) PP(0, 0, LASERFUNKE2);
		break;

	case GRENADE:
		snd(SOUND_EXPLOSION1);
		PP(FX(-100), FX(-100), EXPLOSION_GIGA);
		PP(FX(-20), FX(-20), EXPLOSION_MEDIUM2);
		for (i = 0; i < PN(100); i++)
		{
			PP(FX(-20) + R(40), FX(-20) + R(40), ROCKETSMOKE);
			PP(FX(-20) + R(40), FX(-20) + R(40), FUNKE);
		}
		for (i = 0; i < PN(50); i++) PP(FX(-20) + R(40), FX(-20) + R(40), LONGFUNKE);
		PP(FX(-128), FX(-128), GRENADEFLARE);
		shake_screen(5);
		DamageEnemiesonScreen(p->xPos, p->yPos, 400);
		pj_explode_walls(FINT(p->xPos) / TILESIZE, FINT(p->yPos) / TILESIZE);
		break;

	case SHIELDSPAWNER:
		snd(SOUND_EXPLOSION1);
		for (i = 0; i < PN(10); i++) PP(FX(-5) + R(10), FX(-5) + R(10), SMOKE);
		for (i = 0; i < PN(30); i++) PP(FX(-5) + R(10), FX(-5) + R(10), FUNKE);
		break;

	default:
		break;
	}
	p->Damage = 0;
}

/* ------------------------------------------------------------------ Run */
static int pj_wall(void) { return (bl | br | bo | bu) & B_WAND; }

static void pj_run(ProjectileClass *p)
{
	const int a = p->ShotArt;
	Box rect = pj_rect(p);
	int i;
	bo = bu = bl = br = 0;

	if (p->AnimEnde > 0) p->AnimCount += SYNC(FX(1));

	p->xSpeed += SYNC(p->xAcc);
	p->ySpeed += SYNC(p->yAcc);
	p->xPos += SYNC(p->xSpeed);
	p->yPos += SYNC(p->ySpeed);

	/* off the screen? */
	if (a != FLAMME && a != ROCKETSPIDER && a != EVILBLITZ && a != EVILBLITZ2 && a != SNOWBOMB && a != EIERBOMBE)
	{
		const fx sx = p->xPos - cam_xoff(), sy = p->yPos - cam_yoff();
		if (sy > FX(RENDERHEIGHT + 100) || sy + FX(rect.b) < FX(-100) ||
		    sx > FX(RENDERWIDTH + 100) || sx + FX(rect.r) < FX(-100))
			p->Damage = 0;
	}
	/* out of the level? */
	else if (a != ROCKETSPIDER &&
	         (p->yPos > FX(map_h * TILESIZE) || p->xPos > FX(map_w * TILESIZE) || p->yPos < 0 || p->xPos < 0))
		p->Damage = 0;

	if (p->Damage <= 0) return;

	/* a destructible wall hit by a player's shot: it and the shot go */
	if (!p->DamagePlayer && a != BOMBE && a != BOMBEBIG && a != SHIELDSPAWNER && pj_block_destroy(p, &rect))
	{
		if (a != BLITZBEAM && a != SHIELDSPAWNER) { pj_explode(p); return; }
		pj_check_destroyable_walls(p->xPos, p->yPos, p->xSpeed, p->ySpeed, &rect);
	}

	if (p->CheckBlock)
	{
		fx x = p->xPos, y = p->yPos, xo = p->xPosOld, yo = p->yPosOld;
		bo = BlockOben(x, &y, xo, &yo, &rect, 0);
		bu = BlockUntenNormal(x, y, xo, yo, &rect);
		bl = BlockLinks(&x, y, &xo, yo, &rect, 0);
		br = BlockRechts(&x, y, &xo, yo, &rect, 0);
	}

	/* bouncing off the walls */
	if (p->BounceWalls)
	{
		int hit = 0;
		if ((p->ySpeed < 0 && (bo & B_WAND)) || (p->ySpeed > 0 && (bu & B_WAND))) { p->ySpeed = -p->ySpeed; hit = 1; }
		if ((p->xSpeed > 0 && (br & B_WAND)) || (p->xSpeed < 0 && (bl & B_WAND))) { p->xSpeed = -p->xSpeed; hit = 1; }
		if (a == BOUNCESHOT2 || a == BOUNCESHOT3 || a == BOUNCESHOTBIG2 || a == BOUNCESHOTBIG3)
		{
			p->Damage = (int16_t)(p->Damage - hit);
			if (p->Damage <= 0)
			{
				snd(SOUND_BOUNCESHOT);
				pj_explode(p);
				return;
			}
		}
	}

	switch (a)
	{
	case SKELETOR_SHOT:
	case SPREADSHOT:
	case SPREADSHOT2:
	case SPREADSHOTBIG:
	case SPREADSHOTBIG2:
		if (((bu & B_WAND) && p->ySpeed > 0) || (bo & B_WAND) || (bl & B_WAND) || (br & B_WAND))
		{
			snd(SOUND_SPREADHIT);
			pj_explode(p);
		}
		break;

	case LASERSHOT:
	case LASERSHOT2:
	case LASERSHOTBIG:
	case LASERSHOTBIG2:
		if (p->AnimPhase > p->AnimEnde) p->AnimPhase = 0;
		if (pj_wall()) pj_explode(p);
		break;

	case BOUNCESHOT1:
	case BOUNCESHOTBIG1:
		if (pj_wall())
		{
			if (p->BounceWalls)
			{
				p->Damage = (int16_t)(p->Damage - 3);
				if (p->xSpeed == 0 || p->ySpeed == 0) p->ySpeed = p->xSpeed;
				if (p->Damage <= 0) pj_explode(p);
			}
			else pj_explode(p);
		}
		break;

	case BOUNCESHOT2:
	case BOUNCESHOTBIG2:
		if (pl.CurrentWeaponLevel[pl.SelectedWeapon & 3] >= 5 && pj_wall()) pj_explode(p);
		break;

	case POWERLINE:
		if ((p->xSpeed < 0 && (bl & B_WAND)) || (p->xSpeed > 0 && (br & B_WAND))) pj_explode(p);
		break;

	case FLAMME:
		if ((bl & B_WAND) || (br & B_WAND) || (bo & B_WAND) || (bu & (B_WAND | B_PLATTFORM))) pj_explode(p);
		break;

	case CANONBALL:
	case SUCHSCHUSS:
	case SUCHSCHUSS2:
	case WALKER_LASER:
	case SPIDER_LASER:
	case SPIDERSLOW:
	case SPIDERSHOT:
	case EISZAPFENSHOT:
	case UFOLASER:
	case STELZLASER:
	case PFLANZESHOT:
	case BRATKLOPSSHOT:
	case DRONEBULLET:
	case SCHLEIMSHOT:
	case LAFASSSHOT:
	case BLUEBOMB:
	case ROTZSHOT:
		if (a == ROTZSHOT)
		{
			p->Counter -= SYNC(FX(1));
			if (p->Counter <= 0) { p->Counter = FX(0.1); PP(FX(4), 0, PHARAOSMOKE); }
		}
		if (pj_wall()) pj_explode(p);
		break;

	case FIREBALL:
		p->AnimCount -= SYNC(FX(1));
		if (p->AnimCount < 0) { p->AnimCount = FX(0.1); PP(0, FX(10) + R(5), FIREBALL_SMOKE); }
		if (pj_wall()) pj_explode(p);
		break;

	case FIREBALL_BIG:
		p->AnimCount -= SYNC(FX(1));
		if (p->AnimCount < 0)
		{
			p->AnimCount = FX(0.1);
			for (i = 0; i < 4; i++) PP(FX(10) + R(20), FX(10) + R(20), FIREBALL_SMOKE);
		}
		if (pj_wall()) pj_explode(p);
		break;

	case FIREBALL_BOMB:
		p->Winkel += SYNC(FX(100));
		p->AnimCount -= SYNC(FX(1));
		if (p->AnimCount < 0)
		{
			p->AnimCount = FX(1);
			PP(0, 0, SMOKEBIG);
			for (i = 0; i < 5; i++) PP(R(50), R(50), FUNKE);
		}
		if (pj_wall()) pj_explode(p);
		break;

	case SPIDERLASER:
		p->AnimCount += SYNC(FX(2.5));
		if (p->AnimCount > FX(45)) p->AnimSpeed = (int16_t)(p->AnimSpeed - SYNC(FX(1)));
		if (p->AnimSpeed < 0)
		{
			p->AnimSpeed = FX(0.25);
			PP(FX(460), FX(GetRandom(170) - 90), LILA3);
			if (p->AnimPhase > 0)
			{
				p->AnimPhase = 0;
				snd(SOUND_SPIDERLASER);
				snd(SOUND_LILA);
			}
		}
		if (p->AnimCount > FX(140)) pj_explode(p);
		break;

	case SPIDERSHOT2:
		p->AnimCount -= SYNC(FX(1));
		if (p->AnimCount <= 0) { p->AnimCount = FX(0.3); PP(FX(12), FX(12), LILA); }
		if (pj_wall()) pj_explode(p);
		break;

	case TURRIEXTRAWURST:
	case GOLEMSCHUSS:
		if (a == GOLEMSCHUSS)
		{
			if ((p->yAcc > 0 && p->ySpeed > p->Counter) || (p->yAcc < 0 && p->ySpeed < -p->Counter))
				p->yAcc = (int16_t)-p->yAcc;
		}
		else if (p->Counter > 0)
		{
			p->Counter -= SYNC(FX(1));
			if (p->ySpeed >= 0) p->ySpeed = 0;
		}
		else if (p->yAcc != 0)
		{
			p->yAcc = 0;
			pj_aim(p, pl.xpos + FX(35) - p->xPos, pl.ypos + FX(40) - p->yPos, 35);
		}
		if (pj_wall()) pj_explode(p);
		break;

	case BOMBE:
	case BOMBEBIG:
	{
		const int fahrstuhlunten = g_Fahrstuhl_yPos > 0 && p->yPos + FX(rect.b + 4) > g_Fahrstuhl_yPos;
		if (!(bu & (B_WAND | B_PLATTFORM)) && !fahrstuhlunten) p->yAcc = FX(8);
		if (p->ySpeed > FX(40)) p->ySpeed = FX(40);
		if ((p->xSpeed < 0 && (bl & B_WAND)) || (p->xSpeed > 0 && (br & B_WAND))) p->xSpeed = -p->xSpeed;
		if (p->ySpeed > 0 && ((bu & (B_WAND | B_PLATTFORM)) || fahrstuhlunten))
		{
			p->ySpeed = -p->ySpeed * 3 / 4;
			if (bl & B_SCHRAEGE_L) p->xSpeed = FX(10);
			if (br & B_SCHRAEGE_R) p->xSpeed = FX(-10);
		}
		if (p->AnimPhase == p->AnimEnde) pj_explode(p);
	} break;

	case FEUERFALLE:
	case FEUERFALLE_LAVAMANN:
	case SPIDERFIRE:
		p->Winkel += SYNC(FX(10));
		if (p->AnimPhase >= 29) pj_explode(p);
		break;

	case GOLEMSAEULE:
		p->AnimCount -= SYNC(FX(1));
		if (p->AnimCount <= 0)
		{
			p->AnimCount = FX(13);
			snd(SOUND_FEUERFALLE);
			for (i = 0; i < 25; i++) proj_push(p->xPos, p->yPos + FX(i * 5), FEUERFALLE);
		}
		break;

	case ELEKTROSCHUSS:
		p->Counter -= SYNC(FX(100));
		if (p->Counter < FX(750))
		{
			p->xAcc = FX(-6);
			p->yAcc = FX(-5);
			if (p->xSpeed < FX(-25)) p->xSpeed = FX(-25);
			if (p->ySpeed < FX(4)) p->ySpeed = FX(4);
		}
		if (p->Counter < FX(350)) { p->yAcc = FX(7); p->xAcc = FX(1); }
		if (p->Counter <= FX(-150)) pj_explode(p);
		break;

	case ELEKTROPAMPE:
		if (p->AnimPhase > p->AnimEnde) p->AnimPhase = 0;
		p->Counter -= SYNC(FX(70));
		if (p->Counter <= 0) p->Damage = 0;
		break;

	case PLAYERFIRE:
		if (pj_wall()) p->xSpeed = p->ySpeed = p->xAcc = p->yAcc = 0;
		if (p->AnimPhase > 10) p->Counter -= SYNC(FX(50));
		p->Winkel += SYNC(FX(10));
		if (p->AnimPhase >= 29 || p->Counter <= 0) pj_explode(p);
		break;

	case KRABBLERLASER1: if (bu & B_WAND) pj_explode(p); break;
	case KRABBLERLASER2: if (br & B_WAND) pj_explode(p); break;
	case KRABBLERLASER3: if (bl & B_WAND) pj_explode(p); break;

	case PHARAOLASER:
		if (p->ySpeed > FX(20)) p->ySpeed = FX(20);
		if (p->ySpeed < FX(-20)) p->ySpeed = FX(-20);
		if (pl.ypos + FX(40) < p->yPos + FX(24)) p->yAcc = FX(-5);
		else if (pl.ypos + FX(40) > p->yPos + FX(24)) p->yAcc = FX(5);
		PP(FX(18), FX(18), PHARAOSMOKE);
		if ((bu & B_WAND) && p->ySpeed > 0) p->ySpeed = -p->ySpeed;
		if ((bl & B_WAND) || (br & B_WAND) || (bo & B_WAND)) pj_explode(p);
		break;

	case ROCKETSPIDER:
		if (p->AnimPhase == 0) PP(FX(33), FX(70), ROCKETSMOKE);
		else if (p->AnimPhase == 4) PP(FX(33), FX(-2), ROCKETSMOKE);
		p->AnimCount -= SYNC(FX(1));
		if (p->AnimCount <= 0 && p->AnimPhase == 0)
		{
			p->AnimPhase = 4;
			p->ySpeed = -p->ySpeed;
			p->xPos = pl.xpos;
		}
		if (bu & B_WAND) pj_explode(p);
		break;

	case ROCKETWERFER:
		if (p->xSpeed == 0)
		{
			/* at the player's height: off to the side */
			if (p->yPos > pl.ypos + FX(pl.CollideRect.t) && p->yPos < pl.ypos + FX(pl.CollideRect.b))
			{
				p->yAcc = p->ySpeed < 0 ? FX(5) : FX(-5);
				if (p->AnimPhase == 0) { p->xSpeed = FX(-10); p->xAcc = FX(-5); }
				else { p->xSpeed = FX(10); p->xAcc = FX(5); }
				snd(SOUND_ROCKET);
			}
		}
		else
		{
			if (p->AnimPhase == 0) PP(FX(40), 0, ROCKETSMOKE);
			else if (p->AnimPhase == 1) PP(0, 0, ROCKETSMOKE);
			if (p->yAcc < 0 && p->ySpeed < 0) p->ySpeed = p->yAcc = 0;
			if (p->yAcc > 0 && p->ySpeed > 0) p->ySpeed = p->yAcc = 0;
		}
		if (pj_wall()) pj_explode(p);
		break;

	case TORPEDO:
		if (p->yPos + FX(5) < pl.ypos + FX(45)) p->yAcc = FX(2);
		if (p->yPos + FX(5) > pl.ypos + FX(45)) p->yAcc = FX(-2);
		if (p->ySpeed > FX(10)) p->ySpeed = FX(10);
		if (p->ySpeed < FX(-10)) p->ySpeed = FX(-10);
		p->AnimCount -= SYNC(FX(1));
		if (p->AnimCount < 0)
		{
			p->AnimCount += FX(0.5);
			if (p->AnimPhase == 0) PP(FX(30), 0, BUBBLE);
			else if (p->AnimPhase == 1) PP(0, 0, BUBBLE);
		}
		if (pj_wall()) pj_explode(p);
		break;

	case EVILBLITZ:
		PP(FX(10) + R(50), FX(60) + R(20), EVILFUNKE);
		if (p->ySpeed < FX(-45)) pj_explode(p);
		break;

	case EVILBLITZ2:
		if ((bu & B_WAND) && p->ySpeed > 0)
		{
			p->ySpeed = 0;
			p->AnimEnde = 0;
			p->AnimPhase = 0;
			p->AnimCount = FX(255);
			snd(SOUND_BLITZHIT);
		}
		if (p->ySpeed == 0)
		{
			p->AnimCount -= SYNC(FX(40));
			if (p->AnimCount < 0) pj_explode(p);
		}
		break;

	case BLITZBEAM:
		if (p->Damage > 0) p->Counter -= SYNC(FX(1));
		if (p->Counter <= 0)
		{
			p->Counter = FX(0.1);
			PP(FX(p->Damage / 2 + 1 + GetRandom(3)), FX(p->Damage / 2 + 1 + GetRandom(3)), BEAMSMOKE);
			PP(FX(p->Damage / 2 - 2 + GetRandom(3)), FX(p->Damage / 2 - 6 + GetRandom(3)), BEAMSMOKE2);
		}
		{
			fx x = p->xPos, y = p->yPos, xo = p->xPosOld, yo = p->yPosOld;
			bo = BlockOben(x, &y, xo, &yo, &rect, 0);
			bu = BlockUntenNormal(x, y, xo, yo, &rect);
			bl = BlockLinks(&x, y, &xo, yo, &rect, 0);
			br = BlockRechts(&x, y, &xo, yo, &rect, 0);
		}
		if (pj_wall()) pj_explode(p);
		break;

	case SKELETORGRANATE:
		if (bu & B_WAND)
		{
			p->xSpeed = p->ySpeed = 0;
			p->xAcc = p->yAcc = 0;
			p->Winkel = FX(180);
		}
		p->Counter -= SYNC(FX(2));
		if (p->Counter <= 0)
		{
			if (!(bu & B_WAND))
			{
				if (p->xSpeed > 0) PP(FX(-5), FX(-5), SMOKE);
				else PP(FX(-5), 0, SMOKE);
			}
			p->Counter = FX(1);
			p->Damage--;
			if (p->Damage <= 0) pj_explode(p);
		}
		break;

	case FLUGLASER:
	case ARCSHOT:
	case SNOWBOMB:
	case EVILROUND1:
		if (a == ARCSHOT)
		{
			p->AnimCount -= SYNC(FX(1));
			if (p->AnimCount <= 0) { PP(FX(-2), FX(-2), ROCKETSMOKEBLUE); p->AnimCount = FX(0.1); }
		}
		else if (a == EVILROUND1)
		{
			p->AnimCount -= SYNC(FX(1));
			if (p->AnimCount < 0)
			{
				for (i = 0; i < 2; i++) PP(FX(-2), FX(-2), EVILROUNDSMOKE);
				p->AnimCount += FX(0.1);
			}
		}
		if (pj_wall() || (bu & B_PLATTFORM)) pj_explode(p);
		break;

	case SNOWBOMBSMALL:
		if (pj_wall() || (bu & B_PLATTFORM))
		{
			if (p->ySpeed > 0) p->ySpeed = -p->ySpeed * 2 / 3;
			if (p->ySpeed > FX(-0.5)) pj_explode(p);
		}
		break;

	case FETTESPINNESHOT:
	case FETTESPINNESHOT2:
		if (p->AnimPhase > p->AnimEnde) p->AnimPhase = 0;
		if ((bl & B_WAND) || (br & B_WAND)) pj_explode(p);
		break;

	case EIERBOMBE:
		if (p->AnimCount > p->AnimSpeed)
		{
			p->AnimCount -= p->AnimSpeed;
			if (p->xSpeed < 0)
			{
				if (--p->AnimPhase < 0) { p->AnimPhase = 18; p->AnimEnde = 19; }
			}
			else if (++p->AnimPhase > p->AnimEnde) { p->AnimPhase = 0; p->AnimEnde = 19; }
		}
		if (p->ySpeed > FX(30)) p->ySpeed = FX(30);
		if (!(bu & (B_WAND | B_PLATTFORM)) && p->ySpeed == 0) p->yAcc = FX(4);
		if (p->ySpeed > 0 && (bu & (B_WAND | B_PLATTFORM)))
		{
			p->ySpeed = -p->ySpeed * 3 / 4;
			if (p->ySpeed > FX(-1)) { p->ySpeed = 0; p->yAcc = 0; }
		}
		if ((bl & B_WAND) || (br & B_WAND)) pj_explode(p);
		break;

	case SMARTBOMB:
		p->AnimCount += SYNC(FX(32));
		p->xPos -= SYNC(FX(32));
		p->yPos -= SYNC(FX(32));
		if (p->AnimCount > FX(256)) p->AnimCount = FX(256);
		p->Damage = (int16_t)(FINT(FX(255) - p->AnimCount) * 2);
		break;

	case GRENADE:
		p->AnimCount -= SYNC(FX(1));
		while (p->AnimCount <= 0)
		{
			p->AnimCount += FX(0.1);
			PP(FX(-4), FX(-4) + R(3), SMOKE2);
		}
		if (pj_wall()) pj_explode(p);
		break;

	case SHIELDSPAWNER:
		/* round the player (ySpeed: the angle, xSpeed: the particle counter) */
		if (pl.Blickrichtung == -1) p->ySpeed += SYNC(FX(28.6479));
		else p->ySpeed -= SYNC(FX(28.6479));
		if (p->ySpeed > FX(360)) p->ySpeed -= FX(360);
		if (p->ySpeed < 0) p->ySpeed += FX(360);
		{
			const int ang = FINT(p->ySpeed);
			if (pl.Handlung != PA_RADELN && pl.Handlung != PA_RADELN_FALL)
			{
				p->xPos = pl.xpos + FX(28) + sin_d(ang) * 40;
				p->yPos = pl.ypos + FX(38) + cos_d(ang) * 40;
			}
			else
			{
				p->xPos = pl.xpos + FX(28) + sin_d(ang) * 30;
				p->yPos = pl.ypos + FX(55) + cos_d(ang) * 30;
			}
		}
		p->xSpeed -= SYNC(FX(1));
		if (p->xSpeed < 0) { p->xSpeed = FX(0.1); PP(0, 0, SHIELD); }
		if (pl.Shield <= 0) pj_explode(p);
		break;

	default:
		break;
	}

	/* an enemy's shot hit the player? */
	if (p->Damage > 0 && p->DamagePlayer && pl.Handlung != PA_TOT && pl.Handlung != PA_EXPLODIEREN)
	{
		const Box r2 = pj_rect(p);
		if (SpriteCollision(p->xPos, p->yPos, &r2, pl.xpos, pl.ypos, &pl.CollideRect))
		{
			if (!p->ExplodeOnImpact) DamagePlayer(SYNC(FX(p->Damage)), 0);
			else
			{
				DamagePlayer(FX(p->Damage), 0);
				pj_explode(p);
			}
		}
	}

	p->xPosOld = p->xPos;
	p->yPosOld = p->yPos;
}

/* ------------------------------------------------------------------ CheckCollision */
static uint8_t pj_cand[MAX_LIVE];     /* this step's enemies that can be hit */
static int pj_ncand, pj_schleuse;

static void pj_collide(ProjectileClass *p)
{
	const int a = p->ShotArt;
	if (a == BOMBE) return;
	/* an enemy's shot hits only Schleusen; the wall flames burn the enemies as well */
	if (p->DamagePlayer && a != FEUERFALLE && !pj_schleuse) return;
	const Box r = pj_rect(p);
	for (int c = 0; c < pj_ncand && p->Damage > 0; c++)
	{
		Ent *e = &ents[pj_cand[c]];
		const int art = e->GegnerArt;
		if (art < 0 || e->Energy <= 0) continue;
		if (art == POWERBLOCK && a == SHIELDSPAWNER) continue;
		if (p->DamagePlayer && !(a == FEUERFALLE && art != LAVAMANN) && art != SCHLEUSEH && art != SCHLEUSEV) continue;
		if (!SpriteCollision(p->xPos, p->yPos, &r, e->xPos, e->yPos, &GegnerRect[art])) continue;

		if (e->DamageTaken <= 0) e->DamageTaken = FX(255);
		/* the bosses lose less from grenades, powerlines and smart bombs */
		const int boss = (a == GRENADE || a == POWERLINE || a == SMARTBOMB) && art >= STAHLFAUST && art < PARTIKELSPAWN;
		if (p->ExplodeOnImpact)
		{
			e->Energy -= boss ? FX(p->Damage) * 7 / 10 : FX(p->Damage);
			if (a == POWERLINE && e->Energy < FX(40)) e->Energy = 0;
			snd(SOUND_HIT + e->HitSound);
			p->Damage = 0;
			pj_explode(p);
		}
		else
		{
			e->Energy -= SYNC(boss ? FX(p->Damage) * 7 / 10 : FX(p->Damage));
			snd(SOUND_HIT + e->HitSound);
		}

		if (art == SCHNEEKOPPE)
		{
			if (a != SMARTBOMB) p->Damage = 0;
			if (e->LastAction == -1 && (a == SMARTBOMB || FABS(p->xSpeed) >= FABS(p->ySpeed)))
			{
				e->xSpeed = -e->xSpeed * 3 / 4;
				e->LastAction = 0;
				if (p->ySpeed > 0) e->ySpeed += FX(8);
				if (p->ySpeed < 0) e->ySpeed -= FX(8);
			}
		}
		if (a != SMARTBOMB && (art == SKULL || art == PUNISHER))
		{
			const fx xs = p->xSpeed, ys = p->ySpeed;
			if (a != DIAMONDSHOT) { e->xSpeed += xs / 20; e->ySpeed += ys / 20; }
			else { e->xSpeed += xs / 5; e->ySpeed += ys / 5; }
			if (a != PLAYERFIRE && p->Damage > 0) { p->Damage = 0; pj_explode(p); }
		}
		/* (pAim: one player) */
		if (e->TimeToChange <= 0) e->TimeToChange = FX(30);
	}
}

/* Render's animation (Hurrican advances it while drawing) */
static void pj_animate(ProjectileClass *p)
{
	if (p->ShotArt == SPIDERLASER && p->AnimCount >= FX(45)) p->Damage = 500;
	if (p->ShotArt != EIERBOMBE && p->AnimEnde > 0 && p->AnimCount > p->AnimSpeed)
	{
		p->AnimCount -= p->AnimSpeed;
		if (++p->AnimPhase > p->AnimEnde) p->AnimPhase = 0;
	}
}

#ifndef OJ32X_MARS
static int pj_test_art = -2, pj_test_every = 30, pj_test_dx = 60, pj_test_dy = 20, pj_test_t, pj_test_w = 90;
static void pj_test_hook(void)
{
	if (pj_test_art == -2)
	{
		pj_test_art = -1;
		const char *s = getenv("HB_PROJ");
		if (s)
		{
			int lvl = -1;
			sscanf(s, "%d,%d,%d,%d,%d", &pj_test_art, &pj_test_every, &pj_test_dx, &pj_test_dy, &lvl);
			if (lvl >= 0) for (int i = 0; i < 4; i++) pl.CurrentWeaponLevel[i] = lvl;
			if (getenv("HB_PROJW")) pl.SelectedWeapon = atoi(getenv("HB_PROJW"));
			if (getenv("HB_PROJA")) pj_test_w = atoi(getenv("HB_PROJA"));
			if (pj_test_every < 1) pj_test_every = 1;
		}
		if (getenv("HB_PROJFIND"))             /* list the destructible tiles */
			for (int y = 0, n = 0; y < map_h; y++)
				for (int x = 0; x < map_w && n < 40; x++)
					if (TileBlock(x, y) & B_DESTRUCTIBLE) { printf("destructible %d,%d\n", x * TILESIZE, y * TILESIZE); n++; }
	}
	if (pj_test_art < 0 || pl.Handlung == PA_TOT) return;
	if (++pj_test_t % pj_test_every) return;
	proj_winkel = FX(pj_test_w);
	if (pj_test_art == BLITZBEAM) proj_blitzbeam(80, pl.Blickrichtung == 1 ? FX(270) : FX(90));
	else if (pj_test_art == SHIELDSPAWNER) { pl.Shield = FX(100); proj_push(pl.xpos, pl.ypos, SHIELDSPAWNER); proj_push(pl.xpos, pl.ypos, SHIELDSPAWNER2); pj_test_art = -1; }
	else proj_push(pl.xpos + FX(pj_test_dx), pl.ypos + FX(pj_test_dy), pj_test_art);
}
#endif

/* ------------------------------------------------------------------ DoProjectiles: one step */
static void proj_step_all(void)
{
#ifndef OJ32X_MARS
	pj_test_hook();
#endif
	if (pj_nfresh)
	{
		for (int i = 0; i < PJ_MAX; i++) pj[i].fresh = 0;
		pj_nfresh = 0;
	}
	/* the enemies that shots can hit */
	pj_ncand = 0;
	pj_schleuse = 0;
	for (int k = 0; k < MAX_LIVE; k++)
	{
		const Ent *e = &ents[k];
		if (e->GegnerArt < 0 || !e->Active || !e->Destroyable || e->Energy <= 0) continue;
		pj_cand[pj_ncand++] = (uint8_t)k;
		if (e->GegnerArt == SCHLEUSEH || e->GegnerArt == SCHLEUSEV) pj_schleuse = 1;
	}
	pj_in_step = 1;
	for (int i = 0; i < PJ_MAX; i++)
	{
		ProjectileClass *p = &pj[i];
		if (p->ShotArt < 0 || p->fresh) continue;
		if (p->ShotArt == SHIELDSPAWNER)
		{
			/* its speeds are an angle and a counter: Run must not move it by them */
			p->xPos -= SYNC(p->xSpeed);
			p->yPos -= SYNC(p->ySpeed);
		}
		pj_run(p);
		if (p->Damage > 0)
		{
			pj_animate(p);
			pj_collide(p);
		}
		if (p->Damage <= 0) p->ShotArt = -1;
	}
	pj_in_step = 0;
}

/* ------------------------------------------------------------------ Render */
/* a thin rotated shot (its picture points up at Winkel 0): its frame when it flies (nearly)
 * up or down, else lines along its flight */
static void pj_draw_thin(const ProjectileClass *p, const Pic *pc, int half_w, int c_side, int c_mid)
{
	int W = FINT(p->Winkel) % 360;
	if (W < 0) W += 360;
	if (W <= 8 || W >= 352) { draw_sprite(pc, p->AnimPhase, p->xPos, p->yPos, 0); return; }
	if (W >= 172 && W <= 188) { draw_sprite_flipv(pc, p->AnimPhase, p->xPos, p->yPos, 0); return; }
	const int w = W - 90;
	const fx cx = p->xPos + FX(pc->fw), cy = p->yPos + FX(pc->fh);
	const int L = pc->fh;                         /* half the length (full-size pixels) */
	const fx dx = cos_d(w) * L, dy = sin_d(w) * L;
	for (int k = -half_w; k <= half_w; k++)
	{
		const fx ox = -sin_d(w) * 2 * k, oy = cos_d(w) * 2 * k;
		draw_line(cx - dx + ox, cy - dy + oy, cx + dx + ox, cy + dy + oy, k == 0 ? c_mid : c_side);
	}
}

/* a flame scaled to `s` pixels in Hurrican, centred at (cx, cy) */
static void pj_draw_flame(const Pic *pc, int frame, fx cx, fx cy, int s)
{
	if (s < 52 && pj_flamme.count)
		draw_sprite(&pj_flamme, frame * pj_flamme.count / 30, cx - FX(pj_flamme.fw), cy - FX(pj_flamme.fh), 0);
	else
		draw_sprite(pc, frame, cx - FX(pc->fw), cy - FX(pc->fh), 0);
}

static void proj_draw_all(void)
{
	for (int i = 0; i < PJ_MAX; i++)
	{
		const ProjectileClass *p = &pj[i];
		if (p->ShotArt < 0 || p->fresh || p->Damage <= 0) continue;
		const int a = p->ShotArt;
		const Pic *pc = pj_pic_of(a);
		if (a == SMARTBOMB)
		{
			/* the pressure wave: a circle */
			const int rad = FINT(p->AnimCount);
			const fx cx = p->xPos + FX(rad), cy = p->yPos + FX(rad);
			fx lx = cx + FX(rad), ly = cy;
			for (int d = 15; d <= 360; d += 15)
			{
				const fx nx = cx + cos_d(d) * rad, ny = cy + sin_d(d) * rad;
				draw_line(lx, ly, nx, ny, col_light);
				lx = nx; ly = ny;
			}
			continue;
		}
		if (a == SPIDERLASER)
		{
			if (p->AnimCount < FX(45))
			{
				for (int k = -1; k <= 1; k++)
					draw_line(p->xPos - FX(10), p->yPos + FX(2 * k), p->xPos + FX(470), p->yPos + FX(2 * k), k ? col_red : col_light);
			}
			else if (pc && pc->count)
				for (int k = 0; k < 560; k += pc->fw * 2)
					draw_sprite(pc, 0, p->xPos - FX(10) + FX(k), p->yPos - FX(pc->fh), 0);
			continue;
		}
		if (!pc || !pc->count) continue;
		switch (a)
		{
		case BOUNCESHOT1:
			draw_sprite(pc, 0, p->xPos, p->yPos, 0);
			break;
		case FEUERFALLE:
		case SPIDERFIRE:
		case PLAYERFIRE:
		{
			const int A = p->AnimPhase, s = A * 22 / 10 + 20;
			pj_draw_flame(pc, A, p->xPos + FX(46 - (A + 30) + s / 2), p->yPos + FX(54 - (A + 30) + s / 2), s);
		} break;
		case FEUERFALLE_LAVAMANN:
		{
			const int A = 30 - p->AnimPhase, s = A * 22 / 10 + 20;
			pj_draw_flame(pc, p->AnimPhase, p->xPos + FX(46 - (A + 30) + s / 2), p->yPos + FX(54 - (A + 30) + s / 2), s);
		} break;
		case BLITZBEAM:
			draw_sprite(pc, p->AnimPhase, p->xPos + FX(p->Damage / 2 - pc->fw), p->yPos + FX(p->Damage / 2 - pc->fh), 0);
			break;
		case LASERSHOT:
		case LASERSHOT2:
			pj_draw_thin(p, pc, a == LASERSHOT ? 1 : 2, col_blue, col_light);
			break;
		case LASERSHOTBIG:
		case LASERSHOTBIG2:
			pj_draw_thin(p, pc, a == LASERSHOTBIG ? 2 : 3, col_blue, col_light);
			break;
		case SKELETOR_SHOT:
			pj_draw_thin(p, pc, 0, col_yellow, col_yellow);
			break;
		case FLUGLASER:
			pj_draw_thin(p, pc, 1, col_red, col_red);
			break;
		case EISZAPFENSHOT:
			pj_draw_thin(p, pc, 1, col_blue, col_light);
			break;
		case FIREBALL:
		{
			/* points up at Winkel 0: upside down when it flies down */
			int W = FINT(p->Winkel) % 360;
			if (W < 0) W += 360;
			if (W > 90 && W < 270) draw_sprite_flipv(pc, p->AnimPhase, p->xPos, p->yPos, 0);
			else draw_sprite(pc, p->AnimPhase, p->xPos, p->yPos, 0);
		} break;
		default:
			draw_sprite(pc, p->AnimPhase, p->xPos, p->yPos, 0);
			break;
		}
	}
}
