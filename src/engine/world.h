
enum                            // hardcoded texture numbers
{
    DEFAULT_SKY = 0,
    DEFAULT_GEOM
};

#define OCTAVERSION 33          // diverged at ver 25
#define MAPVERSION 43           // bump if map format changes, see worldio.cpp

struct binary
{
    char head[4];
    int version, headersize;
};

#define OCTASTRLEN 260
struct octacompat25 : binary
{
    int worldsize;
    int numents;
    int waterlevel;
    int lightmaps;
    int lightprecision, lighterror, lightlod;
    uchar ambient;
    uchar watercolour[3];
    uchar blendmap;
    uchar lerpangle, lerpsubdiv, lerpsubdivsize;
    uchar bumperror;
    uchar skylight[3];
    uchar lavacolour[3];
    uchar waterfallcolour[3];
    uchar reserved[10];
    char maptitle[128];
};

struct octacompat28 : binary
{
    int worldsize;
    int numents;
    int numpvs;
    int lightmaps;
    int lightprecision, lighterror, lightlod;
    uchar ambient;
    uchar watercolour[3];
    uchar blendmap;
    uchar lerpangle, lerpsubdiv, lerpsubdivsize;
    uchar bumperror;
    uchar skylight[3];
    uchar lavacolour[3];
    uchar waterfallcolour[3];
    uchar reserved[10];
    char maptitle[128];
};

struct octacompat29 : binary
{
    int worldsize;
    int numents;
    int numpvs;
    int lightmaps;
    int blendmap;
    int numvars;
};

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
    int worldsize, numents, numpvs, lightmaps, blendmap, numvslots;
    int gamever, revision;
    char gameid[4];
};

struct mapzcompat38 : binary
{
    int worldsize, numents, numpvs, lightmaps, blendmap;
    int gamever, revision;
    char gameid[4];
};

struct mapzcompat33 : binary
{
    int worldsize, numents, numpvs, lightmaps, blendmap;
    int gamever, revision;
    char maptitle[128], gameid[4];
};

struct mapzcompat32 : binary
{
    int worldsize, numents, numpvs, lightmaps;
    int gamever, revision;
    char maptitle[128], gameid[4];
};

struct mapzcompat25 : binary
{
    int worldsize, numents, lightmaps;
    int gamever, revision;
    char maptitle[128], gameid[4];
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

struct vertex { vec pos; bvec4 norm; vec2 tc; svec2 lm; bvec4 tangent; };

