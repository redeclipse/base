// worldio.cpp: loading & saving of maps

#include "engine.h"

extern const namemap mapexts[] = {
    { ".mpz", MAP_MAPZ },
    { ".ogz", MAP_OCTA },
};

extern const namemap mapdirs[] = {
    { "maps", MAP_MAPZ },
    { "base", MAP_OCTA },
};

VAR(0, maptype, 1, -1, -1);
VAR(0, maploading, 1, 0, -1);
VAR(0, mapcrc, 1, 0, -1);
SVAR(0, mapfile, "");
SVAR(0, mapname, "");
SVAR(0, maptext, "");

VAR(IDF_PERSIST, autosavebackups, 0, 2, 4); // make backups; 0 = off, 1 = single backup, 2 = named backup, 3/4 = same as 1/2 with move to "backups/"
VAR(IDF_PERSIST, autosavemapshot, 0, 1, 1);
VAR(IDF_PERSIST, autosaveconfigs, 0, 1, 1);
VAR(IDF_PERSIST, autosavetexts, 0, 1, 1);

void fixmaptitle()
{
    string s; // remove colour from these things in RE
    if(filterstring(s, maptitle)) setsvar("maptitle", s, false);
    const char *title = maptitle, *author = strstr(title, " by ");
    if(author && *author)
    {
        char *t = newstring(title, author-title);
        if(t)
        {
            if(*t)
            {
                loopi(4) if(*author) author++;
                if(*author) setsvar("mapauthor", author, true);
                setsvar("maptitle", t, false);
            }
            delete[] t;
        }
    }
}

SVARF(IDF_WORLD, maptitle, "", fixmaptitle());
SVARF(IDF_WORLD, mapauthor, "", { string s; if(filterstring(s, mapauthor)) setsvar("mapauthor", s, false); });

void setnames(const char *fname, int type, int crc)
{
    maptype = type >= 0 || type <= MAP_MAX-1 ? type : MAP_MAPZ;

    string fn, mn, mf;
    if(fname && *fname)
    {
        const char *fcrc = strstr(fname, "_0x");
        if(fcrc && *fcrc)
        {
            char *t = newstring(fname, fcrc-fname);
            if(t)
            {
                if(*t) copystring(fn, t);
                else copystring(fn, fname);
                delete[] t;
            }
            else copystring(fn, fname);
        }
        else copystring(fn, fname);
        if(crc > 0)
        {
            defformatstring(cn, "_0x%.8x", crc);
            concatstring(fn, cn);
        }
        else if(crc < 0) concatstring(fn, "_0x0");
    }
    else formatstring(fn, "%s/untitled", mapdirs[maptype].name);

    if(strpbrk(fn, "/\\")) copystring(mn, fn);
    else formatstring(mn, "%s/%s", mapdirs[maptype].name, fn);
    setsvar("mapname", mn);

    formatstring(mf, "%s%s", mapname, mapexts[maptype].name);
    setsvar("mapfile", mf);

    if(verbose >= 1) conoutf("set map name to %s (%s)", mapname, mapfile);
}

enum { OCTSAV_CHILDREN = 0, OCTSAV_EMPTY, OCTSAV_SOLID, OCTSAV_NORMAL, OCTSAV_LODCUBE };

static int savemapprogress = 0;

void savec(cube *c, const ivec &o, int size, stream *f, bool nolms)
{
    if((savemapprogress++&0xFFF)==0) progress(float(savemapprogress)/allocnodes, "saving octree...");

    loopi(8)
    {
        ivec co(i, o, size);
        if(c[i].children)
        {
            f->putchar(OCTSAV_CHILDREN);
            savec(c[i].children, co, size>>1, f, nolms);
        }
        else
        {
            int oflags = 0, surfmask = 0, totalverts = 0;
            if(c[i].material!=MAT_AIR) oflags |= 0x40;
            if(isempty(c[i])) f->putchar(oflags | OCTSAV_EMPTY);
            else
            {
                if(!nolms)
                {
                    if(c[i].merged) oflags |= 0x80;
                    if(c[i].ext) loopj(6)
                    {
                        const surfaceinfo &surf = c[i].ext->surfaces[j];
                        if(!surf.used()) continue;
                        oflags |= 0x20;
                        surfmask |= 1<<j;
                        totalverts += surf.totalverts();
                    }
                }

                if(isentirelysolid(c[i])) f->putchar(oflags | OCTSAV_SOLID);
                else
                {
                    f->putchar(oflags | OCTSAV_NORMAL);
                    f->write(c[i].edges, 12);
                }
            }

            loopj(6) f->putlil<ushort>(c[i].texture[j]);

            if(oflags&0x40) f->putlil<ushort>(c[i].material);
            if(oflags&0x80) f->putchar(c[i].merged);
            if(oflags&0x20)
            {
                f->putchar(surfmask);
                f->putchar(totalverts);
                loopj(6) if(surfmask&(1<<j))
                {
                    surfaceinfo surf = c[i].ext->surfaces[j];
                    vertinfo *verts = c[i].ext->verts() + surf.verts;
                    int layerverts = surf.numverts&MAXFACEVERTS, numverts = surf.totalverts(),
                        vertmask = 0, vertorder = 0, uvorder = 0,
                        dim = dimension(j), vc = C[dim], vr = R[dim];
                    if(numverts)
                    {
                        if(c[i].merged&(1<<j))
                        {
                            vertmask |= 0x04;
                            if(layerverts == 4)
                            {
                                ivec v[4] = { verts[0].getxyz(), verts[1].getxyz(), verts[2].getxyz(), verts[3].getxyz() };
                                loopk(4)
                                {
                                    const ivec &v0 = v[k], &v1 = v[(k+1)&3], &v2 = v[(k+2)&3], &v3 = v[(k+3)&3];
                                    if(v1[vc] == v0[vc] && v1[vr] == v2[vr] && v3[vc] == v2[vc] && v3[vr] == v0[vr])
                                    {
                                        vertmask |= 0x01;
                                        vertorder = k;
                                        break;
                                    }
                                }
                            }
                        }
                        else
                        {
                            int vis = visibletris(c[i], j, co, size);
                            if(vis&4 || faceconvexity(c[i], j) < 0) vertmask |= 0x01;
                            if(layerverts < 4 && vis&2) vertmask |= 0x02;
                        }
                        bool matchnorm = true;
                        loopk(numverts)
                        {
                            const vertinfo &v = verts[k];
                            if(v.u || v.v) vertmask |= 0x40;
                            if(v.norm) { vertmask |= 0x80; if(v.norm != verts[0].norm) matchnorm = false; }
                        }
                        if(matchnorm) vertmask |= 0x08;
                        if(vertmask&0x40 && layerverts == 4)
                        {
                            loopk(4)
                            {
                                const vertinfo &v0 = verts[k], &v1 = verts[(k+1)&3], &v2 = verts[(k+2)&3], &v3 = verts[(k+3)&3];
                                if(v1.u == v0.u && v1.v == v2.v && v3.u == v2.u && v3.v == v0.v)
                                {
                                    if(surf.numverts&LAYER_DUP)
                                    {
                                        const vertinfo &b0 = verts[4+k], &b1 = verts[4+((k+1)&3)], &b2 = verts[4+((k+2)&3)], &b3 = verts[4+((k+3)&3)];
                                        if(b1.u != b0.u || b1.v != b2.v || b3.u != b2.u || b3.v != b0.v)
                                            continue;
                                    }
                                    uvorder = k;
                                    vertmask |= 0x02 | (((k+4-vertorder)&3)<<4);
                                    break;
                                }
                            }
                        }
                    }
                    surf.verts = vertmask;
                    f->write(&surf, sizeof(surfaceinfo));
                    bool hasxyz = (vertmask&0x04)!=0, hasuv = (vertmask&0x40)!=0, hasnorm = (vertmask&0x80)!=0;
                    if(layerverts == 4)
                    {
                        if(hasxyz && vertmask&0x01)
                        {
                            ivec v0 = verts[vertorder].getxyz(), v2 = verts[(vertorder+2)&3].getxyz();
                            f->putlil<ushort>(v0[vc]); f->putlil<ushort>(v0[vr]);
                            f->putlil<ushort>(v2[vc]); f->putlil<ushort>(v2[vr]);
                            hasxyz = false;
                        }
                        if(hasuv && vertmask&0x02)
                        {
                            const vertinfo &v0 = verts[uvorder], &v2 = verts[(uvorder+2)&3];
                            f->putlil<ushort>(v0.u); f->putlil<ushort>(v0.v);
                            f->putlil<ushort>(v2.u); f->putlil<ushort>(v2.v);
                            if(surf.numverts&LAYER_DUP)
                            {
                                const vertinfo &b0 = verts[4+uvorder], &b2 = verts[4+((uvorder+2)&3)];
                                f->putlil<ushort>(b0.u); f->putlil<ushort>(b0.v);
                                f->putlil<ushort>(b2.u); f->putlil<ushort>(b2.v);
                            }
                            hasuv = false;
                        }
                    }
                    if(hasnorm && vertmask&0x08) { f->putlil<ushort>(verts[0].norm); hasnorm = false; }
                    if(hasxyz || hasuv || hasnorm) loopk(layerverts)
                    {
                        const vertinfo &v = verts[(k+vertorder)%layerverts];
                        if(hasxyz)
                        {
                            ivec xyz = v.getxyz();
                            f->putlil<ushort>(xyz[vc]); f->putlil<ushort>(xyz[vr]);
                        }
                        if(hasuv) { f->putlil<ushort>(v.u); f->putlil<ushort>(v.v); }
                        if(hasnorm) f->putlil<ushort>(v.norm);
                    }
                    if(surf.numverts&LAYER_DUP) loopk(layerverts)
                    {
                        const vertinfo &v = verts[layerverts + (k+vertorder)%layerverts];
                        if(hasuv) { f->putlil<ushort>(v.u); f->putlil<ushort>(v.v); }
                    }
                }
            }
        }
    }
}

struct surfacecompat
{
    uchar texcoords[8];
    uchar w, h;
    ushort x, y;
    uchar lmid, layer;
};

struct normalscompat
{
    bvec normals[4];
};

struct mergecompat
{
    ushort u1, u2, v1, v2;
};

cube *loadchildren(stream *f, const ivec &co, int size, bool &failed);

void convertoldsurfaces(cube &c, const ivec &co, int size, surfacecompat *srcsurfs, int hassurfs, normalscompat *normals, int hasnorms, mergecompat *merges, int hasmerges)
{
    surfaceinfo dstsurfs[6];
    vertinfo verts[6*2*MAXFACEVERTS];
    int totalverts = 0, numsurfs = 6;
    memset(dstsurfs, 0, sizeof(dstsurfs));
    loopi(6) if((hassurfs|hasnorms|hasmerges)&(1<<i))
    {
        surfaceinfo &dst = dstsurfs[i];
        vertinfo *curverts = NULL;
        int numverts = 0;
        surfacecompat *src = NULL, *blend = NULL;
        if(hassurfs&(1<<i))
        {
            src = &srcsurfs[i];
            if(src->layer&2)
            {
                blend = &srcsurfs[numsurfs++];
                dst.lmid[0] = src->lmid;
                dst.lmid[1] = blend->lmid;
                dst.numverts |= LAYER_BLEND;
                if(blend->lmid >= LMID_RESERVED && (src->x != blend->x || src->y != blend->y || src->w != blend->w || src->h != blend->h || memcmp(src->texcoords, blend->texcoords, sizeof(src->texcoords))))
                    dst.numverts |= LAYER_DUP;
            }
            else if(src->layer == 1) { dst.lmid[1] = src->lmid; dst.numverts |= LAYER_BOTTOM; }
            else { dst.lmid[0] = src->lmid; dst.numverts |= LAYER_TOP; }
        }
        else dst.numverts |= LAYER_TOP;
        bool uselms = hassurfs&(1<<i) && (dst.lmid[0] >= LMID_RESERVED || dst.lmid[1] >= LMID_RESERVED || dst.numverts&~LAYER_TOP),
             usemerges = hasmerges&(1<<i) && merges[i].u1 < merges[i].u2 && merges[i].v1 < merges[i].v2,
             usenorms = hasnorms&(1<<i) && normals[i].normals[0] != bvec(128, 128, 128);
        if(uselms || usemerges || usenorms)
        {
            ivec v[4], pos[4], e1, e2, e3, n, vo = ivec(co).mask(0xFFF).shl(3);
            genfaceverts(c, i, v);
            n.cross((e1 = v[1]).sub(v[0]), (e2 = v[2]).sub(v[0]));
            if(usemerges)
            {
                const mergecompat &m = merges[i];
                int offset = -n.dot(v[0].mul(size).add(vo)),
                    dim = dimension(i), vc = C[dim], vr = R[dim];
                loopk(4)
                {
                    const ivec &coords = facecoords[i][k];
                    int cc = coords[vc] ? m.u2 : m.u1,
                        rc = coords[vr] ? m.v2 : m.v1,
                        dc = n[dim] ? -(offset + n[vc]*cc + n[vr]*rc)/n[dim] : vo[dim];
                    ivec &mv = pos[k];
                    mv[vc] = cc;
                    mv[vr] = rc;
                    mv[dim] = dc;
                }
            }
            else
            {
                int convex = (e3 = v[0]).sub(v[3]).dot(n), vis = 3;
                if(!convex)
                {
                    if(ivec().cross(e3, e2).iszero()) { if(!n.iszero()) vis = 1; }
                    else if(n.iszero()) vis = 2;
                }
                int order = convex < 0 ? 1 : 0;
                pos[0] = v[order].mul(size).add(vo);
                pos[1] = vis&1 ? v[order+1].mul(size).add(vo) : pos[0];
                pos[2] = v[order+2].mul(size).add(vo);
                pos[3] = vis&2 ? v[(order+3)&3].mul(size).add(vo) : pos[0];
            }
            curverts = verts + totalverts;
            loopk(4)
            {
                if(k > 0 && (pos[k] == pos[0] || pos[k] == pos[k-1])) continue;
                vertinfo &dv = curverts[numverts++];
                dv.setxyz(pos[k]);
                if(uselms)
                {
                    float u = src->x + (src->texcoords[k*2] / 255.0f) * (src->w - 1),
                          v = src->y + (src->texcoords[k*2+1] / 255.0f) * (src->h - 1);
                    dv.u = ushort(floor(clamp((u) * float(USHRT_MAX+1)/LM_PACKW + 0.5f, 0.0f, float(USHRT_MAX))));
                    dv.v = ushort(floor(clamp((v) * float(USHRT_MAX+1)/LM_PACKH + 0.5f, 0.0f, float(USHRT_MAX))));
                }
                else dv.u = dv.v = 0;
                dv.norm = usenorms && normals[i].normals[k] != bvec(128, 128, 128) ? encodenormal(normals[i].normals[k].tonormal().normalize()) : 0;
            }
            dst.verts = totalverts;
            dst.numverts |= numverts;
            totalverts += numverts;
            if(dst.numverts&LAYER_DUP) loopk(4)
            {
                if(k > 0 && (pos[k] == pos[0] || pos[k] == pos[k-1])) continue;
                vertinfo &bv = verts[totalverts++];
                bv.setxyz(pos[k]);
                bv.u = ushort(floor(clamp((blend->x + (blend->texcoords[k*2] / 255.0f) * (blend->w - 1)) * float(USHRT_MAX+1)/LM_PACKW, 0.0f, float(USHRT_MAX))));
                bv.v = ushort(floor(clamp((blend->y + (blend->texcoords[k*2+1] / 255.0f) * (blend->h - 1)) * float(USHRT_MAX+1)/LM_PACKH, 0.0f, float(USHRT_MAX))));
                bv.norm = usenorms && normals[i].normals[k] != bvec(128, 128, 128) ? encodenormal(normals[i].normals[k].tonormal().normalize()) : 0;
            }
        }
    }
    setsurfaces(c, dstsurfs, verts, totalverts);
}

static inline int convertoldmaterial(int mat)
{
    return ((mat&7)<<MATF_VOLUME_SHIFT) | (((mat>>3)&3)<<MATF_CLIP_SHIFT) | (((mat>>5)&7)<<MATF_FLAG_SHIFT);
}

void loadc(stream *f, cube &c, const ivec &co, int size, bool &failed)
{
    bool haschildren = false;
    int octsav = f->getchar();
    switch(octsav&0x7)
    {
        case OCTSAV_CHILDREN:
            c.children = loadchildren(f, co, size>>1, failed);
            return;

        case OCTSAV_LODCUBE: haschildren = true;    break;
        case OCTSAV_EMPTY:  emptyfaces(c);          break;
        case OCTSAV_SOLID:  solidfaces(c);          break;
        case OCTSAV_NORMAL: f->read(c.edges, 12); break;

        default: failed = true; return;
    }
    loopi(6) c.texture[i] = hdr.version<14 ? f->getchar() : f->getlil<ushort>();
    if(hdr.version < 7) loopi(3) f->getchar(); //f->read(c.colour, 3);
    else if((maptype == MAP_OCTA && hdr.version <= 31) || (maptype == MAP_MAPZ && hdr.version <= 40))
    {
        uchar mask = f->getchar();
        if(mask & 0x80)
        {
            int mat = f->getchar();
            if((maptype == MAP_OCTA && hdr.version <= 26) || (maptype == MAP_MAPZ && hdr.version <= 30))
            {
                static ushort matconv[] = { MAT_AIR, MAT_WATER, MAT_CLIP, MAT_GLASS|MAT_CLIP, MAT_NOCLIP, MAT_LAVA|MAT_DEATH, MAT_AICLIP, MAT_DEATH };
                c.material = size_t(mat) < sizeof(matconv)/sizeof(matconv[0]) ? matconv[mat] : MAT_AIR;
            }
            else c.material = convertoldmaterial(mat);
        }
        surfacecompat surfaces[12];
        normalscompat normals[6];
        mergecompat merges[6];
        int hassurfs = 0, hasnorms = 0, hasmerges = 0;
        if(mask & 0x3F)
        {
            int numsurfs = 6;
            loopi(numsurfs)
            {
                if(i >= 6 || mask & (1 << i))
                {
                    f->read(&surfaces[i], sizeof(surfacecompat));
                    lilswap(&surfaces[i].x, 2);
                    if(hdr.version < 10) ++surfaces[i].lmid;
                    if(hdr.version < 18)
                    {
                        if(surfaces[i].lmid >= LMID_AMBIENT1) ++surfaces[i].lmid;
                        if(surfaces[i].lmid >= LMID_BRIGHT1) ++surfaces[i].lmid;
                    }
                    if(hdr.version < 19)
                    {
                        if(surfaces[i].lmid >= LMID_DARK) surfaces[i].lmid += 2;
                    }
                    if(i < 6)
                    {
                        if(mask & 0x40) { hasnorms |= 1<<i; f->read(&normals[i], sizeof(normalscompat)); }
                        if(surfaces[i].layer != 0 || surfaces[i].lmid != LMID_AMBIENT)
                            hassurfs |= 1<<i;
                        if(surfaces[i].layer&2) numsurfs++;
                    }
                }
            }
        }
        if(hdr.version <= 8) edgespan2vectorcube(c);
        if(hdr.version <= 11)
        {
            swap(c.faces[0], c.faces[2]);
            swap(c.texture[0], c.texture[4]);
            swap(c.texture[1], c.texture[5]);
            if(hassurfs&0x33)
            {
                swap(surfaces[0], surfaces[4]);
                swap(surfaces[1], surfaces[5]);
                hassurfs = (hassurfs&~0x33) | ((hassurfs&0x30)>>4) | ((hassurfs&0x03)<<4);
            }
        }
        if(hdr.version >= 20)
        {
            if(octsav&0x80)
            {
                int merged = f->getchar();
                c.merged = merged&0x3F;
                if(merged&0x80)
                {
                    int mask = f->getchar();
                    if(mask)
                    {
                        hasmerges = mask&0x3F;
                        loopi(6) if(mask&(1<<i))
                        {
                            mergecompat *m = &merges[i];
                            f->read(m, sizeof(mergecompat));
                            lilswap(&m->u1, 4);
                            if(hdr.version <= 25)
                            {
                                int uorigin = m->u1 & 0xE000, vorigin = m->v1 & 0xE000;
                                m->u1 = (m->u1 - uorigin) << 2;
                                m->u2 = (m->u2 - uorigin) << 2;
                                m->v1 = (m->v1 - vorigin) << 2;
                                m->v2 = (m->v2 - vorigin) << 2;
                            }
                        }
                    }
                }
            }
        }
        if(hassurfs || hasnorms || hasmerges)
            convertoldsurfaces(c, co, size, surfaces, hassurfs, normals, hasnorms, merges, hasmerges);
    }
    else
    {
        if(octsav&0x40)
        {
            if((maptype == MAP_OCTA && hdr.version <= 32) || (maptype == MAP_MAPZ && hdr.version <= 42))
            {
                int mat = f->getchar();
                c.material = convertoldmaterial(mat);
            }
            else c.material = f->getlil<ushort>();
        }
        if(octsav&0x80) c.merged = f->getchar();
        if(octsav&0x20)
        {
            int surfmask, totalverts;
            surfmask = f->getchar();
            totalverts = f->getchar();
            newcubeext(c, totalverts, false);
            memset(c.ext->surfaces, 0, sizeof(c.ext->surfaces));
            memset(c.ext->verts(), 0, totalverts*sizeof(vertinfo));
            int offset = 0;
            loopi(6) if(surfmask&(1<<i))
            {
                surfaceinfo &surf = c.ext->surfaces[i];
                f->read(&surf, sizeof(surfaceinfo));
                int vertmask = surf.verts, numverts = surf.totalverts();
                if(!numverts) { surf.verts = 0; continue; }
                surf.verts = offset;
                vertinfo *verts = c.ext->verts() + offset;
                offset += numverts;
                ivec v[4], n, vo = ivec(co).mask(0xFFF).shl(3);
                int layerverts = surf.numverts&MAXFACEVERTS, dim = dimension(i), vc = C[dim], vr = R[dim], bias = 0;
                genfaceverts(c, i, v);
                bool hasxyz = (vertmask&0x04)!=0, hasuv = (vertmask&0x40)!=0, hasnorm = (vertmask&0x80)!=0;
                if(hasxyz)
                {
                    ivec e1, e2, e3;
                    n.cross((e1 = v[1]).sub(v[0]), (e2 = v[2]).sub(v[0]));
                    if(n.iszero()) n.cross(e2, (e3 = v[3]).sub(v[0]));
                    bias = -n.dot(ivec(v[0]).mul(size).add(vo));
                }
                else
                {
                    int vis = layerverts < 4 ? (vertmask&0x02 ? 2 : 1) : 3, order = vertmask&0x01 ? 1 : 0, k = 0;
                    verts[k++].setxyz(v[order].mul(size).add(vo));
                    if(vis&1) verts[k++].setxyz(v[order+1].mul(size).add(vo));
                    verts[k++].setxyz(v[order+2].mul(size).add(vo));
                    if(vis&2) verts[k++].setxyz(v[(order+3)&3].mul(size).add(vo));
                }
                if(layerverts == 4)
                {
                    if(hasxyz && vertmask&0x01)
                    {
                        ushort c1 = f->getlil<ushort>(), r1 = f->getlil<ushort>(), c2 = f->getlil<ushort>(), r2 = f->getlil<ushort>();
                        ivec xyz;
                        xyz[vc] = c1; xyz[vr] = r1; xyz[dim] = n[dim] ? -(bias + n[vc]*xyz[vc] + n[vr]*xyz[vr])/n[dim] : vo[dim];
                        verts[0].setxyz(xyz);
                        xyz[vc] = c1; xyz[vr] = r2; xyz[dim] = n[dim] ? -(bias + n[vc]*xyz[vc] + n[vr]*xyz[vr])/n[dim] : vo[dim];
                        verts[1].setxyz(xyz);
                        xyz[vc] = c2; xyz[vr] = r2; xyz[dim] = n[dim] ? -(bias + n[vc]*xyz[vc] + n[vr]*xyz[vr])/n[dim] : vo[dim];
                        verts[2].setxyz(xyz);
                        xyz[vc] = c2; xyz[vr] = r1; xyz[dim] = n[dim] ? -(bias + n[vc]*xyz[vc] + n[vr]*xyz[vr])/n[dim] : vo[dim];
                        verts[3].setxyz(xyz);
                        hasxyz = false;
                    }
                    if(hasuv && vertmask&0x02)
                    {
                        int uvorder = (vertmask&0x30)>>4;
                        vertinfo &v0 = verts[uvorder], &v1 = verts[(uvorder+1)&3], &v2 = verts[(uvorder+2)&3], &v3 = verts[(uvorder+3)&3];
                        v0.u = f->getlil<ushort>(); v0.v = f->getlil<ushort>();
                        v2.u = f->getlil<ushort>(); v2.v = f->getlil<ushort>();
                        v1.u = v0.u; v1.v = v2.v;
                        v3.u = v2.u; v3.v = v0.v;
                        if(surf.numverts&LAYER_DUP)
                        {
                            vertinfo &b0 = verts[4+uvorder], &b1 = verts[4+((uvorder+1)&3)], &b2 = verts[4+((uvorder+2)&3)], &b3 = verts[4+((uvorder+3)&3)];
                            b0.u = f->getlil<ushort>(); b0.v = f->getlil<ushort>();
                            b2.u = f->getlil<ushort>(); b2.v = f->getlil<ushort>();
                            b1.u = b0.u; b1.v = b2.v;
                            b3.u = b2.u; b3.v = b0.v;
                        }
                        hasuv = false;
                    }
                }
                if(hasnorm && vertmask&0x08)
                {
                    ushort norm = f->getlil<ushort>();
                    loopk(layerverts) verts[k].norm = norm;
                    hasnorm = false;
                }
                if(hasxyz || hasuv || hasnorm) loopk(layerverts)
                {
                    vertinfo &v = verts[k];
                    if(hasxyz)
                    {
                        ivec xyz;
                        xyz[vc] = f->getlil<ushort>(); xyz[vr] = f->getlil<ushort>();
                        xyz[dim] = n[dim] ? -(bias + n[vc]*xyz[vc] + n[vr]*xyz[vr])/n[dim] : vo[dim];
                        v.setxyz(xyz);
                    }
                    if(hasuv) { v.u = f->getlil<ushort>(); v.v = f->getlil<ushort>(); }
                    if(hasnorm) v.norm = f->getlil<ushort>();
                }
                if(surf.numverts&LAYER_DUP) loopk(layerverts)
                {
                    vertinfo &v = verts[k+layerverts], &t = verts[k];
                    v.setxyz(t.x, t.y, t.z);
                    if(hasuv) { v.u = f->getlil<ushort>(); v.v = f->getlil<ushort>(); }
                    v.norm = t.norm;
                }
            }
        }
    }

    c.children = (haschildren ? loadchildren(f, co, size>>1, failed) : NULL);
}

cube *loadchildren(stream *f, const ivec &co, int size, bool &failed)
{
    cube *c = newcubes();
    loopi(8)
    {
        loadc(f, c[i], ivec(i, co, size), size, failed);
        if(failed) break;
    }
    return c;
}

void savevslot(stream *f, VSlot &vs, int prev)
{
    f->putlil<int>(vs.changed);
    f->putlil<int>(prev);
    if(vs.changed & (1<<VSLOT_SHPARAM))
    {
        ushort flags = vs.params.length();
        loopv(vs.params) if(vs.params[i].palette || vs.params[i].palindex) flags |= 0x8000;
        f->putlil<ushort>(flags);
        loopv(vs.params)
        {
            SlotShaderParam &p = vs.params[i];
            f->putlil<ushort>(strlen(p.name));
            f->write(p.name, strlen(p.name));
            loopk(4) f->putlil<float>(p.val[k]);
            if(flags&0x8000) { f->putlil<int>(vs.palette); f->putlil<int>(vs.palindex); }
        }
    }
    if(vs.changed & (1<<VSLOT_SCALE)) f->putlil<float>(vs.scale);
    if(vs.changed & (1<<VSLOT_ROTATION)) f->putlil<int>(vs.rotation);
    if(vs.changed & (1<<VSLOT_OFFSET))
    {
        f->putlil<int>(vs.offset.x);
        f->putlil<int>(vs.offset.y);
    }
    if(vs.changed & (1<<VSLOT_SCROLL))
    {
        f->putlil<float>(vs.scroll.x);
        f->putlil<float>(vs.scroll.y);
    }
    if(vs.changed & (1<<VSLOT_LAYER)) f->putlil<int>(vs.layer);
    if(vs.changed & (1<<VSLOT_ALPHA))
    {
        f->putlil<float>(vs.alphafront);
        f->putlil<float>(vs.alphaback);
    }
    if(vs.changed & (1<<VSLOT_COLOR))
    {
        loopk(3) f->putlil<float>(vs.colorscale[k]);
    }
    if(vs.changed & (1<<VSLOT_PALETTE))
    {
        f->putlil<int>(vs.palette);
        f->putlil<int>(vs.palindex);
    }
    if(vs.changed & (1<<VSLOT_COAST)) f->putlil<float>(vs.coastscale);
}

void savevslots(stream *f, int numvslots)
{
    if(vslots.empty()) return;
    int *prev = new int[numvslots];
    memset(prev, -1, numvslots*sizeof(int));
    loopi(numvslots)
    {
        VSlot *vs = vslots[i];
        if(vs->changed) continue;
        for(;;)
        {
            VSlot *cur = vs;
            do vs = vs->next; while(vs && vs->index >= numvslots);
            if(!vs) break;
            prev[vs->index] = cur->index;
        }
    }
    int lastroot = 0;
    loopi(numvslots)
    {
        VSlot &vs = *vslots[i];
        if(!vs.changed) continue;
        if(lastroot < i) f->putlil<int>(-(i - lastroot));
        savevslot(f, vs, prev[i]);
        lastroot = i+1;
    }
    if(lastroot < numvslots) f->putlil<int>(-(numvslots - lastroot));
    delete[] prev;
}

void loadvslot(stream *f, VSlot &vs, int changed)
{
    vs.changed = changed;
    if(vs.changed & (1<<VSLOT_SHPARAM))
    {
        int flags = f->getlil<ushort>(), numparams = flags&0x7FFF;
        string name;
        loopi(numparams)
        {
            SlotShaderParam &p = vs.params.add();
            int nlen = f->getlil<ushort>();
            f->read(name, min(nlen, MAXSTRLEN-1));
            name[min(nlen, MAXSTRLEN-1)] = '\0';
            if(nlen >= MAXSTRLEN) f->seek(nlen - (MAXSTRLEN-1), SEEK_CUR);
            p.name = getshaderparamname(name);
            p.loc = -1;
            loopk(4) p.val[k] = f->getlil<float>();
            p.palette = flags&0x8000 ? f->getlil<int>() : 0;
            p.palindex = flags&0x8000 ? f->getlil<int>() : 0;
        }
    }
    if(vs.changed & (1<<VSLOT_SCALE)) vs.scale = f->getlil<float>();
    if(vs.changed & (1<<VSLOT_ROTATION)) vs.rotation = f->getlil<int>();
    if(vs.changed & (1<<VSLOT_OFFSET))
    {
        vs.offset.x = f->getlil<int>();
        vs.offset.y = f->getlil<int>();
    }
    if(vs.changed & (1<<VSLOT_SCROLL))
    {
        vs.scroll.x = f->getlil<float>();
        vs.scroll.y = f->getlil<float>();
    }
    if(vs.changed & (1<<VSLOT_LAYER)) vs.layer = f->getlil<int>();
    if(vs.changed & (1<<VSLOT_ALPHA))
    {
        vs.alphafront = f->getlil<float>();
        vs.alphaback = f->getlil<float>();
    }
    if(vs.changed & (1<<VSLOT_COLOR))
    {
        loopk(3) vs.colorscale[k] = f->getlil<float>();
    }
    if(vs.changed & (1<<VSLOT_PALETTE))
    {
        vs.palette = f->getlil<int>();
        vs.palindex = f->getlil<int>();
    }
    if(vs.changed & (1<<VSLOT_COAST)) vs.coastscale = f->getlil<float>();
}

void loadvslots(stream *f, int numvslots)
{
    int *prev = new int[numvslots];
    memset(prev, -1, numvslots*sizeof(int));
    while(numvslots > 0)
    {
        int changed = f->getlil<int>();
        if(changed < 0)
        {
            loopi(-changed) vslots.add(new VSlot(NULL, vslots.length()));
            numvslots += changed;
        }
        else
        {
            prev[vslots.length()] = f->getlil<int>();
            loadvslot(f, *vslots.add(new VSlot(NULL, vslots.length())), changed);
            numvslots--;
        }
    }
    loopv(vslots) if(vslots.inrange(prev[i])) vslots[prev[i]]->next = vslots[i];
    delete[] prev;
}

void saveslotconfig(stream *h, Slot &s, int index)
{
    if(index >= 0)
    {
        if(s.shader)
        {
            h->printf("setshader %s\n", escapeid(s.shader->name));
        }
        loopvj(s.params)
        {
            h->printf("setshaderparam %s", escapeid(s.params[j].name));
            loopk(4) h->printf(" %f", s.params[j].val[k]);
            if(s.params[j].palette || s.params[j].palindex) h->printf(" %d %d", s.params[j].palette, s.params[j].palindex);
            h->printf("\n");
        }
    }
    loopvj(s.sts)
    {
        h->printf("texture");
        if(index >= 0) h->printf(" %s", findtexturetypename(s.sts[j].type));
        else if(!j) h->printf(" %s", findmaterialname(-index));
        else h->printf(" 1");
        h->printf(" %s", escapestring(s.sts[j].name));
        if(!j)
        {
            h->printf(" %d %d %d %f",
                s.variants->rotation, s.variants->offset.x, s.variants->offset.y, s.variants->scale);
            if(index >= 0) h->printf(" // %d", index);
        }
        h->printf("\n");
    }
    if(index >= 0)
    {
        if(!s.variants->scroll.iszero())
            h->printf("texscroll %f %f\n", s.variants->scroll.x * 1000.0f, s.variants->scroll.y * 1000.0f);
        if(s.variants->layer != 0)
        {
            if(s.layermaskname) h->printf("texlayer %d %s %d %f\n", s.variants->layer, escapestring(s.layermaskname), s.layermaskmode, s.layermaskscale);
            else h->printf("texlayer %d\n", s.variants->layer);
        }
        if(s.variants->alphafront != DEFAULT_ALPHA_FRONT || s.variants->alphaback != DEFAULT_ALPHA_BACK)
            h->printf("texalpha %f %f\n", s.variants->alphafront, s.variants->alphaback);
        if(s.variants->colorscale != vec(1, 1, 1))
            h->printf("texcolor %f %f %f\n", s.variants->colorscale.x, s.variants->colorscale.y, s.variants->colorscale.z);
        if(s.variants->palette || s.variants->palindex) h->printf("texpalette %d %d\n", s.variants->palette, s.variants->palindex);
        if(s.variants->coastscale != 1) h->printf("texcoastscale %f\n", s.variants->coastscale);
        if(s.texgrass)
        {
            h->printf("texgrass %s\n", escapestring(s.texgrass));
            if(s.grasscolor != vec(0, 0, 0))
                h->printf("texgrasscolor %f %f %f\n", s.grasscolor.x, s.grasscolor.y, s.grasscolor.z);
            if(s.grassblend > 0) h->printf("texgrassblend %f\n", s.grassblend);
            if(s.grassscale > 0) h->printf("texgrassscale %d\n", s.grassscale);
            if(s.grassheight > 0) h->printf("texgrassheight %d\n", s.grassheight);
        }
    }
    h->printf("\n");
}

void save_config(char *mname)
{
    if(autosavebackups) backup(mname, ".cfg", hdr.revision, autosavebackups > 2, !(autosavebackups%2));
    defformatstring(fname, "%s.cfg", mname);
    stream *h = openutf8file(fname, "w");
    if(!h) { conoutf("\frcould not write config to %s", fname); return; }

    string title, author;
    if(*maptitle) filterstring(title, maptitle);
    else copystring(title, maptitle);
    if(*mapauthor) filterstring(author, mapauthor);
    else copystring(author, mapauthor);
    // config
    h->printf("// %s by %s (%s)\n// Config generated by %s\n\n", title, author, mapname, versionname);

    int vars = 0;
    h->printf("// Variables stored in map file, may be uncommented here, or changed from editmode.\n");
    vector<ident *> ids;
    enumerate(idents, ident, id, ids.add(&id));
    ids.sortname();
    loopv(ids)
    {
        ident &id = *ids[i];
        if(id.flags&IDF_WORLD && !(id.flags&IDF_SERVER)) switch(id.type)
        {
            case ID_VAR: h->printf("// %s %s\n", escapeid(id), intstr(&id)); vars++;break;
            case ID_FVAR: h->printf("// %s %s\n", escapeid(id), floatstr(*id.storage.f)); vars++; break;
            case ID_SVAR: h->printf("// %s %s\n", escapeid(id), escapestring(*id.storage.s)); vars++; break;
            default: break;
        }
    }
    if(vars) h->printf("\n");
    if(verbose >= 2) conoutf("wrote %d variable values", vars);

    int aliases = 0;
    loopv(ids)
    {
        ident &id = *ids[i];
        if(id.type == ID_ALIAS && id.flags&IDF_WORLD && !(id.flags&IDF_SERVER) && strlen(id.name))
        {
            const char *str = id.getstr();
            if(str[0])
            {
                aliases++;
                if(validateblock(str)) h->printf("%s = [%s]\n", escapeid(id), str);
                else h->printf("%s = %s\n", escapeid(id), escapestring(str));
            }
        }
    }
    if(aliases) h->printf("\n");
    if(verbose >= 2) conoutf("saved %d aliases", aliases);

    // texture slots
    int nummats = sizeof(materialslots)/sizeof(materialslots[0]);
    loopi(nummats)
    {
        progress(i/float(nummats), "saving material slots...");

        switch(i&MATF_VOLUME)
        {
            case MAT_WATER: case MAT_LAVA:
                saveslotconfig(h, materialslots[i], -i);
                break;
        }
    }
    if(verbose) conoutf("saved %d material slots", nummats);

    loopv(slots)
    {
        progress(i/float(slots.length()), "saving texture slots...");
        saveslotconfig(h, *slots[i], i);
    }
    if(verbose) conoutf("saved %d texture slots", slots.length());

    loopv(mapmodels)
    {
        progress(i/float(mapmodels.length()), "saving mapmodel slots...");
        h->printf("mmodel %s\n", escapestring(mapmodels[i].name));
    }
    if(mapmodels.length()) h->printf("\n");
    if(verbose) conoutf("saved %d mapmodel slots", mapmodels.length());

    loopv(mapsounds)
    {
        progress(i/float(mapsounds.length()), "saving mapsound slots...");
        h->printf("mapsound %s", escapestring(mapsounds[i].name));
        if((mapsounds[i].vol > 0 && mapsounds[i].vol < 255) || mapsounds[i].maxrad > 0 || mapsounds[i].minrad >= 0)
            h->printf(" %d", mapsounds[i].vol);
        if(mapsounds[i].maxrad > 0 || mapsounds[i].minrad >= 0) h->printf(" %d", mapsounds[i].maxrad);
        if(mapsounds[i].minrad >= 0) h->printf(" %d", mapsounds[i].minrad);
        h->printf("\n");
    }
    if(mapsounds.length()) h->printf("\n");
    if(verbose) conoutf("saved %d mapsound slots", mapsounds.length());

    delete h;
    if(verbose) conoutf("saved config %s", fname);
}
ICOMMAND(0, savemapconfig, "s", (char *mname), if(!(identflags&IDF_WORLD)) save_config(*mname ? mname : mapname));

VARF(IDF_PERSIST, mapshotsize, 0, 512, INT_MAX-1, mapshotsize -= mapshotsize%2);

void save_mapshot(char *mname)
{
    if(autosavebackups) backup(mname, ifmtexts[imageformat], hdr.revision, autosavebackups > 2, !(autosavebackups%2));

    GLuint tex;
    glGenTextures(1, &tex);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    ImageData image(screenw, screenh, 3);
    memset(image.data, 0, 3*screenw*screenh);
    drawcubemap(2, camera1->o, camera1->yaw, camera1->pitch, false, false, false);
    glReadPixels(0, 0, screenw, screenh, GL_RGB, GL_UNSIGNED_BYTE, image.data);
    #if 0 // generates better images without this
    int x = 0, y = 0, w = screenw, h = screenh;
    if(w > h)
    {
        x += (w-h)/2;
        w = h;
    }
    else if(h > w)
    {
        y += (h-w)/2;
        h = w;
    }
    if(x || y) texcrop(image, x, y, w, h);
    #endif
    if(screenw > mapshotsize || screenh > mapshotsize) scaleimage(image, mapshotsize, mapshotsize);
    saveimage(mname, image, imageformat, compresslevel, true);

    glDeleteTextures(1, &tex);
    defformatstring(texname, "%s", mname);
    reloadtexture(texname);
}
ICOMMAND(0, savemapshot, "s", (char *mname), if(!(identflags&IDF_WORLD)) save_mapshot(*mname ? mname : mapname));

void save_world(const char *mname, bool nodata, bool forcesave)
{
    int savingstart = SDL_GetTicks();

    setnames(mname, MAP_MAPZ, forcesave ? -1 : 0);

    if(autosavebackups && !forcesave) backup(mapname, mapexts[MAP_MAPZ].name, hdr.revision, autosavebackups > 2, !(autosavebackups%2));
    conoutf("saving: %s (%s)", mapname, forcesave ? "forced" : "normal");

    stream *f = opengzfile(mapfile, "wb");
    if(!f) { conoutf("\frerror saving %s to %s: file error", mapname, mapfile); return; }

    if(autosavemapshot || forcesave) save_mapshot(mapname);
    if(autosaveconfigs || forcesave) save_config(mapname);
    if(*maptext && (autosavetexts || forcesave))
    {
        defformatstring(fname, "%s.txt", mname);
        stream *h = openutf8file(fname, "w");
        if(!h) conoutf("\frcould not write text to %s", fname);
        else
        {
            h->printf("%s", maptext);
            delete h;
            if(verbose) conoutf("saved text %s", fname);
        }
    }
    game::savemap(forcesave, mapname);

    int numvslots = vslots.length();
    if(!nodata && !multiplayer(false))
    {
        numvslots = compactvslots();
        allchanged();
    }

    savemapprogress = 0;
    progress(0, "saving map..");
    memcpy(hdr.head, "MAPZ", 4);
    hdr.version = MAPVERSION;
    hdr.headersize = sizeof(mapz);
    hdr.gamever = server::getver(1);
    hdr.numents = 0;
    hdr.numvslots = numvslots;
    hdr.revision++;
    string gameid;
    copystring(gameid, server::gameid());
    memcpy(hdr.gameid, gameid, 4);

    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        if(ents[i]->type!=ET_EMPTY || forcesave)
        {
            hdr.numents++;
        }
    }

    hdr.numpvs = nodata ? 0 : getnumviewcells();
    hdr.blendmap = nodata ? 0 : shouldsaveblendmap();
    hdr.lightmaps = nodata ? 0 : lightmaps.length();

    mapz tmp = hdr;
    lilswap(&tmp.version, 10);
    f->write(&tmp, sizeof(mapz));

    // world variables
    int numvars = 0, vars = 0;
    enumerate(idents, ident, id, {
        if((id.type == ID_VAR || id.type == ID_FVAR || id.type == ID_SVAR) && id.flags&IDF_WORLD && !(id.flags&IDF_SERVER) && strlen(id.name)) numvars++;
    });
    f->putlil<int>(numvars);
    enumerate(idents, ident, id, {
        if((id.type == ID_VAR || id.type == ID_FVAR || id.type == ID_SVAR) && id.flags&IDF_WORLD && !(id.flags&IDF_SERVER) && strlen(id.name))
        {
            vars++;
            progress(vars/float(numvars), "saving world variables...");
            f->putlil<int>((int)strlen(id.name));
            f->write(id.name, (int)strlen(id.name)+1);
            f->putlil<int>(id.type);
            switch(id.type)
            {
                case ID_VAR:
                    f->putlil<int>(*id.storage.i);
                    break;
                case ID_FVAR:
                    f->putlil<float>(*id.storage.f);
                    break;
                case ID_SVAR:
                    f->putlil<int>((int)strlen(*id.storage.s));
                    f->write(*id.storage.s, (int)strlen(*id.storage.s)+1);
                    break;
                default: break;
            }
        }
    });

    if(verbose) conoutf("saved %d variables", vars);

    // texture slots
    f->putlil<ushort>(texmru.length());
    loopv(texmru) f->putlil<ushort>(texmru[i]);

    // entities
    int count = 0;
    vector<int> remapents;
    if(!forcesave) entities::remapents(remapents);
    loopv(ents) // extended
    {
        progress(i/float(ents.length()), "saving entities...");
        int idx = remapents.inrange(i) ? remapents[i] : i;
        extentity &e = *(extentity *)ents[idx];
        if(e.type!=ET_EMPTY || forcesave)
        {
            entbase tmp = e;
            lilswap(&tmp.o.x, 3);
            f->write(&tmp, sizeof(entbase));
            f->putlil<int>(e.attrs.length());
            loopvk(e.attrs) f->putlil<int>(e.attrs[k]);
            entities::writeent(f, idx);
            if(entities::maylink(e.type))
            {
                vector<int> links;
                int n = 0;
                loopvk(ents)
                {
                    int kidx = remapents.inrange(k) ? remapents[k] : k;
                    extentity &f = (extentity &)*ents[kidx];
                    if(f.type != ET_EMPTY || forcesave)
                    {
                        if(entities::maylink(f.type) && e.links.find(kidx) >= 0)
                            links.add(n); // align to indices
                        n++;
                    }
                }

                f->putlil<int>(links.length());
                loopvj(links) f->putlil<int>(links[j]); // aligned index
                if(verbose >= 2) conoutf("entity %s (%d) saved %d links", entities::findname(e.type), i, links.length());
            }
            count++;
        }
    }
    if(verbose) conoutf("saved %d entities", count);

    savevslots(f, numvslots);
    if(verbose) conoutf("saved %d vslots", numvslots);

    progress(0, "saving octree...");
    savec(worldroot, ivec(0, 0, 0), hdr.worldsize>>1, f, nodata);

    if(!nodata)
    {
        loopv(lightmaps)
        {
            progress(float(i)/float(lightmaps.length()), "saving lightmaps...");
            LightMap &lm = lightmaps[i];
            f->putchar(lm.type | (lm.unlitx>=0 ? 0x80 : 0));
            if(lm.unlitx>=0)
            {
                f->putlil<ushort>(ushort(lm.unlitx));
                f->putlil<ushort>(ushort(lm.unlity));
            }
            f->write(lm.data, lm.bpp*LM_PACKW*LM_PACKH);
        }
        if(verbose) conoutf("saved %d lightmaps", lightmaps.length());
        if(getnumviewcells()>0)
        {
            progress(0, "saving PVS...");
            savepvs(f);
            if(verbose) conoutf("saved %d PVS view cells", getnumviewcells());
        }
        if(shouldsaveblendmap())
        {
            progress(0, "saving blendmap...");
            saveblendmap(f);
            if(verbose) conoutf("saved blendmap");
        }
    }
    delete f;
    mapcrc = crcfile(mapfile);
    conoutf("saved %s (\fs%s\fS by \fs%s\fS) v%d:%d(r%d) [0x%.8x] in %.1f secs", mapname, *maptitle ? maptitle : "Untitled", *mapauthor ? mapauthor : "Unknown", hdr.version, hdr.gamever, hdr.revision, mapcrc, (SDL_GetTicks()-savingstart)/1000.0f);
}

ICOMMAND(0, savemap, "s", (char *mname), if(!(identflags&IDF_WORLD)) save_world(*mname ? mname : mapname));

static void sanevars()
{
    setvar("fullbright", 0, false);
    setvar("blankgeom", 0, false);
}

VAR(IDF_HEX, sunlight, 0, 0, 0xFFFFFF); // OCTA compatibility
VAR(0, sunlightyaw, 0, 0, 360);
VAR(0, sunlightpitch, -90, 90, 90);
FVAR(0, sunlightscale, 0, 1, 16);

bool load_world(const char *mname, int crc)       // still supports all map formats that have existed since the earliest cube betas!
{
    int loadingstart = SDL_GetTicks();
    mapcrc = 0;
    setsvar("maptext", "", false);
    maploading = 1;
    loop(format, MAP_MAX) loop(tempfile, crc > 0 ? 2 : 1)
    {
        int mask = maskpackagedirs(format == MAP_OCTA ? ~0 : ~PACKAGEDIR_OCTA);
        setnames(mname, format, tempfile && crc > 0 ? crc : 0);

        int filecrc = crcfile(mapfile);
        if(crc > 0) conoutf("checking map: %s [0x%.8x] (need: 0x%.8x)", mapfile, filecrc, crc);
        if(!tempfile && crc > 0 && crc != filecrc)
        {
            maskpackagedirs(mask);
            continue; // skipped iteration
        }
        stream *f = opengzfile(mapfile, "rb");
        if(!f)
        {
            conoutf("\frnot found: %s", mapfile);
            maskpackagedirs(mask);
            continue;
        }

        bool samegame = true;
        int eif = 0;
        mapz newhdr;
        if(f->read(&newhdr, sizeof(binary))!=(int)sizeof(binary))
        {
            conoutf("\frerror loading %s: malformatted universal header", mapname);
            delete f;
            maploading = 0;
            maskpackagedirs(mask);
            return false;
        }
        lilswap(&newhdr.version, 2);

        clearworldvars();
        if(memcmp(newhdr.head, "MAPZ", 4) == 0 || memcmp(newhdr.head, "BFGZ", 4) == 0)
        {
            // this check removed below due to breakage: size_t(newhdr.headersize) > sizeof(chdr) || f->read(&chdr.worldsize, newhdr.headersize-sizeof(binary))!=size_t(newhdr.headersize)-sizeof(binary)
            #define MAPZCOMPAT(ver) \
                mapzcompat##ver chdr; \
                memcpy(&chdr, &newhdr, sizeof(binary)); \
                if(f->read(&chdr.worldsize, sizeof(chdr)-sizeof(binary))!=sizeof(chdr)-sizeof(binary)) \
                { \
                    conoutf("\frerror loading %s: malformatted mapz v%d[%d] header", mapname, newhdr.version, ver); \
                    delete f; \
                    maploading = 0; \
                    maskpackagedirs(mask); \
                    return false; \
                }

            if(newhdr.version <= 25)
            {
                MAPZCOMPAT(25);
                lilswap(&chdr.worldsize, 5);
                memcpy(&newhdr.worldsize, &chdr.worldsize, sizeof(int)*2);
                newhdr.numpvs = 0;
                newhdr.lightmaps = chdr.lightmaps;
                newhdr.blendmap = 0;
                newhdr.numvslots = 0;
                memcpy(&newhdr.gamever, &chdr.gamever, sizeof(int)*2);
                memcpy(&newhdr.gameid, &chdr.gameid, 4);
                setsvar("maptitle", chdr.maptitle, true);
            }
            else if(newhdr.version <= 32)
            {
                MAPZCOMPAT(32);
                lilswap(&chdr.worldsize, 6);
                memcpy(&newhdr.worldsize, &chdr.worldsize, sizeof(int)*4);
                newhdr.blendmap = 0;
                newhdr.numvslots = 0;
                memcpy(&newhdr.gamever, &chdr.gamever, sizeof(int)*2);
                memcpy(&newhdr.gameid, &chdr.gameid, 4);
                setsvar("maptitle", chdr.maptitle, true);
            }
            else if(newhdr.version <= 33)
            {
                MAPZCOMPAT(33);
                lilswap(&chdr.worldsize, 7);
                memcpy(&newhdr.worldsize, &chdr.worldsize, sizeof(int)*5);
                newhdr.numvslots = 0;
                memcpy(&newhdr.gamever, &chdr.gamever, sizeof(int)*2);
                memcpy(&newhdr.gameid, &chdr.gameid, 4);
                setsvar("maptitle", chdr.maptitle, true);
            }
            else if(newhdr.version <= 38)
            {
                MAPZCOMPAT(38);
                lilswap(&chdr.worldsize, 7);
                memcpy(&newhdr.worldsize, &chdr.worldsize, sizeof(int)*5);
                newhdr.numvslots = 0;
                memcpy(&newhdr.gamever, &chdr.gamever, sizeof(int)*2);
            }
            else
            {
                if(size_t(newhdr.headersize) > sizeof(newhdr) || f->read(&newhdr.worldsize, newhdr.headersize-sizeof(binary))!=size_t(newhdr.headersize)-sizeof(binary))
                {
                    conoutf("\frerror loading %s: malformatted mapz v%d header", mapname, newhdr.version);
                    delete f;
                    maploading = 0;
                    maskpackagedirs(mask);
                    return false;
                }
                lilswap(&newhdr.worldsize, 8);
            }

            if(newhdr.version > MAPVERSION)
            {
                conoutf("\frerror loading %s: requires a newer version of %s", mapname, versionname);
                delete f;
                maploading = 0;
                maskpackagedirs(mask);
                return false;
            }

            resetmap(false);
            hdr = newhdr;
            progress(0, "please wait..");
            maptype = MAP_MAPZ;
            mapcrc = filecrc;

            if(hdr.version <= 24) copystring(hdr.gameid, "bfa", 4); // all previous maps were bfa-fps
            if(verbose) conoutf("loading v%d map from %s game v%d", hdr.version, hdr.gameid, hdr.gamever);

            if(hdr.version >= 25 || (hdr.version == 24 && hdr.gamever >= 44))
            {
                int numvars = hdr.version >= 25 ? f->getlil<int>() : f->getchar(), vars = 0;
                identflags |= IDF_WORLD;
                progress(0, "loading variables...");
                loopi(numvars)
                {
                    progress(i/float(numvars), "loading variables...");
                    int len = hdr.version >= 25 ? f->getlil<int>() : f->getchar();
                    if(len)
                    {
                        string name;
                        f->read(name, len+1);
                        if(hdr.version <= 34)
                        {
                            if(!strcmp(name, "cloudcolour")) copystring(name, "cloudlayercolour");
                            if(!strcmp(name, "cloudblend")) copystring(name, "cloudlayerblend");
                        }
                        if(hdr.version <= 41)
                        {
                            if(!strcmp(name, "liquidcurb")) copystring(name, "liquidcoast");
                            else if(!strcmp(name, "floorcurb")) copystring(name, "floorcoast");
                            else if(!strcmp(name, "aircurb")) copystring(name, "aircoast");
                            else if(!strcmp(name, "slidecurb")) copystring(name, "slidecoast");
                            else if(!strcmp(name, "floatcurb")) copystring(name, "floatcoast");
                        }
                        ident *id = idents.access(name);
                        bool proceed = true;
                        int type = hdr.version >= 28 ? f->getlil<int>()+(hdr.version >= 29 ? 0 : 1) : (id ? id->type : ID_VAR);
                        if(!id || type != id->type)
                        {
                            if(id && hdr.version <= 28 && id->type == ID_FVAR && type == ID_VAR)
                                type = ID_FVAR;
                            else proceed = false;
                        }
                        if(!id || !(id->flags&IDF_WORLD) || id->flags&IDF_SERVER) proceed = false;

                        switch(type)
                        {
                            case ID_VAR:
                            {
                                int val = hdr.version >= 25 ? f->getlil<int>() : f->getchar();
                                if(proceed)
                                {
                                    if(val > id->maxval) val = id->maxval;
                                    else if(val < id->minval) val = id->minval;
                                    setvar(name, val, true);
                                }
                                break;
                            }
                            case ID_FVAR:
                            {
                                float val = hdr.version >= 29 ? f->getlil<float>() : float(f->getlil<int>())/100.f;
                                if(proceed)
                                {
                                    if(val > id->maxvalf) val = id->maxvalf;
                                    else if(val < id->minvalf) val = id->minvalf;
                                    setfvar(name, val, true);
                                }
                                break;
                            }
                            case ID_SVAR:
                            {
                                int slen = f->getlil<int>();
                                if(slen >= 0)
                                {
                                    char *val = newstring(slen);
                                    f->read(val, slen+1);
                                    if(proceed && slen) setsvar(name, val, true);
                                    delete[] val;
                                }
                                break;
                            }
                            default:
                            {
                                if(hdr.version <= 27)
                                {
                                    if(hdr.version >= 25) f->getlil<int>();
                                    else f->getchar();
                                }
                                proceed = false;
                                break;
                            }
                        }
                        if(!proceed)
                        {
                            if(verbose) conoutf("\frWARNING: ignoring variable %s stored in map", name);
                        }
                        else vars++;
                    }
                }
                identflags &= ~IDF_WORLD;
                if(verbose) conoutf("loaded %d variables", vars);
            }
            sanevars();

            if(!server::canload(hdr.gameid))
            {
                if(verbose) conoutf("\frWARNING: loading map from %s game type in %s, ignoring game specific data", hdr.gameid, server::gameid());
                samegame = false;
            }
        }
        else if(memcmp(newhdr.head, "OCTA", 4) == 0)
        {
            octa ohdr;
            memcpy(&ohdr, &newhdr, sizeof(binary));

            // this check removed due to breakage: (size_t)ohdr.headersize > sizeof(chdr) || f->read(&chdr.worldsize, ohdr.headersize-sizeof(binary))!=ohdr.headersize-(int)sizeof(binary)
            #define OCTACOMPAT(ver) \
                octacompat##ver chdr; \
                memcpy(&chdr, &ohdr, sizeof(binary)); \
                if(f->read(&chdr.worldsize, sizeof(chdr)-sizeof(binary))!=sizeof(chdr)-sizeof(binary)) \
                { \
                    conoutf("\frerror loading %s: malformatted octa v%d[%d] header", mapname, ver, ohdr.version); \
                    delete f; \
                    maploading = 0; \
                    maskpackagedirs(mask); \
                    return false; \
                }

            #define OCTAVARS \
                if(chdr.lightprecision) setvar("lightprecision", chdr.lightprecision); \
                if(chdr.lighterror) setvar("lighterror", chdr.lighterror); \
                if(chdr.bumperror) setvar("bumperror", chdr.bumperror); \
                setvar("lightlod", chdr.lightlod); \
                if(chdr.ambient) setvar("ambient", chdr.ambient); \
                setvar("skylight", (int(chdr.skylight[0])<<16) | (int(chdr.skylight[1])<<8) | int(chdr.skylight[2])); \
                setvar("watercolour", (int(chdr.watercolour[0])<<16) | (int(chdr.watercolour[1])<<8) | int(chdr.watercolour[2]), true); \
                setvar("waterfallcolour", (int(chdr.waterfallcolour[0])<<16) | (int(chdr.waterfallcolour[1])<<8) | int(chdr.waterfallcolour[2])); \
                setvar("lavacolour", (int(chdr.lavacolour[0])<<16) | (int(chdr.lavacolour[1])<<8) | int(chdr.lavacolour[2])); \
                setvar("fullbright", 0, true); \
                if(chdr.lerpsubdivsize || chdr.lerpangle) setvar("lerpangle", chdr.lerpangle); \
                if(chdr.lerpsubdivsize) \
                { \
                    setvar("lerpsubdiv", chdr.lerpsubdiv); \
                    setvar("lerpsubdivsize", chdr.lerpsubdivsize); \
                } \
                setsvar("maptitle", chdr.maptitle, true); \
                ohdr.numvars = 0;

            if(ohdr.version <= 25)
            {
                OCTACOMPAT(25);
                lilswap(&chdr.worldsize, 7);
                memcpy(&ohdr.worldsize, &chdr.worldsize, sizeof(int)*2);
                ohdr.numpvs = 0;
                memcpy(&ohdr.lightmaps, &chdr.lightmaps, sizeof(int)*3);
                ohdr.numvslots = 0;
                OCTAVARS;
            }
            else if(ohdr.version <= 28)
            {
                OCTACOMPAT(28);
                lilswap(&chdr.worldsize, 7);
                memcpy(&ohdr.worldsize, &chdr.worldsize, sizeof(int)*6);
                OCTAVARS;
                ohdr.blendmap = chdr.blendmap;
                ohdr.numvslots = 0;
            }
            else if(ohdr.version <= 29)
            {
                OCTACOMPAT(29);
                lilswap(&chdr.worldsize, 6);
                memcpy(&ohdr.worldsize, &chdr.worldsize, sizeof(int)*6);
                ohdr.numvslots = 0;
            }
            else
            {
                if(f->read(&ohdr.worldsize, sizeof(octa)-sizeof(binary))!=sizeof(octa)-(int)sizeof(binary))
                {
                    conoutf("\frerror loading %s: malformatted octa v%d header", mapname, ohdr.version);
                    delete f;
                    maploading = 0;
                    maskpackagedirs(mask);
                    return false;
                }
                lilswap(&ohdr.worldsize, 7);
            }

            if(ohdr.version > OCTAVERSION)
            {
                conoutf("\frerror loading %s: requires a newer version of Cube 2 support", mapname);
                delete f;
                maploading = 0;
                maskpackagedirs(mask);
                return false;
            }

            resetmap(false);
            hdr = newhdr;
            progress(0, "please wait..");
            maptype = MAP_OCTA;

            memcpy(hdr.head, ohdr.head, 4);
            hdr.gamever = 0; // sauer has no gamever
            hdr.worldsize = ohdr.worldsize;
            if(hdr.worldsize > 1<<18) hdr.worldsize = 1<<18;
            hdr.numents = ohdr.numents;
            hdr.numpvs = ohdr.numpvs;
            hdr.lightmaps = ohdr.lightmaps;
            hdr.blendmap = ohdr.blendmap;
            hdr.numvslots = ohdr.numvslots;
            hdr.revision = 1;

            if(ohdr.version >= 29) loopi(ohdr.numvars)
            {
                int type = f->getchar(), ilen = f->getlil<ushort>();
                string name;
                f->read(name, min(ilen, OCTASTRLEN-1));
                name[min(ilen, OCTASTRLEN-1)] = '\0';
                if(ilen >= OCTASTRLEN) f->seek(ilen - (OCTASTRLEN-1), SEEK_CUR);
                if(!strcmp(name, "cloudblend")) copystring(name, "cloudlayerblend");
                if(!strcmp(name, "cloudalpha")) copystring(name, "cloudblend");
                if(!strcmp(name, "grassalpha")) copystring(name, "grassblend");
                if(!strcmp(name, "skyboxcolour")) copystring(name, "skycolour");
                if(!strcmp(name, "cloudcolour")) copystring(name, "cloudlayercolour");
                if(!strcmp(name, "cloudboxcolour")) copystring(name, "cloudcolour");
                ident *id = getident(name);
                bool exists = id && id->type == type && id->flags&IDF_WORLD && !(id->flags&IDF_SERVER);
                switch(type)
                {
                    case ID_VAR:
                    {
                        int val = f->getlil<int>();
                        if(exists && id->minval <= id->maxval) setvar(name, val, true);
                        break;
                    }

                    case ID_FVAR:
                    {
                        float val = f->getlil<float>();
                        if(exists && id->minvalf <= id->maxvalf) setfvar(name, val, true);
                        break;
                    }

                    case ID_SVAR:
                    {
                        int slen = f->getlil<ushort>();
                        string val;
                        f->read(val, min(slen, OCTASTRLEN-1));
                        val[min(slen, OCTASTRLEN-1)] = '\0';
                        if(slen >= OCTASTRLEN) f->seek(slen - (OCTASTRLEN-1), SEEK_CUR);
                        if(exists) setsvar(name, val, true);
                        break;
                    }
                }
            }
            sanevars();

            string gameid;
            if(hdr.version >= 16)
            {
                int len = f->getchar();
                f->read(gameid, len+1);
            }
            else copystring(gameid, "fps");
            memcpy(hdr.gameid, gameid, 4);

            if(!server::canload(hdr.gameid))
            {
                if(verbose) conoutf("\frWARNING: loading OCTA v%d map from %s game, ignoring game specific data", hdr.version, hdr.gameid);
                samegame = false;
            }
            else if(verbose) conoutf("loading OCTA v%d map from %s game", hdr.version, hdr.gameid);

            if(hdr.version>=16)
            {
                eif = f->getlil<ushort>();
                int extrasize = f->getlil<ushort>();
                loopj(extrasize) f->getchar();
            }

            if(hdr.version<25) hdr.numpvs = 0;
            if(hdr.version<28) hdr.blendmap = 0;
        }
        else
        {
            delete f;
            maskpackagedirs(mask);
            continue;
        }

        progress(0, "clearing world...");

        texmru.shrink(0);
        if(hdr.version<14)
        {
            uchar oldtl[256];
            f->read(oldtl, sizeof(oldtl));
            loopi(256) texmru.add(oldtl[i]);
        }
        else
        {
            ushort nummru = f->getlil<ushort>();
            loopi(nummru) texmru.add(f->getlil<ushort>());
        }

        freeocta(worldroot);
        worldroot = NULL;

        progress(0, "loading entities...");
        vector<extentity *> &ents = entities::getents();
        loopi(hdr.numents)
        {
            progress(i/float(hdr.numents), "loading entities...");
            extentity &e = *ents.add(entities::newent());
            if(maptype == MAP_OCTA || (maptype == MAP_MAPZ && hdr.version <= 36))
            {
                entcompat ec;
                f->read(&ec, sizeof(entcompat));
                lilswap(&ec.o.x, 3);
                lilswap(ec.attr, 5);
                e.o = ec.o;
                e.type = ec.type;
                e.attrs.add(0, 5);
                loopk(5) e.attrs[k] = ec.attr[k];
            }
            else
            {
                f->read(&e, sizeof(entbase));
                lilswap(&e.o.x, 3);
                int numattr = f->getlil<int>();
                e.attrs.add(0, clamp(numattr, entities::numattrs(e.type), MAXENTATTRS));
                loopk(numattr)
                {
                    int val = f->getlil<int>();
                    if(e.attrs.inrange(k)) e.attrs[k] = val;
                }
            }
            if((maptype == MAP_OCTA && hdr.version <= 27) || (maptype == MAP_MAPZ && hdr.version <= 31)) e.attrs[4] = 0; // init ever-present attr5
            if(maptype == MAP_OCTA) f->seek(eif, SEEK_CUR);

            // sauerbraten version increments
            if(hdr.version <= 10 && e.type >= 7) e.type++;
            if(hdr.version <= 12 && e.type >= 8) e.type++;
            if(hdr.version <= 14 && e.type >= ET_MAPMODEL && e.type <= 16)
            {
                if(e.type == 16) e.type = ET_MAPMODEL;
                else e.type++;
            }
            if(hdr.version <= 20 && e.type >= ET_ENVMAP) e.type++;
            if(hdr.version <= 21 && e.type >= ET_PARTICLES) e.type++;
            if(hdr.version <= 22 && e.type >= ET_SOUND) e.type++;
            if(hdr.version <= 23 && e.type >= ET_LIGHTFX) e.type++;

            // version increments
            if((maptype == MAP_OCTA || (maptype == MAP_MAPZ && hdr.version <= 35)) && e.type >= ET_SUNLIGHT) e.type++;
            if(!samegame && (e.type >= ET_GAMESPECIFIC || hdr.version <= 14))
            {
                if(maptype == MAP_MAPZ && entities::maylink(hdr.gamever <= 49 && e.type >= 10 ? e.type-1 : e.type, hdr.gamever))
                {
                    int links = f->getlil<int>();
                    f->seek(sizeof(int)*links, SEEK_CUR);
                }
                e.type = ET_EMPTY;
                continue;
            }
            entities::readent(f, maptype, hdr.version, hdr.gameid, hdr.gamever, i);
            if(maptype == MAP_MAPZ && entities::maylink(hdr.gamever <= 49 && e.type >= 10 ? e.type-1 : e.type, hdr.gamever))
            {
                int links = f->getlil<int>();
                e.links.add(0, links);
                loopk(links) e.links[k] = f->getlil<int>();
                if(verbose >= 2) conoutf("entity %s (%d) loaded %d link(s)", entities::findname(e.type), i, links);
            }

            if(maptype == MAP_OCTA && e.type == ET_PARTICLES && e.attrs[0] >= 11)
            {
                if(e.attrs[0] <= 12) e.attrs[0] += 3;
                else e.attrs[0] = 0; // bork it up
            }
            if(e.type == ET_MAPMODEL)
            {
                if(hdr.version <= 14)
                {
                    e.o.z += e.attrs[2];
                    if(e.attrs[3] && verbose) conoutf("\frWARNING: mapmodel ent (index %d) uses texture slot %d", i, e.attrs[3]);
                    e.attrs[2] = e.attrs[3] = 0;
                }
                if(maptype == MAP_OCTA || hdr.version <= 31)
                {
                    int angle = e.attrs[0];
                    e.attrs[0] = e.attrs[1];
                    e.attrs[1] = angle;
                    loopk(e.attrs.length()-2) e.attrs[k+2] = 0;
                }
                if(maptype == MAP_MAPZ && hdr.version <= 37)
                {
                    e.attrs[2] = e.attrs[3];
                    e.attrs[5] = e.attrs[4];
                    e.attrs[3] = e.attrs[4] = 0;
                }
                if(maptype == MAP_MAPZ && hdr.gamever <= 219)
                { // insert pitch at index 2 between yaw and roll
                    e.attrs.insert(2, 0);
                    if(e.attrs.length() > MAXENTATTRS) e.attrs.setsize(MAXENTATTRS);
                }
            }
            if(e.type == ET_SUNLIGHT && hdr.version <= 38) e.attrs[1] -= 90; // reorient pitch axis
            if((maptype == MAP_OCTA && hdr.version <= 30) || (maptype == MAP_MAPZ && hdr.version <= 39)) switch(e.type)
            {
                case ET_PLAYERSTART: case ET_MAPMODEL: e.attrs[1] = (e.attrs[1] + 180)%360; break;
                case ET_SUNLIGHT: e.attrs[0] = (e.attrs[0] + 180)%360; break;
                default: break;
            }
            if(verbose && !insideworld(e.o) && e.type != ET_LIGHT && e.type != ET_LIGHTFX && e.type != ET_SUNLIGHT)
                conoutf("\frWARNING: ent outside of world: enttype[%s] index %d (%f, %f, %f)", entities::findname(e.type), i, e.o.x, e.o.y, e.o.z);
        }
        if(verbose) conoutf("loaded %d entities", hdr.numents);
        if(maptype == MAP_OCTA && sunlight)
        {
            extentity &e = *ents.add(entities::newent());
            e.attrs.add(0, 6);
            e.type = ET_SUNLIGHT;
            e.o = vec(hdr.worldsize/2, hdr.worldsize/2, hdr.worldsize*3/4);
            e.attrs[0] = sunlightyaw;
            e.attrs[1] = sunlightpitch-90;
            e.attrs[2] = int(((sunlight>>16)&0xFF)*sunlightscale*5/8);
            e.attrs[3] = int(((sunlight>>8)&0xFF)*sunlightscale*5/8);
            e.attrs[4] = int((sunlight&0xFF)*sunlightscale*5/8);
            e.attrs[5] = -1;
        }

        progress(0, "loading slots...");
        loadvslots(f, hdr.numvslots);

        progress(0, "loading octree...");
        bool failed = false;
        worldroot = loadchildren(f, ivec(0, 0, 0), hdr.worldsize>>1, failed);
        if(failed) conoutf("\frgarbage in map");

        progress(0, "validating...");
        validatec(worldroot, hdr.worldsize>>1);

        worldscale = 0;
        while(1<<worldscale < hdr.worldsize) worldscale++;

        if(!failed)
        {
            if(hdr.version >= 7) loopi(hdr.lightmaps)
            {
                progress(i/(float)hdr.lightmaps, "loading lightmaps...");
                LightMap &lm = lightmaps.add();
                if(hdr.version >= 17)
                {
                    int type = f->getchar();
                    lm.type = type&0x7F;
                    if(hdr.version >= 20 && type&0x80)
                    {
                        lm.unlitx = f->getlil<ushort>();
                        lm.unlity = f->getlil<ushort>();
                    }
                }
                if(lm.type&LM_ALPHA && (lm.type&LM_TYPE)!=LM_BUMPMAP1) lm.bpp = 4;
                lm.data = new uchar[lm.bpp*LM_PACKW*LM_PACKH];
                f->read(lm.data, lm.bpp * LM_PACKW * LM_PACKH);
                lm.finalize();
            }

            if(hdr.numpvs > 0) loadpvs(f);
            if(hdr.blendmap) loadblendmap(f);

            if(verbose) conoutf("loaded %d lightmaps", hdr.lightmaps);
        }

        progress(0, "initialising entities...");
        loopv(ents)
        {
            extentity &e = *ents[i];

            if((maptype == MAP_OCTA || (maptype == MAP_MAPZ && hdr.version <= 29)) && ents[i]->type == ET_LIGHTFX && ents[i]->attrs[0] == LFX_SPOTLIGHT)
            {
                int closest = -1;
                float closedist = 1e10f;
                loopvk(ents) if(ents[k]->type == ET_LIGHT)
                {
                    extentity &a = *ents[k];
                    float dist = e.o.dist(a.o);
                    if(dist < closedist)
                    {
                        closest = k;
                        closedist = dist;
                    }
                }
                if(ents.inrange(closest) && closedist <= 100)
                {
                    extentity &a = *ents[closest];
                    if(e.links.find(closest) < 0) e.links.add(closest);
                    if(a.links.find(i) < 0) a.links.add(i);
                    if(verbose) conoutf("\frWARNING: auto linked spotlight %d to light %d", i, closest);
                }
            }
        }
        entities::initents(maptype, hdr.version, hdr.gameid, hdr.gamever);
        delete f;
        defformatstring(fname, "%s.txt", mapname);
        char *buf = loadfile(fname, NULL);
        if(buf)
        {
            setsvar("maptext", buf, false);
            delete[] buf;
        }

        progress(0, "initialising config...");
        identflags |= IDF_WORLD;
        defformatstring(cfgname, "%s.cfg", mapname);
        if(maptype == MAP_OCTA)
        {
            execfile("config/map/octa.cfg"); // for use with -pSAUER_DIR
            execfile(cfgname);
        }
        else if(!execfile(cfgname, false)) execfile("config/map/default.cfg");
        identflags &= ~IDF_WORLD;

        progress(0, "preloading models...");
        preloadusedmapmodels(true);
        conoutf("loaded %s (\fs%s\fS by \fs%s\fS) v.%d:%d(r%d) [0x%.8x] in %.1fs", mapname, *maptitle ? maptitle : "Untitled", *mapauthor ? mapauthor : "Unknown", hdr.version, hdr.gamever, hdr.revision, mapcrc, (SDL_GetTicks()-loadingstart)/1000.0f);

        progress(0, "checking world...");
        if((maptype == MAP_OCTA && hdr.version <= 25) || (maptype == MAP_MAPZ && hdr.version <= 26))
            fixlightmapnormals();
        if((maptype == MAP_OCTA && hdr.version <= 31) || (maptype == MAP_MAPZ && hdr.version <= 40))
            fixrotatedlightmaps();

        entitiesinoctanodes();
        initlights();
        allchanged(true);

        progress(0, "preloading textures...");
        preloadtextures(IDF_GAMEPRELOAD);
        maskpackagedirs(mask);

        progress(0, "starting world...");
        game::startmap();
        maploading = 0;
        return true;
    }
    conoutf("\frunable to load %s", mname);
    setsvar("maptext", "", false);
    maploading = mapcrc = 0;
    return false;
}

void writeobj(char *name)
{
    defformatstring(fname, "%s.obj", name);
    stream *f = openfile(path(fname), "w");
    if(!f) return;
    f->printf("# obj file of Cube 2 level\n\n");
    defformatstring(mtlname, "%s.mtl", name);
    path(mtlname);
    f->printf("mtllib %s\n\n", mtlname);
    vector<vec> verts;
    vector<vec2> texcoords;
    hashtable<vec, int> shareverts(1<<16);
    hashtable<vec2, int> sharetc(1<<16);
    hashtable<int, vector<ivec2> > mtls(1<<8);
    vector<int> usedmtl;
    vec bbmin(1e16f, 1e16f, 1e16f), bbmax(-1e16f, -1e16f, -1e16f);
    loopv(valist)
    {
        vtxarray &va = *valist[i];
        ushort *edata = NULL;
        vertex *vdata = NULL;
        if(!readva(&va, edata, vdata)) continue;
        ushort *idx = edata;
        loopj(va.texs)
        {
            elementset &es = va.eslist[j];
            if(usedmtl.find(es.texture) < 0) usedmtl.add(es.texture);
            vector<ivec2> &keys = mtls[es.texture];
            loopk(es.length[1])
            {
                int n = idx[k] - va.voffset;
                const vertex &v = vdata[n];
                const vec &pos = v.pos;
                const vec2 &tc = v.tc;
                ivec2 &key = keys.add();
                key.x = shareverts.access(pos, verts.length());
                if(key.x == verts.length())
                {
                    verts.add(pos);
                    loopl(3)
                    {
                        bbmin[l] = min(bbmin[l], pos[l]);
                        bbmax[l] = max(bbmax[l], pos[l]);
                    }
                }
                key.y = sharetc.access(tc, texcoords.length());
                if(key.y == texcoords.length()) texcoords.add(tc);
            }
            idx += es.length[1];
        }
        delete[] edata;
        delete[] vdata;
    }

    vec center(-(bbmax.x + bbmin.x)/2, -(bbmax.y + bbmin.y)/2, -bbmin.z);
    loopv(verts)
    {
        vec v = verts[i];
        v.add(center);
        if(v.y != floor(v.y)) f->printf("v %.3f ", -v.y); else f->printf("v %d ", int(-v.y));
        if(v.z != floor(v.z)) f->printf("%.3f ", v.z); else f->printf("%d ", int(v.z));
        if(v.x != floor(v.x)) f->printf("%.3f\n", v.x); else f->printf("%d\n", int(v.x));
    }
    f->printf("\n");
    loopv(texcoords)
    {
        const vec2 &tc = texcoords[i];
        f->printf("vt %.6f %.6f\n", tc.x, 1-tc.y);
    }
    f->printf("\n");

    usedmtl.sort();
    loopv(usedmtl)
    {
        vector<ivec2> &keys = mtls[usedmtl[i]];
        f->printf("g slot%d\n", usedmtl[i]);
        f->printf("usemtl slot%d\n\n", usedmtl[i]);
        for(int i = 0; i < keys.length(); i += 3)
        {
            f->printf("f");
            loopk(3) f->printf(" %d/%d", keys[i+2-k].x+1, keys[i+2-k].y+1);
            f->printf("\n");
        }
        f->printf("\n");
    }
    delete f;

    f = openfile(mtlname, "w");
    if(!f) return;
    f->printf("# mtl file of map\n\n");
    loopv(usedmtl)
    {
        VSlot &vslot = lookupvslot(usedmtl[i], false);
        f->printf("newmtl slot%d\n", usedmtl[i]);
        f->printf("map_Kd %s\n", findfile(vslot.slot->sts.empty() ? notexture->name : vslot.slot->sts[0].name, "r"));
        f->printf("\n");
    }
    delete f;
}

ICOMMAND(0, writeobj, "s", (char *s), if(!(identflags&IDF_WORLD)) writeobj(s));

int getworldsize() { return hdr.worldsize; }
int getmapversion() { return hdr.version; }
ICOMMAND(0, mapversion, "", (void), intret(getmapversion()));
int getmaprevision() { return hdr.revision; }
ICOMMAND(0, maprevision, "", (void), intret(getmaprevision()));
