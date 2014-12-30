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
	Monster *monst;
	/*
	Feature *feat;
	Item *items[10];
	Engraving *engrav;
	*/

	/* does this block movement? */
	int blocked;
	
	/* the unit on the tile */
	int unit;

	/* feature if present */
	int feat;

	/* index into tileset */
	int terrain;
};

enum
{
	LWIDTH	= 200,
	LHEIGHT	= 100,
};

typedef struct Level Level;
struct Level
{
	int width;
	int height;
	Tile *tiles;

	Point up;
	Point down;
};

Level *genlevel(int width, int height);
void freelevel(Level *l);
Tile *tileat(Level *l, Point p);

typedef struct Camera Camera;
struct Camera
{
	Point pos;
	Rectangle size;
	Rectangle box;
};

void ccenter(Camera *c, Point p);
Point ctrans(Camera *c, Point p);

