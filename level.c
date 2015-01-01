#include <u.h>
#include <libc.h>
#include <draw.h>

#include "dat.h"

static void
clear(Level *l, Point p, int dist)
{
	Rectangle r;
	Point p2;
	Tile *t;

	r = Rect(p.x-dist, p.y-dist, p.x+dist+1, p.y+dist+1);
	rectclip(&r, l->r);

	for(p2.x = r.min.x; p2.x < r.max.x; p2.x++){
		for(p2.y = r.min.y; p2.y < r.max.y; p2.y++){
			if(eqpt(p, p2))
				continue;
			t = tileat(l, p2);
			t->unit = t->feat = 0;
			flagat(l, p2) = 0;
		}
	}
}

static void
drunk1(Level *l, Point p, int n)
{
	int cur, last;
	Point next;
	Rectangle clipr;
	Tile *t;

	clipr = insetrect(l->r, 1);

	last = 0;
	while(n > 0){
		if(nrand(4) == 0)
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
			n--;
		}
	}
}

static int
drunken(Level *l, int type)
{
	int cnt, npath;
	Point p, *path;
	Tile *t;

	cnt = ((l->width * l->height) / 4); //* 2;

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

	drunk1(l, l->up, cnt);
	drunk1(l, l->down, cnt);

	/* check reachability */
	npath = pathfind(l, l->up, l->down, &path);
	if(npath < 0)
		goto redo;
	else
		free(path);

	return cnt * 2;
}

static void
gen(Level *l)
{
	int i, q, rnd, space, npath;
	Point pup, pdown, p, *path;
	Tile *t;
	Monster *m;

	pup = (Point){nrand(l->width-4)+2, nrand(l->height-4)+2};
	tileat(l, pup)->feat = TUPSTAIR;
	setflagat(l, pup, Fhasfeature|Fportal);
	l->up = pup;

	while(1){
		pdown = (Point){nrand(l->width-4)+2, nrand(l->height-4)+2};
		/* already upstair? */
		if(flagat(l, pdown) & Fportal)
			continue;
		/* too close? */
		p = subpt(pup, pdown);
		if((npath = pathfind(l, pup, pdown, &path)) < 0)
			sysfatal("pathfind: %r");
		free(path);
		if(npath < sqrt(l->width * l->height)-4)
			continue;
		setflagat(l, pdown, Fhasfeature|Fportal);
		tileat(l, pdown)->feat = TDOWNSTAIR;
		l->down = pdown;
		break;
	}

	space = 0;

	space += drunken(l, TTREE);

	/* some monsters */
	q = (l->width*l->height) / 40;
	q += 2;
	rnd = nrand(q)+q;
	for(i = 0; i < rnd && space > 10; i++){
		do {
			p = (Point){nrand(l->width), nrand(l->height)};
		} while(hasflagat(l, p, Fblocked|Fhasfeature));

		t = tileat(l, p);
		q = nrand(4)+TSOLDIER;
		t->unit = q;
		m = monst(q);
		m->l = l;
		m->pt = p;
		t->monst = m;
		setflagat(l, p, Fblocked|Fhasmonster);

		/* setup ai state */
		idlestate(m);

		space--;
	}

	/* clear space around stairs */
	clear(l, pup, 1);
	clear(l, pdown, 1);
}

Level*
genlevel(int width, int height)
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

	gen(l);

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
				free(tileat(l, Pt(x, y))->monst);
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
		if(ptinrect(p2, l->r) && (!hasflagat(l, p2, Fblocked) || hasflagat(l, p2, Fhasmonster))){
			neigh[count++] = p2;
		}
	}

	*n = count;
	return neigh;
}

