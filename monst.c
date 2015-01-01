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
	werrstr("missing monster %d", idx);
	return nil;
}

int
mupdate(Monster *m)
{
	if(m->aglobal != nil)
		m->aglobal->exec(m);
	fprint(2, "updating '%s' @ %P\n", m->md->name, m->pt);
	if(m->acur != nil)
		m->acur->exec(m);

	return 0;
}

void
mchangestate(Monster *m, AIState *a)
{
	assert(a != nil);
	m->aprev = m->acur;
	if(m->acur != nil)
		m->acur->exit(m);
	m->acur = a;
	m->acur->enter(m);
}

void
mrevertstate(Monster *m)
{
	mchangestate(m, m->aprev);
}

int
maction(Monster *m, int what, Point where)
{
	int dmg;
	Tile *cur, *targ;
	Monster *mtarg;

	switch(what){
	default:
		fprint(2, "unknown move %d\n", what);
	case MNONE:
		/* do nothing */
		return 0;
		break;
	case MMOVE:
		targ = tileat(m->l, where);
		if(hasflagat(m->l, where, Fhasmonster) && targ->monst != nil){
			/* monster; attack it */
			mtarg = targ->monst;
			dmg = roll(m->md->atk, m->md->rolls) - mtarg->md->def;
			if(dmg <= 0)
				warn("the %s misses!", m->md->name);
			else {
				warn("the %s hits for %d!", m->md->name, dmg);
				mtarg->hp -= dmg;
				if(mtarg->hp < 1){
					dbg("the %s is killed!", mtarg->md->name);
					clrflagat(m->l, where, Fhasmonster|Fblocked);
					free(mtarg);
					targ->monst = nil;
					targ->unit = 0;
				}
			}

			/* no movement */
			return 0;
		}
		if(!hasflagat(m->l, where, Fblocked)){
			/* move */
			cur = tileat(m->l, m->pt);
			setflagat(m->l, where, (Fhasmonster|Fblocked));
			clrflagat(m->l, m->pt, (Fhasmonster|Fblocked));
			targ->unit = cur->unit;
			cur->unit = 0;
			targ->monst = cur->monst;
			cur->monst = nil;

			m->pt = where;
			return 1;
		}
		break;
	case MUSE:
		/* portal */
/*
		t = tileat(level, src);
		what = t->unit;
		if(t->feat == (dir==UP ? TUPSTAIR : TDOWNSTAIR)){
			m = t->monst;
			t->monst = nil;
			freelevel(level);
			if((level = genlevel(nrand(LSIZE)+LSIZE, nrand(LSIZE)+LSIZE)) == nil)
				sysfatal("genlevel: %r");

			if(dir == UP)
				dst = level->down;
			else
				dst = level->up;

			t = tileat(level, dst);
			t->unit = what;
			t->monst = m;
			setflagat(level, dst, Fhasmonster|Fblocked);
			return subpt(src, dst);
		}
*/
		return -1;
		break;
	}

	/* bad move. */
	return -1;
}

