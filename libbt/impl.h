enum {
	BT_LEAF	= 0,
	BT_SEQUENCE,
	BT_PRIORITY,
	BT_PARALLEL,
	BT_DYNGUARD,
	BT_INVERT,
	BT_MAXTYPE,
};

struct BehaviorNode {
	short type;
	short ref;
	int status;
	BehaviorNode *guard;
	char name[16];
};

typedef struct BehaviorLeaf BehaviorLeaf;
struct BehaviorLeaf {
	BehaviorNode;
	BehaviorAction action;
};

typedef struct BehaviorBranch BehaviorBranch;
struct BehaviorBranch {
	BehaviorNode;

	int childcurrent;
	int childcount;
	BehaviorNode **children;	
};

typedef struct BehaviorParallel BehaviorParallel;
struct BehaviorParallel {
	BehaviorBranch;

	int S;
	int F;
};

typedef struct BehaviorRepeat BehaviorRepeat;
struct BehaviorRepeat {
	BehaviorBranch;

	int cur;
	int max;
};
