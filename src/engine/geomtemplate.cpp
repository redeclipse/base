// geomtemplate.cpp: geometry templates -- the octree cubes inside a geotemplate
// entity's box, built into their own vertex arrays and drawn as hardware
// instances by geoinstance entities.
// Design: docs/superpowers/specs/2026-10-02-geometry-templates-design.md

#include "engine.h"

// The box a geotemplate requests: centred on the entity, half-extents in
// attributes 1-3 (as soundenv and physics zones). The entity is the pivot.
void geomtemplatebox(const extentity &e, vec &bmin, vec &bmax)
{
    vec half(max(e.attrs[1], 0), max(e.attrs[2], 0), max(e.attrs[3], 0));
    bmin = vec(e.o).sub(half);
    bmax = vec(e.o).add(half);
}

static inline bool cubeinsidebox(const ivec &o, int size, const vec &bmin, const vec &bmax)
{
    return o.x >= bmin.x && o.y >= bmin.y && o.z >= bmin.z &&
           o.x + size <= bmax.x && o.y + size <= bmax.y && o.z + size <= bmax.z;
}

static inline bool cubeoverlapsbox(const ivec &o, int size, const vec &bmin, const vec &bmax)
{
    return o.x < bmax.x && o.y < bmax.y && o.z < bmax.z &&
           o.x + size > bmin.x && o.y + size > bmin.y && o.z + size > bmin.z;
}

// The cube of `size` at `o` in the octree `root` (whose children are
// rootsize/2), subdividing empty space on the way down
static cube &makecube(cube *root, int rootsize, const ivec &o, int size)
{
    int scale = 0;
    while(1<<scale < rootsize) scale++;
    cube *c = root;
    for(scale--;; scale--)
    {
        cube &cur = c[octastep(o.x, o.y, o.z, scale)];
        if(1<<scale == size) return cur;
        if(!cur.children) cur.children = newcubes(F_EMPTY);
        c = cur.children;
    }
}

// A leaf of the source as a template cube: shape, textures and surfaces (which
// carry smoothed normals), never its vertex array, entities or t-joints.
// Merged faces are reset, as clearmerge() does: a merge can reach past the
// box, and the template's own octree is re-merged. Blended faces keep their
// top layer only, the blendmap being world-locked. Alpha stays alpha, so it
// stays out of the opaque range; other materials are dropped.
static void copytemplatecube(const cube &src, cube &dst)
{
    dst.children = NULL;
    dst.ext = NULL;
    memcpy(dst.faces, src.faces, sizeof(dst.faces));
    memcpy(dst.texture, src.texture, sizeof(dst.texture));
    dst.material = src.material&MAT_ALPHA;
    dst.merged = 0;
    dst.visible = 0;
    if(!src.ext) return;
    cubeext *ext = newcubeext(dst, src.ext->maxverts, false);
    memcpy(ext->surfaces, src.ext->surfaces, sizeof(ext->surfaces));
    memcpy(ext->verts(), src.ext->verts(), src.ext->maxverts*sizeof(vertinfo));
    loopi(6)
    {
        surfaceinfo &surf = ext->surfaces[i];
        if(src.merged&(1<<i)) surf = brightsurface;
        else if(surf.numverts&LAYER_BOTTOM) surf.numverts = (surf.numverts&~LAYER_BLEND)|LAYER_TOP;
    }
}

static int capturecubes(cube *c, const ivec &co, int size, const vec &bmin, const vec &bmax, cube *dst, int rootsize, ivec &capmin, ivec &capmax)
{
    int count = 0;
    loopi(8)
    {
        ivec o(i, co, size);
        if(!cubeoverlapsbox(o, size, bmin, bmax)) continue;
        if(c[i].children) count += capturecubes(c[i].children, o, size>>1, bmin, bmax, dst, rootsize, capmin, capmax);
        else if(!isempty(c[i]) && cubeinsidebox(o, size, bmin, bmax))
        {
            copytemplatecube(c[i], makecube(dst, rootsize, o, size));
            capmin.min(o);
            capmax.max(ivec(o).add(size));
            count++;
        }
    }
    return count;
}

// Copies every non-empty leaf of `src` lying fully inside the box into `dst`
// (an empty octree of the same size) at the same coordinates. Returns the
// number of leaves; capmin/capmax are their union, or (1,1,1)/(0,0,0).
int capturegeomtemplate(cube *src, int rootsize, const vec &bmin, const vec &bmax, cube *dst, ivec &capmin, ivec &capmax)
{
    capmin = ivec(INT_MAX, INT_MAX, INT_MAX);
    capmax = ivec(INT_MIN, INT_MIN, INT_MIN);
    int count = capturecubes(src, ivec(0, 0, 0), rootsize>>1, bmin, bmax, dst, rootsize, capmin, capmax);
    if(!count)
    {
        capmin = ivec(1, 1, 1);
        capmax = ivec(0, 0, 0);
    }
    return count;
}

float geominstancescale(const extentity &e) { return e.attrs[4] > 0 ? e.attrs[4]/100.0f : 1.0f; }

// T(e.o) Rz(yaw) Rx(pitch) Ry(-roll) S(scale) T(-pivot): the orientation
// BIH::ellipsecollide gives a mapmodel, about the template's pivot
void calcgeominstance(const extentity &e, const vec &pivot, matrix4x3 &m)
{
    m.identity();
    if(e.attrs[1]) m.rotate_around_z(sincosmod360(e.attrs[1]));
    if(e.attrs[2]) m.rotate_around_x(sincosmod360(e.attrs[2]));
    if(e.attrs[3]) m.rotate_around_y(sincosmod360(-e.attrs[3]));
    float scale = geominstancescale(e);
    if(scale != 1) m.scale(scale);
    m.settranslation(e.o);
    m.translate(vec(pivot).neg());
}

// The world box around the 8 transformed corners of a captured box
void calcgeominstancebb(const matrix4x3 &m, const ivec &capmin, const ivec &capmax, ivec &bbmin, ivec &bbmax)
{
    vec lo(1e16f, 1e16f, 1e16f), hi(-1e16f, -1e16f, -1e16f);
    loopi(8)
    {
        vec corner(i&1 ? capmax.x : capmin.x, i&2 ? capmax.y : capmin.y, i&4 ? capmax.z : capmin.z);
        vec p = m.transform(corner);
        lo.min(p);
        hi.max(p);
    }
    bbmin = ivec::floor(lo);
    bbmax = ivec::ceil(hi);
}
