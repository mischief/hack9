</$objtype/mkfile

BIN=/$objtype/bin/games

<mk.common

CFLAGS=$CFLAGS -Ilibmap -Ilibbt

LIBMAP=libmap/libmap.$O.a
LIBBT=libbt/libbt.$O.a
LIB=$LIBMAP $LIBBT

</sys/src/cmd/mkone

$LIBMAP:
	cd libmap
	mk

$LIBBT:
	cd libbt
	mk

clean:V:
	@{ cd libmap; mk clean }
	@{ cd libbt; mk clean }
	rm -f *.[$OS] [$OS].out *.acid $TARG

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
