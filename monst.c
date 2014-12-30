#include <u.h>
#include <libc.h>
#include <draw.h>

#include "dat.h"

Monster*
monst(int idx)
{
	Monster *m;
	MonsterData *md;

	if(idx < 0 || idx > nelem(monstdata))
		goto missing;

	md = &monstdata[idx];
	if(md->name == nil)
		goto missing;

	m = mallocz(sizeof(Monster), 1);
	if(m == nil)
		return nil;

	m->md = md;
	m->hp = md->maxhp;

	return m;

missing:
	werrstr("missing monster %d\n", idx);
	return nil;
}

