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
	int hit, crit, dmg;
	hit = roll(1, 100);
	if(hit < 10+abs(mt->ac-10)*2){
		warn("the %s misses the %s!", m->md->name, mt->md->name);
		return 0;
	}

	dmg = roll(m->md->atk, m->md->rolls);

	if(dmg && mt->ac < 0){
		dmg -= 1+nrand(-mt->ac);
		if(dmg < 1)
			dmg = 1;
	}

	crit = roll(1, 5);
	if(crit == 1){
		dmg *= 2;
		warn("the %s critically strikes the %s for %d!", m->md->name, mt->md->name, dmg);
	} else {
		warn("the %s hits the %s for %d!", m->md->name, mt->md->name, dmg);
	}

	mt->hp -= dmg;

	if(mt->hp < 1){
		mt->flags = Mdead;
		m->kills++;
		bad("the %s kills the %s!", m->md->name, mt->md->name);
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
		cur = tileat(m->l, m->pt);
		what = cur->unit;
		if(cur->portal != nil){
			clrflagat(m->l, m->pt, (Fhasmonster|Fblocked));
			m = cur->monst;
			cur->monst = nil;
			if(cur->portal->to == nil){
				if((cur->portal->to = genlevel(nrand(20)+20, nrand(20)+20, nrand(3)+1)) == nil)
					sysfatal("genlevel: %r");

				cur->portal->pt = cur->portal->to->up;
				targ = tileat(cur->portal->to, cur->portal->pt);
				if(targ->portal != nil){
					targ->portal->to = m->l;
					targ->portal->pt = m->pt;
				}
			}

			targ = tileat(cur->portal->to, cur->portal->pt);
			targ->unit = what;
			targ->monst = m;
			m->l = cur->portal->to;
			m->pt = cur->portal->pt;
			setflagat(cur->portal->to, cur->portal->pt, Fhasmonster|Fblocked);
			return 1;
		}
		break;
	case MSPECIAL:
		break;
	}

	/* bad move. */
	return -1;
}

