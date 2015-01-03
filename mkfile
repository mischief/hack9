</$objtype/mkfile

BIN=$home/bin/$objtype
TARG=hack9

HFILES=\
	dat.h\
	alg.h\

OFILES=\
	ai.$O\
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

install:V:	$BIN/$TARG
	cp nethack.32x32 $home/lib/

$O.alg:	algtest.$O alg.$O
	$LD -o $target $prereq

