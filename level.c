#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "dat.h"
#include "alg.h"

void
addspawn(Level *l, Point p, char *what, int freq, int istrap)
{
	Spawn sp;

	if(l->nspawns < nelem(l->spawns)-1){
		sp.pt = p;
		strcpy(sp.what, what);
		sp.freq = freq;
		sp.turn = nrand(1000);
		sp.istrap = istrap;
		l->spawns[l->nspawns++] = sp;
		tileat(l, p)->feat = TSPAWN;
		setflagat(l, p, Fhasfeature);
	}
}

static void
mkportal(Level *l, Point p, int feat, Level *to, Point tp)
{
	Tile *t;

	flagat(l, p) = (Fhasfeature|Fportal);
	t = tileat(l, p);
	t->feat = feat;
	t->portal = emalloc(sizeof(Portal));
	t->portal->to = to;
	t->portal->pt = tp;
}

static void
lrandstairs(Level *l)
{
	Rectangle in;
	Point pup, pdown;

	in = insetrect(l->r, 3);

	pup = (Point){nrand(Dx(in))+3, nrand(Dy(in))+3};
	do {
		pdown = (Point){nrand(Dx(in))+3, nrand(Dy(in))+3};
	} while(eqpt(pdown, pup) || manhattan(pdown, pup) < ORTHOCOST*15);

	l->up = pup;
	l->down = pdown;

	mkportal(l, pup, TUPSTAIR, nil, ZP);
	mkportal(l, pdown, TDOWNSTAIR, nil, ZP);
}

static int
gen(Level *l, int type)
{
	lrandstairs(l);

	switch(type){
	case 0:
		mkldebug(l);
		break;
	case 1:
		mklforest(l);
		break;
	case 2:
		mklgraveyard(l);
		break;
	case 3:
		mklvolcano(l);
		break;
	}

	return 0;
}

Level*
genlevel(int width, int height, int type)
{
	Level *l;

	l = nil;

	do {
		if(l != nil)
			freelevel(l);

		l = mklevel(width, height, TFLOOR);
	} while(gen(l, type) < 0);

	return l;
}

void
freelevel(Level *l)
{
	int x, y;
	Point p;
	Tile *t;

	for(x = 0; x < l->width; x++){
		for(y = 0; y < l->height; y++){
			p = (Point){x, y};
			t = tileat(l, p);
			if(hasflagat(l, p, Fhasmonster))
				mfree(t->monst);
			if(hasflagat(l, p, Fportal))
				free(t->portal);
			if(hasflagat(l, p, Fhasitem)){
				ilfree(&t->items);
			}
		}
	}

	free(l->tiles);
	free(l->flags);
	if(l->ai != nil)
		freestate(l->ai);
	free(l);
}

Tile*
tileat(Level *l, Point p)
{
	return l->tiles+(p.y*l->width)+p.x;
}

