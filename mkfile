</$objtype/mkfile

BIN=/$objtype/bin/games
TARG=hack9
HFILES=\
	dat.h\
	alg.h\

OFILES=\
	ai.$O\
	alg.$O\
	hack9.$O\
	item.$O\
	level.$O\
	levelgen.$O\
	monst.$O\
	path.$O\
	tile.$O\
	ui.$O\
	util.$O\

CFLAGS=$CFLAGS -Ilibmap

LIBMAP=libmap/libmap.$O.a
LIB=$LIBMAP

</sys/src/cmd/mkone

$LIBMAP:
	cd libmap
	mk

clean:V:
	@{ cd libmap; mk clean }
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
