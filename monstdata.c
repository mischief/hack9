#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "dat.h"

enum
{
	UNALIGNED	= 0,
	GNOME		= 50,
	UNDEAD		= -50,
	HUMAN		= -20,

};

MonsterData monstdata[1200] = {
				/* name,			xpl,align,	mvr, hp, ac, rolls, atk */
[TLARGECAT]		{"large cat",		0,	UNALIGNED,	16,	16, 8, 1, 3},
[TGNOME]		{"gnome",			1,	GNOME,		6,	16, 10, 1, 6},
[TGNOMELORD]	{"gnome lord",		3,	GNOME,		8,	32, 10, 1, 8},
[TGWIZARD]		{"gnome wizard",	3,	GNOME,		10,	32, 4, 1, 6},
[TGNOMEKING]	{"gnome king",		5,	GNOME,		10,	48, 10, 2, 6},
[TLICH]			{"lich",			2,	UNDEAD,		6,	32, 3, 3, 2},
[TSOLDIER]		{"soldier",			6,	HUMAN,		10,	16, 10, 1, 8},
[TSERGEANT]		{"sergeant",		8,	HUMAN,		10,	16, 10, 2, 6},
[TLIEUTENANT]	{"lieutenant",		10,	HUMAN,		10,	32, 10, 3, 4},
[TCAPTAIN]		{"captain",			12,	HUMAN,		10,	48, 10, 4, 4},
[TGHOST]		{"ghost",			1,	UNDEAD,		6,	16, -5, 1, 4},
[TWIZARD]		{"wizard",			0,	127,		14, 64, 5, 2, 3},
};

