/*
 * Group B4's PC test hook (not in the console build): HB_B4OBJ="id,x,y,value1,value2" adds
 * one object to the stage's map objects, as if the map had it (for the bosses that are in
 * none of the 9 maps: EVILHURRI, BRATKLOPS, EISFAUST). Called from a B4 type's load function,
 * after OBJECTS.BIN is read.
 */
#ifndef OJ32X_MARS
static void b4t_inject(void)
{
	static uint8_t *buf;
	const char *s = getenv("HB_B4OBJ");
	int id, x, y, v1, v2;
	if (!s || sscanf(s, "%d,%d,%d,%d,%d", &id, &x, &y, &v1, &v2) != 5 || nobjs + 1 > MAX_OBJS) return;
	free(buf);
	buf = (uint8_t *)malloc(12 * (size_t)(nobjs + 1));
	if (!buf) return;
	int k = 0;
	while (k < nobjs && OBJ_X(k) < x) k++;
	memcpy(buf, objs, 12 * (size_t)k);
	const int vals[6] = { id, x, y, v1, v2, 0 };
	for (int i = 0; i < 6; i++) { buf[12 * k + 2 * i] = (uint8_t)(vals[i] >> 8); buf[12 * k + 2 * i + 1] = (uint8_t)vals[i]; }
	memcpy(buf + 12 * (k + 1), objs + 12 * k, 12 * (size_t)(nobjs - k));
	objs = buf;
	nobjs++;
}
#endif
