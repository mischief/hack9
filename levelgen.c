#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "map.h"
#include "bt.h"

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
lfeature(Level *l, Point p, int type, int flags)
{
	Tile *t;

	t = tileat(l, p);
	t->feat = type;

	flagat(l, p) = flags;
}

static void
lfeatrect(Level *l, Rectangle r, int type, int flags)
{
	Point p;

	for(p.x = r.min.x; p.x <= r.max.x; p.x++)
		for(p.y = r.min.y; p.y <= r.max.y; p.y++)
			lfeature(l, p, type, flags);
}

static void
lportal(Level *l, Point p, int feat, Level *to, Point tp)
{
	Tile *t;
	Portal *port;

	lfeature(l, p, feat, Fhasfeature);

	port = emalloc(sizeof(Portal));
	port->to = to;
	port->pt = tp;

	t = tileat(l, p);

	t->portal = port;
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
	lfeatrect(l, tmp, type, Fblocked|Fhasfeature);

	tmp = Rect(r.min.x, r.max.y-i, r.max.x, r.max.y);
	lclearrect(l, tmp);
	lfeatrect(l, tmp, type, Fblocked|Fhasfeature);

	tmp = Rect(r.min.x, r.min.y+i, r.min.x+i, r.max.y-i);
	lclearrect(l, tmp);
	lfeatrect(l, tmp, type, Fblocked|Fhasfeature);

	tmp = Rect(r.max.x-i, r.min.y+i, r.max.x, r.max.y-i);
	lclearrect(l, tmp);
	lfeatrect(l, tmp, type, Fblocked|Fhasfeature);
}

static void
lspawn(Level *l, Point p, char *what, int freq, int istrap)
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

	lportal(l, pup, TUPSTAIR, nil, ZP);
	lportal(l, pdown, TDOWNSTAIR, nil, ZP);
}

static int
lgen(Level *l, int type)
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
lgenerate(int width, int height, int type)
{
	Level *l;

	l = nil;

	do {
		if(l != nil)
			lfree(l);

		l = lnew(width, height, TFLOOR, "somewhere");
	} while(lgen(l, type) < 0);

	return l;
}
/* more sbias makes it straighter. */
static void
drunk1(Level *l, Point p, uint count, uint sbias)
{
	int cur, last;
	Point next;
	Rectangle clipr;
	Tile *t;

	clipr = insetrect(l->or, 1);

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

	cnt = ((l->width * l->height) / howmuch); //* 2;
	redos = 0;

redo:
	if(redos++ > 10)
		return 0;
	/* fill */
	for(p.x = 0; p.x < l->width; p.x++){
		for(p.y = 0; p.y < l->height; p.y++){
			if(!hasflagat(l, p, Fblocked|Fhasfeature))
				lfeature(l, p, type, Fblocked|Fhasfeature);
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
		t->monst = mcreate(l, p, type);
		t->unit = t->monst->md->tile;
		setflagat(l, p, Fhasmonster);
	}
}

int
mkldebug(Level *l)
{
	Tile *t;

	drunken(l, TTREE, 3, 3, 3);
	lclear(l, l->up, 1);
	lclear(l, l->down, 1);
	lmkmons(l, &l->down, 1, "lich", 1);

	t = tileat(l, l->up);
	setflagat(l, l->up, Fhasitem);
	iladd(&t->items, ibyname("shield"));
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
	lmkmons(l, &p, 1, "captain", 0);
	lmkmons(l, &p, 1, "lieutenant", 1);
	lspawn(l, p, "soldier", 57, 2);
	lspawn(l, p, "sergeant", 91, 1);
	lspawn(l, p, "lieutenant", 97, 1);
	lspawn(l, p, "captain", 131, 1);

	lclear(l, p2, 2);
	lmkmons(l, &p2, 1, "gnome king", 0);
	lmkmons(l, &p2, 1, "gnome wizard", 1);
	lspawn(l, p2, "gnome", 13, 1);
	lspawn(l, p2, "gnome lord", 17, 2);
	lspawn(l, p2, "gnome wizard", 27, 1);
	lspawn(l, p2, "gnome king", 101, 1);

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

	lspawn(l, p, "lich", 101, 1);
	lspawn(l, p, "demilich", 157, 0);
	lspawn(l, p, "ghost", 15, 2);
	lspawn(l, p, "human zombie", 27, 2);
	lspawn(l, p, "skeleton", 71, 1);

	lmkmons(l, &p2, 1, "gnome king", 0);
	lmkmons(l, &p2, 1, "gnome wizard", 1);
	lspawn(l, p2, "gnome", 29, 1);
	lspawn(l, p2, "gnome lord", 45, 2);
	lspawn(l, p2, "gnome wizard", 57, 2);
	lspawn(l, p2, "gnome king", 71, 1);

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

	lspawn(l, p, "large cat", 77, 0);
	lspawn(l, p2, "gnome lord", 27, 2);

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

	lborder(l, mid, 1, TTLCORNER);
	lborder(l, mid, -1, TWATER);

	/* create path in */
	p = (Point){mid.min.x, mid.max.y};
	p.x += nrand(Dx(mid)-2)+2;

	lclearrect(l, Rpt(p, addpt(p, Pt(0,1))));

	lspawn(l, addpt(mid.min, Pt(Dx(mid)/2, Dy(mid)/2)), "soldier", 21, 0);

	l->up = Pt(1,1);
	l->down = Pt(4,1);

	return 0;
}

