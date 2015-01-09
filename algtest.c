#include <u.h>
#include <libc.h>
#include <draw.h>

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

int
testshuffle(void)
{
	int arr[] = {1, 3, 5, 7, 9, 11, 13, 17};
	Point pt[] = {{0,0}, {1, 1}, {2, 2}, {3, 3}};
	long i, sz;

	fmtinstall('P', Pfmt);
	
	sz = sizeof(arr)/sizeof(arr[0]);

	print("\n");
	for(i=0;i<sz;i++)
		print("%d ", arr[i]);
	print("\n");
	shuffle(arr, sz, sizeof(arr[0]));
	for(i=0;i<sz;i++)
		print("%d ", arr[i]);
	print("\n");

	sz = sizeof(pt)/sizeof(pt[0]);
	for(i=0;i<sz;i++)
		print("%P ", pt[i]);
	print("\n");
	shuffle(pt, sz, sizeof(pt[0]));
	for(i=0;i<sz;i++)
		print("%P ", pt[i]);
	print("\n");
	return 0;
}

struct {
	char *n;
	tfun f;
} tfuns[] = {
	{"priq", testpriq},
	{"shuffle", testshuffle},
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
