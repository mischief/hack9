#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "dat.h"

/* clear removes all monsters/features near p. */
static void
clear(Level *l, Point p, int dist)
{
	Rectangle r;
	Point p2;
	Tile *t;

	r = Rect(p.x-dist, p.y-dist, p.x+dist, p.y+dist);
	rectclip(&r, l->r);

	for(p2.x = r.min.x; p2.x <= r.max.x; p2.x++){
		for(p2.y = r.min.y; p2.y <= r.max.y; p2.y++){
			if(eqpt(p, p2))
				continue;
			t = tileat(l, p2);
			t->unit = t->feat = 0;
			if(t->monst){
				mfree(t->monst);
				t->monst = nil;
			}
			flagat(l, p2) = 0;
		}
	}
}

/* more sbias makes it straighter. */
static void
drunk1(Level *l, Point p, uint count, uint sbias)
{
	int cur, last;
	Point next;
	Rectangle clipr;
	Tile *t;

	clipr = insetrect(l->r, 1);

	last = nrand(NCARDINAL);
	while(count > 0){
		if(nrand(sbias+1) == 0)
			cur = nrand(NCARDINAL);
		else
			cur = last;
		next = addpt(p, cardinals[cur]);
		if(!ptinrect(next, clipr))
			continue;
		last = cur;
		p = next;
		if(!eqpt(p, l->up) && !eqpt(p, l->down) && hasflagat(l, p, Fblocked|Fhasfeature)){
			t = tileat(l, p);
			t->feat = 0;
			flagat(l, p) = 0;
			count--;
		}
	}
}

/*
 * drunken performs the drunkard's walk. as s1 and
 * s2 increase, the paths become more biased toward
 * 'straight'. it loops until the downstairs is
 * reachable from the upstairs.
 */
static int
drunken(Level *l, int type, int howmuch, int s1, int s2)
{
	int cnt, npath;
	Point p, *path;
	Tile *t;

	cnt = ((l->width * l->height) / howmuch); //* 2;

redo:
	/* fill */
	for(p.x = 0; p.x < l->width; p.x++){
		for(p.y = 0; p.y < l->height; p.y++){
			if(!hasflagat(l, p, Fblocked|Fhasfeature)){
				t = tileat(l, p);
				t->feat = type;
				flagat(l, p) = (Fblocked|Fhasfeature);
			}
		}
	}

	drunk1(l, l->up, cnt/2, s1);
	drunk1(l, l->down, cnt/2, s2);

	/* check reachability */
	npath = pathfind(l, l->up, l->down, &path, Fblocked);
	if(npath < 0)
		goto redo;
	else
		free(path);

	return cnt * 2;
}

/* create a monster in the idle state */
static Monster*
mkmons(Level *l, Point p, int type)
{
	Monster *m;
	m = monst(type);
	m->l = l;
	m->pt = p;
	/* setup ai state */
	idle(m);
	return m;
}

/* genmonsters spawns count of type monsters at random places on l. */
static void
genmonsters(Level *l, int type, int count)
{
	int i;
	Point p;
	Tile *t;

	for(i = 0; i < count; i++){
		do{
			p = (Point){nrand(l->width), nrand(l->height)};
		}while(hasflagat(l, p, Fhasmonster|Fhasfeature|Fblocked));

		t = tileat(l, p);
		t->unit = type;
		t->monst = mkmons(l, p, type);
		setflagat(l, p, Fhasmonster);
	}
}

/*
 * several is a recursive function, that spawns monsters
 * centered at p[0] expaning outward according to a
 * von neumann neighborhood with radius r. for example:
 *	several(l, p, 1, TGHOST, 2);
 * spawns 13 ghosts centered at p on l.
 */
static void
several(Level *l, Point *p, int count, int type, int r)
{
	int i, j, n;
	Point *neigh;
	Tile *t;
	for(i = 0; i < count; i++){
		if(!hasflagat(l, p[i], Fhasmonster)){
			t = tileat(l, p[i]);
			t->unit = type;
			t->monst = mkmons(l, p[i], type);
			setflagat(l, p[i], Fhasmonster);
		}
		if(r > 0){
			neigh = lneighbor(l, p[i], &n);
			for(j = 0; j < n; j++){
				several(l, neigh, n, type, r-1);
			}
			free(neigh);
		}
	}
}

static void addspawn(Level *l, Point p, int what, int freq)
{
	if(l->nspawns < nelem(l->spawns)-1)
		l->spawns[l->nspawns++] = (Spawn){p, what, freq};
}

static void
levelexec(AIState *a)
{
	int i;
	Level *l;
	l = a->aux;

	for(i = 0; i < l->nspawns; i++){
		if(turn % l->spawns[i].freq == 0){
			several(l, &l->spawns[i].pt, 1, l->spawns[i].what, 0);
		}
	}
}

static void
gen(Level *l, int type)
{
	int space, npath;
	Point pup, pdown, p, *path;
	Tile *t;

	space = 0;

	pup = ZP;
	pdown = ZP;

	while(1){
		if(!eqpt(pup,ZP))
			flagat(l, pup) = 0;
		do{
			pup = (Point){nrand(l->width), nrand(l->height)};
		}while(!ptinrect(pup, insetrect(l->r, 3)));

		if(!eqpt(pdown, ZP))
			flagat(l, pup) = 0;
		do{
			pdown = (Point){nrand(l->width), nrand(l->height)};
		}while(!ptinrect(pdown, insetrect(l->r, 3)));

		/* already upstair? */
		if(flagat(l, pdown) & Fportal)
			continue;
		/* too close? */
		p = subpt(pup, pdown);
		if((npath = pathfind(l, pup, pdown, &path, Fblocked)) < 0)
			continue;
		free(path);
		if(npath < sqrt(l->width * l->height)-4)
			continue;

		setflagat(l, pup, Fhasfeature|Fportal);
		t = tileat(l, pup);
		t->feat = TUPSTAIR;
		t->portal = mallocz(sizeof(Portal), 1);
		l->up = pup;

		setflagat(l, pdown, Fhasfeature|Fportal);
		t = tileat(l, pdown);
		t->feat = TDOWNSTAIR;
		t->portal = mallocz(sizeof(Portal), 1);
		l->down = pdown;
		break;
	}

	switch(type){
	case 0:
		drunken(l, TTREE, 3, 3, 3);
		clear(l, pup, 1);
		clear(l, pdown, 1);
		//several(l, &l->down, 1, TLICH, 0);
		several(l, &l->down, 1, TGWIZARD, 1);
		//several(l, &l->up, 1, TGWIZARD, 0);
		break;
		break;
	case 1:
		space += drunken(l, TTREE, 2, 0, 0);
		clear(l, pup, 1);
		clear(l, pdown, 1);
		genmonsters(l, TGWIZARD, space/128);
		genmonsters(l, TSOLDIER, space/64);
		genmonsters(l, TSERGEANT, space/128);
		several(l, &l->down, 1, TCAPTAIN, 0);
		several(l, &l->down, 1, TLIEUTENANT, 1);
		addspawn(l, l->down, TSOLDIER, 20);
		addspawn(l, l->down, TSERGEANT, 40);
		addspawn(l, l->up, TGWIZARD, 20);
		break;
	case 2:
		space += drunken(l, TGRAVE, 2, 4, 4);
		clear(l, l->up, 2);
		clear(l, l->down, 2);
		genmonsters(l, TGWIZARD, space/48);
		genmonsters(l, TGHOST, space/64);
		several(l, &l->down, 1, TLICH, 2);
		clear(l, pdown, 1);
		addspawn(l, l->up, TGWIZARD, 11);
		addspawn(l, l->down, TLICH, 61);
		addspawn(l, l->down, TGHOST, 31);
		break;
	case 3:
		drunken(l, TLAVA, 2, 1, 1);
		clear(l, pup, 2);
		clear(l, pdown, 2);
		do{
			p = (Point){nrand(l->width), nrand(l->height)};
		}while(hasflagat(l, p, Fblocked|Fhasfeature));
		addspawn(l, p, TLARGECAT, 11);
		do{
			p = (Point){nrand(l->width), nrand(l->height)};
		}while(hasflagat(l, p, Fblocked|Fhasfeature));
		addspawn(l, p, TLARGECAT, 13);
		do{
			p = (Point){nrand(l->width), nrand(l->height)};
		}while(hasflagat(l, p, Fblocked|Fhasfeature));
		addspawn(l, p, TGWIZARD, 9);
		do{
			p = (Point){nrand(l->width), nrand(l->height)};
		}while(hasflagat(l, p, Fblocked|Fhasfeature));
		addspawn(l, p, TGWIZARD, 15);
		break;
	}

	l->ai = mkstate("level", nil, l, nil, levelexec, nil);
}

Level*
genlevel(int width, int height, int type)
{
	int x, y;
	Level *l;

	l = mallocz(sizeof(Level), 1);
	if(l == nil)
		return nil;

	l->width = width;
	l->height = height;
	l->r = Rect(0, 0, width, height);

	l->tiles = mallocz(sizeof(Tile) * width * height, 1);
	if(l->tiles == nil)
		goto err0;

	l->flags = mallocz(sizeof(int) * width * height, 1);
	if(l->flags == nil)
		goto err1;

	for(x = 0; x < width; x++){
		for(y = 0; y < height; y++){
			tileat(l, Pt(x, y))->terrain = TFLOOR;
		}
	}

	gen(l, type);

	return l;

err1:
	free(l->tiles);
err0:
	free(l);
	return nil;
}

void
freelevel(Level *l)
{
	int x, y;
	Point p;

	for(x = 0; x < l->width; x++){
		for(y = 0; y < l->height; y++){
			p = (Point){x, y};
			if(hasflagat(l, p, Fhasmonster))
				mfree(tileat(l, Pt(x, y))->monst);
		}
	}

	free(l->tiles);
	free(l);
}

Tile*
tileat(Level *l, Point p)
{
	return l->tiles+(p.y*l->width)+p.x;
}

/* von neumann neighborhood */
Point*
lneighbor(Level *l, Point p, int *n)
{
	int i, count;
	Point *neigh, p2, diff[] = { {1, 0}, {0, -1}, {-1, 0}, {0, 1} };

	count = 0;
	neigh = mallocz(sizeof(Point) * nelem(diff), 1);

	for(i = 0; i < nelem(diff); i++){
		p2 = addpt(p, diff[i]);
		if(ptinrect(p2, l->r)){ // && !hasflagat(l, p2, Fblocked)){ //|| hasflagat(l, p2, Fhasmonster))){
			neigh[count++] = p2;
		}
	}

	*n = count;
	return neigh;
}

