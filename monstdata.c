#include <u.h>
#include <libc.h>
#include <draw.h>

#include "dat.h"

MonsterData monstdata[392] = {
[TLARGECAT]		{"large cat", 5, 2, 1},
[TGWIZARD]		{"gnomish wizard", 1, 2, 1},
[TSOLDIER]		{"soldier", 4, 2, 1},
[TSERGEANT]		{"sergeant", 8, 2, 1},
[TLIEUTENANT]	{"lieutenant", 16, 2, 2},
[TCAPTAIN]		{"captain", 32, 2, 3},
[TWIZARD]		{"wizard", 64, 5, 3},
};

