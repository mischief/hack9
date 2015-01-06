#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>

#include "dat.h"
#include "alg.h"

enum
{
	LSIZE	= 20,
};

int debug;
Monster *player;
char *user;
char *home;
long turn = 0;

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
	Point p;
	Monster *m;
	Level *l;
	Priq *tomove;

	tomove = priqnew(32);
	l = player->l;

	/* run ai on level, for spawns etc */
	if(l->ai->exec != nil)
		l->ai->exec(l->ai);

	for(p.x = 0; p.x < l->width; p.x++){
		for(p.y = 0; p.y < l->height; p.y++){
			if(hasflagat(l, p, Fhasmonster)){
				m = tileat(l, p)->monst;
				/* i don't think this is supposed to happen. */
				if(m == nil)
					continue;
				incref(m);
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
void
usage(void)
{
	fprint(2, "usage: %s [-d]", argv0);
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
	default:
		usage();
	}ARGEND

	//#include <pool.h>
	//mainmem->flags = POOL_PARANOIA|POOL_ANTAGONISM;

	srand(truerand());

	user = getenv("user");
	home = getenv("home");

	uiinit(argv0);

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
		player->hp = 500;
		player->md->maxhp = 500;
	}

	uiredraw(0);

	for(;;){
			turn++;
			movemons();
	}
}

