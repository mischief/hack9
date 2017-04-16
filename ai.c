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
planpathattack(void *v)
{
	Monster *m;

	m = v;

	return planpathkey(m, "attack_dst", "attack_offset", "attack_data");
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

	if(wd->next == wd->npath)
		return TASKSUCCESS;

	np = wd->path[wd->next++];

	/* could just call maction(m,MMOVE,np) but
	 * this prevents friendly fire
	 */
	if(hasflagat(m->l, np, Fhasmonster|Fblocked))
		return TASKFAIL;

	int rv = maction(m, MMOVE, np);
	if(rv != 1)
		return TASKFAIL;

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
walktoattack(void *v)
{
	Monster *m;
	m = v;
	return walktoinner(m, "attack_data");
}

enum {
	ATTACKRADIUS	= 6,
};

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
findenemy(void *v)
{
	int i, r, xd, yd, xs, ys;
	Point c, p, s;
	Tile *t;
	Monster *m, *target;
	AiData *dtgt;

	m = v;
	c = m->pt;

	r = ATTACKRADIUS;

	target = nil;

	/* check if we already have a target */
	dtgt = mapget(m->bb, "attack_target");
	if(dtgt != nil)
		target = dtgt->m;

	if(target != nil && checkenemy(m, target) == 0)
		return TASKSUCCESS;

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
				if(!hasflagat(m->l, s, Fhasmonster))
					continue;

				t = tileat(m->l, s);
				target = t->monst;

				if(checkenemy(m, target) != 0)
					continue;

				//fprint(2, "FINDENEMY: %s %P\n", target->md->name, target->pt);

				/* found a suitable target */
				incref(&target->ref);
				if(mapset(m->bb, "attack_target", aimonst(target)) < 0)
					OOM();

				return TASKSUCCESS;
			}
		}
	}

	return TASKFAIL;
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

	return bnatk;
}

void
idle(Monster *m)
{
	Map *map;
	BehaviorNode *root;

	map = mapnew(aivfree);
	if(map == nil)
		OOM();

	root = btdynguard("root", bnattack(), bnidle(), nil);

	if(root == nil)
		OOM();

	m->bt = root;
	m->bb = map;
}
