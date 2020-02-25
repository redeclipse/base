// world.cpp: core map management stuff

#include "engine.h"

mapz hdr;
VARN(0, mapscale, worldscale, 1, 0, 0);
VARN(0, mapsize, worldsize, 1, 0, 0);
VARN(0, emptymap, _emptymap, 1, 0, 0);

VAR(0, octaentsize, 0, 64, 1024);
VAR(0, entselradius, 0, 2, 10);

static inline void transformbb(const entity &e, vec &center, vec &radius)
{
    if(e.attrs[5] > 0) { float scale = e.attrs[5]/100.0f; center.mul(scale); radius.mul(scale); }
    rotatebb(center, radius, e.attrs[1], e.attrs[2], e.attrs[3]);
}

static inline void mmboundbox(const entity &e, model *m, vec &center, vec &radius)
{
    m->boundbox(center, radius);
    transformbb(e, center, radius);
}

static inline void mmcollisionbox(const entity &e, model *m, vec &center, vec &radius)
{
    m->collisionbox(center, radius);
    transformbb(e, center, radius);
}

static inline void decalboundbox(const entity &e, DecalSlot &s, vec &center, vec &radius)
{
    float size = max(float(e.attrs[4]), 1.0f);
    center = vec(0, s.depth * size/2, 0);
    radius = vec(size/2, s.depth * size/2, size/2);
    rotatebb(center, radius, e.attrs[1], e.attrs[2], e.attrs[3]);
}

bool getentboundingbox(const extentity &e, ivec &o, ivec &r)
{
    switch(e.type)
    {
        case ET_EMPTY:
            return false;
        case ET_DECAL:
            {
                DecalSlot &s = lookupdecalslot(e.attrs[0], false);
                vec center, radius;
                decalboundbox(e, s, center, radius);
                center.add(e.o);
                radius.max(entselradius);
                o = ivec(vec(center).sub(radius));
                r = ivec(vec(center).add(radius).add(1));
                break;
            }
        case ET_MAPMODEL:
            if(model *m = loadmapmodel(e.attrs[0]))
            {
                vec center, radius;
                mmboundbox(e, m, center, radius);
                center.add(e.o);
                radius.max(entselradius);
                o = ivec(vec(center).sub(radius));
                r = ivec(vec(center).add(radius).add(1));
                break;
            }
        // invisible mapmodels use entselradius
        default:
            o = ivec(vec(e.o).sub(entselradius));
            r = ivec(vec(e.o).add(entselradius+1));
            break;
    }
    return true;
}

enum
{
    MODOE_ADD      = 1<<0,
    MODOE_UPDATEBB = 1<<1,
    MODOE_CHANGED  = 1<<2
};

void modifyoctaentity(int flags, int id, extentity &e, cube *c, const ivec &cor, int size, const ivec &bo, const ivec &br, int leafsize, vtxarray *lastva = NULL)
{
    loopoctabox(cor, size, bo, br)
    {
        ivec o(i, cor, size);
        vtxarray *va = c[i].ext && c[i].ext->va ? c[i].ext->va : lastva;
        if(c[i].children != NULL && size > leafsize)
            modifyoctaentity(flags, id, e, c[i].children, o, size>>1, bo, br, leafsize, va);
        else if(flags&MODOE_ADD)
        {
            if(!c[i].ext || !c[i].ext->ents) ext(c[i]).ents = new octaentities(o, size);
            octaentities &oe = *c[i].ext->ents;
            switch(e.type)
            {
                case ET_DECAL:
                    if(va)
                    {
                        va->bbmin.x = -1;
                        if(oe.decals.empty()) va->decals.add(&oe);
                    }
                    oe.decals.add(id);
                    oe.bbmin.min(bo).max(oe.o);
                    oe.bbmax.max(br).min(ivec(oe.o).add(oe.size));
                    break;
                case ET_MAPMODEL:
                    if(loadmapmodel(e.attrs[0]))
                    {
                        if(va)
                        {
                            va->bbmin.x = -1;
                            if(oe.mapmodels.empty()) va->mapmodels.add(&oe);
                        }
                        oe.mapmodels.add(id);
                        oe.bbmin.min(bo).max(oe.o);
                        oe.bbmax.max(br).min(ivec(oe.o).add(oe.size));
                        break;
                    }
                    // invisible mapmodel
                default:
                    oe.other.add(id);
                    break;
            }

        }
        else if(c[i].ext && c[i].ext->ents)
        {
            octaentities &oe = *c[i].ext->ents;
            switch(e.type)
            {
                case ET_DECAL:
                    oe.decals.removeobj(id);
                    if(va)
                    {
                        va->bbmin.x = -1;
                        if(oe.decals.empty()) va->decals.removeobj(&oe);
                    }
                    oe.bbmin = oe.bbmax = oe.o;
                    oe.bbmin.add(oe.size);
                    loopvj(oe.decals)
                    {
                        extentity &e = *entities::getents()[oe.decals[j]];
                        ivec eo, er;
                        if(getentboundingbox(e, eo, er))
                        {
                            oe.bbmin.min(eo);
                            oe.bbmax.max(er);
                        }
                    }
                    oe.bbmin.max(oe.o);
                    oe.bbmax.min(ivec(oe.o).add(oe.size));
                    break;
                case ET_MAPMODEL:
                    if(loadmapmodel(e.attrs[0]))
                    {
                        oe.mapmodels.removeobj(id);
                        if(va)
                        {
                            va->bbmin.x = -1;
                            if(oe.mapmodels.empty()) va->mapmodels.removeobj(&oe);
                        }
                        oe.bbmin = oe.bbmax = oe.o;
                        oe.bbmin.add(oe.size);
                        loopvj(oe.mapmodels)
                        {
                            extentity &e = *entities::getents()[oe.mapmodels[j]];
                            ivec eo, er;
                            if(getentboundingbox(e, eo, er))
                            {
                                oe.bbmin.min(eo);
                                oe.bbmax.max(er);
                            }
                        }
                        oe.bbmin.max(oe.o);
                        oe.bbmax.min(ivec(oe.o).add(oe.size));
                        break;
                    }
                    // invisible mapmodel
                default:
                    oe.other.removeobj(id);
                    break;
            }
            if(oe.mapmodels.empty() && oe.decals.empty() && oe.other.empty())
                freeoctaentities(c[i]);
        }
        if(c[i].ext && c[i].ext->ents) c[i].ext->ents->query = NULL;
        if(va && va!=lastva)
        {
            if(lastva)
            {
                if(va->bbmin.x < 0) lastva->bbmin.x = -1;
            }
            else if(flags&MODOE_UPDATEBB) updatevabb(va);
        }
    }
}

vector<int> outsideents;
int spotlights = 0, volumetriclights = 0, nospeclights = 0;

static bool modifyoctaent(int flags, int id, extentity &e)
{
    if(flags&MODOE_ADD ? e.flags&EF_OCTA : !(e.flags&EF_OCTA)) return false;

    ivec o, r;
    if(!getentboundingbox(e, o, r)) return false;

    if(!insideworld(e.o))
    {
        int idx = outsideents.find(id);
        if(flags&MODOE_ADD)
        {
            if(idx < 0) outsideents.add(id);
        }
        else if(idx >= 0) outsideents.removeunordered(idx);
    }
    else
    {
        int leafsize = octaentsize, limit = max(r.x - o.x, max(r.y - o.y, r.z - o.z));
        while(leafsize < limit) leafsize *= 2;
        int diff = ~(leafsize-1) & ((o.x^r.x)|(o.y^r.y)|(o.z^r.z));
        if(diff && (limit > octaentsize/2 || diff < leafsize*2)) leafsize *= 2;
        modifyoctaentity(flags, id, e, worldroot, ivec(0, 0, 0), worldsize>>1, o, r, leafsize);
    }
    e.flags ^= EF_OCTA;
    switch(e.type)
    {
        case ET_LIGHT:
            clearlightcache(id);
            if(e.attrs[6]&L_VOLUMETRIC) { if(flags&MODOE_ADD) volumetriclights++; else --volumetriclights; }
            if(e.attrs[6]&L_NOSPEC) { if(!(flags&MODOE_ADD ? nospeclights++ : --nospeclights)) cleardeferredlightshaders(); }
            break;
        case ET_LIGHTFX: if(!(flags&MODOE_ADD ? spotlights++ : --spotlights)) { cleardeferredlightshaders(); cleanupvolumetric(); } break;
        case ET_PARTICLES: clearparticleemitters(); break;
        case ET_DECAL: if(flags&MODOE_CHANGED) changed(o, r, false); break;
        case ET_WIND: if(flags&MODOE_ADD) addwind(&e); else remwind(&e); break;
        default: break;
    }
    return true;
}

static inline bool modifyoctaent(int flags, int id)
{
    vector<extentity *> &ents = entities::getents();
    return ents.inrange(id) && modifyoctaent(flags, id, *ents[id]);
}

static inline void addentity(int id)        { modifyoctaent(MODOE_ADD|MODOE_UPDATEBB, id); }
static inline void addentityedit(int id, bool fix = true)
{
    if(fix) entities::fixentity(id, true);
    modifyoctaent(MODOE_ADD|MODOE_UPDATEBB|MODOE_CHANGED, id);
}
static inline void removeentity(int id)     { modifyoctaent(MODOE_UPDATEBB, id); }
static inline void removeentityedit(int id) { modifyoctaent(MODOE_UPDATEBB|MODOE_CHANGED, id); }

void freeoctaentities(cube &c)
{
    if(!c.ext) return;
    if(entities::getents().length())
    {
        while(c.ext->ents && !c.ext->ents->mapmodels.empty()) removeentity(c.ext->ents->mapmodels.pop());
        while(c.ext->ents && !c.ext->ents->decals.empty())    removeentity(c.ext->ents->decals.pop());
        while(c.ext->ents && !c.ext->ents->other.empty())     removeentity(c.ext->ents->other.pop());
    }
    if(c.ext->ents)
    {
        delete c.ext->ents;
        c.ext->ents = NULL;
    }
}

void entitiesinoctanodes()
{
    vector<extentity *> &ents = entities::getents();
    loopv(ents) modifyoctaent(MODOE_ADD, i, *ents[i]);
}

static inline void findents(octaentities &oe, int low, int high, bool notspawned, const vec &pos, const vec &invradius, vector<int> &found)
{
    vector<extentity *> &ents = entities::getents();
    loopv(oe.other)
    {
        int id = oe.other[i];
        extentity &e = *ents[id];
        if(e.type >= low && e.type <= high && (e.spawned() || notspawned) && vec(e.o).sub(pos).mul(invradius).squaredlen() <= 1) found.add(id);
    }
}

static inline void findents(cube *c, const ivec &o, int size, const ivec &bo, const ivec &br, int low, int high, bool notspawned, const vec &pos, const vec &invradius, vector<int> &found)
{
    loopoctabox(o, size, bo, br)
    {
        if(c[i].ext && c[i].ext->ents) findents(*c[i].ext->ents, low, high, notspawned, pos, invradius, found);
        if(c[i].children && size > octaentsize)
        {
            ivec co(i, o, size);
            findents(c[i].children, co, size>>1, bo, br, low, high, notspawned, pos, invradius, found);
        }
    }
}

void findents(int low, int high, bool notspawned, const vec &pos, const vec &radius, vector<int> &found)
{
    vec invradius(1/radius.x, 1/radius.y, 1/radius.z);
    ivec bo(vec(pos).sub(radius).sub(1)),
         br(vec(pos).add(radius).add(1));
    int diff = (bo.x^br.x) | (bo.y^br.y) | (bo.z^br.z) | octaentsize,
        scale = worldscale-1;
    if(diff&~((1<<scale)-1) || uint(bo.x|bo.y|bo.z|br.x|br.y|br.z) >= uint(worldsize))
    {
        findents(worldroot, ivec(0, 0, 0), 1<<scale, bo, br, low, high, notspawned, pos, invradius, found);
        return;
    }
    cube *c = &worldroot[octastep(bo.x, bo.y, bo.z, scale)];
    if(c->ext && c->ext->ents) findents(*c->ext->ents, low, high, notspawned, pos, invradius, found);
    scale--;
    while(c->children && !(diff&(1<<scale)))
    {
        c = &c->children[octastep(bo.x, bo.y, bo.z, scale)];
        if(c->ext && c->ext->ents) findents(*c->ext->ents, low, high, notspawned, pos, invradius, found);
        scale--;
    }
    if(c->children && 1<<scale >= octaentsize) findents(c->children, ivec(bo).mask(~((2<<scale)-1)), 1<<scale, bo, br, low, high, notspawned, pos, invradius, found);
}

char *entname(entity &e)
{
    static string fullentname;
    copystring(fullentname, entities::findname(e.type));
    const char *einfo = entities::entinfo(e);
    if(*einfo)
    {
        concatstring(fullentname, ": ");
        concatstring(fullentname, einfo);
    }
    return fullentname;
}
extern bool havesel, selectcorners;
int entlooplevel = 0;
int efocus = -1, enthover = -1, entorient = -1, oldhover = -1;
bool undonext = true;

VARF(0, entediting, 0, 1, 1, { if(!entediting) { entcancel(); efocus = enthover = -1; } });

bool noentedit()
{
    if(!editmode) { conoutft(CON_DEBUG, "\frOperation only allowed in edit mode"); return true; }
    return !entediting;
}

bool pointinsel(const selinfo &sel, const vec &o)
{
    return(o.x <= sel.o.x+sel.s.x*sel.grid
        && o.x >= sel.o.x
        && o.y <= sel.o.y+sel.s.y*sel.grid
        && o.y >= sel.o.y
        && o.z <= sel.o.z+sel.s.z*sel.grid
        && o.z >= sel.o.z);
}

vector<int> entgroup;

bool haveselent()
{
    return entgroup.length() > 0;
}

void entcancel()
{
    entgroup.shrink(0);
}

void entadd(int id)
{
    undonext = true;
    entgroup.add(id);
}

undoblock *newundoent()
{
    int numents = entgroup.length();
    if(numents <= 0) return NULL;
    vector<extentity *> &ents = entities::getents();
    int numattrs = 0;
    loopv(entgroup) numattrs += ents[entgroup[i]]->attrs.length();
    undoblock *u = (undoblock *)new uchar[sizeof(undoblock) + numents*sizeof(undoent) + numattrs*sizeof(int)];
    u->numents = numents;
    undoent *e = (undoent *)(u + 1);
    int *attr = (int *)(e + numents);
    loopv(entgroup)
    {
        entity &g = *(entity *)ents[entgroup[i]];
        e->i = entgroup[i];
        e->e = g;
        e->numattrs = g.attrs.length();
        loopvj(g.attrs) *attr++ = g.attrs[j];
        e++;
    }
    return u;
}

void makeundoent()
{
    if(!undonext) return;
    undonext = false;
    if(!editmode) return;
    oldhover = enthover;
    undoblock *u = newundoent();
    if(u) addundo(u);
}

// convenience macros implicitly define:
// e         entity, currently edited ent
// n         int,    index to currently edited ent
#define addimplicit(f)    { if(entgroup.empty() && enthover>=0) { entadd(enthover); undonext = (enthover != oldhover); f; entgroup.drop(); } else f; }
#define enteditv(i, f, v) \
{ \
    entfocusv(i, \
    { \
        removeentityedit(n);  \
        f; \
        if(e.type!=ET_EMPTY) addentityedit(n); \
        entities::editent(n, true); \
        clearshadowcache(); \
    }, v); \
}
#define entedit(i, f)   enteditv(i, f, entities::getents())
#define addgroup(exp)   { vector<extentity *> &ents = entities::getents(); loopv(ents) entfocusv(i, if(exp) entadd(n), ents); }
#define setgroup(exp)   { entcancel(); addgroup(exp); }
#define groupeditloop(f){ vector<extentity *> &ents = entities::getents(); entlooplevel++; int _ = efocus; loopv(entgroup) enteditv(entgroup[i], f, ents); efocus = _; entlooplevel--; }
#define groupeditpure(f){ if(entlooplevel>0) { entedit(efocus, f); } else { groupeditloop(f); commitchanges(); } }
#define groupeditundo(f){ makeundoent(); groupeditpure(f); }
#define groupedit(f)    { addimplicit(groupeditundo(f)); }

vec getselpos()
{
    vector<extentity *> &ents = entities::getents();
    if(entgroup.length() && ents.inrange(entgroup[0])) return ents[entgroup[0]]->o;
    if(ents.inrange(enthover)) return ents[enthover]->o;
    return vec(sel.o);
}

undoblock *copyundoents(undoblock *u)
{
    entcancel();
    undoent *e = u->ents();
    loopi(u->numents)
        entadd(e[i].i);
    undoblock *c = newundoent();
    loopi(u->numents) if(e[i].e.type==ET_EMPTY)
        entgroup.removeobj(e[i].i);
    return c;
}

void pasteundoent(int idx, const entbase &ue, int *attrs, int numattrs)
{
    if(idx < 0 || idx >= MAXENTS) return;
    vector<extentity *> &ents = entities::getents();
    while(ents.length() < idx) ents.add(entities::newent())->type = ET_EMPTY;
    numattrs = min(numattrs, MAXENTATTRS);
    int efocus = -1, minattrs = entities::numattrs(ue.type);
    entedit(idx,
    {
        (entbase &)e = ue;
        e.attrs.setsize(max(numattrs, minattrs), 0);
        loopk(numattrs) e.attrs[k] = *attrs++;
        for(int k = numattrs; k < minattrs; k++) e.attrs[k] = 0;
    });
}

void pasteundoents(undoblock *u)
{
    undoent *ue = u->ents();
    int *attrs = u->attrs();
    loopi(u->numents)
    {
        pasteundoent(ue[i].i, ue[i].e, attrs, ue[i].numattrs);
        attrs += ue[i].numattrs;
    }
}

void entflip()
{
    if(noentedit()) return;
    int d = dimension(sel.orient);
    float mid = sel.s[d]*sel.grid/2+sel.o[d];
    groupeditundo(e.o[d] -= (e.o[d]-mid)*2);
}

void entrotate(int *cw)
{
    if(noentedit()) return;
    int d = dimension(sel.orient);
    int dd = (*cw<0) == dimcoord(sel.orient) ? R[d] : C[d];
    float mid = sel.s[dd]*sel.grid/2+sel.o[dd];
    vec s(sel.o.v);
    groupeditundo(
        e.o[dd] -= (e.o[dd]-mid)*2;
        e.o.sub(s);
        swap(e.o[R[d]], e.o[C[d]]);
        e.o.add(s);
    );
}

VAR(IDF_PERSIST, entselmdlbox, 0, 0, 1); // wanted this off in RE
VAR(IDF_PERSIST, entselmapmodelbox, 0, 0, 1);
VAR(IDF_PERSIST, entseldecalbox, 0, 0, 1);
bool entselectionbox(extentity &e, vec &eo, vec &es, bool full)
{
    model *m = NULL;
    const char *mname = entselmdlbox ? entities::entmdlname(e.type, e.attrs) : NULL;
    bool found = false, faked = false;
    if(mname && (m = loadmodel(mname)))
    {
        m->collisionbox(eo, es);
        if(es.x > es.y) es.y = es.x; else es.x = es.y; // square
        es.z = (es.z + eo.z + 1 + entselradius)/2; // enclose ent radius box and model box
        eo.x += e.o.x;
        eo.y += e.o.y;
        eo.z = e.o.z - entselradius + es.z;
        found = true;
    }
    else if(e.type == ET_MAPMODEL && (m = loadmapmodel(e.attrs[0])))
    {
        if(!full && !entselmapmodelbox) faked = true;
        else
        {
            mmcollisionbox(e, m, eo, es);
            es.max(entselradius);
            eo.add(e.o);
            found = true;
        }
    }
    else if(e.type == ET_DECAL)
    {
        if(!full && !entseldecalbox) faked = true;
        else
        {
            DecalSlot &s = lookupdecalslot(e.attrs[0], false);
            decalboundbox(e, s, eo, es);
            es.max(entselradius);
            eo.add(e.o);
            found = true;
        }
    }
    if(!found)
    {
        es = vec(entselradius);
        eo = e.o;
    }
    eo.sub(es);
    es.mul(2);
    return faked;
}

VAR(0, entselsnap, 0, 1, 1);
VAR(0, entmovingshadow, 0, 1, 1);

extern void boxs(int orient, vec o, const vec &s, float size);
extern void boxs(int orient, vec o, const vec &s);
extern void boxs3D(const vec &o, vec s, int g);
extern bool editmoveplane(const vec &o, const vec &ray, int d, float off, vec &handle, vec &dest, bool first);

int entmoving = 0;

void entdrag(const vec &ray)
{
    if(noentedit() || !haveselent()) return;

    float r = 0, c = 0;
    static vec dest, handle;
    vec eo, es;
    int d = dimension(entorient),
        dc= dimcoord(entorient);

    entfocus(entgroup.last(),
        entselectionbox(e, eo, es);

        if(!editmoveplane(e.o, ray, d, eo[d] + (dc ? es[d] : 0), handle, dest, entmoving==1))
            return;

        ivec g(dest);
        int z = g[d]&(~(sel.grid-1));
        g.add(sel.grid/2).mask(~(sel.grid-1));
        g[d] = z;

        r = (entselsnap ? g[R[d]] : dest[R[d]]) - e.o[R[d]];
        c = (entselsnap ? g[C[d]] : dest[C[d]]) - e.o[C[d]];
    );

    if(entmoving==1) makeundoent();
    groupeditpure(e.o[R[d]] += r; e.o[C[d]] += c);
    entmoving = 2;
}

static void renderentbox(const vec &eo, vec es)
{
    es.add(eo);

    // bottom quad
    gle::attrib(eo.x, eo.y, eo.z); gle::attrib(es.x, eo.y, eo.z);
    gle::attrib(es.x, eo.y, eo.z); gle::attrib(es.x, es.y, eo.z);
    gle::attrib(es.x, es.y, eo.z); gle::attrib(eo.x, es.y, eo.z);
    gle::attrib(eo.x, es.y, eo.z); gle::attrib(eo.x, eo.y, eo.z);

    // top quad
    gle::attrib(eo.x, eo.y, es.z); gle::attrib(es.x, eo.y, es.z);
    gle::attrib(es.x, eo.y, es.z); gle::attrib(es.x, es.y, es.z);
    gle::attrib(es.x, es.y, es.z); gle::attrib(eo.x, es.y, es.z);
    gle::attrib(eo.x, es.y, es.z); gle::attrib(eo.x, eo.y, es.z);

    // sides
    gle::attrib(eo.x, eo.y, eo.z); gle::attrib(eo.x, eo.y, es.z);
    gle::attrib(es.x, eo.y, eo.z); gle::attrib(es.x, eo.y, es.z);
    gle::attrib(es.x, es.y, eo.z); gle::attrib(es.x, es.y, es.z);
    gle::attrib(eo.x, es.y, eo.z); gle::attrib(eo.x, es.y, es.z);
}

void renderentselection(const vec &o, const vec &ray, bool entmoving)
{
    if(noentedit() || (entgroup.empty() && enthover < 0)) return;
    vec eo, es;

    vector<int> full;
    if(entgroup.length())
    {
        gle::colorub(0, 128, 0);
        gle::defvertex();
        gle::begin(GL_LINES, entgroup.length()*24);
        loopv(entgroup) entfocus(entgroup[i],
            if(entselectionbox(e, eo, es)) full.add(entgroup[i]);
            renderentbox(eo, es);
        );
        xtraverts += gle::end();
    }

    if(enthover >= 0)
    {
        gle::colorub(0, 128, 0);
        entfocus(enthover,
            if(entselectionbox(e, eo, es)) full.add(enthover);
        ); // also ensures enthover is back in focus
        boxs3D(eo, es, 1);
        if(entmoving && entmovingshadow==1)
        {
            vec a, b;
            gle::colorub(128, 128, 128);
            (a = eo).x = eo.x - fmod(eo.x, worldsize); (b = es).x = a.x + worldsize; boxs3D(a, b, 1);
            (a = eo).y = eo.y - fmod(eo.y, worldsize); (b = es).y = a.x + worldsize; boxs3D(a, b, 1);
            (a = eo).z = eo.z - fmod(eo.z, worldsize); (b = es).z = a.x + worldsize; boxs3D(a, b, 1);
        }
        gle::colorub(200, 0, 0);
        boxs(entorient, eo, es);
        boxs(entorient, eo, es, clamp(0.015f*camera1->o.dist(eo)*tan(fovy*0.5f*RAD), 0.1f, 1.0f));
    }

    if(full.length())
    {
        gle::colorub(0, 128, 128);
        gle::defvertex();
        gle::begin(GL_LINES, full.length()*24);
        loopv(full) entfocus(full[i],
            entselectionbox(e, eo, es, true);
            renderentbox(eo, es);
        );
        xtraverts += gle::end();
    }
}

bool enttoggle(int id)
{
    undonext = true;
    int i = entgroup.find(id);
    if(i < 0)
        entadd(id);
    else
        entgroup.remove(i);
    return i < 0;
}

bool hoveringonent(int ent, int orient)
{
    if(noentedit()) return false;
    entorient = orient;
    if((efocus = enthover = ent) >= 0)
        return true;
    efocus   = entgroup.empty() ? -1 : entgroup.last();
    enthover = -1;
    return false;
}

VAR(0, entitysurf, 0, 0, 1);

ICOMMAND(0, entadd, "", (),
{
    if(enthover >= 0 && !noentedit())
    {
        if(entgroup.find(enthover) < 0) entadd(enthover);
        if(entmoving > 1) entmoving = 1;
    }
});

ICOMMAND(0, enttoggle, "", (),
{
    if(enthover < 0 || noentedit() || !enttoggle(enthover)) { entmoving = 0; intret(0); }
    else { if(entmoving > 1) entmoving = 1; intret(1); }
});

ICOMMAND(0, entmoving, "b", (int *n),
{
    if(*n >= 0)
    {
        if(!*n || enthover < 0 || noentedit()) entmoving = 0;
        else
        {
            if(entgroup.find(enthover) < 0) { entadd(enthover); entmoving = 1; }
            else if(!entmoving) entmoving = 1;
        }
    }
    intret(entmoving);
});

void entpush(int *dir)
{
    if(noentedit()) return;
    int d = dimension(entorient);
    int s = dimcoord(entorient) ? -*dir : *dir;
    if(entmoving)
    {
        groupeditpure(e.o[d] += float(s*sel.grid)); // editdrag supplies the undo
    }
    else
        groupedit(e.o[d] += float(s*sel.grid));
    if(entitysurf==1)
    {
        physent *player = (physent *)game::focusedent(true);
        if(!player) player = camera1;
        player->o[d] += s*sel.grid;
        player->resetinterp(true);
    }
}

VAR(0, entautoviewdist, 0, 25, 100);
void entautoview(int *dir)
{
    if(!haveselent()) return;
    static int s = 0;
    physent *player = (physent *)game::focusedent(true);
    if(!player) player = camera1;
    vec v(player->o);
    v.sub(worldpos);
    v.normalize();
    v.mul(entautoviewdist);
    int t = s + *dir;
    s = abs(t) % entgroup.length();
    if(t<0 && s>0) s = entgroup.length() - s;
    entfocus(entgroup[s],
        v.add(e.o);
        player->o = v;
        player->resetinterp(true);
    );
}

COMMAND(0, entautoview, "i");
COMMAND(0, entflip, "");
COMMAND(0, entrotate, "i");
COMMAND(0, entpush, "i");

void delent()
{
    if(noentedit()) return;
    groupedit(e.type = ET_EMPTY;);
    entcancel();
}
COMMAND(0, delent, "");

VAR(0, entdrop, 0, 2, 3);

void dropenttofloor(extentity *e)
{
    if(!insideworld(e->o)) return;
    vec v(0.0001f, 0.0001f, -1);
    v.normalize();
    if(raycube(e->o, v, worldsize) >= worldsize) return;
    physent d;
    d.type = ENT_CAMERA;
    d.o = e->o;
    d.vel = vec(0, 0, -1);
    d.radius = 1.0f;
    d.height = entities::dropheight(*e);
    d.aboveeye = 1.0f;
    while (!collide(&d, v) && d.o.z > 0.f) d.o.z -= 0.1f;
    e->o = d.o;
}

bool dropentity(extentity &e, int drop = -1)
{
    vec radius(4.0f, 4.0f, 4.0f);
    if(drop<0) drop = entdrop;
    if(e.type == ET_MAPMODEL)
    {
        model *m = loadmapmodel(e.attrs[0]);
        if(m)
        {
            vec center;
            mmboundbox(e, m, center, radius);
            radius.x += fabs(center.x);
            radius.y += fabs(center.y);
        }
        radius.z = 0.0f;
    }
    switch(drop)
    {
    case 1:
        if(e.type != ET_LIGHT && e.type != ET_LIGHTFX)
            dropenttofloor(&e);
        break;
    case 2:
    case 3:
        int cx = 0, cy = 0;
        if(sel.cxs == 1 && sel.cys == 1)
        {
            cx = (sel.cx ? 1 : -1) * sel.grid / 2;
            cy = (sel.cy ? 1 : -1) * sel.grid / 2;
        }
        e.o = vec(sel.o);
        int d = dimension(sel.orient), dc = dimcoord(sel.orient);
        e.o[R[d]] += sel.grid / 2 + cx;
        e.o[C[d]] += sel.grid / 2 + cy;
        if(!dc)
            e.o[D[d]] -= radius[D[d]];
        else
            e.o[D[d]] += sel.grid + radius[D[d]];

        if(drop == 3)
            dropenttofloor(&e);
        break;
    }
    return true;
}

void dropent()
{
    if(noentedit()) return;
    groupedit(dropentity(e));
}
COMMAND(0, dropent, "");

static int keepents = 0;

extentity *newentity(bool local, const vec &o, int type, const attrvector &attrs, int &idx, bool fix = true, bool alter = true)
{
    vector<extentity *> &ents = entities::getents();
    if(local)
    {
        idx = -1;
        for(int i = keepents; i < ents.length(); i++) if(ents[i]->type == ET_EMPTY) { idx = i; break; }
        if(idx < 0 && ents.length() >= MAXENTS) { conoutft(CON_DEBUG, "\frToo many entities"); return NULL; }
    }
    else while(ents.length() < idx) ents.add(entities::newent())->type = ET_EMPTY;
    extentity &e = *entities::newent();
    e.o = o;
    e.type = type;
    e.attrs.write(0, attrs, MAXENTATTRS);
    memset(e.reserved, 0, sizeof(e.reserved));
    if(ents.inrange(idx)) { entities::deleteent(ents[idx]); ents[idx] = &e; }
    else { idx = ents.length(); ents.add(&e); }
    if(local && fix) entities::fixentity(idx, true, true, alter);
    return &e;
}

int newentity(const vec &v, int type, const attrvector &attrs, bool fix, bool alter)
{
    int idx = -1;
    extentity *t = newentity(true, v, type, attrs, idx, fix, alter);
    if(!t) return -1;
    t->type = ET_EMPTY;
    enttoggle(idx);
    makeundoent();
    entedit(idx, e.type = type);
    commitchanges();
    return idx;
}

int newentity(int type, const attrvector &attrs, bool fix, bool alter)
{
    int idx = -1;
    extentity *t = newentity(true, camera1->o, type, attrs, idx, fix, alter);
    if(!t) return -1;
    dropentity(*t);
    t->type = ET_EMPTY;
    enttoggle(idx);
    makeundoent();
    entedit(idx, e.type = type);
    commitchanges();
    return idx;
}

void entattrs(const char *str, attrvector &attrs)
{
    static vector<char *> buf;
    explodelist(str, buf, MAXENTATTRS);
    attrs.setsize(buf.length(), 0);
    loopv(buf) attrs[i] = parseint(buf[i]);
    buf.deletearrays();
}

void newent(char *what, char *attr)
{
    if(noentedit()) return;
    int type = entities::findtype(what);
    attrvector attrs;
    entattrs(attr, attrs);
    if(type != ET_EMPTY) newentity(type, attrs);
}
COMMAND(0, newent, "ss");

int entcopygrid;
vector<entity> entcopybuf;

void entcopy()
{
    if(noentedit()) return;
    entcopygrid = sel.grid;
    entcopybuf.shrink(0);
    addimplicit({
        loopv(entgroup) entfocus(entgroup[i], entcopybuf.add(e).o.sub(vec(sel.o)));
    });
}
COMMAND(0, entcopy, "");

void entpaste()
{
    if(noentedit() || entcopybuf.empty()) return;
    entcancel();
    float m = float(sel.grid)/float(entcopygrid);
    loopv(entcopybuf)
    {
        const entity &c = entcopybuf[i];
        vec o = vec(c.o).mul(m).add(vec(sel.o));
        int idx;
        extentity *e = newentity(true, o, c.type, c.attrs, idx, true, false);
        if(!e) continue;
        entadd(idx);
        keepents = max(keepents, idx+1);
    }
    keepents = 0;
    int j = 0;
    groupeditundo(e.type = entcopybuf[j++].type;);
}
COMMAND(0, entpaste, "");

void entreplace()
{
    if(noentedit() || entcopybuf.empty()) return;
    const entity &c = entcopybuf[0];
    if(entgroup.length() || enthover >= 0)
    {
        groupedit({
            e.type = c.type;
            e.attrs.write(0, c.attrs);
            loopvk(c.links) e.links.add(c.links[k]);
        });
    }
    else newentity(c.type, c.attrs, true, false);
}
COMMAND(0, entreplace, "");

void entset(char *what, char *attr)
{
    if(noentedit()) return;
    int type = entities::findtype(what);
    if(type == ET_EMPTY)
    {
        conoutft(CON_DEBUG, "\frUnknown entity type \"%s\"", what);
        return;
    }
    attrvector attrs;
    entattrs(attr, attrs);
    groupedit({
        e.type = type;
        e.attrs.add(0, clamp(attrs.length(), entities::numattrs(e.type), MAXENTATTRS) - e.attrs.length());
        loopk(min(attrs.length(), e.attrs.length())) e.attrs[k] = attrs[k];
    });
}
COMMAND(0, entset, "ss");

void entlink(int *parent)
{
    if(entgroup.length() > 1)
    {
        const vector<extentity *> &ents = entities::getents();
        int index = entgroup[0];
        if(ents.inrange(index))
        {
            loopi(entgroup.length()-1)
            {
                int node = entgroup[i+1];

                if(verbose >= 2) conoutf("\faAttempting to link %d and %d (%d)", index, node, i+1);
                if(ents.inrange(node))
                {
                    if(!entities::linkents(index, node) && !entities::linkents(node, index))
                        conoutf("\frFailed linking %d and %d (%d)", index, node, i+1);
                }
                else conoutf("\fr%d (%d) is not in range", node, i+1);
                if(!*parent) index = node;
            }
        }
        else conoutf("\fr%d (%d) is not in range", index, 0);
    }
    else conoutft(CON_DEBUG, "\frMore than one entity must be selected to link");
}
COMMAND(0, entlink, "i");

void nearestent()
{
    if(noentedit()) return;
    int closest = -1;
    float closedist = 1e16f;
    vector<extentity *> &ents = entities::getents();
    physent *player = (physent *)game::focusedent(true);
    if(!player) player = camera1;
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type == ET_EMPTY) continue;
        float dist = e.o.dist(player->o);
        if(dist < closedist)
        {
            closest = i;
            closedist = dist;
        }
    }
    if(closest >= 0) entadd(closest);
}
COMMAND(0, nearestent, "");

ICOMMAND(0, enthavesel,"", (), addimplicit(intret(entgroup.length())));
ICOMMAND(0, entselect, "e", (uint *body), if(!noentedit()) addgroup(e.type != ET_EMPTY && entgroup.find(n)<0 && executebool(body)));
ICOMMAND(0, entloop, "e", (uint *body), if(!noentedit()) addimplicit(groupeditloop(((void)e, execute(body)))));
ICOMMAND(0, insel, "", (), entfocus(efocus, intret(pointinsel(sel, e.o))));
ICOMMAND(0, entget, "", (), entfocus(efocus,
{
    defformatstring(s, "%s", entities::findname(e.type));
    loopv(e.attrs)
    {
        defformatstring(str, " %d", e.attrs[i]);
        concatstring(s, str);
    }
    result(s);
}));
ICOMMAND(0, entindex, "", (), intret(efocus));

void enttype(char *type, int *numargs)
{
    if(*numargs >= 1)
    {
        int typeidx = entities::findtype(type);
        if(typeidx != ET_EMPTY) groupedit(e.type = typeidx);
    }
    else entfocus(efocus,
    {
        result(entities::findname(e.type));
    })
}
COMMAND(0, enttype, "sN");

void entattr(int *attr, int *val, int *numargs)
{
    if(*numargs >= 2)
    {
        if(*attr >= 0 && *attr < MAXENTATTRS)
            groupedit({
                e.attrs.add(0, *attr + 1 - e.attrs.length());
                e.attrs[*attr] = *val;
            });
    }
    else entfocus(efocus,
        if(e.attrs.inrange(*attr)) intret(e.attrs[*attr]);
    );
}
COMMAND(0, entattr, "iiN");

void entprop(int *attr, int *val)
{
    if(*attr >= 0 && *attr < MAXENTATTRS)
        groupedit({
            e.attrs.add(0, *attr + 1 - e.attrs.length());
            e.attrs[*attr] += *val;
        });
}
COMMAND(0, entprop, "ii");

int findentity(int type, int index, vector<int> &attr)
{
    const vector<extentity *> &ents = entities::getents();
    for(int i = index; i<ents.length(); i++)
    {
        extentity &e = *ents[i];
        if(e.type==type)
        {
            bool find = true;
            loopvk(attr) if(!e.attrs.inrange(k) || e.attrs[k] != attr[k])
            {
                find = false;
                break;
            }
            if(find) return i;
        }
    }
    loopj(min(index, ents.length()))
    {
        extentity &e = *ents[j];
        if(e.type==type)
        {
            bool find = true;
            loopvk(attr) if(!e.attrs.inrange(k) || e.attrs[k] != attr[k])
            {
                find = false;
                break;
            }
            if(find) return j;
        }
    }
    return -1;
}

void splitocta(cube *c, int size)
{
    if(size <= 0x1000) return;
    loopi(8)
    {
        if(!c[i].children) c[i].children = newcubes(isempty(c[i]) ? F_EMPTY : F_SOLID);
        splitocta(c[i].children, size>>1);
    }
}

void clearworldvars(bool msg)
{
    identflags |= IDF_WORLD;
    enumerate(idents, ident, id,
    {
        if(id.flags&IDF_WORLD && !(id.flags&IDF_SERVER)) // reset world vars
        {
            switch (id.type)
            {
                case ID_VAR: setvar(id.name, id.def.i, true); break;
                case ID_FVAR: setfvar(id.name, id.def.f, true); break;
                case ID_SVAR: setsvar(id.name, id.def.s && *id.def.s ? id.def.s : "", true); break;
                case ID_ALIAS: worldalias(id.name, ""); break;
                default: break;
            }
        }
    });
    if(msg) conoutf("World variables reset");
    identflags &= ~IDF_WORLD;
}

ICOMMAND(0, resetworldvars, "", (), if(editmode || identflags&IDF_WORLD) clearworldvars(true));

void resetmap(bool empty, int variant)
{
    progress(-1, "Resetting map...");
    resetdecals();
    resetmaterials();
    resetmapmodels();
    clearsound();
    resetblendmap();
    clearlights();
    clearpvs();
    clearslots();
    clearparticles();
    clearwindemitters();
    clearstains();
    clearsleep();
    cancelsel();
    pruneundos();
    setsvar("maptext", "", false);
    mapcrc = 0;
    mapvariant = variant;

    entities::clearents();
    outsideents.setsize(0);
    spotlights = 0;
    volumetriclights = 0;
    nospeclights = 0;
    game::resetmap(empty);
}

bool emptymap(int scale, bool force, const char *mname, bool usecfg)    // main empty world creation routine
{
    if(!force && !editmode)
    {
        conoutft(CON_DEBUG, "\frNewmap only allowed in edit mode");
        return false;
    }

    clearworldvars();
    resetmap(!usecfg);
    setnames(mname);
    setvar("mapscale", scale<10 ? 10 : (scale>16 ? 16 : scale), true, false, true);
    setvar("mapsize", 1<<worldscale, true, false, true);
    setvar("emptymap", 1, true, false, true);

    memcpy(hdr.head, "MAPZ", 4);
    hdr.version = MAPVERSION;
    hdr.gamever = server::getver(1);
    hdr.headersize = sizeof(mapz);
    hdr.worldsize = 1<<worldscale;
    hdr.revision = 0;
    hdr.numpvs = 0;
    hdr.blendmap = 0;
    copystring(hdr.gameid, server::gameid(), 4);

    texmru.shrink(0);
    freeocta(worldroot);
    worldroot = newcubes(F_EMPTY);
    loopi(4) solidfaces(worldroot[i]);

    if(hdr.worldsize > 0x1000) splitocta(worldroot, hdr.worldsize>>1);

    if(usecfg)
    {
        identflags |= IDF_WORLD;
        execfile("config/map/default.cfg");
        identflags &= ~IDF_WORLD;
    }

    allchanged(initing != INIT_QUIT);
    entities::initents(hdr.version, hdr.gameid, hdr.gamever);
    game::startmap(true);

    return true;
}

bool enlargemap(bool split, bool force)
{
    if(!force && !editmode)
    {
        conoutft(CON_DEBUG, "\frMapenlarge only allowed in edit mode");
        return false;
    }
    if(worldsize >= 1<<16) return false;
    while(outsideents.length()) removeentity(outsideents.pop());

    worldscale++;
    worldsize *= 2;
    cube *c = newcubes(F_EMPTY);
    c[0].children = worldroot;
    loopi(3)
    {
        if(split)
        {
            cube *n = newcubes(F_EMPTY);
            loopk(4) solidfaces(n[k]);
            c[i+1].children = n;
        }
        else solidfaces(c[i+1]);
    }
    worldroot = c;

    if(worldsize > 0x1000) splitocta(worldroot, worldsize>>1);

    enlargeblendmap();

    allchanged();

    return true;
}

static bool isallempty(cube &c)
{
    if(!c.children) return isempty(c);
    loopi(8) if(!isallempty(c.children[i])) return false;
    return true;
}

void shrinkmap()
{
    if(noedit(true) || (nompedit && multiplayer())) return;
    if(worldsize <= 1<<10) return;

    int octant = -1;
    loopi(8) if(!isallempty(worldroot[i]))
    {
        if(octant >= 0) return;
        octant = i;
    }
    if(octant < 0) return;

    while(outsideents.length()) removeentity(outsideents.pop());

    if(!worldroot[octant].children) subdividecube(worldroot[octant], false, false);
    cube *root = worldroot[octant].children;
    worldroot[octant].children = NULL;
    freeocta(worldroot);
    worldroot = root;
    worldscale--;
    worldsize /= 2;

    ivec offset(octant, ivec(0, 0, 0), worldsize);
    vector<extentity *> &ents = entities::getents();
    loopv(ents) ents[i]->o.sub(vec(offset));

    shrinkblendmap(octant);

    allchanged();

    conoutf("Shrunk map to size %d", worldscale);
}

ICOMMAND(0, newmap, "is", (int *i, char *n), if(emptymap(*i, false, n)) game::newmap(::max(*i, 0), n));
ICOMMAND(0, mapenlarge, "i", (int *n), if(enlargemap(*n!=0, false)) game::newmap(*n!=0 ? -2 : -1));
COMMAND(0, shrinkmap, "");
ICOMMAND(0, mapsize, "", (void),
{
    int size = 0;
    while(1<<size < worldsize) size++;
    intret(size);
});

void mpeditent(int i, const vec &o, int type, attrvector &attr, bool local)
{
    if(i < 0 || i >= MAXENTS) return;
    vector<extentity *> &ents = entities::getents();
    if(ents.length()<=i)
    {
        extentity *e = newentity(local, o, type, attr, i, false, false);
        if(!e) return;
        addentityedit(i);
    }
    else
    {
        extentity &e = *ents[i];
        removeentityedit(i);
        e.type = type;
        e.o = o;
        e.attrs.add(0, clamp(attr.length(), entities::numattrs(e.type), MAXENTATTRS) - e.attrs.length());
        loopk(min(attr.length(), e.attrs.length())) e.attrs[k] = attr[k];
        addentityedit(i);
    }
    entities::editent(i, local);
    clearshadowcache();
    commitchanges();
}
