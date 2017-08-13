#include <u.h>
#include <libc.h>
#include <bio.h>

#include <bt.h>

static unsigned long
memuse(void)
{
	unsigned long b;

	b = 0;
#ifdef __linux__
	int fd = open("/proc/self/status", OREAD);
	if(fd == -1)
		return 0;

	Biobuf rb;
	if(Binit(&rb, fd, OREAD) != 0){
		close(fd);
		return 0;
	}

	char *line;

	while((line = Brdstr(&rb, '\n', 1)) != nil){
		if(strncmp(line, "VmRSS:", 6) == 0){
			/* in kB */
			b = strtoul(line+6, nil, 10);
			b *= 1000;
			free(line);
			break;
		}
		free(line);
	}

	Bterm(&rb);
	close(fd);
#endif

	return b;
}

static int
nop(void *a)
{
}

BehaviorNode*
mkbt(void)
{
	BehaviorNode *one, *two, *three;
	BehaviorNode *root;

	one = btleaf("one", nop);
	two = btleaf("two", nop);
	three = btleaf("three", nop);

	root = btsequence("root", one, two, three, nil);

	return root;
}

void
main(int argc, char *argv[])
{
	int n;

	ARGBEGIN{
	default:
		sysfatal("no argumnts");
	}ARGEND

	n = 100000;

	while(n-- > 0)
		mkbt();

	print("%lud\n", memuse());

	exits(nil);
}
