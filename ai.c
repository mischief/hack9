#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "bt.h"
#include "map.h"

#include "dat.h"
#include "alg.h"

enum
{
	IDLERADIUS	= 6,
	IDLEIDLE	= 4,
};

enum
{
	AIPOINT	= 0,
	AIINT,
	AIMONST,
	AIITEM,
	AIPATH,
	AIWALKTO,
};

typedef struct AiData AiData;
struct AiData
{
	u32int type;
	union {
		int i;
		Point pt;
		Monster *m;
		Item *it;
		Point *path;
		WalkToData *wd;
	};
};

static void
aivfree(void *v)
{
	AiData *d;

	d = v;

	if(d == nil)
		return;

	switch(d->type){
	case AIMONST:
		mfree(d->m);
		break;
	case AIITEM:
		ifree(d->it);
		break;
	case AIPATH:
		free(d->path);
		break;
	case AIWALKTO:
		free(d->wd->path);
		free(d->wd);
		break;
	}

	free(d);
}

static AiData*
aidata(u32int typ)
{
	AiData *d = emalloc(sizeof(AiData));
	d->type = typ;
	return d;
}

AiData*
aipt(Point pt)
{
	AiData *d = aidata(AIPOINT);
	d->pt = pt;
	return d;
}

AiData*
aiint(int i)
{
	AiData *d = aidata(AIINT);
	d->i = i;
	return d;
}

AiData*
aimonst(Monster *m)
{
	AiData *d = aidata(AIMONST);
	d->m = m;
	return d;
}

AiData*
aiitem(Item *i)
{
	AiData *d = aidata(AIITEM);
	d->it = i;
	return d;
}	

AiData*
aipath(Point *path)
{
	AiData *d = aidata(AIPATH);
	d->path = path;
	return d;
}

AiData*
aiwd(WalkToData *wd)
{
	AiData *d = aidata(AIWALKTO);
	d->wd = wd;
	return d;
}

static void
OOM(void)
{
	sysfatal("ai: %r");
}

static int
wander(void *v)
{
	int try;
	Point p;
	Monster *m;
	AiData *pt, *in;

	m = v;

	for(try = 0; try < 5; try++){
		p = addpt(m->pt, Pt(nrand(IDLERADIUS*2+1)-IDLERADIUS, nrand(IDLERADIUS*2+1)-IDLERADIUS));

		if(ptinrect(p, m->l->r) &&
			!hasflagat(m->l, p, Fblocked|Fhasmonster) &&
			manhattan(m->pt, p) >= ORTHOCOST*2)
			break;
	}

	if(try == 5)
		return TASKFAIL;

	pt = aipt(p);
	in = aiint(0);

	if(mapset(m->bb, "wander_dst", pt) < 0)
		OOM();

	if(mapset(m->bb, "wander_offset", in) < 0)
		OOM();

	return TASKSUCCESS;
}

static WalkToData*
planpathcalc(Monster *m, Point dst, int offset)
{
	int npath;
	Point *path;
	WalkToData *wd;

	npath = pathfind(m->l, m->pt, dst, &path, Fhasmonster|Fblocked);
	if(npath < 0)
		return nil;

	wd = emalloc(sizeof(WalkToData));
	wd->next = 1;
	wd->npath = npath - offset;
	wd->path = path;

	return wd;
}

static int
planpathkey(Monster *m, char *dst, char *offset, char *data)
{
	WalkToData *wd;
	AiData *dd, *doff;

	dd = mapget(m->bb, dst);
	doff = mapget(m->bb, offset);

	wd = planpathcalc(m, dd->pt, doff->i);
	if(wd == nil)
		return TASKFAIL;

	if(mapset(m->bb, data, aiwd(wd)) < 0)
		OOM();

	return TASKSUCCESS;
}

static int
planpathwander(void *v)
{
	Monster *m;

	m = v;

	return planpathkey(m, "wander_dst", "wander_offset", "wander_data");
}

static int
planpathgetstuff(void *v)
{
	Monster *m;

	m = v;

	return planpathkey(m, "getstuff_dst", "getstuff_offset", "getstuff_data");
}

static int
planpathattack(void *v)
{
	Monster *m;

	m = v;

	return planpathkey(m, "attack_dst", "attack_offset", "attack_data");
}

static int
planpathguard(void *v)
{
	Monster *m;

	m = v;

	return planpathkey(m, "guard_dst", "guard_offset", "guard_data");
}

static int
walktoinner(Monster *m, char *data)
{
	Point np;
	WalkToData *wd;
	AiData *dwd;

	dwd = mapget(m->bb, data);
	assert(dwd != nil);

	wd = dwd->wd;

	if(wd->next == wd->npath){
		mapdelete(m->bb, data);
		return TASKSUCCESS;
	}

	np = wd->path[wd->next];

	/* could just call maction(m,MMOVE,np) but
	 * this prevents friendly fire
	 */
	if(hasflagat(m->l, np, Fhasmonster|Fblocked)){
		mapdelete(m->bb, data);
		return TASKFAIL;
	}

	int rv = maction(m, MMOVE, np);
	if(rv != 1){
		mapdelete(m->bb, data);
		return TASKFAIL;
	}

	wd->next++;

	return TASKRUNNING;
}

static int
walktowander(void *v)
{
	Monster *m;

	m = v;

	return walktoinner(m, "wander_data");
}

static int
walktogetstuff(void *v)
{
	Monster *m;
	m = v;
	return walktoinner(m, "getstuff_data");
}

static int
walktoattack(void *v)
{
	Monster *m;
	m = v;
	return walktoinner(m, "attack_data");
}

static int
walktoguard(void *v)
{
	Monster *m;
	m = v;
	return walktoinner(m, "guard_data");
}

enum {
	ATTACKRADIUS	= 6,
};

typedef int (*checktile)(Monster *m, Level *l, Point p);

static int
findthing(Monster *m, Level *l, int radius, checktile ct)
{
	int i, r, xd, yd, xs, ys;
	Point c, p, s;

	c = m->pt;

	r = radius;

	for(p.x = c.x; p.x >= c.x - r; p.x--){
		for(p.y = c.y; p.y >= c.y - r; p.y--){
			xd = p.x - c.x;
			yd = p.y - c.y;

			/* skip points outside circle */
			if(xd*xd+yd*yd > r*r)
				continue;

			xs = c.x - xd;
			ys = c.y - yd;

			Point pts[] = { p, Pt(p.x, ys), Pt(xs, p.y), Pt(xs, ys) };
			shuffle(pts, 4, sizeof(Point));
			for(i = 0; i < 4; i++){
				s = pts[i];

				if(!ptinrect(s, m->l->r))
					continue;

				if(ct(m, l, s) == 0)
					return TASKSUCCESS;
			}
		}
	}

	return TASKFAIL;
}

static int
checkenemy(Monster *m, Monster *target)
{
	if(m == target)
		return -1;
	if(target->flags & Mdead)
		return -1;
	if(abs(m->align - target->align) <= 15)
		return -1;
	return 0;
}

static int
checkenemy2(Monster *m, Level *l, Point p)
{
	Tile *t;
	Monster *target;

	if(!hasflagat(l, p, Fhasmonster))
		return -1;

	t = tileat(l, p);
	target = t->monst;

	if(checkenemy(m, target) != 0)
		return -1;

	/* found a suitable target */
	incref(&target->ref);
	if(mapset(m->bb, "attack_target", aimonst(target)) < 0)
		OOM();

	return 0;
}

static int
findenemy(void *v)
{
	Monster *m, *target;
	AiData *dtgt;

	m = v;

	target = nil;

	/* check if we already have a target */
	dtgt = mapget(m->bb, "attack_target");
	if(dtgt != nil)
		target = dtgt->m;

	if(target != nil && checkenemy(m, target) == 0)
		return TASKSUCCESS;

	return findthing(m, m->l, ATTACKRADIUS, checkenemy2);
}

static int
targetenemy(void *v)
{
	Monster *m;
	AiData *dtgt, *dst, *off;

	m = v;

	dtgt = mapget(m->bb, "attack_target");
	if(dtgt == nil)
		return TASKFAIL;

	dst = aipt(dtgt->m->pt);
	off = aiint(1);

	if(mapset(m->bb, "attack_dst", dst) < 0)
		OOM();

	if(mapset(m->bb, "attack_offset", off) < 0)
		OOM();

	return TASKSUCCESS;
}

static int
attackenemy(void *v)
{
	Monster *m, *target;
	AiData *dtgt;
	m = v;

	dtgt = mapget(m->bb, "attack_target");
	assert(dtgt != nil);

	target = dtgt->m;

	/* monster has moved */
	if(manhattan(m->pt, target->pt) > ORTHOCOST)
		return TASKFAIL;

	/* hit the enemy */
	if(maction(m, MMOVE, target->pt) != 0)
		return TASKFAIL;

	if(target->flags & Mdead)
		return TASKSUCCESS;

	return TASKRUNNING;
}

static BehaviorNode*
bnidle(void)
{
	BehaviorNode *bnwfind, *bnplanpath, *bnwalkto;

	bnwfind = btleaf("wander find", wander);
	bnplanpath = btleaf("wander plan path", planpathwander);
	bnwalkto = btleaf("wander move", walktowander);

	btsetguard(bnwalkto, bnplanpath);
	btsetguard(bnplanpath, bnwfind);

	return bnwalkto;	
}

static int
checkitem(Monster *m, Level *l, Point p)
{
	int typ;
	Tile *t;
	Item *target;

	if(!hasflagat(l, p, Fhasitem))
		return -1;

	if(hasflagat(l, p, Fhasmonster))
		return -1;

	/* something is here */
	t = tileat(l, p);
	target = ilnth(&t->items, 0);

	typ = target->id->type;

	/* if we can't use it, forget it */
	if(typ < NEQUIP && m->armor[typ] != nil)
		return -1;

	incref(&target->ref);
	if(mapset(m->bb, "getstuff_target", aiitem(target)) < 0)
		OOM();
	if(mapset(m->bb, "getstuff_dst", aipt(p)) < 0)
		OOM();
	if(mapset(m->bb, "getstuff_offset", aiint(0)) < 0)
		OOM();

	return 0;
}

static int
findstuff(void *v)
{
	Monster *m;
	AiData *itgt, *idst, *ioff;

	m = v;

	/* make sure it is still there */
	itgt = mapget(m->bb, "getstuff_target");
	idst = mapget(m->bb, "getstuff_dst");
	ioff = mapget(m->bb, "getstuff_offset");
	if(itgt != nil && idst != nil && ioff != nil)
		return TASKSUCCESS;

	return findthing(m, m->l, 6, checkitem);
}

static int
ontop(void *v)
{
	Monster *m;
	AiData *idst;

	m = v;

	idst = mapget(m->bb, "getstuff_dst");
	if(idst == nil || !eqpt(m->pt, idst->pt))
		return TASKFAIL;

	return TASKSUCCESS;
}

static int
pickup(void *v)
{
	Monster *m;
	AiData *itgt;

	m = v;

	itgt = mapget(m->bb, "getstuff_target");
	msg("the %s picks up a %i", m->md->name, itgt->it);

	if(maction(m, MPICKUP, m->pt) < 0)
		return TASKFAIL;

	return TASKSUCCESS;
}

static void
getend(void *v)
{
	Monster *m;

	m = v;

	mapdelete(m->bb, "getstuff_target");
	mapdelete(m->bb, "getstuff_dst");
	mapdelete(m->bb, "getstuff_offset");
	mapdelete(m->bb, "getstuff_data");
}

static BehaviorNode*
bngetstuff(void)
{
	BehaviorNode *bnifind;
	BehaviorNode *bnroute, 	*bnontop, *bnplanpath, *bnwalkto;
	BehaviorNode *bnpickup;
	BehaviorNode *bnget;

	bnifind = btleaf("getstuff find", findstuff);
	bnontop = btleaf("ontop of stuff", ontop);
	bnplanpath = btleaf("getstuff plan path", planpathgetstuff);
	bnwalkto = btleaf("getstuff move", walktogetstuff);

	/* either we are on top of our target, or walk to it */
	bnroute = btparallel("getstuff route", 1, 2,
		bnontop,
		btsequence("getstuff walk", bnplanpath, bnwalkto, nil),
		nil);

	bnpickup = btleaf("getstuff pickup", pickup);

	bnget = btsequence("getstuff", bnroute, bnpickup, nil);

	btsetguard(bnget, bnifind);
	btsetend(bnget, getend);

	return bnget;
}

static void
atkend(void *v)
{
	Monster *m = v;
	mapdelete(m->bb, "attack_target");
	mapdelete(m->bb, "attack_dst");
	mapdelete(m->bb, "attack_offset");
	mapdelete(m->bb, "attack_data");
}

static BehaviorNode*
bnattack(void)
{
	BehaviorNode *bnfind, *bntarget;
	BehaviorNode *bnplanpath, *bnmove, *bnhit;
	BehaviorNode *bnatk;

	bnfind = btleaf("find enemy", findenemy);

	bntarget = btleaf("target enemy", targetenemy);
	bnplanpath = btleaf("attack plan path", planpathattack);
	bnmove = btleaf("move to enemy", walktoattack);
	bnhit = btleaf("hit enemy", attackenemy);

	bnatk = btsequence("attack", bnplanpath, bnmove, bnhit, nil);
	btsetguard(bnatk, bntarget);
	btsetguard(bntarget, bnfind);
	btsetend(bnatk, atkend);

	return bnatk;
}

static int
haveequip(void *v)
{
	int i, typ;
	Monster *m;
	Item *it;

	m = v;

	for(i = 0; (it = ilnth(&m->inv, i)) != nil; i++){
		typ = it->id->type;

		if(typ < NEQUIP && m->armor[typ] == nil)
			break;
	}

	if(it == nil)
		return TASKFAIL;

	dbg("equip %i", it);

	if(mapset(m->bb, "equip_inv_slot", aiint(i)) < 0)
		OOM();
	return TASKSUCCESS;
}

static int
equipstuff(void *v)
{
	int slot;
	Monster *m;
	AiData *dslot;

	m = v;

	dslot = mapget(m->bb, "equip_inv_slot");
	slot = dslot->i;
	mapdelete(m->bb, "equip_inv_slot");

	warn("the %s equips a %i!", m->md->name, ilnth(&m->inv, slot));

	assert(mwield(m, slot) == 1);
	return TASKSUCCESS;
}

static BehaviorNode*
bnequip(void)
{
	BehaviorNode *bnhave, *bnuse;

	bnhave = btleaf("have equip", haveequip);
	bnuse = btleaf("equip item", equipstuff);

	btsetguard(bnuse, bnhave);

	return bnuse;
}

enum {
	GUARDRADIUS	= 6 * ORTHOCOST,
};

static int
guardcheck(void *v)
{
	Monster *m;
	Point ptgt;
	AiData *dtgt;

	m = v;

	/* if we have not set, we aren't guarding */
	dtgt = mapget(m->bb, "guard_target");
	if(dtgt == nil)
		return TASKFAIL;

	switch(dtgt->type){
	case AIPOINT:
		ptgt = dtgt->pt;
		break;
	case AIMONST:
		ptgt = dtgt->m->pt;
		break;
	default:
		abort();
	}

	if(mapget(m->bb, "guard_dst") != nil){
		/* sometimes stop early */
		if(nrand(3) == 0 && manhattan(m->pt, ptgt) < GUARDRADIUS)
			return TASKFAIL;

		return TASKSUCCESS;
	}

	if(manhattan(m->pt, ptgt) <= GUARDRADIUS)
		return TASKFAIL;

	if(mapset(m->bb, "guard_dst", aipt(ptgt)) < 0)
		OOM();
	if(mapset(m->bb, "guard_offset", aiint(1)) < 0)
		OOM();

	return TASKSUCCESS;
}

static void
guardend(void *v)
{
	Monster *m = v;
	mapdelete(m->bb, "guard_dst");
}

static BehaviorNode*
btguard(void)
{
	BehaviorNode *bnshouldguard, *bnplanpath, *bnwalkto;

	bnshouldguard = btleaf("guard check", guardcheck);
	bnplanpath = btleaf("guard plan path", planpathguard);
	bnwalkto = btleaf("guard move", walktoguard);

	btsetguard(bnwalkto, bnplanpath);
	btsetguard(bnplanpath, bnshouldguard);

	btsetend(bnwalkto, guardend);

	return bnwalkto;
}

void
idle(Monster *m)
{
	Map *map;
	BehaviorNode *root;

	map = mapnew(aivfree);
	if(map == nil)
		OOM();

	if(nrand(10) > 3)
		root = btdynguard("root", bnattack(), bnequip(), bngetstuff(), bnidle(), nil);
	else
		root = btdynguard("root", bnequip(), bngetstuff(), bnattack(), bnidle(), nil);

	if(root == nil)
		OOM();

	m->bt = root;
	m->bb = map;
}

void
aiguard(Monster *m, Monster *target, Point p)
{
	Map *map;
	BehaviorNode *root;
	AiData *tgt;

	map = mapnew(aivfree);
	if(map == nil)
		OOM();

	m->bb = map;

	root = btdynguard("root", btguard(), bnattack(), bnequip(), bngetstuff(), bnidle(), nil);

	m->bt = root;

	if(target != nil){
		incref(&target->ref);
		tgt = aimonst(target);
	} else
		tgt = aipt(p);

	if(mapset(m->bb, "guard_target", tgt) < 0)
		OOM();
}
