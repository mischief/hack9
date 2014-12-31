typedef struct Priq Priq;
struct Priq
{
	void **elem;
	int n;
	int alloc;
};

typedef int (*pricmp)(void *v1, void *v2);
Priq *priqnew(int size);
void priqfree(Priq *q);
void priqpush(Priq *q, void *val, pricmp cmp);
void *priqpop(Priq *q, pricmp cmp);
void *priqtop(Priq *q);
int priqhas(Priq *q, void *val);

