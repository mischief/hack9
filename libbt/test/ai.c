#include <u.h>
#include <libc.h>
#include <draw.h>

#include <bt.h>

typedef struct Agent Agent;
struct Agent {
	Point pos;
	Point target;
};

int
rstate(void)
{
	return nrand(TASKSUCCESS)+1;
}

int
pstate(int state)
{
	print(" -> %s\n", btstatenames[state]);
	return state;
}

/* find a random place to goto */
int
wpfind(void *a)
{
	Agent *agent = a;

	print("wander find");
	agent->target = Pt(nrand(11), nrand(11));
	return pstate(TASKSUCCESS);
}

/* find an enemy */
int
afind(void *a)
{
	Agent *agent = a;

	print("find");

	if(nrand(8) == 0){
		agent->target = Pt(nrand(11), nrand(11));
		return pstate(TASKSUCCESS);
	}

	return pstate(TASKFAIL);
}

/* move towards the enemy */
int
amove(void *a)
{
	Agent *agent = a;

	print("move");

	if((abs(agent->pos.x - agent->target.x) + abs(agent->pos.y - agent->target.y)) < 2)
		return pstate(TASKSUCCESS);

	if(agent->target.x > agent->pos.x){
		agent->pos.x++;
		return pstate(TASKRUNNING);
	}

	if(agent->target.x < agent->pos.x){
		agent->pos.x--;
		return pstate(TASKRUNNING);
	}

	if(agent->target.y > agent->pos.y){
		agent->pos.y++;
		return pstate(TASKRUNNING);
	}

	if(agent->target.y < agent->pos.y){
		agent->pos.y--;
		return pstate(TASKRUNNING);
	}

	return pstate(TASKFAIL);
}

/* simulate hitting an enemy */
int
ahit(void *a)
{
	print("hit");

	if(nrand(4) == 0)
		return pstate(TASKSUCCESS);
	return pstate(TASKRUNNING);
}

BehaviorNode*
mkbt(void)
{
	BehaviorNode *find, *move, *hit;
	BehaviorNode *wfind, *wmove, *wander;
	BehaviorNode *attack, *idle;
	BehaviorNode *root;

	find = btleaf("find enemy", afind);
	move = btleaf("move to target", amove);

	/* die yankee pig dog */
	hit = btleaf("hit enemy", ahit);

	/* combat ai is a simple search and destroy */
	attack = btsequence("attack enemy", find, move, hit, nil);

	wfind = btleaf("wander find", wpfind);
	wmove = btleaf("wander move", amove);
	wander = btsequence("wander", wfind, wmove, nil);

	/* find is added here so control will pop out of
	 * wander when an enemy is found
	 */

	idle = btparallel("wander", 1, 2, find, wander, nil);

	root = btpriority("root", attack, idle, nil);
	return root;
}

void
test_bt(void)
{
	int i, state;
	Agent a;
	BehaviorNode *root;
	Behavior *b;
	BehaviorState *bs;

	a.pos = Pt(0, 0);
	a.target = Pt(-1, -1);

	root = mkbt();
	b = btroot(root);
	bs = btstatenew(b);

	for(i = 0; i < 100; i++){
		print("======== TICK ========\n");
		print("pos=%P target=%P %T\n", a.pos, a.target, root);
		state = bttick(b, bs, &a);
		print(" -> %s\n", btstatenames[state]);
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

	fmtinstall('P', Pfmt);
	fmtinstall('T', btfmt);

	alarm(5000);

	srand(truerand());

	test_bt();

	exits(nil);
}
