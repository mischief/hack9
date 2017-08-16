enum {
	TASKFRESH	= 0,
	TASKRUNNING,
	TASKFAIL,
	TASKSUCCESS,
	TASKMAX,
};

extern char *btstatenames[TASKMAX];

typedef int (*BehaviorAction)(void*);

typedef struct Behavior Behavior;
#pragma incomplete Behavior

typedef struct BehaviorState BehaviorState;
#pragma incomplete Behavior

typedef struct BehaviorNode BehaviorNode;
#pragma incomplete BehaviorNode

Behavior *btnew();
Behavior *btroot(BehaviorNode *root);
BehaviorState *btstatenew(Behavior*);
void btstatefree(BehaviorState*, void *agent);

/* leaf action */
BehaviorNode *btleaf(char*, BehaviorAction);

/* control flow */
BehaviorNode *btsequence(char*, ...);
BehaviorNode *btpriority(char*, ...);
BehaviorNode *btparallel(char*, int, int, ...);
BehaviorNode *btdynguard(char*, ...);

/* decorators */
BehaviorNode *btinvert(char*, BehaviorNode*);

/* set guard function for node */
void btsetguard(BehaviorNode*, BehaviorNode*);

/* set end function for a node */
void btsetend(BehaviorNode*, void (*)(void*));

void btfree(Behavior*);
int bttick(Behavior*, BehaviorState*, void*);

/* %T */
int btfmt(Fmt *f);

