#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "map.h"
#include "bt.h"

#include "dat.h"
#include "alg.h"

static Behavior *behavior;

/* AI function for levels */
static int
lexec(void *v)
{
	int i, j, n;
	Point p, np, *neigh;
	Level *l;
	Spawn *sp;
	Tile *t;
	MonsterData *md;
	Monster *m;
	Item *it;

	l = v;

	for(i = 0; i < l->nspawns; i++){
		sp = &l->spawns[i];
		if(sp->turn++ % sp->freq == 0){
			p = sp->pt;

			/* le trap */
			if(sp->istrap > 0){
				neigh = neighbor(l->r, p, &n);
				for(j = 0; j < n; j++){
					np = neigh[j];
					if(hasflagat(l, np, Fhasmonster)){
						t = tileat(l, np);
						m = t->monst;
						md = mdbyname(sp->what);
						if(md == nil){
							dbg("programming error: monster %s not found: %r", sp->what);
							continue;
						}
						if(strcmp(m->md->name, sp->what) != 0 && abs(m->align - md->align) > 15){
							lmkmons(l, &p, 1, sp->what, sp->istrap);
						}
					}
				}

				free(neigh);
			}
			lmkmons(l, &p, 1, sp->what, 0);
		}
	}

	for(p.x = 0; p.x < l->width; p.x++){
		for(p.y = 0; p.y < l->height; p.y++){
			if(hasflagat(l, p, Fhasitem)){
				t = tileat(l, p);
				for(i = 0; i < t->items.count; i++){
					it = ilnth(&t->items, i);
					if(++it->age > IMAXAGE){
						ifree(iltakenth(&t->items, i));
						if(t->items.count == 0)
							clrflagat(l, p, Fhasitem);
					}
				}
			}
		}
	}

	return TASKSUCCESS;
}

void
linit(void)
{
	BehaviorNode *root;

	root = btleaf("level", lexec);

	assert(root != nil);

	behavior = btroot(root);

	assert(behavior != nil);
}

Level*
lnew(int width, int height, int floor, char *name)
{
	int i;
	Point p;
	Level *l;

	l = mallocz(sizeof(Level), 1);
	if(l == nil)
		return nil;

	snprint(l->name, sizeof(l->name), "%s", name);

	l->width = width;
	l->height = height;
	l->r = Rect(0, 0, width-1, height-1);
	l->or = Rect(0, 0, width, height);

	l->tiles = mallocz(sizeof(Tile) * width * height, 1);
	if(l->tiles == nil)
		goto err0;

	l->flags = mallocz(sizeof(int) * width * height, 1);
	if(l->flags == nil)
		goto err1;

	l->bt = behavior;
	l->bs = btstatenew(behavior);
	if(l->bs == nil)
		goto err2;

	for(p.x = 0; p.x < width; p.x++){
		for(p.y = 0; p.y < height; p.y++){
			tileat(l, p)->terrain = floor;
		}
	}

	for(i = 0; i < NCARDINAL; i++)
		l->exits[i] = pickpoint(l->r, i, 2);

	return l;

err2:
	free(l->flags);
err1:
	free(l->tiles);
err0:
	free(l);
	return nil;
}

void
lfree(Level *l)
{
	int x, y;
	Point p;
	Tile *t;

	btstatefree(l->bs, l);

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

/*
 * lmkmons is a recursive function, that spawns monsters
 * centered at p[0] expaning outward according to a
 * von neumann neighborhood with radius r. for example:
 *	lmkmons(l, p, 1, TGHOST, 2);
 * spawns 13 ghosts centered at p on l.
 */
void
lmkmons(Level *l, Point *p, int count, char *type, int r)
{
	int i, j, n;
	Point *neigh;
	Tile *t;
	for(i = 0; i < count; i++){
		if(!hasflagat(l, p[i], Fhasmonster|Fblocked)){
			t = tileat(l, p[i]);
			t->monst = mcreate(l, p[i], type);
			t->unit = t->monst->md->tile;
			setflagat(l, p[i], Fhasmonster);
		}
		if(r > 0){
			neigh = neighbor(l->r, p[i], &n);
			for(j = 0; j < n; j++){
				lmkmons(l, neigh, n, type, r-1);
			}
			free(neigh);
		}
	}
}

Tile*
tileat(Level *l, Point p)
{
	return l->tiles+(p.y*l->width)+p.x;
}

