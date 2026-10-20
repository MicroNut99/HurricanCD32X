/*
 * WARNING - the "warning" signal (triggers/Trigger_Warning.cpp): sits on the screen's lower
 * middle, counts down three blinks, then goes. Its picture is drawn by Hurrican's Gameplay.cpp
 * (not by the object: its DoDraw is empty), the engine has no such drawing - nothing is shown.
 * Own members: f[0] = WarningCount.
 */
static void warning_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(100);
	e->Value2 = 0;
	e->Destroyable = 0;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	e->f[0] = FX(100);
}

static void warning_ki(Ent *e)
{
	e->xPos = cam_xoff() + FX(230);
	e->yPos = cam_yoff() + FX(400);
	e->f[0] -= SYNC(FX(8));
	if (e->f[0] <= 0)
	{
		e->Value2++;
		if (e->Value2 < 3) e->f[0] = FX(100);
		else
		{
			e->f[0] = 0;
			e->Energy = 0;
		}
	}
}

static void warning_explode(Ent *e) { gegner_default_explode(e); }

TYPEDEF(WARNING) = { warning_init, warning_ki, warning_explode, 0, 0 };
