#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <keyboard.h>
#include <mouse.h>

#include "dat.h"

enum
{
	LSIZE	= 20,
};

int debug;
static int uisz = 6;
static long turn = 0;
static char *user;
static char *home;
static Monster *player;
static int gameover = 0;

enum
{
	CGREEN,
	CYELLOW,
	CORANGE,
	CRED,
	CGREY,
	CMAGENTA,
	CEND,
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
	char buf[1024];
	char *tileset;

	if(initdraw(nil, nil, name) < 0)
		sysfatal("initdraw: %r");

	if((ui.mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");

	if((ui.kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");

	if((ui.msgc = chancreate(sizeof(Msg*), debug?10000:100)) == nil)
		sysfatal("chancreate: %r");

	tileset = getenv("tileset");
	if(tileset == nil)
		tileset = strdup("nethack.32x32");

	snprint(buf, sizeof(buf), "%s/lib/%s", home, tileset);
	if((ui.tiles = opentile("nethack.32x32", 32, 32)) == nil)
		if((ui.tiles = opentile(buf, 32, 32)) == nil)
		sysfatal("opentile: %r");

	if((ui.cols = mallocz(CEND * sizeof(Image*), 1)) == nil)
		sysfatal("malloc cols: %r");

	ui.cols[CGREEN] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DGreen);
	ui.cols[CYELLOW] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DYellow);
	ui.cols[CORANGE] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xFFA500FF);
	ui.cols[CRED] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DRed);
	ui.cols[CGREY] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xAAAAAAFF);
	ui.cols[CMAGENTA] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DMagenta);
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
bad(char *fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);
	uimsg(CRED, fmt, arg);
	va_end(arg);
}

void
dbg(char *fmt, ...)
{
	va_list arg;

	if(debug < 1)
		return;
	va_start(arg, fmt);
	uimsg(CMAGENTA, fmt, arg);
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
	int i, j, logline, sw;
	Point p;
	Image *c;
	char buf[256];
	Msg *m;

	sw = stringwidth(font, " ");

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

	snprint(buf, sizeof(buf), "%s the %s", user, player->md->name);
	p = string(screen, p, display->white, ZP, font, buf);
	p.x += sw*2;

	snprint(buf, sizeof(buf), "%ld kills", player->kills);
	p = string(screen, p, display->white, ZP, font, buf);

	p.x = r.min.x;
	p.y += font->height;
	snprint(buf, sizeof(buf), "T:%ld", turn);
	p = string(screen, p, display->white, ZP, font, buf);
	p.x += sw;

	snprint(buf, sizeof(buf), "%ld/%d HP", player->hp, player->md->maxhp);
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
	p.x += sw;

	snprint(buf, sizeof(buf), "%ld AC", player->ac);
	p = string(screen, p, display->white, ZP, font, buf);
	p.x += sw;

	if(player->flags & Mdead){
		p = stringbg(screen, p, display->black, ZP, font, "dead", ui.cols[CRED], ZP);
		p.x += sw;
	}
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
redraw(UI *ui, int new, int justui)
{
	int width, height;

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

	if(!justui){
		width = Dx(screen->r) / ui->tiles->width;
		height = (Dy(screen->r) - (font->height*uisz)) / ui->tiles->height;
		ui->viewr = view(player->pt, player->l, width, height+1);
		ui->camp = subpt(Pt(width/2, height/2), player->pt);
		draw(screen, screen->r, display->black, nil, ZP);
		drawlevel(player->l, ui->tiles, ui->viewr);
	}
	drawui(ui->uir);
	flushimage(display, 1);
}

void
movemons(void)
{
	int i, nmove;
	Point p, *tomove;
	Monster *m;

	nmove = 0;
	for(p.x = 0; p.x < player->l->width; p.x++){
		for(p.y = 0; p.y < player->l->height; p.y++){
			if(!eqpt(p, player->pt) && hasflagat(player->l, p, Fhasmonster))
				nmove++;
		}
	}

	tomove = mallocz(sizeof(Point)*nmove, 1);
	if(tomove == nil)
		sysfatal("movemons: %r");

	i = 0;
	for(p.x = 0; p.x < player->l->width; p.x++){
		for(p.y = 0; p.y < player->l->height; p.y++){
			if(!eqpt(p, player->pt) && hasflagat(player->l, p, Fhasmonster))
				tomove[i++] = p;
		}
	}

	for(i = 0; i < nmove; i++){
		p = tomove[i];
		m = tileat(player->l, p)->monst;

		/* it's possible the monster died in the meantime. */
		if(m == nil)
			continue;
		mupdate(m);
	}

	free(tomove);
}

void
usage(void)
{
	fprint(2, "usage: %s [-d] [-u uisz]", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	Rune c;
	Msg *l, *lp;
	
	/* of viewport, in size of tiles */
	int move, dir;
	Tile *t;
	Level *level;

	ARGBEGIN{
	case 'd':
		debug++;
		break;
	case 'u':
		uisz = atoi(EARGF(usage()));
		if(uisz < 3)
			sysfatal("ui too small");
		break;
	default:
		usage();
	}ARGEND

	srand(truerand());

	user = getenv("user");
	home = getenv("home");

	initui(argv0);

	/* initial level */
	if(debug > 0){
		if((level = genlevel(11, 11, 0)) == nil)
			sysfatal("genlevel: %r");
	} else {
		if((level = genlevel(nrand(LSIZE)+LSIZE, nrand(LSIZE)+LSIZE, nrand(3)+1)) == nil)
			sysfatal("genlevel: %r");
	}

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

	if(debug > 0){
		player->hp = 1000;
		player->md->maxhp = 1000;
	}

	redraw(&ui, 0, 0);

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
			redraw(&ui, 1, 0);
			break;
		case AKEYBOARD:
			if(c == Kdel)
				threadexitsall(nil);
			if(gameover > 0){
				bad("you are dead. game over, man.");
				break;
			}

			move = MNONE;
			dir = NODIR;

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
			case 's':
				/* special */
				move = MSPECIAL;
				c = recvul(ui.kc->c);
				switch(c){
				case 'h':
				case Kleft:
					dir = WEST;
					break;
				case 'j':
				case Kdown:
					dir = SOUTH;
					break;
				case 'k':
				case Kup:
					dir = NORTH;
					break;
				case 'l':
				case Kright:
					dir = EAST;
					break;
				default:
					break;
				}
				if(dir == NODIR){
					msg("bad direction for special");
					continue;
				}
			case '.':
				break;
			case '+':
				uisz+=2;
				goto redrw;
			case '-':
				if(uisz > 5)
					uisz-=2;
				goto redrw;
			case ' ':
				msg("");
			default:
				/* don't waste a move */
				goto redrw;
			}

			if(turn % 5 == 0 && player->hp < player->md->maxhp){
				good("you get 1 hp.");
				player->hp++;
			}

			if(move != MNONE){
				if(maction(player, move, addpt(player->pt, cardinals[dir])) < 0)
					msg("ouch!");
			}

			turn++;
			movemons();

			if(player->flags & Mdead){
				bad("you died!");
				gameover++;
			}

redrw:
			redraw(&ui, 0, 0);
			break;
		case ALOG:
			if(ui.msg == nil){
				ui.msg = l;
				ui.nmsg = 1;
			} else {
				l->next = ui.msg;
				ui.msg = l;
				ui.nmsg++;
				if(ui.nmsg > 100){
					for(lp = ui.msg; lp->next->next != nil; lp = lp->next)
						;
					free(lp->next);
					lp->next = nil;
				}
			}
			redraw(&ui, 0, 1);
			break;
		}
	}
}

