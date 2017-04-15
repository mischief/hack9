#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <keyboard.h>
#include <mouse.h>

#include "dat.h"
#include "alg.h"

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

typedef struct KeyMenu KeyMenu;
struct KeyMenu
{
	/* label printed with the menu */
	char *label;
	/* gen is called until it returns Runemax. */
	Rune (*gen)(int, char*, int);
};

static int menu(Keyboardctl *kc, Mousectl *mc, KeyMenu *km);
static int uienter(char *ask, char *buf, int len, Mousectl *mc, Keyboardctl *kc, Screen *scr);

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
	/* current ui size */
	int uisz;

	/* colors */
	Image **cols;

	/* mouse, keyboard */
	Mousectl *mc;
	Keyboardctl *kc;

	/* log chan */
	Channel *msgc;

	/* linked list of current msgs */
	Msg *msg;
	int nmsg;

	/* tileset */
	Tileset *tiles;

	/* level view rectangle, in tiles */
	Rectangle viewr;
	/* ui rectangle, in pixels */
	Rectangle uir;
	Point camp;

	/* timer tick, see /^tickproc */
	Channel *tickcmd;
	Channel *tick;

	/* should auto-move? */
	int autoidle;
	/* show far away messages? */
	int farmsg;
} ui;


static void
tickproc(void *v)
{
	USED(v);

	ulong time;
	time = 0;
	enum { ATIME, ATICK, AEND };
	Alt a[AEND+1] = {
		{ ui.tickcmd,	&time,	CHANRCV },
		{ ui.tick,		&time,	CHANEND },
		{ nil,			nil,	CHANEND },
	};
	for(;;){
		switch(alt(a)){
		case ATIME:
			if(time == 0)
				a[ATICK].op = CHANEND;
			else
				a[ATICK].op = CHANSND;
			break;
		case ATICK:
			sleep(time);
			break;
		}
	}
}

void
uiinit(char *name)
{
	int tx, ty;
	char *tileset, *tmp;

	tx = ty = 0;

	if(initdraw(nil, nil, name) < 0)
		sysfatal("initdraw: %r");

	if((smallfont = openfont(display, "/lib/font/bit/fixed/unicode.4x6.font")) == nil)
		sysfatal("openfont: %r");

	if((ui.mc = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");

	if((ui.kc = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");

	if((ui.tickcmd = chancreate(sizeof(ulong), 0)) == nil)
		sysfatal("chancreate: %r");

	if((ui.tick = chancreate(sizeof(ulong), 0)) == nil)
		sysfatal("chancreate: %r");

	proccreate(tickproc, nil, 4096);

	if((ui.msgc = chancreate(sizeof(Msg*), 1000)) == nil)
		sysfatal("chancreate: %r");

	tileset = getenv("tileset");
	if(tileset != nil){
		if((tmp = getenv("tilesetx")) == nil || (tx = atoi(tmp)) == 0)
			sysfatal("zero tilesetx");
		free(tmp);

		if((tmp = getenv("tilesety")) == nil || (ty = atoi(tmp)) == 0)
			sysfatal("zero tilesety");
		free(tmp);
	} else {
		if(access("nethack.32x32", AREAD) == 0)
			tileset = strdup("nethack.32x32");
		else
			tileset = strdup("/lib/hack9/nethack.32x32");
		tx = 32;
		ty = 32;
	}

	if((ui.tiles = opentile(tileset, tx, ty)) == nil)
		sysfatal("opentile %s @ %dx%d: %r", tileset, tx, ty);
	free(tileset);

	if((ui.cols = mallocz(CEND * sizeof(Image*), 1)) == nil)
		sysfatal("malloc cols: %r");

	ui.cols[CGREEN] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DGreen);
	ui.cols[CYELLOW] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DYellow);
	ui.cols[CORANGE] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xFFA500FF);
	ui.cols[CRED] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DRed);
	ui.cols[CGREY] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, 0xAAAAAAFF);
	ui.cols[CMAGENTA] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DMagenta);

	ui.uisz = 6;
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
		threadcreate(tmsg, l, 4096);
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

static Image*
hpcolor(int hp, int maxhp)
{
	Image *c;

	if(hp > (maxhp/4)*3)
		c = ui.cols[CGREEN];
	else if(hp > maxhp/2)
		c = ui.cols[CYELLOW];
	else if(hp > maxhp/4)
		c = ui.cols[CORANGE];
	else
		c = ui.cols[CRED];

	return c;
}

/* draw the whole level on the screen using tileset ts.
 * r.min is the upper left visible tile, and r.max is
 * the bottom right visible tile.
 * */
static void
drawlevel(Level *l, Tileset *ts, Rectangle r)
{
	int what;
	double hp, maxhp;
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
				if(t->monst != nil){
					m = t->monst;
					hp = m->hp, maxhp = m->maxhp;
					snprint(buf, sizeof(buf), "%d/%d HP", (int)hp, (int)maxhp);
					c = hpcolor(hp, maxhp);
					p2 = addpt(sp, bot);
					string(screen, p2, c, ZP, smallfont, buf);
					p2.y -= smallfont->height;
					snprint(buf, sizeof(buf), "L%d", m->xpl);
					stringbg(screen, p2, display->white, ZP, smallfont, buf, display->black, ZP);
					/* debugging the ai state */
					if(debug){
						if(m->ai != nil){
							stringbg(screen, sp, display->white, ZP, smallfont, m->ai->name, display->black, ZP);
						} else {
							stringbg(screen, sp, display->white, ZP, smallfont, m->aglobal->name, display->black, ZP);
						}
					}
				} else if(hasflagat(l, p, Fhasitem)){
					drawtile(ts, screen, sp, ilnth(&t->items, 0)->id->tile);
				}
			}
		}
	}
}

static void
drawui(Rectangle r)
{
	int i, logline, sw;
	double hp, maxhp;
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

	snprint(buf, sizeof(buf), "%d kills", player->kills);
	p = string(screen, p, display->white, ZP, font, buf);

	p.x = r.min.x;
	p.y += font->height;
	snprint(buf, sizeof(buf), "T:%ld", turn);
	p = string(screen, p, display->white, ZP, font, buf);
	p.x += sw;

	snprint(buf, sizeof(buf), "L:%d XP:%d/%d", player->xpl, player->xp, xpcalc(player->xpl));
	p = string(screen, p, display->white, ZP, font, buf);
	p.x += sw;

	hp = player->hp, maxhp = player->maxhp;
	snprint(buf, sizeof(buf), "HP:%d/%d", (int)hp, (int)maxhp);
	if(hp > (maxhp/4)*3)
		c = ui.cols[CGREEN];
	else if(hp > maxhp/2)
		c = ui.cols[CYELLOW];
	else if(hp > maxhp/4)
		c = ui.cols[CORANGE];
	else
		c = ui.cols[CRED];
	p = string(screen, p, c, ZP, font, buf);
	p.x += sw;

	snprint(buf, sizeof(buf), "AC:%d", player->ac);
	p = string(screen, p, display->white, ZP, font, buf);
	p.x += sw;

	if(player->flags & Mdead){
		p = stringbg(screen, p, display->black, ZP, font, "dead", ui.cols[CRED], ZP);
		p.x += sw;
	}
}

static Rectangle
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

static void
redraw(UI *ui, int new, int justui)
{
	int width, height;

	if(new){
		if(getwindow(display, Refnone) < 0)
			sysfatal("getwindow: %r");

		/* height should be big enough for 5 tiles and the ui size */
		if(Dx(screen->r) < 300 || Dy(screen->r) < (ui->tiles->height*5 + font->height*ui->uisz)){
			fprint(2, "window %R too small\n", screen->r);
			return;
		}

	}

	ui->uir = Rpt(Pt(screen->r.min.x, screen->r.max.y-(font->height*ui->uisz)), screen->r.max);

	if(!justui){
		width = Dx(screen->r) / ui->tiles->width;
		height = (Dy(screen->r) - (font->height*ui->uisz)) / ui->tiles->height;
		ui->viewr = view(player->pt, player->l, width, height+2);
		ui->camp = subpt(Pt(width/2, height/2), player->pt);
		draw(screen, screen->r, display->black, nil, ZP);
		drawlevel(player->l, ui->tiles, ui->viewr);
	}
	drawui(ui->uir);
	flushimage(display, 1);
}

void
uiredraw(int justui)
{
	redraw(&ui, 0, justui);
}

static Rune
dbgmenu(int idx, char *s, int sz)
{
	switch(idx){
	case 0:
		snprint(s, sz, "toggle debug flag: %d", debug);
		return 'd';
	case 1:
		snprint(s, sz, "toggle far messages: %d", farmsg);
		return 'f';
	case 2:
		snprint(s, sz, "revive and gain max hp");
		return 'r';
	case 3:
		snprint(s, sz, "wish for an item");
		return 'w';
	}
	return Runemax;
}

static Rune
inventorymenu(int idx, char *s, int sz)
{
	Item *i;

	if(player->inv.count <= 0 || idx > player->inv.count-1)
		return Runemax;

	i = ilnth(&player->inv, idx);
	snprint(s, sz, "%i", i);

	return idx+'a';
}

static Rune
equipmenu(int idx, char *s, int sz)
{
	Item *ptr;
	Rune key[NEQUIP] = { 'w', 'c', 'h', 's', 'a', 'g', 'b' };

	if(idx >= NEQUIP)
		return Runemax;

	ptr = player->armor[idx];

	if(ptr != nil){
		snprint(s, sz, "%s: %i", itypetoname(ptr->id->type), ptr);
		return key[idx];
	}

	return 0;
}

static Rune
dirmenu(int idx, char *s, int sz)
{
	USED(sz);

	switch(idx){
	case WEST:
		strcpy(s, "west");
		return 'h';
	case SOUTH:
		strcpy(s, "south");
		return 'j';
	case NORTH:
		strcpy(s, "north");
		return 'k';
	case EAST:
		strcpy(s, "east");
		return 'l';
	}
	return Runemax;
}

static int
cmove(Rune c)
{
	int move, dir;

	move = MNONE;
	dir = NODIR;

	switch(c){
	case 'h':
	case L'←':
		move = MMOVE;
		dir = WEST;
		break;
	case 'j':
	case L'↓':
		move = MMOVE;
		dir = SOUTH;
		break;
	case 'k':
	case L'↑':
		move = MMOVE;
		dir = NORTH;
		break;
	case 'l':
	case L'→':
		move = MMOVE;
		dir = EAST;
		break;
	}

	if((player->flags & Mdead) == 0 && move != MNONE){
		if(maction(player, move, addpt(player->pt, cardinals[dir])) < 0){
			msg("ouch!");
			return 0;
		}
	}

	return 1;
}

static int
cport(Rune c)
{
	USED(c);

	if((player->flags & Mdead) == 0){
		if(maction(player, MUSE, addpt(player->pt, cardinals[NODIR])) < 0){
			msg("there's no portal here.");
			return 0;
		}
	}

	return 1;
}

static int
cpickup(Rune c)
{
	USED(c);

	if((player->flags & Mdead) == 0){
		if(maction(player, MPICKUP, addpt(player->pt, cardinals[NODIR])) < 0){
			msg("there's nothing here.");
			return 0;
		}
	}

	return 1;
}

/* use item */
static int
cuse(Rune c)
{
	int i;
	KeyMenu km;
	Item *item;

	USED(c);

	km = (KeyMenu){"use what?", inventorymenu};
	i = menu(ui.kc, ui.mc, &km);
	if(i >= 0 && i <= player->inv.count){
		item = ilnth(&player->inv, i);
		switch(item->id->type){
		case IWEAPON:
		case IHELMET:
		case ICLOAK:
		case ISHIELD:
		case IARMOR:
		case IGLOVES:
		case IBOOTS:
			if(!mwield(player, i)){
				bad("can't equip that");
				return 0;
			} else
				good("you are now %sing the %#i!", item->id->type==IWEAPON?"wield":"wear", item);
			return 1;
		case ICONSUME:
			if(!muse(player, item)){
				bad("you can't eat the %#i", item);
				return 0;
			}

			good("you eat the %#i. tasty!", item);
			/* consumables get used up */
			ifree(iltakenth(&player->inv, i));
			return 1;
		default:
			warn("you aren't sure what to do with the %#i...", item);
			return 0;
			break;
		}
	}
	return 0;
}

static int
cdrop(Rune c)
{
	int i;
	KeyMenu km;
	Item *item;
	Tile *t;

	USED(c);

	/* drop item */
	km = (KeyMenu){"drop what?", inventorymenu};
	i = menu(ui.kc, ui.mc, &km);
	if(i >= 0 && i <= player->inv.count){
		item = iltakenth(&player->inv, i);
		bad("you drop the %i.", item);
		t = tileat(player->l, player->pt);
		setflagat(player->l, player->pt, Fhasitem);
		iladd(&t->items, item);
		return 1;
	}
	return 0;
}

static int
cstrip(Rune c)
{
	int i;
	KeyMenu km;
	Item *item;

	USED(c);

	/* take off armor */
	km = (KeyMenu){"take off what?", equipmenu};
	i = menu(ui.kc, ui.mc, &km);
	if(i >= 0 && i < NEQUIP){
		munwield(player, i);
		item = ilnth(&player->inv, 0);
		warn("you take off the %i.", item);
		return 1;
	}
	return 0;
}

static int
cwait(Rune c)
{
	USED(c);
	return 1;
}

static int
cdebug(Rune c)
{
	int i;
	char buf[64];
	KeyMenu km;
	Item *item;

	USED(c);

	/* debugging */
	km = (KeyMenu){"debug menu", dbgmenu};
	i = menu(ui.kc, ui.mc, &km);
	switch(i){
	case 0:
		debug = !debug;
		break;
	case 1:
		farmsg = !farmsg;
		break;
	case 2:
		/* revive */
		if(hasflagat(player->l, player->pt, Fhasmonster)){
			warn("tile player occupied is blocked");
			break;
		}

		incref(&player->ref);
		player->flags &= ~Mdead;
		player->hp = player->maxhp;
		setflagat(player->l, player->pt, Fhasmonster);
		tileat(player->l, player->pt)->monst = player;
		tileat(player->l, player->pt)->unit = player->md->tile;
		gameover = 0;
		break;
	case 3:
		buf[0] = 0;
		if(uienter("item?", buf, sizeof(buf), ui.mc, ui.kc, nil) > 0){
			item = ibyname(buf);
			if(item == nil){
				bad("no such item %q", buf);
				break;
			}

			maddinv(player, item);
		}
		break;
	}
	return 0;
}

static Rune
settingmenu(int idx, char *s, int sz)
{
	switch(idx){
	case 0:
		snprint(s, sz, "set auto-move: %dms", ui.autoidle);
		return 'A';
	case 1:
		snprint(s, sz, "log lines: %d", ui.uisz-2);
		return 'L';
	}
	return Runemax;
}

static int
csettings(Rune c)
{
	int i, uisz;
	char buf[32];
	KeyMenu km;

	USED(c);

	buf[0] = 0;
	km = (KeyMenu){"settings", settingmenu};
	i = menu(ui.kc, ui.mc, &km);
	switch(i){
	case 0:
		snprint(buf, sizeof(buf), "%d", ui.autoidle);
		if(uienter("autoidle ms:", buf, sizeof(buf), ui.mc, ui.kc, nil) > 0){
			ui.autoidle = atoi(buf);
			if(ui.autoidle < 0)
				ui.autoidle = 0;
			sendul(ui.tickcmd, ui.autoidle);
		}
		break;
	case 1:
		/* ±2 for stats bar */
		snprint(buf, 64, "%d", ui.uisz-2);
		if(uienter("log lines:", buf, sizeof(buf), ui.mc, ui.kc, nil) > 0){
			uisz = atoi(buf);
			if(uisz < 2 || uisz > 30){
				bad("ui size must be between 2 and 30");
				break;
			}

			ui.uisz = uisz+2;
		}
		break;
	}
	return 0;
}

typedef struct keycmd keycmd;
struct keycmd {
	Rune key;
	int (*cmd)(Rune);
	char *help;
};

static int chelp(Rune);

static keycmd keycmds[] = {
/* basic movement */
{ 'h', cmove,		"move west" },
{ 'j', cmove,		"move south", },
{ 'k', cmove,		"move north", },
{ 'l', cmove,		"move east" },
{ L'←', cmove,		"move west" },
{ L'↓', cmove,		"move south", },
{ L'↑', cmove,		"move north", },
{ L'→', cmove,		"move east" },

{ '<', cport,		"use portal" },
{ ',', cpickup,		"pickup item" },
{ 'u', cuse,		"use inventory item" },
{ 'd', cdrop,		"drop inventory item" },
{ 'A', cstrip,		"take off weapon/armor" },
{ '.', cwait,		"wait for a move" },
{ 'S', csettings,	"game settings" },
{ 'D', cdebug,		"debugging options" },
{ '?', chelp,		"this help" },
};

static Rune
helpmenu(int idx, char *s, int sz)
{
	if(idx > nelem(keycmds)-1)
		return Runemax;

	snprint(s, sz, "%s", keycmds[idx].help);
	return keycmds[idx].key;
}

static int
chelp(Rune c)
{
	int i;
	KeyMenu km;

	USED(c);
	km = (KeyMenu){"key commands", helpmenu};
	i = menu(ui.kc, ui.mc, &km);

	if(i >= 0 && i < nelem(keycmds)-1){
		/* could maybe print extended help instead */
		return keycmds[i].cmd(keycmds[i].key);
	}
	return 0;
}

void
uiexec(AIState *ai)
{
	int al;
	Rune c;
	Msg *l, *lp;

	USED(ai);

	enum { ALOG, ATICK, AMOUSE, ARESIZE, AKEYBOARD, AEND };
	Alt a[AEND+1] = {
		{ ui.msgc,			&l,		CHANRCV },
		{ ui.tick,			nil,	CHANRCV },
		{ ui.mc->c,			nil,	CHANRCV },
		{ ui.mc->resizec,	nil,	CHANRCV },
		{ ui.kc->c,			&c,		CHANRCV },
		{ nil,				nil,	CHANEND },
	};

	redraw(&ui, 0, 0);

	for(;;){
		al = alt(a);
		switch(al){
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
		case ATICK:
			return;
		case AMOUSE:
			continue;
		case ARESIZE:
			redraw(&ui, 1, 0);
			continue;
		case AKEYBOARD:
			/* Kdel */
			if(c == 0x7f)
				threadexitsall(nil);

			if(gameover > 0 && gameover++ < 2){
				bad("you are dead. game over, man.");
			}

			/* translation */
			switch(c){
			case Kleft:
				c = L'←';
				break;
			case Kup:
				c = L'↑';
				break;
			case Kdown:
				c = L'↓';
				break;
			case Kright:
				c = L'→';
				break;
			}

			int h;
			keycmd *cmd;
			cmd = nil;
			for(h = 0; h < nelem(keycmds); h++){
				if(c == keycmds[h].key){
					cmd = &keycmds[h];
					break;
				}
			}

			if(cmd == nil || (cmd != nil && !cmd->cmd(c))){
				redraw(&ui, 0, 0);
				continue;
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

static int
ch2ind(KeyMenu *km, Rune want)
{
	int i;
	char buf[64];
	Rune c;

	for(i = 0; (c=km->gen(i, buf, 64)) != Runemax; i++){
		if(c == want)
			return i;
	}
	return -1;
}

enum {
	Spacing	= 6,
};

static int
menu(Keyboardctl *kc, Mousectl *mc, KeyMenu *km)
{
	int i, nitem, maxwidth;
	char menu[64], buf[256];
	Rune ch, in;
	Point p;
	Rectangle r;

	maxwidth = 0;
	for(nitem = 0; (ch=km->gen(nitem, menu, 64)) != Runemax; nitem++){
		snprint(buf, sizeof(buf), "%C - %s", ch, menu);
		i = stringwidth(font, buf);
		if(i > maxwidth)
			maxwidth = i;
	}

	i = stringwidth(font, km->label);
	if(i > maxwidth)
		maxwidth = i;

	enum { AMOUSE, ARESIZE, AKEYBOARD, AEND };
	Alt a[AEND+1] = {
		{ mc->c,		nil,	CHANRCV },
		{ mc->resizec,	nil,	CHANRCV },
		{ kc->c,		&in,	CHANRCV },
		{ nil,			nil,	CHANEND },
	};

	goto drawui;

	for(;;){
		switch(alt(a)){
		case AMOUSE:
			break;
		case ARESIZE:
			redraw(&ui, 1, 0);
drawui:
			r = insetrect(Rpt(screen->r.min, addpt(ui.uir.min, Pt(Dx(ui.uir), 0))), 10);
			/* +1 for the label */
			r.min.y += Dy(r) - (2*Spacing) - font->height*(nitem + 1);
			r.max.x = r.min.x + (2*Spacing) + min(Dx(r), maxwidth);
			r.max.y = r.min.y + (2*Spacing) + font->height*(nitem + 1);

			draw(screen, r, display->black, nil, ZP);
			border(screen, r, 2, display->white, ZP);
			r = insetrect(r, Spacing);

			p = r.min;
			string(screen, p, ui.cols[CGREY], ZP, font, km->label);
			p.y += font->height;
			for(i = 0; i < nitem; i++){
				ch = km->gen(i, menu, 64);
				snprint(buf, sizeof(buf), "%C - %s", ch, menu);
				string(screen, p, display->white, ZP, font, buf);
				p.y += font->height;
			}

			flushimage(display, 1);
			break;
		case AKEYBOARD:
			redraw(&ui, 0, 0);
			return ch2ind(km, in);
		}
	}
}

/* my own copy of libdraw's, since plan9port doesn't have it. */
static int
uienter(char *ask, char *buf, int len, Mousectl *mc, Keyboardctl *kc, Screen *scr)
{
	int done, down, tick, n, h, w, l, i;
	Image *b, *save;
	Point p, o, t;
	Rectangle r, sc;
	Alt a[3];
	Mouse m;
	Rune k;

	o = ZP;
	sc = screen->clipr;
	replclipr(screen, 0, screen->r);

	n = 0;
	if(kc){
		while(nbrecv(kc->c, nil) == 1)
			;
		a[n].op = CHANRCV;
		a[n].c = kc->c;
		a[n].v = &k;
		n++;
	}
	if(mc){
	/* ugly. */
#ifndef PLAN9PORT
		o = mc->xy;
#else
		o = mc->m.xy;
#endif
		a[n].op = CHANRCV;
		a[n].c = mc->c;
		a[n].v = &m;
		n++;
	}
	a[n].op = CHANEND;
	a[n].c = nil;
	a[n].v = nil;

	if(eqpt(o, ZP))
		o = addpt(screen->r.min, Pt(Dx(screen->r)/3, Dy(screen->r)/3));

	if(buf && len > 0)
		n = strlen(buf);
	else {
		buf = nil;
		len = 0;
		n = 0;
	}

	k = -1;
	b = nil;
	tick = n;
	save = nil;
	done = down = 0;

	p = stringsize(font, " ");
	t = ZP;
	h = p.y;
	w = p.x;

	while(!done){
		p = stringsize(font, buf ? buf : "");
		if(ask && ask[0]){
			if(buf) p.x += w;
			p.x += stringwidth(font, ask);
		}
		r = rectaddpt(insetrect(Rpt(ZP, p), -4), o);
		p.x = 0;
		r = rectsubpt(r, p);

		p = ZP;
		if(r.min.x < screen->r.min.x)
			p.x = screen->r.min.x - r.min.x;
		if(r.min.y < screen->r.min.y)
			p.y = screen->r.min.y - r.min.y;
		r = rectaddpt(r, p);
		p = ZP;
		if(r.max.x > screen->r.max.x)
			p.x = r.max.x - screen->r.max.x;
		if(r.max.y > screen->r.max.y)
			p.y = r.max.y - screen->r.max.y;
		r = rectsubpt(r, p);

		r = insetrect(r, -2);
		if(scr){
			if(b == nil)
				b = allocwindow(scr, r, Refbackup, DWhite);
			if(b == nil)
				scr = nil;
		}
		if(scr == nil && save == nil){
			if(b == nil)
				b = screen;
			save = allocimage(display, r, b->chan, 0, DNofill);
			if(save == nil){
				n = -1;
				break;
			}
			draw(save, r, b, nil, r.min);
		}
		draw(b, r, display->black, nil, ZP);
		border(b, r, 2, display->white, ZP);
		p = addpt(r.min, Pt(6, 6));
		if(ask && ask[0]){
			p = string(b, p, display->white, ZP, font, ask);
			if(buf) p.x += w;
		}
		if(buf){
			t = p;
			p = stringn(b, p, display->white, ZP, font, buf, utfnlen(buf, tick));
			draw(b, Rect(p.x-1, p.y, p.x+2, p.y+3), display->white, nil, ZP);
			draw(b, Rect(p.x, p.y, p.x+1, p.y+h), display->white, nil, ZP);
			draw(b, Rect(p.x-1, p.y+h-3, p.x+2, p.y+h), display->white, nil, ZP);
			p = string(b, p, display->white, ZP, font, buf+tick);
		}
		flushimage(display, 1);

nodraw:
		switch(alt(a)){
		case -1:
			done = 1;
			n = -1;
			break;
		case 0:
			if(buf == nil || k == 0x4 || k == '\n'){
				done = 1;
				break;
			}
			if(k == 0x15 || k == 0x1b){
				done = !n;
				buf[n = tick = 0] = 0;
				break;
			}
			if(k == 0x1 || k == Khome){
				tick = 0;
				continue;
			}
			if(k == 0x5 || k == Kend){
				tick = n;
				continue;
			}
			if(k == Kright){
				if(tick < n)
					tick += chartorune(&k, buf+tick);
				continue;
			}
			if(k == Kleft){
				for(i = 0; i < n; i += l){
					l = chartorune(&k, buf+tick);
					if(i+l >= tick){
						tick = i;
						break;
					}
				}
				continue;
			}
			if(k == 0x17){
				while(tick > 0){
					tick--;
					if(tick == 0 ||
					   strchr(" !\"#$%&'()*+,-./:;<=>?@`[\\]^{|}~", buf[tick-1]))
						break;
				}
				buf[n = tick] = 0;
				break;
			}
			if(k == 0x8){
				if(tick <= 0)
					continue;
				for(i = 0; i < n; i += l){
					l = chartorune(&k, buf+i);
					if(i+l >= tick){
						memmove(buf+i, buf+i+l, n - (i+l));
						buf[n -= l] = 0;
						tick -= l;
						break;
					}
				}
				break;
			}
			if(k < 0x20 || k == 0x7f || (k & 0xFF00) == KF || (k & 0xFF00) == 0xF800)
				continue;
			if((len-n) <= (l = runelen(k)))
				continue;
			memmove(buf+tick+l, buf+tick, n - tick);
			runetochar(buf+tick, &k);
			buf[n += l] = 0;
			tick += l;
			break;
		case 1:
			if(!ptinrect(m.xy, r)){
				down = 0;
				goto nodraw;
			}
			if(m.buttons & 7){
				down = 1;
				if(buf && m.xy.x >= (t.x - w)){
					down = 0;
					for(i = 0; i < n; i += l){
						l = chartorune(&k, buf+i);
						t.x += stringnwidth(font, buf+i, 1);
						if(t.x > m.xy.x)
							break;
					}
					tick = i;
				}
				continue;
			}
			done = down;
			break;
		}

		if(b != screen) {
			freeimage(b);
			b = nil;
		} else {
			draw(b, save->r, save, nil, save->r.min);
			freeimage(save);
			save = nil;
		}
	}

	replclipr(screen, 0, sc);
	flushimage(display, 1);
	return n;
}

