struct vertmodel : animmodel
{
    struct vert { vec pos, norm; vec4 tangent; };
    struct vvert { vec pos; hvec2 tc; squat tangent; };
    struct vvertg { hvec4 pos; hvec2 tc; squat tangent; };
    struct vvertgc : vvertg { bvec4 col; };
    struct tcvert { vec2 tc; };
    struct tri { ushort vert[3]; };

    struct vbocacheentry
    {
        GLuint vbuf;
        animstate as;
        int millis;

        vbocacheentry() : vbuf(0) { as.cur.fr1 = as.prev.fr1 = -1; }
    };

    struct vertmesh : mesh
    {
        vert *verts;
        tcvert *tcverts;
        tri *tris;
        bvec4 *vcolors;
        int numverts, numtris;

        int voffset, eoffset, elen;
        ushort minvert, maxvert;

        vertmesh() : verts(0), tcverts(0), tris(0), vcolors(0)
        {
        }

        virtual ~vertmesh()
        {
            DELETEA(verts);
            DELETEA(tcverts);
            DELETEA(tris);
            DELETEA(vcolors);
        }

        void smoothnorms(float limit = 0, bool areaweight = true)
        {
            if(((vertmeshgroup *)group)->numframes == 1) mesh::smoothnorms(verts, numverts, tris, numtris, limit, areaweight);
            else buildnorms(areaweight);
        }

        void buildnorms(bool areaweight = true)
        {
            mesh::buildnorms(verts, numverts, tris, numtris, areaweight, ((vertmeshgroup *)group)->numframes);
        }

        void calctangents(bool areaweight = true)
        {
            mesh::calctangents(verts, tcverts, numverts, tris, numtris, areaweight, ((vertmeshgroup *)group)->numframes);
        }

        void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m)
        {
            loopj(numverts)
            {
                vec v = m.transform(verts[j].pos);
                bbmin.min(v);
                bbmax.max(v);
            }
        }

        void genBIH(BIH::mesh &m)
        {
            m.tris = (const BIH::tri *)tris;
            m.numtris = numtris;
            m.pos = (const uchar *)&verts->pos;
            m.posstride = sizeof(vert);
            m.tc = (const uchar *)&tcverts->tc;
            m.tcstride = sizeof(tcvert);
        }

        void genshadowmesh(vector<triangle> &out, const matrix4x3 &m)
        {
            loopj(numtris)
            {
                triangle &t = out.add();
                t.a = m.transform(verts[tris[j].vert[0]].pos);
                t.b = m.transform(verts[tris[j].vert[1]].pos);
                t.c = m.transform(verts[tris[j].vert[2]].pos);
            }
        }

        static inline void assignvert(vvertg &vv, int j, tcvert &tc, vert &v)
        {
            vv.pos = hvec4(v.pos, 1);
            vv.tc = tc.tc;
            vv.tangent = v.tangent;
        }

        inline void assignvert(vvertgc &vv, int j, tcvert &tc, vert &v)
        {
            vv.pos = hvec4(v.pos, 1);
            vv.tc = tc.tc;
            vv.tangent = v.tangent;
            vv.col = vcolors ? vcolors[j] : bvec4(255, 255, 255, 255);
        }

        template<class T>
        int genvbo(vector<ushort> &idxs, int offset, vector<T> &vverts, int *htdata, int htlen)
        {
            voffset = offset;
            eoffset = idxs.length();
            minvert = 0xFFFF;
            loopi(numtris)
            {
                tri &t = tris[i];
                loopj(3)
                {
                    int index = t.vert[j];
                    vert &v = verts[index];
                    tcvert &tc = tcverts[index];
                    T vv;
                    assignvert(vv, index, tc, v);
                    int htidx = hthash(v.pos)&(htlen-1);
                    loopk(htlen)
                    {
                        int &vidx = htdata[(htidx+k)&(htlen-1)];
                        if(vidx < 0) { vidx = idxs.add(ushort(vverts.length())); vverts.add(vv); break; }
                        else if(!memcmp(&vverts[vidx], &vv, sizeof(T))) { minvert = min(minvert, idxs.add(ushort(vidx))); break; }
                    }
                }
            }
            minvert = min(minvert, ushort(voffset));
            maxvert = max(minvert, ushort(vverts.length()-1));
            elen = idxs.length()-eoffset;
            return vverts.length()-voffset;
        }

        int genvbo(vector<ushort> &idxs, int offset)
        {
            voffset = offset;
            eoffset = idxs.length();
            loopi(numtris)
            {
                tri &t = tris[i];
                loopj(3) idxs.add(voffset+t.vert[j]);
            }
            minvert = voffset;
            maxvert = voffset + numverts-1;
            elen = idxs.length()-eoffset;
            return numverts;
        }

        template<class T>
        static inline void fillvert(T &vv, int j, tcvert &tc, vert &v)
        {
            vv.tc = tc.tc;
        }

        template<class T>
        void fillverts(T *vdata)
        {
            vdata += voffset;
            loopi(numverts) fillvert(vdata[i], i, tcverts[i], verts[i]);
        }

        template<class T>
        void interpverts(const animstate &as, T * RESTRICT vdata, skin &s)
        {
            vdata += voffset;
            const vert * RESTRICT vert1 = &verts[as.cur.fr1 * numverts],
                       * RESTRICT vert2 = &verts[as.cur.fr2 * numverts],
                       * RESTRICT pvert1 = as.interp<1 ? &verts[as.prev.fr1 * numverts] : NULL,
                       * RESTRICT pvert2 = as.interp<1 ? &verts[as.prev.fr2 * numverts] : NULL;
            #define ipvert(attrib, type) v.attrib.lerp(vert1[i].attrib, vert2[i].attrib, as.cur.t)
            #define ipvertp(attrib, type) v.attrib.lerp(type().lerp(pvert1[i].attrib, pvert2[i].attrib, as.prev.t), type().lerp(vert1[i].attrib, vert2[i].attrib, as.cur.t), as.interp)
            if(as.interp<1) loopi(numverts) { T &v = vdata[i]; ipvertp(pos, vec); ipvertp(tangent, vec4); }
            else loopi(numverts) { T &v = vdata[i]; ipvert(pos, vec); ipvert(tangent, vec4); }
            #undef ipvert
            #undef ipvertp
        }

        void render(const animstate *as, skin &s, vbocacheentry &vc)
        {
            if(!Shader::lastshader) return;
            glDrawRangeElements_(GL_TRIANGLES, minvert, maxvert, elen, GL_UNSIGNED_SHORT, &((vertmeshgroup *)group)->edata[eoffset]);
            glde++;
            xtravertsva += numverts;
        }
    };

    struct tag
    {
        char *name;
        matrix4x3 matrix;

        tag() : name(NULL) {}
        ~tag() { DELETEA(name); }
    };

    struct vertmeshgroup : meshgroup
    {
        int numframes;
        tag *tags;
        int numtags;
        bool usecolor;

        static const int MAXVBOCACHE = 16;
        vbocacheentry vbocache[MAXVBOCACHE];

        ushort *edata;
        GLuint ebuf;
        int vlen, vertsize;
        uchar *vdata;

        vertmeshgroup() : numframes(0), tags(NULL), numtags(0), usecolor(false), edata(NULL), ebuf(0), vlen(0), vertsize(0), vdata(NULL)
        {
        }

        virtual ~vertmeshgroup()
        {
            DELETEA(tags);
            if(ebuf) glDeleteBuffers_(1, &ebuf);
            loopi(MAXVBOCACHE)
            {
                if(vbocache[i].vbuf) glDeleteBuffers_(1, &vbocache[i].vbuf);
            }
            DELETEA(vdata);
        }

        int findtag(const char *name)
        {
            loopi(numtags) if(!strcmp(tags[i].name, name)) return i;
            return -1;
        }

        bool addtag(const char *name, const matrix4x3 &matrix)
        {
            int idx = findtag(name);
            if(idx >= 0)
            {
                if(!testtags) return false;
                loopi(numframes)
                {
                    tag &t = tags[i*numtags + idx];
                    t.matrix = matrix;
                }
            }
            else
            {
                tag *newtags = new tag[(numtags+1)*numframes];
                loopi(numframes)
                {
                    tag *dst = &newtags[(numtags+1)*i], *src = &tags[numtags*i];
                    if(!i)
                    {
                        loopj(numtags) swap(dst[j].name, src[j].name);
                        dst[numtags].name = newstring(name);
                    }
                    loopj(numtags) dst[j].matrix = src[j].matrix;
                    dst[numtags].matrix = matrix;
                }
                if(tags) delete[] tags;
                tags = newtags;
                numtags++;
            }
            return true;
        }

        int totalframes() const { return numframes; }

        void concattagtransform(part *p, int i, const matrix4x3 &m, matrix4x3 &n)
        {
            n.mul(m, tags[i].matrix);
        }

        void calctagmatrix(part *p, int i, const animstate &as, matrix4 &matrix)
        {
            const matrix4x3 &tag1 = tags[as.cur.fr1*numtags + i].matrix,
                            &tag2 = tags[as.cur.fr2*numtags + i].matrix;
            matrix4x3 tag;
            tag.lerp(tag1, tag2, as.cur.t);
            if(as.interp<1)
            {
                const matrix4x3 &tag1p = tags[as.prev.fr1*numtags + i].matrix,
                                &tag2p = tags[as.prev.fr2*numtags + i].matrix;
                matrix4x3 tagp;
                tagp.lerp(tag1p, tag2p, as.prev.t);
                tag.lerp(tagp, tag, as.interp);
            }
            tag.d.mul(p->model->scale * sizescale);
            matrix = matrix4(tag);
        }

        void genvbo(vbocacheentry &vc)
        {
            if(!vc.vbuf) glGenBuffers_(1, &vc.vbuf);
            if(ebuf) return;

            vector<ushort> idxs;

            vlen = 0;
            if(numframes>1)
            {
                vertsize = sizeof(vvert);
                looprendermeshes(vertmesh, m, vlen += m.genvbo(idxs, vlen));
                DELETEA(vdata);
                vdata = new uchar[vlen*vertsize];
                looprendermeshes(vertmesh, m,
                {
                    m.fillverts((vvert *)vdata);
                });
            }
            else
            {
                looprendermeshes(vertmesh, m, { if(m.vcolors) { usecolor = true; break; }});
                gle::bindvbo(vc.vbuf);
                #define GENVBO(type) \
                    do \
                    { \
                        vertsize = sizeof(type); \
                        vector<type> vverts; \
                        looprendermeshes(vertmesh, m, vlen += m.genvbo(idxs, vlen, vverts, htdata, htlen)); \
                        glBufferData_(GL_ARRAY_BUFFER, vverts.length()*sizeof(type), vverts.getbuf(), GL_STATIC_DRAW); \
                    } while(0)
                int numverts = 0, htlen = 128;
                looprendermeshes(vertmesh, m, numverts += m.numverts);
                while(htlen < numverts) htlen *= 2;
                if(numverts*4 > htlen*3) htlen *= 2;
                int *htdata = new int[htlen];
                memset(htdata, -1, htlen*sizeof(int));
                if(usecolor) GENVBO(vvertgc);
                else GENVBO(vvertg);
                delete[] htdata;
                #undef GENVBO
                gle::clearvbo();
            }

            glGenBuffers_(1, &ebuf);
            gle::bindebo(ebuf);
            glBufferData_(GL_ELEMENT_ARRAY_BUFFER, idxs.length()*sizeof(ushort), idxs.getbuf(), GL_STATIC_DRAW);
            gle::clearebo();
        }

        template<class T>
        void bindcolor(T *vverts) { if(enablecolor) disablecolor(); }
        void bindcolor(vvertgc *vverts) { meshgroup::bindcolor(&vverts->col, vertsize); }

        template<class T>
        void bindvbo(const animstate *as, part *p, vbocacheentry &vc)
        {
            T *vverts = 0;
            bindpos(ebuf, vc.vbuf, &vverts->pos, vertsize);
            if(as->cur.anim&ANIM_NOSKIN)
            {
                if(enabletangents) disabletangents();

                if(p->alphatested()) bindtc(&vverts->tc, vertsize);
                else if(enabletc) disabletc();
            }
            else
            {
                bindtangents(&vverts->tangent, vertsize);

                bindtc(&vverts->tc, vertsize);
            }
            bindcolor(vverts);
            if(enablebones) disablebones();
        }

        void bindvbo(const animstate *as, part *p, vbocacheentry &vc)
        {
            if(numframes>1) bindvbo<vvert>(as, p, vc);
            if(usecolor) bindvbo<vvertgc>(as, p, vc);
            else bindvbo<vvertg>(as, p, vc);
        }

        void cleanup()
        {
            loopi(MAXVBOCACHE)
            {
                vbocacheentry &c = vbocache[i];
                if(c.vbuf) { glDeleteBuffers_(1, &c.vbuf); c.vbuf = 0; }
                c.as.cur.fr1 = -1;
            }
            if(ebuf) { glDeleteBuffers_(1, &ebuf); ebuf = 0; }
        }

        void preload(part *p)
        {
            if(numframes > 1) return;
            if(!vbocache->vbuf) genvbo(*vbocache);
        }

        void render(const animstate *as, float pitch, const vec &axis, const vec &forward, modelstate *state, dynent *d, part *p)
        {
            if(as->cur.anim&ANIM_NORENDER)
            {
                loopv(p->links) calctagmatrix(p, p->links[i].tag, *as, p->links[i].matrix);
                return;
            }

            vbocacheentry *vc = NULL;
            if(numframes<=1) vc = vbocache;
            else
            {
                loopi(MAXVBOCACHE)
                {
                    vbocacheentry &c = vbocache[i];
                    if(!c.vbuf) continue;
                    if(c.as==*as) { vc = &c; break; }
                }
                if(!vc) loopi(MAXVBOCACHE) { vc = &vbocache[i]; if(!vc->vbuf || vc->millis < lastmillis) break; }
            }
            if(!vc->vbuf) genvbo(*vc);
            if(numframes>1)
            {
                if(vc->as!=*as)
                {
                    vc->as = *as;
                    vc->millis = lastmillis;
                    looprendermeshes(vertmesh, m,
                    {
                        m.interpverts(*as, (vvert *)vdata, p->skins[i]);
                    });
                    gle::bindvbo(vc->vbuf);
                    glBufferData_(GL_ARRAY_BUFFER, vlen*vertsize, vdata, GL_STREAM_DRAW);
                }
                vc->millis = lastmillis;
            }

            bindvbo(as, p, *vc);

            looprendermeshes(vertmesh, m,
            {
                p->skins[i].bind(m, as, state);
                m.render(as, p->skins[i], *vc);
            });

            loopv(p->links) calctagmatrix(p, p->links[i].tag, *as, p->links[i].matrix);
        }

        virtual bool load(const char *name, float smooth) = 0;
    };

    virtual vertmeshgroup *newmeshes() = 0;

    meshgroup *loadmeshes(const char *name, float smooth = 2)
    {
        vertmeshgroup *group = newmeshes();
        if(!group->load(name, smooth)) { delete group; return NULL; }
        return group;
    }

    meshgroup *sharemeshes(const char *name, float smooth = 2)
    {
        if(!meshgroups.access(name))
        {
            meshgroup *group = loadmeshes(name, smooth);
            if(!group) return NULL;
            meshgroups.add(group);
        }
        return meshgroups[name];
    }

    vertmodel(const char *name) : animmodel(name)
    {
    }
};

template<class MDL> struct vertloader : modelloader<MDL, vertmodel>
{
    vertloader(const char *name) : modelloader<MDL, vertmodel>(name) {}
};

template<class MDL> struct vertcommands : modelcommands<MDL, struct MDL::vertmesh>
{
    typedef struct MDL::vertmeshgroup meshgroup;
    typedef struct MDL::part part;
    typedef struct MDL::skin skin;

    static void loadpart(char *model, float *smooth)
    {
        if(!MDL::loading) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        defformatstring(filename, "%s/%s", MDL::dir, model);
        part &mdl = MDL::loading->addpart();
        if(mdl.index) mdl.disablepitch();
        mdl.meshes = MDL::loading->sharemeshes(path(filename), *smooth > 0 ? cosf(clamp(*smooth, 0.0f, 180.0f)*RAD) : 2);
        if(!mdl.meshes) conoutf("\frCould not load %s", filename);
        else mdl.initskins();
    }

    static void settag(char *tagname, float *tx, float *ty, float *tz, float *rx, float *ry, float *rz)
    {
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        part &mdl = *(part *)MDL::loading->parts.last();
        float cx = *rx ? cosf(*rx/2*RAD) : 1, sx = *rx ? sinf(*rx/2*RAD) : 0,
              cy = *ry ? cosf(*ry/2*RAD) : 1, sy = *ry ? sinf(*ry/2*RAD) : 0,
              cz = *rz ? cosf(*rz/2*RAD) : 1, sz = *rz ? sinf(*rz/2*RAD) : 0;
        matrix4x3 m(matrix3(quat(sx*cy*cz - cx*sy*sz, cx*sy*cz + sx*cy*sz, cx*cy*sz - sx*sy*cz, cx*cy*cz + sx*sy*sz)),
                    vec(*tx, *ty, *tz));
        ((meshgroup *)mdl.meshes)->addtag(tagname, m);
    }

    static void setpitch(float *pitchscale, float *pitchoffset, float *pitchmin, float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        part &mdl = *MDL::loading->parts.last();

        mdl.pitchscale = *pitchscale;
        mdl.pitchoffset = *pitchoffset;
        if(*pitchmin || *pitchmax)
        {
            mdl.pitchmin = *pitchmin;
            mdl.pitchmax = *pitchmax;
        }
        else
        {
            mdl.pitchmin = -360*fabs(mdl.pitchscale) + mdl.pitchoffset;
            mdl.pitchmax = 360*fabs(mdl.pitchscale) + mdl.pitchoffset;
        }
    }

    static void setanim(char *anim, int *frame, int *range, float *speed, int *priority)
    {
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        vector<int> anims;
        game::findanims(anim, anims);
        if(anims.empty()) conoutf("\frCould not find animation %s", anim);
        else loopv(anims)
        {
            MDL::loading->parts.last()->setanim(0, anims[i], *frame, *range, *speed, *priority);
        }
    }

    vertcommands()
    {
        if(MDL::multiparted()) this->modelcommand(loadpart, "load", "sf");
        this->modelcommand(settag, "tag", "sffffff");
        this->modelcommand(setpitch, "pitch", "ffff");
        if(MDL::cananimate()) this->modelcommand(setanim, "anim", "siiff");
    }
};

