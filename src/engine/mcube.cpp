#include "engine.h"
#include "mcube.h"

// the texture new faces get: the one last applied, as a new extrusion would have, or the default when none has been (lasttex 0)
static int shapetex()
{
    extern int lasttex;
    return lasttex > DEFAULT_SKY && vslots.inrange(lasttex) ? lasttex : DEFAULT_GEOM;
}

#define SHAPE_MAXSIDE 2048        // the most cubes a shape's box may have on a side, the width of the largest map at grid 32
#define SHAPE_MAXCUBES (1<<23)    // the most cubes a shape's box may hold, counted at the selection's grid size: about 1 GB solid at grid 1

extern undoblock *newundocube(const selinfo &s);
extern void addundo(undoblock *u);

// records the box for undo before it changes, saying so when it is too big for the undo buffer
static bool shapeundo(const selinfo &s)
{
    undoblock *u = newundocube(s);
    if(!u)
    {
        conoutf(colourred, "A shape of %d x %d x %d cubes is too big to undo, which Undo size (undomegs) sets", s.s.x, s.s.y, s.s.z);
        return false;
    }
    addundo(u);
    return true;
}

// whether a shape can be built now, saying why not when it can't
static bool shapeready()
{
    if(noedit()) return false;
    if(!havesel) { conoutf(colourred, "Select where the shape goes first"); return false; }
    return true;
}

// why box s, once cut to the world, can't hold a shape, or NULL when it can
static const char *shapeproblem(selinfo &s)
{
    static string msg;
    extern int undomegs;
    if(s.s.x <= 0 || s.s.y <= 0 || s.s.z <= 0) return "The shape has no size";
    if(!s.validate()) return "The shape is outside the world";
    if(s.s.x > SHAPE_MAXSIDE || s.s.y > SHAPE_MAXSIDE || s.s.z > SHAPE_MAXSIDE)
        formatstring(msg, "A shape of %d x %d x %d cubes is too big, the limit is %d cubes on a side", s.s.x, s.s.y, s.s.z, SHAPE_MAXSIDE);
    else if(s.s.x * int64_t(s.s.y) * s.s.z > SHAPE_MAXCUBES)
        formatstring(msg, "A shape of %d x %d x %d cubes is too big, the limit is %d cubes in all", s.s.x, s.s.y, s.s.z, SHAPE_MAXCUBES);
    // as newundocube reckons it
    else if(int64_t(sizeof(block3)) + s.s.x * int64_t(s.s.y) * s.s.z * int64_t(sizeof(cube)) > int64_t(undomegs)<<20)
        formatstring(msg, "A shape of %d x %d x %d cubes is too big to undo, which Undo size (undomegs) sets", s.s.x, s.s.y, s.s.z);
    else return NULL;
    return msg;
}

// cuts box s to the world and checks a shape fits it, saying why not when it doesn't
static bool shapefits(selinfo &s)
{
    const char *problem = shapeproblem(s);
    if(problem) conoutf(colourred, "%s", problem);
    return !problem;
}

// why a shape of sx x sy x sz cubes at the selection can't be built now, or nothing when it can, for the Shapes panel
ICOMMAND(0, shapeproblem, "iii", (int *sx, int *sy, int *sz),
{
    if(!editmode) { result("Only in edit mode"); return; }
    if(!havesel) { result("Select where the shape goes first"); return; }
    selinfo s = sel;
    s.s = ivec(*sx, *sy, *sz);
    const char *problem = shapeproblem(s);
    result(problem ? problem : "");
});

// adds or carves the shape over every cube of the box s its constructor set, cut to the world, as cubes of the selection's grid size;
// the box becomes the selection, and one undo takes the whole shape back
static void buildshape(iso &shape, selinfo &s, int detail, bool carve)
{
    if(!shapefits(s)) return;
    // no finer than the smallest cube
    int maxdetail = 0;
    while((1<<maxdetail) < s.grid) maxdetail++;
    detail = clamp(detail, 0, maxdetail);

    int serial = undoserial;
    if(!shapeundo(s)) return;
    sel = s;
    shape.aabb = block3(sel);
    block3 b = shape.aabb;
    b.orient = 4; // the bottom face, so x, y and z run along the world's
    int tex = shapetex();
    loopxyz(b, b.grid, marchshape(c, vec(x, y, z).mul(b.grid), b.grid, shape, detail, carve, tex));
    changed(sel, true);
    sendeditcubes(block3(sel), serial);
}

void buildsphere(float *r, int *d, float *ox, float *oy, float *oz, float *sx, float *sy, float *sz, int *carve)
{
    if(!shapeready()) return;
    selinfo s = sel;
    sphere_iso shape(s, *r, vec(*ox, *oy, *oz), vec(*sx, *sy, *sz));
    buildshape(shape, s, *d, *carve != 0);
}
COMMAND(0, buildsphere, "fiffffffi");

// a sphere stretched by its scales
void buildellipsoid(float *r, int *d, float *ox, float *oy, float *oz, float *sx, float *sy, float *sz, int *carve)
{
    buildsphere(r, d, ox, oy, oz, sx, sy, sz, carve);
}
COMMAND(0, buildellipsoid, "fiffffffi");

void buildhelix(float *r, float *br, int *d, int *carve)
{
    if(!shapeready()) return;
    selinfo s = sel;
    helix_iso shape(s, *br, *r);
    buildshape(shape, s, *d, *carve != 0);
}
COMMAND(0, buildhelix, "ffii");

void buildcylinder(float *r, float *h, int *d, float *ox, float *oy, float *oz, int *carve)
{
    if(!shapeready()) return;
    selinfo s = sel;
    cylinder_iso shape(s, *r, *h, vec(*ox, *oy, *oz));
    buildshape(shape, s, *d, *carve != 0);
}
COMMAND(0, buildcylinder, "ffifffi");

void buildtunnel(float *r, float *h, int *d, float *ox, float *oy, float *oz, int *carve)
{
    if(!shapeready()) return;
    selinfo s = sel;
    tunnel_iso shape(s, *r, *h, vec(*ox, *oy, *oz));
    buildshape(shape, s, *d, *carve != 0);
}
COMMAND(0, buildtunnel, "ffifffi");

void buildpipebend(float *a, float *b, int *d, float *ox, float *oy, float *oz, int *carve)
{
    if(!shapeready()) return;
    selinfo s = sel;
    pipebend_iso shape(s, *a, *b, vec(*ox, *oy, *oz));
    buildshape(shape, s, *d, *carve != 0);
}
COMMAND(0, buildpipebend, "ffifffi");

void buildtorus(float *a, float *b, int *d, float *ox, float *oy, float *oz, int *carve)
{
    if(!shapeready()) return;
    selinfo s = sel;
    torus_iso shape(s, *a, *b, vec(*ox, *oy, *oz));
    buildshape(shape, s, *d, *carve != 0);
}
COMMAND(0, buildtorus, "ffifffi");

void buildparaboloid(float *r, int *d, float *ox, float *oy, float *oz, int *carve)
{
    if(!shapeready()) return;
    selinfo s = sel;
    paraboloid_iso shape(s, *r, vec(*ox, *oy, *oz));
    buildshape(shape, s, *d, *carve != 0);
}
COMMAND(0, buildparaboloid, "fifffi");

void buildbox(float *sx, float *sy, float *sz, float *round, int *d, float *ox, float *oy, float *oz, int *carve)
{
    if(!shapeready()) return;
    selinfo s = sel;
    box_iso shape(s, vec(*sx, *sy, *sz), *round, vec(*ox, *oy, *oz));
    buildshape(shape, s, *d, *carve != 0);
}
COMMAND(0, buildbox, "ffffifffi");

void buildcone(float *r, float *h, int *d, float *ox, float *oy, float *oz, int *carve)
{
    if(!shapeready()) return;
    selinfo s = sel;
    cone_iso shape(s, *r, *h, vec(*ox, *oy, *oz));
    buildshape(shape, s, *d, *carve != 0);
}
COMMAND(0, buildcone, "ffifffi");

// This function uses a bitmap image as a heightmap to generate terrain.
// It treats each pixel as terrain height data at the corner of a column of cubes.
// The X,Y size of the graphic should thus always be one larger than the X,Y
// dimensions of the terrain area you want to create when counted in cubes.
// Only the first byte of color data is used, so heights ranging from 0-255 are
// possible. You should use greyscale images just to avoid confusion.
//
// Parameters: imagename = A bitmap image to use as the heightmap
//             h = topmost height of the terrain, measured in cubes.
//
void buildterrain(char *imagename, float *h, int *carve)
{
    if(!shapeready()) return;
    ImageData image;
    if(!loadimage(imagename, image))
    {
        conoutf(colourred, "Could not load terrain image: %s", imagename);
        return;
    }
    if(image.w < 2 || image.h < 2 || image.compressed)
    {
        conoutf(colourred, "Terrain image %s must be at least 2 x 2 pixels and not compressed", imagename);
        return;
    }
    selinfo s = sel;
    terrain_iso shape(s, image, *h);
    buildshape(shape, s, 1, *carve != 0);
}
COMMAND(0, buildterrain, "sfi");

// the size of a heightmap image as buildterrain loads it, "w h", or nothing when it can't be loaded or used; the last one
// asked for is kept, as the Shapes panel asks every frame
static void terrainimagesize(char *name)
{
    static string last = "", size = "";
    if(strcmp(last, name))
    {
        copystring(last, name);
        size[0] = '\0';
        ImageData image;
        if(name[0] && loadimage(name, image) && image.w >= 2 && image.h >= 2 && !image.compressed) formatstring(size, "%d %d", image.w, image.h);
    }
    result(size);
}
COMMAND(0, terrainimagesize, "s");

void buildoctahedron(float *r, int *d, float *ox, float *oy, float *oz, int *carve)
{
    if(!shapeready()) return;
    selinfo s = sel;
    octahedron_iso shape(s, *r, vec(*ox, *oy, *oz));
    buildshape(shape, s, *d, *carve != 0);
}
COMMAND(0, buildoctahedron, "fifffi");

// imports a .smc file (Sauerbraten marching cubes): a line per cube, "x y z size" from the selection's corner and then its eight
// corner densities, negative inside; with voxel set, each listed cube is made solid instead
struct smccube
{
    ivec o;
    int size;
    double d[8];
};

void importsmc(char *name, int *voxel)
{
    if(!shapeready()) return;
    stream *f = openfile(name, "r");
    if(!f)
    {
        conoutf(colourred, "Could not load SMC file: %s", name);
        return;
    }
    vector<smccube> cubes;
    ivec bbmin(worldsize, worldsize, worldsize), bbmax(0, 0, 0);
    int grid = worldsize, lines = 0, skipped = 0;
    char buf[1024];
    while(f->getline(buf, sizeof(buf)))
    {
        lines++;
        smccube c;
        int x, y, z;
        if(sscanf(buf, "%d %d %d %d %lf %lf %lf %lf %lf %lf %lf %lf", &x, &y, &z, &c.size, &c.d[0], &c.d[1], &c.d[3], &c.d[2], &c.d[4], &c.d[5], &c.d[7], &c.d[6]) != 12 ||
           c.size <= 0 || c.size >= worldsize || (c.size & (c.size-1)))
        {
            // blank lines aren't counted as skipped
            if(buf[strspn(buf, " \t\r\n")]) skipped++;
            continue;
        }
        c.o = ivec(x, y, z).add(sel.o).mask(~(c.size-1));
        if(!insideworld(c.o)) { skipped++; continue; }
        cubes.add(c);
        bbmin.min(c.o);
        bbmax.max(ivec(c.o).add(c.size));
        grid = min(grid, c.size);
    }
    delete f;
    if(skipped) conoutf(colouryellow, "Skipped %d of %d lines of %s that aren't cubes inside the world", skipped, lines, name);
    if(cubes.empty()) { conoutf(colourred, "No cubes to import from %s", name); return; }

    // the box holding every cube, as the selection one undo takes back
    selinfo s = sel;
    s.grid = grid;
    s.o = ivec(bbmin).mask(~(grid-1));
    s.s = ivec(bbmax).sub(s.o).add(grid-1).div(grid);
    int serial = undoserial;
    if(!shapefits(s) || !shapeundo(s)) return;
    sel = s;
    int tex = shapetex();
    loopv(cubes)
    {
        cube &c = lookupcube(cubes[i].o, cubes[i].size);
        if(*voxel)
        {
            discardchildren(c, true);
            solidfaces(c);
            loopj(6) c.texture[j] = tex;
            continue;
        }
        march_cube mc;
        loopj(8) mc.corners[j] = iso_point(cubes[i].d[j]);
        mc.init_cube_index();
        discardchildren(c, true);
        mc.render(c, tex);
    }
    changed(sel, true);
    if(sendeditcubes(block3(sel), serial)) conoutf(colourwhite, "Imported %d cubes from %s", cubes.length(), name);
}
COMMAND(0, importsmc, "si");
