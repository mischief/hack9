#include <u.h>
#include <libc.h>

#include <bt.h>
#include "impl.h"

BehaviorNode*
btleaf(char *name, BehaviorAction action)
{
	BehaviorLeaf *node;

	assert(action != nil);

	node = mallocz(sizeof(BehaviorLeaf), 1);
	if(node == nil)
		return nil;

	setmalloctag(node, getcallerpc(&name));

	snprint(node->name, sizeof(node->name), "%s", name);
	node->type = BT_LEAF;
	node->ref = 0;
	node->action = action;
	return node;
}

static int
btaddchild(BehaviorBranch *node, BehaviorNode *child)
{
	BehaviorNode **kids;

	kids = realloc(node->children, sizeof(BehaviorNode*) * (node->childcount + 1));
	if(kids == nil)
		return -1;

	child->ref++;

	node->children = kids;
	node->children[node->childcount++] = child;
	return 0;
}

static int
btbranch(BehaviorBranch *node, int type, va_list ap)
{
	BehaviorNode *child;

	assert(type >= 0 || type < BT_MAXTYPE);

	node->type = type;

	while((child = va_arg(ap, BehaviorNode*)) != nil){
		if(btaddchild(node, child) < 0)
			goto fail;
	}

	return 0;

fail:
	free(node->children);
	return -1;
}

BehaviorNode*
btsequence(char *name, ...)
{
	va_list ap;
	BehaviorBranch *node;

	node = mallocz(sizeof(BehaviorBranch), 1);
	if(node == nil)
		return nil;

	setmalloctag(node, getcallerpc(&name));

	snprint(node->name, sizeof(node->name), "%s", name);

	va_start(ap, name);

	if(btbranch(node, BT_SEQUENCE, ap) < 0){
		free(node);
		node = nil;
	}

	va_end(ap);
	return node;
}

BehaviorNode*
btpriority(char *name, ...)
{
	va_list ap;
	BehaviorBranch *node;

	node = mallocz(sizeof(BehaviorBranch), 1);
	if(node == nil)
		return nil;

	setmalloctag(node, getcallerpc(&name));

	snprint(node->name, sizeof(node->name), "%s", name);

	va_start(ap, name);

	if(btbranch(node, BT_PRIORITY, ap) < 0){
		free(node);
		node = nil;
	}

	va_end(ap);
	return node;
}

BehaviorNode*
btparallel(char *name, int S, int F, ...)
{
	va_list ap;
	BehaviorParallel *node;

	node = mallocz(sizeof(BehaviorParallel), 1);
	if(node == nil)
		return nil;

	setmalloctag(node, getcallerpc(&name));

	snprint(node->name, sizeof(node->name), "%s", name);

	node->S = S;
	node->F = F;

	va_start(ap, F);

	if(btbranch(node, BT_PARALLEL, ap) < 0){
		free(node);
		node = nil;
	}

	va_end(ap);
	return node;
}

BehaviorNode*
btdynguard(char *name, ...)
{
	va_list ap;
	BehaviorBranch *node;

	node = mallocz(sizeof(BehaviorBranch), 1);
	if(node == nil)
		return nil;

	setmalloctag(node, getcallerpc(&name));

	snprint(node->name, sizeof(node->name), "%s", name);
	node->childcurrent = -1;

	va_start(ap, name);

	if(btbranch(node, BT_DYNGUARD, ap) < 0){
		free(node);
		node = nil;
	}

	va_end(ap);
	return node;	
}

BehaviorNode*
btinvert(char *name, BehaviorNode *child)
{
	BehaviorBranch *node;

	node = mallocz(sizeof(BehaviorBranch), 1);
	if(node == nil)
		return nil;

	setmalloctag(node, getcallerpc(&name));

	snprint(node->name, sizeof(node->name), "%s", name);

	node->type = BT_INVERT;

	if(btaddchild(node, child) < 0){
		free(node);
		node = nil;
	}

	return node;
}

void
btsetguard(BehaviorNode *node, BehaviorNode *guard)
{
	node->guard = guard;
}

static void
btfreetree(BehaviorNode *node, void *agent)
{
	int i;
	BehaviorBranch *branch;

	if(--node->ref > 0)
		return;

	if(node->guard != nil){
		btfreetree(node->guard, agent);
		node->guard = nil;
	}

	if(node->type != BT_LEAF){
		branch = (BehaviorBranch*)node;
		if(branch->children != nil){
			for(i = 0; i < branch->childcount; i++){
				btfreetree(branch->children[i], agent);
				branch->children[i] = nil;
			}
			branch->childcount = 0;
			free(branch->children);
			branch->children = nil;
		}
	}

	free(node);
}

void
btfree(BehaviorNode *node, void *agent)
{
	btcancel(node, agent);
	btfreetree(node, agent);
}
