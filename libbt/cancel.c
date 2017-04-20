#include <u.h>
#include <libc.h>

#include "bt.h"
#include "impl.h"

static void
cancelleaf(BehaviorNode *node, void *agent)
{
	BehaviorLeaf *leaf;

	USED(agent);

	leaf = (BehaviorLeaf*)node;

	leaf->status = TASKFRESH;
}

static void
cancelbranch(BehaviorNode *node, void *agent)
{
	int i;
	BehaviorBranch *branch;
	BehaviorNode *child;

	branch = (BehaviorBranch*)node;

	for(i = 0; i < branch->childcount; i++){
		child = branch->children[i];
		btcancel(child, agent);
	}

	if(node->type == BT_DYNGUARD)
		branch->childcurrent = -1;
	else
		branch->childcurrent = 0;

	branch->status = TASKFRESH;
}

static void (*cancelfns[])(BehaviorNode*, void*) = {
[BT_LEAF]		cancelleaf,
[BT_SEQUENCE]	cancelbranch,
[BT_PRIORITY]	cancelbranch,
[BT_PARALLEL]	cancelbranch,
[BT_DYNGUARD]	cancelbranch,
[BT_INVERT]		cancelbranch,
};

void
btcancel(BehaviorNode *node, void *agent)
{
	if(node->guard != nil)
		btcancel(node->guard, agent);

	cancelfns[node->type](node, agent);
}
