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
	rectclip(&r, Rect(0, 0, l->width, l->height));

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
gen(Level *l)
{
	int i, q, rnd, space;
	Point pup, pdown, p;
	Tile *t;

	space = l->width*l->height;

	pup = Pt(nrand(l->width), nrand(l->height));
	tileat(l, pup)->feat = TUPSTAIR;
	setflagat(l, pup, Fhasfeature);
	l->up = pup;

	space--;

	while(1){
		pdown = Pt(nrand(l->width), nrand(l->height));
		/* already upstair? */
		if(flagat(l, pdown) & Fhasfeature)
			continue;
		/* too close? */
		p = subpt(pup, pdown);
		if(sqrt(p.x*p.x+p.y*p.y) < 8.0)
			continue;
		setflagat(l, pdown, Fhasfeature);
		tileat(l, pdown)->feat = TDOWNSTAIR;
		l->down = pdown;
		break;
	}

	space--;

	/* place an arbitrary amount of trees */
	q = (l->width*l->height) / 10;
	if(q < 3)
		q = 1;
	rnd = nrand(q)+q;
	for(i = 0; i <= rnd && space > 10; i++){
		do {
			p = Pt(nrand(l->width), nrand(l->height));
		} while(hasflagat(l, p, Fblocked|Fhasfeature));

		t = tileat(l, p);
		t->feat = TTREE;
		flagat(l, p) = (Fblocked|Fhasfeature);

		space--;
	}

	/* some monsters */
	rnd = nrand(q)+q;
	for(i = 0; i < rnd && space > 10; i++){
		do {
			p = Pt(nrand(l->width), nrand(l->height));
		} while(hasflagat(l, p, Fblocked|Fhasfeature));

		t = tileat(l, p);
		q = nrand(4)+TSOLDIER;
		t->unit = q; //nrand(320);
		t->monst = monst(q);
		setflagat(l, p, Fblocked|Fhasmonster);

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

