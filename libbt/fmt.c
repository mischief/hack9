#include <u.h>
#include <libc.h>

#include "bt.h"
#include "impl.h"

char *btstatenames[TASKMAX] = {
[TASKFRESH]		"fresh",
[TASKRUNNING]	"running",
[TASKFAIL]		"fail",
[TASKSUCCESS]	"success",
};

int
btfmt(Fmt *f)
{
	int n;
	Rune c;
	char *s, id;
	BehaviorNode *node;
	BehaviorBranch *branch;
	node = va_arg(f->args, BehaviorNode*);

	fmtprint(f, "%s", node->name);
	return 0;

	/*
	for(n = 0; node != nil; n++){
		c = L'×';
		id = node->id;
		s = node->name;
		switch(node->type){
		case BT_LEAF:
			c = L'•';
			node = nil;
			break;
		case BT_SEQUENCE:
			c = L'→';
			branch = (BehaviorBranch*)node;
			node = branch->children[branch->childcurrent];
			fmtprint(f, "(%d/%d)", branch->childcurrent, branch->childcount);
			break;
		case BT_PRIORITY:
			c = '?';
			branch = (BehaviorBranch*)node;
			node = branch->children[branch->childcurrent];
			fmtprint(f, "(%d/%d)", branch->childcurrent, branch->childcount);
			break;
		case BT_PARALLEL:
			c = L'⇒';
			branch = (BehaviorBranch*)node;
			node = branch->children[branch->childcurrent];
			fmtprint(f, "(%d/%d)", branch->childcurrent, branch->childcount);
			break;
		case BT_DYNGUARD:
			c = L'∈';
			branch = (BehaviorBranch*)node;
			if(branch->childcurrent != -1)
				node = branch->children[branch->childcurrent];
			else
				node = nil;
			break;
		case BT_INVERT:
			c = L'¬';
			branch = (BehaviorBranch*)node;
			node = branch->children[branch->childcurrent];
			break;
		default:
			sysfatal("weird node type %d '%s'\n", node->type, node->name);
		}

		fmtprint(f, "%s(%C%s %hhud)", (n!=0)?" ":"", c, s, id);
	}

	return 0;
	*/
}
