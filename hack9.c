#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <keyboard.h>
#include <mouse.h>
#include <pool.h>

#include "dat.h"

enum
{
	LSIZE	= 20,
};

static long turn = 0;
static Level *level;
static char *user;
static Monster *player;
static int gameover = 0;

enum
{
	CGREEN,
	CYELLOW,
	CORANGE,
	CRED,
	CGREY,
};

typedef struct Msg Msg;
struct Msg
{
	char	msg[256];
	ulong	color;
	Msg		*next;
};

typedef struct UI UI;
struct UI
{
	Image **cols;
	Mousectl *mc;
	Keyboardctl *kc;
	Channel *msgc;
	Msg *msg;
	int nmsg;
	Tileset *tiles;
	Rectangle viewr;
	Rectangle uir;
	Point camp;
} ui;

static void
initui(char *name)
{
	if(initdraw(nil, nil, name) < 0)
		sysfatal("initdraw: %r");

	if((ui.mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");

	if((ui.kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");

	if((ui.msgc = chancreate(sizeof(Msg*), 100)) == nil)
		sysfatal("chancreate: %r");

	if((ui.tiles = opentile("nethack.32x32", 32, 32)) == nil)
		sysfatal("opentile: %r");

	if((ui.cols = mallocz(5 * sizeof(Image*), 1)) == nil)
		sysfatal("malloc cols: %r");

	ui.cols[CGREEN] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DGreen);
	ui.cols[CYELLOW] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DYellow);
	ui.cols[CORANGE] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xFFA500FF);
	ui.cols[CRED] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DRed);
	ui.cols[CGREY] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xAAAAAAFF);
}

static void
uimsg(uint color, char *fmt, va_list arg)
{
	Msg *l;

	l = mallocz(sizeof(Msg), 1);
	assert(l != nil);
	l->color = color;
	vsnprint(l->msg, sizeof(l->msg)-1, fmt, arg);

	while(nbsendp(ui.msgc, l) == 0)
		yield();
}

void
msg(char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	uimsg(CGREY, fmt, arg);
	va_end(arg);
}

void
good(char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	uimsg(CGREEN, fmt, arg);
	va_end(arg);
}

void
warn(char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	uimsg(CYELLOW, fmt, arg);
	va_end(arg);
}

void
dbg(char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	uimsg(CRED, fmt, arg);
	va_end(arg);
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

	for(x = r.min.x; x < r.max.x; x++){
		for(y = r.min.y; y < r.max.y; y++){
			/* world cell */
			xy = (Point){x, y};
			/* screen space */
			tpos = xy;
			tpos = addpt(tpos, ui.camp);
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

void
drawui(Rectangle r)
{
	int i, j, logline;
	Point p;
	Image *c;
	char buf[256];
	Msg *m;

	draw(screen, r, display->black, nil, ZP);

	logline = Dy(r) - (font->height*2);

	p = r.min;
	p.y += logline-font->height;

	m = ui.msg;
	for(i = logline/font->height; m != nil && i > 0; i--){
		snprint(buf, sizeof(buf), "%s", m->msg);
		string(screen, p, ui.cols[m->color], ZP, font, buf);
		p.y -= font->height;
		m = m->next;
	}

	p = r.min;
	p.y += logline;
	line(screen, p, Pt(screen->r.max.x, p.y), Enddisc, Enddisc, 0, display->white, ZP);

	if(gameover>0)
		return;

	snprint(buf, sizeof(buf), "%s the %s", user, player->md->name);
	p = string(screen, p, display->white, ZP, font, buf);
	p.x += 10;

	p.x = r.min.x;
	p.y += font->height;
	snprint(buf, sizeof(buf), "T:%ld ", turn);
	p = string(screen, p, display->white, ZP, font, buf);

	snprint(buf, sizeof(buf), "%ld/%d HP ", player->hp, player->md->maxhp);
	i = player->hp, j = player->md->maxhp;
	if(i > (j/4)*3)
		c = ui.cols[CGREEN];
	else if(i > j/2)
		c = ui.cols[CYELLOW];
	else if(i > j/4)
		c = ui.cols[CORANGE];
	else
		c = ui.cols[CRED];
	p = string(screen, p, c, ZP, font, buf);
}

Rectangle
view(Point pos, Level *l, int scrwidth, int scrheight)
{
	Point p;
	Rectangle r;

	/* compute vieweable area of the map */
	p = Pt((scrwidth/2)+2, (scrheight/2));
	r = Rpt(subpt(pos, p), addpt(pos, p));
	rectclip(&r, Rect(0, 0, l->width, l->height));
	return r;
}

void
redraw(UI *ui, int new)
{
	int width, height, uisz;

	uisz = 7;

	if(new){
		if(getwindow(display, Refnone) < 0)
			sysfatal("getwindow: %r");

		/* height should be big enough for 5 tiles, and 5 text lines. */
		if(Dx(screen->r) < 300 || Dy(screen->r) < (ui->tiles->height*5 + font->height*uisz)){
			fprint(2, "window %R too small\n", screen->r);
			return;
		}

	}

	ui->uir = Rpt(Pt(screen->r.min.x, screen->r.max.y-(font->height*uisz)), screen->r.max);

	if(gameover<1){
		width = Dx(screen->r) / ui->tiles->width;
		height = (Dy(screen->r) - (font->height*2)) / ui->tiles->height;
		ui->viewr = view(player->pt, level, width, height);
		ui->camp = subpt(Pt(width/2, height/2), player->pt);
		draw(screen, screen->r, display->black, nil, ZP);
		drawlevel(level, ui->tiles, ui->viewr);
	}
	drawui(ui->uir);
	flushimage(display, 1);
}

void
movemons(void)
{
	int i, nmove, npath;
	Point p, p2, *tomove, *path;
	Monster *m;

	nmove = 0;
	for(p.x = 0; p.x < level->width; p.x++){
		for(p.y = 0; p.y < level->height; p.y++){
			if(!eqpt(p, player->pt) && hasflagat(level, p, Fhasmonster))
				nmove++;
		}
	}

	tomove = mallocz(sizeof(Point)*nmove, 1);
	if(tomove == nil)
		sysfatal("movemons: %r");

	i = 0;
	for(p.x = 0; p.x < level->width; p.x++){
		for(p.y = 0; p.y < level->height; p.y++){
			if(!eqpt(p, player->pt) && hasflagat(level, p, Fhasmonster))
				tomove[i++] = p;
		}
	}

	for(i = 0; i < nmove; i++){
		p = tomove[i];
		m = tileat(level, p)->monst;

		/* it's possible the monster died in the meantime. */
		if(m == nil)
			continue;
		mupdate(m);
		continue;

		if(manhattan(p, player->pt) < 6 * ORTHOCOST){
			/* move the monster toward player */
			npath = pathfind(level, m->pt, player->pt, &path);
			if(npath >= 0){
				/* step once along path, path[0] is cur pos */
				maction(m, MMOVE, path[1]);
				free(path);
			}
		} else {
			/* random move */
			p2 = addpt(m->pt, cardinals[nrand(NCARDINAL)]);
			if(!hasflagat(level, p2, Fblocked))
				maction(m, MMOVE, p2);
		}
	}

	free(tomove);
}

void
threadmain(int argc, char *argv[])
{
	Rune c;
	Msg *l, *lp;
	
	/* of viewport, in size of tiles */
	int move, dir;
	Tile *t;

	ARGBEGIN{
	}ARGEND

	mainmem->flags = POOL_PARANOIA;

	srand(truerand());

	user = getenv("user");

	initui(argv0);

	if((level = genlevel(nrand(LSIZE)+LSIZE, nrand(LSIZE)+LSIZE)) == nil)
		sysfatal("genlevel: %r");

	/* the player */
	player = monst(TWIZARD);
	if(player == nil)
		sysfatal("monst: %r");
	player->l = level;
	player->pt = level->up;
	t = tileat(level, player->pt);
	t->unit = TWIZARD;
	t->monst = player;
	setflagat(level, player->pt, Fhasmonster|Fblocked);

	redraw(&ui, 0);

	enum { AMOUSE, ARESIZE, AKEYBOARD, ALOG, AEND };
	Alt a[AEND+1] = {
		{ ui.mc->c,			nil,	CHANRCV },
		{ ui.mc->resizec,	nil,	CHANRCV },
		{ ui.kc->c,			&c,		CHANRCV },
		{ ui.msgc,			&l,		CHANRCV },
		{ nil,				nil,	CHANEND },
	};

	for(;;){
		switch(alt(a)){
		case AMOUSE:
			break;
		case ARESIZE:
			redraw(&ui, 1);
			break;
		case AKEYBOARD:
			if(c == Kdel)
				threadexitsall(nil);
			if(gameover > 0){
				dbg("you are dead. game over, man.");
				break;
			}

			move = MNONE;
			dir = NODIR;

			if(turn % 5 == 0 && player->hp < player->md->maxhp){
				good("you get 1 hp.");
				player->hp++;
			}

			switch(c){
			case Kdel:
				threadexitsall(nil);
				break;
			case 'h':
			case Kleft:
				move = MMOVE;
				dir = WEST;
				break;
			case 'j':
			case Kdown:
				move = MMOVE;
				dir = SOUTH;
				break;
			case 'k':
			case Kup:
				move = MMOVE;
				dir = NORTH;
				break;
			case 'l':
			case Kright:
				move = MMOVE;
				dir = EAST;
				break;
			case '<':
			case '>':
				move = MUSE;
				break;
			case '.':
				break;
			case ' ':
				msg("");
			default:
				/* don't waste a move */
				continue;
			}

			if(move != MNONE && dir != NODIR){
				if(maction(player, move, addpt(player->pt, cardinals[dir])) < 0)
					msg("ouch!");
			}

			turn++;
			if(turn % 2 ==0){
				movemons();
			}

			if(player->hp < 2){
				gameover++;
			}

			redraw(&ui, 0);
			break;
		case ALOG:
			if(ui.msg == nil){
				ui.msg = l;
				ui.nmsg = 1;
			} else {
				l->next = ui.msg;
				ui.msg = l;
				ui.nmsg++;
				if(ui.nmsg > 10){
					for(lp = ui.msg; lp->next != nil; lp = lp->next)
						;
					free(lp->next);
					lp->next = nil;
				}
			}
			redraw(&ui, 0);
			break;
		}
	}
}

