#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>

#include "map.h"
#include "bt.h"

#include "dat.h"
#include "alg.h"

enum
{
	LSIZE	= 20,
};

int debug;
int farmsg = 0;
Monster *player;
char *user;
char *home;
long turn = 0;

int
isyou(Monster *m)
{
	return m == player;
}

int
nearyou(Point p)
{
	if(farmsg)
		return 1;
	return manhattan(player->pt, p) < ORTHOCOST*5;
}

/*
 * sort monsters for movement order.
 * player comes first, followed by monsters,
 * sorted by what their total move points will be
 * after adjusting for the ones they will gain during
 * mupdate.
 */
static int
monsort(void *a, void *b)
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

static void
movemons(void)
{
	Point p;
	Monster *m;
	Level *l;
	Priq *tomove;

	tomove = priqnew(32);
	l = player->l;

	/* run ai on level, for spawns etc */
	bttick(l->bt, l);

	for(p.x = 0; p.x < l->width; p.x++){
		for(p.y = 0; p.y < l->height; p.y++){
			if(hasflagat(l, p, Fhasmonster)){
				m = tileat(l, p)->monst;
				/* i don't think this is supposed to happen. */
				if(m == nil)
					continue;
				incref(&m->ref);
				priqpush(tomove, m, monsort);
			}
		}
	}

	while((m = priqpop(tomove, monsort)) != nil){
		/* it's possible the monster died in the meantime. */
		if((m->flags & Mdead) == 0)
			mupdate(m);
		mfree(m);
	}

	/* always give the player a move if dead, otherwise the ui breaks */
	if(player->flags & Mdead)
		mupdate(player);

	priqfree(tomove);
}

static void
usage(void)
{
	fprint(2, "usage: %s [-d]\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	long seed;
	Tile *t;
	Level *level;

	seed = -1;

	ARGBEGIN{
	case 'd':
		debug++;
		break;
	case 'S':
		seed = atoi(EARGF(usage()));
		break;
	default:
		usage();
	}ARGEND

	//#include <pool.h>
	//mainmem->flags = POOL_PARANOIA|POOL_ANTAGONISM;

	if(seed == -1)
		seed = truerand();
	srand(seed);

	if(debug)
		fprint(2, "seed %ld\n", seed);

	quotefmtinstall();

	user = getenv("user");
	home = getenv("home");

	uiinit(argv0);

	if(!itemdbopen("/lib/hack9/item.ndb"))
		if(!itemdbopen("item.ndb"))
			sysfatal("itemdbopen: %r");

	if(!monstdbopen("/lib/hack9/monster.ndb"))
		if(!monstdbopen("monster.ndb"))
			sysfatal("monstdbopen: %r");

	/* initial level */
	if((level = lgenerate(nrand(LSIZE)+LSIZE, nrand(LSIZE)+LSIZE, debug?0:nrand(3)+1)) == nil)
		sysfatal("genlevel: %r");

	/* the player */
	player = mbyname("wizard");
	if(player == nil)
		sysfatal("monst: %r");

	/* don't delete when we die so ui works */
	incref(&player->ref);

	player->l = level;
	player->pt = level->up;

	maddinv(player, ibyname("dagger"));

	t = tileat(level, player->pt);
	t->unit = player->md->tile;
	t->monst = player;
	setflagat(level, player->pt, Fhasmonster);

	player->bt = btleaf("input", uibt);

	if(debug > 0){
		player->hp = 500;
		player->maxhp = 500;
	}

	uiredraw(0);

	for(;;){
			turn++;
			movemons();
	}
}
