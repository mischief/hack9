#include <u.h>
#include <libc.h>

#include "map.h"

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

typedef struct Entry Entry;
struct Entry {
	char *key;
	u32int hash;
	void *value;
	Entry *next;
};

#define HASHSIZE 13

struct Map {
	Entry *tab[HASHSIZE];
	void (*free)(void*);
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

	if(fr != nil)
		m->free = fr;
	else
		m->free = nopfree;

	return m;
}

void
mapfree(Map *m)
{
	int i;
	Entry *e, *next;

	for(i = 0; i < HASHSIZE; i++){
		e = m->tab[i];
		while(e != nil){
			next = e->next;
			free(e->key);
			m->free(e->value);
			free(e);
			e = next;
		}
	}
}

static Entry*
mapgete(Map *m, char *key)
{
	Entry *e;
	u32int hash;

	maphash(key, strlen(key), &hash);

	for(e = m->tab[hash % HASHSIZE]; e != nil; e = e->next)
		if(e->hash == hash && strcmp(e->key, key) == 0)
			return e;

	return nil;
}

int
mapset(Map *m, char *key, void *value)
{
	Entry *e;
	u32int hash;

	if((e = mapgete(m, key)) == nil){
		/* not found */
		e = mallocz(sizeof(Entry), 1);
		if(e == nil)
			return -1;

		e->key = strdup(key);
		if(e->key == nil){
			free(e);
			return -1;
		}

		maphash(key, strlen(key), &hash);

		e->hash = hash;
		e->value = value;

		e->next = m->tab[hash % HASHSIZE];
		m->tab[hash % HASHSIZE] = e;
	} else {
		/* already there */
		m->free(e->value);
		e->value = value;
	}

	return 0;
}

void*
mapget(Map *m, char *key)
{
	Entry *e;
	
	e = mapgete(m, key);

	if(e != nil)
		return e->value;

	return nil;
}

void
mapdelete(Map *m, char *key)
{
	Entry *e, *t;
	u32int hash;

	maphash(key, strlen(key), &hash);

	e = t = m->tab[hash % HASHSIZE];

	while(e != nil){
		if(e->hash == hash && strcmp(e->key, key) == 0){
			if(e == m->tab[hash % HASHSIZE])
				m->tab[hash % HASHSIZE] = e->next;
			else
				t->next = e->next;

			free(e->key);
			m->free(e->value);
			free(e);
			return;
		}

		t = e;
		e = e->next;
	}
}

void
mapdump(Map *m, int fd)
{
	u32int i;
	Entry *e;

	for(i = 0; i < HASHSIZE; i++){
		fprint(2, "SLOT %d\n", i);
		for(e = m->tab[i]; e != nil; e = e->next){
			fprint(fd, "%-10s\t%#0p\t%#010ux\n",
				e->key, e->value, e->hash);
		}
	}
}
