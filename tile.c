#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>

#include "dat.h"

Tileset*
opentile(char *name, int width, int height)
{
	Tileset *t;
	int fd;

	if(name == nil || width < 1 || height < 1){
		werrstr("bad tileset: %s %d %d", name, width, height);
		return nil;
	}

	t = mallocz(sizeof(Tileset), 1);
	if(t == nil)
		return nil;

	t->width = width;
	t->height = height;
	t->sz = Pt(width, height);

	if(strrchr(name, '/') != nil)
		t->name = strdup(strrchr(name, '/')+1);
	else
		t->name = strdup(name);

	fd = open(name, OREAD);
	if(fd < 0)
		goto err;

	t->image = readimage(display, fd, 0);
	if(t->image == nil)
		goto err;

	close(fd);
	return t;

err:
	free(t->name);
	if(fd != -1)
		close(fd);
	free(t);
	return nil;
}

void
freetile(Tileset *t)
{
	free(t->name);
	freeimage(t->image);
	free(t);
}

void
drawtile(Tileset *t, Image *dst, Point p, int i)
{
	Rectangle dstr;
	Point spt;
	int wid;

	wid = Dx(t->image->r) / t->width;

	dstr.min = p;
	dstr.max = addpt(p, t->sz);

	spt.x = (i % wid) * t->width;
	spt.y = (i / wid) * t->height;

	draw(dst, dstr, t->image, nil, spt);
}
