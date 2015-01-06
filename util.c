#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "dat.h"

Point cardinals[] = {
[WEST]	{ -1,  0 },
[SOUTH]	{  0,  1 },
[NORTH]	{  0, -1 },
[EAST]	{  1,  0 },
[NODIR]	{  0,  0 },
};

int
roll(int count, int sides)
{
	int d;
	d = 0;
	while(count--> 0)
		d += 1+nrand(sides);
	return d;
}

int
min(int a, int b)
{
	if(a < b)
		return a;
	return b;
}

int
max(int a, int b)
{
	if(a > b)
		return a;
	return b;
}

