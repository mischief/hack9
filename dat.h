typedef struct Tileset Tileset;
typedef struct Tile Tile;
typedef struct Portal Portal;
typedef struct Level Level;
typedef struct MonsterData MonsterData;
typedef struct Monster Monster;
typedef struct Camera Camera;

/* hack9.c */
void msg(char *fmt, ...);
void good(char *fmt, ...);
void warn(char *fmt, ...);
void dbg(char *fmt, ...);

struct Tileset
{
	/* of each tile */
	int width;
	int height;
	Point sz;

	/* from whence we came */
	char *name;

	/* actual graphics */
	Image *image;
};

/* tile.c */
Tileset *opentile(char *name, int width, int height);
void freetile(Tileset *t);

/* draw i'th tile onto dst */
void drawtile(Tileset *t, Image *dst, Point p, int i);

struct MonsterData
{
	char *name;
	uint maxhp;
	uint atk;
	uint rolls;
};

extern MonsterData monstdata[392];

/* possible moves */
enum
{
	MNONE,
	MMOVE,	/* also attack */
	MUSE,
};

struct Monster
{
	/* current level */
	Level *l;
	/* location */
	Point pt;

	/* alignment */
	char align;

	/* current hp */
	long hp;

	/* no free */
	MonsterData *md;
};

Monster *monst(int idx);
int maction(Monster *m, int what, Point where);

/* well known tiles, also indexes into monstdata */
enum
{
	/* monsters */
	TLARGECAT	= 39,
	TGWIZARD	= 168,
	TSOLDIER	= 280,
	TSERGEANT	= 281,
	TLIEUTENANT	= 282,
	TCAPTAIN	= 283,
	TWIZARD		= 349,
	/* features */
	TWALL		= 840,
	TTREE		= 847,
	TFLOOR		= 848,
	TUPSTAIR	= 851,
	TDOWNSTAIR	= 852,
};

struct Portal
{
	/* where it takes us to */
	Level *to;
	/* location in to */
	Point pt;
};

struct Tile
{
	/* current monster occupying tile */
	Monster *monst;

	/* has a portal */
	Portal *portal;
	/*
	Feature *feat;
	Item *items[10];
	Engraving *engrav;
	*/

	/* the unit on the tile */
	int unit;

	/* feature if present */
	int feat;

	/* index into tileset */
	int terrain;

	int flags;
};

enum
{
	/* tile flags */
	Fhasobject	= 0x1,
	Fhasmonster	= 0x2,
	Fhasitem	= 0x4,
	Fhasfeature	= 0x8,
	Fblocked	= 0x10,
	Fportal		= 0x20,
};

struct Level
{
	int width;
	int height;
	Tile *tiles;
	int *flags;

	Rectangle r;
	Point up;
	Point down;
};

Level *genlevel(int width, int height);
void freelevel(Level *l);
Tile *tileat(Level *l, Point p);
#define flagat(l, p) (*(l->flags+(p.y*l->width)+p.x))
#define hasflagat(l, p, F) (*(l->flags+(p.y*l->width)+p.x) & (F))
#define setflagat(l, p, F) (*(l->flags+(p.y*l->width)+p.x) |= (F))
#define clrflagat(l, p, F) (*(l->flags+(p.y*l->width)+p.x) &= ~(F))
Point *lneighbor(Level *l, Point p, int *n);

struct Camera
{
	Point pos;
	Rectangle size;
	Rectangle box;
};

/* camera.c */
void ccenter(Camera *c, Point p);
Point ctrans(Camera *c, Point p);

enum {
	/* cost to move N S E W */
	ORTHOCOST	= 10,
};

/* path.c */
int manhattan(Point cur, Point targ);
int pathfind(Level *l, Point start, Point end, Point **path);

/* util.c */
enum
{
	NORTH,
	SOUTH,
	EAST,
	WEST,
	NCARDINAL,
	NODIR,
};

extern Point cardinals[];
int roll(int count, int sides);
int min(int a, int b);
int max(int a, int b);

