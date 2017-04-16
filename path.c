#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "map.h"
#include "bt.h"

#include "dat.h"
#include "alg.h"

/* manhattan distance */
int
manhattan(Point cur, Point targ)
{
	return ORTHOCOST * (abs(cur.x - targ.x) + abs(cur.y - targ.y));
}

/* pathfinding implementation */
typedef struct PNode PNode;
struct PNode
{
	PNode *parent;

	/* visited? */
	int closed;
	/* cost from start to this node */
	int G;
	/* estimated cost from this node to end */
	int H;
	/* X/Y */
	Point pt;
};

static int
pnodecmp(void *a, void *b)
{
	PNode *pa, *pb;
	int v1, v2;
	pa = a, pb = b;
	v1 = pa->G + pa->H;
	v2 = pb->G + pb->H;
	return v1 < v2 ? -1 : v1 > v2;
}

int
pathfind(Level *l, Point start, Point end, Point **path, int not)
{
	int i, nneigh, npath;
	Point *neighborp, pt;
	PNode *nodes, *p, *neigh, *head, *tmp, *rev;
	Priq *q;

	npath = -1;

	//dbg("pathfind %P → %P not %06b", start, end, not);
	if(eqpt(start, end))
		return npath;

	nodes = emalloc(sizeof(PNode) * l->width * l->height);
	if(nodes == nil)
		return npath;

	q = priqnew(0);
	if(q == nil)
		goto fail;

	p = &nodes[start.y*l->width + start.x];
	p->pt = start;
	priqpush(q, p, pnodecmp);

	while((p = priqpop(q, pnodecmp)) != nil){
		p->closed = 1;

		if(eqpt(p->pt, end))
			break;

		neighborp = neighbor(l->r, p->pt, &nneigh);
		shuffle(neighborp, nneigh, sizeof(Point));
		for(i = 0; i < nneigh; i++){
			pt = neighborp[i];
			neigh = &nodes[pt.y*l->width + pt.x];
			if(eqpt(pt, end) || (flagat(l, pt) & not) == 0)
			if(!neigh->closed){
				if(!priqhas(q, neigh)){
					neigh->pt = pt;
					neigh->G = p->G + ORTHOCOST;
					neigh->H = manhattan(pt, end);
					priqpush(q, neigh, pnodecmp);
					neigh->parent = p;
				} else if(p->G + ORTHOCOST < neigh->G){
					neigh->G = p->G + ORTHOCOST;
					neigh->parent = p;
				}
			}
		}

		free(neighborp);
	}

	p = &nodes[end.y*l->width + end.x];

	if(p->parent == nil){
		werrstr("no path");
		goto fail2;
	}

	/* reverse; ugh */
	i = 0;
	rev = nil;
	for(head = &nodes[end.y*l->width + end.x]; head; i++){
		tmp = head;
		head = head->parent;
		tmp->parent = rev;
		rev = tmp;
	}

	npath = i;

	*path = emalloc(sizeof(Point) * i + 2);
	for(i = 0; i < npath; i++){
		(*path)[i] = rev->pt;
		rev = rev->parent;
	}

fail2:
	priqfree(q);
fail:
	free(nodes);
	return npath;
}

