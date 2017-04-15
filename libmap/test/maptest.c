#include <u.h>
#include <libc.h>
#include <thread.h>

#include <map.h>
#include <bench.h>

static void
tfree(void *v)
{
	free(v);
}

static int*
mkint(int i)
{
	int *p;

	p = malloc(sizeof(int));
	assert(p != nil);
	*p = i;
	return p;
}

void
bench_mapset(B *b)
{
	int i;
	char buf[32];
	Map *m;

	m = mapnew(tfree);
	assert(m != nil);

	benchreset(b);

	for(i = 0; i < b->N; i++){
		snprint(buf, sizeof(buf), "%d", i);
		assert(mapset(m, buf, mkint(i)) == 0);
	}

	mapfree(m);
}

void
bench_mapget(B *b)
{
	int i, n;
	char buf[32];
	Map *m;

	m = mapnew(tfree);
	assert(m != nil);

	for(i = 0; i < b->N; i++){
		snprint(buf, sizeof(buf), "%d", i);
		assert(mapset(m, buf, mkint(i)) == 0);

		if(i >= 249){
			snprint(buf, sizeof(buf), "%d", 249);
			assert(mapget(m, buf) != nil);
		}
	}

	benchreset(b);

	for(i = 0; i < b->N; i++){
		n = i;
		snprint(buf, sizeof(buf), "%d", n);
		if(mapget(m, buf) == nil){
			//fprint(2, "get %d/%d == nil\n", n, b->N);
			//mapdump(m, 2);
			abort();
		}
	}

	mapfree(m);
}

void
bench_mapdelete(B *b)
{
	int i;
	char buf[32];
	Map *m;

	m = mapnew(tfree);
	assert(m != nil);

	for(i = 1; i < b->N; i++){
		snprint(buf, sizeof(buf), "%d", i);
		assert(mapset(m, buf, mkint(i)) == 0);
	}

	benchreset(b);

	for(i = 1; i < b->N; i++){
		snprint(buf, sizeof(buf), "%d", i);
		mapdelete(m, buf);
	}

	mapfree(m);
}

void
bench_snprintn(B *b)
{
	int i;
	char buf[32];

	for(i = 0; i < b->N; i++){
		snprint(buf, sizeof(buf), "%d", i);
	}
}

void
test_mapset(void)
{
	int i;
	char buf[32];
	void *v;
	vlong v1, v2;
	Map *m;

	m = mapnew(nil);
	assert(m != nil);

	v1 = nsec();

	for(i = 0; i < 10000; i++){
		snprint(buf, sizeof(buf), "%d", i);
		//fprint(2, "mapset('%d', '%d')\n", i, i);
		assert(mapset(m, buf, mkint(i)) == 0);
	}

	v2 = nsec();
	fprint(2, "delta = %llud ms\n", (v2-v1)/1000000LL);

	v1 = nsec();
	for(i = 0; i < 10000; i++){
		snprint(buf, sizeof(buf), "%d", i);
		v = mapget(m, buf);
		//fprint(2, "mapget('%s') == %#p\n", buf, v);
		assert(i == *((int*)v));
	}

	v2 = nsec();
	fprint(2, "delta = %llud ms\n", (v2-v1)/1000000LL);

	//mapdump(m, 2);
}

static void
test_mapupdate(void)
{
	int rv;
	Map *m;

	m = mapnew(tfree);
	assert(m != nil);

	rv = mapset(m, "foo", malloc(0));
	assert(rv == 0);

	rv = mapset(m, "foo", malloc(0));
	assert(rv == 0);

	//mapdump(m, 2);
	mapfree(m);
}

int
atnote(void *, char*)
{
	abort();
	threadexitsall(nil);
	return NDFLT;
}

void
usage(void)
{
	fprint(2, "usage: %s\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char *argv[])
{
	ARGBEGIN{
	default:
		usage();
	}ARGEND

//#include <pool.h>
//mainmem->flags = POOL_PARANOIA;

	//threadnotify(atnote, 1);

	benchinit();
	//BENCHTIME = 10000000000LL;

	//bench("snprint", bench_snprintn);

	test_mapset();
	test_mapupdate();
	bench("mapset", bench_mapset);
	bench("mapget", bench_mapget);
	bench("mapdelete", bench_mapdelete);

	threadexitsall(nil);
}
