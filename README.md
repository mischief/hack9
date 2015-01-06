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
* apply these ([1](https://plan9port-review.googlesource.com/#/c/1150/) [2](https://plan9port-review.googlesource.com/#/c/1151/)) patches:
```
#!sh
cd $PLAN9
curl -s https://plan9port-review.googlesource.com/changes/1150/revisions/1ed3265c399902edd85b1d110211a5f3fdf1d640/patch?download | base64 -d | patch -p1
curl -s https://plan9port-review.googlesource.com/changes/1151/revisions/9a34834c8d0d34ba4524a0b673629830753a2890/patch?download | base64 -d | patch -p1
```
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

