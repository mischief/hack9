#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "dat.h"

AIState*
mkstate(char *name, Monster *m, void *aux, AIFun enter, AIFun exec, AIFun exit)
{
	AIState *i;
	i = mallocz(sizeof(AIState), 1);
	assert(i != nil);
	snprint(i->name, sizeof(i->name), "%s", name);
	i->m = m;
	i->aux = aux;
	i->enter = enter;
	i->exec = exec;
	i->exit = exit;
	return i;
}

void
freestate(AIState *a)
{
	free(a);
}

enum
{
	IDLERADIUS	= 4,
	IDLEIDLE	= 4,
};

typedef struct idledata idledata;
struct idledata
{
	int idle;
	int attacking;
};

static void
idleenter(AIState *a)
{
	idledata *d;
	d = mallocz(sizeof(idledata), 1);
	assert(d != nil);
	a->aux = d;
	d->idle = nrand(IDLEIDLE);
}

static void
idleexec(AIState *a)
{
	idledata *d;
	Monster *m, *mt;
	int i, r, xd, yd, xs, ys;
	Point c, p, s;
	d = a->aux;
	m = a->m;

	if(m->ai == nil)
		d->idle++;

	if(d->idle > IDLEIDLE){
		mpushstate(m, wander(m));
		d->idle = 0;
		return;
	}

	if(m->ai == nil)
		d->attacking = 0;
	if(d->attacking != 0)
		return;

	/* find something to attack */
	r = 4;
	c = m->pt;
	for(p.x = c.x - r; p.x <= c.x; p.x++){
		for(p.y = c.y - r; p.y <= c.y; p.y++){
			if(eqpt(p, c))
				continue;
			xd = p.x - c.x;
			yd = p.y - c.y;
			if(xd*xd+yd*yd <= r*r){
				xs = c.x - xd;
				ys = c.y - yd;

				Point pts[] = { p, Pt(p.x, ys), Pt(xs, p.y), Pt(xs, ys) };
				for(i = 0; i < 4; i++){
					s = pts[i];
					if(!ptinrect(s, m->l->r))
						continue;
					if(hasflagat(m->l, s, Fhasmonster)){
						mt = tileat(m->l, s)->monst;
						if(abs(m->align - mt->align) > 15){
							mpushstate(m, attack(m, mt));
							d->attacking = 1;
							return;
						}
					}
				}
			}
		}
	}
}

static void
idleexit(AIState *a)
{
	free(a->aux);
}

void
idle(Monster *m)
{
	AIState *i;
	i = mkstate("idle", m, nil, idleenter, idleexec, idleexit);
	i->enter(i);
	m->aglobal = i;
}

static void
wanderexec(AIState *a)
{
	Point p;
	Monster *m;
	AIState *ai;
	m = a->m;

	if(a->aux != nil){
		ai = mpopstate(m);
		freestate(ai);
		return;
	}

	do {
		p = addpt(m->pt, Pt(nrand(IDLERADIUS*2+1)-IDLERADIUS, nrand(IDLERADIUS*2+1)-IDLERADIUS));
	} while(!ptinrect(p, m->l->r) ||  hasflagat(m->l, p, Fblocked) || manhattan(m->pt, p) < ORTHOCOST*2);

	mpushstate(m, walkto(m, p, 5));
	a->aux=a;
}

AIState*
wander(Monster *m)
{
	AIState *i;
	i = mkstate("wander", m, nil, nil, wanderexec, nil);
	return i;
}

static void
attackenter(AIState *a)
{
	Monster *mt;
	mt = a->aux;
	incref(mt);
}

static void
attackexec(AIState *a)
{
	int d;
	Monster *m, *mtarg;
	AIState *anew;
	m = a->m;
	mtarg = a->aux;

	d = manhattan(m->pt, mtarg->pt);
	if(mtarg->flags & Mdead || d > ORTHOCOST*5 ){
		/* stop the chase */
		anew = mpopstate(m);
		freestate(anew);
		return;
	} else if(d > ORTHOCOST){
		/* walk to target */
		anew = walkto(m, mtarg->pt, 0);
		mpushstate(m, anew);
	} else {
		/* attack! */
		maction(m, MMOVE, mtarg->pt);
	}
}

static void
attackexit(AIState *a)
{
	Monster *mt;
	mt = a->aux;
	mfree(mt);
}

AIState*
attack(Monster *m, Monster *targ)
{
	AIState *i;
	i = mkstate("attack", m, targ, attackenter, attackexec, attackexit);
	return i;
}

typedef struct walktodata walktodata;
struct walktodata
{
	Point dst;
	int npath;
	int next;
	Point *path;
	int wait;
	int blocked;
};

static void
walktoexec(AIState *a)
{
	walktodata *d;
	Monster *m;
	Point p;

	d = a->aux;
	m = a->m;

	if(d->path == nil){
		d->npath = pathfind(m->l, m->pt, d->dst, &d->path);
		d->next = 1;
		d->blocked = 0;
	}

	/* reached end */
	if(manhattan(m->pt, d->dst) == ORTHOCOST || d->path == nil || d->next == d->npath){
leave:
		a = mpopstate(m);
		freestate(a);
		return;
	}

	p = d->path[d->next];
	
	/* if the next move is blocked */
	if(hasflagat(m->l, p, Fblocked)){
		if(++d->blocked > d->wait)
			goto leave;
		return;
	}

	d->blocked = 0;
	d->next++;

	maction(m, MMOVE, p);
}

static void
walktoexit(AIState *a)
{
	walktodata *d;
	d = a->aux;
	if(d->path != nil)
		free(d->path);
	free(d);
}

AIState*
walkto(Monster *m, Point p, int wait)
{
	walktodata *d;
	AIState *i;
	d = mallocz(sizeof(*d), 1);
	d->dst = p;
	d->wait = wait;
	assert(d != nil);
	i = mkstate("walk", m, d, nil, walktoexec, walktoexit);
	return i;
}

