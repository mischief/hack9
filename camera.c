#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "dat.h"

void
ccenter(Camera *c, Point p)
{
	Point pos;

	/* center */
	pos = subpt(p, divpt(Pt(Dx(c->size), Dy(c->size)), 2));

	c->pos = p;
	c->box = Rpt(pos, addpt(pos, c->size.max));
}

Point
ctrans(Camera *c, Point p)
{
	return subpt(p, c->box.min);
}

