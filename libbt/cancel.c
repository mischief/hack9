#include <u.h>
#include <libc.h>

#include "bt.h"
#include "impl.h"

static void
cancelleaf(BehaviorNode *node, BehaviorState *bs, void *agent)
{
	BehaviorLeafState *state;

	USED(agent);

	state = btstatefor(node, bs);
	state->node.status = TASKFRESH;
}

static void
cancelbranch(BehaviorNode *node, BehaviorState *bs, void *agent)
{
	int i;
	BehaviorBranch *branch;
	BehaviorBranchState *state;
	BehaviorNode *child;

	branch = (BehaviorBranch*)node;
	state = btstatefor(node, bs);

	for(i = 0; i < branch->childcount; i++){
		child = branch->children[i];
		btcancel(child, bs, agent);
	}

	if(node->type == BT_DYNGUARD)
		state->childcurrent = -1;
	else
		state->childcurrent = 0;

	state->node.status = TASKFRESH;
}

static void (*cancelfns[])(BehaviorNode*, BehaviorState*, void*) = {
[BT_LEAF]		cancelleaf,
[BT_SEQUENCE]	cancelbranch,
[BT_PRIORITY]	cancelbranch,
[BT_PARALLEL]	cancelbranch,
[BT_DYNGUARD]	cancelbranch,
[BT_INVERT]		cancelbranch,
};

void
btcancel(BehaviorNode *node, BehaviorState *bs, void *agent)
{
	if(node->guard != nil)
		btcancel(node->guard, bs, agent);

	cancelfns[node->type](node, bs, agent);
}
