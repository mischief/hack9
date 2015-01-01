#include <u.h>
#include <libc.h>
#include <draw.h>

#include "dat.h"

MonsterData monstdata[392] = {
[TLARGECAT]		{"large cat", 5, 0, 2, 1},
[TGWIZARD]		{"gnomish wizard", 1, 1, 2, 1},
[TSOLDIER]		{"soldier", 4, 2, 2, 1},
[TSERGEANT]		{"sergeant", 8, 2, 2, 1},
[TLIEUTENANT]	{"lieutenant", 16, 3, 2, 2},
[TCAPTAIN]		{"captain", 32, 4, 2, 3},
[TWIZARD]		{"wizard", 64, 4, 5, 3},
};

