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

void
threadmain(int argc, char *argv[])
{
	Rune c;
	
	/* of viewport, in size of tiles */
	int width, height;
	Point pt;
	Rectangle r;

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

	if((level = genlevel(10, 10)) == nil)
		sysfatal("genlevel: %r");

	pos = Pt(0, 0);
	tileat(level, pos)->unit = TWIZARD;

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
				if(pos.x-1 >= 0){
					tileat(level, pos)->unit = 0;
					pos.x -= 1;
					tileat(level, pos)->unit = TWIZARD;
				}
				break;
			case 'j':
				if(pos.y+1 < level->height){
					tileat(level, pos)->unit = 0;
					pos.y += 1;
					tileat(level, pos)->unit = TWIZARD;
				}
				break;
			case 'k':
				if(pos.y-1 >= 0){
					tileat(level, pos)->unit = 0;
					pos.y -= 1;
					tileat(level, pos)->unit = TWIZARD;
				}
				break;
			case 'l':
				if(pos.x+1 < level->width){
					tileat(level, pos)->unit = 0;
					pos.x += 1;
					tileat(level, pos)->unit = TWIZARD;
				}
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

