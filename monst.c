#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

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
	m->ac = md->def;
	m->align = md->align;

	incref(m);

	return m;

missing:
	werrstr("missing monster %d", idx);
	return nil;
}

void
mfree(Monster *m)
{
	AIState *a;

	if(decref(m) != 0)
		return;

	if(m->aglobal != nil){
		m->aglobal->exit(m->aglobal);
		freestate(m->aglobal);
	}
	if(m->ai != nil){
		while((a = mpopstate(m)) != nil)
			freestate(a);
	}

	free(m);
}

int
mupdate(Monster *m)
{
	if(m->aglobal != nil)
		m->aglobal->exec(m->aglobal);
	if(m->ai != nil){
		dbg("the %s is %sing...", m->md->name, m->ai->name);
		m->ai->exec(m->ai);
	}

	return 0;
}

void
mpushstate(Monster *m, AIState *a)
{
	assert(a != nil);
	a->prev = m->ai;
	m->ai = a;
	if(a->enter != nil)
		a->enter(a);
}

AIState*
mpopstate(Monster *m)
{
	AIState *a;

	if(m->ai == nil)
		return nil;

	a = m->ai;
	if(a->exit != nil)
		a->exit(a);
	m->ai = a->prev;

	return a;
}

static int
mattack(Monster *m, Monster *mt)
{
	int hit, dmg;
	hit = roll(1, 20);
	if(hit > 10+mt->ac){
		warn("the %s misses!", m->md->name);
		return 0;
	}

	dmg = 1+roll(m->md->atk, m->md->rolls);
	warn("the %s hits for %d!", m->md->name, dmg);
	mt->hp -= dmg;
	if(mt->hp < 1){
		mt->flags = Mdead;
		bad("the %s is killed!", mt->md->name);
		return 1;
	}

	return 0;
}

int
maction(Monster *m, int what, Point where)
{
	Tile *cur, *targ;

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
			if(mattack(m, targ->monst) > 0){
				clrflagat(m->l, where, Fhasmonster|Fblocked);
				mfree(targ->monst);
				targ->monst = nil;
				targ->unit = 0;
			}
			return 0;
		} else if(!hasflagat(m->l, where, Fblocked)){
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

