#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <bio.h>
#include <ndb.h>

#include "dat.h"

enum
{
	MONNAME		= 0,
	MONTILE,
	MONXPL,
	MONALIGN,
	MONMVRATE,
	MONHP,
	MONAC,
	MONATK,
	MONEQUIP,
	MONEQPROB,
};

static char *monstdbcmd[] = {
[MONNAME]	"monster",
[MONTILE]	"tile",
[MONXPL]	"xpl",
[MONALIGN]	"align",
[MONMVRATE]	"mvr",
[MONHP]		"hp",
[MONAC]		"ac",
[MONATK]	"atk",
[MONEQUIP]	"equip",
[MONEQPROB]	"eqprob",
};

static Ndb *monstdb;

int
monstdbopen(char *file)
{
	monstdb = ndbopen(file);
	if(monstdb == nil)
		return 0;
	return 1;
}

typedef struct mdcache mdcache;
struct mdcache
{
	MonsterData *md;
	mdcache *next;
};

static mdcache *monstcache = nil;

/* TODO: sanity checks */
static int
mdaddattr(MonsterData *md, int type, Ndbtuple *t)
{
	Ndbtuple *line;
	EquipData *equip;

	switch(type){
	case MONNAME:
		strncpy(md->name, t->val, sizeof(md->name)-1);
		md->name[sizeof(md->name)-1] = 0;
		break;
	case MONTILE:
		md->tile = atol(t->val);
	case MONXPL:
		md->basexpl = atol(t->val);
		break;
	case MONALIGN:
		md->align = (char)atol(t->val);
		break;
	case MONMVRATE:
		md->mvr = atol(t->val);
		break;
	case MONHP:
		md->maxhp = atol(t->val);
		break;
	case MONAC:
		md->def = atol(t->val);
		break;
	case MONATK:
		if(!parseroll(t->val, &md->rolls, &md->atk)){
			werrstr("bad roll '%s'", t->val);
			return 0;
		}
		break;
	case MONEQUIP:
		if(md->nequip > sizeof(md->equip)-1)
			break;

		equip = &md->equip[md->nequip++];
		snprint(equip->name, SZNAME, "%s", t->val);
		equip->prob = 1.0;

		for(line = t->line; line != t; line = line->line){
			if(strcmp(line->attr, monstdbcmd[MONEQPROB]) == 0){
				equip->prob = atof(line->val);
				break;
			}
		}
		break;
	case MONEQPROB:
		/* handled by MONEQUIP */
		break;
	default:
		return 0;
	}

	return 1;
}

MonsterData*
mdbyname(char *monster)
{
	int i;
	Ndbtuple *t, *nt;
	Ndbs search;
	MonsterData *md;
	mdcache *mdc;

	/* cached? */
	for(mdc = monstcache; mdc != nil; mdc = mdc->next){
		if(strcmp(monster, mdc->md->name) == 0){
			return mdc->md;
		}
	}

	t = ndbsearch(monstdb, &search, "monster", monster);
	if(t == nil)
		return nil;

	md = mallocz(sizeof(MonsterData), 1);
	assert(md != nil);

	for(nt = t; nt != nil; nt = nt->entry){
		for(i = 0; i < nelem(monstdbcmd); i++){
			if(strcmp(nt->attr, monstdbcmd[i]) == 0){
				if(!mdaddattr(md, i, nt))
					sysfatal("bad monster value '%s=%s': %r", nt->attr, nt->val);
				goto okattr;
			}
		}
		sysfatal("unknown monster attribute '%s'", nt->attr);
okattr:
		continue;
	}

	ndbfree(t);

	mdc = mallocz(sizeof(mdcache), 1);
	assert(mdc != nil);

	if(monstcache == nil){
		monstcache = mdc;
		monstcache->md = md;
	} else {
		mdc->md = md;
		mdc->next = monstcache;
		monstcache = mdc;
	}

	return md;
}

Monster*
mbyname(char *monster)
{
	MonsterData *md;
	Monster *m;

	md = mdbyname(monster);
	if(md == nil)
		goto missing;

	m = mallocz(sizeof(Monster), 1);
	assert(m != nil);

	m->xpl = md->basexpl;
	if(m->xpl == 0)
		m->xp = 0;
	else
		m->xp = xpcalc(m->xpl-1);
	m->align = md->align;
	m->mvr = md->mvr;
	m->mvp = nrand(m->mvr);
	m->maxhp = md->maxhp;
	m->maxhp += roll(m->xpl, 8);
	m->hp = m->maxhp;
	m->ac = md->def;

	m->md = md;

	incref(&m->ref);

	return m;
missing:
	werrstr("missing data for monster '%s'", monster);
	return nil;
}

void
mfree(Monster *m)
{
	int n;
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

	for(n = 0; n < NEQUIP; n++){
		if(m->armor[n] != nil){
			munwield(m, n);
		}
	}

	ilfree(&m->inv);

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
		if((m->flags & Mdead) == 0 && m->hp < m->maxhp && m->turns % ((42/(m->xpl+2))+1) == 0){
			m->hp+=1.0;
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
maddxp(Monster *m, int xp)
{
	int hpgain, last;

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
	int n, hit, ac, crit, dmg, xp;
	Item *i, *weap;
	Tile *t;

	hit = roll(1, 20);
	ac = mt->ac >= 0 ? mt->ac : -(1+nrand(mt->ac));

	if(hit > 10 + ac + m->xpl){
		if(nearyou(m->pt))
			warn("the %s misses the %s!", m->md->name, mt->md->name);
		return 0;
	}

	dmg = roll(m->md->rolls, m->md->atk);

	/* factor in weapon damage */
	weap = m->armor[IWEAPON];
	if(weap != nil)
		dmg += roll(weap->id->rolls, weap->id->atk);

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
		/* poor guy is dead. */
		mt->hp = 0;
		mt->flags = Mdead;

		/* take off all armor */
		for(n = 0; n < NEQUIP; n++){
			if(mt->armor[n] != nil){
				munwield(mt, n);
			}
		}

		/* drop dead guy's stuff */
		while(mt->inv.count > 0){
			i = iltakenth(&mt->inv, 0);
			t = tileat(mt->l, mt->pt);
			setflagat(mt->l, mt->pt, Fhasitem);
			iladd(&t->items, i);
		}

		xp = 10 + pow(mt->xpl, 2.0);
		if(nearyou(m->pt))
			bad("the %s kills the %s, and gains %ld xp.", m->md->name, mt->md->name, xp);

		/* gain some xp */
		maddxp(m, xp);

		m->kills++;

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

static char*
portalname(int tile)
{
	switch(tile){
	case TSQUARE:
		return "walkway";
	case TUPSTAIR:
		return "staircase going up";
	case TDOWNSTAIR:
		return "staircase going down";
	case TPORTAL:
		return "magic portal";
	default:
		sysfatal("no portal name for tile %d\n", tile);
	}

	return nil;
}

/* -1 bad move, 0 didn't move, 1 moved */
int
maction(Monster *m, int what, Point where)
{
	int n;
	Tile *cur, *targ;
	Item *i;

	if(!ptinrect(where, m->l->r))
		return -1;

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

			if(isyou(m) && hasflagat(m->l, where, Fhasitem)){
				n = 0;
				while((i = ilnth(&targ->items, n++)) != nil)
					warn("you see %i here.", i);
			}

			if(isyou(m) && hasflagat(m->l, where, Fportal)){
				warn("you see a %s to %s here.", portalname(targ->feat), targ->portal->to->name);
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
				if((cur->portal->to = genlevel(nrand(20)+50, nrand(20)+20, nrand(3)+1)) == nil)
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
	case MPICKUP:
		if(!hasflagat(m->l, where, Fhasitem))
			return -1;
		targ = tileat(m->l, where);
		i = iltakenth(&targ->items, 0);
		if(i == nil)
			return -1;
		if(isyou(m))
			good("you take the %i.", i);
		maddinv(m, i);
		if(targ->items.count == 0)
			clrflagat(m->l, where, Fhasitem);
		return 0;
		break;
	case MSPECIAL:
		bad("the special move is broken. please fix me.");
		break;
	}

	/* bad move. */
	return -1;
}

/* wield/wear n'th item in inventory. */
int
mwield(Monster *m, int n)
{
	int type;
	Item **ptr, *old, *new;

	old = nil;
	new = ilnth(&m->inv, n);
	if(new == nil)
		return 0;

	type = new->id->type;
	if(type < IWEAPON || type >= NEQUIP)
		return 0;

	new = iltakenth(&m->inv, n);
	type = new->id->type;
	ptr = &m->armor[type];

	if(*ptr != nil)
		old = *ptr;

	*ptr = new;

	if(old != nil){
		m->ac += old->id->ac;
		maddinv(m, old);
	}

	/* account for ac */
	m->ac -= new->id->ac;

	return 1;
}

int
munwield(Monster *m, int type)
{
	Item **ptr;

	assert(type >= IWEAPON && type < NEQUIP);

	ptr = &m->armor[type];

	if(*ptr == nil)
		return 0;

	m->ac += (*ptr)->id->ac;
	maddinv(m, *ptr);
	*ptr = nil;
	return 1;
}

/* somehow use the item */
int
muse(Monster *m, Item *item)
{
	switch(item->id->type){
	case ICONSUME:
		if(m->hp >= m->maxhp)
			break;
		m->hp += item->id->heal;
		if(m->hp > m->maxhp)
			m->hp = m->maxhp;
		return 1;
	}
	return 0;
}

void
maddinv(Monster *m, Item *i)
{
	Item *it;

	i->age = 0;

	/* dedup item */
	if(i->id->flags & IFSTACKS){
		for(it = m->inv.head; it != nil; it = it->next){
			if(it->id == i->id){
				it->count += i->count;
				ifree(i);
				return;
			}
		}
	}

	iladd(&m->inv, i);
}

/* generate equipment for monster based on equipment in db */
void
mgenequip(Monster *m)
{
	int i;
	double prob;
	EquipData *equip;
	Item *it;

	for(i = 0; i < m->md->nequip; i++){
		equip = &m->md->equip[i];
		prob = frand();
		if(prob <= equip->prob){
			it = ibyname(equip->name);
			maddinv(m, it);
			mwield(m, 0);
		}
	}
}

/* return the amount of xp required to gain a level */
int
xpcalc(int level)
{
	return 20 + (10 * (2*level) * (level+pow(0.95, level)));
}

