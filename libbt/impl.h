enum {
	BT_LEAF	= 0,
	BT_SEQUENCE,
	BT_PRIORITY,
	BT_PARALLEL,
	BT_DYNGUARD,
	BT_INVERT,
	BT_MAXTYPE,
};

void btcancel(BehaviorNode*, BehaviorState*, void*);
void *btstatefor(BehaviorNode*, BehaviorState*);

struct BehaviorNode {
	unsigned char id;
	unsigned char type;
	short ref;
	BehaviorNode *guard;
	void (*end)(void*);
	char name[16];
};

typedef struct BehaviorNodeState BehaviorNodeState;
struct BehaviorNodeState {
	int status;
};

typedef struct BehaviorLeaf BehaviorLeaf;
struct BehaviorLeaf {
	BehaviorNode node;
	BehaviorAction action;
};

typedef struct BehaviorLeafState BehaviorLeafState;
struct BehaviorLeafState {
	BehaviorNodeState node;
};

typedef struct BehaviorBranch BehaviorBranch;
struct BehaviorBranch {
	BehaviorNode node;

	int childcount;
	BehaviorNode **children;	
};

typedef struct BehaviorBranchState BehaviorBranchState;
struct BehaviorBranchState {
	BehaviorNodeState node;
	int childcurrent;
};

typedef struct BehaviorParallel BehaviorParallel;
struct BehaviorParallel {
	BehaviorBranch branch;

	int S;
	int F;
};

/* unused currently */
/*
typedef struct BehaviorRepeat BehaviorRepeat;
struct BehaviorRepeat {
	BehaviorBranch;

	int cur;
	int max;
};
*/

typedef struct BehaviorState BehaviorState;
struct BehaviorState {
	Behavior *behavior;
	union {
		/* BT_LEAF */
		BehaviorLeafState leaf;
		/* BT_SEQUENCE, BT_PRIORITY, BT_PARALLEL
		 * BT_DYNGUARD, BT_INVERT */
		BehaviorBranchState branch;
	} states[];
};

typedef struct Behavior Behavior;
struct Behavior {
	BehaviorNode *root;
	unsigned char nstates;
};

