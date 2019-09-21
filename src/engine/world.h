enum                            // hardcoded texture numbers
{
    DEFAULT_SKY = 0,
    DEFAULT_GEOM,
    NUMDEFAULTSLOTS
};

#define OCTAVERSION 33          // diverged at ver 25
#define MAPVERSION 48           // bump if map format changes, see worldio.cpp

struct binary
{
    char head[4];
    int version, headersize;
};

#define OCTASTRLEN 260
struct octa : binary
{
    int worldsize;
    int numents;
    int numpvs;
    int lightmaps;
    int blendmap;
    int numvars;
    int numvslots;
};

struct mapz : binary
{
    int worldsize, numents, numpvs, blendmap, numvslots;
    int gamever, revision;
    char gameid[4];
};

struct mapzcompat43 : binary
{
    int worldsize, numents, numpvs, lightmaps, blendmap, numvslots;
    int gamever, revision;
    char gameid[4];
};

struct entcompat
{
    vec o;
    short attr[5];
    uchar type;
    uchar reserved;
};

#define WATER_AMPLITUDE 0.4f
#define WATER_OFFSET 1.1f

enum
{
    MATSURF_NOT_VISIBLE = 0,
    MATSURF_VISIBLE,
    MATSURF_EDIT_ONLY
};

#define isliquid(mat) ((mat)==MAT_WATER || (mat)==MAT_LAVA)
#define isclipped(mat) ((mat)==MAT_GLASS)
#define isdeadly(mat) ((mat)==MAT_LAVA)

#define TEX_SCALE 8.0f

struct vertex { vec pos; bvec4 norm; vec tc; bvec4 tangent; };
