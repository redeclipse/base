#include "engine.h"
#include "mcube.h"

//- select bottom face for loopxyz, so that x y z is correct x y z [cube] in block3, change sel
#define finish_geom(iso_geom)                                                                                \
    iso_geom->aabb.orient = 4;                                                                               \
    loopxyz(iso_geom->aabb, iso_geom->aabb.grid, march_cube(c, x, y, z, iso_geom->aabb.grid, iso_geom, *d)); \
    changed(sel, true);

struct collision_field
{
    vector<shape_box> shapes;

    int collide(ivec o, ivec e)
    {
        loopv(shapes) if(shapes[i].collide(o, e)) return 1;
        return 0;
    }

    void addbox(ivec o, ivec e)
    {
        shapes.add(shape_box(o, e));
    }
};

void buildsphere(float *r, int *d, float *ox, float *oy, float *oz, float *sx, float *sy, float *sz)
{
    // iso* iso_geom = &sphere_iso(sel,*r,vec(*ox,*oy,*oz),vec(*sx,*sy,*sz));
    sphere_iso temp_geom = sphere_iso(sel, *r, vec(*ox, *oy, *oz), vec(*sx, *sy, *sz));
    iso *iso_geom = (iso *)&temp_geom;
    finish_geom(iso_geom);
}
COMMAND(0, buildsphere, "fiffffff");

void buildhelix(float *r, float *br, int *d)
{
    // iso* iso_geom = &helix_iso(sel,*br,*r);
    helix_iso temp_geom = helix_iso(sel, *br, *r);
    iso *iso_geom = (iso *)&temp_geom;
    finish_geom(iso_geom);
}
COMMAND(0, buildhelix, "ffi");

void buildcylinder(float *r, float *h, int *d, float *ox, float *oy, float *oz)
{
    cylinder_iso temp_geom = cylinder_iso(sel, *r, *h, vec(*ox, *oy, *oz));
    iso *iso_geom = (iso *)&temp_geom;
    finish_geom(iso_geom);
}
COMMAND(0, buildcylinder, "ffifff");

void buildtunnel(float *r, float *h, int *d, float *ox, float *oy, float *oz)
{
    tunnel_iso temp_geom = tunnel_iso(sel, *r, *h, vec(*ox, *oy, *oz));
    iso *iso_geom = (iso *)&temp_geom;
    finish_geom(iso_geom);
}
COMMAND(0, buildtunnel, "ffifff");

void buildpipebend(float *a, float *b, int *d, float *ox, float *oy, float *oz)
{
    pipebend_iso temp_geom = pipebend_iso(sel, *a, *b, vec(*ox, *oy, *oz));
    iso *iso_geom = (iso *)&temp_geom;
    finish_geom(iso_geom);
}
COMMAND(0, buildpipebend, "ffifff");

void buildtorus(float *a, float *b, int *d, float *ox, float *oy, float *oz)
{
    torus_iso temp_geom = torus_iso(sel, *a, *b, vec(*ox, *oy, *oz));
    iso *iso_geom = (iso *)&temp_geom;
    finish_geom(iso_geom);
}
COMMAND(0, buildtorus, "ffifff");

void buildparaboloid(float *r, int *d, float *ox, float *oy, float *oz)
{
    paraboloid_iso temp_geom = paraboloid_iso(sel, *r, vec(*ox, *oy, *oz));
    iso *iso_geom = (iso *)&temp_geom;
    finish_geom(iso_geom);
}
COMMAND(0, buildparaboloid, "fifff");

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
void terrain(char *imagename, float *h)
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

    terrain_iso temp_geom = terrain_iso(sel, surface, *h);
    iso *iso_geom = (iso *)&temp_geom;
    finish_geom(iso_geom);
}
COMMAND(0, terrain, "sf");

void buildoctahedron(float *r, int *d, float *ox, float *oy, float *oz)
{
    octahedron_iso temp_geom = octahedron_iso(sel, *r, vec(*ox, *oy, *oz));
    iso *iso_geom = (iso *)&temp_geom;
    finish_geom(iso_geom);
}
COMMAND(0, buildoctahedron, "fifff");

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
    march_cube *mc;
    
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
            mc = new march_cube(lookupcube(ivec(sel.o.x + x, sel.o.y + y, sel.o.z + z), g));
            mc->corners[0] = iso_point(d1);
            mc->corners[1] = iso_point(d2);
            mc->corners[2] = iso_point(d4);
            mc->corners[3] = iso_point(d3);
            mc->corners[4] = iso_point(d5);
            mc->corners[5] = iso_point(d6);
            mc->corners[6] = iso_point(d8);
            mc->corners[7] = iso_point(d7);

            mc->init_cube_index();
            mc->render();
        }
    }
    conoutf(colourwhite, "cubes: %i", cnt);
    f->close();
    allchanged();
}
COMMAND(0, importsmc, "si");

//-- FOR DEBUG
void buildci(unsigned int *ci)
{
    sel.orient = 4; // select bottom face for loopxyz, so that x y z is correct x y z [cube] in block3
    loopxyz(sel, sel.grid, march_cube(c, *ci));
    changed(sel, true);
}
COMMAND(0, buildci, "i");

void buildallci()
{
    sel.s = ivec(51, 5, 1);
    sel.orient = 4; // select bottom face for loopxyz, so that x y z is correct x y z [cube] in block3
    unsigned int ci = 0;
    loopxyz(sel, sel.grid, march_cube(c, ci); ci++;);
    changed(sel, true);
}
COMMAND(0, buildallci, "");

void buildallcisorted()
{
    int sy = sel.o.y;
    loop(x, 22)
    {
        sel.o.y = sy;
        buildci(&cube_index_base_cases[x]);
        for(unsigned int ci = 0; ci < 255; ci++)
        {
            if(cube_index_cases[ci] == cube_index_base_cases[x] && ci != cube_index_base_cases[x])
            {
                sel.o.y += sel.grid;
                buildci(&ci);
            }
        }
        sel.o.x += sel.grid;
    }
}
COMMAND(0, buildallcisorted, "");

void getci()
{
    uchar nce[12];
    bool equal = true;

    loopxyz(sel, sel.grid, loop(ci, 255) {
        loop(e, 12) { nce[e] = c.edges[e]; }
        
        march_cube(c,ci);
        equal = true;
        
        loop(e, 12) { if(c.edges[e] != nce[e]) { equal = false; } }
        loop(e, 12) { c.edges[e] = nce[e]; }
        
        if(equal)
        {
            conoutf(colourwhite, "ci: %i[%i]", ci, cube_index_cases[ci]);
            ci = 255;
        }
    });

    changed(sel, true);
}
COMMAND(0, getci, "");

void mgetline(char *name)
{
    int l = 1;
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
        sscanf(buf, "%i %i %i %i %lf %lf %lf %lf %lf %lf %lf %lf", &x, &y, &z, &g, &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8);

        if(x == sel.o.x && y == sel.o.y && z == sel.o.z)
            conoutf(colourwhite, "%i: %lf %lf %lf %lf %lf %lf %lf %lf", l, d1, d2, d3, d4, d5, d6, d7, d8);

        l++;
    }
    f->close();
}
COMMAND(0, mgetline, "");

//- FOR DEBUG, end
