#include <u.h>
#include <libc.h>
#include <draw.h>

#include "dat.h"

static void
gen(Level *l)
{
	int i, q;
	Tile *t;

	tileat(l, Pt(0, 0))->feat = TUPSTAIR;
	tileat(l, Pt(l->width-1, l->height-1))->feat = TDOWNSTAIR;

	q = (l->width*l->height) / 3;
	if(q < 3)
		q = 1;
	for(i = 0; i <= nrand(q)+(q/2); i++){
		again:
		t = tileat(l, Pt(nrand(l->width), nrand(l->height)));
		if(t->feat)
			goto again;

		t->feat = TTREE;
	}
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

