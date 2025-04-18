#include "engine.h"

VAR(0, showpastegrid, 0, 0, 1);
VAR(0, showcursorgrid, 0, 0, 1);
VAR(0, showselboxgrid, 0, 0, 1);

bool boxoutline = false;

void boxs(int orient, vec o, const vec &s, float size)
{
    int d = dimension(orient), dc = dimcoord(orient);
    float f = boxoutline ? (dc>0 ? 0.2f : -0.2f) : 0;
    o[D[d]] += dc * s[D[d]] + f;

    vec r(0, 0, 0), c(0, 0, 0);
    r[R[d]] = s[R[d]];
    c[C[d]] = s[C[d]];

    vec v1 = o, v2 = vec(o).add(r), v3 = vec(o).add(r).add(c), v4 = vec(o).add(c);

    r[R[d]] = 0.5f*size;
    c[C[d]] = 0.5f*size;

    gle::defvertex();
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attrib(vec(v1).sub(r).sub(c));
        gle::attrib(vec(v1).add(r).add(c));
    gle::attrib(vec(v2).add(r).sub(c));
        gle::attrib(vec(v2).sub(r).add(c));
    gle::attrib(vec(v3).add(r).add(c));
        gle::attrib(vec(v3).sub(r).sub(c));
    gle::attrib(vec(v4).sub(r).add(c));
        gle::attrib(vec(v4).add(r).sub(c));
    gle::attrib(vec(v1).sub(r).sub(c));
        gle::attrib(vec(v1).add(r).add(c));
    xtraverts += gle::end();
}

void boxs(int orient, vec o, const vec &s)
{
    int d = dimension(orient), dc = dimcoord(orient);
    float f = boxoutline ? (dc>0 ? 0.2f : -0.2f) : 0;
    o[D[d]] += dc * s[D[d]] + f;

    gle::defvertex();
    gle::begin(GL_LINE_LOOP);

    gle::attrib(o); o[R[d]] += s[R[d]];
    gle::attrib(o); o[C[d]] += s[C[d]];
    gle::attrib(o); o[R[d]] -= s[R[d]];
    gle::attrib(o);

    xtraverts += gle::end();
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

    gle::defvertex();
    gle::begin(GL_LINES);
    loop(x, xs)
    {
        o[R[d]] += g;
        gle::attrib(o);
        o[C[d]] += ys*g;
        gle::attrib(o);
        o[C[d]] = oy;
    }
    loop(y, ys)
    {
        o[C[d]] += g;
        o[R[d]] = ox;
        gle::attrib(o);
        o[R[d]] += xs*g;
        gle::attrib(o);
    }
    xtraverts += gle::end();
}

void renderboundboxes()
{
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    ldrnotextureshader->set();
    gle::colorub(255, 255, 255);

    dynent *e = NULL;
    int numdyns = game::numdynents(2);
    loopi(numdyns) if((e = (dynent *)game::iterdynents(i, 2)) && e->state == CS_ALIVE)
    {
        vec rad = vec(e->xradius*2, e->yradius*2, e->height+e->aboveeye), o = vec(e->o).sub(vec(e->xradius, e->yradius, e->height));
        loopj(6) boxs(j, o, rad);
        if(showboundingboxes > 1) physics::renderboundboxes((physent *)e, rad, o);
    }

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
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

ICOMMANDV(0, hassel, havesel ? 1 : 0);

extern int entmoving;
extern int entselsnap;
extern int entselsnapmode;
extern int entmovesnapent;

VARF(0, dragging, 0, 0, 1,
    if(!dragging || cor[0]<0) return;
    lastcur = cur;
    lastcor = cor;
    sel.grid = gridsize;
    sel.orient = orient;
);

int movingaxis = -1;
ICOMMAND(0, movingaxis, "bN", (int *axis, int *numargs),
{
    if(*numargs > 0) movingaxis = clamp(*axis, -1, 2);
    else intret(movingaxis);
});

int moving = 0;
ICOMMAND(0, moving, "b", (int *n),
{
    if(*n >= 0)
    {
        movingaxis = -1;

        if(!*n || (moving<=1 && !pointinsel(sel, vec(cur).add(1)))) moving = 0;
        else if(!moving) moving = 1;
    }
    intret(moving);
});

VARF(0, gridpower, 0, 3, 12,
{
    if(dragging) return;
    while((1<<gridpower) >= worldsize) gridpower--;
    gridsize = 1<<gridpower;
    cancelsel();
});

VAR(0, passthroughsel, 0, 0, 1);
VAR(0, editing, 1, 0, 0);
VAR(0, selectcorners, 0, 0, 1);
VARF(0, hmapedit, 0, 0, 1, horient = sel.orient);

void forcenextundo() { lastsel.orient = -1; }

namespace hmap { void cancel(); }

void cubecancel()
{
    havesel = false;
    moving = dragging = hmapedit = passthroughsel = 0;
    movingaxis = -1;
    forcenextundo();
    hmap::cancel();
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
    editing = (editmode ? 1 : 0);
    client::edittoggled(editmode);
    cancelsel();
    stoppaintblendmap();
    //keyrepeat(editmode);
}

int selinview()
{
    if(!editmode || haveselent()) return -1;
    vec o(sel.o), s(sel.s);
    s.mul(sel.grid / 2.0f);
    o.add(s);
    return isvisiblesphere(max(s.x, s.y, s.z), o);
}
ICOMMANDV(0, selinview, selinview());

bool selvisible()
{
    int vfc = selinview();
    return vfc < 0 || vfc != VFC_NOT_VISIBLE ? 0 : 1;
}
ICOMMANDV(0, selvisible, selvisible());

bool noedit(bool view, bool msg)
{
    if(!editmode) { conoutf(colourred, "Operation only allowed in edit mode"); return true; }
    if(view || haveselent()) return false;
    int vfc = selinview();
    bool viewable = (vfc >= 0 && vfc != VFC_NOT_VISIBLE);
    if(!viewable && msg) conoutf(colourred, "Selection not in view");
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

void setorient(int n)
{
    if(noedit()) return;
    int m = clamp(n, 0, O_ANY-1);
    sel.cx = 0;
    sel.cy = 0;
    sel.cxs = sel.s[R[dimension(m)]]*2;
    sel.cys = sel.s[C[dimension(m)]]*2;
    sel.orient = m;
}
ICOMMAND(0, selorient, "iN$", (int *n, int *numargs, ident *id),
{
    if(*numargs > 0) setorient(*n);
    else if(*numargs < 0) intret(sel.orient);
    else printvar(id, sel.orient);
});

void addorient(int n)
{
    if(noedit()) return;
    int m = sel.orient+n;
    while(m >= O_ANY) m -= O_ANY;
    while(m < 0) m += O_ANY;
    setorient(m);
}
ICOMMAND(0, addselorient, "i", (int *n), addorient(*n));

///////// selection support /////////////

cube &blockcube(int x, int y, int z, const block3 &b, int rgrid) // looks up a world cube, based on coordinates mapped by the block
{
    int dim = dimension(b.orient), dc = dimcoord(b.orient);
    ivec s(dim, x*b.grid, y*b.grid, dc*(b.s[dim]-1)*b.grid);
    s.add(b.o);
    if(dc) s[dim] -= z*b.grid; else s[dim] += z*b.grid;
    return lookupcube(s, rgrid);
}

////////////// cursor ///////////////

bool hasselchildmat = false;
int selchildcount = 0;
ushort selchildmat[MATF_NUMVOL] = { 0, 0, 0, 0, 0, 0, 0 };

ICOMMANDV(0, havesel, havesel ? selchildcount : 0);
ICOMMANDVS(0, selchildmat, getmaterialdesc(selchildmat));

// Count material and material variants
void countselmat(ushort mat)
{
    selchildmat[(mat & MATF_VOLUME) >> MATF_VOLUME_SHIFT] |= mat & (MATF_VOLUME | MATF_INDEX);
    selchildmat[(mat & MATF_CLIP) >> MATF_CLIP_SHIFT] |= mat & MATF_CLIP;
    selchildmat[0] |= mat & MATF_FLAGS;
}

void countselchild(cube *c, const ivec &cor, int size)
{
    ivec ss = ivec(sel.s).mul(sel.grid);
    loopoctaboxsize(cor, size, sel.o, ss)
    {
        ivec o(i, cor, size);
        if(c[i].children) countselchild(c[i].children, o, size/2);
        else
        {
            selchildcount++;
            if(c[i].material != MAT_AIR) countselmat(c[i].material);
        }
    }
}

void normalizelookupcube(const ivec &o)
{
    if(lusize>gridsize)
    {
        lu.x += (o.x-lu.x)/gridsize*gridsize;
        lu.y += (o.y-lu.y)/gridsize*gridsize;
        lu.z += (o.z-lu.z)/gridsize*gridsize;
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

namespace hmap { inline bool isheightmap(int orient, int d, bool empty, cube *c); }
extern void entdrag(const vec &ray);
extern bool hoveringonent(vector<int> ents, int orient);
extern void renderentselection(const vec &o, const vec &ray, bool entmoving);

VAR(0, gridlookup, 0, 0, 1);
VAR(0, passthroughcube, 0, 1, 1);
VAR(0, passthroughent, 0, 1, 1);
VAR(IDF_PERSIST, passthroughentcancel, 0, 0, 1);
VARF(0, passthrough, 0, 0, 1,
{
    passthroughsel = passthrough;
    if(passthroughentcancel) entcancel();
});

CVAR(IDF_PERSIST, selgridhmap, 0x66FF22);
CVAR(IDF_PERSIST, selgridcursor, 0xFFBBBB);
CVAR(IDF_PERSIST, selgridskew, 0x101010);
VAR(IDF_PERSIST, selgridpulse, 0, 500, VAR_MAX);
FVAR(IDF_PERSIST, selgridwidth, 0, 4, 10);

CVAR(IDF_PERSIST, selboxgridcursor, 0x888888);
CVAR(IDF_PERSIST, selboxgridskew, 0x101010);
VAR(IDF_PERSIST, selboxgridpulse, 0, 500, VAR_MAX);
FVAR(IDF_PERSIST, selboxgridwidth, 0, 4, 10);
CVAR(IDF_PERSIST, selboxgridref, 0xFF2222);
FVAR(IDF_PERSIST, selboxgridrefwidth, 0, 2, 10);

CVAR(IDF_PERSIST, selbox2dcursor, 0x101010);
CVAR(IDF_PERSIST, selbox2dskew, 0x888888);
VAR(IDF_PERSIST, selbox2dpulse, 0, 500, VAR_MAX);
FVAR(IDF_PERSIST, selbox2dwidth, 0, 4, 10);

CVAR(IDF_PERSIST, selbox3dhmap, 0x228822);
CVAR(IDF_PERSIST, selbox3dcursor, 0x101010);
CVAR(IDF_PERSIST, selbox3dskew, 0x222288);
VAR(IDF_PERSIST, selbox3dpulse, 0, 500, VAR_MAX);
FVAR(IDF_PERSIST, selbox3dwidth, 0, 4, 10);

CVAR(IDF_PERSIST, selectionhmap, 0x22FF22);
CVAR(IDF_PERSIST, selectioncursor, 0xFFFFFF);
CVAR(IDF_PERSIST, selectionskew, 0x101010);
VAR(IDF_PERSIST, selectionpulse, 0, 500, VAR_MAX);
FVAR(IDF_PERSIST, selectionwidth, 0, 4, 10);

VAR(IDF_PERSIST, selectionoffset, 0, 1, 1);

int geteditorient(int curorient, int axis)
{
    int finalorient = -1;

    if(curorient >= 0 && axis < 0) finalorient = curorient;
    else
    {
        vec dir;
        vecfromyawpitch(camera1->yaw, camera1->pitch, 1, 0, dir);

        float maxdot = -1;
        finalorient = 0;

        // maps normals to orientations
        static vec dirs[] =
        {
            vec(-1,  0,  0),
            vec( 1,  0,  0),
            vec( 0, -1,  0),
            vec( 0,  1,  0),
            vec( 0,  0, -1),
            vec( 0,  0,  1)
        };

        #define AXIS_X 1
        #define AXIS_Y (1 << 1)
        #define AXIS_Z (1 << 2)

        // maps planes suitable for particular axes
        static int sideaxes[] =
        {
            AXIS_Y | AXIS_Z,
            AXIS_Y | AXIS_Z,
            AXIS_X | AXIS_Z,
            AXIS_X | AXIS_Z,
            AXIS_X | AXIS_Y,
            AXIS_X | AXIS_Y
        };

        // find most suitable plane in relation to the camera direction
        loopi(6) if(axis < 0 || sideaxes[i] & (1 << axis))
        {
            float dot = dir.dot(dirs[i]);
            if(dot > maxdot)
            {
                maxdot = dot;
                finalorient = i;
            }
        }
    }

    return finalorient;
}

void rendereditcursor()
{
    int editorient = geteditorient(orient, movingaxis);

    int d   = dimension(sel.orient),
        od  = dimension(editorient),
        odc = dimcoord(editorient);

    bool hidecursor = hud::hasinput() == 1 || blendpaintmode, hovering = false;
    hmapsel = false;
    entmovesnapent = -1;

    physent *player = (physent *)game::focusedent(true);
    if(!player) player = camera1;

    if(moving)
    {
        static vec dest, handle;
        static ivec oldpos;
        if(editmoveplane(vec(sel.o), cursordir, od, sel.o[D[od]]+odc*sel.grid*sel.s[D[od]], handle, dest, moving==1))
        {
            if(moving==1)
            {
                oldpos = sel.o;
                dest.add(handle);
                handle = vec(ivec(handle).mask(~(sel.grid-1)));
                dest.sub(handle);
                moving = 2;
            }
            ivec o = ivec(dest).mask(~(sel.grid-1));
            ivec newpos = sel.o;

            newpos[R[od]] = o[R[od]];
            newpos[C[od]] = o[C[od]];

            switch(movingaxis)
            {
                case 0: newpos.y = oldpos.y; newpos.z = oldpos.z; break;
                case 1: newpos.x = oldpos.x; newpos.z = oldpos.z; break;
                case 2: newpos.x = oldpos.x; newpos.y = oldpos.y; break;
            }

            sel.o = newpos;
        }
    }
    else if(entmoving)
    {
        vector<int> ents;
        int eo = 0;

        if(entselsnap && entselsnapmode == 1)
            rayent(player->o, cursordir, 1e16f, RAY_ENTS | RAY_SKIPFIRST, gridsize, eo, ents,
                   &entgroup);

        if(ents.length()) entmovesnapent = ents[0];

        entdrag(cursordir);
    }
    else
    {
        ivec w;
        float sdist = 0, wdist = 0, t;
        int eo = 0;
        vector<int> ents;

        wdist = rayent(player->o, cursordir, 1e16f,
                       (editmode && showmat ? RAY_EDITMAT : 0)   // select cubes first
                       | (!dragging && entediting && (!passthrough || !passthroughent) ? RAY_ENTS : 0)
                       | RAY_SKIPFIRST
                       | (passthroughcube || passthrough ? RAY_PASS : 0), gridsize, eo, ents, NULL);

        if((havesel || dragging) && !passthroughsel && !hmapedit)     // now try selecting the selection
            if(rayboxintersect(vec(sel.o), vec(sel.s).mul(sel.grid), player->o, cursordir, sdist, orient))
            {   // and choose the nearest of the two
                if(sdist < wdist)
                {
                    wdist = sdist;
                    ents.setsize(0);
                }
            }

        if(hidecursor) ents.setsize(0);
        if((hovering = hoveringonent(ents, eo)))
        {
            if(!havesel)
            {
                selchildcount = 0;
                memset(selchildmat, 0, sizeof(selchildmat));
                sel.s = ivec(0, 0, 0);
            }
        }
        else
        {
            vec w = vec(cursordir).mul(wdist+0.05f).add(player->o);
            if(!insideworld(w))
            {
                loopi(3) wdist = min(wdist, ((cursordir[i] > 0 ? worldsize : 0) - player->o[i]) / cursordir[i]);
                w = vec(cursordir).mul(wdist-0.05f).add(player->o);
                if(!insideworld(w))
                {
                    wdist = 0;
                    loopi(3) w[i] = clamp(player->o[i], 0.0f, float(worldsize));
                }
            }
            cube *c = &lookupcube(ivec(w));
            if(gridlookup && !dragging && !moving && !havesel && hmapedit!=1) gridsize = lusize;
            int mag = gridsize && lusize ? lusize / gridsize : 0;
            normalizelookupcube(ivec(w));
            if(sdist == 0 || sdist > wdist) rayboxintersect(vec(lu), vec(gridsize), player->o, cursordir, t=0, orient); // just getting orient
            cur = lu;
            cor = ivec(vec(w).mul(2).div(gridsize));
            od = dimension(orient);
            d = dimension(sel.orient);

            if(hmapedit==1 && dimcoord(horient) == (cursordir[dimension(horient)]<0))
            {
                hmapsel = hmap::isheightmap(horient, dimension(horient), false, c);
                if(hmapsel)
                    od = dimension(orient = horient);
            }

            if(dragging)
            {
                updateselection();
                sel.cx   = min(cor[R[d]], lastcor[R[d]]);
                sel.cy   = min(cor[C[d]], lastcor[C[d]]);
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
            memset(selchildmat, 0, sizeof(selchildmat));
            countselchild(worldroot, ivec(0, 0, 0), worldsize/2);
            if(mag>=1 && selchildcount==1)
            {
                countselmat(c->material);
                if(mag>1) selchildcount = -mag;
            }
        }
    }

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    // cursors

    ldrnotextureshader->set();

    renderentselection(player->o, cursordir, entmoving!=0);

    float offset = 1.0f;
    if (outline || (fullbright && blankgeom)) {
      if (selectionoffset) {
        boxoutline = true;
      } else {
        offset = 2.0f;
      }
    }
    enablepolygonoffset(GL_POLYGON_OFFSET_LINE, offset);

    #define planargrid(q,r,s) \
    { \
        for (float v = 0.f; v < (worldsize-s); v += s) \
        { \
            vec a; \
            a = q; a.x = v; boxs3D(a, r, s); \
            a = q; a.y = v; boxs3D(a, r, s); \
            a = q; a.z = v; boxs3D(a, r, s); \
        } \
    }

    if(!moving && !hovering && !hidecursor)
    {
        bool ishmap = hmapedit == 1 && hmapsel;
        if(showcursorgrid)
        {
            bvec col = ishmap ? selgridhmap : selgridcursor;
            if(!ishmap && selgridpulse)
            {
                float part = (lastmillis%(selgridpulse*2))/float(selgridpulse);
                if(part > 1) part = 2-part;
                loopi(3) col[i] = clamp((col[i]*(1-part))+(selgridskew[i]*part), 0, 255);
            }
            gle::colorub(col.r, col.g, col.b);
            if(selgridwidth != 1) glLineWidth(selgridwidth);
            planargrid(vec(lu), vec(1, 1, 1), gridsize);
            if(selectionwidth != 1) glLineWidth(1);
        }
        bvec col = ishmap ? selectionhmap : selectioncursor;
        if(!ishmap && selectionpulse)
        {
            float part = (lastmillis%(selectionpulse*2))/float(selectionpulse);
            if(part > 1) part = 2-part;
            loopi(3) col[i] = clamp((col[i]*(1-part))+(selectionskew[i]*part), 0, 255);
        }
        gle::colorub(col.r, col.g, col.b);
        if(selectionwidth != 1) glLineWidth(selectionwidth);
        boxs(orient, vec(lu), vec(lusize));
        if(selectionwidth != 1) glLineWidth(1);
    }

    // selections
    if(havesel || moving)
    {
        d = dimension(sel.orient);
        bvec col = selboxgridcursor;
        if(selboxgridpulse)
        {
            float part = (lastmillis%(selboxgridpulse*2))/float(selboxgridpulse);
            if(part > 1) part = 2-part;
            loopi(3) col[i] = clamp((col[i]*(1-part))+(selboxgridskew[i]*part), 0, 255);
        }
        gle::colorub(col.r, col.g, col.b);
        if(selboxgridwidth != 1) glLineWidth(selboxgridwidth);
        boxsgrid(sel.orient, vec(sel.o), vec(sel.s), sel.grid);
        if(selectionwidth != 1) glLineWidth(1);

        col = selboxgridref;
        if(selboxgridpulse)
        {
            float part = (lastmillis%(selboxgridpulse*2))/float(selboxgridpulse);
            if(part > 1) part = 2-part;
            loopi(3) col[i] = clamp((col[i]*(1-part))+(selboxgridskew[i]*part), 0, 255);
        }
        gle::colorub(col.r, col.g, col.b);
        if(selboxgridrefwidth != 1) glLineWidth(selboxgridrefwidth);
        boxs3D(vec(sel.o).sub(0.5f*min(gridsize*0.25f, 2.0f)), vec(min(gridsize*0.25f, 2.0f)), 1);
        if(selboxgridrefwidth != 1) glLineWidth(1);

        vec co(sel.o.v), cs(sel.s.v);
        co[R[d]] += 0.5f*(sel.cx*gridsize);
        co[C[d]] += 0.5f*(sel.cy*gridsize);
        cs[R[d]]  = 0.5f*(sel.cxs*gridsize);
        cs[C[d]]  = 0.5f*(sel.cys*gridsize);
        cs[D[d]] *= gridsize;
        col = selbox2dcursor;
        if(selbox2dpulse)
        {
            float part = (lastmillis%(selbox2dpulse*2))/float(selbox2dpulse);
            if(part > 1) part = 2-part;
            loopi(3) col[i] = clamp((col[i]*(1-part))+(selbox2dskew[i]*part), 0, 255);
        }
        gle::colorub(col.r, col.g, col.b);
        if(selbox2dwidth != 1) glLineWidth(selbox2dwidth);
        boxs(sel.orient, co, cs);
        if(selectionwidth != 1) glLineWidth(1);

        col = hmapedit ? selbox3dhmap : selbox3dcursor;
        if(!hmapedit && selbox3dpulse)
        {
            float part = (lastmillis%(selbox3dpulse*2))/float(selbox3dpulse);
            if(part > 1) part = 2-part;
            loopi(3) col[i] = clamp((col[i]*(1-part))+(selbox3dskew[i]*part), 0, 255);
        }
        gle::colorub(col.r, col.g, col.b);
        if(selbox3dwidth != 1) glLineWidth(selbox3dwidth);
        boxs3D(vec(sel.o), vec(sel.s), sel.grid);
        if(selbox3dwidth != 1) glLineWidth(1);

        if(showselboxgrid)
        {
            gle::colorub(30, 30, 30);
            planargrid(vec(sel.o), vec(sel.s), sel.grid);
        }

        if(movingaxis >= 0)
        {
            vec selpos = vec(sel.o);
            vec selend = vec(sel.s);

            vec from = selpos,
                to   = selend;

            from[movingaxis] = selpos[movingaxis] - fmod(selpos[movingaxis], worldsize);
            to[movingaxis]   = from[movingaxis] + worldsize;

            gle::colorub(0, 128, 0);
            boxs3D(from, to, sel.grid);
        }
    }

    if(showpastegrid && localedit && localedit->copy)
    {
        gle::colorub(0, 192, 192);
        boxs3D(havesel ? vec(sel.o) : vec(lu), vec(localedit->copy->s), havesel ? sel.grid : gridsize);
    }

    disablepolygonoffset(GL_POLYGON_OFFSET_LINE);

    boxoutline = false;

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

int lasttex = 0, lasttexmillis = -1;
VARR(texpaneltimer, 0);
VARR(curtexindex, -1);

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

void tryedit()
{
    if(!editmode) return;
    if(blendpaintmode) trypaintblendmap();
}

//////////// ready changes to vertex arrays ////////////

static bool haschanged = false;

void readychanges(const ivec &bbmin, const ivec &bbmax, cube *c, const ivec &cor, int size)
{
    loopoctabox(cor, size, bbmin, bbmax)
    {
        ivec o(i, cor, size);
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

    int oldlen = valist.length();
    resetclipplanes();
    entitiesinoctanodes();
    inbetweenframes = false;
    octarender();
    inbetweenframes = true;
    setupmaterials(oldlen);
    clearshadowcache();
    updatevabbs();
}

void changed(const ivec &bbmin, const ivec &bbmax, bool commit)
{
    readychanges(bbmin, bbmax, worldroot, ivec(0, 0, 0), worldsize/2);
    haschanged = true;

    if(commit) commitchanges();
}

void changed(const block3 &sel, bool commit)
{
    if(sel.s.iszero()) return;
    readychanges(ivec(sel.o).sub(1), ivec(sel.s).mul(sel.grid).add(sel.o).add(1), worldroot, ivec(0, 0, 0), worldsize/2);
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
    if(bsize <= 0 || bsize > (100<<20)) return NULL;
    block3 *b = (block3 *)new (false) uchar[bsize];
    if(b) blockcopy(s, rgrid, b);
    return b;
}

void freeblock(block3 *b, bool alloced = true)
{
    cube *q = b->c();
    loopi(b->size()) discardchildren(*q++);
    if(alloced) delete[] b;
}

void selgridmap(const selinfo &sel, uchar *g)                           // generates a map of the cube sizes at each grid point
{
    loopxyz(sel, -sel.grid, (*g++ = bitscan(lusize), (void)c));
}

void freeundo(undoblock *u)
{
    if(!u->numents) freeblock(u->block(), false);
    delete[] (uchar *)u;
}

void pasteundoblock(block3 *b, uchar *g)
{
    cube *s = b->c();
    loopxyz(*b, 1<<min(int(*g++), worldscale-1), pastecube(*s++, c));
}

void pasteundo(undoblock *u)
{
    if(u->numents) pasteundoents(u);
    else pasteundoblock(u->block(), u->gridmap());
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
        int size = b->size(), total = size;
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
VARR(totalundos, 0);

void pruneundos(int maxremain)                          // bound memory
{
    while(totalundos > maxremain && !undos.empty())
    {
        undoblock *u = undos.popfirst();
        totalundos -= u->size;
        freeundo(u);
    }
    //conoutf(colourgrey, "Undo: %d of %d(%%%d)", totalundos, undomegs<<20, totalundos*100/(undomegs<<20));
    while(!redos.empty())
    {
        undoblock *u = redos.popfirst();
        totalundos -= u->size;
        freeundo(u);
    }
}

void clearundos() { pruneundos(0); }

COMMAND(0, clearundos, "");

undoblock *newundocube(const selinfo &s)
{
    int ssize = s.size(),
        selgridsize = ssize,
        blocksize = sizeof(block3)+ssize*sizeof(cube);
    if(blocksize <= 0 || blocksize > (undomegs<<20)) return NULL;
    undoblock *u = (undoblock *)new (false) uchar[sizeof(undoblock) + blocksize + selgridsize];
    if(!u) return NULL;
    u->numents = 0;
    block3 *b = u->block();
    blockcopy(s, -s.grid, b);
    uchar *g = u->gridmap();
    selgridmap(s, g);
    return u;
}

void addundo(undoblock *u)
{
    u->size = undosize(u);
    u->timestamp = totalmillis;
    undos.add(u);
    totalundos += u->size;
    pruneundos(undomegs<<20);
}

VAR(IDF_PERSIST, nompedit, 0, 1, 1);

void makeundo(selinfo &s)
{
    undoblock *u = newundocube(s);
    if(u) addundo(u);
}

void makeundo()                        // stores state of selected cubes before editing
{
    if(lastsel==sel || sel.s.iszero()) return;
    lastsel=sel;
    makeundo(sel);
}

static inline int countblock(cube *c, int n = 8)
{
    int r = 0;
    loopi(n) if(c[i].children) r += countblock(c[i].children); else ++r;
    return r;
}

static int countblock(block3 *b) { return countblock(b->c(), b->size()); }

enum
{
    UNDO_NONE = 0,
    UNDO_CUBE,
    UNDO_ENT
};

int swapundo(undolist &a, undolist &b, int op)
{
    int undotype = UNDO_NONE;

    if(noedit()) return UNDO_NONE;
    if(a.empty()) { conoutf(colourred, "Nothing more to %s", op == EDIT_REDO ? "redo" : "undo"); return UNDO_NONE; }
    int ts = a.last->timestamp;
    if(multiplayer(false))
    {
        int n = 0, ops = 0;
        for(undoblock *u = a.last; u && ts==u->timestamp; u = u->prev)
        {
            ++ops;
            n += u->numents ? u->numents : countblock(u->block());
            if(ops > 10 || n > 2500)
            {
                conoutf(colourred, "Undo too big for multiplayer");
                if(nompedit) { multiplayer(); return UNDO_NONE; }
                op = -1;
                break;
            }
        }
    }
    selinfo l = sel;
    while(!a.empty() && ts==a.last->timestamp)
    {
        if(op >= 0) client::edittrigger(sel, op);
        undoblock *u = a.poplast(), *r;
        undotype = u->numents ? UNDO_ENT : UNDO_CUBE;
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
            r->timestamp = totalmillis;
            b.add(r);
        }
        pasteundo(u);
        if(!u->numents) changed(*u->block(), false);
        freeundo(u);
    }
    commitchanges();
    if(!hmapsel)
    {
        sel = l;
        reorient();
    }
    forcenextundo();

    return undotype;
}

int editundo() { return swapundo(undos, redos, EDIT_UNDO); }
int editredo() { return swapundo(redos, undos, EDIT_REDO); }

ICOMMAND(0, hasundos, "iN$", (int *n, int *numargs, ident *id),
{
    if(*numargs < 0) intret(undos.empty() ? 0 : 1);
    else printvar(id, undos.empty() ? 0 : 1);
});
ICOMMAND(0, hasredos, "iN$", (int *n, int *numargs, ident *id),
{
    if(*numargs < 0) intret(redos.empty() ? 0 : 1);
    else printvar(id, redos.empty() ? 0 : 1);
});

// guard against subdivision
#define protectsel(f) { undoblock *_u = newundocube(sel); f; if(_u) { pasteundo(_u); freeundo(_u); } }

vector<editinfo *> editinfos;
editinfo *localedit = NULL;

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

struct vslothdr
{
    ushort index;
    ushort slot;
};

static void packvslots(cube &c, vector<uchar> &buf, vector<ushort> &used)
{
    if(c.children)
    {
        loopi(8) packvslots(c.children[i], buf, used);
    }
    else loopi(6)
    {
        ushort index = c.texture[i];
        if(vslots.inrange(index) && vslots[index]->changed && used.find(index) < 0)
        {
            used.add(index);
            VSlot &vs = *vslots[index];
            vslothdr &hdr = *(vslothdr *)buf.pad(sizeof(vslothdr));
            hdr.index = index;
            hdr.slot = vs.slot->index;
            lilswap(&hdr.index, 2);
            packvslot(buf, vs);
        }
    }
}

static void packvslots(block3 &b, vector<uchar> &buf)
{
    vector<ushort> used;
    cube *c = b.c();
    loopi(b.size()) packvslots(c[i], buf, used);
    memset(buf.pad(sizeof(vslothdr)), 0, sizeof(vslothdr));
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
    if(buf.get((uchar *)&hdr, sizeof(hdr)) < int(sizeof(hdr))) return false;
    lilswap(hdr.o.v, 3);
    lilswap(hdr.s.v, 3);
    lilswap(&hdr.grid, 1);
    lilswap(&hdr.orient, 1);
    if(hdr.size() > (1<<20) || hdr.grid <= 0 || hdr.grid > (1<<12)) return false;
    b = (block3 *)new (false) uchar[sizeof(block3)+hdr.size()*sizeof(cube)];
    if(!b) return false;
    *b = hdr;
    cube *c = b->c();
    memset(c, 0, b->size()*sizeof(cube));
    loopi(b->size()) unpackcube(c[i], buf);
    return true;
}

struct vslotmap
{
    int index;
    VSlot *vslot;

    vslotmap() {}
    vslotmap(int index, VSlot *vslot) : index(index), vslot(vslot) {}
};
static vector<vslotmap> unpackingvslots;

static void unpackvslots(cube &c, ucharbuf &buf)
{
    if(c.children)
    {
        loopi(8) unpackvslots(c.children[i], buf);
    }
    else loopi(6)
    {
        ushort tex = c.texture[i];
        loopvj(unpackingvslots) if(unpackingvslots[j].index == tex) { c.texture[i] = unpackingvslots[j].vslot->index; break; }
    }
}

static void unpackvslots(block3 &b, ucharbuf &buf)
{
    while(buf.remaining() >= int(sizeof(vslothdr)))
    {
        vslothdr &hdr = *(vslothdr *)buf.pad(sizeof(vslothdr));
        lilswap(&hdr.index, 2);
        if(!hdr.index) break;
        VSlot &vs = *lookupslot(hdr.slot, false).variants;
        VSlot ds;
        if(!unpackvslot(buf, ds, false)) break;
        if(vs.index < 0 || vs.index == DEFAULT_SKY) continue;
        VSlot *edit = editvslot(vs, ds);
        unpackingvslots.add(vslotmap(hdr.index, edit ? edit : &vs));
    }

    cube *c = b.c();
    loopi(b.size()) unpackvslots(c[i], buf);

    unpackingvslots.setsize(0);
}

static bool compresseditinfo(const uchar *inbuf, int inlen, uchar *&outbuf, int &outlen)
{
    uLongf len = compressBound(inlen);
    if(len > (1<<20)) return false;
    outbuf = new (false) uchar[len];
    if(!outbuf || compress2((Bytef *)outbuf, &len, (const Bytef *)inbuf, inlen, Z_BEST_COMPRESSION) != Z_OK || len > (1<<16))
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
    outbuf = new (false) uchar[len];
    if(!outbuf || uncompress((Bytef *)outbuf, &len, (const Bytef *)inbuf, inlen) != Z_OK)
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
    packvslots(*e->copy, buf);
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
    unpackvslots(*e->copy, buf);
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

struct undoenthdr
{
    ushort i;
    entbase e;
    uchar numattrs;
};

bool packundo(undoblock *u, int &inlen, uchar *&outbuf, int &outlen)
{
    vector<uchar> buf;
    buf.reserve(512);
    *(ushort *)buf.pad(2) = lilswap(ushort(u->numents));
    if(u->numents)
    {
        undoent *ue = u->ents();
        int numattrs = 0;
        undoenthdr *hdr = (undoenthdr *)buf.pad(u->numents*sizeof(undoenthdr));
        loopi(u->numents)
        {
            hdr[i].i = lilswap(ushort(ue[i].i));
            hdr[i].e = ue[i].e;
            hdr[i].numattrs = ue[i].numattrs;
            numattrs += ue[i].numattrs;
        }
        int *attrs = (int *)buf.pad(numattrs*sizeof(int));
        memcpy(attrs, u->attrs(), numattrs*sizeof(int));
        lilswap(attrs, numattrs);
    }
    else
    {
        block3 &b = *u->block();
        if(!packblock(b, buf)) return false;
        buf.put(u->gridmap(), b.size());
        packvslots(b, buf);
    }
    inlen = buf.length();
    return compresseditinfo(buf.getbuf(), buf.length(), outbuf, outlen);
}

bool unpackundo(const uchar *inbuf, int inlen, int outlen)
{
    uchar *outbuf = NULL;
    if(!uncompresseditinfo(inbuf, inlen, outbuf, outlen)) return false;
    ucharbuf buf(outbuf, outlen);
    if(buf.remaining() < 2)
    {
        delete[] outbuf;
        return false;
    }

    int numents = lilswap(*(const ushort *)buf.pad(2));
    if(numents)
    {
        if(buf.remaining() < numents*int(sizeof(undoenthdr)))
        {
            delete[] outbuf;
            return false;
        }
        undoenthdr *hdr = (undoenthdr *)buf.pad(numents*sizeof(undoenthdr));
        int numattrs = 0;
        loopi(numents)
        {
            lilswap(&hdr[i].i);
            numattrs += hdr[i].numattrs;
        }
        if(buf.remaining() < numattrs*int(sizeof(int)))
        {
            delete[] outbuf;
            return false;
        }
        int *attrs = (int *)buf.pad(numattrs*sizeof(int));
        lilswap(attrs, numattrs);
        loopi(numents)
        {
            pasteundoent(hdr[i].i, hdr[i].e, attrs, hdr[i].numattrs);
            attrs += hdr[i].numattrs;
        }
    }
    else
    {
        block3 *b = NULL;
        if(!unpackblock(b, buf) || b->grid >= worldsize || buf.remaining() < b->size())
        {
            freeblock(b);
            delete[] outbuf;
            return false;
        }
        uchar *g = buf.pad(b->size());
        unpackvslots(*b, buf);
        pasteundoblock(b, g);
        changed(*b, false);
        freeblock(b);
    }
    delete[] outbuf;
    commitchanges();
    return true;
}

bool packundo(int op, int &inlen, uchar *&outbuf, int &outlen)
{
    switch(op)
    {
        case EDIT_UNDO: return !undos.empty() && packundo(undos.last, inlen, outbuf, outlen);
        case EDIT_REDO: return !redos.empty() && packundo(redos.last, inlen, outbuf, outlen);
        default: return false;
    }
}

struct prefabheader
{
    char magic[4];
    int version;
};

struct prefab : editinfo
{
    char *name;
    GLuint ebo, vbo;
    int numtris, numverts;

    prefab() : name(NULL), ebo(0), vbo(0), numtris(0), numverts(0) {}
    ~prefab() { DELETEA(name); if(copy) freeblock(copy); }

    void cleanup()
    {
        if(ebo) { glDeleteBuffers_(1, &ebo); ebo = 0; }
        if(vbo) { glDeleteBuffers_(1, &vbo); vbo = 0; }
        numtris = numverts = 0;
    }
};

static hashnameset<prefab> prefabs;

void cleanupprefabs()
{
    enumerate(prefabs, prefab, p, p.cleanup());
}

void delprefab(char *name)
{
    prefab *p = prefabs.access(name);
    if(p)
    {
        p->cleanup();
        prefabs.remove(name);
        conoutf(colourwhite, "Deleted prefab %s", name);
    }
}
COMMAND(0, delprefab, "s");

void saveprefab(char *name)
{
    if(!name[0] || noedit(true) || (nompedit && multiplayer())) return;
    prefab *b = prefabs.access(name);
    if(!b)
    {
        b = &prefabs[name];
        b->name = newstring(name);
    }
    if(b->copy) freeblock(b->copy);
    protectsel(b->copy = blockcopy(block3(sel), sel.grid));
    changed(sel);
    defformatstring(filename, strpbrk(name, "/\\") ? "%s.obr" : "prefab/%s.obr", name);
    path(filename);
    stream *f = opengzfile(filename, "wb");
    if(!f) { conoutf(colourred, "Could not write prefab to %s", filename); return; }
    prefabheader hdr;
    memcpy(hdr.magic, "OEBR", 4);
    hdr.version = 0;
    lilswap(&hdr.version, 1);
    f->write(&hdr, sizeof(hdr));
    streambuf<uchar> s(f);
    if(!packblock(*b->copy, s)) { delete f; conoutf(colourred, "Could not pack prefab %s", filename); return; }
    delete f;
    conoutf(colourwhite, "Wrote prefab file %s", filename);
}
ICOMMAND(0, saveprefab, "s", (char *s), if(!(identflags&IDF_MAP)) saveprefab(s));

void pasteblock(block3 &b, selinfo &sel, bool local)
{
    sel.s = b.s;
    int o = sel.orient;
    sel.orient = b.orient;
    cube *s = b.c();
    loopselxyz(if(!isempty(*s) || s->children || s->material != MAT_AIR) pastecube(*s, c); s++); // 'transparent'. old opaque by 'delcube; paste'
    sel.orient = o;
}

prefab *loadprefab(const char *name, bool msg = true)
{
   prefab *b = prefabs.access(name);
   if(b) return b;

   defformatstring(filename, strpbrk(name, "/\\") ? "%s.obr" : "prefab/%s.obr", name);
   path(filename);
   stream *f = opengzfile(filename, "rb");
   if(!f) { if(msg) conoutf(colourred, "Could not read prefab %s", filename); return NULL; }
   prefabheader hdr;
   if(f->read(&hdr, sizeof(hdr)) != sizeof(prefabheader) || memcmp(hdr.magic, "OEBR", 4)) { delete f; if(msg) conoutf(colourred, "Prefab %s has malformatted header", filename); return NULL; }
   lilswap(&hdr.version, 1);
   if(hdr.version != 0) { delete f; if(msg) conoutf(colourred, "Prefab %s uses unsupported version", filename); return NULL; }
   streambuf<uchar> s(f);
   block3 *copy = NULL;
   if(!unpackblock(copy, s)) { delete f; if(msg) conoutf(colourred, "Could not unpack prefab %s", filename); return NULL; }
   delete f;

   b = &prefabs[name];
   b->name = newstring(name);
   b->copy = copy;

   return b;
}

/* Create a copy of block3 */
block3 *copyblock(block3 *s)
{
    int bsize = sizeof(block3)+sizeof(cube)*s->size();
    if(bsize <= 0 || bsize > (100<<20)) return 0;
    block3 *b = (block3 *)new uchar[bsize];
    *b = *s;
    loopi(s->size()) copycube(s->c()[i], b->c()[i]);
    return b;
}

/* Copy prefab `name` to clipboard */
void copyprefab(char *name)
{
    if(!name[0] || noedit()) return;
    prefab *b = loadprefab(name, true);
    if(!b) return;
    if(multiplayer(false)) client::edittrigger(sel, EDIT_COPY, 1);
    if(!localedit) localedit = editinfos.add(new editinfo);
    if(localedit->copy) freeblock(localedit->copy);
    localedit->copy = copyblock(b->copy);
}
COMMAND(0, copyprefab, "s");

struct prefabmesh
{
    struct vertex { vec pos; bvec4 norm; };

    static const int SIZE = 1<<9;
    int table[SIZE];
    vector<vertex> verts;
    vector<int> chain;
    vector<ushort> tris;

    prefabmesh() { memset(table, -1, sizeof(table)); }

    int addvert(const vertex &v)
    {
        uint h = hthash(v.pos)&(SIZE-1);
        for(int i = table[h]; i>=0; i = chain[i])
        {
            const vertex &c = verts[i];
            if(c.pos==v.pos && c.norm==v.norm) return i;
        }
        if(verts.length() >= int(USHRT_MAX)) return -1;
        verts.add(v);
        chain.add(table[h]);
        return table[h] = verts.length()-1;
    }

    int addvert(const vec &pos, const bvec &norm)
    {
        vertex vtx;
        vtx.pos = pos;
        vtx.norm = norm;
        return addvert(vtx);
   }

    void setup(prefab &p)
    {
        if(tris.empty()) return;

        p.cleanup();

        loopv(verts) verts[i].norm.flip();
        if(!p.vbo) glGenBuffers_(1, &p.vbo);
        gle::bindvbo(p.vbo);
        glBufferData_(GL_ARRAY_BUFFER, verts.length()*sizeof(vertex), verts.getbuf(), GL_STATIC_DRAW);
        gle::clearvbo();
        p.numverts = verts.length();

        if(!p.ebo) glGenBuffers_(1, &p.ebo);
        gle::bindebo(p.ebo);
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER, tris.length()*sizeof(ushort), tris.getbuf(), GL_STATIC_DRAW);
        gle::clearebo();
        p.numtris = tris.length()/3;
    }

};

static void genprefabmesh(prefabmesh &r, cube &c, const ivec &co, int size)
{
    if(c.children)
    {
        neighbourstack[++neighbourdepth] = c.children;
        loopi(8)
        {
            ivec o(i, co, size/2);
            genprefabmesh(r, c.children[i], o, size/2);
        }
        --neighbourdepth;
    }
    else if(!isempty(c))
    {
        int vis;
        loopi(6) if((vis = visibletris(c, i, co, size)))
        {
            ivec v[4];
            genfaceverts(c, i, v);
            int convex = 0;
            if(!flataxisface(c, i)) convex = faceconvexity(v);
            int order = vis&4 || convex < 0 ? 1 : 0, numverts = 0;
            vec vo(co), pos[4], norm[4];
            pos[numverts++] = vec(v[order]).mul(size/8.0f).add(vo);
            if(vis&1) pos[numverts++] = vec(v[order+1]).mul(size/8.0f).add(vo);
            pos[numverts++] = vec(v[order+2]).mul(size/8.0f).add(vo);
            if(vis&2) pos[numverts++] = vec(v[(order+3)&3]).mul(size/8.0f).add(vo);
            guessnormals(pos, numverts, norm);
            int index[4];
            loopj(numverts) index[j] = r.addvert(pos[j], bvec(norm[j]));
            loopj(numverts-2) if(index[0]!=index[j+1] && index[j+1]!=index[j+2] && index[j+2]!=index[0])
            {
                r.tris.add(index[0]);
                r.tris.add(index[j+1]);
                r.tris.add(index[j+2]);
            }
        }
    }
}

void genprefabmesh(prefab &p)
{
    block3 b = *p.copy;
    b.o = ivec(0, 0, 0);

    cube *oldworldroot = worldroot;
    int oldworldscale = worldscale, oldworldsize = worldsize;

    worldroot = newcubes();
    worldscale = 1;
    worldsize = 2;
    while(worldsize < max(max(b.s.x, b.s.y), b.s.z)*b.grid)
    {
        worldscale++;
        worldsize *= 2;
    }

    cube *s = p.copy->c();
    loopxyz(b, b.grid, if(!isempty(*s) || s->children) pastecube(*s, c); s++);

    prefabmesh r;
    neighbourstack[++neighbourdepth] = worldroot;
    loopi(8) genprefabmesh(r, worldroot[i], ivec(i, ivec(0, 0, 0), worldsize/2), worldsize/2);
    --neighbourdepth;
    r.setup(p);

    freeocta(worldroot);

    worldroot = oldworldroot;
    worldscale = oldworldscale;
    worldsize = oldworldsize;

    useshaderbyname("prefab");
}

static void renderprefab(prefab &p, const vec &o, float yaw, float pitch, float roll, float size, const vec &color, float blend)
{
    if(!p.numtris)
    {
        genprefabmesh(p);
        if(!p.numtris) return;
    }

    block3 &b = *p.copy;

    matrix4 m;
    m.identity();
    m.settranslation(o);
    if(yaw) m.rotate_around_z(yaw*RAD);
    if(pitch) m.rotate_around_x(pitch*RAD);
    if(roll) m.rotate_around_y(-roll*RAD);
    matrix3 w(m);
    if(size > 0 && size != 1) m.scale(size);
    m.translate(vec(b.s).mul(-b.grid*0.5f));

    gle::bindvbo(p.vbo);
    gle::bindebo(p.ebo);
    gle::enablevertex();
    gle::enablenormal();
    prefabmesh::vertex *v = (prefabmesh::vertex *)0;
    gle::vertexpointer(sizeof(prefabmesh::vertex), v->pos.v);
    gle::normalpointer(sizeof(prefabmesh::vertex), v->norm.v, GL_BYTE);

    matrix4 pm;
    pm.mul(camprojmatrix, m);
    GLOBALPARAM(prefabmatrix, pm);
    GLOBALPARAM(prefabworld, w);
    SETSHADER(prefab);
    gle::colorf(color.x*ldrscale, color.y*ldrscale, color.z*ldrscale, blend);
    glDrawRangeElements_(GL_TRIANGLES, 0, p.numverts-1, p.numtris*3, GL_UNSIGNED_SHORT, (ushort *)0);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    enablepolygonoffset(GL_POLYGON_OFFSET_LINE);

    pm.mul(camprojmatrix, m);
    GLOBALPARAM(prefabmatrix, pm);
    SETSHADER(prefab);
    gle::colorf(outlinecolour.x*ldrscale, outlinecolour.y*ldrscale, outlinecolour.z*ldrscale, blend);
    glDrawRangeElements_(GL_TRIANGLES, 0, p.numverts-1, p.numtris*3, GL_UNSIGNED_SHORT, (ushort *)0);

    disablepolygonoffset(GL_POLYGON_OFFSET_LINE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    gle::disablevertex();
    gle::disablenormal();
    gle::clearebo();
    gle::clearvbo();
}

void renderprefab(const char *name, const vec &o, float yaw, float pitch, float roll, float size, const vec &color, float blend)
{
    prefab *p = loadprefab(name, false);
    if(p) renderprefab(*p, o, yaw, pitch, roll, size, color, blend);
}

void previewprefab(const char *name, const vec &color, float blend, float yaw, float offsetyaw)
{
    prefab *p = loadprefab(name, false);
    if(p)
    {
        block3 &b = *p->copy;
        float ryaw = yaw;
        vec o = calcmodelpreviewpos(vec(b.s).mul(b.grid*0.5f), ryaw);
        ryaw += offsetyaw;
        renderprefab(*p, o, ryaw, 0, 0, 1, color, blend);
    }
}

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
    if(localedit->copy) sel.s = localedit->copy->s;
    reorient();
    havesel = true;
}

void paste()
{
    if(noedit(true) || !localedit) return;
    mppaste(localedit, sel, true);
}

COMMAND(0, copy, "");
COMMAND(0, pasteclear, "");
COMMAND(0, pastehilight, "");
COMMAND(0, paste, "");
ICOMMAND(0, undo, "", (), intret(editundo()));
ICOMMAND(0, redo, "", (), intret(editredo()));

static vector<int *> editingvslots;
struct vslotref
{
    vslotref(int &index) { editingvslots.add(&index); }
    ~vslotref() { editingvslots.pop(); }
};
#define editingvslot(...) vslotref vslotrefs[] = { __VA_ARGS__ }; (void)vslotrefs;

void compacteditvslots()
{
    loopv(editingvslots) if(*editingvslots[i]) compactvslot(*editingvslots[i]);
    loopv(unpackingvslots) compactvslot(*unpackingvslots[i].vslot);
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

namespace hmap
{
    vector<int> textures;

    void cancel() { textures.setsize(0); }

    ICOMMAND(0, hmapcancel, "", (), cancel());
    ICOMMAND(0, hmapselect, "", (),
        int t = lookupcube(cur).texture[orient];
        int i = textures.find(t);
        if(i<0)
            textures.add(t);
        else
            textures.remove(i);
    );

    inline bool isheightmap(int o, int d, bool empty, cube *c)
    {
        return havesel ||
            (empty && isempty(*c)) ||
            textures.empty() ||
            textures.find(c->texture[o]) >= 0;
    }

    #define MAXBRUSH    64
    #define MAXBRUSHC   63
    #define MAXBRUSH2   32
    int brush[MAXBRUSH][MAXBRUSH];
    VAR(0, brushx, 0, MAXBRUSH2, MAXBRUSH);
    VAR(0, brushy, 0, MAXBRUSH2, MAXBRUSH);
    bool paintbrush = 0;
    int brushmaxx = 0, brushminx = MAXBRUSH;
    int brushmaxy = 0, brushminy = MAXBRUSH;
    char *brushname = NULL;

    void clearbrush()
    {
        memset(brush, 0, sizeof brush);
        brushmaxx = brushmaxy = 0;
        brushminx = brushminy = MAXBRUSH;
        DELETEA(brushname);
        paintbrush = false;
    }
    COMMAND(0, clearbrush, "");

    void setbrushname(const char *name)
    {
        if(!name || !*name) return;
        DELETEA(brushname);
        brushname = newstring(name);
    }
    ICOMMAND(0, brushname, "sN$", (char *name, int *numargs, ident *id),
    {
        if(*numargs > 0) setbrushname(name);
        else if(*numargs < 0) result(brushname ? brushname : "");
        else printsvar(id, brushname ? brushname : "");
    });

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
    COMMAND(0, brushvert, "iii");

    void brushimport(const char *name)
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
        else conoutf(colourred, "Could not load: %s", name);
    }

    COMMAND(0, brushimport, "s");

    #define PAINTED     1
    #define NOTHMAP     2
    #define MAPPED      16
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
        cube *c = &lookupcube(t, gridsize);
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
        makeundo(hundo);

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
            pullhmap(worldsize*8, <, >, 0, 8, +);

        cube **c  = cmap[x][y];
        int e[2][2];
        int notempty = 0;

        loopk(4) if(c[k])
        {
            loopi(2) loopj(2)
            {
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
        hws= (worldsize>>gridpower);
        gx = (cur[R[d]] >> gridpower) + cx - MAXBRUSH2;
        gy = (cur[C[d]] >> gridpower) + cy - MAXBRUSH2;
        gz = (cur[D[d]] >> gridpower);
        fs = dc ? 4 : 0;
        fg = dc ? gridsize : -gridsize;
        mx = max(0, -gx); // ripple range
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
        nz = worldsize-gridsize;
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

void edithmap(int dir, int mode)
{
    if((nompedit && multiplayer()) || !hmapsel) return;
    hmap::run(dir, mode);
}

///////////// main cube edit ////////////////

int bounded(int n) { return n<0 ? 0 : (n>8 ? 8 : n); }

int pushedge(uchar &edge, int dir, int dc)
{
    int ne = bounded(edgeget(edge, dc)+dir);
    edgeset(edge, dc, ne);
    int oe = edgeget(edge, 1-dc);
    if((dir<0 && dc && oe>ne) || (dir>0 && dc==0 && oe<ne)) edgeset(edge, 1-dc, ne);

    return ne;
}

int linkedpush(cube &c, int d, int x, int y, int dc, int dir)
{
    ivec v, p;
    getcubevector(c, d, x, y, dc, v);
    int minedge = 8, newedge = 0;

    loopi(2) loopj(2)
    {
        getcubevector(c, d, i, j, dc, p);
        if(v==p)
        {
            newedge = pushedge(cubeedge(c, d, i, j), dir, dc);
            if(newedge < minedge) minedge = newedge;
        }
    }

    return minedge;
}

void linkedset(cube &c, int d, int x, int y, int dc, int val)
{
    ivec v, p;
    getcubevector(c, d, x, y, dc, v);

    loopi(2) loopj(2)
    {
        getcubevector(c, d, i, j, dc, p);
        if(v==p)
            edgeset(cubeedge(c, d, i, j), bounded(val), dc);
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

int editfaceinternal(int dir, int mode, selinfo &sel, bool local)
{
    int d = dimension(sel.orient);
    int dc = dimcoord(sel.orient);
    int seldir = dc ? -dir : dir;
    int state = 8;

    if(mode==1)
    {
        int h = sel.o[d]+dc*sel.grid;
        if(((dir>0) == dc && h<=0) || ((dir<0) == dc && h>=worldsize)) return 0;
        if(dir<0) sel.o[d] += sel.grid * seldir;
        state = dir;
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
                state = linkedpush(c, d, sel.corner&1, sel.corner>>1, dc, seldir); // corner command
            else
            {
                loop(mx,2) loop(my,2)                                       // pull/push edges command
                {
                    if(x==0 && mx==0 && sel.cx) continue;
                    if(y==0 && my==0 && sel.cy) continue;
                    if(x==sel.s[R[d]]-1 && mx==1 && (sel.cx+sel.cxs)&1) continue;
                    if(y==sel.s[C[d]]-1 && my==1 && (sel.cy+sel.cys)&1) continue;
                    if(p[mx+my*2] != ((uchar *)&bak)[mx+my*2]) continue;

                    int minedge = linkedpush(c, d, mx, my, dc, seldir);
                    if(minedge < state) state = minedge;
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

    return state;
}

VAR(0, editfacekeepsel, 0, 0, 1);

int mpeditface(int dir, int mode, selinfo &sel, bool local)
{
    if(mode==1 && (sel.cx || sel.cy || sel.cxs&1 || sel.cys&1)) mode = 0;

    if(local)
        client::edittrigger(sel, EDIT_FACE, dir, mode);

    selinfo oldsel = sel;
    int state = editfaceinternal(dir, mode, sel, local);

    if(editfacekeepsel)
    {
        sel = oldsel;
        int d = dimension(sel.orient);
        int dc = dimcoord(sel.orient);
        //int seldir = dc ? -dir : -dir;

        sel.s[d] += -dir;
        if(!dc) sel.o[d] += sel.grid * dir;
    }

    return state;
}

int editface(int *dir, int *mode)
{
    int state = 0;

    if(noedit(moving!=0)) return 0;
    if(hmapedit!=1)
        state = mpeditface(*dir, *mode, sel, true);
    else
        edithmap(*dir, *mode);

    return state;
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
        player->resetinterp(true);
    }
}

void mpdelcube(selinfo &sel, bool local)
{
    if(local) client::edittrigger(sel, EDIT_DELCUBE);
    loopselxyz(discardchildren(c, true); emptyfaces(c));
}

void delcube()
{
    if(noedit(true)) return;
    mpdelcube(sel, true);
}

COMMAND(0, pushsel, "i");
ICOMMAND(0, editface, "ii", (int *dir, int *mode), intret(editface(dir, mode)));
COMMAND(0, delcube, "");

/////////// texture editing //////////////////

selinfo repsel;
int reptex = -1;

static vector<vslotmap> remappedvslots;

static VSlot *remapvslot(int index, bool delta, const VSlot &ds)
{
    loopv(remappedvslots) if(remappedvslots[i].index == index) return remappedvslots[i].vslot;
    VSlot &vs = lookupvslot(index, false);
    if(vs.index < 0 || vs.index == DEFAULT_SKY) return NULL;
    VSlot *edit = NULL;
    if(delta)
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

static void remapvslots(cube &c, bool delta, const VSlot &ds, int orient, bool &findrep, VSlot *&findedit)
{
    if(c.children)
    {
        loopi(8) remapvslots(c.children[i], delta, ds, orient, findrep, findedit);
        return;
    }
    static VSlot ms;
    if(orient<0) loopi(6)
    {
        VSlot *edit = remapvslot(c.texture[i], delta, ds);
        if(edit)
        {
            c.texture[i] = edit->index;
            if(!findedit) findedit = edit;
        }
    }
    else
    {
        int i = visibleorient(c, orient);
        VSlot *edit = remapvslot(c.texture[i], delta, ds);
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

void mpeditvslot(int delta, VSlot &ds, int allfaces, selinfo &sel, bool local)
{
    if(local)
    {
        client::edittrigger(sel, EDIT_VSLOT, delta, allfaces, 0, &ds);
        if(!(lastsel==sel)) tofronttex();
        if(allfaces || !(repsel == sel)) reptex = -1;
        repsel = sel;
    }
    bool findrep = local && !allfaces && reptex < 0;
    VSlot *findedit = NULL;
    loopselxyz(remapvslots(c, delta != 0, ds, allfaces ? -1 : sel.orient, findrep, findedit));
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

bool mpeditvslot(int delta, int allfaces, selinfo &sel, ucharbuf &buf)
{
    VSlot ds;
    if(!unpackvslot(buf, ds, delta != 0)) return false;
    editingvslot(ds.layer, ds.detail);
    mpeditvslot(delta, ds, allfaces, sel, false);
    return true;
}

VAR(0, allfaces, 0, 0, 1);
VAR(0, usevdelta, 1, 0, 0);

void vdelta(uint *body)
{
    if(noedit()) return;
    usevdelta++;
    execute(body);
    usevdelta--;
}
COMMAND(IDF_NOECHO, vdelta, "e");

void vrotate(int *n)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_ROTATION;
    ds.rotation = usevdelta ? *n : clamp(*n, 0, 7);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, vrotate, "i");
ICOMMAND(0, getvrotate, "ii", (int *tex, int *type), intret(universallookup(*tex, *type).rotation));

void vangle(float *a)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_ANGLE;
    ds.angle = vec(*a, sinf(RAD**a), cosf(RAD**a));
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, vangle, "f");
ICOMMAND(0, getvangle, "ii", (int *tex, int *type), floatret(universallookup(*tex, *type).angle.x));

void voffset(int *x, int *y)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_OFFSET;
    ds.offset = usevdelta ? ivec2(*x, *y) : ivec2(*x, *y).max(0);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, voffset, "ii");
ICOMMAND(0, getvoffset, "ii", (int *tex, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    defformatstring(str, "%d %d", vslot.offset.x, vslot.offset.y);
    result(str);
});

void vscroll(float *s, float *t)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_SCROLL;
    ds.scroll = vec2(*s/1000.0f, *t/1000.0f);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, vscroll, "ff");
ICOMMAND(0, getvscroll, "ii", (int *tex, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    defformatstring(str, "%s %s", floatstr(vslot.scroll.x*1000.0f), floatstr(vslot.scroll.y*1000.0f));
    result(str);
});

void vscale(float *scale)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_SCALE;
    ds.scale = *scale <= 0 ? 1 : (usevdelta ? *scale : clamp(*scale, 1/8.0f, 8.0f));
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, vscale, "f");
ICOMMAND(0, getvscale, "ii", (int *tex, int *type), floatret(universallookup(*tex, *type).scale));

void vlayer(int *n)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_LAYER;
    if(vslots.inrange(*n))
    {
        ds.layer = *n;
        if(vslots[ds.layer]->changed && nompedit && multiplayer()) return;
    }
    editingvslot(ds.layer);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, vlayer, "i");
ICOMMAND(0, getvlayer, "ii", (int *tex, int *type), intret(universallookup(*tex, *type).layer));

void vdetail(int *n)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_DETAIL;
    if(vslots.inrange(*n))
    {
        ds.detail = *n;
        if(vslots[ds.detail]->changed && nompedit && multiplayer()) return;
    }
    editingvslot(ds.detail);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, vdetail, "i");
ICOMMAND(0, getvdetail, "ii", (int *tex, int *type), intret(universallookup(*tex, *type).detail));

void valpha(float *front, float *back)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_ALPHA;
    ds.alphafront = clamp(*front, 0.0f, 1.0f);
    ds.alphaback = clamp(*back, 0.0f, 1.0f);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, valpha, "ff");
ICOMMAND(0, getvalpha, "ii", (int *tex, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    defformatstring(str, "%s %s", floatstr(vslot.alphafront), floatstr(vslot.alphaback));
    result(str);
});

void vcolour(float *r, float *g, float *b)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_COLOR;
    ds.colorscale = vec(max(*r, 0.0f), max(*g, 0.0f), max(*b, 0.0f));
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, vcolour, "fff");
ICOMMAND(0, getvcolour, "ii", (int *tex, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    result(vslot.colorscale);
});

void vpalette(int *p, int *x)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_PALETTE;
    ds.palette = max(*p, 0);
    ds.palindex = max(*x, 0);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, vpalette, "ii");
ICOMMAND(0, getvpalette, "ii", (int *tex, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    defformatstring(str, "%d %d", vslot.palette, vslot.palindex);
    result(str);
});

void vshadow(float *shadow)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_SHADOW;
    ds.shadow = *shadow;
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, vshadow, "f");
ICOMMAND(0, getvshadow, "fi", (float *tex, int *type), floatret(universallookup(*tex, *type).shadow));

void vrefract(float *k, float *r, float *g, float *b)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_REFRACT;
    ds.refractscale = clamp(*k, 0.0f, 1.0f);
    if(ds.refractscale > 0 && (*r > 0 || *g > 0 || *b > 0))
        ds.refractcolor = vec(clamp(*r, 0.0f, 1.0f), clamp(*g, 0.0f, 1.0f), clamp(*b, 0.0f, 1.0f));
    else
        ds.refractcolor = vec(1, 1, 1);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);

}
COMMAND(0, vrefract, "ffff");
ICOMMAND(0, getvrefract, "ii", (int *tex, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    defformatstring(str, "%s %s %s %s", floatstr(vslot.refractscale), floatstr(vslot.refractcolor.r), floatstr(vslot.refractcolor.g), floatstr(vslot.refractcolor.b));
    result(str);
});

void vcoastscale(float *value)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_COAST;
    ds.coastscale = clamp(*value, 0.f, 1000.f);
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, vcoastscale, "f");
ICOMMAND(0, getvcoastscale, "ii", (int *tex, int *type), floatret(universallookup(*tex, *type).coastscale));

void vreset()
{
    if(noedit()) return;
    VSlot ds;
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, vreset, "");

void vshaderparam(const char *name, float *x, float *y, float *z, float *w, int *palette, int *palindex)
{
    if(noedit()) return;
    VSlot ds;
    ds.changed = 1<<VSLOT_SHPARAM;
    if(name[0])
    {
        SlotShaderParam p = { getshaderparamname(name), -1, 0, *palette, *palindex, {*x, *y, *z, *w} };
        ds.params.add(p);
    }
    mpeditvslot(usevdelta, ds, allfaces, sel, true);
}
COMMAND(0, vshaderparam, "sffffii");
ICOMMAND(0, getvshaderparam, "isi", (int *tex, const char *name, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    float pal = 0;
    float palidx = 0;
    float *params = findslotparam(vslot, name, pal, palidx);

    if(params)
    {
        defformatstring(str, "%s %s %s %s", floatstr(params[0]), floatstr(params[1]),
            floatstr(params[2]), floatstr(params[3]));
        if(pal || palidx) concformatstring(str, " %d %d", int(pal), int(palidx));
        result(str);
    }
});
ICOMMAND(0, getvshaderparamnames, "ii", (int *tex, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    vector<char> str;
    loopv(vslot.params)
    {
        SlotShaderParam &p = vslot.params[i];
        if(i) str.put(' ');
        str.put(p.name, strlen(p.name));
    }
    str.add('\0');
    stringret(newstring(str.getbuf(), str.length()-1));
});

ICOMMAND(0, getvshadername, "ii", (int *tex, int *type), result(universallookup(*tex, *type).slot->shader->name));
ICOMMAND(0, getvshadertype, "ii", (int *tex, int *type), intret(universallookup(*tex, *type).slot->shader->type));

ICOMMAND(0, getvindex, "ii", (int *tex, int *type), intret(universallookup(*tex, *type).slot->index));
ICOMMAND(0, getvsmooth, "ii", (int *tex, int *type), intret(universallookup(*tex, *type).slot->smooth));
ICOMMAND(0, getvtexmask, "ii", (int *tex, int *type), intret(universallookup(*tex, *type).slot->texmask));

ICOMMAND(0, getvgrassname, "i", (int *tex),  result(lookupvslot(*tex, false).slot->grass));
ICOMMAND(0, getvgrasscolour, "i", (int *tex),
{
    VSlot &vslot = lookupvslot(*tex, false);
    result(vslot.slot->grasscolor);
});
ICOMMAND(0, getvgrassblend, "i", (int *tex), floatret(lookupvslot(*tex, false).slot->grassblend));
ICOMMAND(0, getvgrasscale, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grassscale));
ICOMMAND(0, getvgrassheight, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grassheight));

ICOMMAND(0, getvgrasstexname, "i", (int *tex), result(lookupvslot(*tex, false).slot->grasstex->name));
ICOMMAND(0, getvgrasstextype, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->type));
ICOMMAND(0, getvgrasstexwidth, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->w));
ICOMMAND(0, getvgrasstexheight, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->h));
ICOMMAND(0, getvgrasstexxs, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->xs));
ICOMMAND(0, getvgrasstexys, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->ys));
ICOMMAND(0, getvgrasstexbpp, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->bpp));
ICOMMAND(0, getvgrasstexclamp, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->tclamp));
ICOMMAND(0, getvgrasstexframe, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->frame));
ICOMMAND(0, getvgrasstexdelay, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->delay));
ICOMMAND(0, getvgrasstexlast, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->last));
ICOMMAND(0, getvgrasstexthrob, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->throb ? 1 : 0));
ICOMMAND(0, getvgrasstextype, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->type));
ICOMMAND(0, getvgrasstexframes, "i", (int *tex), intret(lookupvslot(*tex, false).slot->grasstex->frames.length()));

ICOMMAND(0, getvtexcount, "ii", (int *tex, int *type), intret(universallookup(*tex, *type).slot->sts.length()));
ICOMMAND(0, getvtexname, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts)) return;
    result(vslot.slot->sts[*sts].name);
});
ICOMMAND(0, getvtextype, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts)) return;
    intret(vslot.slot->sts[*sts].type);
});
ICOMMAND(0, getvtexcombined, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts)) return;
    intret(vslot.slot->sts[*sts].combined);
});
ICOMMAND(0, getvteximgname, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    result(vslot.slot->sts[*sts].t->name);
});
ICOMMAND(0, getvteximgtype, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->type);
});
ICOMMAND(0, getvteximgwidth, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->w);
});
ICOMMAND(0, getvteximgheight, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->h);
});
ICOMMAND(0, getvteximgxs, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->xs);
});
ICOMMAND(0, getvteximgys, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->ys);
});
ICOMMAND(0, getvteximgbpp, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->bpp);
});
ICOMMAND(0, getvteximgclamp, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->tclamp);
});
ICOMMAND(0, getvteximgframe, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->frame);
});
ICOMMAND(0, getvteximgdelay, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->delay);
});
ICOMMAND(0, getvteximglast, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->last);
});
ICOMMAND(0, getvteximgmipmap, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->mipmap ? 1 : 0);
});
ICOMMAND(0, getvteximgthrob, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->throb ? 1 : 0);
});
ICOMMAND(0, getvteximgframes, "iii", (int *tex, int *sts, int *type),
{
    VSlot &vslot = universallookup(*tex, *type);
    if(!vslot.slot->sts.inrange(*sts) || !vslot.slot->sts[*sts].t) return;
    intret(vslot.slot->sts[*sts].t->frames.length());
});

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

static int unpacktex(int &tex, ucharbuf &buf, bool insert = true)
{
    if(tex < 0x10000) return true;
    VSlot ds;
    if(!unpackvslot(buf, ds, false)) return false;
    VSlot &vs = *lookupslot(tex & 0xFFFF, false).variants;
    if(vs.index < 0 || vs.index == DEFAULT_SKY) return false;
    VSlot *edit = insert ? editvslot(vs, ds) : findvslot(*vs.slot, vs, ds);
    if(!edit) return false;
    tex = edit->index;
    return true;
}

int shouldpacktex(int index)
{
    if(vslots.inrange(index))
    {
        VSlot &vs = *vslots[index];
        if(vs.changed) return 0x10000 + vs.slot->index;
    }
    return 0;
}

bool mpedittex(int tex, int allfaces, selinfo &sel, ucharbuf &buf)
{
    if(!unpacktex(tex, buf)) return false;
    mpedittex(tex, allfaces, sel, false);
    return true;
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

void edittex(int i, bool save, bool edit)
{
    lasttex = i;
    lasttexmillis = totalmillis;
    if(save)
    {
        loopvj(texmru) if(texmru[j]==lasttex) { curtexindex = j; break; }
    }
    if(edit) mpedittex(i, allfaces, sel, true);
}

void edittex_(int *dir)
{
    if(noedit()) return;
    filltexlist();
    if(texmru.empty()) return;
    texpaneltimer = totalmillis;
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
    if(noedit(true))
    {
        intret(-1);
        return;
    }
    filltexlist();
    int index = curtexindex < 0 ? 0 : curtexindex;
    if(!texmru.inrange(index))
    {
        intret(-1);
        return;
    }
    intret(texmru[index]);
}

void getseltex(int *allcubes)
{
    if(noedit(true))
    {
        intret(-1);
        return;
    }

    int tex = -1;

    if(*allcubes)
    {
        loopxyz(sel, -sel.grid, {
            if(!c.children && !isempty(c))
            {
                tex = c.texture[sel.orient];
                goto end;
            }
        });
    }
    else
    {
        cube &c = lookupcube(sel.o, -sel.grid);
        if(!c.children && !isempty(c)) tex = c.texture[sel.orient];
    }

    end:
    intret(tex);
}

void gettexname(int *tex, int *subslot)
{
    if(*tex<0) return;
    VSlot &vslot = lookupvslot(*tex, false);
    Slot &slot = *vslot.slot;
    if(!slot.sts.inrange(*subslot)) return;
    result(slot.sts[*subslot].name);
}

void getdecalname(int *decal, int *subslot)
{
    if(*decal<0) return;
    DecalSlot &slot = lookupdecalslot(*decal, false);
    if(!slot.sts.inrange(*subslot)) return;
    result(slot.sts[*subslot].name);
}

void getslottex(int *idx)
{
    if(*idx < 0 || !slots.inrange(*idx)) { intret(-1); return; }
    Slot &slot = lookupslot(*idx, false);
    intret(slot.variants->index);
}

void gettextags(int *idx)
{
    if(*idx < 0 || !slots.inrange(*idx)) { intret(-1); return; }
    Slot &slot = lookupslot(*idx, false);
    result(slot.tags);
}

COMMANDN(0, edittex, edittex_, "i");
ICOMMAND(0, settex, "i", (int *tex), { if(!vslots.inrange(*tex) || noedit()) return; filltexlist(); edittex(*tex); });
COMMAND(0, gettex, "");
COMMAND(0, getcurtex, "");
COMMAND(0, getseltex, "i");
ICOMMAND(0, getreptex, "", (), { if(!noedit()) intret(vslots.inrange(reptex) ? reptex : -1); });
COMMAND(0, gettexname, "ii");
COMMAND(0, getdecalname, "ii");
ICOMMAND(0, texmru, "b", (int *idx), { filltexlist(); intret(texmru.inrange(*idx) ? texmru[*idx] : texmru.length()); });
ICOMMAND(0, texmrufind, "i", (int *idx), { filltexlist(); intret(texmru.find(*idx)); });
COMMAND(0, getslottex, "i");
ICOMMAND(0, texloaded, "i", (int *tex), intret(slots.inrange(*tex) && slots[*tex]->loaded ? 1 : 0));
COMMAND(0, gettextags, "i");

ICOMMANDV(0, numvslots, vslots.length());
ICOMMANDV(0, numslots, slots.length());
ICOMMANDV(0, numdecalslots, decalslots.length());

int getseltexs(int idx)
{
    static int lastgetseltex = 0;
    static vector<int> seltexs;
    if(lastgetseltex != totalmillis)
    {
        seltexs.shrink(0);
        loopxyz(sel, -sel.grid,
        {
            if(c.children || isempty(c)) continue;

            bool found = false;
            int tex = lookupslot(universallookup(c.texture[sel.orient], TEXSLOT_NORMAL).slot->index, false).variants->index;
            loopv(seltexs) if(seltexs[i] == tex)
            {
                found = true;
                break;
            }
            if(found) continue;
            seltexs.add(tex);
        });
        lastgetseltex = totalmillis;
    }
    if(idx < 0) return seltexs.length();
    if(seltexs.inrange(idx)) return seltexs[idx];
    return -1;
}
ICOMMAND(0, getseltexs, "b", (int *idx), intret(getseltexs(*idx)));

ICOMMAND(0, texhasvariants, "i", (int *index),
{
    if(slots.inrange(*index)) intret(slots[*index]->variants->next != NULL);
    else intret(0);
});

#define LOOPTEXMRU(name,op) \
    ICOMMAND(0, looptexmru##name, "iire", (int *count, int *skip, ident *id, uint *body), \
    { \
        filltexlist(); \
        loopstart(id, stack); \
        op(texmru, *count, *skip, \
        { \
            loopiter(id, stack, texmru[i]); \
            execute(body); \
        }); \
        loopend(id, stack); \
    }); \
    ICOMMAND(0, looptexmru##name##if, "iiree", (int *count, int *skip, ident *id, uint *cond, uint *body), \
    { \
        filltexlist(); \
        loopstart(id, stack); \
        op(texmru, *count, *skip, \
        { \
            loopiter(id, stack, texmru[i]); \
            if(executebool(cond)) execute(body); \
        }); \
        loopend(id, stack); \
    }); \
    ICOMMAND(0, looptexmru##name##while, "iiree", (int *count, int *skip, ident *id, uint *cond, uint *body), \
    { \
        filltexlist(); \
        loopstart(id, stack); \
        op(texmru, *count, *skip, \
        { \
            loopiter(id, stack, texmru[i]); \
            if(!executebool(cond)) break; \
            execute(body); \
        }); \
        loopend(id, stack); \
    });
LOOPTEXMRU(,loopcsv);
LOOPTEXMRU(rev,loopcsvrev);

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

bool mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, ucharbuf &buf)
{
    if(!unpacktex(oldtex, buf, false)) return false;
    editingvslot(oldtex);
    if(!unpacktex(newtex, buf)) return false;
    mpreplacetex(oldtex, newtex, insel, sel, false);
    return true;
}

void replacetex(bool insel, int oldtex = -1, int newtex = -1)
{
    if(noedit(!insel)) return;
    mpreplacetex(oldtex, vslots.inrange(newtex) ? newtex : lasttex, insel, sel, true);
}

ICOMMAND(0, replace, "", (),
{
    if(!vslots.inrange(reptex)) { conoutf(colourred, "Can only replace after a valid texture edit"); return; }
    replacetex(false, reptex);
});
ICOMMAND(0, replacesel, "", (),
{
    if(!vslots.inrange(reptex)) { conoutf(colourred, "Can only replace after a valid texture edit"); return; }
    replacetex(true, reptex);
});
ICOMMAND(0, replacetex, "bb", (int *n, int *o),
{
    if(!vslots.inrange(*n)) { conoutf(colourred, "Can't replace texture with %d as it does not exist", *n); return; }
    if(!vslots.inrange(*o)) { conoutf(colourred, "Can't replace texture %d as it does not exist", *o); return; }
    replacetex(false, *o, *n);
});
ICOMMAND(0, replacetexsel, "bb", (int *n, int *o),
{
    if(!vslots.inrange(*n)) { conoutf(colourred, "Can't replace texture with %d as it does not exist", *n); return; }
    if(!vslots.inrange(*o)) { conoutf(colourred, "Can't replace texture %d as it does not exist", *o); return; }
    replacetex(true, *o, *n);
});
ICOMMAND(0, replaceall, "b", (int *n), replacetex(false, -1, *n));
ICOMMAND(0, replaceallsel, "b", (int *n), replacetex(true, -1, *n));

void resettexmru()
{
    int old = texmru.inrange(curtexindex) ? texmru[curtexindex] : -1;
    texmru.shrink(0);
    loopv(vslots) texmru.add(i);
    curtexindex = texmru.find(old);
}
ICOMMAND(0, resettexmru, "", (void), resettexmru());

////////// flip and rotate ///////////////
static inline uint dflip(uint face) { return face==F_EMPTY ? face : 0x88888888 - (((face&0xF0F0F0F0)>>4) | ((face&0x0F0F0F0F)<<4)); }
static inline uint cflip(uint face) { return ((face&0xFF00FF00)>>8) | ((face&0x00FF00FF)<<8); }
static inline uint rflip(uint face) { return ((face&0xFFFF0000)>>16)| ((face&0x0000FFFF)<<16); }
static inline uint mflip(uint face) { return (face&0xFF0000FF) | ((face&0x00FF0000)>>8) | ((face&0x0000FF00)<<8); }

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

static inline void rotatequad(cube &a, cube &b, cube &c, cube &d)
{
    cube t = a; a = b; b = c; c = d; d = t;
}

void rotatecube(cube &c, int d) // rotates cube clockwise. see pics in cvs for help.
{
    c.faces[D[d]] = cflip(mflip(c.faces[D[d]]));
    c.faces[C[d]] = dflip(mflip(c.faces[C[d]]));
    c.faces[R[d]] = rflip(mflip(c.faces[R[d]]));
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
        else c.material &= (filtermat ? ~filtermat : 0);
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
        if(filter < 0) { conoutf(colourred, "Unknown material \"%s\"", filtername); return; }
    }
    int id = -1;
    if(name[0] || filter < 0)
    {
        id = findmaterial(name, true);
        if(id<0) { conoutf(colourred, "Unknown material \"%s\"", name); return; }
    }
    mpeditmat(id, filter, *style, sel, true);
}
COMMAND(0, editmat, "ssi");

#define EDITSTAT(name, type, val) \
    ICOMMAND(0, editstat##name, "", (), \
    { \
        static int laststat = 0; \
        static type prevstat = 0; \
        static type curstat = 0; \
        if(totalmillis - laststat >= hud::statrate) \
        { \
            prevstat = curstat; \
            laststat = totalmillis - (totalmillis%hud::statrate); \
        } \
        if(prevstat == curstat) curstat = (val); \
        type##ret(curstat); \
    });
EDITSTAT(wtr, int, wtris/1024);
EDITSTAT(vtr, int, (vtris*100)/max(wtris, 1));
EDITSTAT(wvt, int, wverts/1024);
EDITSTAT(vvt, int, (vverts*100)/max(wverts, 1));
EDITSTAT(evt, int, xtraverts/1024);
EDITSTAT(eva, int, xtravertsva/1024);
EDITSTAT(octa, int, allocnodes*8);
EDITSTAT(va, int, allocva);
EDITSTAT(glde, int, glde);
EDITSTAT(geombatch, int, gbatches);
EDITSTAT(oq, int, getnumqueries());
EDITSTAT(pvs, int, getnumviewcells());

