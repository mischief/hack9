</$objtype/mkfile

BIN=/$objtype/bin/games

<mk.common

</sys/src/cmd/mkone

install:V:	$BIN/$TARG
	mkdir -p /lib/hack9
	cp nethack.32x32 /lib/hack9
	cp monster.ndb /lib/hack9
	cp item.ndb /lib/hack9

uninstall:V:
	rm -r $BIN/$TARG
	rm -r /lib/hack9

$O.alg:	algtest.$O alg.$O
	$LD -o $target $prereq
