#include "engine.h"

struct normalkey
{
    vec pos;
    int smooth;
};

static inline uint hthash(const normalkey &k) { return hthash(k.pos); }

struct normalgroup
{
    vec pos;
    int smooth, flat, normals, tnormals;

    normalgroup() : smooth(0), flat(0), normals(-1), tnormals(-1) {}
    normalgroup(const normalkey &key) : pos(key.pos), smooth(key.smooth), flat(0), normals(-1), tnormals(-1) {}
};

static inline bool htcmp(const normalkey &k, const normalgroup &n) { return k.pos == n.pos && k.smooth == n.smooth; }

struct normal
{
    int next;
    vec surface;
};

struct tnormal
{
    int next;
    float offset;
    int normals[2];
    normalgroup *groups[2];
};

hashset<normalgroup> normalgroups(1<<16);
vector<normal> normals;
vector<tnormal> tnormals;
vector<int> smoothgroups;

VAR(IDF_WORLD, lerpangle, 0, 44, 180);

static bool usetnormals = true;

static int addnormal(const vec &pos, int smooth, const vec &surface)
{
    normalkey key = { pos, smooth };
    normalgroup &g = normalgroups.access(key, key);
    normal &n = normals.add();
    n.next = g.normals;
    n.surface = surface;
    return g.normals = normals.length()-1;
}

static void addtnormal(const vec &pos, int smooth, float offset, int normal1, int normal2, const vec &pos1, const vec &pos2)
{
    normalkey key = { pos, smooth };
    normalgroup &g = normalgroups.access(key, key);
    tnormal &n = tnormals.add();
    n.next = g.tnormals;
    n.offset = offset;
    n.normals[0] = normal1;
    n.normals[1] = normal2;
    normalkey key1 = { pos1, smooth }, key2 = { pos2, smooth };
    n.groups[0] = normalgroups.access(key1);
    n.groups[1] = normalgroups.access(key2);
    g.tnormals = tnormals.length()-1;
}

static int addnormal(const vec &pos, int smooth, int axis)
{
    normalkey key = { pos, smooth };
    normalgroup &g = normalgroups.access(key, key);
    g.flat += 1<<(4*axis);
    return axis - 6;
}

static inline void findnormal(const normalgroup &g, float lerpthreshold, const vec &surface, vec &v)
{
    v = vec(0, 0, 0);
    int total = 0;
    if(surface.x >= lerpthreshold) { int n = (g.flat>>4)&0xF; v.x += n; total += n; }
    else if(surface.x <= -lerpthreshold) { int n = g.flat&0xF; v.x -= n; total += n; }
    if(surface.y >= lerpthreshold) { int n = (g.flat>>12)&0xF; v.y += n; total += n; }
    else if(surface.y <= -lerpthreshold) { int n = (g.flat>>8)&0xF; v.y -= n; total += n; }
    if(surface.z >= lerpthreshold) { int n = (g.flat>>20)&0xF; v.z += n; total += n; }
    else if(surface.z <= -lerpthreshold) { int n = (g.flat>>16)&0xF; v.z -= n; total += n; }
    for(int cur = g.normals; cur >= 0;)
    {
        normal &o = normals[cur];
        if(o.surface.dot(surface) >= lerpthreshold)
        {
            v.add(o.surface);
            total++;
        }
        cur = o.next;
    }
    if(total > 1) v.normalize();
    else if(!total) v = surface;
}

static inline bool findtnormal(const normalgroup &g, float lerpthreshold, const vec &surface, vec &v)
{
    float bestangle = lerpthreshold;
    tnormal *bestnorm = NULL;
    for(int cur = g.tnormals; cur >= 0;)
    {
        tnormal &o = tnormals[cur];
        static const vec flats[6] = { vec(-1, 0, 0), vec(1, 0, 0), vec(0, -1, 0), vec(0, 1, 0), vec(0, 0, -1), vec(0, 0, 1) };
        vec n1 = o.normals[0] < 0 ? flats[o.normals[0]+6] : normals[o.normals[0]].surface,
            n2 = o.normals[1] < 0 ? flats[o.normals[1]+6] : normals[o.normals[1]].surface,
            nt;
        nt.lerp(n1, n2, o.offset).normalize();
        float tangle = nt.dot(surface);
        if(tangle >= bestangle)
        {
            bestangle = tangle;
            bestnorm = &o;
        }
        cur = o.next;
    }
    if(!bestnorm) return false;
    vec n1, n2;
    findnormal(*bestnorm->groups[0], lerpthreshold, surface, n1);
    findnormal(*bestnorm->groups[1], lerpthreshold, surface, n2);
    v.lerp(n1, n2, bestnorm->offset).normalize();
    return true;
}

void findnormal(const vec &pos, int smooth, const vec &surface, vec &v)
{
    normalkey key = { pos, smooth };
    const normalgroup *g = normalgroups.access(key);
    if(g)
    {
        int angle = smoothgroups.inrange(smooth) && smoothgroups[smooth] >= 0 ? smoothgroups[smooth] : lerpangle;
        float lerpthreshold = cos360(angle) - 1e-5f;
        if(g->tnormals < 0 || !findtnormal(*g, lerpthreshold, surface, v))
            findnormal(*g, lerpthreshold, surface, v);
    }
    else v = surface;
}

VAR(IDF_WORLD, lerpsubdiv, 0, 2, 4);
VAR(IDF_WORLD, lerpsubdivsize, 4, 4, 128);

static uint normalprogress = 0;

void show_addnormals_progress()
{
    float bar1 = float(normalprogress) / float(allocnodes);
    progress(bar1, "Computing normals...");
}

void addnormals(cube &c, const ivec &o, int size)
{
    CHECK_CALCLIGHT_PROGRESS(return, show_addnormals_progress);

    if(c.children)
    {
        normalprogress++;
        size >>= 1;
        loopi(8) addnormals(c.children[i], ivec(i, o, size), size);
        return;
    }
    else if(isempty(c)) return;

    vec pos[MAXFACEVERTS];
    int norms[MAXFACEVERTS];
    int tj = usetnormals && c.ext ? c.ext->tjoints : -1, vis;
    loopi(6) if((vis = visibletris(c, i, o, size)))
    {
        CHECK_CALCLIGHT_PROGRESS(return, show_addnormals_progress);
        if(c.texture[i] == DEFAULT_SKY) continue;

        vec planes[2];
        int numverts = c.ext ? c.ext->surfaces[i].numverts&MAXFACEVERTS : 0, convex = 0, numplanes = 0;
        if(numverts)
        {
            vertinfo *verts = c.ext->verts() + c.ext->surfaces[i].verts;
            vec vo(ivec(o).mask(~0xFFF));
            loopj(numverts)
            {
                vertinfo &v = verts[j];
                pos[j] = vec(v.x, v.y, v.z).mul(1.0f/8).add(vo);
            }
            if(!(c.merged&(1<<i)) && !flataxisface(c, i)) convex = faceconvexity(verts, numverts, size);
        }
        else if(c.merged&(1<<i)) continue;
        else
        {
            ivec v[4];
            genfaceverts(c, i, v);
            if(!flataxisface(c, i)) convex = faceconvexity(v);
            int order = vis&4 || convex < 0 ? 1 : 0;
            vec vo(o);
            pos[numverts++] = vec(v[order]).mul(size/8.0f).add(vo);
            if(vis&1) pos[numverts++] = vec(v[order+1]).mul(size/8.0f).add(vo);
            pos[numverts++] = vec(v[order+2]).mul(size/8.0f).add(vo);
            if(vis&2) pos[numverts++] = vec(v[(order+3)&3]).mul(size/8.0f).add(vo);
        }

        if(!flataxisface(c, i))
        {
            planes[numplanes++].cross(pos[0], pos[1], pos[2]).normalize();
            if(convex) planes[numplanes++].cross(pos[0], pos[2], pos[3]).normalize();
        }

        VSlot &vslot = lookupvslot(c.texture[i], false);
        int smooth = vslot.slot->smooth;

        if(!numplanes) loopk(numverts) norms[k] = addnormal(pos[k], smooth, i);
        else if(numplanes==1) loopk(numverts) norms[k] = addnormal(pos[k], smooth, planes[0]);
        else
        {
            vec avg = vec(planes[0]).add(planes[1]).normalize();
            norms[0] = addnormal(pos[0], smooth, avg);
            norms[1] = addnormal(pos[1], smooth, planes[0]);
            norms[2] = addnormal(pos[2], smooth, avg);
            for(int k = 3; k < numverts; k++) norms[k] = addnormal(pos[k], smooth, planes[1]);
        }

        while(tj >= 0 && tjoints[tj].edge < i*(MAXFACEVERTS+1)) tj = tjoints[tj].next;
        while(tj >= 0 && tjoints[tj].edge < (i+1)*(MAXFACEVERTS+1))
        {
            int edge = tjoints[tj].edge, e1 = edge%(MAXFACEVERTS+1), e2 = (e1+1)%numverts;
            const vec &v1 = pos[e1], &v2 = pos[e2];
            ivec d(vec(v2).sub(v1).mul(8));
            int axis = abs(d.x) > abs(d.y) ? (abs(d.x) > abs(d.z) ? 0 : 2) : (abs(d.y) > abs(d.z) ? 1 : 2);
            if(d[axis] < 0) d.neg();
            reduceslope(d);
            int origin = int(min(v1[axis], v2[axis])*8)&~0x7FFF,
                offset1 = (int(v1[axis]*8) - origin) / d[axis],
                offset2 = (int(v2[axis]*8) - origin) / d[axis];
            vec o = vec(v1).sub(vec(d).mul(offset1/8.0f)), n1, n2;
            float doffset = 1.0f / (offset2 - offset1);

            while(tj >= 0)
            {
                tjoint &t = tjoints[tj];
                if(t.edge != edge) break;
                float offset = (t.offset - offset1) * doffset;
                vec tpos = vec(d).mul(t.offset/8.0f).add(o);
                addtnormal(tpos, smooth, offset, norms[e1], norms[e2], v1, v2);
                tj = t.next;
            }
        }
    }
}

void calcnormals(bool lerptjoints)
{
    usetnormals = lerptjoints;
    if(usetnormals) findtjoints();
    normalprogress = 1;
    loopi(8) addnormals(worldroot[i], ivec(i, ivec(0, 0, 0), worldsize/2), worldsize/2);
}

void clearnormals()
{
    normalgroups.clear();
    normals.setsize(0);
    tnormals.setsize(0);
}

void resetsmoothgroups()
{
    smoothgroups.setsize(0);
}

int smoothangle(int id, int angle)
{
    if(id < 0) id = smoothgroups.length();
    if(id >= 10000) return -1;
    while(smoothgroups.length() <= id) smoothgroups.add(-1);
    if(angle >= 0) smoothgroups[id] = min(angle, 180);
    return id;
}

ICOMMAND(0, smoothangle, "ib", (int *id, int *angle), intret(smoothangle(*id, *angle)));

