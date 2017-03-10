typedef struct AIState AIState;
typedef struct EquipData EquipData;
typedef struct ItemData ItemData;
typedef struct Item Item;
typedef struct ItemList ItemList;
typedef struct Tileset Tileset;
typedef struct Tile Tile;
typedef struct Portal Portal;
typedef struct Spawn Spawn;
typedef struct Level Level;
typedef struct MonsterData MonsterData;
typedef struct Monster Monster;

/* sizes */
enum
{
	/* name of a monster, item, itemlist */
	SZNAME = 64,
};

/* directions. ordering matters to drunk1() */
enum
{
	WEST	= 0,
	SOUTH,
	EAST,
	NORTH,
	NCARDINAL,
	NODIR,
};

/* hack9.c */
extern int debug;
extern int farmsg;
extern Monster *player;
extern char *user;
extern char *home;
extern long turn;
int isyou(Monster *m);
int nearyou(Point p);

typedef void (*AIFun)(AIState *);

struct AIState
{
	AIState *prev;
	char name[16];
	Monster *m;
	void *aux;
	AIFun enter;
	AIFun exec;
	AIFun exit;
};

/* ai.c */
AIState *mkstate(char *name, Monster *m, void *aux, AIFun enter, AIFun exec, AIFun exit);
void freestate(AIState *a);
void idle(Monster *m);
AIState* wander(Monster *m);
AIState* walkto(Monster *m, Point p, int wait);
AIState* attack(Monster *m, Monster *targ);

enum
{
	/* possible item types */
	IWEAPON	= 0,
	IHELMET,
	ICLOAK,
	ISHIELD,
	IARMOR,
	IGLOVES,
	IBOOTS,
	NEQUIP,

	/* item flags */
	IFSTACKS = 0x1,
};

struct ItemData
{
	/* item name */
	char name[SZNAME];
	/* type */
	int type;
	/* tile of this item */
	int tile;

	/* $ value */
	int cost;
	/* weight */
	int weight;

	/* if weapon, these are set */
	int rolls;
	int atk;

	/* if armor, this is set */
	int ac;

	/* various flags */
	int flags;
};

enum
{
	IMAXAGE	= 500,
};

struct Item
{
	Ref ref;

	/* age, for automatic deletion */
	int age;

	/* count of items in stack */
	int count;

	/* has a unique name */
	char *name;

	/* base data */
	ItemData *id;

	/* in a list */
	Item *next;
};

struct ItemList
{
	int count;
	Item *head, *tail;
};

/* item.c */
#pragma varargck type "i" Item*
char *itypetoname(int type);
int itemdbopen(char *file);
ItemData *idbyname(char *item);
Item *ibyname(char *item);
void ifree(Item *i);
void ilfree(ItemList *il);
Item *iltakenth(ItemList *il, int n);
Item *ilnth(ItemList *il, int n);
void iladd(ItemList *il, Item *i);

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

/* describes what a monster can be equipped with. */
struct EquipData
{
	/* of item/itemlist */
	char name[SZNAME];
	/* chance of being created */
	double prob;
};

struct MonsterData
{
	char name[SZNAME];
	/* offset into tileset */
	int tile;

	/* base experience level */
	int basexpl;
	char align;
	int mvr;
	int maxhp;
	int def;
	int rolls;
	int atk;

	EquipData equip[10];
	int nequip;
};

/* possible moves */
enum
{
	MNONE,
	MMOVE,	/* also attack */
	MUSE,
	MPICKUP,
	MSPECIAL,
};

/* monster flags */
enum
{
	Mdead	= 0x1,
};

struct Monster
{
	/* references */
	Ref ref;

	/* current level */
	Level *l;
	/* location */
	Point pt;

	/* alignment */
	char align;

	/* move rate */
	int mvr;
	/* move points */
	int mvp;
	/* turn counter */
	int turns;

	/* current hp */
	double hp;
	/* maximum hp */
	double maxhp;

	/* armor class */
	int ac;

	/* kill count */
	int kills;

	/* experience level */
	int xpl;
	/* experience gained */
	int xp;

	/* flags */
	u32int flags;

	/* ai state */
	AIState *ai;
	AIState *aglobal;

	/* inventory */
	ItemList inv;

	/* equipped weapon & armor */
	Item *armor[NEQUIP];

	/* no free */
	MonsterData *md;
};

/* monst.c */
int monstdbopen(char *file);
MonsterData *mdbyname(char *monster);
Monster *mbyname(char *monster);
void mfree(Monster *m);
int mupdate(Monster *m);
void mpushstate(Monster *m, AIState *a);
AIState *mpopstate(Monster *m);
int maction(Monster *m, int what, Point where);
int mwield(Monster *m, int n);
int munwield(Monster *m, int type);
void maddinv(Monster *m, Item *i);
void mgenequip(Monster *m);
int xpcalc(int level);

/* well known tiles, indexes into tileset */
enum
{
	/* features */
	TWALL		= 840,
	TTREE		= 847,
	TFLOOR		= 848,
	TUPSTAIR	= 851,
	TDOWNSTAIR	= 852,
	TGRAVE		= 856,
	TWATER		= 860,
	TICE		= 861,
	TLAVA		= 862,
	TSPAWN		= 888,
	TPORTAL		= 891,
};

struct Portal
{
	char name[32];
	/* where it takes us to */
	Level *to;
	/* location in to */
	Point pt;
};

struct Spawn
{
	/* where spawn occurs */
	Point pt;
	/* what to spawn */
	char what[SZNAME];
	/* frequency of spawn, modulo turns */
	int freq;
	/* turn counter */
	int turn;
	/* spawn baddies when enemy is around? */
	int istrap;
};

struct Tile
{
	/* current monster occupying tile */
	Monster *monst;

	/* has a portal */
	Portal *portal;

	/* items here */
	ItemList items;

	/*
	Feature *feat;
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
	Fblocked	= 0x1,
	Fhasmonster	= 0x2,
	Fhasfeature	= 0x4,
	Fhasobject	= 0x8,
	Fhasitem	= 0x10,
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

	AIState *ai;

	int		nspawns;
	Spawn	spawns[10];
};

/* level.c */
Level *genlevel(int width, int height, int type);
void freelevel(Level *l);
void addspawn(Level *l, Point p, char *what, int freq, int istrap);
Tile *tileat(Level *l, Point p);
#define flagat(l, p) (*(l->flags+(p.y*l->width)+p.x))
#define hasflagat(l, p, F) (*(l->flags+(p.y*l->width)+p.x) & (F))
#define setflagat(l, p, F) (*(l->flags+(p.y*l->width)+p.x) |= (F))
#define clrflagat(l, p, F) (*(l->flags+(p.y*l->width)+p.x) &= ~(F))

/* levelgen.c */
Level *mklevel(int width, int height, int floor);
void several(Level *l, Point *p, int count, char *type, int r);
int mkldebug(Level *l);
int mklforest(Level *l);
int mklgraveyard(Level *l);
int mklvolcano(Level *l);
int mklcastle(Level *l);

enum {
	/* cost to move N S E W */
	ORTHOCOST	= 10,
};

/* path.c */
int manhattan(Point cur, Point targ);
int pathfind(Level *l, Point start, Point end, Point **path, int not);

/* ui.c */
void msg(char *fmt, ...);
void good(char *fmt, ...);
void warn(char *fmt, ...);
void bad(char *fmt, ...);
void dbg(char *fmt, ...);
void uiinit(char *name);
void uiexec(AIState *ai);
void uiredraw(int justui);

/* util.c */
extern Point cardinals[];
Point pickpoint(Rectangle r, int dir, int inset);
int roll(int count, int sides);
int parseroll(char *str, int *count, int *sides);
int min(int a, int b);
int max(int a, int b);
void *emalloc(ulong);
