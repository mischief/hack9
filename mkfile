</$objtype/mkfile

TARG=hack9

HFILES=\
	dat.h\
	alg.h\

OFILES=\
	alg.$O\
	camera.$O\
	hack9.$O\
	level.$O\
	monst.$O\
	monstdata.$O\
	path.$O\
	tile.$O\
	util.$O\

CLEANFILES=$O.alg

</sys/src/cmd/mkone

$O.alg:	algtest.$O alg.$O
	$LD -o $target $prereq

