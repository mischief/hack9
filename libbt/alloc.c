#include <u.h>
#include <libc.h>

#include "bt.h"
#include "impl.h"

static void
btinitnode(BehaviorNode *node, short type, char *name)
{
	assert(type >= 0 || type < BT_MAXTYPE);
	memset(node, 0, sizeof(BehaviorNode));
	node->type = type;
	snprint(node->name, sizeof(node->name), "%s", name);
}

BehaviorNode*
btleaf(char *name, BehaviorAction action)
{
	BehaviorLeaf *leaf;

	assert(action != nil);

	leaf = mallocz(sizeof(BehaviorLeaf), 1);
	if(leaf == nil)
		return nil;

	setmalloctag(leaf, getcallerpc(&name));

	btinitnode(&leaf->node, BT_LEAF, name);

	leaf->action = action;
	return (BehaviorNode*)leaf;
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
btbranch(BehaviorBranch *node, va_list ap)
{
	BehaviorNode *child;

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
	BehaviorBranch *branch;

	branch = mallocz(sizeof(BehaviorBranch), 1);
	if(branch == nil)
		return nil;

	setmalloctag(branch, getcallerpc(&name));

	btinitnode(&branch->node, BT_SEQUENCE, name);

	va_start(ap, name);

	if(btbranch(branch, ap) < 0){
		free(branch);
		branch = nil;
	}

	va_end(ap);
	return (BehaviorNode*)branch;
}

BehaviorNode*
btpriority(char *name, ...)
{
	va_list ap;
	BehaviorBranch *priority;

	priority = mallocz(sizeof(BehaviorBranch), 1);
	if(priority == nil)
		return nil;

	setmalloctag(priority, getcallerpc(&name));

	btinitnode(&priority->node, BT_PRIORITY, name);

	va_start(ap, name);

	if(btbranch(priority, ap) < 0){
		free(priority);
		priority = nil;
	}

	va_end(ap);
	return (BehaviorNode*)priority;
}

BehaviorNode*
btparallel(char *name, int S, int F, ...)
{
	va_list ap;
	BehaviorParallel *parallel;

	parallel = mallocz(sizeof(BehaviorParallel), 1);
	if(parallel == nil)
		return nil;

	setmalloctag(parallel, getcallerpc(&name));

	btinitnode(&parallel->branch.node, BT_PARALLEL, name);

	parallel->S = S;
	parallel->F = F;

	va_start(ap, F);

	if(btbranch(&parallel->branch, ap) < 0){
		free(parallel);
		parallel = nil;
	}

	va_end(ap);
	return (BehaviorNode*)parallel;
}

BehaviorNode*
btdynguard(char *name, ...)
{
	va_list ap;
	BehaviorBranch *dynguard;

	dynguard = mallocz(sizeof(BehaviorBranch), 1);
	if(dynguard == nil)
		return nil;

	setmalloctag(dynguard, getcallerpc(&name));

	btinitnode(&dynguard->node, BT_DYNGUARD, name);

	dynguard->childcurrent = -1;

	va_start(ap, name);

	if(btbranch(dynguard, ap) < 0){
		free(dynguard);
		dynguard = nil;
	}

	va_end(ap);
	return (BehaviorNode*)dynguard;	
}

BehaviorNode*
btinvert(char *name, BehaviorNode *child)
{
	BehaviorBranch *invert;

	invert = mallocz(sizeof(BehaviorBranch), 1);
	if(invert == nil)
		return nil;

	setmalloctag(invert, getcallerpc(&name));

	btinitnode(&invert->node, BT_INVERT, name);

	if(btaddchild(invert, child) < 0){
		free(invert);
		invert = nil;
	}

	return (BehaviorNode*)invert;
}

void
btsetguard(BehaviorNode *node, BehaviorNode *guard)
{
	if(node != nil)
		node->guard = guard;
}

void
btsetend(BehaviorNode *node, void (*end)(void*))
{
	if(node != nil)
		node->end = end;
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
