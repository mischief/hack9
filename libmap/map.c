#include <u.h>
#include <libc.h>

#include <map.h>

#define DBG if(0)

#define isdeleted(x) ((x>>31) != 0)
#define setentry(e, key, value) (e->key = key, e->value = value)
#define mapdistance(m, hash, slot) ((slot + m->size - (hash % m->size)) % m->size)

static void
jenkinshash(const void *k, uintptr len, u32int *h)
{
	u32int hash, i;
	const u8int *key;

	key = k;

/*
	for(hash = i = 0; i < len; i++){
		hash = hash * 33 ^ key[i];
	}

	assert(key);
	assert(h);
*/

	for(hash = i = 0; i < len; ++i){
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}

	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	/* XXX: drop top and bottom bits */
	hash &= 0x7FFFFFFF;
	hash |= (hash == 0);

	*h = hash;
}

void (*maphash)(const void*, uintptr, u32int*) = jenkinshash;

/* stanford bithacks */
static const int MultiplyDeBruijnBitPosition[32] = {
	0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
	8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
};

static int
ilog2(u32int v)
{
	// first round down to one less than a power of 2 
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return MultiplyDeBruijnBitPosition[(u32int)(v * 0x07C4ACDDU) >> 27];
}

typedef struct Entry Entry;
struct Entry {
	char *key;
	void *value;
};

struct Map {
	Entry *tab;
	u32int *htab;
	u32int size;
	u32int count;
	int probecount;
	void (*free)(void*);
};

const u32int primemax = 4294967295;

static u32int primesz[] = {
	5, 13, 31, 61, 89, 127, 521, 1279, 3217,
	9689, 11213, 19937, 23209, 44497, 86243,
	110503, 132049, 216091, 756839, 1257787,
	2976221, 3021377, 6972593, 13945193,
	27890389, 55780799, 111561613,
	4294967295
};

static void
nopfree(void *v)
{
	USED(v);
}

Map*
mapnew(void (*fr)(void*))
{
	Map *m;

	m = mallocz(sizeof(Map), 1);
	if(m == nil)
		return nil;

	m->size = primesz[0];
	m->tab = mallocz(sizeof(Entry)*m->size, 1);
	if(m->tab == nil){
		free(m);
		return nil;
	}

	m->htab = mallocz(sizeof(u32int)*m->size, 1);
	if(m->htab == nil){
		free(m->tab);
		free(m);
		return nil;
	}

	m->probecount = ilog2(m->size);

	if(fr != nil)
		m->free = fr;
	else
		m->free = nopfree;

	return m;
}

void
mapfree(Map *m)
{
	u32int i;
	Entry *e;

	for(i = 0; i < m->size; i++){
		if(m->htab[i] != 0 && !isdeleted(m->htab[i])){
			e = &m->tab[i];
			free(e->key);
			if(m->free != nil)
				m->free(e->value);
		}
	}

	free(m->tab);
	free(m->htab);
	free(m);
}

/*
static void
setentry(Entry *e, char *key, void *value)
{
	e->key = key;
	e->value = value;
}

static u32int
mapdistance(Map *m, u32int hash, u32int slot)
{
	return (slot + m->size - (hash % m->size)) % m->size;
}
*/

static void
mapswap(Map *m, u32int slot, char **key, void **value, u32int *hash)
{
	Entry *e;
	char *ctmp;
	void *vtmp;
	u32int utmp;

	e = &m->tab[slot];

	DBG fprint(2, "swapping %s/%s at %ud\n", e->key, *key, slot);

	ctmp = e->key;
	e->key = *key;
	*key = ctmp;

	vtmp = e->value;
	e->value = *value;
	*value = vtmp;

	utmp = m->htab[slot];
	m->htab[slot] = *hash;
	*hash = utmp;
}

static int mapgrow(Map*);
static void edelete(Map*, Entry*);

static int
mapset1(Map *m, char *key, void *value, u32int hash)
{
	int used;
	u32int slot, i, dist, edist, ehash;
	Entry *e;

loop:
	slot = hash % m->size;
	dist = 0;

	for(i = 0; i <= m->probecount; i++){
		used = m->htab[slot] == hash;
		if(m->htab[slot] == 0 || used){
found:
			DBG fprint(2, "set slot=%ud hash=%#08ux key=%s value=%#p\n",
				slot, hash, key, value);
			e = &m->tab[slot];

			/* not the same key! find new slot */
			if(used && strcmp(key, e->key) != 0)
				goto next;

			/* on overwrite of same key, delete old value */
			if(used && strcmp(key, e->key) == 0)
				edelete(m, e);

			m->htab[slot] = hash;

			setentry(e, key, value);
			return 0;			
		}

		ehash = m->htab[slot];
		edist = mapdistance(m, ehash, slot);
		if(edist < dist){
			if(isdeleted(ehash))
				goto found;

			/* robin hood */
			mapswap(m, slot, &key, &value, &hash);
			dist = edist;
		}

next:
		slot = (slot + 1) % m->size;
		dist++;
	}

	/* hit probe count - grow map */
	if(mapgrow(m) != 0)
		return -1;
	goto loop;
}

static u32int
nextsize(u32int v)
{
	u32int i;

	for(i = 0; i < nelem(primesz); i++){
		if(primesz[i] > v){
			i = primesz[i];
			break;
		}
	}

	assert(i != primemax);
	return i;
}

static int
mapgrow(Map *m)
{
	Entry *tab, *e;
	Map tmp;
	u32int i, newsz, ohash, *htab;

	newsz = nextsize(m->size);

	DBG fprint(2, "grow to %ud\n", newsz);

	tab = mallocz(sizeof(Entry) * newsz, 1);
	if(tab == nil)
		return -1;

	htab = mallocz(sizeof(u32int) * newsz, 1);
	if(htab == nil){
		free(tab);
		return -1;
	}

	tmp.tab = tab;
	tmp.htab = htab;
	tmp.size = newsz;
	tmp.probecount = ilog2(newsz);

	for(i = 0; i < m->size; i++){
		e = &m->tab[i];
		ohash = m->htab[i];
		if(ohash != 0 && !isdeleted(ohash)){
			if(mapset1(&tmp, e->key, e->value, m->htab[i]) != 0){
				/* whatever */
				abort();
			}
		}
	}

	free(m->tab);
	free(m->htab);

	m->tab = tmp.tab;
	m->htab = tmp.htab;
	m->size = tmp.size;
	m->probecount = tmp.probecount;

	return 0;
}

int
mapset(Map *m, char *key, void *value)
{
	u32int hash;
	char *kd;

	maphash(key, strlen(key), &hash);

	kd = strdup(key);
	if(kd == nil)
		return -1;
	setmalloctag(kd, getcallerpc(&m));

	if(mapset1(m, kd, value, hash) != 0)
		return -1;

	m->count++;

	return 0;
}

static int
mapslot(Map *m, char *key)
{
	u32int hash, ehash, slot, dist;
	Entry *e;

	jenkinshash(key, strlen(key), &hash);
	slot = hash % m->size;
	dist = 0;

	for(;;){
		ehash = m->htab[slot];
		if(ehash == 0)
			return -1;

		if(dist > mapdistance(m, ehash, slot))
			return -1;

		e = &m->tab[slot];
		if(ehash == hash && strcmp(e->key, key) == 0)
			return slot;

		slot = (slot + 1) % m->size;
		dist++;
	}
}

void*
mapget(Map *m, char *key)
{
	int slot;
	Entry *e;

	slot = mapslot(m, key);
	if(slot < 0)
		return nil;

	e = &m->tab[slot];
	return e->value;
}

static void
edelete(Map *m, Entry *e)
{
	free(e->key);
	m->free(e->value);
	e->key = nil;
	e->value = nil;
}

static void
sdelete(Map *m, int slot)
{
	Entry *e;

	m->htab[slot] |= 0x80000000;
	e = &m->tab[slot];
	edelete(m, e);
}

void
mapdelete(Map *m, char *key)
{
	int slot;

	slot = mapslot(m, key);
	if(slot < 0)
		return;

	sdelete(m, slot);
	m->count--;
}

void
mapdump(Map *m, int fd)
{
	u32int i;
	Entry *e;

	for(i = 0; i < m->size; i++){
		e = &m->tab[i];
		fprint(fd, "%-04d %-10s\t%#0p\t%#010ux - D=%ud\n",
			i, e->key, e->value, m->htab[i], e->key == nil ? 0 : mapdistance(m, m->htab[i], i));
	}
}
