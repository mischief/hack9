#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "map.h"
#include "bt.h"

#include "dat.h"
#include "alg.h"

void
lfree(Level *l)
{
	int x, y;
	Point p;
	Tile *t;

	btfree(l->bt, l);

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
	free(l);
}

Tile*
tileat(Level *l, Point p)
{
	return l->tiles+(p.y*l->width)+p.x;
}

