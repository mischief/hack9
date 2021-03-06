#include <u.h>
#include <libc.h>

#include <bt.h>

typedef struct Agent Agent;
struct Agent {
	int hunger;
	int food;
};

int rstate(void)
{
	int state = nrand(TASKSUCCESS)+1;
	print("\t%-20s\n", btstatenames[state]);
	return state;
}

int
pstate(int state)
{
	print("\t%-20s\n", btstatenames[state]);
	return state;
}

int
findfoodnearby(void *a)
{
	int r;
	Agent *agent = a;

	print("findfood");

	r = nrand(5);
	if(r == 0)
		return pstate(TASKSUCCESS);

	if(r == 1)
		return pstate(TASKFAIL);

	return pstate(TASKRUNNING);
}

int movetofood(void *a) { print("movetofood"); return rstate(); }

int
pickupfood(void *a)
{
	Agent *agent = a;

	print("pickupfood");

	agent->food += nrand(3)+1;
	return pstate(TASKSUCCESS);
}

int goout(void *a) { print("goout"); return rstate(); }

int
eatfood(void *a)
{
	Agent *agent = a;

	print("eatfood");

	if(agent->food > 0){
		agent->food--;
		agent->hunger--;

		if(agent->food == 0)
			return pstate(TASKSUCCESS);
		return pstate(TASKRUNNING);
	}

	return pstate(TASKFAIL);
}

BehaviorNode*
mkbt(void)
{
	BehaviorNode *find, *moveto, *pickup, *gout, *eat;
	BehaviorNode *bnear, *get, *root;

	find = btleaf("find food", findfoodnearby);
	moveto = btleaf("move to food", movetofood);
	pickup = btleaf("pickup food", pickupfood);
	gout = btleaf("go out", goout);
	eat = btleaf("eat food", eatfood);

	bnear = btsequence("nearby food", find, moveto, pickup, nil);
	get = btpriority("get food", bnear, gout, nil);
	root = btsequence("root", get, eat, nil);

	return root;
}

void
test_bt(void)
{
	int i;
	Agent a;
	BehaviorNode *root;
	Behavior *b;
	BehaviorState *bs;

	a.hunger = 10;
	a.food = 0;

	root = mkbt();
	b = btroot(root);
	bs = btstatenew(b);

	while(a.hunger > 0){
		bttick(b, bs, &a);
		print("hunger=%d food=%d\n", a.hunger, a.food);
		sleep(10);
	}

	btstatefree(bs, &a);
	btfree(b);
}

void
usage(void)
{
	fprint(2, "usage: %s\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	ARGBEGIN{
	default:
		usage();
	}ARGEND

	alarm(5000);

	srand(truerand());

	test_bt();

	exits(nil);
}
