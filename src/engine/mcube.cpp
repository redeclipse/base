#include "engine.h"
#include "mcube.h"

// the texture new faces get: the one last applied, as a new extrusion would have, or the default when none has been (lasttex 0)
static int shapetex()
{
    extern int lasttex;
    return lasttex > DEFAULT_SKY && vslots.inrange(lasttex) ? lasttex : DEFAULT_GEOM;
}

#define SHAPE_MAXSIDE 2048        // the most cubes a Shape's box may have on a side, the width of the largest map at grid 32
#define SHAPE_MAXCUBES (1<<25)    // the most cubes a Shape's box may hold, counted at the selection's grid size

extern void makeundo(selinfo &s);

// whether a Shape can be built now, saying why not when it can't
static bool shapeready()
{
    if(noedit()) return false;
    if(!havesel) { conoutf(colourred, "Select where the Shape goes first"); return false; }
    return true;
}

// adds or carves the Shape over every cube of the box s its constructor set, cut to the world, as cubes of the selection's grid size;
// the box becomes the selection, and one undo takes the whole Shape back
static void buildshape(iso &shape, selinfo &s, int detail, bool carve)
{
    if(!s.validate()) { conoutf(colourred, "The Shape is outside the world"); return; }
    if(s.s.x > SHAPE_MAXSIDE || s.s.y > SHAPE_MAXSIDE || s.s.z > SHAPE_MAXSIDE)
    {
        conoutf(colourred, "A Shape of %d x %d x %d cubes is too big, the limit is %d cubes on a side", s.s.x, s.s.y, s.s.z, SHAPE_MAXSIDE);
        return;
    }
    if(s.s.x * int64_t(s.s.y) * s.s.z > SHAPE_MAXCUBES)
    {
        conoutf(colourred, "A Shape of %d x %d x %d cubes is too big, the limit is %d cubes in all", s.s.x, s.s.y, s.s.z, SHAPE_MAXCUBES);
        return;
    }
    // no finer than the smallest cube
    int maxdetail = 0;
    while((1<<maxdetail) < s.grid) maxdetail++;
    detail = clamp(detail, 0, maxdetail);

    sel = s;
    makeundo(sel);
    shape.aabb = block3(sel);
    block3 b = shape.aabb;
    b.orient = 4; // the bottom face, so x, y and z run along the world's
    int tex = shapetex();
    loopxyz(b, b.grid, marchshape(c, vec(x, y, z).mul(b.grid), b.grid, shape, detail, carve, tex));
    changed(sel, true);
}

void buildsphere(float *r, int *d, float *ox, float *oy, float *oz, float *sx, float *sy, float *sz, int *carve)
{
    if(!shapeready()) return;
    selinfo s = sel;
    sphere_iso shape(s, *r, vec(*ox, *oy, *oz), vec(*sx, *sy, *sz));
    buildshape(shape, s, *d, *carve != 0);
}
COMMAND(0, buildsphere, "fiffffffi");

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
void terrain(char *imagename, float *h, int *carve)
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
COMMAND(0, terrain, "sfi");

void buildoctahedron(float *r, int *d, float *ox, float *oy, float *oz, int *carve)
{
    if(!shapeready()) return;
    selinfo s = sel;
    octahedron_iso shape(s, *r, vec(*ox, *oy, *oz));
    buildshape(shape, s, *d, *carve != 0);
}
COMMAND(0, buildoctahedron, "fifffi");

// IMPORT *.smc (Sauerbraten Marching Cubes)

void importsmc(char *name, bool *voxel)
{
    int cnt = 0;
    stream *f = openfile(name, "r");
    if(!f)
    {
        conoutf(colourwhite, "Could not load SMC file: %s", name);
        return;
    }
    
	char buf[1024];
    int x, y, z, g;
    double d1, d2, d3, d4, d5, d6, d7, d8;
    
	while(f->getline(buf, sizeof(buf)))
    {

        cnt++;
        sscanf(buf, "%i %i %i %i %lf %lf %lf %lf %lf %lf %lf %lf", &x, &y, &z, &g, &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8);
        if(*voxel)
        {
            solidfaces(lookupcube(ivec(sel.o.x + x, sel.o.y + y, sel.o.z + z), g));
        }
        else
        {
            march_cube mc;
            mc.corners[0] = iso_point(d1);
            mc.corners[1] = iso_point(d2);
            mc.corners[2] = iso_point(d4);
            mc.corners[3] = iso_point(d3);
            mc.corners[4] = iso_point(d5);
            mc.corners[5] = iso_point(d6);
            mc.corners[6] = iso_point(d8);
            mc.corners[7] = iso_point(d7);
            mc.init_cube_index();
            mc.render(lookupcube(ivec(sel.o.x + x, sel.o.y + y, sel.o.z + z), g), shapetex());
        }
    }
    conoutf(colourwhite, "cubes: %i", cnt);
    f->close();
    allchanged();
}
COMMAND(0, importsmc, "si");
