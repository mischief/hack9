#include <u.h>
#include <libc.h>
#include <draw.h>

#include "dat.h"

static void
clear(Level *l, Point p, int dist)
{
	int x, y;
	Point p2;
	Rectangle r;
	Tile *t;

	r = Rect(p.x-dist, p.y-dist, p.x+dist+1, p.y+dist+1);
	rectclip(&r, Rect(0, 0, l->width, l->height));

	for(x = r.min.x; x < r.max.x; x++){
		for(y = r.min.y; y < r.max.y; y++){
			p2 = (Point){x, y};
			if(eqpt(p, p2))
				continue;
			t = tileat(l, p2);
			t->unit = 0;
			t->feat = 0;
			t->blocked = 0;
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
	l->up = pup;

again1:
	pdown = Pt(nrand(l->width), nrand(l->height));
	t = tileat(l, pdown);
	/* already upstair? */
	if(t->feat)
		goto again1;
	/* too close? */
	p = subpt(pup, pdown);
	if(sqrt(p.x*p.x+p.y*p.y) < 8.0)
		goto again1;
	t->feat = TDOWNSTAIR;
	l->down = pdown;

	space -= 2;

	/* place an arbitrary amount of trees */
	q = (l->width*l->height) / 4;
	if(q < 3)
		q = 1;
	rnd = nrand(q) + q/2;
	for(i = 0; i <= rnd && space > 10; i++){
		do {
			t = tileat(l, Pt(nrand(l->width), nrand(l->height)));
		} while(t->blocked || t->feat);

		t->feat = TTREE;
		t->blocked = 1;

		space--;
	}

	/* some monsters */
	q = nrand(10)+10;
	rnd = nrand(q) + q/2;
	for(i = 0; i < rnd && space > 10; i++){
		do {
			t = tileat(l, Pt(nrand(l->width), nrand(l->height)));
		} while(t->blocked || t->feat);

		t->unit = TLARGECAT; //nrand(320);
		t->monst = monst(TLARGECAT);
		t->blocked = 1;

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

	for(x = 0; x < width; x++){
		for(y = 0; y < height; y++){
			tileat(l, Pt(x, y))->terrain = TFLOOR;
		}
	}

	gen(l);

	return l;

err0:
	free(l);
	return nil;
}

void
freelevel(Level *l)
{
	int x, y;
	Tile *t;

	for(x = 0; x < l->width; x++){
		for(y = 0; y < l->height; y++){
			t = tileat(l, Pt(x, y));
			if(t->monst != nil)
				free(t->monst);
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

