#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "dat.h"

MonsterData monstdata[1200] = {
				/* name,		align, mvr, hp, ac, atk, rolls */
[TLARGECAT]		{"large cat",		1, 12, 8, 8, 2, 1},
[TGWIZARD]		{"gnomish wizard",	50, 12, 8, 6, 2, 1},
[TLICH]			{"lich",			-50, 16, 16, 4, 2, 2},
[TSOLDIER]		{"soldier",			-20, 12, 8, 8, 2, 2},
[TSERGEANT]		{"sergeant",		-20, 13, 16, 6, 5, 1},
[TLIEUTENANT]	{"lieutenant",		-20, 14, 32, 4, 3, 2},
[TCAPTAIN]		{"captain",			-20, 15, 64, 2, 3, 3},
[TGHOST]		{"ghost",			-50, 6, 8, -10, 1, 2},
[TWIZARD]		{"wizard",			20, 16, 64, -1, 2, 4},
};

