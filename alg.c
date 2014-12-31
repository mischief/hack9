#include <u.h>
#include <libc.h>
#include "alg.h"

Priq*
priqnew(int size)
{
	Priq *p;
	p = mallocz(sizeof(Priq), 1);
	assert(p != nil);
	p->elem = mallocz(sizeof(void*)*(size+1), 1);
	assert(p->elem != nil);
	p->n = 0;
	p->alloc = size+1;
	return p;
}

void
priqfree(Priq *q)
{
	free(q->elem);
	free(q);
}

void
priqpush(Priq *q, void *val, pricmp cmp)
{
	void **newe;
	int i;

	if(q->n+1 >= q->alloc){
		q->alloc *= 2;
		newe = realloc(q->elem, sizeof(void*)*q->alloc);
		assert(newe != nil);
		q->elem = newe;
	}

	for(i = ++q->n; i > 1 && cmp(q->elem[i/2], val) > 0; i /= 2){
		q->elem[i] = q->elem[i/2];
	}

	q->elem[i] = val;
}

void*
priqpop(Priq *q, pricmp cmp)
{
	void *val, *last;
	int i, child;

	if(q->n == 0)
		return nil;

	val = q->elem[1];
	last = q->elem[q->n--];

	for(i = 1; i*2 <= q->n; i = child){
		child = i*2;
		if(child != q->n && cmp(q->elem[child], q->elem[child+1]) > 0)
			child++;
		if(cmp(last, q->elem[child]) > 0)
			q->elem[i] = q->elem[child];
		else
			break;
	}

	q->elem[i] = last;

	/* lets not bother shrinking for now */
	/*
	if(q->n < q->alloc/2 && q->n >= 16){
		q->elem = realloc(q->elem, (q->alloc /= 2) * sizeof(void*));
	}
	*/

	return val;
}

void*
priqtop(Priq *q){
	if(q->n == 0)
		return nil;
	return q->elem[1];
}

int
priqhas(Priq *q, void *val)
{
	int i;
	for(i = 1; i < q->n; i++){
		if(q->elem[i] == val)
			return 1;
	}
	return 0;
}

