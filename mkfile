</$objtype/mkfile

BIN=$home/bin/$objtype

<mk.common

</sys/src/cmd/mkone

install:V:	$BIN/$TARG
	mkdir $home/lib/hack9
	cp nethack.32x32 $home/lib/hack9
	cp monster.ndb $home/lib/hack9
	cp item.ndb $home/lib/hack9

$O.alg:	algtest.$O alg.$O
	$LD -o $target $prereq

