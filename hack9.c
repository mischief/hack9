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

Point pos;
Camera cam;

static int
min(int a, int b)
{
	if(a < b)
		return a;
	return b;
}

static void
drawlevel(Level *l, Tileset *ts, int width, int height)
{
	int x, y, what;
	Point xy, tpos, cen;
	Tile *t;

	for(x = 0; x < width; x++){
		for(y = 0; y < height; y++){
			xy = (Point){x, y};

			/* very wrong. */
			tpos = xy;
			tpos.x *= ts->width;
			tpos.y *= ts->height;
			tpos = ctrans(&cam, tpos);
			t = tileat(l, xy);
			what = t->terrain;
			if(t->unit)
				what = t->unit;
			else if(t->feat)
				what = t->feat;
			cen = addpt(screen->r.min, tpos);
			//if(what == TWIZARD)
			//	fprint(2, "draw %d at %P %P ", what, tpos, cen);
			drawtile(ts, screen, cen, what);
		}
	}
}

void
threadmain(int argc, char *argv[])
{
	Rune r;
	
	/* of viewport, in size of tiles */
	int width, height;

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
	cam = (Camera){ZP, Rect(0, 0, Dy(screen->r), Dx(screen->r)), Rect(0, 0, Dy(screen->r), Dx(screen->r))};
	ccenter(&cam, Pt(pos.x * tiles->width, pos.y * tiles->height));
	drawlevel(level, tiles, min(width+1, level->width), min(height+1, level->height));
	flushimage(display, 1);

	enum { AMOUSE, ARESIZE, AKEYBOARD, AEND };
	Alt a[AEND+1] = {
		{ mc->c,		nil,	CHANRCV },
		{ mc->resizec,	nil,	CHANRCV },
		{ kc->c,		&r,		CHANRCV },
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

			cam = (Camera){ZP, Rect(0, 0, Dy(screen->r), Dx(screen->r)), Rect(0, 0, Dy(screen->r), Dx(screen->r))};
			ccenter(&cam, Pt(pos.x * tiles->width, pos.y * tiles->height));
			drawlevel(level, tiles, min(width+1, level->width), min(height+1, level->height));
			flushimage(display, 1);
			break;
		case AKEYBOARD:
			switch(r){
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
			ccenter(&cam, pos);
			drawlevel(level, tiles, min(width+1, level->width), min(height+1, level->height));
			flushimage(display, 1);
			break;
		}
	}
}

