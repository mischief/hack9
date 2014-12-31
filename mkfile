</$objtype/mkfile

TARG=hack9

HFILES=\
	dat.h\
	alg.h\

OFILES=\
	hack9.$O\
	tile.$O\
	camera.$O\
	level.$O\
	path.$O\
	alg.$O\
	monst.$O\
	monstdata.$O\

CLEANFILES=$O.alg

</sys/src/cmd/mkone

$O.alg:	algtest.$O alg.$O
	$LD -o $target $prereq

