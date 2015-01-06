</$objtype/mkfile

BIN=$home/bin/$objtype

<mk.common

</sys/src/cmd/mkone

install:V:	$BIN/$TARG
	cp nethack.32x32 $home/lib/

$O.alg:	algtest.$O alg.$O
	$LD -o $target $prereq

