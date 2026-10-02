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

vector<geomtemplate *> geomtemplates;
static bool geomtemplatesync = true; // template entities changed since the last update

geomtemplate *findgeomtemplate(int id)
{
    loopv(geomtemplates) if(geomtemplates[i]->id == id) return geomtemplates[i];
    return NULL;
}

geomtemplate *geominstancetemplate(const extentity &e)
{
    geomtemplate *t = findgeomtemplate(e.attrs[0]);
    return t && t->vas.length() ? t : NULL;
}

bool geominstancebb(const extentity &e, ivec &bbmin, ivec &bbmax)
{
    geomtemplate *t = geominstancetemplate(e);
    if(!t) return false;
    matrix4x3 m;
    calcgeominstance(e, t->pivot, m);
    calcgeominstancebb(m, t->capmin, t->capmax, bbmin, bbmax);
    return true;
}

int geotemplatestate(int id)
{
    geomtemplate *t = findgeomtemplate(id);
    return t ? t->tris : -1;
}

static void freegeomtemplate(geomtemplate &t)
{
    loopv(t.vas) destroytemplateva(t.vas[i]);
    t.vas.setsize(0);
    DELETEP(t.bih);
    t.verts = t.tris = 0;
}

static void buildgeomtemplate(geomtemplate &t)
{
    freegeomtemplate(t);
    cube *root = newcubes(F_EMPTY);
    capturegeomtemplate(worldroot, worldsize, t.reqmin, t.reqmax, root, t.capmin, t.capmax);
    if(!t.empty())
    {
        // calcmerges() works on worldroot and tells the root level apart by it
        cube *oldroot = worldroot;
        worldroot = root;
        calcmerges();
        worldroot = oldroot;
        buildtemplatevas(root, t.vas);
    }
    freeocta(root);
    loopv(t.vas)
    {
        t.verts += t.vas[i]->verts;
        t.tris += t.vas[i]->tris;
    }
    t.rebuilds++;
    t.dirty = false;
}

// An instance's place in the octree follows its template's pivot and captured
// box, so its instances leave the octree before either changes;
// entitiesinoctanodes() puts them back
static void removegeominstances(int id)
{
    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type == ET_GEOINSTANCE && e.attrs[0] == id && e.flags&EF_OCTA) removeoctaentity(i);
    }
}

// Matches the templates to the geotemplate entities: the lowest index defining
// an id wins
static void syncgeomtemplates()
{
    const vector<extentity *> &ents = entities::getents();
    vector<int> seen;
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_GEOTEMPLATE) continue;
        int id = e.attrs[0];
        geomtemplate *t = findgeomtemplate(id);
        if(seen.find(id) >= 0)
        {
            conoutf(colourred, "Geometry template %d is defined by entities %d and %d, using %d", id, t->ent, i, t->ent);
            continue;
        }
        seen.add(id);
        vec bmin, bmax;
        geomtemplatebox(e, bmin, bmax);
        if(!t)
        {
            removegeominstances(id);
            t = geomtemplates.add(new geomtemplate);
            t->id = id;
        }
        else if(t->ent != i || t->pivot != e.o || t->reqmin != bmin || t->reqmax != bmax)
        {
            removegeominstances(id);
            t->dirty = true;
        }
        t->ent = i;
        t->pivot = e.o;
        t->reqmin = bmin;
        t->reqmax = bmax;
    }
    loopvrev(geomtemplates)
    {
        geomtemplate *t = geomtemplates[i];
        if(seen.find(t->id) >= 0) continue;
        removegeominstances(t->id);
        freegeomtemplate(*t);
        delete t;
        geomtemplates.remove(i);
    }
}

void geomtemplateentschanged() { geomtemplatesync = true; }

bool geomtemplatesdirty()
{
    if(geomtemplatesync) return true;
    loopv(geomtemplates) if(geomtemplates[i]->dirty) return true;
    return false;
}

// A geometry change in bbmin..bbmax (changed()): every template whose
// requested box it touches, grown by one, is rebuilt on the next commit
void markgeomtemplates(const ivec &bbmin, const ivec &bbmax)
{
    loopv(geomtemplates)
    {
        geomtemplate &t = *geomtemplates[i];
        if(t.reqmax.x < bbmin.x - 1 || t.reqmax.y < bbmin.y - 1 || t.reqmax.z < bbmin.z - 1 ||
           t.reqmin.x > bbmax.x + 1 || t.reqmin.y > bbmax.y + 1 || t.reqmin.z > bbmax.z + 1)
            continue;
        t.dirty = true;
    }
}

// Called by allchanged() (rebuildall) and commitchanges(), before
// entitiesinoctanodes(), which re-adds the instances removed here
void updategeomtemplates(bool rebuildall)
{
    if(geomtemplatesync || rebuildall)
    {
        syncgeomtemplates();
        geomtemplatesync = false;
    }
    loopv(geomtemplates)
    {
        geomtemplate &t = *geomtemplates[i];
        if(!t.dirty && !rebuildall) continue;
        removegeominstances(t.id);
        buildgeomtemplate(t);
    }
}

// GL teardown (cleanupva): instances leave the octree while their templates
// still say where they are, then everything is freed and resynced later
void cleargeomtemplates()
{
    loopv(geomtemplates)
    {
        removegeominstances(geomtemplates[i]->id);
        freegeomtemplate(*geomtemplates[i]);
    }
    geomtemplates.deletecontents();
    geomtemplatesync = true;
}

extern void boxs3D(const vec &o, vec s, int g);

// Edit mode: each template's requested box faint, its captured box bright,
// so the cubes a template takes are visible at a glance. Called with the
// entity selection's GL state (ldrnotextureshader, additive blend).
void rendergeomtemplateboxes()
{
    if(!editmode) return;
    loopv(geomtemplates)
    {
        geomtemplate &t = *geomtemplates[i];
        gle::colorub(48, 48, 48);
        boxs3D(t.reqmin, vec(t.reqmax).sub(t.reqmin), 1);
        if(t.empty()) continue;
        gle::colorub(0, 160, 160);
        boxs3D(vec(t.capmin), vec(ivec(t.capmax).sub(t.capmin)), 1);
    }
}

#ifdef DEBUG_UTILS
// "capmin capmax verts tris instances rebuilds" of a template, or "" -- the
// verification surface of tools/harness/geotemplate-selftest.ps1
ICOMMAND(0, geotemplateinfo, "i", (int *id),
{
    if(identflags&IDF_MAP) { result(""); return; }
    geomtemplate *t = findgeomtemplate(*id);
    if(!t) { result(""); return; }
    int instances = 0;
    const vector<extentity *> &ents = entities::getents();
    loopv(ents) if(ents[i]->type == ET_GEOINSTANCE && ents[i]->attrs[0] == *id) instances++;
    ivec cmin = t->empty() ? ivec(0, 0, 0) : t->capmin;
    ivec cmax = t->empty() ? ivec(0, 0, 0) : t->capmax;
    defformatstring(s, "%d %d %d %d %d %d %d %d %d %d", cmin.x, cmin.y, cmin.z, cmax.x, cmax.y, cmax.z, t->verts, t->tris, instances, t->rebuilds);
    result(s);
});

// "minx miny minz maxx maxy maxz" of an instance's world bounds, or ""
ICOMMAND(0, geoinstancebb, "i", (int *idx),
{
    const vector<extentity *> &ents = entities::getents();
    ivec bbmin; // not "ivec bbmin, bbmax": a comma outside parentheses splits the macro arguments
    ivec bbmax;
    if(identflags&IDF_MAP || !ents.inrange(*idx) || ents[*idx]->type != ET_GEOINSTANCE || !geominstancebb(*ents[*idx], bbmin, bbmax)) { result(""); return; }
    defformatstring(s, "%d %d %d %d %d %d", bbmin.x, bbmin.y, bbmin.z, bbmax.x, bbmax.y, bbmax.z);
    result(s);
});
#endif
