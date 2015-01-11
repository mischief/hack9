#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <bio.h>
#include <ndb.h>

#include "dat.h"

enum
{
	ITEMNAME	= 0,
	ITEMTILE,
	ITEMTYPE,
	ITEMCOST,
	ITEMWEIGHT,
	ITEMATK,
	ITEMAC,
	ITEMSTACKS,
};

static char *itemdbcmd[] = {
[ITEMNAME]		"item",
[ITEMTILE]		"tile",
[ITEMTYPE]		"type",
[ITEMCOST]		"cost",
[ITEMWEIGHT]		"weight",
[ITEMATK]		"atk",
[ITEMAC]		"ac",
[ITEMSTACKS]	"stacks",
};

static char *itemtype[] = {
[IWEAPON]	"weapon",
[IHELMET]	"helmet",
[ISHIELD]	"shield",
[IARMOR]	"armor",
};

static Ndb *itemdb;

static int
ifmt(Fmt *f)
{
	char *name, extra[64];
	Item *i;

	extra[0] = 0;
	i = va_arg(f->args, Item*);

	if(i->name != nil)
		name = i->name;
	else
		name = i->id->name;

	switch(i->id->type){
	case IWEAPON:
		snprint(extra, sizeof(extra), " (%dd%d)", i->id->rolls, i->id->atk);
		break;
	case IHELMET:
	case ISHIELD:
	case IARMOR:
		snprint(extra, sizeof(extra), " (ac %d)", i->id->ac);
		break;
	}

	if(i->count != 1)
		return fmtprint(f, "%d %ss%s", i->count, name, extra);
	return fmtprint(f, "a %s%s", name, extra);
}

int
itemdbopen(char *file)
{
	fmtinstall('i', ifmt);

	itemdb = ndbopen(file);
	if(itemdb == nil)
		return 0;
	return 1;
}

typedef struct idcache idcache;
struct idcache
{
	ItemData *id;
	idcache *next;
};

static idcache *itemcache = nil;

/* TODO: sanity checks */
static int
idaddattr(ItemData *id, int attr, char *val)
{
	int i;

	switch(attr){
	case ITEMNAME:
		strncpy(id->name, val, sizeof(id->name)-1);
		id->name[sizeof(id->name)-1] = 0;
		break;
	case ITEMTILE:
		id->tile = atoi(val);
		break;
	case ITEMTYPE:
		for(i = 0; i < nelem(itemtype); i++){
			if(strcmp(val, itemtype[i]) == 0){
				id->type = i;
				return 1;
			}
		}
		werrstr("unknown item type '%s'", val);
		return 0;
		break;
	case ITEMCOST:
		id->cost = atoi(val);
		break;
	case ITEMWEIGHT:
		id->weight = atoi(val);
		break;
	case ITEMATK:
		if(!parseroll(val, &id->rolls, &id->atk)){
			werrstr("bad roll '%s'", val);
			return 0;
		}
		break;
	case ITEMAC:
		id->ac = atoi(val);
		break;
	case ITEMSTACKS:
		id->flags |= IFSTACKS;
		break;
	default:
		return 0;
	}

	return 1;
}

ItemData*
idbyname(char *item)
{
	int i;
	Ndbtuple *t, *nt;
	Ndbs search;
	ItemData *id;
	idcache *idc;

	/* cached? */
	for(idc = itemcache; idc != nil; idc = idc->next){
		if(strcmp(item, idc->id->name) == 0){
			return idc->id;
		}
	}

	t = ndbsearch(itemdb, &search, "item", item);
	if(t == nil)
		return nil;

	id = mallocz(sizeof(ItemData), 1);
	assert(id != nil);

	for(nt = t; nt != nil; nt = nt->entry){
		for(i = 0; i < nelem(itemdbcmd); i++){
			if(strcmp(nt->attr, itemdbcmd[i]) == 0){
				if(!idaddattr(id, i, nt->val))
					sysfatal("bad item value '%s=%s': %r", nt->attr, nt->val);
				goto okattr;
			}
		}
		sysfatal("unknown item attribute '%s'", nt->attr);
okattr:
		continue;
	}

	idc = mallocz(sizeof(idcache), 1);
	assert(idc != nil);

	if(itemcache == nil){
		itemcache = idc;
		itemcache->id = id;
	} else {
		idc->id = id;
		idc->next = itemcache;
		itemcache = idc;
	}

	return id;
}

Item*
ibyname(char *item)
{
	ItemData *id;
	Item *i;

	id = idbyname(item);
	if(id == nil)
		goto missing;

	i = mallocz(sizeof(Item), 1);
	assert(i != nil);

	i->count = 1;
	i->id = id;

	incref(&i->ref);

	return i;
missing:
	werrstr("missing data for item '%s'", item);
	return nil;

}

void
ifree(Item *i)
{
	if(i == nil)
		return;

	if(decref(&i->ref) != 0)
		return;

	free(i);
}

void
ilfree(ItemList *il)
{
	Item *i, *next;
	for(i = il->head; i != nil; i = next){
		next = i->next;
		ifree(i);
	}
}

/* take n'th item out of il. */
Item*
iltakenth(ItemList *il, int n)
{
	int j;
	Item *i, *prev;

	i = il->head;
	prev = nil;
	
	for(j = 0; j < n && i != nil; j++){
		prev = i;
		i = i->next;
	}

	if(i == nil)
		return nil;

	if(n != 0)
		prev->next = i->next;
	else
		il->head = il->head->next;

	il->count--;

	return i;
}

Item*
ilnth(ItemList *il, int n)
{
	int j;
	Item *i;

	i = il->head;
	
	for(j = 0; j < n && i != nil; j++){
		i = i->next;
	}

	if(i == nil)
		return nil;

	return i;
}

void
iladd(ItemList *il, Item *i)
{
	i->next = il->head;
	il->head = i;
	il->count++;
}
