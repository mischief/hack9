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
	int i, r, xd, yd, xs, ys, npath;
	Point c, p, s, *path;
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
	for(p.x = c.x; p.x >= c.x - r; p.x--){
		for(p.y = c.y; p.y >= c.y - r; p.y--){
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
						/* BUG: should not be nil. */
						if(mt == nil)
							continue;
						if(abs(m->align - mt->align) > 15){
							if(manhattan(m->pt, s) != ORTHOCOST){
								npath = pathfind(m->l, m->pt, s, &path, Fblocked);
								if(npath < 0)
									continue;
								if(npath > r*2){
									free(path);
									continue;
								}
								free(path);
							}
							d->attacking = 1;
							mpushstate(m, attack(m, mt));
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
	int try;
	Point p;
	Monster *m;
	AIState *ai;
	m = a->m;

	if(a->aux != nil){
leave:
		ai = mpopstate(m);
		freestate(ai);
		return;
	}

	try = 0;
	do {
		p = addpt(m->pt, Pt(nrand(IDLERADIUS*2+1)-IDLERADIUS, nrand(IDLERADIUS*2+1)-IDLERADIUS));
		if(try++ > 10)
			goto leave;
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

static int
walktoinner(AIState *a)
{
	Point p;
	walktodata *d;
	Monster *m;
	d = a->aux;
	m = a->m;

	//dbg("%s path → %P %d/%d", m->md->name, d->dst, d->next, d->npath);
	if(d->path == nil || !eqpt(m->pt, d->dst)){
		if(d->path != nil){
			free(d->path);
			d->path = nil;
		}
		/* plan route */
		d->npath = pathfind(m->l, m->pt, d->dst, &d->path, Fblocked);
		d->next = 1;
		d->blocked = 0;
		if(d->npath < 0){
			//dbg("%s route to %P blocked; routing through monsters", m->md->name, d->dst);
			/* path not through monsters available? */
			d->npath = pathfind(m->l, m->pt, d->dst, &d->path, Fhasfeature);
			d->next = 1;
			d->blocked = 1;
			if(d->npath < 0){
				goto blocked;
			}
		}
	}

	if(d->next == d->npath){
		free(d->path);
		d->path = nil;
		d->npath = 0;
		return -1;
	}

	p = d->path[d->next];
	//dbg("next %P %06b", p, flagat(m->l, p));

	if(hasflagat(m->l, p, Fblocked)){
blocked:
		dbg("%s blocked %d/%d", m->md->name, d->blocked, d->wait);
		if(++d->blocked > d->wait){
			if(d->path != nil){
				free(d->path);
				d->path = nil;
			}
			d->npath = 0;
			return -1;
		}
		return 0;
	}

	d->blocked = 0;
	d->next++;

	maction(m, MMOVE, p);
	return 0;
}

static void
walktoexec(AIState *a)
{
	Monster *m;
	m = a->m;

	if(walktoinner(a) < 0){
		a = mpopstate(m);
		freestate(a);
		return;
	}
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

typedef struct attackdata attackdata;
struct attackdata
{
	Monster *mt;
	Point last;
	AIState *w;
	walktodata *wd;
};

static void
attackenter(AIState *a)
{
	attackdata *d;
	d = a->aux;
	if(d->w->enter != nil)
		d->w->enter(d->w);
}

static void
attackexec(AIState *a)
{
	int n;
	attackdata *d;
	Monster *m, *mt;
	m = a->m;
	d = a->aux;
	mt = d->mt;

	n = manhattan(m->pt, mt->pt);
	if(mt->flags & Mdead || n > ORTHOCOST*5 ){
		/* stop the chase */
		a = mpopstate(m);
		freestate(a);
		return;
	} else if(n > ORTHOCOST){
		/* walk to target */
		if(!eqpt(mt->pt, d->last)){
			d->wd->dst = mt->pt;
		}
		walktoinner(d->w);
		d->last = mt->pt;
	} else {
		/* attack! */
		maction(m, MMOVE, mt->pt);
	}
}

static void
attackexit(AIState *a)
{
	attackdata *d;
	d = a->aux;

	if(d->w->exit != nil)
		d->w->exit(d->w);
	freestate(d->w);
	mfree(d->mt);
	free(d);
}

AIState*
attack(Monster *m, Monster *targ)
{
	attackdata *d;
	AIState *i;
	d = mallocz(sizeof(*d), 1);
	incref(&targ->ref);
	d->mt = targ;
	d->wd = mallocz(sizeof(walktodata), 1);
	d->w = mkstate("chase", m, d->wd, nil, nil, walktoexit);
	i = mkstate("attack", m, d, attackenter, attackexec, attackexit);
	return i;
}

