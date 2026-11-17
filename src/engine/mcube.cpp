#include "engine.h"
#include "mcube.h"

// adds or carves the Shape over every cube of its box, as cubes of the selection's grid size, the bottom face selected so x, y and z run along the world's
static void buildshape(iso &shape, int detail, bool carve)
{
    block3 b = shape.aabb;
    b.orient = 4;
    loopxyz(b, b.grid, marchshape(c, vec(x, y, z).mul(b.grid), b.grid, shape, detail, carve));
    changed(sel, true);
}

void buildsphere(float *r, int *d, float *ox, float *oy, float *oz, float *sx, float *sy, float *sz, int *carve)
{
    sphere_iso shape = sphere_iso(sel, *r, vec(*ox, *oy, *oz), vec(*sx, *sy, *sz));
    buildshape(shape, *d, *carve != 0);
}
COMMAND(0, buildsphere, "fiffffffi");

void buildhelix(float *r, float *br, int *d, int *carve)
{
    helix_iso shape = helix_iso(sel, *br, *r);
    buildshape(shape, *d, *carve != 0);
}
COMMAND(0, buildhelix, "ffii");

void buildcylinder(float *r, float *h, int *d, float *ox, float *oy, float *oz, int *carve)
{
    cylinder_iso shape = cylinder_iso(sel, *r, *h, vec(*ox, *oy, *oz));
    buildshape(shape, *d, *carve != 0);
}
COMMAND(0, buildcylinder, "ffifffi");

void buildtunnel(float *r, float *h, int *d, float *ox, float *oy, float *oz, int *carve)
{
    tunnel_iso shape = tunnel_iso(sel, *r, *h, vec(*ox, *oy, *oz));
    buildshape(shape, *d, *carve != 0);
}
COMMAND(0, buildtunnel, "ffifffi");

void buildpipebend(float *a, float *b, int *d, float *ox, float *oy, float *oz, int *carve)
{
    pipebend_iso shape = pipebend_iso(sel, *a, *b, vec(*ox, *oy, *oz));
    buildshape(shape, *d, *carve != 0);
}
COMMAND(0, buildpipebend, "ffifffi");

void buildtorus(float *a, float *b, int *d, float *ox, float *oy, float *oz, int *carve)
{
    torus_iso shape = torus_iso(sel, *a, *b, vec(*ox, *oy, *oz));
    buildshape(shape, *d, *carve != 0);
}
COMMAND(0, buildtorus, "ffifffi");

void buildparaboloid(float *r, int *d, float *ox, float *oy, float *oz, int *carve)
{
    paraboloid_iso shape = paraboloid_iso(sel, *r, vec(*ox, *oy, *oz));
    buildshape(shape, *d, *carve != 0);
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
    int detail = 1;
    int *d = &detail;

    const char *fname = findfile(imagename, "r");
    SDL_Surface *surface = IMG_Load(fname);
    if(!surface)
    {
        conoutf(colourred, "Could not load terrain image: %s", fname);
        return;
    }

    terrain_iso shape = terrain_iso(sel, surface, *h);
    buildshape(shape, *d, *carve != 0);
}
COMMAND(0, terrain, "sfi");

void buildoctahedron(float *r, int *d, float *ox, float *oy, float *oz, int *carve)
{
    octahedron_iso shape = octahedron_iso(sel, *r, vec(*ox, *oy, *oz));
    buildshape(shape, *d, *carve != 0);
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
            mc.render(lookupcube(ivec(sel.o.x + x, sel.o.y + y, sel.o.z + z), g));
        }
    }
    conoutf(colourwhite, "cubes: %i", cnt);
    f->close();
    allchanged();
}
COMMAND(0, importsmc, "si");
