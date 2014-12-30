#include <u.h>
#include <libc.h>
#include <draw.h>

#include "dat.h"

static void
clear(Level *l, Point p, int dist)
{
	int x, y;
	Rectangle r;
	Tile *t;

	r = Rect(p.x-dist, p.y-dist, p.x+dist, p.y+dist);
	rectclip(&r, Rect(0, 0, l->width, l->height));

	for(x = r.min.x; x <= r.max.x; x++){
		for(y = r.min.y; y <= r.max.y; y++){
			p = (Point){x, y};
			t = tileat(l, p);
			if(t->feat != TUPSTAIR && t->feat != TDOWNSTAIR){
				t->feat = 0;
				t->blocked = 0;
			}
		}
	}

}

static void
gen(Level *l)
{
	int i, q;
	Point pup, pdown, p;
	Tile *t;

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

	/* place an arbitrary amount of trees */
	q = (l->width*l->height) / 2;
	if(q < 3)
		q = 1;
	for(i = 0; i <= nrand(q)+(q/2); i++){
again2:
		t = tileat(l, Pt(nrand(l->width), nrand(l->height)));
		if(t->blocked || t->feat)
			goto again2;

		t->feat = TTREE;
		t->blocked = 1;
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
	free(l->tiles);
}

Tile*
tileat(Level *l, Point p)
{
	return l->tiles+(p.y*l->width)+p.x;
}

