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

static Font *smallfont;

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

	if((smallfont = openfont(display, "/lib/font/bit/fixed/unicode.4x6.font")) == nil)
		sysfatal("openfont: %r");

	if((ui.mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");

	if((ui.kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");

	if((ui.msgc = chancreate(sizeof(Msg*), 100)) == nil)
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
tmsg(void *v)
{
	Msg *l;
	l = v;
	while(nbsendp(ui.msgc, l) == 0)
		yield();
}


static void
uimsg(uint color, char *fmt, va_list arg)
{
	Msg *l;

	l = mallocz(sizeof(Msg), 1);
	assert(l != nil);
	l->color = color;
	vsnprint(l->msg, sizeof(l->msg)-1, fmt, arg);
	if(nbsendp(ui.msgc, l) == 0)
		threadcreate(tmsg, l, 512);
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
	char tfmt[256];

	if(debug < 1)
		return;
	snprint(tfmt, sizeof(tfmt), "T:%ld %s", turn, fmt);
	va_start(arg, fmt);
	uimsg(CMAGENTA, tfmt, arg);
	va_end(arg);
}


/* draw the whole level on the screen using tileset ts.
 * r.min is the upper left visible tile, and r.max is
 * the bottom right visible tile.
 * */
static void
drawlevel(Level *l, Tileset *ts, Rectangle r)
{
	int what, i, j;
	char buf[16];
	Point p, p2, sp, bot;
	Image *c;
	Tile *t;
	Monster *m;
	
	bot = (Point){0, ts->height - smallfont->height};

	for(p.x = r.min.x; p.x < r.max.x; p.x++){
		for(p.y = r.min.y; p.y < r.max.y; p.y++){
			/* screen space */
			sp = p;
			sp = addpt(sp, ui.camp);
			sp.x *= ts->width;
			sp.y *= ts->height;
			sp = addpt(sp, screen->r.min);
			t = tileat(l, p);
			if(t){
				what = t->terrain;
				if(t->unit)
					what = t->unit;
				else if(t->feat)
					what = t->feat;
				drawtile(ts, screen, sp, what);
				if(t->monst != nil && t->monst != player){
					m = t->monst;
					i = m->hp, j = m->md->maxhp;
					snprint(buf, sizeof(buf), "%d/%d HP", i, j);
					if(i > (j/4)*3)
						c = ui.cols[CGREEN];
					else if(i > j/2)
						c = ui.cols[CYELLOW];
					else if(i > j/4)
						c = ui.cols[CORANGE];
					else
						c = ui.cols[CRED];
					p2 = addpt(sp, bot);
					string(screen, p2, c, ZP, smallfont, buf);
					/* debugging the ai state */
					if(debug){
						if(m->ai != nil){
							stringbg(screen, sp, display->white, ZP, smallfont, m->ai->name, display->black, ZP);
						} else {
							stringbg(screen, sp, display->white, ZP, smallfont, m->aglobal->name, display->black, ZP);
						}
					}
				}
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

	i = player->hp, j = player->md->maxhp;
	snprint(buf, sizeof(buf), "%d/%d HP", i, j);
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

/*
 * sort monsters for movement order.
 * player comes first, followed by monsters,
 * sorted by what their total move points will be
 * after adjusting for the ones they will gain during
 * mupdate.
 */
static int
monsort(const void *a, const void *b)
{
	int sa, sb;
	const Monster *am, *bm;
	am = a, bm = b;
	sa = am->mvp + am->mvr;
	sb = bm->mvp + bm->mvr;

	if(am == player)
		return -1;
	if(sa > sb)
		return -1;
	return sa < sb;
}

void
movemons(void)
{
	int i, nmove;
	Point p;
	Monster *m, **tomove;

	nmove = 0;
	for(p.x = 0; p.x < player->l->width; p.x++){
		for(p.y = 0; p.y < player->l->height; p.y++){
			if(hasflagat(player->l, p, Fhasmonster))
				nmove++;
		}
	}

	tomove = mallocz(sizeof(Monster*)*nmove, 1);
	if(tomove == nil)
		sysfatal("movemons: %r");

	i = 0;
	for(p.x = 0; p.x < player->l->width; p.x++){
		for(p.y = 0; p.y < player->l->height; p.y++){
			if(hasflagat(player->l, p, Fhasmonster)){
				m = tileat(player->l, p)->monst;
				incref(m);
				tomove[i++] = m;
			}
		}
	}

	qsort(tomove, nmove, sizeof(Monster*), monsort);

	for(i = 0; i < nmove; i++){
		m = tomove[i];
		/* it's possible the monster died in the meantime. */
		if((m->flags & Mdead) == 0)
			mupdate(m);

		mfree(m);
	}

	/* always give the player a move if dead, otherwise the ui breaks */
	if(player->flags & Mdead)
		mupdate(player);

	free(tomove);
}

void
uiexec(AIState *ai)
{
	Rune c;
	Msg *l, *lp;
	int move, dir;

	USED(ai);

	enum { ALOG, AMOUSE, ARESIZE, AKEYBOARD, AEND };
	Alt a[AEND+1] = {
		{ ui.msgc,			&l,		CHANRCV },
		{ ui.mc->c,			nil,	CHANRCV },
		{ ui.mc->resizec,	nil,	CHANRCV },
		{ ui.kc->c,			&c,		CHANRCV },
		{ nil,				nil,	CHANEND },
	};

	redraw(&ui, 0, 0);

	for(;;){
		switch(alt(a)){
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
		case AMOUSE:
			continue;
		case ARESIZE:
			redraw(&ui, 1, 0);
			continue;
		case AKEYBOARD:
			if(c == Kdel)
				threadexitsall(nil);

			if(gameover > 0){
				bad("you are dead. game over, man.");
				c = '.';
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
				break;
			case '.':
				/* do nothing */
				break;
			case '+':
				uisz+=2;
				redraw(&ui, 0, 0);
				continue;
			case '-':
				if(uisz > 5)
					uisz-=2;
				redraw(&ui, 0, 0);
				continue;
			case ' ':
				msg("");
			default:
				/* don't waste a move */
				redraw(&ui, 0, 0);
				continue;
			}

			if(move != MNONE){
				if(maction(player, move, addpt(player->pt, cardinals[dir])) < 0)
					msg("ouch!");
			}

			if(gameover == 0 && player->flags & Mdead){
				bad("you died!");
				gameover++;
			}

			redraw(&ui, 0, 0);
			return;
		}
	}
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
	/* of viewport, in size of tiles */
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

	//#include <pool.h>
	//mainmem->flags = POOL_PARANOIA|POOL_ANTAGONISM;

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

	/* don't delete when we die so ui works */
	incref(player);

	if(player == nil)
		sysfatal("monst: %r");
	player->l = level;
	player->pt = level->up;
	player->mvr = 16;
	t = tileat(level, player->pt);
	t->unit = TWIZARD;
	t->monst = player;
	setflagat(level, player->pt, Fhasmonster|Fblocked);

	player->ai = mkstate("input", player, nil, nil, uiexec, nil);

	if(debug > 0){
		player->hp = 1000;
		player->md->maxhp = 1000;
	}

	redraw(&ui, 0, 0);

	for(;;){
			turn++;
			movemons();
	}
}

