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

	m->type = idx;
	m->align = md->align;
	m->mvr = md->mvr;
	m->mvp = nrand(m->mvr);
	m->hp = md->maxhp;
	m->maxhp = md->maxhp;
	m->ac = md->def;
	m->xpl = md->basexpl;
	m->md = md;

	incref(&m->ref);
	return m;

missing:
	werrstr("missing monster %d", idx);
	return nil;
}

void
mfree(Monster *m)
{
	AIState *a;

	if(decref(&m->ref) != 0)
		return;

	if(m->aglobal != nil){
		if(m->aglobal->exit != nil)
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
	m->mvp += m->mvr;

	while(m->mvp > 12){
		if(m->aglobal != nil)
			m->aglobal->exec(m->aglobal);
		if(m->ai != nil){
			//dbg("the %s is %sing...", m->md->name, m->ai->name);
			if(m->ai->exec != nil)
				m->ai->exec(m->ai);
		} else {
			//dbg("the %s is %sing...", m->md->name, m->aglobal->name);
		}

		m->mvp -= 12;
		m->turns++;

		/* gain some hp if you aren't dead. */
		if((m->flags & Mdead) == 0)
		if(m->hp < m->maxhp){
			/* below half, 2%. above, 0.5%. */
			if(m->hp < m->maxhp/2)
				m->hp += 1.0 + (double)m->maxhp/50.0;
			else
				m->hp += (double)m->maxhp/200.0;
			if(m->hp > m->maxhp)
				m->hp = m->maxhp;
		}
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

/* determines what happens when a monster gets some xp */
static void
maddxp(Monster *m, long xp)
{
	long hpgain, last;

	dbg("%s get %ld xp total %ld next %ld", m->md->name, xp, m->xp, xpcalc(m->xpl));

	last = m->xpl;
	m->xp += xp;
	while(m->xp >= xpcalc(m->xpl)){
		m->xpl++;
		hpgain = 1 + roll(1, 8);
		m->maxhp += hpgain;
		m->hp += hpgain*2;
	}
	
	if(m->xpl > last)
		if(nearyou(m->pt))
			good("the %s becomes level %d!", m->md->name, m->xpl);
}

static int
mattack(Monster *m, Monster *mt)
{
	int hit, crit, dmg, xp;
	hit = roll(1, 100);
	if(hit < 10+abs(mt->ac-10)*2){
		if(nearyou(m->pt))
			warn("the %s misses the %s!", m->md->name, mt->md->name);
		return 0;
	}

	dmg = roll(m->md->rolls, m->md->atk);

	if(dmg && mt->ac < 0){
		dmg -= 1+nrand(-mt->ac);
		if(dmg < 1)
			dmg = 1;
	}

	crit = roll(1, 5);
	if(crit == 1){
		dmg *= 2;
		if(nearyou(m->pt))
			warn("the %s critically strikes the %s for %d!", m->md->name, mt->md->name, dmg);
	} else {
		if(nearyou(m->pt))
			msg("the %s hits the %s for %d.", m->md->name, mt->md->name, dmg);
	}

	mt->hp -= dmg;

	if(mt->hp <= 0){
		mt->hp = 0;
		mt->flags = Mdead;
		m->kills++;

		xp = 10+10*pow(mt->xpl, 1.75);

		if(nearyou(m->pt))
			bad("the %s kills the %s, and gains %ld xp.", m->md->name, mt->md->name, xp);
		
		/* gain some xp */
		maddxp(m, xp);

		/* gain 1/4 your target's max hp */
		if(m->hp < m->maxhp){
			m->hp += (double)mt->maxhp/4.0;
			if(m->hp > m->maxhp)
				m->hp = m->maxhp;
		}

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
		if(hasflagat(m->l, where, Fhasmonster) && targ->monst != nil && (targ->monst->flags & Mdead) == 0){
			/* monster; attack it */
			if(mattack(m, targ->monst) > 0){
				/* dead. */
				clrflagat(m->l, where, Fhasmonster);
				mfree(targ->monst);
				targ->monst = nil;
				targ->unit = 0;
			}
			return 0;
		} else if(!hasflagat(m->l, where, Fblocked|Fhasmonster)){
			/* move */
			cur = tileat(m->l, m->pt);
			setflagat(m->l, where, (Fhasmonster));
			clrflagat(m->l, m->pt, (Fhasmonster));
			targ->unit = cur->unit;
			cur->unit = 0;
			targ->monst = cur->monst;
			cur->monst = nil;

			m->pt = where;

			if(isyou(m) && hasflagat(m->l, where, Fportal)){
				warn("you see a %s here.", targ->portal->name);
			}

			return 1;
		}
		break;
	case MUSE:
		/* portal */
		cur = tileat(m->l, m->pt);
		what = cur->unit;
		if(cur->portal != nil){
			clrflagat(m->l, m->pt, (Fhasmonster));
			m = cur->monst;
			cur->monst = nil;
			if(cur->portal->to == nil){
				if((cur->portal->to = genlevel(nrand(20)+50, nrand(20)+10, nrand(3)+1)) == nil)
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
			setflagat(cur->portal->to, cur->portal->pt, Fhasmonster);
			return 1;
		}
		break;
	case MSPECIAL:
		bad("the special move is broken. please fix me.");
		break;
	}

	/* bad move. */
	return -1;
}

/* return the amount of xp required to gain a level */
long
xpcalc(long level)
{
	return 20 + (10 * (2*level) * (level+pow(0.95, level)));
}

