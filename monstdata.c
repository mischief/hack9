#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "dat.h"

MonsterData monstdata[392] = {
				/* name, align, hp, ac, atk, rolls */
[TLARGECAT]		{"large cat", 0, 5, 0, 2, 1},
[TGWIZARD]		{"gnomish wizard", -10, 1, 1, 2, 1},
[TSOLDIER]		{"soldier", -20, 4, 2, 5, 1},
[TSERGEANT]		{"sergeant", -20, 8, 2, 5, 1},
[TLIEUTENANT]	{"lieutenant", -20, 16, 3, 5, 1},
[TCAPTAIN]		{"captain", -20, 32, 4, 5, 2},
[TWIZARD]		{"wizard", 20, 64, 2, 5, 3},
};

