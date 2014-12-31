typedef struct Tileset Tileset;
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

Tileset *opentile(char *name, int width, int height);
void freetile(Tileset *t);

/* draw i'th tile onto dst */
void drawtile(Tileset *t, Image *dst, Point p, int i);

typedef struct MonsterData MonsterData;
struct MonsterData
{
	char *name;
	uint maxhp;
	uint atk;
};

extern MonsterData monstdata[392];

typedef struct Monster Monster;
struct Monster
{
	int hp;

	/* doesn't need to be free'd */
	MonsterData *md;
};

Monster *monst(int idx);

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

typedef struct Tile Tile;
struct Tile
{
	/* current monster occupying tile */
	Monster *monst;
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
};

typedef struct Level Level;
struct Level
{
	int width;
	int height;
	Tile *tiles;
	int *flags;

	Point up;
	Point down;
};

Level *genlevel(int width, int height);
void freelevel(Level *l);
Tile *tileat(Level *l, Point p);
//#define tileat(l, p) (l->tiles+(p.y*l->width)+p.x)
#define flagat(l, p) (*(l->flags+(p.y*l->width)+p.x))
#define hasflagat(l, p, F) (*(l->flags+(p.y*l->width)+p.x) & (F))
#define setflagat(l, p, F) (*(l->flags+(p.y*l->width)+p.x) |= (F))
#define clrflagat(l, p, F) (*(l->flags+(p.y*l->width)+p.x) &= ~(F))

typedef struct Camera Camera;
struct Camera
{
	Point pos;
	Rectangle size;
	Rectangle box;
};

void ccenter(Camera *c, Point p);
Point ctrans(Camera *c, Point p);

