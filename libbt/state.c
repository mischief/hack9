#include <u.h>
#include <libc.h>

#include "bt.h"
#include "impl.h"

static void
_initnode(BehaviorNode *node, unsigned char *idx)
{
	int i;
	BehaviorNode *child;
	BehaviorBranch *branch;

	if(node->id != 0)
		return;

	node->id = *idx;
	(*idx)++;

	//print("%s = %hhud\n", node->name, node->id);

	switch(node->type){
	case BT_LEAF:
		break;
	case BT_SEQUENCE:
	case BT_PRIORITY:
	case BT_PARALLEL:
	case BT_DYNGUARD:
	case BT_INVERT:
		branch = (BehaviorBranch*) node;

		for(i = 0; i < branch->childcount; i++){
			child = branch->children[i];
			_initnode(child, idx);
		}

		break;
	default:
		abort();
	}
}

Behavior*
btroot(BehaviorNode *root)
{
	unsigned char idx;
	Behavior *b;

	assert(root != nil);

	b = mallocz(sizeof(Behavior), 1);
	if(b == nil)
		return nil;

	idx = 0;
	_initnode(root, &idx);

	// TODO: alloc idx size array
	b->root = root;
	b->nstates = idx;

	return b;
}

BehaviorState*
btstatenew(Behavior *b)
{
	BehaviorState *state;

	assert(b != nil);

	state = mallocz(sizeof(BehaviorState) + b->nstates * sizeof(state->states[0]), 1);
	if(state == nil)
		return nil;

	state->behavior = b;

	return state;
}

void
btstatefree(BehaviorState *bs, void *agent)
{
	assert(bs != nil);

	btcancel(bs->behavior->root, bs, agent);
	free(bs);
}

