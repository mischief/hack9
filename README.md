# hack9 #

## running ##

### plan9 ###
* run:

```
#!rc

cd
hg clone https://bitbucket.org/mischief/hack9 hack9
cd hack9
mk
8.out
```

### unix ###
* install [plan9port](https://github.com/9fans/plan9port).
* make sure `$PLAN9` is set.
* make sure `$PLAN9` is in `$PATH`.
* run:
```
#!sh

cd
hg clone https://bitbucket.org/mischief/hack9 hack9
cd hack9
mk -f mk.p9p
o.hack9
```

