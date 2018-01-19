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

const char *mapvariants[MPV_MAX] = { "all", "day", "night" };
VAR(0, mapvariant, 1, 0, -1);

bool checkmapvariant(int variant)
{
    if(variant > 0 && mapvariant > 0 && mapvariant != variant) return false;
    return true;
}

void changemapvariant(int variant)
{
    if(variant != mapvariant)
    {
        mapvariant = variant;
        initskybox();
        allchanged(true);
    }
}

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
SVARF(IDF_WORLD, mapdesc, "", { string s; if(filterstring(s, mapdesc)) setsvar("mapdesc", s, false); });

void validmapname(char *dst, const char *src, const char *prefix = NULL, const char *alt = "untitled", size_t maxlen = 200)
{
    if(prefix) while(*prefix) *dst++ = *prefix++;
    const char *start = dst;
    if(src) loopi(maxlen)
    {
        char c = *src++;
        if(iscubealnum(c) || c == '_' || c == '-' || c == '/' || c == '\\') *dst++ = c;
        else break;
    }
    if(dst > start) *dst = '\0';
    else if(dst != alt) copystring(dst, alt, maxlen);
}

void setnames(const char *fname, int type, int crc)
{
    maptype = type >= 0 || type <= MAP_MAX-1 ? type : MAP_MAPZ;

    string fn, mn, mf;
    validmapname(fn, fname);
    char *fcrc = strstr(fn, "_0x");
    if(fcrc) *fcrc = '\0';
    if(crc > 0) concformatstring(fn, "_0x%.8x", crc);
    else if(crc < 0) concatstring(fn, "_0x0");

    if(strpbrk(fn, "/\\")) copystring(mn, fn);
    else formatstring(mn, "%s/%s", mapdirs[maptype].name, fn);

    // Any map with a CRC must be placed in temp/, so we add the prefix if it does not already exist.
    if(crc != 0 && strstr(mn, "temp/") != mn && strstr(mn, "temp\\") != mn) prependstring(mn, "temp/");
    // Set the mapname variable. If there is no CRC, we need to remove the temp/ prefix.
    setsvar("mapname", (crc == 0 && strstr(mn, "temp/") == mn) ? (mn + 5) : mn);

    formatstring(mf, "%s%s", mapname, mapexts[maptype].name);
    setsvar("mapfile", mf);

    if(verbose) conoutf("Set map name to %s (%s)", mapname, mapfile);
}

enum { OCTSAV_CHILDREN = 0, OCTSAV_EMPTY, OCTSAV_SOLID, OCTSAV_NORMAL };

#define LM_PACKW 512
#define LM_PACKH 512
#define LAYER_DUP (1<<7)

struct polysurfacecompat
{
    uchar lmid[2];
    uchar verts, numverts;
};

static int savemapprogress = 0;

void savec(cube *c, const ivec &o, int size, stream *f, bool nodata)
{
    if((savemapprogress++&0xFFF)==0) progress(float(savemapprogress)/allocnodes, "Saving octree...");

    loopi(8)
    {
        ivec co(i, o, size);
        if(c[i].children)
        {
            f->putchar(OCTSAV_CHILDREN);
            savec(c[i].children, co, size>>1, f, nodata);
        }
        else
        {
            int oflags = 0, surfmask = 0, totalverts = 0;
            if(c[i].material!=MAT_AIR) oflags |= 0x40;
            if(isempty(c[i])) f->putchar(oflags | OCTSAV_EMPTY);
            else
            {
                if(!nodata)
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
                        vertmask = 0, vertorder = 0,
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
                            if(v.norm) { vertmask |= 0x80; if(v.norm != verts[0].norm) matchnorm = false; }
                        }
                        if(matchnorm) vertmask |= 0x08;
                    }
                    surf.verts = vertmask;
                    f->write(&surf, sizeof(surf));
                    bool hasxyz = (vertmask&0x04)!=0, hasnorm = (vertmask&0x80)!=0;
                    if(layerverts == 4)
                    {
                        if(hasxyz && vertmask&0x01)
                        {
                            ivec v0 = verts[vertorder].getxyz(), v2 = verts[(vertorder+2)&3].getxyz();
                            f->putlil<ushort>(v0[vc]); f->putlil<ushort>(v0[vr]);
                            f->putlil<ushort>(v2[vc]); f->putlil<ushort>(v2[vr]);
                            hasxyz = false;
                        }
                    }
                    if(hasnorm && vertmask&0x08) { f->putlil<ushort>(verts[0].norm); hasnorm = false; }
                    if(hasxyz || hasnorm) loopk(layerverts)
                    {
                        const vertinfo &v = verts[(k+vertorder)%layerverts];
                        if(hasxyz)
                        {
                            ivec xyz = v.getxyz();
                            f->putlil<ushort>(xyz[vc]); f->putlil<ushort>(xyz[vr]);
                        }
                        if(hasnorm) f->putlil<ushort>(v.norm);
                    }
                }
            }
        }
    }
}

cube *loadchildren(stream *f, const ivec &co, int size, bool &failed);

void loadc(stream *f, cube &c, const ivec &co, int size, bool &failed)
{
    int octsav = f->getchar();
    switch(octsav&0x7)
    {
        case OCTSAV_CHILDREN:
            c.children = loadchildren(f, co, size>>1, failed);
            return;
        case OCTSAV_EMPTY:  emptyfaces(c);          break;
        case OCTSAV_SOLID:  solidfaces(c);          break;
        case OCTSAV_NORMAL: f->read(c.edges, 12); break;

        default: failed = true; return;
    }
    loopi(6) c.texture[i] = f->getlil<ushort>();
    if(octsav&0x40) c.material = f->getlil<ushort>();
    if(octsav&0x80) c.merged = f->getchar();
    if(octsav&0x20)
    {
        int surfmask, totalverts;
        surfmask = f->getchar();
        totalverts = max(f->getchar(), 0);
        newcubeext(c, totalverts, false);
        memset(c.ext->surfaces, 0, sizeof(c.ext->surfaces));
        memset(c.ext->verts(), 0, totalverts*sizeof(vertinfo));
        int offset = 0;
        loopi(6) if(surfmask&(1<<i))
        {
            surfaceinfo &surf = c.ext->surfaces[i];
            if(hdr.version <= 43)
            {
                polysurfacecompat psurf;
                f->read(&psurf, sizeof(polysurfacecompat));
                surf.verts = psurf.verts;
                surf.numverts = psurf.numverts;
            }
            else f->read(&surf, sizeof(surf));
            int vertmask = surf.verts, numverts = surf.totalverts();
            if(!numverts) { surf.verts = 0; continue; }
            surf.verts = offset;
            vertinfo *verts = c.ext->verts() + offset;
            offset += numverts;
            ivec v[4], n, vo = ivec(co).mask(0xFFF).shl(3);
            int layerverts = surf.numverts&MAXFACEVERTS, dim = dimension(i), vc = C[dim], vr = R[dim], bias = 0;
            genfaceverts(c, i, v);
            bool hasxyz = (vertmask&0x04)!=0, hasuv = hdr.version <= 43 && (vertmask&0x40)!=0, hasnorm = (vertmask&0x80)!=0;
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
                    loopk(4) f->getlil<ushort>();
                    if(surf.numverts&LAYER_DUP) loopk(4) f->getlil<ushort>();
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
                if(hasuv) { f->getlil<ushort>(); f->getlil<ushort>(); }
                if(hasnorm) v.norm = f->getlil<ushort>();
            }
            if(hasuv && surf.numverts&LAYER_DUP) loopk(layerverts) { f->getlil<ushort>(); f->getlil<ushort>(); }
        }
    }
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
        loopk(2) f->putlil<int>(vs.offset[k]);
    }
    if(vs.changed & (1<<VSLOT_SCROLL))
    {
        loopk(2) f->putlil<float>(vs.scroll[k]);
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
    if(vs.changed & (1<<VSLOT_REFRACT))
    {
        f->putlil<float>(vs.refractscale);
        loopk(3) f->putlil<float>(vs.refractcolor[k]);
    }
    if(vs.changed & (1<<VSLOT_DETAIL)) f->putlil<int>(vs.detail);
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
        loopk(2) vs.offset[k] = f->getlil<int>();
    }
    if(vs.changed & (1<<VSLOT_SCROLL))
    {
        loopk(2) vs.scroll[k] = f->getlil<float>();
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
    if(vs.changed & (1<<VSLOT_REFRACT))
    {
        vs.refractscale = f->getlil<float>();
        loopk(3) vs.refractcolor[k] = f->getlil<float>();
    }
    if(vs.changed & (1<<VSLOT_DETAIL)) vs.detail = f->getlil<int>();
}

void loadvslots(stream *f, int numvslots)
{
    int *prev = new (false) int[numvslots];
    if(!prev) return;
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

void saveslotconfig(stream *h, Slot &s, int index, bool decal)
{
    if(index >= 0 && s.shader)
    {
        h->printf("setshader %s\n", escapeid(s.shader->name));
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
        if(decal && !j) h->printf(" decal");
        else if(index >= 0) h->printf(" %s", findtexturetypename(s.sts[j].type));
        else if(!j) h->printf(" %s", findmaterialname(-index));
        else h->printf(" 1");
        h->printf(" %s", escapestring(s.sts[j].name));
        if(!j && index >= 0) h->printf(" // %d", index);
        h->printf("\n");
    }

    if(s.variants->offset.x || s.variants->offset.y) h->printf("texoffset %d %d\n", s.variants->offset.x, s.variants->offset.y);
    if(s.variants->rotation) h->printf("texrotate %d\n", s.variants->rotation);
    if(s.variants->scale != 1) h->printf("texscale %g\n", s.variants->scale);
    if(s.variants->colorscale != vec(1, 1, 1))
        h->printf("texcolor %f %f %f\n", s.variants->colorscale.x, s.variants->colorscale.y, s.variants->colorscale.z);
    if(s.variants->palette || s.variants->palindex) h->printf("texpalette %d %d\n", s.variants->palette, s.variants->palindex);
    if(s.variants->refractscale > 0)
        h->printf("texrefract %g %g %g %g", s.variants->refractscale, s.variants->refractcolor.x,  s.variants->refractcolor.y, s.variants->refractcolor.z);
    if(!s.variants->scroll.iszero())
        h->printf("texscroll %f %f\n", s.variants->scroll.x * 1000.0f, s.variants->scroll.y * 1000.0f);

    if(decal)
    {
        DecalSlot &d = (DecalSlot &)s;
        if(d.depth != DEFAULTDECALDEPTH || d.fade != DEFAULTDECALFADE)
            h->printf("decaldepth %f %f", d.depth, d.fade);
    }
    if(index >= 0)
    {
        if(s.variants->layer != 0) h->printf("texlayer %d\n", s.variants->layer);
        if(s.variants->detail != 0) h->printf("texdetail %d\n", s.variants->detail);
        if(s.smooth >= 0) h->printf("texsmooth %i -1\n", s.smooth);

        if(s.variants->alphafront != DEFAULT_ALPHA_FRONT || s.variants->alphaback != DEFAULT_ALPHA_BACK)
            h->printf("texalpha %f %f\n", s.variants->alphafront, s.variants->alphaback);

        if(s.grass && s.grass[0])
        {
            h->printf("texgrass %s\n", escapestring(s.grass));
            if(s.grasscolor != vec(0, 0, 0))
                h->printf("texgrasscolor %f %f %f\n", s.grasscolor.x, s.grasscolor.y, s.grasscolor.z);
            if(s.grassblend > 0) h->printf("texgrassblend %f\n", s.grassblend);
            if(s.grassscale > 0) h->printf("texgrassscale %d\n", s.grassscale);
            if(s.grassheight > 0) h->printf("texgrassheight %d\n", s.grassheight);
        }
        if(s.variants->coastscale != 1) h->printf("texcoastscale %f\n", s.variants->coastscale);
    }
    h->printf("\n");
}

void save_config(char *mname)
{
    if(autosavebackups) backup(mname, ".cfg", hdr.revision, autosavebackups > 2, !(autosavebackups%2));
    defformatstring(fname, "%s.cfg", mname);
    stream *h = openutf8file(fname, "w");
    if(!h) { conoutf("\frCould not write config to %s", fname); return; }

    string title, author, desc;
    if(*maptitle) filterstring(title, maptitle);
    else copystring(title, maptitle);
    if(*mapauthor) filterstring(author, mapauthor);
    else copystring(author, mapauthor);
    if(*mapdesc) filterstring(desc, mapdesc);
    else copystring(desc, mapdesc);
    // config
    h->printf("// %s by %s (%s)\n", title, author, mapname);
    if(*desc) h->printf("// %s\n", desc);
    h->printf("\n");

    int aliases = 0;
    enumerate(idents, ident, id,
    {
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
    });
    if(aliases) h->printf("\n");
    if(verbose) conoutf("Saved %d aliases", aliases);

    // texture slots
    int nummats = sizeof(materialslots)/sizeof(materialslots[0]);
    loopi(nummats)
    {
        progress(i/float(nummats), "Saving material slots...");

        switch(i&MATF_VOLUME)
        {
            case MAT_WATER: case MAT_GLASS: case MAT_LAVA:
                saveslotconfig(h, materialslots[i], -i, false);
                break;
        }
    }
    if(verbose) conoutf("Saved %d material slots", nummats);

    loopv(smoothgroups)
    {
        if(smoothgroups[i] < 0) continue;
        h->printf("smoothangle %i %i\n", i, smoothgroups[i]);
    }

    loopv(slots)
    {
        progress(i/float(slots.length()), "Saving texture slots...");
        saveslotconfig(h, *slots[i], i, false);
    }
    if(verbose) conoutf("Saved %d texture slots", slots.length());

    loopv(decalslots)
    {
        progress(i/float(decalslots.length()), "Saving decal slots...");
        saveslotconfig(h, *decalslots[i], i, true);
    }
    if(verbose) conoutf("Saved %d decal slots", decalslots.length());

    loopv(mapmodels)
    {
        progress(i/float(mapmodels.length()), "Saving mapmodel slots...");
        h->printf("mapmodel %s\n", escapestring(mapmodels[i].name));
    }
    if(mapmodels.length()) h->printf("\n");
    if(verbose) conoutf("Saved %d mapmodel slots", mapmodels.length());

    loopv(mapsounds)
    {
        progress(i/float(mapsounds.length()), "Saving mapsound slots...");
        h->printf("mapsound %s", escapestring(mapsounds[i].name));
        if((mapsounds[i].vol > 0 && mapsounds[i].vol < 255) || mapsounds[i].maxrad > 0 || mapsounds[i].minrad >= 0)
            h->printf(" %d", mapsounds[i].vol);
        if(mapsounds[i].maxrad > 0 || mapsounds[i].minrad >= 0) h->printf(" %d", mapsounds[i].maxrad);
        if(mapsounds[i].minrad >= 0) h->printf(" %d", mapsounds[i].minrad);
        h->printf("\n");
    }
    if(mapsounds.length()) h->printf("\n");
    if(verbose) conoutf("Saved %d mapsound slots", mapsounds.length());

    delete h;
    if(verbose) conoutf("Saved config %s", fname);
}
ICOMMAND(0, savemapconfig, "s", (char *mname), if(!(identflags&IDF_WORLD)) save_config(*mname ? mname : mapname));

VARF(IDF_PERSIST, mapshotsize, 0, 512, INT_MAX-1, mapshotsize -= mapshotsize%2);

void save_mapshot(char *mname)
{
    if(autosavebackups) backup(mname, ifmtexts[imageformat], hdr.revision, autosavebackups > 2, !(autosavebackups%2));
    GLuint tex;
    glGenTextures(1, &tex);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    ImageData image(vieww, viewh, 3);
    memset(image.data, 0, 3*vieww*viewh);
    drawtex = DRAWTEX_MAPSHOT;
    gl_drawview();
    drawtex = 0;
    glReadPixels(0, 0, vieww, viewh, GL_RGB, GL_UNSIGNED_BYTE, image.data);
    #if 0 // generates better images without this
    int x = 0, y = 0, w = vieww, h = viewh;
    if(w > h) { x += (w-h)/2; w = h; }
    else if(h > w) { y += (h-w)/2; h = w; }
    if(x || y) texcrop(image, x, y, w, h);
    #endif
    if(vieww > mapshotsize || viewh > mapshotsize) scaleimage(image, mapshotsize, mapshotsize);
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
    conoutf("Saving: %s (%s)", mapname, forcesave ? "forced" : "normal");

    stream *f = opengzfile(mapfile, "wb");
    if(!f) { conoutf("\frError saving %s to %s: file error", mapname, mapfile); return; }

    if(autosavemapshot || forcesave) save_mapshot(mapname);
    if(autosaveconfigs || forcesave) save_config(mapname);
    if(maptext[0] && (autosavetexts || forcesave))
    {
        defformatstring(fname, "%s.txt", mapname);
        stream *h = openutf8file(fname, "w");
        if(!h) conoutf("\frCould not write text to %s", fname);
        else
        {
            h->printf("%s", maptext);
            delete h;
            if(verbose) conoutf("Saved text %s", fname);
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
    progress(-1, "Saving map..");
    memcpy(hdr.head, "MAPZ", 4);
    hdr.version = MAPVERSION;
    hdr.headersize = sizeof(mapz);
    hdr.worldsize = worldsize;
    hdr.gamever = server::getver(1);
    hdr.numents = 0;
    const vector<extentity *> &ents = entities::getents();
    loopv(ents) if(ents[i]->type!=ET_EMPTY || forcesave) hdr.numents++;
    hdr.numpvs = nodata ? 0 : getnumviewcells();
    hdr.blendmap = shouldsaveblendmap();
    hdr.numvslots = numvslots;
    hdr.revision++;
    string gameid;
    copystring(gameid, server::gameid());
    memcpy(hdr.gameid, gameid, 4);

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
            progress(vars/float(numvars), "Saving world variables...");
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

    if(verbose) conoutf("Saved %d variables", vars);

    // texture slots
    f->putlil<ushort>(texmru.length());
    loopv(texmru) f->putlil<ushort>(texmru[i]);

    // entities
    int count = 0;
    vector<int> remapents;
    if(!forcesave) entities::remapents(remapents);
    loopv(ents) // extended
    {
        progress(i/float(ents.length()), "Saving entities...");
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
                if(verbose >= 2) conoutf("Entity %s (%d) saved %d links", entities::findname(e.type), i, links.length());
            }
            count++;
        }
    }
    if(verbose) conoutf("Saved %d entities", count);

    savevslots(f, numvslots);
    if(verbose) conoutf("Saved %d vslots", numvslots);

    progress(0, "Saving octree...");
    savec(worldroot, ivec(0, 0, 0), worldsize>>1, f, nodata);

    if(!nodata)
    {
        if(getnumviewcells()>0)
        {
            progress(0, "Saving PVS...");
            savepvs(f);
            if(verbose) conoutf("Saved %d PVS view cells", getnumviewcells());
        }
    }
    if(shouldsaveblendmap())
    {
        progress(0, "Saving blendmap...");
        saveblendmap(f);
        if(verbose) conoutf("Saved blendmap");
    }
    delete f;
    mapcrc = crcfile(mapfile);
    conoutf("Saved %s (\fs%s\fS by \fs%s\fS) v%d:%d(r%d) [0x%.8x] in %.1f secs", mapname, *maptitle ? maptitle : "Untitled", *mapauthor ? mapauthor : "Unknown", hdr.version, hdr.gamever, hdr.revision, mapcrc, (SDL_GetTicks()-savingstart)/1000.0f);
}

ICOMMAND(0, savemap, "s", (char *mname), if(!(identflags&IDF_WORLD)) save_world(*mname ? mname : mapname));

static void sanevars()
{
    setvar("fullbright", 0, false);
    setvar("blankgeom", 0, false);
}

const char *variantvars[] = {
    "ambient", "ambientscale", "skylight", "skylightscale", "fog", "fogcolour", "skybgcolour", "skybox", "skycolour", "skyblend", "skyoverbright", "skyoverbrightmin",
    "skyoverbrightthreshold", "spinsky", "yawsky", "cloudbox", "cloudcolour", "cloudblend", "spinclouds", "yawclouds", "cloudclip", "cloudlayer", "cloudlayercolour",
    "cloudlayerblend", "cloudoffsetx", "cloudoffsety", "cloudscrollx", "cloudscrolly", "cloudscale", "spincloudlayer", "yawcloudlayer", "cloudheight", "cloudfade",
    "cloudsubdiv", "envlayer", "envlayercolour", "envlayerblend", "envoffsetx", "envoffsety", "envscrollx", "envscrolly", "envscale", "spinenvlayer", "yawenvlayer",
    "envheight", "envfade", "envsubdiv", "atmo", "atmoplanetsize", "atmoheight", "atmobright", "atmolight", "atmolightscale", "atmodisksize", "atmodiskbright",
    "atmohaze", "atmohazefade", "atmohazefadescale", "atmoclarity", "atmodensity", "atmoblend", "fogdomeheight", "fogdomemin", "fogdomemax", "fogdomecap", "fogdomeclip",
    "fogdomecolour", "fogdomeclouds", "skytexture", "skyshadow", NULL
};
const char *skypievars[] = { "light", "lightscale", "lightyaw", "lightpitch", NULL };

void copydayvariables(bool rev = false)
{
    for(int v = 0; variantvars[v]; v++)
    {
        defformatstring(newvar, "%snight", variantvars[v]);
        ident *id = idents.access(rev ? newvar : variantvars[v]);
        if(id) switch(id->type)
        {
            case ID_VAR: setvar(rev ? variantvars[v] : newvar, *id->storage.i, true, false, true); break;
            case ID_FVAR: setfvar(rev ? variantvars[v] : newvar, *id->storage.f, true, false, true); break;
            case ID_SVAR: setsvar(rev ? variantvars[v] : newvar, *id->storage.s, true, false); break;
            default: break;
        }
    }
    for(int v = 0; skypievars[v]; v++)
    {
        defformatstring(sunvar, "sun%s", skypievars[v]);
        defformatstring(moonvar, "moon%s", skypievars[v]);
        ident *id = idents.access(rev ? moonvar : sunvar);
        if(id) switch(id->type)
        {
            case ID_VAR: setvar(rev ? sunvar : moonvar, *id->storage.i, true, false, true); break;
            case ID_FVAR: setfvar(rev ? sunvar : moonvar, *id->storage.f, true, false, true); break;
            case ID_SVAR: setsvar(rev ? sunvar : moonvar, *id->storage.s, true, false); break;
            default: break;
        }
    }
    conoutf(rev ? "\fyNight variables copied to Day." : "\fyDay variables copied to Night.");
}
ICOMMAND(0, copydayvars, "i", (int *n), if(editmode) copydayvariables(*n != 0));

bool load_world(const char *mname, int crc, int variant)
{
    int loadingstart = SDL_GetTicks();
    mapcrc = 0;
    setsvar("maptext", "", false);
    loop(format, MAP_MAX) loop(tempfile, crc > 0 ? 2 : 1)
    {
        int mask = maskpackagedirs(format == MAP_OCTA ? ~0 : ~PACKAGEDIR_OCTA);
        setnames(mname, format, tempfile && crc > 0 ? crc : 0);

        int filecrc = crcfile(mapfile);
        if(crc > 0) conoutf("Checking map: %s [0x%.8x] (need: 0x%.8x)", mapfile, filecrc, crc);
        if(!tempfile && crc > 0 && crc != filecrc)
        {
            maskpackagedirs(mask);
            continue; // skipped iteration
        }
        stream *f = opengzfile(mapfile, "rb");
        if(!f)
        {
            conoutf("\frNot found: %s", mapfile);
            maskpackagedirs(mask);
            continue;
        }

        bool samegame = true;
        int eif = 0, lightmaps = 0;
        mapz newhdr;
        if(f->read(&newhdr, sizeof(binary))!=(int)sizeof(binary))
        {
            conoutf("\frError loading %s: malformatted universal header", mapname);
            delete f;
            maskpackagedirs(mask);
            return false;
        }
        lilswap(&newhdr.version, 2);

        clearworldvars();
        setvar("mapscale", 0, true, false, true);
        setvar("mapsize", 0, true, false, true);
        setvar("emptymap", 0, true, false, true);
        if(memcmp(newhdr.head, "MAPZ", 4) == 0 || memcmp(newhdr.head, "BFGZ", 4) == 0)
        {
            if(newhdr.version > MAPVERSION)
            {
                conoutf("\frError loading %s: requires a newer version of %s (with map format version %d)", mapname, versionname, newhdr.version);
                delete f;
                maskpackagedirs(mask);
                return false;
            }
            else if(newhdr.version <= 42)
            {
                conoutf("\frError loading %s: requires an older version of %s (with map format version %d)", mapname, versionname, newhdr.version);
                delete f;
                maskpackagedirs(mask);
                return false;
            }

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
            if(newhdr.version <= 43)
            {
                MAPZCOMPAT(43);
                lilswap(&chdr.worldsize, 8);
                lightmaps = chdr.lightmaps;
                memcpy(&newhdr.worldsize, &chdr.worldsize, sizeof(int)*3);
                memcpy(&newhdr.blendmap, &chdr.blendmap, sizeof(int)*4);
                memcpy(&newhdr.gameid, &chdr.gameid, 4);
            }
            else
            {
                if(size_t(newhdr.headersize) > sizeof(newhdr) || f->read(&newhdr.worldsize, newhdr.headersize-sizeof(binary))!=size_t(newhdr.headersize)-sizeof(binary))
                {
                    conoutf("\frError loading %s: malformatted mapz v%d header", mapname, newhdr.version);
                    delete f;
                    maskpackagedirs(mask);
                    return false;
                }
                lilswap(&newhdr.worldsize, 7);
            }
            #undef MAPZCOMPAT

            resetmap(false, variant);
            hdr = newhdr;
            maploading = 1;
            progress(-1, "Please wait...");
            maptype = MAP_MAPZ;
            mapcrc = filecrc;

            if(verbose) conoutf("Loading v%d map from %s game v%d [%d]", hdr.version, hdr.gameid, hdr.gamever, hdr.worldsize);

            int numvars = f->getlil<int>(), vars = 0;
            identflags |= IDF_WORLD;
            progress(0, "Loading variables...");
            loopi(numvars)
            {
                progress(i/float(numvars), "Loading variables...");
                int len = f->getlil<int>();
                if(len)
                {
                    string name;
                    f->read(name, len+1);
                    ident *id = idents.access(name);
                    bool proceed = true;
                    int type = f->getlil<int>();
                    if(!id || type != id->type || !(id->flags&IDF_WORLD) || id->flags&IDF_SERVER)
                        proceed = false;

                    switch(type)
                    {
                        case ID_VAR:
                        {
                            int val = f->getlil<int>();
                            if(proceed)
                            {
                                if(id->flags&IDF_HEX && uint(id->maxval) == 0xFFFFFFFFU)
                                {
                                    if(uint(val) > uint(id->maxval)) val = uint(id->maxval);
                                    else if(uint(val) < uint(id->minval)) val = uint(id->minval);
                                }
                                else if(val > id->maxval) val = id->maxval;
                                else if(val < id->minval) val = id->minval;
                                setvar(name, val, true);
                            }
                            break;
                        }
                        case ID_FVAR:
                        {
                            float val = f->getlil<float>();
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
                            proceed = false;
                            break;
                        }
                    }
                    if(!proceed) conoutf("\frWARNING: ignoring variable %s stored in map", name);
                    else vars++;
                }
            }
            identflags &= ~IDF_WORLD;
            if(verbose) conoutf("Loaded %d/%d variables", vars, numvars);
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
            if(ohdr.version != OCTAVERSION)
            {
                conoutf("\frError loading %s: requires %s version of Cube 2 support (version %d)", mapname, ohdr.version < OCTAVERSION ? "an older" : "a newer", ohdr.version);
                delete f;
                maskpackagedirs(mask);
                return false;
            }
            if(f->read(&ohdr.worldsize, sizeof(octa)-sizeof(binary))!=sizeof(octa)-(int)sizeof(binary))
            {
                conoutf("\frError loading %s: malformatted octa v%d header", mapname, ohdr.version);
                delete f;
                maskpackagedirs(mask);
                return false;
            }
            lilswap(&ohdr.worldsize, 7);

            resetmap(false, variant);
            hdr = newhdr;
            maploading = 1;
            progress(-1, "Please wait..");
            maptype = MAP_OCTA;

            memcpy(hdr.head, ohdr.head, 4);
            hdr.gamever = 0; // sauer has no gamever
            hdr.worldsize = ohdr.worldsize;
            if(hdr.worldsize > 1<<18) hdr.worldsize = 1<<18;
            hdr.numents = ohdr.numents;
            hdr.numpvs = ohdr.numpvs;
            lightmaps = ohdr.lightmaps;
            hdr.blendmap = ohdr.blendmap;
            hdr.numvslots = ohdr.numvslots;
            hdr.revision = 1;

            loopi(ohdr.numvars)
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
            int len = f->getchar();
            f->read(gameid, len+1);
            memcpy(hdr.gameid, gameid, 4);

            if(!server::canload(hdr.gameid))
            {
                if(verbose) conoutf("\frWARNING: loading OCTA v%d map from %s game, ignoring game specific data", hdr.version, hdr.gameid);
                samegame = false;
            }
            else if(verbose) conoutf("Loading OCTA v%d map from %s game", hdr.version, hdr.gameid);

            eif = f->getlil<ushort>();
            int extrasize = f->getlil<ushort>();
            loopj(extrasize) f->getchar();
        }
        else
        {
            delete f;
            maskpackagedirs(mask);
            continue;
        }

        progress(0, "Clearing world...");

        texmru.shrink(0);
        ushort nummru = f->getlil<ushort>();
        loopi(nummru) texmru.add(f->getlil<ushort>());

        freeocta(worldroot);
        worldroot = NULL;

        int ws = 0;
        while(1<<ws < hdr.worldsize) ws++;
        setvar("mapsize", 1<<ws, true, false, true);
        setvar("mapscale", ws, true, false, true);

        progress(0, "Loading entities...");
        vector<extentity *> &ents = entities::getents();
        int importedsuns = 0;
        float importedsunyaw = 0, importedsunpitch = 0;
        vec importedsuncolor(0, 0, 0);
        loopi(hdr.numents)
        {
            progress(i/float(hdr.numents), "Loading entities...");
            extentity &e = *ents.add(entities::newent());
            if(maptype == MAP_OCTA)
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
            if(maptype == MAP_OCTA) f->seek(eif, SEEK_CUR);

            // version increments
            if(maptype == MAP_OCTA && e.type >= ET_DECAL) e.type++;
            bool oldsun = maptype == MAP_MAPZ && hdr.version <= 43 && e.type == ET_DECAL;
            if(!samegame && e.type >= ET_GAMESPECIFIC)
            {
                if(oldsun || (maptype == MAP_MAPZ && entities::maylink(e.type, hdr.gamever)))
                {
                    int links = f->getlil<int>();
                    f->seek(sizeof(int)*links, SEEK_CUR);
                }
                e.type = ET_EMPTY;
                continue;
            }
            else if(oldsun)
            {
                int links = f->getlil<int>();
                loopk(links) f->getlil<int>(); //f->seek(sizeof(int)*links, SEEK_CUR);
                importedsuns++;
                importedsunyaw += e.attrs[1];
                importedsunpitch += e.attrs[2];
                importedsuncolor.add(vec(e.attrs[2], e.attrs[3], e.attrs[4]).div(255));
                e.type = ET_EMPTY;
                continue;
            }

            entities::readent(f, maptype, hdr.version, hdr.gameid, hdr.gamever, i);

            if(maptype == MAP_MAPZ && entities::maylink(e.type, hdr.gamever))
            {
                int links = f->getlil<int>();
                e.links.add(0, links);
                loopk(links) e.links[k] = f->getlil<int>();
                if(verbose >= 2) conoutf("Entity %s (%d) loaded %d link(s)", entities::findname(e.type), i, links);
            }

            if(maptype == MAP_OCTA && e.type == ET_PARTICLES && e.attrs[0] >= 11)
            {
                if(e.attrs[0] <= 12) e.attrs[0] += 3;
                else e.attrs[0] = 0; // bork it up
            }
            if(maptype == MAP_OCTA && e.type == ET_MAPMODEL)
            {
                int angle = e.attrs[0];
                e.attrs[0] = e.attrs[1];
                e.attrs[1] = angle;
                loopk(e.attrs.length()-2) e.attrs[k+2] = 0;
            }
            if((maptype == MAP_OCTA || (maptype == MAP_MAPZ && hdr.version <= 43)) && e.type == ET_MAPMODEL)
            {
                e.attrs[1] -= 90;
                while(e.attrs[1] < 0) e.attrs[1] += 360;
            }
            if(maptype == MAP_MAPZ && hdr.version <= 43 && e.type == ET_LIGHTFX)
            {
                if(e.attrs[0] == LFX_FLICKER) e.type = ET_EMPTY;
                else if(e.attrs[0] > LFX_FLICKER) e.attrs[0] -= 1;
            }
            if(!insideworld(e.o) && e.type != ET_LIGHT && e.type != ET_LIGHTFX)
                conoutf("\frWARNING: ent outside of world: enttype[%d](%s) index %d (%f, %f, %f) [%d, %d]", e.type, entities::findname(e.type), i, e.o.x, e.o.y, e.o.z, worldsize, worldscale);
        }
        if(maptype == MAP_MAPZ && hdr.version <= 43)
        {
            if(importedsuns)
            {
                if(importedsuns > 1)
                {
                    importedsunyaw /= importedsuns;
                    importedsunpitch /= importedsuns;
                    importedsuncolor.div(importedsuns);
                }
                importedsunpitch += 90.f;
                while(importedsunpitch > 90.f)
                {
                    importedsunpitch -= 90.f;
                    importedsunyaw += 180.f;
                }
                while(importedsunpitch < -90.f)
                {
                    importedsunpitch += 90.f;
                    importedsunyaw += 180.f;
                }
                while(importedsunyaw > 360.f) importedsunyaw -= 360.f;
                while(importedsunyaw < 0.f) importedsunyaw += 360.f;
                importedsuncolor.normalize();
                setvar("sunlight", importedsuncolor.tohexcolor(), true, false, true);
                setfvar("sunlightyaw", importedsunyaw, true, false, true);
                setfvar("sunlightpitch", importedsunpitch, true, false, true);
            }
            else
            {
                extern bvec skylight;
                if(!skylight.iszero())
                {
                    setvar("sunlight", 0x010101, true, false, true);
                    setfvar("sunlightyaw", 0, true, false, true);
                    setfvar("sunlightpitch", 90, true, false, true);
                }
            }
        }
        if(maptype == MAP_OCTA || (maptype == MAP_MAPZ && hdr.version <= 44))
            copydayvariables();

        if(verbose) conoutf("Loaded %d entities", hdr.numents);

        progress(0, "Loading slots...");
        loadvslots(f, hdr.numvslots);

        progress(0, "Loading octree...");
        bool failed = false;
        worldroot = loadchildren(f, ivec(0, 0, 0), hdr.worldsize>>1, failed);
        if(failed) conoutf("\frGarbage in map");

        progress(0, "Validating...");
        validatec(worldroot, hdr.worldsize>>1);

        if(!failed)
        {
            if(lightmaps > 0) loopi(lightmaps)
            {
                int type = f->getchar();
                if(type&0x80) loopi(2) f->getlil<ushort>();
                int bpp = 3;
                if(type&(1<<4) && (type&0x0F)!=2) bpp = 4;
                f->seek(bpp*LM_PACKW*LM_PACKH, SEEK_CUR);
            }

            if(hdr.numpvs > 0) loadpvs(f, hdr.numpvs);
            if(hdr.blendmap) loadblendmap(f, hdr.blendmap);
        }

        progress(0, "Initialising entities...");
        loopv(ents)
        {
            extentity &e = *ents[i];

            if(maptype == MAP_OCTA && ents[i]->type == ET_LIGHTFX && ents[i]->attrs[0] == LFX_SPOTLIGHT)
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
            if(buf[0]) setsvar("maptext", buf, false);
            delete[] buf;
        }

        progress(0, "Initialising config...");
        identflags |= IDF_WORLD;
        defformatstring(cfgname, "%s.cfg", mapname);
        if(maptype == MAP_OCTA)
        {
            execfile("config/map/octa.cfg"); // for use with -pSAUER_DIR
            execfile(cfgname);
        }
        else if(!execfile(cfgname, false)) execfile("config/map/default.cfg");
        if(maptype == MAP_OCTA || (maptype == MAP_MAPZ && hdr.version <= 43))
        {
            resetmaterials();
            execfile("config/map/material.cfg");
            resetdecals();
            execfile("config/map/decals.cfg");
        }
        identflags &= ~IDF_WORLD;

        progress(0, "Preloading models...");
        preloadusedmapmodels(true);
        conoutf("Loaded %s (\fs%s\fS by \fs%s\fS) v.%d:%d(r%d) [0x%.8x] in %.1fs", mapname, *maptitle ? maptitle : "Untitled", *mapauthor ? mapauthor : "Unknown", hdr.version, hdr.gamever, hdr.revision, mapcrc, (SDL_GetTicks()-loadingstart)/1000.0f);

        progress(0, "Checking world...");

        entitiesinoctanodes();
        initlights();
        allchanged(true);

        progress(0, "Preloading textures...");
        preloadtextures(IDF_GAMEPRELOAD);
        maskpackagedirs(mask);

        progress(0, "Starting world...");
        game::startmap();
        maploading = 0;
        return true;
    }
    conoutf("\frUnable to load %s", mname);
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
    vector<vec> verts, texcoords;
    hashtable<vec, int> shareverts(1<<16), sharetc(1<<16);
    hashtable<int, vector<ivec2> > mtls(1<<8);
    vector<int> usedmtl;
    vec bbmin(1e16f, 1e16f, 1e16f), bbmax(-1e16f, -1e16f, -1e16f);
    loopv(valist)
    {
        vtxarray &va = *valist[i];
        if(!va.edata || !va.vdata) continue;
        ushort *edata = va.edata + va.eoffset;
        vertex *vdata = va.vdata;
        ushort *idx = edata;
        loopj(va.texs)
        {
            elementset &es = va.texelems[j];
            if(usedmtl.find(es.texture) < 0) usedmtl.add(es.texture);
            vector<ivec2> &keys = mtls[es.texture];
            loopk(es.length)
            {
                const vertex &v = vdata[idx[k]];
                const vec &pos = v.pos;
                const vec &tc = v.tc;
                ivec2 &key = keys.add();
                key.x = shareverts.access(pos, verts.length());
                if(key.x == verts.length())
                {
                    verts.add(pos);
                    bbmin.min(pos);
                    bbmax.max(pos);
                }
                key.y = sharetc.access(tc, texcoords.length());
                if(key.y == texcoords.length()) texcoords.add(tc);
            }
            idx += es.length;
        }
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
        const vec &tc = texcoords[i];
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

    conoutf("Generated model: %s", fname);
}

ICOMMAND(0, writeobj, "s", (char *s), if(!(identflags&IDF_WORLD)) writeobj(s));

void writecollideobj(char *name)
{
    if(!havesel)
    {
        conoutf("\frGeometry for collide model not selected");
        return;
    }
    vector<extentity *> &ents = entities::getents();
    extentity *mm = NULL;
    loopv(entgroup)
    {
        extentity &e = *ents[entgroup[i]];
        if(e.type != ET_MAPMODEL || !pointinsel(sel, e.o)) continue;
        mm = &e;
        break;
    }
    if(!mm) loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_MAPMODEL || !pointinsel(sel, e.o)) continue;
        mm = &e;
        break;
    }
    if(!mm)
    {
        conoutf("\frCould not find map model in selection");
        return;
    }
    model *m = loadmapmodel(mm->attrs[0]);
    if(!m)
    {
        mapmodelinfo *mmi = getmminfo(mm->attrs[0]);
        if(mmi) conoutf("\frCould not load map model: %s", mmi->name);
        else conoutf("Could not find map model: %d", mm->attrs[0]);
        return;
    }

    matrix4x3 xform;
    m->calctransform(xform);
    float scale = mm->attrs[5] > 0 ? mm->attrs[5]/100.0f : 1;
    int yaw = mm->attrs[1], pitch = mm->attrs[2], roll = mm->attrs[3];
    matrix3 orient;
    orient.identity();
    if(scale != 1) orient.scale(scale);
    if(yaw) orient.rotate_around_z(sincosmod360(yaw));
    if(pitch) orient.rotate_around_x(sincosmod360(pitch));
    if(roll) orient.rotate_around_y(sincosmod360(-roll));
    xform.mul(orient, mm->o, matrix4x3(xform));
    xform.invert();

    ivec selmin = sel.o, selmax = ivec(sel.s).mul(sel.grid).add(sel.o);
    vector<vec> verts;
    hashtable<vec, int> shareverts;
    vector<int> tris;
    loopv(valist)
    {
        vtxarray &va = *valist[i];
        if(va.geommin.x > selmax.x || va.geommin.y > selmax.y || va.geommin.z > selmax.z ||
           va.geommax.x < selmin.x || va.geommax.y < selmin.y || va.geommax.z < selmin.z)
            continue;
        if(!va.edata || !va.vdata) continue;
        ushort *edata = va.edata + va.eoffset;
        vertex *vdata = va.vdata;
        ushort *idx = edata;
        loopj(va.texs)
        {
            elementset &es = va.texelems[j];
            for(int k = 0; k < es.length; k += 3)
            {
                const vec &v0 = vdata[idx[k]].pos, &v1 = vdata[idx[k+1]].pos, &v2 = vdata[idx[k+2]].pos;
                if(!v0.insidebb(selmin, selmax) || !v1.insidebb(selmin, selmax) || !v2.insidebb(selmin, selmax))
                    continue;
                int i0 = shareverts.access(v0, verts.length());
                if(i0 == verts.length()) verts.add(v0);
                tris.add(i0);
                int i1 = shareverts.access(v1, verts.length());
                if(i1 == verts.length()) verts.add(v1);
                tris.add(i1);
                int i2 = shareverts.access(v2, verts.length());
                if(i2 == verts.length()) verts.add(v2);
                tris.add(i2);
            }
            idx += es.length;
        }
    }

    defformatstring(fname, "%s.obj", name);
    stream *f = openfile(path(fname), "w");
    if(!f) return;
    f->printf("# obj file of Cube 2 collide model\n\n");
    loopv(verts)
    {
        vec v = xform.transform(verts[i]);
        if(v.y != floor(v.y)) f->printf("v %.3f ", -v.y); else f->printf("v %d ", int(-v.y));
        if(v.z != floor(v.z)) f->printf("%.3f ", v.z); else f->printf("%d ", int(v.z));
        if(v.x != floor(v.x)) f->printf("%.3f\n", v.x); else f->printf("%d\n", int(v.x));
    }
    f->printf("\n");
    for(int i = 0; i < tris.length(); i += 3)
       f->printf("f %d %d %d\n", tris[i+2]+1, tris[i+1]+1, tris[i]+1);
    f->printf("\n");

    delete f;

    conoutf("Generated collide model: %s", fname);
}

COMMAND(0, writecollideobj, "s");

ICOMMAND(0, mapversion, "", (void), intret(hdr.version));
ICOMMAND(0, maprevision, "", (void), intret(hdr.revision));

ICOMMAND(0, getmapfile, "s", (char *s),
{
    if(strpbrk(s, "/\\")) result(s);
    else
    {
        defformatstring(str, "%s/%s", mapdirs[MAP_MAPZ].name, s);
        result(str);
    }
});

struct mapcinfo
{
    string file, fileext, title, author, desc;
    int type;
    bool samegame;
    mapz maphdr;

    mapcinfo() { reset(); }

    void reset()
    {
        file[0] = fileext[0] = author[0] = title[0] = desc[0] = 0;
        type = MAP_MAPZ;
        samegame = false;
    }

    static int compare(mapcinfo &a, mapcinfo &b)
    {
        if(a.type < b.type) return -1;
        if(a.type > b.type) return 1;
        return strcmp(a.title, b.title);
    }
};
vector<mapcinfo> mapcinfos;
vector<char *> failmapcs;

int scanmapc(const char *fname)
{
    if(!fname || !*fname) return -1;
    loopv(mapcinfos) if(!strcmp(mapcinfos[i].file, fname)) return i;
    loopv(failmapcs) if(!strcmp(failmapcs[i], fname)) return -1;
    int num = -1;
    stringz(msg);
    loop(format, MAP_MAX)
    {
        int mask = maskpackagedirs(format == MAP_OCTA ? ~0 : ~PACKAGEDIR_OCTA);

        stringz(dfile);
        stringz(dfileext);
        if(strpbrk(fname, "/\\")) copystring(dfile, fname);
        else formatstring(dfile, "%s/%s", mapdirs[format].name, fname);
        formatstring(dfileext, "%s%s", dfile, mapexts[format].name);
        loopv(mapcinfos) if(!strcmp(mapcinfos[i].file, dfile)) return i;
        loopv(failmapcs) if(!strcmp(failmapcs[i], dfile)) return -1;

        stream *f = opengzfile(dfileext, "rb");
        if(!f)
        {
            maskpackagedirs(mask);
            continue;
        }

        mapcinfo &d = mapcinfos.add();
        copystring(d.file, dfile);
        copystring(d.fileext, dfileext);

        if(f->read(&d.maphdr, sizeof(binary))!=(int)sizeof(binary))
        {
            formatstring(msg, "Error loading %s: malformatted universal header", d.fileext);
            delete f;
            maskpackagedirs(mask);
            mapcinfos.pop();
            break;
        }
        lilswap(&d.maphdr.version, 2);
        if(memcmp(d.maphdr.head, "MAPZ", 4) == 0 || memcmp(d.maphdr.head, "BFGZ", 4) == 0)
        {
            if(d.maphdr.version > MAPVERSION)
            {
                formatstring(msg, "Error loading %s: requires a newer version of %s (with map format version %d)", d.fileext, versionname, d.maphdr.version);
                delete f;
                maskpackagedirs(mask);
                mapcinfos.pop();
                break;
            }
            else if(d.maphdr.version <= 42)
            {
                conoutf("\frError loading %s: requires an older version of %s (with map format version %d)", d.fileext, versionname, d.maphdr.version);
                delete f;
                maskpackagedirs(mask);
                mapcinfos.pop();
                return false;
            }

            #define MAPZCOMPAT(ver) \
                mapzcompat##ver chdr; \
                memcpy(&chdr, &d.maphdr, sizeof(binary)); \
                if(f->read(&chdr.worldsize, sizeof(chdr)-sizeof(binary))!=sizeof(chdr)-sizeof(binary)) \
                { \
                    conoutf("\frerror loading %s: malformatted mapz v%d[%d] header", d.fileext, d.maphdr.version, ver); \
                    delete f; \
                    maskpackagedirs(mask); \
                    mapcinfos.pop(); \
                    return false; \
                }
            if(d.maphdr.version <= 43)
            {
                MAPZCOMPAT(43);
                lilswap(&chdr.worldsize, 8);
                memcpy(&d.maphdr.worldsize, &chdr.worldsize, sizeof(int)*3);
                memcpy(&d.maphdr.blendmap, &chdr.blendmap, sizeof(int)*4);
            }
            else
            {
                if(size_t(d.maphdr.headersize) > sizeof(d.maphdr) || f->read(&d.maphdr.worldsize, d.maphdr.headersize-sizeof(binary))!=size_t(d.maphdr.headersize)-sizeof(binary))
                {
                    conoutf("\frError loading %s: malformatted mapz v%d header", d.fileext, d.maphdr.version);
                    delete f;
                    maskpackagedirs(mask);
                    return false;
                }
                lilswap(&d.maphdr.worldsize, 7);
            }
            #undef MAPZCOMPAT

            d.type = MAP_MAPZ;
            int numvars = f->getlil<int>();
            loopi(numvars)
            {
                int len = f->getlil<int>();
                if(len)
                {
                    string name;
                    f->read(name, len+1);
                    int type = f->getlil<int>();
                    switch(type)
                    {
                        case ID_VAR:
                        {
                            f->getlil<int>();
                            break;
                        }
                        case ID_FVAR:
                        {
                            f->getlil<float>();
                            break;
                        }
                        case ID_SVAR:
                        {
                            int slen = f->getlil<int>();
                            if(slen >= 0)
                            {
                                char *val = newstring(slen);
                                f->read(val, slen+1);
                                if(slen)
                                {
                                    if(!strcmp(name, "maptitle")) copystring(d.title, val);
                                    else if(!strcmp(name, "mapauthor")) copystring(d.author, val);
                                    else if(!strcmp(name, "mapdesc")) copystring(d.desc, val);
                                }
                                delete[] val;
                            }
                            break;
                        }
                        default: break;
                    }
                }
            }
            if(!server::canload(d.maphdr.gameid)) d.samegame = false;
        }
        else if(memcmp(d.maphdr.head, "OCTA", 4) == 0)
        {
            octa ohdr;
            memcpy(&ohdr, &d.maphdr, sizeof(binary));

            if(ohdr.version != OCTAVERSION)
            {
                conoutf("\frError loading %s: requires %s version of Cube 2 support (version %d)", d.fileext, ohdr.version < OCTAVERSION ? "an older" : "a newer", ohdr.version);
                delete f;
                maskpackagedirs(mask);
                mapcinfos.pop();
                return false;
            }
            if(f->read(&ohdr.worldsize, sizeof(octa)-sizeof(binary))!=sizeof(octa)-(int)sizeof(binary))
            {
                conoutf("\frError loading %s: malformatted octa v%d header", d.fileext, ohdr.version);
                delete f;
                maskpackagedirs(mask);
                mapcinfos.pop();
                return false;
            }
            lilswap(&ohdr.worldsize, 7);

            d.type = MAP_OCTA;

            memcpy(d.maphdr.head, ohdr.head, 4);
            d.maphdr.gamever = 0; // sauer has no gamever
            d.maphdr.worldsize = ohdr.worldsize;
            if(d.maphdr.worldsize > 1<<18) d.maphdr.worldsize = 1<<18;
            d.maphdr.numents = ohdr.numents;
            d.maphdr.numpvs = ohdr.numpvs;
            d.maphdr.blendmap = ohdr.blendmap;
            d.maphdr.numvslots = ohdr.numvslots;
            d.maphdr.revision = 1;

            loopi(ohdr.numvars)
            {
                int type = f->getchar(), ilen = f->getlil<ushort>();
                string name;
                f->read(name, min(ilen, OCTASTRLEN-1));
                name[min(ilen, OCTASTRLEN-1)] = '\0';
                if(ilen >= OCTASTRLEN) f->seek(ilen - (OCTASTRLEN-1), SEEK_CUR);
                #if 0
                if(!strcmp(name, "cloudblend")) copystring(name, "cloudlayerblend");
                if(!strcmp(name, "cloudalpha")) copystring(name, "cloudblend");
                if(!strcmp(name, "grassalpha")) copystring(name, "grassblend");
                if(!strcmp(name, "skyboxcolour")) copystring(name, "skycolour");
                if(!strcmp(name, "cloudcolour")) copystring(name, "cloudlayercolour");
                if(!strcmp(name, "cloudboxcolour")) copystring(name, "cloudcolour");
                #endif
                switch(type)
                {
                    case ID_VAR:
                    {
                        f->getlil<int>();
                        break;
                    }

                    case ID_FVAR:
                    {
                        f->getlil<float>();
                        break;
                    }

                    case ID_SVAR:
                    {
                        int slen = f->getlil<ushort>();
                        string val;
                        f->read(val, min(slen, OCTASTRLEN-1));
                        val[min(slen, OCTASTRLEN-1)] = '\0';
                        if(slen >= OCTASTRLEN) f->seek(slen - (OCTASTRLEN-1), SEEK_CUR);
                        if(slen && !strcmp(name, "maptitle"))
                        {
                            string s;
                            filterstring(s, val);
                            const char *title = val, *author = strstr(title, " by ");
                            if(author && *author)
                            {
                                char *t = newstring(title, author-title);
                                if(t)
                                {
                                    if(*t)
                                    {
                                        loopi(4) if(*author) author++;
                                        copystring(d.title, t);
                                        copystring(d.author, author);
                                    }
                                    delete[] t;
                                }
                            }
                        }
                        break;
                    }
                }
            }

            string gameid;
            int len = f->getchar();
            f->read(gameid, len+1);
            memcpy(d.maphdr.gameid, gameid, 4);

            if(!server::canload(d.maphdr.gameid)) d.samegame = false;
        }
        else
        {
            delete f;
            maskpackagedirs(mask);
            mapcinfos.pop();
            continue;
        }
        delete f;
        maskpackagedirs(mask);
        stringz(s); // remove colour from these things in RE
        if(filterstring(s, d.title)) copystring(d.title, s);
        if(d.title[0])
        {
            const char *title = d.title, *author = strstr(title, " by ");
            if(author && *author)
            {
                char *t = newstring(title, author-title);
                if(t)
                {
                    if(*t)
                    {
                        loopi(4) if(*author) author++;
                        if(*author) copystring(d.author, author);
                        copystring(d.title, t);
                    }
                    delete[] t;
                }
            }
        }
        else copystring(d.title, fname);
        const char *fcrc = strstr(fname, "_0x");
        if(fcrc && *fcrc)
        {
            if(!strcmp(fname, d.title)) d.title[fcrc-fname] = 0;
            concformatstring(d.title, " (%s)", ++fcrc);
        }
        num = mapcinfos.length()-1;
        break;
    }
    if(num < 0)
    {
        failmapcs.add(newstring(fname));
        if(msg[0])
        {
            conoutft(CON_DEBUG, "%s", msg);
            return -1;
        }
    }
    return num;
}
ICOMMAND(0, mapcscan, "s", (char *name), intret(scanmapc(name)));

void resetmapcs(bool all)
{
    if(all) loopvrev(mapcinfos) mapcinfos.remove(i);
    loopvrev(failmapcs)
    {
        DELETEA(failmapcs[i]);
        failmapcs.remove(i);
    }
}
ICOMMAND(0, mapcreset, "i", (int *all), resetmapcs(*all!=0));
ICOMMAND(0, mapcsort, "", (), mapcinfos.sort(mapcinfo::compare));

void infomapc(int idx, int prop, int pt)
{
    if(idx < 0) intret(mapcinfos.length());
    else if(mapcinfos.inrange(idx))
    {
        mapcinfo &d = mapcinfos[idx];
        switch(prop)
        {
            case 0: result(d.file); break;
            case 1: result(d.fileext); break;
            case 2: result(d.title); break;
            case 3: result(d.author); break;
            case 4: result(d.desc); break;
            case 5:
            {
                switch(pt)
                {
                    case 0: intret(d.maphdr.version); break;
                    case 1: intret(d.maphdr.worldsize); break;
                    case 2: intret(d.maphdr.numents); break;
                    case 3: intret(d.maphdr.numpvs); break;
                    case 4: intret(d.maphdr.blendmap); break;
                    case 5: intret(d.maphdr.numvslots); break;
                    case 6: intret(d.maphdr.gamever); break;
                    case 7: intret(d.maphdr.revision); break;
                    case 8: result(d.maphdr.gameid); break;
                    case -1: intret(9); break;
                    default: result(""); break;
                }
                break;
            }
            case -1: intret(5); break;
            default: result(""); break;
        }
    }
}
ICOMMAND(0, mapcinfo, "bbb", (int *idx, int *prop, int *pt), infomapc(*idx, *prop, *pt));

char *mapctitle(const char *s)
{
    static string mapctitlestr;
    if(*s != '<')
    {
        int num = scanmapc(s);
        if(mapcinfos.inrange(num)) copystring(mapctitlestr, mapcinfos[num].title);
        else copystring(mapctitlestr, s);
        if(iscubelower(mapctitlestr[0])) mapctitlestr[0] = cubeupper(mapctitlestr[0]);
    }
    else copystring(mapctitlestr, s);
    return mapctitlestr;
}
ICOMMAND(0, getmaptitle, "s", (char *s), result(mapctitle(s)));

char *mapcauthor(const char *s)
{
    static string mapcauthorstr;
    if(*s != '<')
    {
        int num = scanmapc(s);
        if(mapcinfos.inrange(num)) copystring(mapcauthorstr, mapcinfos[num].author);
        else copystring(mapcauthorstr, "");
    }
    else copystring(mapcauthorstr, "");
    return mapcauthorstr;
}
ICOMMAND(0, getmapauthor, "s", (char *s), result(mapcauthor(s)));

char *mapcdesc(const char *s)
{
    static string mapcdescstr;
    if(*s != '<')
    {
        int num = scanmapc(s);
        if(mapcinfos.inrange(num)) copystring(mapcdescstr, mapcinfos[num].desc);
        else copystring(mapcdescstr, "");
    }
    else copystring(mapcdescstr, "");
    return mapcdescstr;
}
ICOMMAND(0, getmapdesc, "s", (char *s), result(mapcdesc(s)));

ICOMMAND(0, loopmapcs, "rrrre", (ident *id, ident *id2, ident *id3, ident *id4, uint *body),
{
    loopstart(id, stack);
    loopstart(id2, stack2);
    loopstart(id3, stack3);
    loopstart(id4, stack4);
    loopv(mapcinfos)
    {
        loopiter(id, stack, i);
        loopiter(id2, stack2, mapcinfos[i].file);
        loopiter(id3, stack3, mapcinfos[i].title);
        loopiter(id4, stack4, mapcinfos[i].desc);
        execute(body);
    }
    loopend(id, stack);
    loopend(id2, stack2);
    loopend(id3, stack3);
    loopend(id4, stack4);
});


struct sortmapc
{
    string name, title;

    sortmapc() { name[0] = title[0] = 0; }
    sortmapc(const char *_name) { copystring(name, _name); title[0] = 0; }

    static bool compare(sortmapc &a, sortmapc &b)
    {
        int alen = strlen(a.title), blen = strlen(b.title);
        loopi(alen)
        {
            if(i >= blen) return false;
            if(a.title[i] > b.title[i]) return false;
            if(a.title[i] < b.title[i]) return true;
        }
        return true;
    }
};

char *sortmaplist(const char *names)
{
    static bigstring result;
    result[0] = 0;
    if(!names || !*names) return result;
    vector<char *> mapnames;
    vector<sortmapc> maplist;
    explodelist(names, mapnames);
    loopv(mapnames) if(mapnames[i] && *mapnames[i])
    {
        sortmapc &s = maplist.add(sortmapc(mapnames[i]));
        int num = scanmapc(s.name);
        char *title = mapcinfos.inrange(num) ? mapcinfos[num].title : s.name;
        if(!strncasecmp(title, "A ", 2)) title += 2;
        if(!strncasecmp(title, "The ", 4)) title += 4;
        copystring(s.title, title);
        if(iscubelower(s.title[0])) s.title[0] = cubeupper(s.title[0]);
    }
    mapnames.deletearrays();
    maplist.sort(sortmapc::compare);
    loopv(maplist) concformatstring(result, "%s%s", result[0] ? " " : "", maplist[i].name);
    return result;
}
ICOMMAND(0, sortmaps, "s", (char *s), result(sortmaplist(s)));
