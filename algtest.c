#include <u.h>
#include <libc.h>

#include "alg.h"

#define tassert(x) do{if(x){}else{werrstr("assert: %s", "x"); return 1;}}while(0)

typedef int(*tfun)(void);

static int
icmp(void *a, void *b)
{
	int ia, ib;
	ia = (int)a;
	ib = (int)b;
	return ia < ib ? -1 : ia > ib;
}

int
testpriq(void)
{
	Priq *p;

	p = priqnew(0);
	tassert(p != nil);

	priqpush(p, (void*)1, icmp);
	priqpush(p, (void*)2, icmp);
	priqpush(p, (void*)3, icmp);

	tassert(priqtop(p) == (void*)1);
	tassert(priqpop(p, icmp) == (void*)1);
	tassert(priqpop(p, icmp) == (void*)2);
	tassert(priqpop(p, icmp) == (void*)3);

	priqfree(p);
	return 0;
}

struct {
	char *n;
	tfun f;
} tfuns[] = {
	{"priq", testpriq},
	{nil, nil},
};

void
main(void)
{
	int i;
	for(i = 0; tfuns[i].f != nil; i++){
		fprint(2, "%s: ", tfuns[i].n);
		if(tfuns[i].f() != 0)
			sysfatal("failed: %r");
		else
			fprint(2, "ok\n");
	}
	exits(nil);
}
