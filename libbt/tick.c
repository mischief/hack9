#include <u.h>
#include <libc.h>

#include "bt.h"
#include "impl.h"

/* -1 on failure, >= 0 on success */
static int
btcheckguard(BehaviorNode *node, void *agent)
{
	int status;

	if(node->guard == nil)
		return 0;

	/* check the guard of the guard */
	if(btcheckguard(node->guard, agent) < 0)
		return -1;

	status = bttick(node->guard, agent);
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
tickleaf(BehaviorNode *node, void *agent)
{
	BehaviorLeaf *leaf;

	leaf = (BehaviorLeaf*)node;

	leaf->status = leaf->action(agent);

	return leaf->status;
}

static int
ticksequence(BehaviorNode *node, void *agent)
{
	int state;
	BehaviorBranch *branch;
	BehaviorNode *child;

	branch = (BehaviorBranch*)node;

	child = branch->children[branch->childcurrent];
	state = bttick(child, agent);

	switch(state){
	case TASKRUNNING:
		return TASKRUNNING;
	case TASKFAIL:
		branch->childcurrent = 0;
		return TASKFAIL;
	case TASKSUCCESS:
		if(branch->childcurrent == (branch->childcount - 1)){
			branch->childcurrent = 0;
			return TASKSUCCESS;
		}

		branch->childcurrent++;
		return TASKRUNNING;
	}

	abort();
	return 0;
}

static int
tickpriority(BehaviorNode *node, void *agent)
{
	int state;
	BehaviorBranch *branch;
	BehaviorNode *child;

	branch = (BehaviorBranch*)node;

	child = branch->children[branch->childcurrent];
	state = bttick(child, agent);

	switch(state){
	case TASKSUCCESS:
		branch->childcurrent = 0;
		return TASKSUCCESS;
	case TASKRUNNING:
		return TASKRUNNING;
	case TASKFAIL:
		if(branch->childcurrent == (branch->childcount - 1)){
			branch->childcurrent = 0;
			return TASKFAIL;
		}

		branch->childcurrent++;
		return TASKRUNNING;
	}

	abort();
	return 0;
}

static int
tickparallel(BehaviorNode *node, void *agent)
{
	int i, state, success, failure;
	BehaviorParallel *parallel;
	BehaviorNode *child;

	success = failure = 0;

	parallel = (BehaviorParallel*)node;

	for(i = 0; i < parallel->childcount; i++){
		child = parallel->children[i];

		state = TASKFAIL;
		if(btcheckguard(child, agent) >= 0)
			state = bttick(child, agent);

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
			btcancel(parallel, agent);
			return TASKSUCCESS;
		}

		if(failure >= parallel->F){
			btcancel(parallel, agent);
			return TASKFAIL;
		}
	}

	return TASKRUNNING;
}

static int
tickdynguard(BehaviorNode *node, void *agent)
{
	int i;
	BehaviorBranch *branch;
	BehaviorNode *child, *torun;

	branch = (BehaviorBranch*)node;
	torun = nil;

	for(i = 0; i < branch->childcount; i++){
		child = branch->children[i];
		if(btcheckguard(child, agent) == 0){
			torun = child;
			break;
		}
	}

	if(branch->childcurrent != -1 && torun != branch->children[branch->childcurrent]){
		btcancel(branch->children[branch->childcurrent], agent);
		branch->childcurrent = -1;
	}

	if(torun == nil){
		btcancel(branch, agent);
		return TASKFAIL;
	}

	if(branch->childcurrent == -1){
		branch->childcurrent = i;
	}

	node->status = bttick(torun, agent);
	switch(node->status){
	case TASKSUCCESS:
	case TASKFAIL:
		branch->childcurrent = -1;
		btcancel(branch, agent);
	}

	return node->status;
}

static int
tickinvert(BehaviorNode *node, void *agent)
{
	int state;
	BehaviorBranch *branch;
	BehaviorNode *child;

	branch = (BehaviorBranch*)node;
	child = branch->children[0];

	state = bttick(child, agent);

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

static int (*tickfns[])(BehaviorNode*, void*) = {
[BT_LEAF]		tickleaf,
[BT_SEQUENCE]	ticksequence,
[BT_PRIORITY]	tickpriority,
[BT_PARALLEL]	tickparallel,
[BT_DYNGUARD]	tickdynguard,
[BT_INVERT]		tickinvert,
};

int
bttick(BehaviorNode *node, void *agent)
{
	assert(node->type >= 0 && node->type < BT_MAXTYPE);
	assert(node->type < nelem(tickfns));

	node->status = tickfns[node->type](node, agent);

	if(node->end != nil && (node->status == TASKFAIL || node->status == TASKSUCCESS))
		node->end(agent);

	return node->status;
}
