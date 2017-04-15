typedef struct Map Map;

#pragma incomplete Map;

extern void (*maphash)(const void*, uintptr, u32int*);

Map *mapnew(void (*)(void*));
void mapfree(Map*);
int mapset(Map*, char*, void*);
void *mapget(Map*, char*);
void mapdelete(Map*, char*);

void mapdump(Map*, int);
