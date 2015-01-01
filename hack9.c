#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <keyboard.h>
#include <mouse.h>

#include "dat.h"

enum
{
	LSIZE	= 10,
};

Mousectl *mc;
Keyboardctl *kc;

Tileset *tiles;
Level *level;

/* in tiles */
Point campos;

/* of player */
Point pos;

static int
min(int a, int b)
{
	if(a < b)
		return a;
	return b;
}

/* draw the whole level on the screen using tileset ts.
 * r.min is the upper left visible tile, and r.max is
 * the bottom right visible tile.
 * */
static void
drawlevel(Level *l, Tileset *ts, Rectangle r)
{
	int x, y, what;
	Point xy, tpos;
	Tile *t;

	draw(screen, screen->r, display->black, nil, ZP);

	for(x = r.min.x; x < r.max.x; x++){
		for(y = r.min.y; y < r.max.y; y++){
			/* world cell */
			xy = (Point){x, y};
			/* screen space */
			tpos = xy;
			tpos = addpt(tpos, campos);
			tpos.x *= ts->width;
			tpos.y *= ts->height;
			tpos = addpt(tpos, screen->r.min);
			t = tileat(l, xy);
			if(t){
				what = t->terrain;
				if(t->unit)
					what = t->unit;
				else if(t->feat)
					what = t->feat;
				drawtile(ts, screen, tpos, what);
			}
		}
	}
}

Rectangle
view(Point pos, Level *l, int scrwidth, int scrheight)
{
	Point p;
	Rectangle r;

	/* compute vieweable area of the map */
	p = Pt((scrwidth/2)+2, (scrheight/2)+2);
	r = Rpt(subpt(pos, p), addpt(pos, p));
	rectclip(&r, Rect(0, 0, l->width, l->height));
	return r;
}

enum
{
	DUMMY,
	WEST,
	NORTH,
	EAST,
	SOUTH,
	UP,
	DOWN,
};

/* attempt to move the monster/player at src in dir direction
 * return delta in Point if moved, ZP if not */
static Point
move(Point src, int dir)
{
	int *adj, c, lim, what;
	Point dst;
	Tile *t, *t2;
	Monster *m;

	adj = nil;
	c = lim = 0;
	dst = src;

	switch(dir){
	default:
		fprint(2, "unknown move %d\n", dir);
	case DUMMY:
		return ZP;
		break;
	case NORTH:
		c = -1;
	if(0){
	case SOUTH:
		c = 1;
	}
		lim = level->height;
		adj = &dst.y;
		break;
	case WEST:
		c = -1;
	if(0){
	case EAST:
		c = 1;
	}
		lim = level->width;
		adj = &dst.x;
		break;

	/* special cases; use stairs, change level */
	case UP:
	case DOWN:
		t = tileat(level, src);
		what = t->unit;
		if(t->feat == (dir==UP ? TUPSTAIR : TDOWNSTAIR)){
			m = t->monst;
			t->monst = nil;
			freelevel(level);
			if((level = genlevel(nrand(LSIZE)+LSIZE, nrand(LSIZE)+LSIZE)) == nil)
				sysfatal("genlevel: %r");

			if(dir == UP)
				dst = level->down;
			else
				dst = level->up;

			t = tileat(level, dst);
			t->unit = what;
			t->monst = m;
			setflagat(level, dst, Fhasmonster|Fblocked);
			return subpt(src, dst);
		}
		break;
	}

	if(adj && *adj + c >= 0 && *adj + c < lim){
		*adj += c;
		t = tileat(level, dst);
		if(eqpt(src, pos) && hasflagat(level, dst, Fblocked) && t->monst != nil){
			/* monster; attack it */
			if(--t->monst->hp == 0){
				free(t->monst);
				t->monst = nil;
				t->unit = 0;
				clrflagat(level, dst, Fhasmonster|Fblocked);
			}

			/* no movement */
			return ZP;
		}
		if(!hasflagat(level, dst, Fblocked)){
			/* move */
			t2 = tileat(level, src);
			setflagat(level, dst, (Fhasmonster|Fblocked));
			clrflagat(level, src, (Fhasmonster|Fblocked));
			t->unit = t2->unit;
			t2->unit = 0;
			t->monst = t2->monst;
			t2->monst = nil;
			return subpt(src, dst);
		}
	}

	return ZP;
}

void
movemons(void)
{
	int i, movdir, nmove, npath;
	Point p, p2, *tomove, *path;

	nmove = 0;
	for(p.x = 0; p.x < level->width; p.x++){
		for(p.y = 0; p.y < level->height; p.y++){
			if(!eqpt(p, pos) && hasflagat(level, p, Fhasmonster))
				nmove++;
		}
	}

	tomove = mallocz(sizeof(Point)*nmove, 1);
	if(tomove == nil)
		sysfatal("movemons: %r");

	i = 0;
	for(p.x = 0; p.x < level->width; p.x++){
		for(p.y = 0; p.y < level->height; p.y++){
			if(!eqpt(p, pos) && hasflagat(level, p, Fhasmonster))
				tomove[i++] = p;
		}
	}

	for(i = 0; i < nmove; i++){
		p = tomove[i];
		if(manhattan(p, pos) < 6 * ORTHOCOST){
			/* move the monster toward player */
			npath = pathfind(level, p, pos, &path);
			if(npath >= 0){
				/* step once along path, path[0] is cur pos */
				p2 = subpt(p, path[1]);
				double ang = atan2(p2.y, p2.x);
				movdir = (int)(4 * ang / (2*PI) + 4.5) % 4;
				p2 = move(p, movdir+1);
				free(path);
			}
		} else {
			/* random move */
			move(p, nrand(4)+1);
		}
	}

	free(tomove);
}

void
threadmain(int argc, char *argv[])
{
	Rune c;
	
	/* of viewport, in size of tiles */
	int width, height, movdir;
	Rectangle r;
	Point p;
	Tile *t;

	ARGBEGIN{
	}ARGEND

	srand(truerand());

	if(initdraw(nil, nil, argv0) < 0)
		sysfatal("initdraw: %r");

	if((mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");

	if((kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");

	if((tiles = opentile("nethack.32x32", 32, 32)) == nil)
		sysfatal("opentile: %r");

	if((level = genlevel(nrand(LSIZE)+LSIZE, nrand(LSIZE)+LSIZE)) == nil)
		sysfatal("genlevel: %r");

	/* the player */
	pos = level->up;
	t = tileat(level, pos);
	t->unit = TWIZARD;
	setflagat(level, pos, Fhasmonster|Fblocked);

	width = Dx(screen->r) / tiles->width;
	height = Dy(screen->r) / tiles->height;

	r = view(pos, level, width, height);
	campos = subpt(Pt(width/2, height/2), pos);

	drawlevel(level, tiles, r);
	flushimage(display, 1);

	enum { AMOUSE, ARESIZE, AKEYBOARD, AEND };
	Alt a[AEND+1] = {
		{ mc->c,		nil,	CHANRCV },
		{ mc->resizec,	nil,	CHANRCV },
		{ kc->c,		&c,		CHANRCV },
		{ nil,			nil,	CHANEND },
	};

	for(;;){
		switch(alt(a)){
		case AMOUSE:
			break;
		case ARESIZE:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			width = Dx(screen->r) / tiles->width;
			height = Dy(screen->r) / tiles->height;

			r = view(pos, level, width, height);
			campos = subpt(Pt(width/2, height/2), pos);

			drawlevel(level, tiles, r);
			flushimage(display, 1);
			break;
		case AKEYBOARD:
			movdir = DUMMY;
			switch(c){
			case Kdel:
				threadexitsall(nil);
				break;
			case 'h':
			case Kleft:
				movdir = WEST;
				break;
			case 'j':
			case Kdown:
				movdir = SOUTH;
				break;
			case 'k':
			case Kup:
				movdir = NORTH;
				break;
			case 'l':
			case Kright:
				movdir = EAST;
				break;
			case '<':
				movdir = UP;
				break;
			case '>':
				movdir = DOWN;
				break;
			case '.':
				movdir = DUMMY;
				break;
			default:
				/* don't waste a move */
				continue;
			}

			if(movdir != DUMMY){
				p=move(pos, movdir);
				if(!eqpt(p, ZP)){
					pos = subpt(pos, p);
				}
			}

			static int turns=0;
			if(++turns % 3 ==0){
				movemons();
			}

			r = view(pos, level, width, height);
			campos = subpt(Pt(width/2, height/2), pos);
			drawlevel(level, tiles, r);
			flushimage(display, 1);
			break;
		}
	}
}

