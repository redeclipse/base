#define MAXLIGHTNINGSTEPS 64
#define LIGHTNINGSTEP 8
int lnjitterx[2][MAXLIGHTNINGSTEPS], lnjittery[2][MAXLIGHTNINGSTEPS];
int lnjitterframe = 0, lastlnjitter = 0;

VAR(IDF_PERSIST, lnjittermillis, 0, 100, 1000);
VAR(IDF_PERSIST, lnjitterradius, 0, 4, 100);
FVAR(IDF_PERSIST, lnjitterscale, 0, 0.5f, 10);
VAR(IDF_PERSIST, lnscrollmillis, 1, 300, 5000);
FVAR(IDF_PERSIST, lnscrollscale, 0, 0.125f, 10);
FVAR(IDF_PERSIST, lnblendpower, 0, 0.25f, 1000);

static void calclightningjitter(int frame)
{
    loopi(MAXLIGHTNINGSTEPS)
    {
        lnjitterx[lnjitterframe][i] = -lnjitterradius + rnd(2*lnjitterradius + 1);
        lnjittery[lnjitterframe][i] = -lnjitterradius + rnd(2*lnjitterradius + 1);
    }
}

static void setuplightning()
{
    if(!lastlnjitter || lastmillis-lastlnjitter > lnjittermillis)
    {
        if(!lastlnjitter) calclightningjitter(lnjitterframe);
        lastlnjitter = lastmillis - (lastmillis%lnjittermillis);
        calclightningjitter(lnjitterframe ^= 1);
    }
}

static void renderlightning(Texture *tex, const vec &o, const vec &d, float sz, const bvec4 &midcol, const bvec4 &endcol)
{
    vec step(d);
    step.sub(o);
    float len = step.magnitude();
    int numsteps = clamp(int(ceil(len/LIGHTNINGSTEP)), 2, MAXLIGHTNINGSTEPS);
    step.div(numsteps+1);
    int jitteroffset = detrnd(int(d.x+d.y+d.z), MAXLIGHTNINGSTEPS);
    vec cur(o), up, right;
    up.orthogonal(step);
    up.normalize();
    right.cross(up, step);
    right.normalize();
    float scroll = -float(lastmillis%lnscrollmillis)/lnscrollmillis,
          scrollscale = lnscrollscale*(LIGHTNINGSTEP*tex->ys)/(sz*tex->xs),
          blend = pow(clamp(float(lastmillis - lastlnjitter)/lnjittermillis, 0.0f, 1.0f), lnblendpower),
          jitter0 = (1-blend)*lnjitterscale*sz/lnjitterradius, jitter1 = blend*lnjitterscale*sz/lnjitterradius,
          fadescale = sz/step.magnitude();
    glBegin(GL_TRIANGLE_STRIP);
    loopj(numsteps)
    {
        vec next(cur);
        next.add(step);
        if(j+1==numsteps) next = d;
        else
        {
            int lj = (j+jitteroffset)%MAXLIGHTNINGSTEPS;
            next.add(vec(right).mul((jitter1*lnjitterx[lnjitterframe][lj] + jitter0*lnjitterx[lnjitterframe^1][lj])));
            next.add(vec(up).mul((jitter1*lnjittery[lnjitterframe][lj] + jitter0*lnjittery[lnjitterframe^1][lj])));
        }
        vec dir1 = next, dir2 = next, across;
        dir1.sub(cur);
        dir2.sub(camera1->o);
        across.cross(dir2, dir1).normalize().mul(sz);
        if(!j)
        {
            vec start = vec(cur).sub(vec(step).mul(fadescale));
            float startscroll = scroll - scrollscale*fadescale;
            glColor4ubv(endcol.v); glTexCoord2f(startscroll, 1); glVertex3f(start.x-across.x, start.y-across.y, start.z-across.z);
            glColor4ubv(endcol.v); glTexCoord2f(startscroll, 0); glVertex3f(start.x+across.x, start.y+across.y, start.z+across.z);
        }
        glColor4ubv(midcol.v); glTexCoord2f(scroll, 1); glVertex3f(cur.x-across.x, cur.y-across.y, cur.z-across.z);
        glColor4ubv(midcol.v); glTexCoord2f(scroll, 0); glVertex3f(cur.x+across.x, cur.y+across.y, cur.z+across.z);
        scroll += scrollscale;
        if(j+1==numsteps)
        {
            glColor4ubv(midcol.v); glTexCoord2f(scroll, 1); glVertex3f(next.x-across.x, next.y-across.y, next.z-across.z);
            glColor4ubv(midcol.v); glTexCoord2f(scroll, 0); glVertex3f(next.x+across.x, next.y+across.y, next.z+across.z);
            vec end = vec(next).add(vec(step).mul(fadescale));
            float endscroll = scroll + scrollscale*fadescale;
            glColor4ubv(endcol.v); glTexCoord2f(endscroll, 1); glVertex3f(end.x-across.x, end.y-across.y, end.z-across.z);
            glColor4ubv(endcol.v); glTexCoord2f(endscroll, 0); glVertex3f(end.x+across.x, end.y+across.y, end.z+across.z);
        }
        cur = next;
    }
    glEnd();
}

struct lightningrenderer : sharedlistrenderer
{
    lightningrenderer(const char *texname)
        : sharedlistrenderer(texname, 2, PT_LIGHTNING|PT_GLARE)
    {}

    void startrender()
    {
        glDisable(GL_CULL_FACE);
    }

    void endrender()
    {
        glEnable(GL_CULL_FACE);
    }

    void update()
    {
        setuplightning();
    }

    void renderpart(sharedlistparticle *p, int blend, int ts, float size)
    {
        blend = int(min(blend<<2, 255)*p->blend);
        bvec4 midcol, endcol;
        if(type&PT_MOD) //multiply alpha into color
        {
            midcol = bvec4((p->color.r*blend)>>8, (p->color.g*blend)>>8, (p->color.b*blend)>>8, 0xFF);
            endcol = bvec4(0, 0, 0, 0xFF);
        }
        else
        {
            midcol = bvec4(p->color, blend);
            endcol = bvec4(p->color);
        }
        renderlightning(tex, p->o, p->d, size, midcol, endcol);
    }
};
static lightningrenderer lightnings("<grey>particles/lightning"), lightzaps("<grey>particles/lightzap");
