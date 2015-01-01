#include <u.h>
#include <libc.h>
#include <draw.h>

#include "dat.h"

MonsterData monstdata[392] = {
[TLARGECAT]		{"large cat", 5, 1},
[TGWIZARD]		{"gnomish wizard", 1, 1},
[TSOLDIER]		{"soldier", 4, 1},
[TSERGEANT]		{"sergeant", 8, 1},
[TLIEUTENANT]	{"lieutenant", 16, 1},
[TCAPTAIN]		{"captain", 32, 1},
[TWIZARD]		{"wizard", 100, 2},
};

