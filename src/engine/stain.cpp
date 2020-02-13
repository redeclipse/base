#include "engine.h"

struct stainvert
{
    vec pos;
    bvec4 color;
    vec2 tc;
};

struct staininfo
{
    int millis;
    bvec color;
    uchar owner;
    ushort startvert, endvert;
};

enum
{
    SF_RND4       = 1<<0,
    SF_ROTATE     = 1<<1,
    SF_INVMOD     = 1<<2,
    SF_OVERBRIGHT = 1<<3,
    SF_GLOW       = 1<<4,
    SF_SATURATE   = 1<<5
};

VARF(IDF_PERSIST, maxstaintris, 1, 2048, 16384, initstains());
VAR(IDF_PERSIST, stainfade, 1000, 15000, 60000);
VAR(0, dbgstain, 0, 0, 1);

struct stainbuffer
{
    stainvert *verts;
    int maxverts, startvert, endvert, lastvert, availverts;
    GLuint vbo;
    bool dirty;

    stainbuffer() : verts(NULL), maxverts(0), startvert(0), endvert(0), lastvert(0), availverts(0), vbo(0), dirty(false)
    {}

    ~stainbuffer()
    {
        DELETEA(verts);
    }

    void init(int tris)
    {
        if(verts)
        {
            DELETEA(verts);
            maxverts = startvert = endvert = lastvert = availverts = 0;
        }
        if(tris)
        {
            maxverts = tris*3 + 3;
            availverts = maxverts - 3;
            verts = new stainvert[maxverts];
        }
    }

    void cleanup()
    {
        if(vbo) { glDeleteBuffers_(1, &vbo); vbo = 0; }
    }

    void clear()
    {
        startvert = endvert = lastvert = 0;
        availverts = max(maxverts - 3, 0);
        dirty = true;
    }

    int freestain(const staininfo &d)
    {
        int removed = d.endvert < d.startvert ? maxverts - (d.startvert - d.endvert) : d.endvert - d.startvert;
        startvert = d.endvert;
        if(startvert==endvert) startvert = endvert = lastvert = 0;
        availverts += removed;
        return removed;
    }

    void clearstains(const staininfo &d)
    {
        startvert = d.endvert;
        availverts = endvert < startvert ? startvert - endvert - 3 : maxverts - 3 - (endvert - startvert);
        dirty = true;
    }

    bool faded(const staininfo &d) const { return verts[d.startvert].color.a < 255; }

    void fadestain(const staininfo &d, const bvec4 &color)
    {
        stainvert *vert = &verts[d.startvert],
                  *end = &verts[d.endvert < d.startvert ? maxverts : d.endvert];
        while(vert < end)
        {
            vert->color = color;
            vert++;
        }
        if(d.endvert < d.startvert)
        {
            vert = verts;
            end = &verts[d.endvert];
            while(vert < end)
            {
                vert->color = color;
                vert++;
            }
        }
        dirty = true;
    }

    void render()
    {
        if(startvert == endvert) return;

        if(!vbo) { glGenBuffers_(1, &vbo); dirty = true; }
        gle::bindvbo(vbo);

        int count = endvert < startvert ? maxverts - startvert : endvert - startvert;
        if(dirty)
        {
            glBufferData_(GL_ARRAY_BUFFER, maxverts*sizeof(stainvert), NULL, GL_STREAM_DRAW);
            glBufferSubData_(GL_ARRAY_BUFFER, 0, count*sizeof(stainvert), &verts[startvert]);
            if(endvert < startvert)
            {
                glBufferSubData_(GL_ARRAY_BUFFER, count*sizeof(stainvert), endvert*sizeof(stainvert), verts);
                count += endvert;
            }
            dirty = false;
        }
        else if(endvert < startvert) count += endvert;

        const stainvert *ptr = 0;
        gle::vertexpointer(sizeof(stainvert), ptr->pos.v);
        gle::texcoord0pointer(sizeof(stainvert), ptr->tc.v);
        gle::colorpointer(sizeof(stainvert), ptr->color.v);

        glDrawArrays(GL_TRIANGLES, 0, count);
        xtravertsva += count;
    }

    stainvert *addtri()
    {
        stainvert *tri = &verts[endvert];
        availverts -= 3;
        endvert += 3;
        if(endvert >= maxverts) endvert = 0;
        return tri;
    }

    void addstain(staininfo &d)
    {
        dirty = true;
    }

    bool hasverts() const { return startvert != endvert; }

    int nextverts() const
    {
        return endvert < lastvert ? endvert + maxverts - lastvert : endvert - lastvert;
    }

    int totalverts() const
    {
        return endvert < startvert ? maxverts - (startvert - endvert) : endvert - startvert;
    }

    int totaltris() const
    {
        return (maxverts - 3 - availverts)/3;
    }
};

struct stainrenderer
{
    const char *texname;
    int flags, fadeintime, fadeouttime, timetolive;
    Texture *tex;
    staininfo *stains;
    int maxstains, startstain, endstain;
    stainbuffer verts[NUMSTAINBUFS];

    stainrenderer(const char *texname, int flags = 0, int fadeintime = 0, int fadeouttime = 1000, int timetolive = -1)
        : texname(texname), flags(flags),
          fadeintime(fadeintime), fadeouttime(fadeouttime), timetolive(timetolive),
          tex(NULL),
          stains(NULL), maxstains(0), startstain(0), endstain(0),
          stainu(0), stainv(0)
    {
    }

    ~stainrenderer()
    {
        DELETEA(stains);
    }

    bool usegbuffer() const { return !(flags&(SF_INVMOD|SF_GLOW)); }

    void init(int tris)
    {
        if(stains)
        {
            DELETEA(stains);
            maxstains = startstain = endstain = 0;
        }
        stains = new staininfo[tris];
        maxstains = tris;
        loopi(NUMSTAINBUFS) verts[i].init(i == STAINBUF_TRANSPARENT ? tris/2 : tris);
    }

    void preload()
    {
        tex = textureload(texname, 3);
    }

    int totalstains()
    {
        return endstain < startstain ? maxstains - (startstain - endstain) : endstain - startstain;
    }

    bool hasstains(int sbuf)
    {
        return verts[sbuf].hasverts();
    }

    void clearstains()
    {
        startstain = endstain = 0;
        loopi(NUMSTAINBUFS) verts[i].clear();
    }

    int freestain()
    {
        if(startstain==endstain) return 0;

        staininfo &d = stains[startstain];
        startstain++;
        if(startstain >= maxstains) startstain = 0;

        return verts[d.owner].freestain(d);
    }

    bool faded(const staininfo &d) const { return verts[d.owner].faded(d); }

    void fadestain(const staininfo &d, uchar alpha)
    {
        bvec color = d.color;
        if(flags&(SF_OVERBRIGHT|SF_GLOW|SF_INVMOD)) color.scale(alpha, 255);
        verts[d.owner].fadestain(d, bvec4(color, alpha));
    }

    void clearfadedstains()
    {
        int threshold = lastmillis - (timetolive>=0 ? timetolive : stainfade) - fadeouttime;
        staininfo *d = &stains[startstain],
                  *end = &stains[endstain < startstain ? maxstains : endstain],
                  *cleared[NUMSTAINBUFS] = { NULL };
        for(; d < end && d->millis <= threshold; d++)
            cleared[d->owner] = d;
        if(d >= end && endstain < startstain)
            for(d = stains, end = &stains[endstain]; d < end && d->millis <= threshold; d++)
                cleared[d->owner] = d;
        startstain = d - stains;
        if(startstain == endstain) loopi(NUMSTAINBUFS) verts[i].clear();
        else loopi(NUMSTAINBUFS) if(cleared[i]) verts[i].clearstains(*cleared[i]);
    }

    void fadeinstains()
    {
        if(!fadeintime) return;
        staininfo *d = &stains[endstain],
                  *end = &stains[endstain < startstain ? 0 : startstain];
        while(d > end)
        {
            d--;
            int fade = lastmillis - d->millis;
            if(fade < fadeintime) fadestain(*d, (fade<<8)/fadeintime);
            else if(faded(*d)) fadestain(*d, 255);
            else return;
        }
        if(endstain < startstain)
        {
            d = &stains[maxstains];
            end = &stains[startstain];
            while(d > end)
            {
                d--;
                int fade = lastmillis - d->millis;
                if(fade < fadeintime) fadestain(*d, (fade<<8)/fadeintime);
                else if(faded(*d)) fadestain(*d, 255);
                else return;
            }
        }
    }

    void fadeoutstains()
    {
        staininfo *d = &stains[startstain],
                  *end = &stains[endstain < startstain ? maxstains : endstain];
        int offset = (timetolive>=0 ? timetolive : stainfade) + fadeouttime - lastmillis;
        while(d < end)
        {
            int fade = d->millis + offset;
            if(fade >= fadeouttime) return;
            fadestain(*d, (fade<<8)/fadeouttime);
            d++;
        }
        if(endstain < startstain)
        {
            d = stains;
            end = &stains[endstain];
            while(d < end)
            {
                int fade = d->millis + offset;
                if(fade >= fadeouttime) return;
                fadestain(*d, (fade<<8)/fadeouttime);
                d++;
            }
        }
    }

    static void setuprenderstate(int sbuf, bool gbuf, int layer)
    {
        if(gbuf) maskgbuffer(sbuf == STAINBUF_TRANSPARENT ? "cg" : "c");
        else zerofogcolor();

        if(layer && ghasstencil)
        {
            glStencilFunc(GL_EQUAL, layer, 0x07);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        }

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

        enablepolygonoffset(GL_POLYGON_OFFSET_FILL);

        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);

        gle::enablevertex();
        gle::enabletexcoord0();
        gle::enablecolor();
    }

    static void cleanuprenderstate(int sbuf, bool gbuf, int layer)
    {
        gle::clearvbo();

        gle::disablevertex();
        gle::disabletexcoord0();
        gle::disablecolor();

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        disablepolygonoffset(GL_POLYGON_OFFSET_FILL);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        if(gbuf) maskgbuffer(sbuf == STAINBUF_TRANSPARENT ? "cndg" : "cnd");
        else resetfogcolor();
    }

    void cleanup()
    {
        loopi(NUMSTAINBUFS) verts[i].cleanup();
    }

    void render(int sbuf)
    {
        float colorscale = 1, alphascale = 1;
        if(flags&SF_OVERBRIGHT)
        {
            glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
            SETVARIANT(overbrightstain, sbuf == STAINBUF_TRANSPARENT ? 0 : -1, 0);
        }
        else if(flags&SF_GLOW)
        {
            glBlendFunc(GL_ONE, GL_ONE);
            colorscale = ldrscale;
            if(flags&SF_SATURATE) colorscale *= 2;
            alphascale = 0;
            SETSHADER(foggedstain);
        }
        else if(flags&SF_INVMOD)
        {
            glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
            alphascale = 0;
            SETSHADER(foggedstain);
        }
        else
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            colorscale = ldrscale;
            if(flags&SF_SATURATE) colorscale *= 2;
            SETVARIANT(stain, sbuf == STAINBUF_TRANSPARENT ? 0 : -1, 0);
        }
        LOCALPARAMF(colorscale, colorscale, colorscale, colorscale, alphascale);

        glBindTexture(GL_TEXTURE_2D, tex->id);

        verts[sbuf].render();
    }

    staininfo &newstain()
    {
        staininfo &d = stains[endstain];
        int next = endstain + 1;
        if(next>=maxstains) next = 0;
        if(next==startstain) freestain();
        endstain = next;
        return d;
    }

    ivec bbmin, bbmax;
    vec staincenter, stainnormal, staintangent, stainbitangent;
    float stainradius, stainu, stainv;
    bvec4 staincolor;

    void addstain(const vec &center, const vec &dir, float radius, const bvec &color, int info)
    {
        if(dir.iszero()) return;

        bbmin = ivec(center).sub(radius);
        bbmax = ivec(center).add(radius).add(1);

        staincolor = bvec4(color, 255);
        staincenter = center;
        stainradius = radius;
        stainnormal = dir;
#if 0
        staintangent.orthogonal(dir);
#else
        staintangent = vec(dir.z, -dir.x, dir.y);
        staintangent.project(dir);
#endif
        if(flags&SF_ROTATE) staintangent.rotate(sincos360[rnd(360)], dir);
        staintangent.normalize();
        stainbitangent.cross(staintangent, dir);
        if(flags&SF_RND4)
        {
            stainu = 0.5f*(info&1);
            stainv = 0.5f*((info>>1)&1);
        }

        loopi(NUMSTAINBUFS) verts[i].lastvert = verts[i].endvert;
        gentris(worldroot, ivec(0, 0, 0), worldsize>>1);
        loopi(NUMSTAINBUFS)
        {
            stainbuffer &buf = verts[i];
            if(buf.endvert == buf.lastvert) continue;

            if(dbgstain)
            {
                int nverts = buf.nextverts();
                static const char * const sbufname[NUMSTAINBUFS] = { "opaque", "transparent", "mapmodel" };
                conoutf("tris = %d, verts = %d, total tris = %d, %s", nverts/3, nverts, buf.totaltris(), sbufname[i]);
            }

            staininfo &d = newstain();
            d.owner = i;
            d.color = color;
            d.millis = lastmillis;
            d.startvert = buf.lastvert;
            d.endvert = buf.endvert;
            buf.addstain(d);
        }
    }

    void gentris(cube &cu, int orient, const ivec &o, int size, materialsurface *mat = NULL, int vismask = 0)
    {
        vec pos[MAXFACEVERTS+4];
        int numverts = 0, numplanes = 1;
        vec planes[2];
        if(mat)
        {
            planes[0] = vec(0, 0, 0);
            switch(orient)
            {
            #define GENFACEORIENT(orient, v0, v1, v2, v3) \
                case orient: \
                    planes[0][dimension(orient)] = dimcoord(orient) ? 1 : -1; \
                    v0 v1 v2 v3 \
                    break;
            #define GENFACEVERT(orient, vert, x,y,z, xv,yv,zv) \
                    pos[numverts++] = vec(x xv, y yv, z zv);
                GENFACEVERTS(o.x, o.x, o.y, o.y, o.z, o.z, , + mat->csize, , + mat->rsize, + 0.1f, - 0.1f);
            #undef GENFACEORIENT
            #undef GENFACEVERT
            }
        }
        else if(cu.texture[orient] == DEFAULT_SKY) return;
        else if(cu.ext && (numverts = cu.ext->surfaces[orient].numverts&MAXFACEVERTS))
        {
            vertinfo *verts = cu.ext->verts() + cu.ext->surfaces[orient].verts;
            ivec vo = ivec(o).mask(~0xFFF).shl(3);
            loopj(numverts) pos[j] = vec(verts[j].getxyz().add(vo)).mul(1/8.0f);
            planes[0].cross(pos[0], pos[1], pos[2]).normalize();
            if(numverts >= 4 && !(cu.merged&(1<<orient)) && !flataxisface(cu, orient) && faceconvexity(verts, numverts, size))
            {
                planes[1].cross(pos[0], pos[2], pos[3]).normalize();
                numplanes++;
            }
        }
        else if(cu.merged&(1<<orient)) return;
        else if(!vismask || (vismask&0x40 && visibleface(cu, orient, o, size, MAT_AIR, (cu.material&MAT_ALPHA)^MAT_ALPHA, MAT_ALPHA)))
        {
            ivec v[4];
            genfaceverts(cu, orient, v);
            int vis = 3, convex = faceconvexity(v, vis), order = convex < 0 ? 1 : 0;
            vec vo(o);
            pos[numverts++] = vec(v[order]).mul(size/8.0f).add(vo);
            if(vis&1) pos[numverts++] = vec(v[order+1]).mul(size/8.0f).add(vo);
            pos[numverts++] = vec(v[order+2]).mul(size/8.0f).add(vo);
            if(vis&2) pos[numverts++] = vec(v[(order+3)&3]).mul(size/8.0f).add(vo);
            planes[0].cross(pos[0], pos[1], pos[2]).normalize();
            if(convex) { planes[1].cross(pos[0], pos[2], pos[3]).normalize(); numplanes++; }
        }
        else return;

        stainbuffer &buf = verts[mat || cu.material&MAT_ALPHA ? STAINBUF_TRANSPARENT : STAINBUF_OPAQUE];
        loopl(numplanes)
        {
            const vec &n = planes[l];
            float facing = n.dot(stainnormal);
            if(facing <= 0) continue;
            vec p = vec(pos[0]).sub(staincenter);
#if 0
            // intersect ray along stain normal with plane
            float dist = n.dot(p) / facing;
            if(fabs(dist) > stainradius) continue;
            vec pcenter = vec(stainnormal).mul(dist).add(staincenter);
#else
            // travel back along plane normal from the stain center
            float dist = n.dot(p);
            if(fabs(dist) > stainradius) continue;
            vec pcenter = vec(n).mul(dist).add(staincenter);
#endif
            vec ft, fb;
            ft.orthogonal(n);
            ft.normalize();
            fb.cross(ft, n);
            vec pt = vec(ft).mul(ft.dot(staintangent)).add(vec(fb).mul(fb.dot(staintangent))).normalize(),
                pb = vec(ft).mul(ft.dot(stainbitangent)).add(vec(fb).mul(fb.dot(stainbitangent))).project(pt).normalize();
            vec v1[MAXFACEVERTS+4], v2[MAXFACEVERTS+4];
            float ptc = pt.dot(pcenter), pbc = pb.dot(pcenter);
            int numv;
            if(numplanes >= 2)
            {
                if(l) { pos[1] = pos[2]; pos[2] = pos[3]; }
                numv = polyclip(pos, 3, pt, ptc - stainradius, ptc + stainradius, v1);
                if(numv<3) continue;
            }
            else
            {
                numv = polyclip(pos, numverts, pt, ptc - stainradius, ptc + stainradius, v1);
                if(numv<3) continue;
            }
            numv = polyclip(v1, numv, pb, pbc - stainradius, pbc + stainradius, v2);
            if(numv<3) continue;
            float tsz = flags&SF_RND4 ? 0.5f : 1.0f, scale = tsz*0.5f/stainradius,
                  tu = stainu + tsz*0.5f - ptc*scale, tv = stainv + tsz*0.5f - pbc*scale;
            pt.mul(scale); pb.mul(scale);
            stainvert dv1 = { v2[0], staincolor, vec2(pt.dot(v2[0]) + tu, pb.dot(v2[0]) + tv) },
                      dv2 = { v2[1], staincolor, vec2(pt.dot(v2[1]) + tu, pb.dot(v2[1]) + tv) };
            int totalverts = 3*(numv-2);
            if(totalverts > buf.maxverts-3) return;
            while(buf.availverts < totalverts)
            {
                if(!freestain()) return;
            }
            loopk(numv-2)
            {
                stainvert *tri = buf.addtri();
                tri[0] = dv1;
                tri[1] = dv2;
                dv2.pos = v2[k+2];
                dv2.tc = vec2(pt.dot(v2[k+2]) + tu, pb.dot(v2[k+2]) + tv);
                tri[2] = dv2;
            }
        }
    }

    void findmaterials(vtxarray *va)
    {
        materialsurface *matbuf = va->matbuf;
        int matsurfs = va->matsurfs;
        loopi(matsurfs)
        {
            materialsurface &m = matbuf[i];
            if(!isclipped(m.material&MATF_VOLUME)) { i += m.skip; continue; }
            int dim = dimension(m.orient), dc = dimcoord(m.orient);
            if(dc ? stainnormal[dim] <= 0 : stainnormal[dim] >= 0) { i += m.skip; continue; }
            int c = C[dim], r = R[dim];
            for(;;)
            {
                materialsurface &m = matbuf[i];
                if(m.o[dim] >= bbmin[dim] && m.o[dim] <= bbmax[dim] &&
                   m.o[c] + m.csize >= bbmin[c] && m.o[c] <= bbmax[c] &&
                   m.o[r] + m.rsize >= bbmin[r] && m.o[r] <= bbmax[r])
                {
                    static cube dummy;
                    gentris(dummy, m.orient, m.o, max(m.csize, m.rsize), &m);
                }
                if(i+1 >= matsurfs) break;
                materialsurface &n = matbuf[i+1];
                if(n.material != m.material || n.orient != m.orient) break;
                i++;
            }
        }
    }

    void findescaped(cube *c, const ivec &o, int size, int escaped)
    {
        loopi(8)
        {
            cube &cu = c[i];
            if(escaped&(1<<i))
            {
                ivec co(i, o, size);
                if(cu.children) findescaped(cu.children, co, size>>1, cu.escaped);
                else
                {
                    int vismask = cu.merged;
                    if(vismask) loopj(6) if(vismask&(1<<j)) gentris(cu, j, co, size);
                }
            }
        }
    }

    void genmmtri(const vec v[3])
    {
        vec n;
        n.cross(v[0], v[1], v[2]).normalize();
        float facing = n.dot(stainnormal);
        if(facing <= 0) return;

        vec p = vec(v[0]).sub(staincenter);
#if 0
        float dist = n.dot(p) / facing;
        if(fabs(dist) > stainradius) return;
        vec pcenter = vec(stainnormal).mul(dist).add(staincenter);
#else
        float dist = n.dot(p);
        if(fabs(dist) > stainradius) return;
        vec pcenter = vec(n).mul(dist).add(staincenter);
#endif

        vec ft, fb;
        ft.orthogonal(n);
        ft.normalize();
        fb.cross(ft, n);
        vec pt = vec(ft).mul(ft.dot(staintangent)).add(vec(fb).mul(fb.dot(staintangent))).normalize(),
            pb = vec(ft).mul(ft.dot(stainbitangent)).add(vec(fb).mul(fb.dot(stainbitangent))).project(pt).normalize();
        vec v1[3+4], v2[3+4];
        float ptc = pt.dot(pcenter), pbc = pb.dot(pcenter);
        int numv = polyclip(v, 3, pt, ptc - stainradius, ptc + stainradius, v1);
        if(numv<3) return;
        numv = polyclip(v1, numv, pb, pbc - stainradius, pbc + stainradius, v2);
        if(numv<3) return;
        float tsz = flags&SF_RND4 ? 0.5f : 1.0f, scale = tsz*0.5f/stainradius,
              tu = stainu + tsz*0.5f - ptc*scale, tv = stainv + tsz*0.5f - pbc*scale;
        pt.mul(scale); pb.mul(scale);
        stainvert dv1 = { v2[0], staincolor, vec2(pt.dot(v2[0]) + tu, pb.dot(v2[0]) + tv) },
                  dv2 = { v2[1], staincolor, vec2(pt.dot(v2[1]) + tu, pb.dot(v2[1]) + tv) };
        int totalverts = 3*(numv-2);
        stainbuffer &buf = verts[STAINBUF_MAPMODEL];
        if(totalverts > buf.maxverts-3) return;
        while(buf.availverts < totalverts)
        {
            if(!freestain()) return;
        }
        loopk(numv-2)
        {
            stainvert *tri = buf.addtri();
            tri[0] = dv1;
            tri[1] = dv2;
            dv2.pos = v2[k+2];
            dv2.tc = vec2(pt.dot(v2[k+2]) + tu, pb.dot(v2[k+2]) + tv);
            tri[2] = dv2;
        }
    }

    void genmmtris(octaentities &oe)
    {
        const vector<extentity *> &ents = entities::getents();
        loopv(oe.mapmodels)
        {
            extentity &e = *ents[oe.mapmodels[i]];
            model *m = loadmapmodel(e.attrs[0]);
            if(!m) continue;

            vec center, radius;
            float rejectradius = m->collisionbox(center, radius), scale = e.attrs[5] > 0 ? e.attrs[5]/100.0f : 1;
            center.mul(scale);
            if(staincenter.reject(vec(e.viewpos).add(center), stainradius + rejectradius*scale)) continue;

            if(m->animated() || (!m->bih && !m->setBIH())) continue;

            int yaw = e.attrs[1], pitch = e.attrs[2], roll = e.attrs[3];

            m->bih->genstaintris(this, staincenter, stainradius, e.viewpos, yaw, pitch, roll, scale);
        }
    }

    void gentris(cube *c, const ivec &o, int size, int escaped = 0)
    {
        int overlap = octaboxoverlap(o, size, bbmin, bbmax);
        loopi(8)
        {
            cube &cu = c[i];
            if(overlap&(1<<i))
            {
                ivec co(i, o, size);
                if(cu.ext)
                {
                    if(cu.ext->va && cu.ext->va->matsurfs) findmaterials(cu.ext->va);
                    if(cu.ext->ents && cu.ext->ents->mapmodels.length()) genmmtris(*cu.ext->ents);
                }
                if(cu.children) gentris(cu.children, co, size>>1, cu.escaped);
                else
                {
                    int vismask = cu.visible;
                    if(vismask&0xC0)
                    {
                        if(vismask&0x80) loopj(6) gentris(cu, j, co, size, NULL, vismask);
                        else loopj(6) if(vismask&(1<<j)) gentris(cu, j, co, size);
                    }
                }
            }
            else if(escaped&(1<<i))
            {
                ivec co(i, o, size);
                if(cu.children) findescaped(cu.children, co, size>>1, cu.escaped);
                else
                {
                    int vismask = cu.merged;
                    if(vismask) loopj(6) if(vismask&(1<<j)) gentris(cu, j, co, size);
                }
            }
        }
    }
};

stainrenderer stains[] =
{
    stainrenderer("<grey>particles/scorch", SF_ROTATE, 500, 1000, 10000),
    stainrenderer("<grey>particles/scorch", SF_ROTATE, 500, 1000, 2000),
    stainrenderer("<grey>particles/blood", SF_RND4|SF_ROTATE|SF_INVMOD, 0, 1000, 10000),
    stainrenderer("<grey>particles/bullet", SF_OVERBRIGHT, 0, 1000, 10000),
    stainrenderer("<grey>particles/energy", SF_ROTATE|SF_GLOW|SF_SATURATE, 150, 500, 3000),
    stainrenderer("<grey>particles/stain", SF_SATURATE, 100, 900, 1000),
    stainrenderer("<grey>particles/smoke", SF_ROTATE, 500, 1000, 10000)
};

void initstains()
{
    if(initing) return;
    loopi(sizeof(stains)/sizeof(stains[0])) stains[i].init(maxstaintris);
    loopi(sizeof(stains)/sizeof(stains[0]))
    {
        loadprogress = float(i+1)/(sizeof(stains)/sizeof(stains[0]));
        stains[i].preload();
    }
    loadprogress = 0;
}

void clearstains()
{
    loopi(sizeof(stains)/sizeof(stains[0])) stains[i].clearstains();
}

VARN(IDF_PERSIST, stains, showstains, 0, 1, 1);

bool renderstains(int sbuf, bool gbuf, int layer)
{
    bool rendered = false;
    loopi(sizeof(stains)/sizeof(stains[0]))
    {
        stainrenderer &d = stains[i];
        if(d.usegbuffer() != gbuf) continue;
        if(sbuf == STAINBUF_OPAQUE)
        {
            d.clearfadedstains();
            d.fadeinstains();
            d.fadeoutstains();
        }
        if(!showstains || !d.hasstains(sbuf)) continue;
        if(!rendered)
        {
            rendered = true;
            stainrenderer::setuprenderstate(sbuf, gbuf, layer);
        }
        d.render(sbuf);
    }
    if(!rendered) return false;
    stainrenderer::cleanuprenderstate(sbuf, gbuf, layer);
    return true;
}

void cleanupstains()
{
    loopi(sizeof(stains)/sizeof(stains[0])) stains[i].cleanup();
}

VAR(IDF_PERSIST, maxstaindistance, 1, 512, 10000);

void addstain(int type, const vec &center, const vec &surface, float radius, const bvec &color, int info)
{
    int id = type-1;
    if(!showstains || type<=0 || (size_t)type>sizeof(stains)/sizeof(stains[0]) || center.dist(camera1->o) - radius > maxstaindistance) return;
    stainrenderer &d = stains[id];
    d.addstain(center, surface, radius, color, info);
}

void genstainmmtri(stainrenderer *s, const vec v[3])
{
    s->genmmtri(v);
}

