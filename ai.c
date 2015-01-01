#include <u.h>
#include <libc.h>
#include <draw.h>

#include "dat.h"

enum
{
	IDLERADIUS = 4,
};

typedef struct idledata idledata;
struct idledata
{
	int npath;
	int next;
	Point *path;
};

static void
idleenter(Monster *m)
{
	idledata *d;
	d = mallocz(sizeof(idledata), 1);
	assert(d != nil);
	m->acur->aux = d;
}

static void
idleexec(Monster *m)
{
	Point p;
	idledata *d;
	d = m->acur->aux;
	assert(d != nil);

	if(d->path == nil){
		/* plan a new path */
		do {
			p = addpt(m->pt, Pt(nrand(IDLERADIUS*2+1)-IDLERADIUS, nrand(IDLERADIUS*2+1)-IDLERADIUS));
		} while(!ptinrect(p, m->l->r) ||  hasflagat(m->l, p, Fblocked) || manhattan(m->pt, p) < ORTHOCOST*2);
		d->npath = pathfind(m->l, m->pt, p, &d->path);
		d->next = 1;
	}

	if(d->path == nil)
		return;

	if(d->next == d->npath){
reroute:
		free(d->path);
		d->path = nil;
		return;
	}

	p = d->path[d->next];
	
	/* if the next move is blocked, reroute. */
	if(hasflagat(m->l, p, Fblocked))
		goto reroute;

	d->next++;

	maction(m, MMOVE, p);
}

static void
idleexit(Monster *m)
{
	free(m->acur->aux);
}

void
idlestate(Monster *m)
{
	AIState *i;
	i = mallocz(sizeof(AIState), 1);
	assert(i != nil);
	i->enter = idleenter;
	i->exec = idleexec;
	i->exit = idleexit;

	mchangestate(m, i);
}

