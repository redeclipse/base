#include "engine.h"

editinfo *localedit = NULL;

VAR(0, showpastegrid, 0, 0, 1);
VAR(0, showcursorgrid, 0, 0, 1);
VAR(0, showselgrid, 0, 0, 1);

bool boxoutline = false;

void boxs(int orient, vec o, const vec &s)
{
    int d = dimension(orient), dc = dimcoord(orient);
    float f = boxoutline ? (dc>0 ? 0.2f : -0.2f) : 0;
    o[D[d]] += dc * s[D[d]] + f;

    glBegin(GL_LINE_LOOP);

    glVertex3fv(o.v); o[R[d]] += s[R[d]];
    glVertex3fv(o.v); o[C[d]] += s[C[d]];
    glVertex3fv(o.v); o[R[d]] -= s[R[d]];
    glVertex3fv(o.v);

    glEnd();
    xtraverts += 4;
}

void boxs3D(const vec &o, vec s, int g)
{
    s.mul(g);
    loopi(6)
        boxs(i, o, s);
}

void boxsgrid(int orient, vec o, vec s, int g)
{
    int d = dimension(orient), dc = dimcoord(orient);
    float ox = o[R[d]],
          oy = o[C[d]],
          xs = s[R[d]],
          ys = s[C[d]],
          f = boxoutline ? (dc>0 ? 0.2f : -0.2f) : 0;

    o[D[d]] += dc * s[D[d]]*g + f;

    glBegin(GL_LINES);
    loop(x, xs) {
        o[R[d]] += g;
        glVertex3fv(o.v);
        o[C[d]] += ys*g;
        glVertex3fv(o.v);
        o[C[d]] = oy;
    }
    loop(y, ys) {
        o[C[d]] += g;
        o[R[d]] = ox;
        glVertex3fv(o.v);
        o[R[d]] += xs*g;
        glVertex3fv(o.v);
    }
    glEnd();
    xtraverts += 2*int(xs+ys);
}

selinfo sel, lastsel, savedsel;

int orient = 0;
int gridsize = 8;
ivec cor, lastcor;
ivec cur, lastcur;

extern int entediting;
bool editmode = false;
bool havesel = false;
bool hmapsel = false;
int horient = 0;

extern int entmoving;

VARF(0, dragging, 0, 0, 1,
    if(!dragging || cor[0]<0) return;
    lastcur = cur;
    lastcor = cor;
    sel.grid = gridsize;
    sel.orient = orient;
);

int moving = 0;
ICOMMAND(0, moving, "b", (int *n),
{
    if(*n >= 0)
    {
        if(!*n || (moving<=1 && !pointinsel(sel, cur.tovec().add(1)))) moving = 0;
        else if(!moving) moving = 1;
    }
    intret(moving);
});

VARF(0, gridpower, 0, 3, 12,
{
    if(dragging) return;
    gridsize = 1<<gridpower;
    if(gridsize>=hdr.worldsize) gridsize = hdr.worldsize/2;
    cancelsel();
});

VAR(0, passthroughsel, 0, 0, 1);
VAR(0, editing, 1, 0, 0);
VAR(0, selectcorners, 0, 0, 1);
VARF(0, hmapedit, 0, 0, 1, horient = sel.orient);

void forcenextundo() { lastsel.orient = -1; }

extern void hmapcancel();

void cubecancel()
{
    havesel = false;
    moving = dragging = hmapedit = passthroughsel = 0;
    forcenextundo();
    hmapcancel();
}

void cancelsel()
{
    cubecancel();
    entcancel();
}

void toggleedit(bool force)
{
    if(!force && (!connected(false) || !client::allowedittoggle(editmode))) return;
    editmode = !editmode;
    editing = entediting = (editmode ? 1 : 0);
    client::edittoggled(editmode);
    cancelsel();
    efocus = enthover = -1;
    //keyrepeat(editmode);
}

bool noedit(bool view)
{
    if(!editmode) { conoutf("\froperation only allowed in edit mode"); return true; }
    if(view || haveselent()) return false;
    float r = 1.0f;
    vec o = sel.o.tovec(), s = sel.s.tovec();
    s.mul(float(sel.grid) / 2.0f);
    o.add(s);
    r = float(max(s.x, max(s.y, s.z)));
    bool viewable = (isvisiblesphere(r, o) != VFC_NOT_VISIBLE);
    if(!viewable) conoutf("\frselection not in view");
    return !viewable;
}

void reorient()
{
    sel.cx = 0;
    sel.cy = 0;
    sel.cxs = sel.s[R[dimension(orient)]]*2;
    sel.cys = sel.s[C[dimension(orient)]]*2;
    sel.orient = orient;
}

void selextend()
{
    if(noedit(true)) return;
    loopi(3)
    {
        if(cur[i]<sel.o[i])
        {
            sel.s[i] += (sel.o[i]-cur[i])/sel.grid;
            sel.o[i] = cur[i];
        }
        else if(cur[i]>=sel.o[i]+sel.s[i]*sel.grid)
        {
            sel.s[i] = (cur[i]-sel.o[i])/sel.grid+1;
        }
    }
}

ICOMMAND(0, edittoggle, "", (), toggleedit(false));
COMMAND(0, entcancel, "");
COMMAND(0, cubecancel, "");
COMMAND(0, cancelsel, "");
COMMAND(0, reorient, "");
COMMAND(0, selextend, "");

ICOMMAND(0, selmoved, "", (), { if(noedit(true)) return; intret(sel.o != savedsel.o ? 1 : 0); });
ICOMMAND(0, selsave, "", (), { if(noedit(true)) return; savedsel = sel; });
ICOMMAND(0, selrestore, "", (), { if(noedit(true)) return; sel = savedsel; });
ICOMMAND(0, selswap, "", (), { if(noedit(true)) return; swap(sel, savedsel); });

///////// selection support /////////////

cube &blockcube(int x, int y, int z, const block3 &b, int rgrid) // looks up a world cube, based on coordinates mapped by the block
{
    int dim = dimension(b.orient), dc = dimcoord(b.orient);
    ivec s(dim, x*b.grid, y*b.grid, dc*(b.s[dim]-1)*b.grid);
    s.add(b.o);
    if(dc) s[dim] -= z*b.grid; else s[dim] += z*b.grid;
    return lookupcube(s.x, s.y, s.z, rgrid);
}

#define loopxy(b)       loop(y,(b).s[C[dimension((b).orient)]]) loop(x,(b).s[R[dimension((b).orient)]])
#define loopxyz(b, r, f) { loop(z,(b).s[D[dimension((b).orient)]]) loopxy((b)) { cube &c = blockcube(x,y,z,b,r); f; } }
#define loopselxyz(f)   { if(local) makeundo(); loopxyz(sel, sel.grid, f); changed(sel); }
#define selcube(x, y, z) blockcube(x, y, z, sel, sel.grid)

////////////// cursor ///////////////

int selchildcount = 0, selchildmat = -1;

ICOMMAND(0, havesel, "", (), intret(havesel ? selchildcount : 0));

void countselchild(cube *c, const ivec &cor, int size)
{
    ivec ss = ivec(sel.s).mul(sel.grid);
    loopoctaboxsize(cor, size, sel.o, ss)
    {
        ivec o(i, cor.x, cor.y, cor.z, size);
        if(c[i].children) countselchild(c[i].children, o, size/2);
        else
        {
            selchildcount++;
            if(c[i].material != MAT_AIR && selchildmat != MAT_AIR)
            {
                if(selchildmat < 0) selchildmat = c[i].material;
                else if(selchildmat != c[i].material) selchildmat = MAT_AIR;
            }
        }
    }
}

void normalizelookupcube(int x, int y, int z)
{
    if(lusize>gridsize)
    {
        lu.x += (x-lu.x)/gridsize*gridsize;
        lu.y += (y-lu.y)/gridsize*gridsize;
        lu.z += (z-lu.z)/gridsize*gridsize;
    }
    else if(gridsize>lusize)
    {
        lu.x &= ~(gridsize-1);
        lu.y &= ~(gridsize-1);
        lu.z &= ~(gridsize-1);
    }
    lusize = gridsize;
}

void updateselection()
{
    sel.o.x = min(lastcur.x, cur.x);
    sel.o.y = min(lastcur.y, cur.y);
    sel.o.z = min(lastcur.z, cur.z);
    sel.s.x = abs(lastcur.x-cur.x)/sel.grid+1;
    sel.s.y = abs(lastcur.y-cur.y)/sel.grid+1;
    sel.s.z = abs(lastcur.z-cur.z)/sel.grid+1;
}

bool editmoveplane(const vec &o, const vec &ray, int d, float off, vec &handle, vec &dest, bool first)
{
    plane pl(d, off);
    float dist = 0.0f;
    physent *player = (physent *)game::focusedent(true);
    if(!player) player = camera1;
    if(!pl.rayintersect(player->o, ray, dist))
        return false;

    dest = vec(ray).mul(dist).add(player->o);
    if(first) handle = vec(dest).sub(o);
    dest.sub(handle);
    return true;
}

inline bool isheightmap(int orient, int d, bool empty, cube *c);
extern void entdrag(const vec &ray);
extern bool hoveringonent(int ent, int orient);
extern void renderentselection(const vec &o, const vec &ray, bool entmoving);
extern float rayent(const vec &o, const vec &ray, float radius, int mode, int size, int &orient, int &ent);

VAR(0, gridlookup, 0, 0, 1);
VAR(0, passthroughcube, 0, 1, 1);

void rendereditcursor()
{
    vec target(worldpos);
    if(!insideworld(target)) loopi(3)
        target[i] = max(min(target[i], float(hdr.worldsize)), 0.0f);
    vec ray(target);
    physent *player = (physent *)game::focusedent(true);
    if(!player) player = camera1;
    ray.sub(player->o).normalize();
    int d   = dimension(sel.orient),
        od  = dimension(orient),
        odc = dimcoord(orient);

    bool hidecursor = hud::hasinput(), hovering = false;
    hmapsel = false;

    if(moving)
    {
        static vec dest, handle;
        if(editmoveplane(sel.o.tovec(), camdir, od, sel.o[D[od]]+odc*sel.grid*sel.s[D[od]], handle, dest, moving==1))
        {
            if(moving==1)
            {
                dest.add(handle);
                handle = ivec(handle).mask(~(sel.grid-1)).tovec();
                dest.sub(handle);
                moving = 2;
            }
            ivec o = ivec(dest).mask(~(sel.grid-1));
            sel.o[R[od]] = o[R[od]];
            sel.o[C[od]] = o[C[od]];
        }
    }
    else if(entmoving)
    {
        entdrag(ray);
    }
    else
    {
        ivec w;
        float sdist = 0, wdist = 0, t;
        int entorient = 0, ent = -1;

        wdist = rayent(player->o, ray, 1e16f,
                       (editmode && showmat ? RAY_EDITMAT : 0)   // select cubes first
                                            | (!dragging && entediting ? RAY_ENTS : 0)
                                            | RAY_SKIPFIRST
                                            | (passthroughcube==1 ? RAY_PASS : 0), gridsize, entorient, ent);

        if((havesel || dragging) && !passthroughsel && !hmapedit)     // now try selecting the selection
            if(rayboxintersect(sel.o.tovec(), vec(sel.s.tovec()).mul(sel.grid), player->o, ray, sdist, orient))
            {   // and choose the nearest of the two
                if(sdist < wdist)
                {
                    wdist = sdist;
                    ent = -1;
                }
            }

        if((hovering = hoveringonent(hidecursor ? -1 : ent, entorient)))
        {
            if(!havesel)
            {
                selchildcount = 0;
                selchildmat = -1;
                sel.s = ivec(0, 0, 0);
            }
        }
        else
        {
            vec w = vec(ray).mul(wdist+0.05f).add(player->o);
            if(!insideworld(w))
            {
                loopi(3) wdist = min(wdist, ((ray[i] > 0 ? hdr.worldsize : 0) - player->o[i]) / ray[i]);
                w = vec(ray).mul(wdist-0.05f).add(player->o);
                if(!insideworld(w))
                {
                    wdist = 0;
                    loopi(3) w[i] = clamp(player->o[i], 0.0f, float(hdr.worldsize));
                }
            }
            cube *c = &lookupcube(int(w.x), int(w.y), int(w.z));
            if(gridlookup && !dragging && !moving && !havesel && hmapedit!=1) gridsize = lusize;
            int mag = gridsize && lusize ? lusize / gridsize : 0;
            normalizelookupcube(int(w.x), int(w.y), int(w.z));
            if(sdist == 0 || sdist > wdist) rayboxintersect(lu.tovec(), vec(gridsize), player->o, ray, t=0, orient); // just getting orient
            cur = lu;
            cor = vec(w).mul(2).div(gridsize);
            od = dimension(orient);
            d = dimension(sel.orient);

            if(hmapedit==1 && dimcoord(horient) == (ray[dimension(horient)]<0))
            {
                hmapsel = isheightmap(horient, dimension(horient), false, c);
                if(hmapsel)
                    od = dimension(orient = horient);
            }

            if(dragging)
            {
                updateselection();
                sel.cx  = min(cor[R[d]], lastcor[R[d]]);
                sel.cy  = min(cor[C[d]], lastcor[C[d]]);
                sel.cxs  = max(cor[R[d]], lastcor[R[d]]);
                sel.cys  = max(cor[C[d]], lastcor[C[d]]);

                if(!selectcorners)
                {
                    sel.cx &= ~1;
                    sel.cy &= ~1;
                    sel.cxs &= ~1;
                    sel.cys &= ~1;
                    sel.cxs -= sel.cx-2;
                    sel.cys -= sel.cy-2;
                }
                else
                {
                    sel.cxs -= sel.cx-1;
                    sel.cys -= sel.cy-1;
                }

                sel.cx  &= 1;
                sel.cy  &= 1;
                havesel = true;
            }
            else if(!havesel)
            {
                sel.o = lu;
                sel.s.x = sel.s.y = sel.s.z = 1;
                sel.cx = sel.cy = 0;
                sel.cxs = sel.cys = 2;
                sel.grid = gridsize;
                sel.orient = orient;
                d = od;
            }

            sel.corner = (cor[R[d]]-(lu[R[d]]*2)/gridsize)+(cor[C[d]]-(lu[C[d]]*2)/gridsize)*2;
            selchildcount = 0;
            selchildmat = -1;
            countselchild(worldroot, ivec(0, 0, 0), hdr.worldsize/2);
            if(mag>=1 && selchildcount==1)
            {
                selchildmat = c->material;
                if(mag>1) selchildcount = -mag;
            }
        }
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    // cursors

    notextureshader->set();

    renderentselection(player->o, ray, entmoving!=0);

    boxoutline = outline || (fullbright && blankgeom);

    enablepolygonoffset(GL_POLYGON_OFFSET_LINE);

    #define planargrid(q,r,s) \
    { \
        for (float v = 0.f; v < (hdr.worldsize-s); v += s) \
        { \
            vec a; \
            a = q; a.x = v; boxs3D(a, r, s); \
            a = q; a.y = v; boxs3D(a, r, s); \
            a = q; a.z = v; boxs3D(a, r, s); \
        } \
    }

    if(!moving && !hovering && !hidecursor)
    {
        bool ishmap = hmapedit==1 && hmapsel;
        if(showcursorgrid)
        {
            glColor3ub(ishmap ? 0 : 30, 30, ishmap ? 0 : 30);
            planargrid(lu.tovec(), vec(1, 1, 1), gridsize);
        }
        glColor3ub(ishmap ? 0 : 255, 255, ishmap ? 0 : 255);
        boxs(orient, lu.tovec(), vec(lusize));
    }

    // selections
    if(havesel || moving)
    {
        d = dimension(sel.orient);
        glColor3ub(120, 120, 120);  // grid
        boxsgrid(sel.orient, sel.o.tovec(), sel.s.tovec(), sel.grid);
        glColor3ub(255, 0, 0);  // 0 reference
        boxs3D(sel.o.tovec().sub(0.5f*min(gridsize*0.25f, 2.0f)), vec(min(gridsize*0.25f, 2.0f)), 1);
        glColor3ub(120, 120, 120);// 2D selection box
        vec co(sel.o.v), cs(sel.s.v);
        co[R[d]] += 0.5f*(sel.cx*gridsize);
        co[C[d]] += 0.5f*(sel.cy*gridsize);
        cs[R[d]]  = 0.5f*(sel.cxs*gridsize);
        cs[C[d]]  = 0.5f*(sel.cys*gridsize);
        cs[D[d]] *= gridsize;
        boxs(sel.orient, co, cs);
        if(hmapedit==1) glColor3ub(0, 120, 0); // 3D selection box
        else glColor3ub(0, 0, 120);
        boxs3D(sel.o.tovec(), sel.s.tovec(), sel.grid);
        if(showselgrid)
        {
            glColor3ub(30, 30, 30);
            planargrid(sel.o.tovec(), sel.s.tovec(), sel.grid);
        }
    }

    if(showpastegrid && localedit && localedit->copy)
    {
        glColor3ub(0, 192, 192);
        boxs3D(havesel ? sel.o.tovec() : lu.tovec(), localedit->copy->s.tovec(), havesel ? sel.grid : gridsize);
    }

    disablepolygonoffset(GL_POLYGON_OFFSET_LINE);

    boxoutline = false;

    notextureshader->set();

    glDisable(GL_BLEND);
}

//////////// ready changes to vertex arrays ////////////

static bool haschanged = false;

void readychanges(const ivec &bbmin, const ivec &bbmax, cube *c, const ivec &cor, int size)
{
    loopoctabox(cor, size, bbmin, bbmax)
    {
        ivec o(i, cor.x, cor.y, cor.z, size);
        if(c[i].ext)
        {
            if(c[i].ext->va)             // removes va s so that octarender will recreate
            {
                int hasmerges = c[i].ext->va->hasmerges;
                destroyva(c[i].ext->va);
                c[i].ext->va = NULL;
                if(hasmerges) invalidatemerges(c[i], o, size, true);
            }
            freeoctaentities(c[i]);
            c[i].ext->tjoints = -1;
        }
        if(c[i].children)
        {
            if(size<=1)
            {
                solidfaces(c[i]);
                discardchildren(c[i], true);
                brightencube(c[i]);
            }
            else readychanges(bbmin, bbmax, c[i].children, o, size/2);
        }
        else brightencube(c[i]);
    }
}

void commitchanges(bool force)
{
    if(!force && !haschanged) return;
    haschanged = false;

    extern vector<vtxarray *> valist;
    int oldlen = valist.length();
    resetclipplanes();
    entitiesinoctanodes();
    inbetweenframes = false;
    octarender();
    inbetweenframes = true;
    setupmaterials(oldlen);
    invalidatepostfx();
    updatevabbs();
    resetblobs();
}

void changed(const block3 &sel, bool commit = true)
{
    if(sel.s.iszero()) return;
    readychanges(ivec(sel.o).sub(1), ivec(sel.s).mul(sel.grid).add(sel.o).add(1), worldroot, ivec(0, 0, 0), hdr.worldsize/2);
    haschanged = true;

    if(commit) commitchanges();
}

//////////// copy and undo /////////////
static inline void copycube(const cube &src, cube &dst)
{
    dst = src;
    dst.visible = 0;
    dst.merged = 0;
    dst.ext = NULL; // src cube is responsible for va destruction
    if(src.children)
    {
        dst.children = newcubes(F_EMPTY);
        loopi(8) copycube(src.children[i], dst.children[i]);
    }
}

static inline void pastecube(const cube &src, cube &dst)
{
    discardchildren(dst);
    copycube(src, dst);
}

void blockcopy(const block3 &s, int rgrid, block3 *b)
{
    *b = s;
    cube *q = b->c();
    loopxyz(s, rgrid, copycube(c, *q++));
}

block3 *blockcopy(const block3 &s, int rgrid)
{
    int bsize = sizeof(block3)+sizeof(cube)*s.size();
    if(bsize <= 0 || bsize > (100<<20)) return 0;
    block3 *b = (block3 *)new uchar[bsize];
    blockcopy(s, rgrid, b);
    return b;
}

void freeblock(block3 *b, bool alloced = true)
{
    cube *q = b->c();
    loopi(b->size()) discardchildren(*q++);
    if(alloced) delete[] b;
}

void selgridmap(selinfo &sel, int *g)                           // generates a map of the cube sizes at each grid point
{
    loopxyz(sel, -sel.grid, (*g++ = lusize, (void)c));
}

void freeundo(undoblock *u)
{
    if(!u->numents) freeblock(u->block(), false);
    delete[] (uchar *)u;
}

void pasteundo(undoblock *u)
{
    if(u->numents) pasteundoents(u);
    else
    {
        block3 *b = u->block();
        cube *s = b->c();
        int *g = u->gridmap();
        loopxyz(*b, *g++, pastecube(*s++, c));
    }
}

static inline int undosize(undoblock *u)
{
    if(u->numents)
    {
        undoent *e = u->ents();
        int numattrs = 0;
        loopi(u->numents) numattrs += e[i].numattrs;
        return u->numents*sizeof(undoent) + numattrs*sizeof(int);
    }
    else
    {
        block3 *b = u->block();
        cube *q = b->c();
        int size = b->size(), total = size*sizeof(int);
        loopj(size) total += familysize(*q++)*sizeof(cube);
        return total;
    }
}

struct undolist
{
    undoblock *first, *last;

    undolist() : first(NULL), last(NULL) {}

    bool empty() { return !first; }

    void add(undoblock *u)
    {
        u->next = NULL;
        u->prev = last;
        if(!first) first = last = u;
        else
        {
            last->next = u;
            last = u;
        }
    }

    undoblock *popfirst()
    {
        undoblock *u = first;
        first = first->next;
        if(first) first->prev = NULL;
        else last = NULL;
        return u;
    }

    undoblock *poplast()
    {
        undoblock *u = last;
        last = last->prev;
        if(last) last->next = NULL;
        else first = NULL;
        return u;
    }
};

undolist undos, redos;
VAR(IDF_PERSIST, undomegs, 0, 8, 100);                              // bounded by n megs
VAR(IDF_READONLY, totalundos, 1, 0, -1);

void pruneundos(int maxremain)                          // bound memory
{
    while(totalundos > maxremain && !undos.empty())
    {
        undoblock *u = undos.popfirst();
        totalundos -= u->size;
        freeundo(u);
    }
    //conoutf("\faundo: %d of %d(%%%d)", totalundos, undomegs<<20, totalundos*100/(undomegs<<20));
    while(!redos.empty())
    {
        undoblock *u = redos.popfirst();
        totalundos -= u->size;
        freeundo(u);
    }
}

void clearundos() { pruneundos(0); }

COMMAND(0, clearundos, "");

undoblock *newundocube(selinfo &s)
{
    int ssize = s.size(),
        selgridsize = ssize*sizeof(int),
        blocksize = sizeof(block3)+ssize*sizeof(cube);
    if(blocksize <= 0 || blocksize > (undomegs<<20)) return NULL;
    undoblock *u = (undoblock *)new uchar[sizeof(undoblock) + blocksize + selgridsize];
    u->numents = 0;
    block3 *b = (block3 *)(u + 1);
    blockcopy(s, -s.grid, b);
    int *g = (int *)((uchar *)b + blocksize);
    selgridmap(s, g);
    return u;
}

void addundo(undoblock *u)
{
    u->size = undosize(u);
    u->timestamp = lastmillis;
    undos.add(u);
    totalundos += u->size;
    pruneundos(undomegs<<20);
}

void makeundoex(selinfo &s)
{
    if(multiplayer(false)) return;
    undoblock *u = newundocube(s);
    if(u) addundo(u);
}

void makeundo()                        // stores state of selected cubes before editing
{
    if(lastsel==sel || sel.s.iszero()) return;
    lastsel=sel;
    makeundoex(sel);
}

void swapundo(undolist &a, undolist &b, const char *s)
{
    if(noedit() || multiplayer()) return;
    if(a.empty()) { conoutf("\frnothing more to %s", s); return; }
    int ts = a.last->timestamp;
    selinfo l = sel;
    while(!a.empty() && ts==a.last->timestamp)
    {
        undoblock *u = a.poplast(), *r;
        if(u->numents) r = copyundoents(u);
        else
        {
            block3 *ub = u->block();
            l.o = ub->o;
            l.s = ub->s;
            l.grid = ub->grid;
            l.orient = ub->orient;
            r = newundocube(l);
        }
        if(r)
        {
            r->size = u->size;
            r->timestamp = lastmillis;
            b.add(r);
        }
        pasteundo(u);
        if(!u->numents) changed(l, false);
        freeundo(u);
    }
    commitchanges();
    if(!hmapsel)
    {
        sel = l;
        reorient();
    }
    forcenextundo();
}

void editundo() { swapundo(undos, redos, "undo"); }
void editredo() { swapundo(redos, undos, "redo"); }

// guard against subdivision
#define protectsel(f) { undoblock *_u = newundocube(sel); f; if(_u) { pasteundo(_u); freeundo(_u); } }

vector<editinfo *> editinfos;

template<class B>
static void packcube(cube &c, B &buf)
{
    if(c.children)
    {
        buf.put(0xFF);
        loopi(8) packcube(c.children[i], buf);
    }
    else
    {
        cube data = c;
        lilswap(data.texture, 6);
        buf.put(c.material&0xFF);
        buf.put(c.material>>8);
        buf.put(data.edges, sizeof(data.edges));
        buf.put((uchar *)data.texture, sizeof(data.texture));
    }
}

template<class B>
static bool packblock(block3 &b, B &buf)
{
    if(b.size() <= 0 || b.size() > (1<<20)) return false;
    block3 hdr = b;
    lilswap(hdr.o.v, 3);
    lilswap(hdr.s.v, 3);
    lilswap(&hdr.grid, 1);
    lilswap(&hdr.orient, 1);
    buf.put((const uchar *)&hdr, sizeof(hdr));
    cube *c = b.c();
    loopi(b.size()) packcube(c[i], buf);
    return true;
}

template<class B>
static void unpackcube(cube &c, B &buf)
{
    int mat = buf.get();
    if(mat == 0xFF)
    {
        c.children = newcubes(F_EMPTY);
        loopi(8) unpackcube(c.children[i], buf);
    }
    else
    {
        c.material = mat | (buf.get()<<8);
        buf.get(c.edges, sizeof(c.edges));
        buf.get((uchar *)c.texture, sizeof(c.texture));
        lilswap(c.texture, 6);
    }
}

template<class B>
static bool unpackblock(block3 *&b, B &buf)
{
    if(b) { freeblock(b); b = NULL; }
    block3 hdr;
    buf.get((uchar *)&hdr, sizeof(hdr));
    lilswap(hdr.o.v, 3);
    lilswap(hdr.s.v, 3);
    lilswap(&hdr.grid, 1);
    lilswap(&hdr.orient, 1);
    if(hdr.size() > (1<<20)) return false;
    b = (block3 *)new uchar[sizeof(block3)+hdr.size()*sizeof(cube)];
    *b = hdr;
    cube *c = b->c();
    memset(c, 0, b->size()*sizeof(cube));
    loopi(b->size()) unpackcube(c[i], buf);
    return true;
}

static bool compresseditinfo(const uchar *inbuf, int inlen, uchar *&outbuf, int &outlen)
{
    uLongf len = compressBound(inlen);
    if(len > (1<<20)) return false;
    outbuf = new uchar[len];
    if(compress2((Bytef *)outbuf, &len, (const Bytef *)inbuf, inlen, Z_BEST_COMPRESSION) != Z_OK || len > (1<<16))
    {
        delete[] outbuf;
        outbuf = NULL;
        return false;
    }
    outlen = len;
    return true;
}

static bool uncompresseditinfo(const uchar *inbuf, int inlen, uchar *&outbuf, int &outlen)
{
    if(compressBound(outlen) > (1<<20)) return false;
    uLongf len = outlen;
    outbuf = new uchar[len];
    if(uncompress((Bytef *)outbuf, &len, (const Bytef *)inbuf, inlen) != Z_OK)
    {
        delete[] outbuf;
        outbuf = NULL;
        return false;
    }
    outlen = len;
    return true;
}

bool packeditinfo(editinfo *e, int &inlen, uchar *&outbuf, int &outlen)
{
    vector<uchar> buf;
    if(!e || !e->copy || !packblock(*e->copy, buf)) return false;
    inlen = buf.length();
    return compresseditinfo(buf.getbuf(), buf.length(), outbuf, outlen);
}

bool unpackeditinfo(editinfo *&e, const uchar *inbuf, int inlen, int outlen)
{
    if(e && e->copy) { freeblock(e->copy); e->copy = NULL; }
    uchar *outbuf = NULL;
    if(!uncompresseditinfo(inbuf, inlen, outbuf, outlen)) return false;
    ucharbuf buf(outbuf, outlen);
    if(!e) e = editinfos.add(new editinfo);
    if(!unpackblock(e->copy, buf))
    {
        delete[] outbuf;
        return false;
    }
    delete[] outbuf;
    return true;
}

void freeeditinfo(editinfo *&e)
{
    if(!e) return;
    editinfos.removeobj(e);
    if(e->copy) freeblock(e->copy);
    delete e;
    e = NULL;
}

struct prefabheader
{
    char magic[4];
    int version;
};

struct prefab : editinfo
{
    char *name;

    prefab() : name(NULL) {}
    ~prefab() { DELETEA(name); if(copy) freeblock(copy); }
};

static inline bool htcmp(const char *key, const prefab &b) { return !strcmp(key, b.name); }

static hashset<prefab> prefabs;

void delprefab(char *name)
{
    if(prefabs.remove(name))
        conoutf("deleted prefab %s", name);
}
COMMAND(0, delprefab, "s");

void saveprefab(char *name)
{
    if(!name[0] || noedit(true) || multiplayer()) return;
    prefab *b = prefabs.access(name);
    if(!b)
    {
        b = &prefabs[name];
        b->name = newstring(name);
    }
    if(b->copy) freeblock(b->copy);
    protectsel(b->copy = blockcopy(block3(sel), sel.grid));
    changed(sel);
    defformatstring(filename)(strpbrk(name, "/\\") ? "%s.obr" : "prefab/%s.obr", name);
    path(filename);
    stream *f = opengzfile(filename, "wb");
    if(!f) { conoutf("\frcould not write prefab to %s", filename); return; }
    prefabheader hdr;
    memcpy(hdr.magic, "OEBR", 4);
    hdr.version = 0;
    lilswap(&hdr.version, 1);
    f->write(&hdr, sizeof(hdr));
    streambuf<uchar> s(f);
    if(!packblock(*b->copy, s)) { delete f; conoutf("\frcould not pack prefab %s", filename); return; }
    delete f;
    conoutf("wrote prefab file %s", filename);
}
ICOMMAND(0, saveprefab, "s", (char *s), if(!(identflags&IDF_WORLD)) saveprefab(s));

void pasteblock(block3 &b, selinfo &sel, bool local)
{
    sel.s = b.s;
    int o = sel.orient;
    sel.orient = b.orient;
    cube *s = b.c();
    loopselxyz(if(!isempty(*s) || s->children || s->material != MAT_AIR) pastecube(*s, c); s++); // 'transparent'. old opaque by 'delcube; paste'
    sel.orient = o;
}

void pasteprefab(char *name)
{
    if(!name[0] || noedit() || multiplayer()) return;
    prefab *b = prefabs.access(name);
    if(!b)
    {
        defformatstring(filename)(strpbrk(name, "/\\") ? "%s.obr" : "prefab/%s.obr", name);
        path(filename);
        stream *f = opengzfile(filename, "rb");
        if(!f) { conoutf("\frcould not read prefab %s", filename); return; }
        prefabheader hdr;
        if(f->read(&hdr, sizeof(hdr)) != sizeof(prefabheader) || memcmp(hdr.magic, "OEBR", 4)) { delete f; conoutf("\frprefab %s has malformatted header", filename); return; }
        lilswap(&hdr.version, 1);
        if(hdr.version != 0) { delete f; conoutf("\frprefab %s uses unsupported version", filename); return; }
        streambuf<uchar> s(f);
        block3 *copy = NULL;
        if(!unpackblock(copy, s)) { delete f; conoutf("\frcould not unpack prefab %s", filename); return; }
        delete f;
        b = &prefabs[name];
        b->name = newstring(name);
        b->copy = copy;
    }
    pasteblock(*b->copy, sel, true);
}
COMMAND(0, pasteprefab, "s");

void mpcopy(editinfo *&e, selinfo &sel, bool local)
{
    if(local) client::edittrigger(sel, EDIT_COPY);
    if(e==NULL) e = editinfos.add(new editinfo);
    if(e->copy) freeblock(e->copy);
    e->copy = NULL;
    protectsel(e->copy = blockcopy(block3(sel), sel.grid));
    changed(sel);
}

void mppaste(editinfo *&e, selinfo &sel, bool local)
{
    if(e==NULL) return;
    if(local) client::edittrigger(sel, EDIT_PASTE);
    if(e->copy) pasteblock(*e->copy, sel, local);
}

void copy()
{
    if(noedit(true)) return;
    mpcopy(localedit, sel, true);
}

void pasteclear()
{
    if(!localedit) return;
    freeeditinfo(localedit);
}

void pastehilight()
{
    if(!localedit) return;
    sel.s = localedit->copy->s;
    reorient();
    havesel = true;
}

void paste()
{
    if(noedit() || !localedit) return;
    mppaste(localedit, sel, true);
}

COMMAND(0, copy, "");
COMMAND(0, pasteclear, "");
COMMAND(0, pastehilight, "");
COMMAND(0, paste, "");
COMMANDN(0, undo, editundo, "");
COMMANDN(0, redo, editredo, "");

static VSlot *editingvslot = NULL;

void compacteditvslots()
{
    if(editingvslot && editingvslot->layer) compactvslot(editingvslot->layer);
    loopv(editinfos)
    {
        editinfo *e = editinfos[i];
        compactvslots(e->copy->c(), e->copy->size());
    }
    for(undoblock *u = undos.first; u; u = u->next)
        if(!u->numents)
            compactvslots(u->block()->c(), u->block()->size());
    for(undoblock *u = redos.first; u; u = u->next)
        if(!u->numents)
            compactvslots(u->block()->c(), u->block()->size());
}

///////////// height maps ////////////////

#define MAXBRUSH    64
#define MAXBRUSHC   63
#define MAXBRUSH2   32
int brush[MAXBRUSH][MAXBRUSH];
VAR(0, brushx, 0, MAXBRUSH2, MAXBRUSH);
VAR(0, brushy, 0, MAXBRUSH2, MAXBRUSH);
bool paintbrush = 0;
int brushmaxx = 0, brushminx = MAXBRUSH;
int brushmaxy = 0, brushminy = MAXBRUSH;

void clearbrush()
{
    memset(brush, 0, sizeof brush);
    brushmaxx = brushmaxy = 0;
    brushminx = brushminy = MAXBRUSH;
    paintbrush = false;
}

void brushvert(int *x, int *y, int *v)
{
    *x += MAXBRUSH2 - brushx + 1; // +1 for automatic padding
    *y += MAXBRUSH2 - brushy + 1;
    if(*x<0 || *y<0 || *x>=MAXBRUSH || *y>=MAXBRUSH) return;
    brush[*x][*y] = clamp(*v, 0, 8);
    paintbrush = paintbrush || (brush[*x][*y] > 0);
    brushmaxx = min(MAXBRUSH-1, max(brushmaxx, *x+1));
    brushmaxy = min(MAXBRUSH-1, max(brushmaxy, *y+1));
    brushminx = max(0,          min(brushminx, *x-1));
    brushminy = max(0,          min(brushminy, *y-1));
}

void brushimport(char *name)
{
    ImageData s;
    if(loadimage(name, s))
    {
        if(s.w > MAXBRUSH || s.h > MAXBRUSH) // only use max size
            scaleimage(s, MAXBRUSH, MAXBRUSH);

        uchar *pixel = s.data;
        int value, x, y;

        clearbrush();
        brushx = brushy = MAXBRUSH2; // set real coords to 0,0

        loopi(s.w)
        {
            x = i;
            loopj(s.h)
            {
                y = j;
                value = 0;

                loopk(s.bpp) // add the entire pixel together
                {
                    value += pixel[0];
                    pixel++;
                }

                value /= s.bpp; // average the entire pixel
                value /= 32; // scale to cube shapes (256 / 8)
                brushvert(&x, &y, &value);
            }
        }
    }
    else conoutf("\frcould not load: %s", name);
}

COMMAND(0, brushimport, "s");

vector<int> htextures;

COMMAND(0, clearbrush, "");
COMMAND(0, brushvert, "iii");
void hmapcancel() { htextures.setsize(0); }
COMMAND(0, hmapcancel, "");
ICOMMAND(0, hmapselect, "", (),
    int t = lookupcube(cur.x, cur.y, cur.z).texture[orient];
    int i = htextures.find(t);
    if(i<0)
        htextures.add(t);
    else
        htextures.remove(i);
);

inline bool isheightmap(int o, int d, bool empty, cube *c)
{
    return havesel ||
           (empty && isempty(*c)) ||
           htextures.empty() ||
           htextures.find(c->texture[o]) >= 0;
}

namespace hmap
{
#   define PAINTED     1
#   define NOTHMAP     2
#   define MAPPED      16
    uchar  flags[MAXBRUSH][MAXBRUSH];
    cube   *cmap[MAXBRUSHC][MAXBRUSHC][4];
    int    mpz[MAXBRUSHC][MAXBRUSHC];
    int    map [MAXBRUSH][MAXBRUSH];

    selinfo changes;
    bool selecting;
    int d, dc, dr, dcr, biasup, br, hws, fg;
    int gx, gy, gz, mx, my, mz, nx, ny, nz, bmx, bmy, bnx, bny;
    uint fs;
    selinfo hundo;

    cube *getcube(ivec t, int f)
    {
        t[d] += dcr*f*gridsize;
        if(t[d] > nz || t[d] < mz) return NULL;
        cube *c = &lookupcube(t.x, t.y, t.z, gridsize);
        if(c->children) forcemip(*c, false);
        discardchildren(*c, true);
        if(!isheightmap(sel.orient, d, true, c)) return NULL;
        if     (t.x < changes.o.x) changes.o.x = t.x;
        else if(t.x > changes.s.x) changes.s.x = t.x;
        if     (t.y < changes.o.y) changes.o.y = t.y;
        else if(t.y > changes.s.y) changes.s.y = t.y;
        if     (t.z < changes.o.z) changes.o.z = t.z;
        else if(t.z > changes.s.z) changes.s.z = t.z;
        return c;
    }

    uint getface(cube *c, int d)
    {
        return  0x0f0f0f0f & ((dc ? c->faces[d] : 0x88888888 - c->faces[d]) >> fs);
    }

    void pushside(cube &c, int d, int x, int y, int z)
    {
        ivec a;
        getcubevector(c, d, x, y, z, a);
        a[R[d]] = 8 - a[R[d]];
        setcubevector(c, d, x, y, z, a);
    }

    void addpoint(int x, int y, int z, int v)
    {
        if(!(flags[x][y] & MAPPED))
          map[x][y] = v + (z*8);
        flags[x][y] |= MAPPED;
    }

    void select(int x, int y, int z)
    {
        if((NOTHMAP & flags[x][y]) || (PAINTED & flags[x][y])) return;
        ivec t(d, x+gx, y+gy, dc ? z : hws-z);
        t.shl(gridpower);

        // selections may damage; must makeundo before
        hundo.o = t;
        hundo.o[D[d]] -= dcr*gridsize*2;
        makeundoex(hundo);

        cube **c = cmap[x][y];
        loopk(4) c[k] = NULL;
        c[1] = getcube(t, 0);
        if(!c[1] || !isempty(*c[1]))
        {   // try up
            c[2] = c[1];
            c[1] = getcube(t, 1);
            if(!c[1] || isempty(*c[1])) { c[0] = c[1]; c[1] = c[2]; c[2] = NULL; }
            else { z++; t[d]+=fg; }
        }
        else // drop down
        {
            z--;
            t[d]-= fg;
            c[0] = c[1];
            c[1] = getcube(t, 0);
        }

        if(!c[1] || isempty(*c[1])) { flags[x][y] |= NOTHMAP; return; }

        flags[x][y] |= PAINTED;
        mpz [x][y]  = z;

        if(!c[0]) c[0] = getcube(t, 1);
        if(!c[2]) c[2] = getcube(t, -1);
        c[3] = getcube(t, -2);
        c[2] = !c[2] || isempty(*c[2]) ? NULL : c[2];
        c[3] = !c[3] || isempty(*c[3]) ? NULL : c[3];

        uint face = getface(c[1], d);
        if(face == 0x08080808 && (!c[0] || !isempty(*c[0]))) { flags[x][y] |= NOTHMAP; return; }
        if(c[1]->faces[R[d]] == F_SOLID)   // was single
            face += 0x08080808;
        else                               // was pair
            face += c[2] ? getface(c[2], d) : 0x08080808;
        face += 0x08080808;                // c[3]
        uchar *f = (uchar*)&face;
        addpoint(x,   y,   z, f[0]);
        addpoint(x+1, y,   z, f[1]);
        addpoint(x,   y+1, z, f[2]);
        addpoint(x+1, y+1, z, f[3]);

        if(selecting) // continue to adjacent cubes
        {
            if(x>bmx) select(x-1, y, z);
            if(x<bnx) select(x+1, y, z);
            if(y>bmy) select(x, y-1, z);
            if(y<bny) select(x, y+1, z);
        }
    }

    void ripple(int x, int y, int z, bool force)
    {
        if(force) select(x, y, z);
        if((NOTHMAP & flags[x][y]) || !(PAINTED & flags[x][y])) return;

        bool changed = false;
        int *o[4], best, par, q = 0;
        loopi(2) loopj(2) o[i+j*2] = &map[x+i][y+j];
        #define pullhmap(I, LT, GT, M, N, A) do { \
            best = I; \
            loopi(4) if(*o[i] LT best) best = *o[q = i] - M; \
            par = (best&(~7)) + N; \
            /* dual layer for extra smoothness */ \
            if(*o[q^3] GT par && !(*o[q^1] LT par || *o[q^2] LT par)) { \
                if(*o[q^3] GT par A 8 || *o[q^1] != par || *o[q^2] != par) { \
                    *o[q^3] = (*o[q^3] GT par A 8 ? par A 8 : *o[q^3]); \
                    *o[q^1] = *o[q^2] = par; \
                    changed = true; \
                } \
            /* single layer */ \
            } else { \
                loopj(4) if(*o[j] GT par) { \
                    *o[j] = par; \
                    changed = true; \
                } \
            } \
        } while(0)

        if(biasup)
            pullhmap(0, >, <, 1, 0, -);
        else
            pullhmap(hdr.worldsize*8, <, >, 0, 8, +);

        cube **c  = cmap[x][y];
        int e[2][2];
        int notempty = 0;

        loopk(4) if(c[k]) {
            loopi(2) loopj(2) {
                e[i][j] = min(8, map[x+i][y+j] - (mpz[x][y]+3-k)*8);
                notempty |= e[i][j] > 0;
            }
            if(notempty)
            {
                c[k]->texture[sel.orient] = c[1]->texture[sel.orient];
                solidfaces(*c[k]);
                loopi(2) loopj(2)
                {
                    int f = e[i][j];
                    if(f<0 || (f==0 && e[1-i][j]==0 && e[i][1-j]==0))
                    {
                        f=0;
                        pushside(*c[k], d, i, j, 0);
                        pushside(*c[k], d, i, j, 1);
                    }
                    edgeset(cubeedge(*c[k], d, i, j), dc, dc ? f : 8-f);
                }
            }
            else
                emptyfaces(*c[k]);
        }

        if(!changed) return;
        if(x>mx) ripple(x-1, y, mpz[x][y], true);
        if(x<nx) ripple(x+1, y, mpz[x][y], true);
        if(y>my) ripple(x, y-1, mpz[x][y], true);
        if(y<ny) ripple(x, y+1, mpz[x][y], true);

#define DIAGONAL_RIPPLE(a,b,exp) if(exp) { \
            if(flags[x a][ y] & PAINTED) \
                ripple(x a, y b, mpz[x a][y], true); \
            else if(flags[x][y b] & PAINTED) \
                ripple(x a, y b, mpz[x][y b], true); \
        }

        DIAGONAL_RIPPLE(-1, -1, (x>mx && y>my)); // do diagonals because adjacents
        DIAGONAL_RIPPLE(-1, +1, (x>mx && y<ny)); //    won't unless changed
        DIAGONAL_RIPPLE(+1, +1, (x<nx && y<ny));
        DIAGONAL_RIPPLE(+1, -1, (x<nx && y>my));
        }

#define loopbrush(i) for(int x=bmx; x<=bnx+i; x++) for(int y=bmy; y<=bny+i; y++)

    void paint()
    {
        loopbrush(1)
            map[x][y] -= dr * brush[x][y];
    }

    void smooth()
    {
        int sum, div;
        loopbrush(-2)
        {
            sum = 0;
            div = 9;
            loopi(3) loopj(3)
                if(flags[x+i][y+j] & MAPPED)
                    sum += map[x+i][y+j];
                else div--;
            if(div)
                map[x+1][y+1] = sum / div;
        }
    }

    void rippleandset()
    {
        loopbrush(0)
            ripple(x, y, gz, false);
    }

    void run(int dir, int mode)
    {
        d  = dimension(sel.orient);
        dc = dimcoord(sel.orient);
        dcr= dc ? 1 : -1;
        dr = dir>0 ? 1 : -1;
        br = dir>0 ? 0x08080808 : 0;
     //   biasup = mode == dir<0;
        biasup = dir<0;
        bool paintme = paintbrush;
        int cx = (sel.corner&1 ? 0 : -1);
        int cy = (sel.corner&2 ? 0 : -1);
        hws= (hdr.worldsize>>gridpower);
        gx = (cur[R[d]] >> gridpower) + cx - MAXBRUSH2;
        gy = (cur[C[d]] >> gridpower) + cy - MAXBRUSH2;
        gz = (cur[D[d]] >> gridpower);
        fs = dc ? 4 : 0;
        fg = dc ? gridsize : -gridsize;
        mx = max(0, -gx);                   // ripple range
        my = max(0, -gy);
        nx = min(MAXBRUSH-1, hws-gx) - 1;
        ny = min(MAXBRUSH-1, hws-gy) - 1;
        if(havesel)
        {   // selection range
            bmx = mx = max(mx, (sel.o[R[d]]>>gridpower)-gx);
            bmy = my = max(my, (sel.o[C[d]]>>gridpower)-gy);
            bnx = nx = min(nx, (sel.s[R[d]]+(sel.o[R[d]]>>gridpower))-gx-1);
            bny = ny = min(ny, (sel.s[C[d]]+(sel.o[C[d]]>>gridpower))-gy-1);
        }
        if(havesel && mode<0) // -ve means smooth selection
            paintme = false;
        else
        {   // brush range
            bmx = max(mx, brushminx);
            bmy = max(my, brushminy);
            bnx = min(nx, brushmaxx-1);
            bny = min(ny, brushmaxy-1);
        }
        nz = hdr.worldsize-gridsize;
        mz = 0;
        hundo.s = ivec(d,1,1,5);
        hundo.orient = sel.orient;
        hundo.grid = gridsize;
        forcenextundo();

        changes.grid = gridsize;
        changes.s = changes.o = cur;
        memset(map, 0, sizeof map);
        memset(flags, 0, sizeof flags);

        selecting = true;
        select(clamp(MAXBRUSH2-cx, bmx, bnx),
               clamp(MAXBRUSH2-cy, bmy, bny),
              dc ? gz : hws - gz);
        selecting = false;
        if(paintme)
            paint();
        else
            smooth();
        rippleandset();                       // pull up points to cubify, and set
        changes.s.sub(changes.o).shr(gridpower).add(1);
        changed(changes);
    }
}

void edithmap(int dir, int mode) {
    if(multiplayer() || !hmapsel) return;
    hmap::run(dir, mode);
}

///////////// main cube edit ////////////////

int bounded(int n) { return n<0 ? 0 : (n>8 ? 8 : n); }

void pushedge(uchar &edge, int dir, int dc)
{
    int ne = bounded(edgeget(edge, dc)+dir);
    edgeset(edge, dc, ne);
    int oe = edgeget(edge, 1-dc);
    if((dir<0 && dc && oe>ne) || (dir>0 && dc==0 && oe<ne)) edgeset(edge, 1-dc, ne);
}

void linkedpush(cube &c, int d, int x, int y, int dc, int dir)
{
    ivec v, p;
    getcubevector(c, d, x, y, dc, v);

    loopi(2) loopj(2)
    {
        getcubevector(c, d, i, j, dc, p);
        if(v==p)
            pushedge(cubeedge(c, d, i, j), dir, dc);
    }
}

static ushort getmaterial(cube &c)
{
    if(c.children)
    {
        ushort mat = getmaterial(c.children[7]);
        loopi(7) if(mat != getmaterial(c.children[i])) return MAT_AIR;
        return mat;
    }
    return c.material;
}

VAR(0, invalidcubeguard, 0, 1, 1);

void mpeditface(int dir, int mode, selinfo &sel, bool local)
{
    if(mode==1 && (sel.cx || sel.cy || sel.cxs&1 || sel.cys&1)) mode = 0;
    int d = dimension(sel.orient);
    int dc = dimcoord(sel.orient);
    int seldir = dc ? -dir : dir;

    if(local)
        client::edittrigger(sel, EDIT_FACE, dir, mode);

    if(mode==1)
    {
        int h = sel.o[d]+dc*sel.grid;
        if(((dir>0) == dc && h<=0) || ((dir<0) == dc && h>=hdr.worldsize)) return;
        if(dir<0) sel.o[d] += sel.grid * seldir;
    }

    if(dc) sel.o[d] += sel.us(d)-sel.grid;
    sel.s[d] = 1;

    loopselxyz(
        if(c.children) solidfaces(c);
        ushort mat = getmaterial(c);
        discardchildren(c, true);
        c.material = mat;
        if(mode==1) // fill command
        {
            if(dir<0)
            {
                solidfaces(c);
                cube &o = blockcube(x, y, 1, sel, -sel.grid);
                loopi(6)
                    c.texture[i] = o.children ? DEFAULT_GEOM : o.texture[i];
            }
            else
                emptyfaces(c);
        }
        else
        {
            uint bak = c.faces[d];
            uchar *p = (uchar *)&c.faces[d];

            if(mode==2)
                linkedpush(c, d, sel.corner&1, sel.corner>>1, dc, seldir); // corner command
            else
            {
                loop(mx,2) loop(my,2)                                       // pull/push edges command
                {
                    if(x==0 && mx==0 && sel.cx) continue;
                    if(y==0 && my==0 && sel.cy) continue;
                    if(x==sel.s[R[d]]-1 && mx==1 && (sel.cx+sel.cxs)&1) continue;
                    if(y==sel.s[C[d]]-1 && my==1 && (sel.cy+sel.cys)&1) continue;
                    if(p[mx+my*2] != ((uchar *)&bak)[mx+my*2]) continue;

                    linkedpush(c, d, mx, my, dc, seldir);
                }
            }

            optiface(p, c);
            if(invalidcubeguard==1 && !isvalidcube(c))
            {
                uint newbak = c.faces[d];
                uchar *m = (uchar *)&bak;
                uchar *n = (uchar *)&newbak;
                loopk(4) if(n[k] != m[k]) // tries to find partial edit that is valid
                {
                    c.faces[d] = bak;
                    c.edges[d*4+k] = n[k];
                    if(isvalidcube(c))
                        m[k] = n[k];
                }
                c.faces[d] = bak;
            }
        }
    );
    if(mode==1 && dir>0)
        sel.o[d] += sel.grid * seldir;
}

void editface(int *dir, int *mode)
{
    if(noedit(moving!=0)) return;
    if(hmapedit!=1)
        mpeditface(*dir, *mode, sel, true);
    else
        edithmap(*dir, *mode);
}

VAR(0, selectionsurf, 0, 0, 1);

void pushsel(int *dir)
{
    if(noedit(moving!=0)) return;
    int d = dimension(orient);
    int s = dimcoord(orient) ? -*dir : *dir;
    sel.o[d] += s*sel.grid;
    if(selectionsurf==1)
    {
        physent *player = (physent *)game::focusedent(true);
        if(!player) player = camera1;
        player->o[d] += s*sel.grid;
        player->resetinterp();
    }
}

void mpdelcube(selinfo &sel, bool local)
{
    if(local) client::edittrigger(sel, EDIT_DELCUBE);
    loopselxyz(discardchildren(c, true); emptyfaces(c));
}

void delcube()
{
    if(noedit()) return;
    mpdelcube(sel, true);
}

COMMAND(0, pushsel, "i");
COMMAND(0, editface, "ii");
COMMAND(0, delcube, "");

/////////// texture editing //////////////////

int curtexindex = -1, lasttex = 0, lasttexmillis = -1;
int texpaneltimer = 0;
vector<ushort> texmru;

void tofronttex()                                       // maintain most recently used of the texture lists when applying texture
{
    int c = curtexindex;
    if(texmru.inrange(c))
    {
        texmru.insert(0, texmru.remove(c));
        curtexindex = -1;
    }
}

selinfo repsel;
int reptex = -1;

struct vslotmap
{
    int index;
    VSlot *vslot;

    vslotmap() {}
    vslotmap(int index, VSlot *vslot) : index(index), vslot(vslot) {}
};
static vector<vslotmap> remappedvslots;

VAR(0, usevdelta, 1, 0, 0);

static VSlot *remapvslot(int index, const VSlot &ds)
{
    loopv(remappedvslots) if(remappedvslots[i].index == index) return remappedvslots[i].vslot;
    VSlot &vs = lookupvslot(index, false);
    if(vs.index < 0 || vs.index == DEFAULT_SKY) return NULL;
    VSlot *edit = NULL;
    if(usevdelta)
    {
        VSlot ms;
        mergevslot(ms, vs, ds);
        edit = ms.changed ? editvslot(vs, ms) : vs.slot->variants;
    }
    else edit = ds.changed ? editvslot(vs, ds) : vs.slot->variants;
    if(!edit) edit = &vs;
    remappedvslots.add(vslotmap(vs.index, edit));
    return edit;
}

static void remapvslots(cube &c, const VSlot &ds, int orient, bool &findrep, VSlot *&findedit)
{
    if(c.children)
    {
        loopi(8) remapvslots(c.children[i], ds, orient, findrep, findedit);
        return;
    }
    static VSlot ms;
    if(orient<0) loopi(6)
    {
        VSlot *edit = remapvslot(c.texture[i], ds);
        if(edit)
        {
            c.texture[i] = edit->index;
            if(!findedit) findedit = edit;
        }
    }
    else
    {
        int i = visibleorient(c, orient);
        VSlot *edit = remapvslot(c.texture[i], ds);
        if(edit)
        {
            if(findrep)
            {
                if(reptex < 0) reptex = c.texture[i];
                else if(reptex != c.texture[i]) findrep = false;
            }
            c.texture[i] = edit->index;
            if(!findedit) findedit = edit;
        }
    }
}

void edittexcube(cube &c, int tex, int orient, bool &findrep)
{
    if(orient<0) loopi(6) c.texture[i] = tex;
    else
    {
        int i = visibleorient(c, orient);
        if(findrep)
        {
            if(reptex < 0) reptex = c.texture[i];
            else if(reptex != c.texture[i]) findrep = false;
        }
        c.texture[i] = tex;
    }
    if(c.children) loopi(8) edittexcube(c.children[i], tex, orient, findrep);
}

VAR(0, allfaces, 0, 0, 1);

void mpeditvslot(VSlot &ds, int allfaces, selinfo &sel, bool local)
{
    if(local)
    {
        if(!(lastsel==sel)) tofronttex();
        if(allfaces || !(repsel == sel)) reptex = -1;
        repsel = sel;
    }
    bool findrep = local && !allfaces && reptex < 0;
    VSlot *findedit = NULL;
    editingvslot = &ds;
    loopselxyz(remapvslots(c, ds, allfaces ? -1 : sel.orient, findrep, findedit));
    editingvslot = NULL;
    remappedvslots.setsize(0);
    if(local && findedit)
    {
        lasttex = findedit->index;
        lasttexmillis = totalmillis;
        curtexindex = texmru.find(lasttex);
        if(curtexindex < 0)
        {
            curtexindex = texmru.length();
            texmru.add(lasttex);
        }
    }
}

void vdelta(char *body)
{
    if(noedit() || multiplayer()) return;
    usevdelta++;
    execute(body);
    usevdelta--;
}
COMMAND(0, vdelta, "s");

void vrotate(int *n)
{
    if(noedit() || multiplayer()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_ROTATION;
    ds.rotation = usevdelta ? *n : clamp(*n, 0, 5);
    mpeditvslot(ds, allfaces, sel, true);
}
COMMAND(0, vrotate, "i");

void voffset(int *x, int *y)
{
    if(noedit() || multiplayer()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_OFFSET;
    ds.xoffset = usevdelta ? *x : max(*x, 0);
    ds.yoffset = usevdelta ? *y : max(*y, 0);
    mpeditvslot(ds, allfaces, sel, true);
}
COMMAND(0, voffset, "ii");

void vscroll(float *s, float *t)
{
    if(noedit() || multiplayer()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_SCROLL;
    ds.scrollS = *s/1000.0f;
    ds.scrollT = *t/1000.0f;
    mpeditvslot(ds, allfaces, sel, true);
}
COMMAND(0, vscroll, "ff");

void vscale(float *scale)
{
    if(noedit() || multiplayer()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_SCALE;
    ds.scale = *scale <= 0 ? 1 : (usevdelta ? *scale : clamp(*scale, 1/8.0f, 8.0f));
    mpeditvslot(ds, allfaces, sel, true);
}
COMMAND(0, vscale, "f");

void vlayer(int *n)
{
    if(noedit() || multiplayer()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_LAYER;
    ds.layer = vslots.inrange(*n) ? *n : 0;
    mpeditvslot(ds, allfaces, sel, true);
}
COMMAND(0, vlayer, "i");

void valpha(float *front, float *back)
{
    if(noedit() || multiplayer()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_ALPHA;
    ds.alphafront = clamp(*front, 0.0f, 1.0f);
    ds.alphaback = clamp(*back, 0.0f, 1.0f);
    mpeditvslot(ds, allfaces, sel, true);
}
COMMAND(0, valpha, "ff");

void vcolour(float *r, float *g, float *b)
{
    if(noedit() || multiplayer()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_COLOR;
    ds.colorscale = vec(max(*r, 0.0f), max(*g, 0.0f), max(*b, 0.0f));
    mpeditvslot(ds, allfaces, sel, true);
}
COMMAND(0, vcolour, "fff");

void vpalette(int *p, int *x)
{
    if(noedit() || multiplayer()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_PALETTE;
    ds.palette = max(*p, 0);
    ds.palindex = max(*x, 0);
    mpeditvslot(ds, allfaces, sel, true);
}
COMMAND(0, vpalette, "ii");

void vcoastscale(float *value)
{
    if(noedit() || multiplayer()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_COAST;
    ds.coastscale = clamp(*value, 0.f, 1000.f);
    mpeditvslot(ds, allfaces, sel, true);
}
COMMAND(0, vcoastscale, "f");

void vreset()
{
    if(noedit() || multiplayer()) return;
    VSlot ds;
    mpeditvslot(ds, allfaces, sel, true);
}
COMMAND(0, vreset, "");

void vshaderparam(const char *name, float *x, float *y, float *z, float *w, int *palette, int *palindex)
{
    if(noedit() || multiplayer()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_SHPARAM;
    if(name[0])
    {
        ShaderParam p = { getshaderparamname(name), SHPARAM_LOOKUP, -1, -1, *palette, *palindex, {*x, *y, *z, *w} };
        ds.params.add(p);
    }
    mpeditvslot(ds, allfaces, sel, true);
}
COMMAND(0, vshaderparam, "sffffii");

void mpedittex(int tex, int allfaces, selinfo &sel, bool local)
{
    if(local)
    {
        client::edittrigger(sel, EDIT_TEX, tex, allfaces);
        if(allfaces || !(repsel == sel)) reptex = -1;
        repsel = sel;
    }
    bool findrep = local && !allfaces && reptex < 0;
    loopselxyz(edittexcube(c, tex, allfaces ? -1 : sel.orient, findrep));
}

void filltexlist()
{
    if(texmru.length()!=vslots.length())
    {
        loopvrev(texmru) if(texmru[i]>=vslots.length())
        {
            if(curtexindex > i) curtexindex--;
            else if(curtexindex == i) curtexindex = -1;
            texmru.remove(i);
        }
        loopv(vslots) if(texmru.find(i)<0) texmru.add(i);
    }
}

void compactmruvslots()
{
    remappedvslots.setsize(0);
    loopvrev(texmru)
    {
        if(vslots.inrange(texmru[i]))
        {
            VSlot &vs = *vslots[texmru[i]];
            if(vs.index >= 0)
            {
                texmru[i] = vs.index;
                continue;
            }
        }
        if(curtexindex > i) curtexindex--;
        else if(curtexindex == i) curtexindex = -1;
        texmru.remove(i);
    }
    if(vslots.inrange(lasttex))
    {
        VSlot &vs = *vslots[lasttex];
        lasttex = vs.index >= 0 ? vs.index : 0;
    }
    else lasttex = 0;
    reptex = vslots.inrange(reptex) ? vslots[reptex]->index : -1;
}

void edittex(int i, bool save = true, bool edit = true)
{
    lasttex = i;
    lasttexmillis = totalmillis;
    if(save)
    {
        loopvj(texmru) if(texmru[j]==lasttex) { curtexindex = j; break; }
    }
    if(edit) mpedittex(i, allfaces, sel, true);
}
ICOMMAND(0, settex, "i", (int *slot), edittex(*slot));

VAR(IDF_PERSIST, texpaneltime, 0, 5000, VAR_MAX);
void edittex_(int *dir)
{
    if(noedit()) return;
    filltexlist();
    if(texmru.empty()) return;
    texpaneltimer = texpaneltime;
    if(!(lastsel==sel)) tofronttex();
    curtexindex = clamp(curtexindex<0 ? 0 : curtexindex+*dir, 0, texmru.length()-1);
    edittex(texmru[curtexindex], false);
}

void gettex()
{
    if(noedit(true)) return;
    filltexlist();
    int tex = -1;
    loopxyz(sel, sel.grid, tex = c.texture[sel.orient]);
    loopv(texmru) if(texmru[i]==tex)
    {
        curtexindex = i;
        tofronttex();
        return;
    }
}

void getcurtex()
{
    if(noedit(true)) return;
    filltexlist();
    int index = curtexindex < 0 ? 0 : curtexindex;
    if(!texmru.inrange(index)) return;
    intret(texmru[index]);
}

void getseltex()
{
    if(noedit(true)) return;
    cube &c = lookupcube(sel.o.x, sel.o.y, sel.o.z, -sel.grid);
    if(c.children || isempty(c)) return;
    intret(c.texture[sel.orient]);
}

void gettexname(int *tex, int *subslot)
{
    if(noedit(true) || *tex<0) return;
    VSlot &vslot = lookupvslot(*tex, false);
    Slot &slot = *vslot.slot;
    if(!slot.sts.inrange(*subslot)) return;
    result(slot.sts[*subslot].name);
}

COMMANDN(0, edittex, edittex_, "i");
COMMAND(0, gettex, "");
COMMAND(0, getcurtex, "");
COMMAND(0, getseltex, "");
ICOMMAND(0, getreptex, "", (), { if(!noedit()) intret(vslots.inrange(reptex) ? reptex : -1); });
COMMAND(0, gettexname, "ii");

void replacetexcube(cube &c, int oldtex, int newtex)
{
    loopi(6) if(oldtex < 0 || c.texture[i] == oldtex) c.texture[i] = newtex;
    if(c.children) loopi(8) replacetexcube(c.children[i], oldtex, newtex);
}

void mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, bool local)
{
    if(local) client::edittrigger(sel, EDIT_REPLACE, oldtex, newtex, insel ? 1 : 0);
    if(insel)
    {
        loopselxyz(replacetexcube(c, oldtex, newtex));
    }
    else
    {
        loopi(8) replacetexcube(worldroot[i], oldtex, newtex);
    }
    allchanged();
}

void replacetex(bool insel, int texnum = -1)
{
    if(noedit()) return;
    mpreplacetex(texnum, lasttex, insel, sel, true);
}

ICOMMAND(0, replace, "iN", (int *t, int *numargs), {
    int tex = *numargs >= 1 ? *t : reptex;
    if(tex < 0) { conoutf("\frcan only replace after a texture edit"); return; }
    replacetex(false, tex);
});
ICOMMAND(0, replacesel, "iN", (int *t, int *numargs), {
    int tex = *numargs >= 1 ? *t : reptex;
    if(tex < 0) { conoutf("\frcan only replace after a texture edit"); return; }
    replacetex(true, tex);
});
ICOMMAND(0, replaceall, "", (void), replacetex(false));
ICOMMAND(0, replaceallsel, "", (void), replacetex(true));

void resettexmru()
{
    int old = texmru.inrange(curtexindex) ? texmru[curtexindex] : -1;
    texmru.shrink(0);
    loopv(vslots) texmru.add(i);
    curtexindex = texmru.find(old);
}
ICOMMAND(0, resettexmru, "", (void), resettexmru());

////////// flip and rotate ///////////////
uint dflip(uint face) { return face==F_EMPTY ? face : 0x88888888 - (((face&0xF0F0F0F0)>>4) | ((face&0x0F0F0F0F)<<4)); }
uint cflip(uint face) { return ((face&0xFF00FF00)>>8) | ((face&0x00FF00FF)<<8); }
uint rflip(uint face) { return ((face&0xFFFF0000)>>16)| ((face&0x0000FFFF)<<16); }
uint mflip(uint face) { return (face&0xFF0000FF) | ((face&0x00FF0000)>>8) | ((face&0x0000FF00)<<8); }

void flipcube(cube &c, int d)
{
    swap(c.texture[d*2], c.texture[d*2+1]);
    c.faces[D[d]] = dflip(c.faces[D[d]]);
    c.faces[C[d]] = cflip(c.faces[C[d]]);
    c.faces[R[d]] = rflip(c.faces[R[d]]);
    if(c.children)
    {
        loopi(8) if(i&octadim(d)) swap(c.children[i], c.children[i-octadim(d)]);
        loopi(8) flipcube(c.children[i], d);
    }
}

void rotatequad(cube &a, cube &b, cube &c, cube &d)
{
    cube t = a; a = b; b = c; c = d; d = t;
}

void rotatecube(cube &c, int d) // rotates cube clockwise. see pics in cvs for help.
{
    c.faces[D[d]] = cflip (mflip(c.faces[D[d]]));
    c.faces[C[d]] = dflip (mflip(c.faces[C[d]]));
    c.faces[R[d]] = rflip (mflip(c.faces[R[d]]));
    swap(c.faces[R[d]], c.faces[C[d]]);

    swap(c.texture[2*R[d]], c.texture[2*C[d]+1]);
    swap(c.texture[2*C[d]], c.texture[2*R[d]+1]);
    swap(c.texture[2*C[d]], c.texture[2*C[d]+1]);

    if(c.children)
    {
        int row = octadim(R[d]);
        int col = octadim(C[d]);
        for(int i=0; i<=octadim(d); i+=octadim(d)) rotatequad
        (
            c.children[i+row],
            c.children[i],
            c.children[i+col],
            c.children[i+col+row]
        );
        loopi(8) rotatecube(c.children[i], d);
    }
}

void mpflip(selinfo &sel, bool local)
{
    if(local)
    {
        client::edittrigger(sel, EDIT_FLIP);
        makeundo();
    }
    int zs = sel.s[dimension(sel.orient)];
    loopxy(sel)
    {
        loop(z,zs) flipcube(selcube(x, y, z), dimension(sel.orient));
        loop(z,zs/2)
        {
            cube &a = selcube(x, y, z);
            cube &b = selcube(x, y, zs-z-1);
            swap(a, b);
        }
    }
    changed(sel);
}

void flip()
{
    if(noedit()) return;
    mpflip(sel, true);
}

void mprotate(int cw, selinfo &sel, bool local)
{
    if(local) client::edittrigger(sel, EDIT_ROTATE, cw);
    int d = dimension(sel.orient);
    if(!dimcoord(sel.orient)) cw = -cw;
    int m = sel.s[C[d]] < sel.s[R[d]] ? C[d] : R[d];
    int ss = sel.s[m] = max(sel.s[R[d]], sel.s[C[d]]);
    if(local) makeundo();
    loop(z,sel.s[D[d]]) loopi(cw>0 ? 1 : 3)
    {
        loopxy(sel) rotatecube(selcube(x,y,z), d);
        loop(y,ss/2) loop(x,ss-1-y*2) rotatequad
        (
            selcube(ss-1-y, x+y, z),
            selcube(x+y, y, z),
            selcube(y, ss-1-x-y, z),
            selcube(ss-1-x-y, ss-1-y, z)
        );
    }
    changed(sel);
}

void rotate(int *cw)
{
    if(noedit()) return;
    mprotate(*cw, sel, true);
}

COMMAND(0, flip, "");
COMMAND(0, rotate, "i");

enum { EDITMATF_EMPTY = 0x10000, EDITMATF_NOTEMPTY = 0x20000, EDITMATF_SOLID = 0x30000, EDITMATF_NOTSOLID = 0x40000 };
static const struct { const char *name; int filter; } editmatfilters[] =
{
    { "empty", EDITMATF_EMPTY },
    { "notempty", EDITMATF_NOTEMPTY },
    { "solid", EDITMATF_SOLID },
    { "notsolid", EDITMATF_NOTSOLID }
};

void setmat(cube &c, ushort mat, ushort matmask, ushort filtermat, ushort filtermask, int filtergeom)
{
    if(c.children)
        loopi(8) setmat(c.children[i], mat, matmask, filtermat, filtermask, filtergeom);
    else if((c.material&filtermask) == filtermat)
    {
        switch(filtergeom)
        {
            case EDITMATF_EMPTY: if(isempty(c)) break; return;
            case EDITMATF_NOTEMPTY: if(!isempty(c)) break; return;
            case EDITMATF_SOLID: if(isentirelysolid(c)) break; return;
            case EDITMATF_NOTSOLID: if(!isentirelysolid(c)) break; return;
        }
        if(mat!=MAT_AIR)
        {
            c.material &= matmask;
            c.material |= mat;
        }
        else c.material = MAT_AIR;
    }
}

void mpeditmat(int matid, int filter, int style, selinfo &sel, bool local)
{
    if(local) client::edittrigger(sel, EDIT_MAT, matid, filter, style);

    ushort filtermat = 0, filtermask = 0, matmask;
    int filtergeom = 0;
    if(filter >= 0)
    {
        filtermat = filter&0xFFFF;
        filtermask = filtermat&(MATF_VOLUME|MATF_INDEX) ? MATF_VOLUME|MATF_INDEX : (filtermat&MATF_CLIP ? MATF_CLIP : filtermat);
        filtergeom = filter&~0xFFFF;
    }
    switch(style)
    {
        case 1: filtergeom = EDITMATF_NOTEMPTY; break;
        case 2: filtergeom = EDITMATF_EMPTY; break;
        case 3: filtergeom = EDITMATF_NOTSOLID; break;
        case 4: filtergeom = EDITMATF_SOLID; break;
    }
    if(matid < 0)
    {
        matid = 0;
        matmask = filtermask;
        if(isclipped(filtermat&MATF_VOLUME)) matmask &= ~MATF_CLIP;
        if(isdeadly(filtermat&MATF_VOLUME)) matmask &= ~MAT_DEATH;
    }
    else
    {
        matmask = matid&(MATF_VOLUME|MATF_INDEX) ? 0 : (matid&MATF_CLIP ? ~MATF_CLIP : ~matid);
        if(isclipped(matid&MATF_VOLUME)) matid |= MAT_CLIP;
        if(isdeadly(matid&MATF_VOLUME)) matid |= MAT_DEATH;
    }
    loopselxyz(setmat(c, matid, matmask, filtermat, filtermask, filtergeom));
}

void editmat(char *name, char *filtername, int *style)
{
    if(noedit()) return;
    int filter = -1;
    if(filtername[0])
    {
        loopi(sizeof(editmatfilters)/sizeof(editmatfilters[0])) if(!strcmp(editmatfilters[i].name, filtername)) { filter = editmatfilters[i].filter; break; }
        if(filter < 0) filter = findmaterial(filtername, true);
        if(filter < 0) { conoutf("\frunknown material \"%s\"", filtername); return; }
    }
    int id = -1;
    if(name[0] || filter < 0)
    {
        id = findmaterial(name, true);
        if(id<0) { conoutf("\frunknown material \"%s\"", name); return; }
    }
    mpeditmat(id, filter, *style, sel, true);
}

COMMAND(0, editmat, "ssi");

VAR(IDF_PERSIST, autoapplytexgui, 0, 1, 1);
VAR(IDF_PERSIST, autopreviewtexgui, 0, 1, 1);
VAR(IDF_PERSIST, autoclosetexgui, 0, 2, 2);

VAR(IDF_PERSIST, thumbwidth, 0, 8, 1000);
VAR(IDF_PERSIST, thumbheight, 0, 6, 1000);
VAR(IDF_PERSIST, thumbtime, 0, 25, 1000);
FVAR(IDF_PERSIST, thumbsize, 0, 2, 25);

static int lastthumbnail = 0;
struct texturegui : guicb
{
    bool menuon;
    int menustart, menupage, menutex, rolltex;

    texturegui() : menustart(-1), menutex(-1), rolltex(-1) {}

    void gui(guient &g, bool firstpass)
    {
        extern VSlot dummyvslot;
        int curtex = menutex, numpages = max((texmru.length() + thumbwidth*thumbheight - 1)/(thumbwidth*thumbheight), 1)-1;
        if(autopreviewtexgui && texmru.inrange(rolltex)) curtex = rolltex;
        if(menupage > numpages) menupage = numpages;
        else if(menupage < 0) menupage = 0;
        g.start(menustart, menuscale, NULL, true);
        uilist(g, {
            g.space(2);
            if(g.button("\fgauto apply", 0xFFFFFF, autoapplytexgui ? "checkboxon" : "checkbox", 0xFFFFFF)&GUI_UP)
                autoapplytexgui = autoapplytexgui ? 0 : 1;
            g.space(2);
            if(g.button("\fgauto preview", 0xFFFFFF, autopreviewtexgui ? "checkboxon" : "checkbox", 0xFFFFFF)&GUI_UP)
                autopreviewtexgui = autopreviewtexgui ? 0 : 1;
            g.space(2);
            if(g.button("\fgauto close", 0xFFFFFF, autoclosetexgui ? (autoclosetexgui > 1 ? "checkboxtwo" : "checkboxon") : "checkbox", 0xFFFFFF)&GUI_UP)
                autoclosetexgui = autoclosetexgui ? (autoclosetexgui > 1 ? 0 : 2) : 1;
            g.space(2);
            if(g.button("\foreset order", 0xFFFFFF, NULL)&GUI_UP)
            {
                int old = texmru.inrange(menutex) ? texmru[menutex] : -1;
                resettexmru();
                curtex = menutex = rolltex = texmru.find(old);
            }
        });
        g.space(1);
        uilist(g, {
            if(texmru.inrange(curtex))
            {
                VSlot &v = lookupvslot(texmru[curtex], false);
                if(v.slot->sts.empty()) g.texture(dummyvslot, thumbheight*thumbsize, false);
                else if(!v.slot->loaded && !v.slot->thumbnail)
                {
                    if(totalmillis-lastthumbnail<thumbtime)
                        g.texture(dummyvslot, thumbheight*thumbsize, false); //create an empty space
                    loadthumbnail(*v.slot);
                    lastthumbnail = totalmillis;
                }
                int ret = g.texture(v, thumbheight*thumbsize, true);
                if(ret&GUI_UP)
                {
                    edittex(texmru[curtex]);
                    if(autoclosetexgui) menuon = false;
                }
            }
            else g.image(textureload(nothumbtex, 3), thumbheight*thumbsize, true);
            g.space(1);
            uilistv(g, 2, {
                uilist(g, loop(h, thumbheight)
                {
                    uilist(g,  loop(w, thumbwidth)
                    {
                        int ti = (menupage*thumbheight+h)*thumbwidth+w;
                        if(ti<texmru.length())
                        {
                            VSlot &v = lookupvslot(texmru[ti], false);
                            if(v.slot->sts.empty()) continue;
                            else if(!v.slot->loaded && !v.slot->thumbnail)
                            {
                                if(totalmillis-lastthumbnail<thumbtime)
                                {
                                    g.texture(dummyvslot, thumbsize, false); //create an empty space
                                    continue;
                                }
                                loadthumbnail(*v.slot);
                                lastthumbnail = totalmillis;
                            }
                            int ret = g.texture(v, thumbsize, true);
                            if(ret&GUI_UP)
                            {
                                if(autoapplytexgui && (v.slot->loaded || v.slot->thumbnail!=notexture))
                                {
                                    menutex = ti;
                                    edittex(texmru[ti]);
                                    if(autoclosetexgui > 1) menuon = false;
                                }
                            }
                            else if(ret&GUI_ROLLOVER) rolltex = ti;
                        }
                        else g.texture(dummyvslot, thumbsize, false); //create an empty space
                    });
                });
                g.slider(menupage, 0, numpages, 0xFFFFFF, NULL, true, true);
            });
        });
        g.space(1);
        uilist(g, {
            g.space(1);
            if(texmru.inrange(curtex))
            {
                VSlot &v = lookupvslot(texmru[curtex]);
                g.textf("#%-3d \fa%s", 0xFFFFFF, NULL, 0, -1, texmru[curtex], v.slot->sts.empty() ? "<unknown texture>" : v.slot->sts[0].name);
            }
            else g.textf("no texture selected", 0x888888);
        });
        g.end();
    }

    void showtextures(bool on)
    {
        if(on != menuon && (menuon = on))
        {
            menustart = starttime();
            cube &c = lookupcube(sel.o.x, sel.o.y, sel.o.z, -sel.grid);
            rolltex = -1;
            if(texmru.length() > 0)
            {
                if(!texmru.inrange(menutex = !isempty(c) ? texmru.find(c.texture[sel.orient]) : texmru.find(lasttex)))
                    menutex = 0;
                menupage = clamp(menutex, 0, texmru.length()-1)/(thumbwidth*thumbheight);
            }
            else menutex = menupage = 0;
        }
    }

    void show()
    {
        if(!menuon) return;
        filltexlist();
        if(!editmode) menuon = false;
        else UI::addcb(this);
    }
} gui;

void texturemenu() { gui.show(); }
bool closetexgui() { if(gui.menuon) { gui.menuon = false; return true; } return false; }

void showtexgui(int *n)
{
    if(!editmode) { conoutf("\froperation only allowed in edit mode"); return; }
    gui.showtextures(*n==0 ? !gui.menuon : *n==1);
}

COMMAND(0, showtexgui, "i"); // 0/noargs = toggle, 1 = on, other = off - will autoclose when exiting editmode

void rendertexturepanel(int w, int h)
{
    if((texpaneltimer -= curtime)>0 && editmode)
    {
        glLoadIdentity();
        int width = w*1800/h;
        glOrtho(0, width, 1800, 0, -1, 1);
        int y = 50, gap = 10;

        SETSHADER(rgbonly);

        loopi(7)
        {
            int s = (i == 3 ? 285 : 220), ti = curtexindex+i-3;
            if(texmru.inrange(ti))
            {
                VSlot &vslot = lookupvslot(texmru[ti]), *layer = NULL;
                Slot &slot = *vslot.slot;
                Texture *tex = slot.sts.empty() ? notexture : slot.sts[0].t, *glowtex = NULL, *layertex = NULL;
                if(slot.texmask&(1<<TEX_GLOW))
                {
                    loopvj(slot.sts) if(slot.sts[j].type==TEX_GLOW) { glowtex = slot.sts[j].t; break; }
                }
                if(vslot.layer)
                {
                    layer = &lookupvslot(vslot.layer);
                    layertex = layer->slot->sts.empty() ? notexture : layer->slot->sts[0].t;
                }
                float sx = min(1.0f, tex->xs/(float)tex->ys), sy = min(1.0f, tex->ys/(float)tex->xs);
                int x = width-s-50, r = s;
                float tc[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
                float xoff = vslot.xoffset, yoff = vslot.yoffset;
                if(vslot.rotation)
                {
                    if((vslot.rotation&5) == 1) { swap(xoff, yoff); loopk(4) swap(tc[k][0], tc[k][1]); }
                    if(vslot.rotation >= 2 && vslot.rotation <= 4) { xoff *= -1; loopk(4) tc[k][0] *= -1; }
                    if(vslot.rotation <= 2 || vslot.rotation == 5) { yoff *= -1; loopk(4) tc[k][1] *= -1; }
                }
                loopk(4) { tc[k][0] = tc[k][0]/sx - xoff/tex->xs; tc[k][1] = tc[k][1]/sy - yoff/tex->ys; }
                glBindTexture(GL_TEXTURE_2D, tex->id);
                vec colorscale = vslot.getcolorscale();
                loopj(glowtex ? 3 : 2)
                {
                    if(j < 2) glColor4f(j*colorscale.x, j*colorscale.y, j*colorscale.z, texpaneltimer/1000.0f);
                    else
                    {
                        glBindTexture(GL_TEXTURE_2D, glowtex->id);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                        vec glowcolor = vslot.getglowcolor();
                        glColor4f(glowcolor.x, glowcolor.y, glowcolor.z, texpaneltimer/1000.0f);
                    }
                    glBegin(GL_TRIANGLE_STRIP);
                    glTexCoord2fv(tc[0]); glVertex2f(x,   y);
                    glTexCoord2fv(tc[1]); glVertex2f(x+r, y);
                    glTexCoord2fv(tc[3]); glVertex2f(x,   y+r);
                    glTexCoord2fv(tc[2]); glVertex2f(x+r, y+r);
                    glEnd();
                    xtraverts += 4;
                    if(j==1 && layertex)
                    {
                        vec layerscale = layer->getcolorscale();
                        glColor4f(layerscale.x, layerscale.y, layerscale.z, texpaneltimer/1000.0f);
                        glBindTexture(GL_TEXTURE_2D, layertex->id);
                        glBegin(GL_TRIANGLE_STRIP);
                        glTexCoord2fv(tc[0]); glVertex2f(x+r/2, y+r/2);
                        glTexCoord2fv(tc[1]); glVertex2f(x+r,   y+r/2);
                        glTexCoord2fv(tc[3]); glVertex2f(x+r/2, y+r);
                        glTexCoord2fv(tc[2]); glVertex2f(x+r,   y+r);
                        glEnd();
                        xtraverts += 4;
                    }
                    if(!j)
                    {
                        r -= 10;
                        x += 5;
                        y += 5;
                    }
                    else if(j == 2) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }
            }
            y += s+gap;
        }

        defaultshader->set();
    }
    else texpaneltimer = 0;
}
