#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "dat.h"
#include "alg.h"

/* removes all monsters/features near p, except p itself */
static void
lclear(Level *l, Point p, int dist)
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

static void
lclearrect(Level *l, Rectangle r)
{
	Point p;
	Tile *t;

	rectclip(&r, l->r);

	for(p.x = r.min.x; p.x <= r.max.x; p.x++){
		for(p.y = r.min.y; p.y <= r.max.y; p.y++){
			t = tileat(l, p);
			t->unit = t->feat = 0;
			if(t->monst != nil){
				mfree(t->monst);
				t->monst = nil;
			}
			if(t->portal != nil){
				free(t->portal);
				t->portal = nil;
			}
			flagat(l, p) = 0;
		}
	}
}

static void
lfillrect(Level *l, Rectangle r, int type)
{
	Point p;
	Tile *t;

	for(p.x = r.min.x; p.x <= r.max.x; p.x++){
		for(p.y = r.min.y; p.y <= r.max.y; p.y++){
			t = tileat(l, p);
			t->feat = type;
			flagat(l, p) = (Fblocked|Fhasfeature);
		}
	}
}

static void
lborder(Level *l, Rectangle r, int i, int type)
{
	Rectangle tmp;

	assert(i != 0);

	if(i < 0){
		r = insetrect(r, i);
		i = -i;
	}

	i -= 1;

	tmp = Rect(r.min.x, r.min.y, r.max.x, r.min.y+i);
	lclearrect(l, tmp);
	lfillrect(l, tmp, type);

	tmp = Rect(r.min.x, r.max.y-i, r.max.x, r.max.y);
	lclearrect(l, tmp);
	lfillrect(l, tmp, type);

	tmp = Rect(r.min.x, r.min.y+i, r.min.x+i, r.max.y-i);
	lclearrect(l, tmp);
	lfillrect(l, tmp, type);

	tmp = Rect(r.max.x-i, r.min.y+i, r.max.x, r.max.y-i);
	lclearrect(l, tmp);
	lfillrect(l, tmp, type);
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
	int cnt, npath, redos;
	Point p, *path;
	Tile *t;

	cnt = ((l->width * l->height) / howmuch); //* 2;
	redos = 0;

redo:
	if(redos++ > 10)
		return 0;
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
mkmons(Level *l, Point p, char *type)
{
	Monster *m;
	m = mbyname(type);
	if(m == nil)
		sysfatal("monstbyname: %r");
	m->l = l;
	m->pt = p;
	/* setup ai state */
	idle(m);
	return m;
}

/*
 * several is a recursive function, that spawns monsters
 * centered at p[0] expaning outward according to a
 * von neumann neighborhood with radius r. for example:
 *	several(l, p, 1, TGHOST, 2);
 * spawns 13 ghosts centered at p on l.
 */
void
several(Level *l, Point *p, int count, char *type, int r)
{
	int i, j, n;
	Point *neigh;
	Tile *t;
	for(i = 0; i < count; i++){
		if(!hasflagat(l, p[i], Fhasmonster|Fblocked)){
			t = tileat(l, p[i]);
			t->monst = mkmons(l, p[i], type);
			t->unit = t->monst->md->tile;
			setflagat(l, p[i], Fhasmonster);
		}
		if(r > 0){
			neigh = neighbor(l->r, p[i], &n);
			for(j = 0; j < n; j++){
				several(l, neigh, n, type, r-1);
			}
			free(neigh);
		}
	}
}

/* genmonsters spawns count of type monsters at random places on l. */
static void
genmonsters(Level *l, char *type, int count)
{
	int i;
	Point p;
	Tile *t;

	for(i = 0; i < count; i++){
		do{
			p = (Point){nrand(l->width), nrand(l->height)};
		}while(hasflagat(l, p, Fhasmonster|Fhasfeature|Fblocked));

		t = tileat(l, p);
		t->monst = mkmons(l, p, type);
		t->unit = t->monst->md->tile;
		setflagat(l, p, Fhasmonster);
	}
}

static void
levelexec(AIState *a)
{
	int i, j, n;
	Point p, np, *neigh;
	Level *l;
	Spawn *sp;
	Tile *t;
	MonsterData *md;
	Monster *m;

	l = a->aux;

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
							several(l, &p, 1, sp->what, sp->istrap);
						}
					}
				}

				free(neigh);
			}
			several(l, &p, 1, sp->what, 0);
		}
	}
}

Level*
mklevel(int width, int height, int floor)
{
	Point p;
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

	l->ai = mkstate("level", nil, l, nil, levelexec, nil);
	if(l->ai == nil)
		goto err2;

	for(p.x = 0; p.x < width; p.x++){
		for(p.y = 0; p.y < height; p.y++){
			tileat(l, p)->terrain = floor;
		}
	}

	return l;

err2:
	free(l->flags);
err1:
	free(l->tiles);
err0:
	free(l);
	return nil;
}

int
mkldebug(Level *l)
{
	drunken(l, TTREE, 3, 3, 3);
	lclear(l, l->up, 1);
	lclear(l, l->down, 1);
	several(l, &l->down, 1, "lich", 1);
	return 0;
}

int
mklforest(Level *l)
{
	int space;
	Point p, p2;
	Rectangle in;
	
	in = insetrect(l->r, 3);

	space = drunken(l, TTREE, 2, 0, 0);
	lclear(l, l->up, 1);
	lclear(l, l->down, 1);

	do {
		p = (Point){nrand(Dx(in))+3, nrand(Dy(in))+3};
	} while(flagat(l, p) != 0);
	do {
		p2 = (Point){nrand(Dx(in))+3, nrand(Dy(in))+3};
	} while(flagat(l, p2) != 0 || manhattan(p, p2) < ORTHOCOST*10 || manhattan(p, p2) > ORTHOCOST*20);

	lclear(l, p, 2);
	several(l, &p, 1, "captain", 0);
	several(l, &p, 1, "lieutenant", 1);
	addspawn(l, p, "soldier", 57, 2);
	addspawn(l, p, "sergeant", 91, 1);
	addspawn(l, p, "lieutenant", 97, 1);
	addspawn(l, p, "captain", 131, 1);

	lclear(l, p2, 2);
	several(l, &p2, 1, "gnome king", 0);
	several(l, &p2, 1, "gnome wizard", 1);
	addspawn(l, p2, "gnome", 13, 1);
	addspawn(l, p2, "gnome lord", 17, 2);
	addspawn(l, p2, "gnome wizard", 27, 1);
	addspawn(l, p2, "gnome king", 101, 1);

	/* don't start empty */
	genmonsters(l, "gnome lord", space/128);
	genmonsters(l, "sergeant", space/128);
	return 0;
}

int
mklgraveyard(Level *l)
{
	int space;
	Point p, p2;
	Rectangle in;
	
	in = insetrect(l->r, 3);

	space = drunken(l, TGRAVE, 2, 4, 4);
	lclear(l, l->up, 2);
	lclear(l, l->down, 2);

	do {
		p = (Point){nrand(Dx(in))+3, nrand(Dy(in))+3};
	} while(flagat(l, p) != 0);
	do {
		p2 = (Point){nrand(Dx(in))+3, nrand(Dy(in))+3};
	} while(flagat(l, p2) != 0 || manhattan(p, p2) < ORTHOCOST*15 || manhattan(p, p2) > ORTHOCOST*20);

	lclear(l, p, 2);
	lclear(l, p2, 2);

	addspawn(l, p, "lich", 101, 1);
	addspawn(l, p, "demilich", 157, 0);
	addspawn(l, p, "ghost", 39, 2);
	addspawn(l, p, "human zombie", 43, 2);
	addspawn(l, p, "skeleton", 127, 1);

	several(l, &p2, 1, "gnome king", 0);
	several(l, &p2, 1, "gnome wizard", 1);
	addspawn(l, p2, "gnome", 23, 1);
	addspawn(l, p2, "gnome lord", 25, 2);
	addspawn(l, p2, "gnome wizard", 37, 2);
	addspawn(l, p2, "gnome king", 53, 1);

	genmonsters(l, "ghost", space/64);
	genmonsters(l, "gnome", space/64);

	return 0;
}

int
mklvolcano(Level *l)
{
	int space;
	Point p, p2;
	Rectangle in;
	
	in = insetrect(l->r, 3);

	space = drunken(l, TLAVA, 2, 1, 1);
	lclear(l, l->up, 2);
	lclear(l, l->down, 2);

	do {
		p = (Point){nrand(Dx(in))+3, nrand(Dy(in))+3};
	} while(flagat(l, p) != 0);
	do {
		p2 = (Point){nrand(Dx(in))+3, nrand(Dy(in))+3};
	} while(flagat(l, p2) != 0 || manhattan(p, p2) < ORTHOCOST*10 || manhattan(p, p2) > ORTHOCOST*20);

	lclear(l, p, 2);
	lclear(l, p2, 2);

	addspawn(l, p, "large cat", 77, 0);
	addspawn(l, p2, "gnome lord", 27, 2);

	genmonsters(l, "large cat", space/128);
	genmonsters(l, "gnome lord", space/64);
	return 0;
}

int
mklcastle(Level *l)
{
	int n;
	Point p;
	Rectangle mid;

	mid = l->r;

	n = Dx(l->r)/5;
	mid.min.x += n;
	mid.max.x -= n;
	n = Dy(l->r)/5;
	mid.min.y += n;
	mid.max.y -= n;

	lborder(l, mid, 1, TWALL);
	lborder(l, mid, -1, TWATER);

	/* create path in */
	p = (Point){mid.min.x, mid.max.y};
	p.x += nrand(Dx(mid)-2)+2;

	lclearrect(l, Rpt(p, addpt(p, Pt(0,1))));

	addspawn(l, addpt(mid.min, Pt(Dx(mid)/2, Dy(mid)/2)), "soldier", 21, 0);

	l->up = Pt(1,1);
	l->down = Pt(4,1);

	return 0;
}

