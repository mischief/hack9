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
		d += 1+lnrand(sides);
	return d;
}

int
parseroll(char *str, int *count, int *sides)
{
	int i;
	char *f[2];

	if(str == nil || count == nil || sides == nil)
		return 0;

	i = gettokens(str, f, 2, "d");
	if(i != 2)
		return 0;

	*count = atoi(f[0]);
	*sides = atoi(f[1]);
	return 1;
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

