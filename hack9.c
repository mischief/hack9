#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <keyboard.h>
#include <mouse.h>

#include "dat.h"

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
	NORTH,
	SOUTH,
	EAST,
	WEST,
	UP,
	DOWN,
};

void
move(int dir)
{
	int *adj, c, lim;
	Point dst;
	Tile *t;

	adj = nil;
	c = lim = 0;
	dst = pos;

	switch(dir){
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

	/* special cases; use stairs */
	case UP:
	case DOWN:
		t = tileat(level, pos);
		if(t->feat == (dir==UP ? TUPSTAIR : TDOWNSTAIR)){
			free(level);
			if((level = genlevel(nrand(10)+10, nrand(10)+10)) == nil)
				sysfatal("genlevel: %r");

			if(dir == UP)
				pos = level->down;
			else
				pos = level->up;

			tileat(level, pos)->unit = TWIZARD;
		}
		break;
	}

	if(adj && *adj + c >= 0 && *adj + c < lim){
		*adj += c;
		t = tileat(level, dst);
		if(!t->blocked){
			tileat(level, pos)->unit = 0;
			tileat(level, dst)->unit = TWIZARD;
			pos = dst;
		}
	}
}

void
threadmain(int argc, char *argv[])
{
	Rune c;
	
	/* of viewport, in size of tiles */
	int width, height;
	Rectangle r;
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

	if((level = genlevel(nrand(10)+10, nrand(10)+10)) == nil)
		sysfatal("genlevel: %r");

	pos = level->up;
	t = tileat(level, pos);
	t->unit = TWIZARD;

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
			switch(c){
			case Kdel:
				threadexitsall(nil);
				break;
			case 'h':
				move(WEST);
				break;
			case 'j':
				move(SOUTH);
				break;
			case 'k':
				move(NORTH);
				break;
			case 'l':
				move(EAST);
				break;
			case '<':
				move(UP);
				break;
			case '>':
				move(DOWN);
				break;
			}

			r = view(pos, level, width, height);
			campos = subpt(Pt(width/2, height/2), pos);
			drawlevel(level, tiles, r);
			flushimage(display, 1);
			break;
		}
	}
}

