#include <u.h>
#include <libc.h>

#include "bt.h"
#include "impl.h"

static int btnodetick(BehaviorNode*, BehaviorState*, void*);

void*
btstatefor(BehaviorNode *node, BehaviorState *bs)
{
	return &bs->states[node->id];
}

/* -1 on failure, >= 0 on success */
static int
btcheckguard(BehaviorNode *node, BehaviorState *bs, void *agent)
{
	int status;

	assert(node != nil);
	assert(bs != nil);

	if(node->guard == nil)
		return 0;

	/* check the guard of the guard */
	if(btcheckguard(node->guard, bs, agent) < 0)
		return -1;

	status = btnodetick(node->guard, bs, agent);
	switch(status){
	case TASKSUCCESS:
		return 0;
	case TASKFAIL:
		return -1;
	}

	abort();
	return 0;
}

static int
tickleaf(BehaviorNode *node, BehaviorState *bs, void *agent)
{
	BehaviorLeaf *leaf;

	leaf = (BehaviorLeaf*)node;

	return leaf->action(agent);
}

static int
ticksequence(BehaviorNode *node, BehaviorState *bs, void *agent)
{
	int status;
	BehaviorBranch *branch;
	BehaviorBranchState *state;
	BehaviorNode *child;

	branch = (BehaviorBranch*)node;
	state = btstatefor(node, bs);

	child = branch->children[state->childcurrent];
	status = btnodetick(child, bs, agent);

	switch(status){
	case TASKRUNNING:
		return TASKRUNNING;
	case TASKFAIL:
		state->childcurrent = 0;
		return TASKFAIL;
	case TASKSUCCESS:
		if(state->childcurrent == (branch->childcount - 1)){
			state->childcurrent = 0;
			return TASKSUCCESS;
		}

		state->childcurrent++;
		return TASKRUNNING;
	}

	abort();
	return 0;
}

static int
tickpriority(BehaviorNode *node, BehaviorState *bs, void *agent)
{
	int status;
	BehaviorBranch *branch;
	BehaviorBranchState *state;
	BehaviorNode *child;

	branch = (BehaviorBranch*)node;
	state = btstatefor(node, bs);

	child = branch->children[state->childcurrent];
	status = btnodetick(child, bs, agent);

	switch(status){
	case TASKSUCCESS:
		state->childcurrent = 0;
		return TASKSUCCESS;
	case TASKRUNNING:
		return TASKRUNNING;
	case TASKFAIL:
		if(state->childcurrent == (branch->childcount - 1)){
			state->childcurrent = 0;
			return TASKFAIL;
		}

		state->childcurrent++;
		return TASKRUNNING;
	}

	abort();
	return 0;
}

static int
tickparallel(BehaviorNode *node, BehaviorState *bs, void *agent)
{
	int i, state, success, failure;
	BehaviorParallel *parallel;
	BehaviorNode *child;

	success = failure = 0;

	parallel = (BehaviorParallel*)node;

	for(i = 0; i < parallel->branch.childcount; i++){
		child = parallel->branch.children[i];

		state = TASKFAIL;
		if(btcheckguard(child, bs, agent) >= 0)
			state = btnodetick(child, bs, agent);

		switch(state){
		case TASKSUCCESS:
			success++;
			break;
		case TASKFAIL:
			failure++;
			break;
		case TASKRUNNING:
			break;
		default:
			abort();
		}

		if(success >= parallel->S){
			btcancel((BehaviorNode*)parallel, bs, agent);
			return TASKSUCCESS;
		}

		if(failure >= parallel->F){
			btcancel((BehaviorNode*)parallel, bs, agent);
			return TASKFAIL;
		}
	}

	return TASKRUNNING;
}

static int
tickdynguard(BehaviorNode *node, BehaviorState *bs, void *agent)
{
	int i;
	BehaviorBranch *branch;
	BehaviorBranchState *state;
	BehaviorNode *child, *torun;

	branch = (BehaviorBranch*)node;
	state = btstatefor(node, bs);
	torun = nil;

	for(i = 0; i < branch->childcount; i++){
		child = branch->children[i];
		if(btcheckguard(child, bs, agent) == 0){
			torun = child;
			break;
		}
	}

	if(state->childcurrent != -1 && torun != branch->children[state->childcurrent]){
		btcancel(branch->children[state->childcurrent], bs, agent);
		state->childcurrent = -1;
	}

	if(torun == nil){
		btcancel((BehaviorNode*)branch, bs, agent);
		return TASKFAIL;
	}

	if(state->childcurrent == -1){
		state->childcurrent = i;
	}

	i = btnodetick(torun, bs, agent);
	switch(i){
	case TASKSUCCESS:
	case TASKFAIL:
		state->childcurrent = -1;
		btcancel((BehaviorNode*)branch, bs, agent);
	}

	return i;
}

static int
tickinvert(BehaviorNode *node, BehaviorState *bs, void *agent)
{
	int state;
	BehaviorBranch *branch;
	BehaviorNode *child;

	branch = (BehaviorBranch*)node;
	child = branch->children[0];

	state = btnodetick(child, bs, agent);

	switch(state){
	case TASKSUCCESS:
		return TASKFAIL;
	case TASKFAIL:
		return TASKSUCCESS;
	case TASKRUNNING:
		return TASKRUNNING;
	}

	abort();
	return 0;
}

static int (*tickfns[])(BehaviorNode*, BehaviorState*, void*) = {
[BT_LEAF]		tickleaf,
[BT_SEQUENCE]	ticksequence,
[BT_PRIORITY]	tickpriority,
[BT_PARALLEL]	tickparallel,
[BT_DYNGUARD]	tickdynguard,
[BT_INVERT]		tickinvert,
};

static int
btnodetick(BehaviorNode *node, BehaviorState *bs, void *agent)
{
	BehaviorNodeState *state;

	assert(node != nil);
	assert(node->type >= 0 && node->type < BT_MAXTYPE);
	assert(node->type < nelem(tickfns));
	assert(bs != nil);

	state = btstatefor(node, bs);

	state->status = tickfns[node->type](node, bs, agent);

	if(node->end != nil && (state->status == TASKFAIL || state->status == TASKSUCCESS))
		node->end(agent);

	return state->status;
}

int
bttick(Behavior *b, BehaviorState *bs, void *agent)
{
	return btnodetick(b->root, bs, agent);
}

