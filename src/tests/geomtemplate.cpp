#include "engine.h"

// Geometry template geometry, see engine/geomtemplate.cpp: the box an entity
// requests, which cubes a template captures, and the instance transform.

static bool nearvec(const vec &a, const vec &b) { return a.dist(b) < 1e-3f; }

static void setents(extentity &e, const vec &o, int n, const int *vals)
{
    e.o = o;
    e.attrs.setsize(0);
    loopi(n) e.attrs.add(vals[i]);
}

static void testbox()
{
    extentity e;
    const int a[] = { 1, 16, 8, 4 };
    setents(e, vec(100, 200, 300), 4, a);
    vec bmin, bmax;
    geomtemplatebox(e, bmin, bmax);
    ASSERT(nearvec(bmin, vec(84, 192, 296)));
    ASSERT(nearvec(bmax, vec(116, 208, 304)));

    const int b[] = { 1, -5, 8, 4 }; // negative half-extents count as 0
    setents(e, vec(100, 200, 300), 4, b);
    geomtemplatebox(e, bmin, bmax);
    ASSERT(bmin.x == 100 && bmax.x == 100);
}

static void testcapture()
{
    // A 64-unit octree: root children are 32, their children 16
    cube *src = newcubes(F_EMPTY);
    src[0].children = newcubes(F_EMPTY);
    cube &a = src[0].children[0]; // (0,0,0) size 16
    cube &b = src[0].children[1]; // (16,0,0) size 16
    solidfaces(a);
    solidfaces(b);
    a.material = MAT_ALPHA;

    // Surface data: a blended face loses its bottom layer, a merged one resets
    newcubeext(a, 4, false);
    memset(a.ext->surfaces, 0, sizeof(a.ext->surfaces));
    a.ext->surfaces[2].numverts = LAYER_BOTTOM;
    a.ext->surfaces[3].numverts = LAYER_TOP|4;
    a.merged = 1<<3;

    // b straddles the box's +x face (16..32 against a box ending at 24)
    cube *dst = newcubes(F_EMPTY);
    ivec capmin, capmax;
    int n = capturegeomtemplate(src, 64, vec(-1, -1, -1), vec(24, 17, 17), dst, capmin, capmax);
    ASSERT(n == 1);
    ASSERT(capmin == ivec(0, 0, 0) && capmax == ivec(16, 16, 16));
    ASSERT(dst[0].children && isentirelysolid(dst[0].children[0]) && isempty(dst[0].children[1]));
    const cube &c = dst[0].children[0];
    ASSERT(c.material == MAT_ALPHA);
    ASSERT(!c.merged);
    ASSERT(c.ext && c.ext->surfaces[2].numverts == LAYER_TOP);
    ASSERT(c.ext->surfaces[3].numverts == LAYER_TOP && c.ext->surfaces[3].verts == 0);
    ASSERT(!c.ext->va && !c.ext->ents && c.ext->tjoints < 0);
    freeocta(dst);

    // An off-grid box takes both
    dst = newcubes(F_EMPTY);
    n = capturegeomtemplate(src, 64, vec(-0.5f, -0.5f, -0.5f), vec(32.5f, 16.5f, 16.5f), dst, capmin, capmax);
    ASSERT(n == 2);
    ASSERT(capmin == ivec(0, 0, 0) && capmax == ivec(32, 16, 16));
    freeocta(dst);

    // Nothing inside
    dst = newcubes(F_EMPTY);
    n = capturegeomtemplate(src, 64, vec(40, 40, 40), vec(50, 50, 50), dst, capmin, capmax);
    ASSERT(n == 0 && capmin.x > capmax.x);
    ASSERT(!dst[0].children && isempty(dst[0]));
    freeocta(dst);

    freeocta(src);
}

static void testinstance()
{
    extentity e;
    matrix4x3 m;

    // yaw 90, scale 200, pivot (10,0,0): +x one unit from the pivot lands 2 along +y
    const int a[] = { 1, 90, 0, 0, 200, 0, 0, 0, 0 };
    setents(e, vec(100, 0, 0), 9, a);
    ASSERT(geominstancescale(e) == 2);
    calcgeominstance(e, vec(10, 0, 0), m);
    ASSERT(nearvec(m.transform(vec(10, 0, 0)), vec(100, 0, 0)));
    ASSERT(nearvec(m.transform(vec(11, 0, 0)), vec(100, 2, 0)));

    // scale 0 means 100
    const int b[] = { 1, 0, 0, 0, 0, 0, 0, 0, 0 };
    setents(e, vec(0, 0, 0), 9, b);
    ASSERT(geominstancescale(e) == 1);

    // pitch and roll keep lengths (uniform scale only)
    const int c[] = { 1, 30, 40, 50, 150, 0, 0, 0, 0 };
    setents(e, vec(5, 6, 7), 9, c);
    calcgeominstance(e, vec(1, 2, 3), m);
    ASSERT(fabs(m.transform(vec(2, 2, 3)).dist(vec(5, 6, 7)) - 1.5f) < 1e-3f);
    ASSERT(fabs(m.transform(vec(1, 2, 4)).dist(vec(5, 6, 7)) - 1.5f) < 1e-3f);

    // Pitch and roll DIRECTION, pinned to literals. With the pivot at the origin and
    // the instance at the origin, local offsets map as follows (Rz(yaw) Rx(pitch)
    // Ry(-roll), each with the sign conventions of vec::rotate_around_*):
    //   pitch 90 alone:  (x, y, z) -> (x, -z, y)    +y goes up, +z goes to -y
    //   roll 90 alone:   (x, y, z) -> (-z, y, x)    +x goes up, +z goes to -x
    //   both:            (x, y, z) -> (-z, -x, y)   roll first, then pitch
    const int p90[] = { 1, 0, 90, 0, 0, 0, 0, 0, 0 };
    setents(e, vec(0, 0, 0), 9, p90);
    calcgeominstance(e, vec(0, 0, 0), m);
    ASSERT(nearvec(m.transform(vec(0, 1, 0)), vec(0, 0, 1)));
    ASSERT(nearvec(m.transform(vec(0, 0, 1)), vec(0, -1, 0)));
    ASSERT(nearvec(m.transform(vec(1, 0, 0)), vec(1, 0, 0)));
    const int r90[] = { 1, 0, 0, 90, 0, 0, 0, 0, 0 };
    setents(e, vec(0, 0, 0), 9, r90);
    calcgeominstance(e, vec(0, 0, 0), m);
    ASSERT(nearvec(m.transform(vec(1, 0, 0)), vec(0, 0, 1)));
    ASSERT(nearvec(m.transform(vec(0, 0, 1)), vec(-1, 0, 0)));
    ASSERT(nearvec(m.transform(vec(0, 1, 0)), vec(0, 1, 0)));
    const int pr90[] = { 1, 0, 90, 90, 0, 0, 0, 0, 0 };
    setents(e, vec(0, 0, 0), 9, pr90);
    calcgeominstance(e, vec(0, 0, 0), m);
    ASSERT(nearvec(m.transform(vec(1, 0, 0)), vec(0, -1, 0)));
    ASSERT(nearvec(m.transform(vec(0, 1, 0)), vec(0, 0, 1)));
    ASSERT(nearvec(m.transform(vec(0, 0, 1)), vec(-1, 0, 0)));

    // ... and the same for every angle, against the orientation BIH::ellipsecollide
    // builds for a mapmodel (bih.cpp): the instance's drawn transform and its
    // collision/raycast transform must agree
    static const int angles[][3] = { { 30, 40, 50 }, { 200, -70, 15 }, { 359, 123, -271 }, { 0, 90, 0 }, { 0, 0, 90 }, { 90, 90, 90 } };
    loopi(sizeof(angles)/sizeof(angles[0]))
    {
        const int f[] = { 1, angles[i][0], angles[i][1], angles[i][2], 0, 0, 0, 0, 0 };
        setents(e, vec(0, 0, 0), 9, f);
        calcgeominstance(e, vec(0, 0, 0), m);
        matrix3 orient;
        orient.identity();
        if(angles[i][0]) orient.rotate_around_z(sincosmod360(angles[i][0]));
        if(angles[i][1]) orient.rotate_around_x(sincosmod360(angles[i][1]));
        if(angles[i][2]) orient.rotate_around_y(sincosmod360(-angles[i][2]));
        const vec probes[] = { vec(1, 0, 0), vec(0, 1, 0), vec(0, 0, 1), vec(3, -5, 7) };
        loopj(sizeof(probes)/sizeof(probes[0])) ASSERT(nearvec(m.transform(probes[j]), orient.transform(probes[j])));
    }

    // bounds of the captured box under yaw 90 about its centre
    const int d[] = { 1, 90, 0, 0, 0, 0, 0, 0, 0 };
    setents(e, vec(100, 100, 100), 9, d);
    calcgeominstance(e, vec(8, 4, 2), m);
    ivec bbmin, bbmax;
    calcgeominstancebb(m, ivec(0, 0, 0), ivec(16, 8, 4), bbmin, bbmax);
    ASSERT(bbmin == ivec(96, 92, 98) && bbmax == ivec(104, 108, 102));
}

void testgeomtemplate()
{
    testbox();
    testcapture();
    testinstance();
    conoutf(colourwhite, "testgeomtemplate: ok");
}
