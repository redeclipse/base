// 6-directional octree heightfield map format

struct elementset
{
    ushort texture, envmap;
    union
    {
        struct { uchar orient, layer; };
        ushort reuse;
    };
    ushort length, minvert, maxvert;
};

enum
{
    EMID_NONE = 0,
    EMID_CUSTOM,
    EMID_SKY,
    EMID_RESERVED
};

struct materialsurface
{
    ivec o;
    ushort csize, rsize;
    ushort material, skip;
    uchar orient, visible;
    union
    {
        ushort envmap;
        uchar ends;
    };
};

struct vertinfo
{
    ushort x, y, z, norm;

    void setxyz(ushort a, ushort b, ushort c) { x = a; y = b; z = c; }
    void setxyz(const ivec &v) { setxyz(v.x, v.y, v.z); }
    void set(ushort a, ushort b, ushort c, ushort n = 0) { setxyz(a, b, c); norm = n; }
    void set(const ivec &v, ushort n = 0) { set(v.x, v.y, v.z, n); }
    ivec getxyz() const { return ivec(x, y, z); }
};

enum
{
    LAYER_TOP    = (1<<5),
    LAYER_BOTTOM = (1<<6),

    LAYER_BLEND  = LAYER_TOP|LAYER_BOTTOM,

    MAXFACEVERTS = 15
};

struct surfaceinfo
{
    uchar verts, numverts;

    int totalverts() const { return numverts&MAXFACEVERTS; }
    bool used() const { return (numverts&~LAYER_TOP) != 0; }
    void clear() { numverts = (numverts&MAXFACEVERTS) | LAYER_TOP; }
    void brighten() { clear(); }
};

static const surfaceinfo topsurface = {0, LAYER_TOP};
static const surfaceinfo bottomsurface = {0, LAYER_BOTTOM};
#define brightsurface topsurface
#define ambientsurface topsurface

struct grasstri
{
    vec v[4];
    int numv;
    plane surface;
    vec center;
    float radius;
    float minz, maxz;
    ushort texture, blend;
};

struct occludequery
{
    void *owner;
    GLuint id;
    int fragments;
};

struct vtxarray;

struct octaentities
{
    vector<int> mapmodels, decals, other;
    occludequery *query;
    octaentities *next, *rnext;
    int distance;
    ivec o;
    int size;
    ivec bbmin, bbmax;

    octaentities(const ivec &o, int size) : query(0), o(o), size(size), bbmin(o), bbmax(o)
    {
        bbmin.add(size);
    }
};

enum
{
    OCCLUDE_NOTHING = 0,
    OCCLUDE_GEOM,
    OCCLUDE_BB,
    OCCLUDE_PARENT
};

enum
{
    MERGE_ORIGIN = 1<<0,
    MERGE_PART   = 1<<1,
    MERGE_USE    = 1<<2
};

struct vtxarray
{
    vtxarray *parent;
    vector<vtxarray *> children;
    vtxarray *next, *rnext;  // linked list of visible VOBs
    vertex *vdata;           // vertex data
    ushort voffset, eoffset, skyoffset, decaloffset; // offset into vertex data
    ushort *edata, *skydata, *decaldata; // vertex indices
    GLuint vbuf, ebuf, skybuf, decalbuf; // VBOs
    ushort minvert, maxvert; // DRE info
    elementset *texelems, *decalelems;   // List of element indices sets (range) per texture
    materialsurface *matbuf; // buffer of material surfaces
    int verts, tris, texs, blendtris, blends, alphabacktris, alphaback, alphafronttris, alphafront, refracttris, refract, texmask, sky, matsurfs, matmask, distance, rdistance, dyntexs, decaltris, decaltexs;
    ivec o;
    int size;                // location and size of cube.
    ivec geommin, geommax;   // BB of geom
    ivec alphamin, alphamax; // BB of alpha geom
    ivec refractmin, refractmax; // BB of refract geom
    ivec skymin, skymax;     // BB of any sky geom
    ivec lavamin, lavamax;   // BB of any lava
    ivec watermin, watermax; // BB of any water
    ivec glassmin, glassmax; // BB of any glass
    ivec nogimin, nogimax;   // BB of any nogi
    ivec bbmin, bbmax;       // BB of everything including children
    uchar curvfc, occluded;
    occludequery *query;
    vector<octaentities *> mapmodels, decals;
    vector<grasstri> grasstris;
    int hasmerges, mergelevel;
    int shadowmask;
};

struct cube;

struct clipplanes
{
    vec o, r, v[8];
    plane p[12];
    uchar side[12];
    uchar size, visible;
    const cube *owner;
    int version;
};

struct facebounds
{
    ushort u1, u2, v1, v2;

    bool empty() const { return u1 >= u2 || v1 >= v2; }
};

struct tjoint
{
    int next;
    ushort offset;
    uchar edge;
};

struct cubeext
{
    vtxarray *va;            // vertex array for children, or NULL
    octaentities *ents;      // map entities inside cube
    surfaceinfo surfaces[6]; // render info for each surface
    int tjoints;             // linked list of t-joints
    uchar maxverts;          // allocated space for verts

    vertinfo *verts() { return (vertinfo *)(this+1); }
};

struct cube
{
    cube *children;          // points to 8 cube structures which are its children, or NULL. -Z first, then -Y, -X
    cubeext *ext;            // extended info for the cube
    union
    {
        uchar edges[12];     // edges of the cube, each uchar is 2 4bit values denoting the range.
                             // see documentation jpgs for more info.
        uint faces[3];       // 4 edges of each dimension together representing 2 perpendicular faces
    };
    ushort texture[6];       // one for each face. same order as orient.
    ushort material;         // empty-space material
    uchar merged;            // merged faces of the cube
    union
    {
        uchar escaped;       // mask of which children have escaped merges
        uchar visible;       // visibility info for faces
    };
};

struct block3
{
    ivec o, s;
    int grid, orient;
    block3() {}
    block3(const selinfo &sel) : o(sel.o), s(sel.s), grid(sel.grid), orient(sel.orient) {}
    cube *c()           { return (cube *)(this+1); }
    int size()    const { return s.x*s.y*s.z; }
};

struct editinfo
{
    block3 *copy;
    editinfo() : copy(NULL) {}
};

struct undoent  { int i; entbase e; int numattrs; };
struct undoblock // undo header, all data sits in payload
{
    undoblock *prev, *next;
    int size, timestamp, numents; // if numents is 0, is a cube undo record, otherwise an entity undo record

    block3 *block() { return (block3 *)(this + 1); }
    uchar *gridmap()
    {
        block3 *ub = block();
        return (uchar *)(ub->c() + ub->size());
    }
    undoent *ents() { return (undoent *)(this + 1); }
    int *attrs() { return (int *)(ents() + numents); }
};

extern cube *worldroot;             // the world data. only a ptr to 8 cubes (ie: like cube.children above)
extern int wtris, wverts, vtris, vverts, glde, gbatches;
extern int allocnodes, allocva, selchildcount, selchildmat;

const uint F_EMPTY = 0;             // all edges in the range (0,0)
const uint F_SOLID = 0x80808080;    // all edges in the range (0,8)

#define isempty(c) ((c).faces[0]==F_EMPTY)
#define isentirelysolid(c) ((c).faces[0]==F_SOLID && (c).faces[1]==F_SOLID && (c).faces[2]==F_SOLID)
#define setfaces(c, face) { (c).faces[0] = (c).faces[1] = (c).faces[2] = face; }
#define solidfaces(c) setfaces(c, F_SOLID)
#define emptyfaces(c) setfaces(c, F_EMPTY)

#define edgemake(a, b) ((b)<<4|a)
#define edgeget(edge, coord) ((coord) ? (edge)>>4 : (edge)&0xF)
#define edgeset(edge, coord, val) ((edge) = ((coord) ? ((edge)&0xF)|((val)<<4) : ((edge)&0xF0)|(val)))

#define cubeedge(c, d, x, y) ((c).edges[(((d)<<2)+((y)<<1)+(x))])

#define octadim(d)          (1<<(d))                    // creates mask for bit of given dimension
#define octacoord(d, i)     (((i)&octadim(d))>>(d))
#define oppositeocta(d, i)  ((i)^octadim(D[d]))
#define octaindex(d,x,y,z)  (((z)<<D[d])+((y)<<C[d])+((x)<<R[d]))
#define octastep(x, y, z, scale) (((((z)>>(scale))&1)<<2) | ((((y)>>(scale))&1)<<1) | (((x)>>(scale))&1))

static inline uchar octaboxoverlap(const ivec &o, int size, const ivec &bbmin, const ivec &bbmax)
{
    uchar p = 0xFF; // bitmask of possible collisions with octants. 0 bit = 0 octant, etc
    ivec mid = ivec(o).add(size);
    if(mid.z <= bbmin.z)      p &= 0xF0; // not in a -ve Z octant
    else if(mid.z >= bbmax.z) p &= 0x0F; // not in a +ve Z octant
    if(mid.y <= bbmin.y)      p &= 0xCC; // not in a -ve Y octant
    else if(mid.y >= bbmax.y) p &= 0x33; // etc..
    if(mid.x <= bbmin.x)      p &= 0xAA;
    else if(mid.x >= bbmax.x) p &= 0x55;
    return p;
}

#define loopoctabox(o, size, bbmin, bbmax) uchar possible = octaboxoverlap(o, size, bbmin, bbmax); loopi(8) if(possible&(1<<i))
#define loopoctaboxsize(o, size, bborigin, bbsize) uchar possible = octaboxoverlap(o, size, bborigin, ivec(bborigin).add(bbsize)); loopi(8) if(possible&(1<<i))

enum
{
    O_LEFT = 0,
    O_RIGHT,
    O_BACK,
    O_FRONT,
    O_BOTTOM,
    O_TOP,
    O_ANY
};

#define dimension(orient) ((orient)>>1)
#define dimcoord(orient)  ((orient)&1)
#define opposite(orient)  ((orient)^1)

enum
{
    VFC_FULL_VISIBLE = 0,
    VFC_PART_VISIBLE,
    VFC_FOGGED,
    VFC_NOT_VISIBLE,
    PVS_FULL_VISIBLE,
    PVS_PART_VISIBLE,
    PVS_FOGGED
};

#define GENCUBEVERTS(x0,x1, y0,y1, z0,z1) \
    GENCUBEVERT(0, x1, y1, z0) \
    GENCUBEVERT(1, x0, y1, z0) \
    GENCUBEVERT(2, x0, y1, z1) \
    GENCUBEVERT(3, x1, y1, z1) \
    GENCUBEVERT(4, x1, y0, z1) \
    GENCUBEVERT(5, x0, y0, z1) \
    GENCUBEVERT(6, x0, y0, z0) \
    GENCUBEVERT(7, x1, y0, z0)

#define GENFACEVERTX(o,n, x,y,z, xv,yv,zv) GENFACEVERT(o,n, x,y,z, xv,yv,zv)
#define GENFACEVERTSX(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEORIENT(0, GENFACEVERTX(0,0, x0,y1,z1, d0,r1,c1), GENFACEVERTX(0,1, x0,y1,z0, d0,r1,c0), GENFACEVERTX(0,2, x0,y0,z0, d0,r0,c0), GENFACEVERTX(0,3, x0,y0,z1, d0,r0,c1)) \
    GENFACEORIENT(1, GENFACEVERTX(1,0, x1,y1,z1, d1,r1,c1), GENFACEVERTX(1,1, x1,y0,z1, d1,r0,c1), GENFACEVERTX(1,2, x1,y0,z0, d1,r0,c0), GENFACEVERTX(1,3, x1,y1,z0, d1,r1,c0))
#define GENFACEVERTY(o,n, x,y,z, xv,yv,zv) GENFACEVERT(o,n, x,y,z, xv,yv,zv)
#define GENFACEVERTSY(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEORIENT(2, GENFACEVERTY(2,0, x1,y0,z1, c1,d0,r1), GENFACEVERTY(2,1, x0,y0,z1, c0,d0,r1), GENFACEVERTY(2,2, x0,y0,z0, c0,d0,r0), GENFACEVERTY(2,3, x1,y0,z0, c1,d0,r0)) \
    GENFACEORIENT(3, GENFACEVERTY(3,0, x0,y1,z0, c0,d1,r0), GENFACEVERTY(3,1, x0,y1,z1, c0,d1,r1), GENFACEVERTY(3,2, x1,y1,z1, c1,d1,r1), GENFACEVERTY(3,3, x1,y1,z0, c1,d1,r0))
#define GENFACEVERTZ(o,n, x,y,z, xv,yv,zv) GENFACEVERT(o,n, x,y,z, xv,yv,zv)
#define GENFACEVERTSZ(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEORIENT(4, GENFACEVERTZ(4,0, x0,y0,z0, r0,c0,d0), GENFACEVERTZ(4,1, x0,y1,z0, r0,c1,d0), GENFACEVERTZ(4,2, x1,y1,z0, r1,c1,d0), GENFACEVERTZ(4,3, x1,y0,z0, r1,c0,d0)) \
    GENFACEORIENT(5, GENFACEVERTZ(5,0, x0,y0,z1, r0,c0,d1), GENFACEVERTZ(5,1, x1,y0,z1, r1,c0,d1), GENFACEVERTZ(5,2, x1,y1,z1, r1,c1,d1), GENFACEVERTZ(5,3, x0,y1,z1, r0,c1,d1))
#define GENFACEVERTSXY(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEVERTSX(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEVERTSY(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1)
#define GENFACEVERTS(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEVERTSXY(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1) \
    GENFACEVERTSZ(x0,x1, y0,y1, z0,z1, c0,c1, r0,r1, d0,d1)

extern bool collidesolidface(const cube &c, const int orient, const ivec &co, const int size);
