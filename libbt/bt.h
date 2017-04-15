enum {
	TASKFRESH	= 0,
	TASKRUNNING,
	TASKFAIL,
	TASKSUCCESS,
	TASKMAX,
};

extern char *btstatenames[TASKMAX];

typedef int (*BehaviorAction)(void*);

typedef struct BehaviorNode BehaviorNode;
#pragma incomplete BehaviorNode

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

void btcancel(BehaviorNode*, void*);
void btfree(BehaviorNode*, void*);
int bttick(BehaviorNode*, void*);

/* %T */
int btfmt(Fmt *f);
