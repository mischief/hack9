#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "dat.h"

MonsterData monstdata[1200] = {
				/* name, align, hp, ac, atk, rolls */
[TLARGECAT]		{"large cat", 1, 8, 8, 2, 1},
[TGWIZARD]		{"gnomish wizard", 50, 8, 6, 2, 1},
[TLICH]			{"lich", -50, 16, 4, 2, 2},
[TSOLDIER]		{"soldier", -20, 8, 8, 2, 2},
[TSERGEANT]		{"sergeant", -20, 16, 6, 5, 1},
[TLIEUTENANT]	{"lieutenant", -20, 32, 4, 3, 2},
[TCAPTAIN]		{"captain", -20, 64, 2, 3, 3},
[TGHOST]		{"ghost", -50, 8, -10, 1, 2},
[TWIZARD]		{"wizard", 20, 64, -1, 2, 4},
};

