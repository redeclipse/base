struct vertmodel : animmodel
{
    struct vert { vec pos, norm; };
    struct vvert { vec pos; vec2 tc; vec norm; };
    struct vvertbump : vvert { vec tangent; float bitangent; };
    struct tcvert { vec2 tc; };
    struct bumpvert { vec tangent; float bitangent; };
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
        bumpvert *bumpverts;
        tri *tris;
        int numverts, numtris;

        int voffset, eoffset, elen;
        ushort minvert, maxvert;

        vertmesh() : verts(0), tcverts(0), bumpverts(0), tris(0)
        {
        }

        virtual ~vertmesh()
        {
            DELETEA(verts);
            DELETEA(tcverts);
            DELETEA(bumpverts);
            DELETEA(tris);
        }

        void smoothnorms(float limit = 0, bool areaweight = true)
        {
            if(((vertmeshgroup *)group)->numframes != 1)
            {
                buildnorms(areaweight);
                return;
            }
            mesh::smoothnorms(verts, numverts, tris, numtris, limit, areaweight);
        }

        void buildnorms(bool areaweight = true)
        {
            loopk(((vertmeshgroup *)group)->numframes)
                mesh::buildnorms(&verts[k*numverts], numverts, tris, numtris, areaweight); 
        }

        void calctangents(bool areaweight = true)
        {
            if(bumpverts) return;
            bumpverts = new bumpvert[((vertmeshgroup *)group)->numframes*numverts];
            loopk(((vertmeshgroup *)group)->numframes)
                mesh::calctangents(&bumpverts[k*numverts], &verts[k*numverts], tcverts, numverts, tris, numtris, areaweight);
        }

        void calcbb(vec &bbmin, vec &bbmax, const matrix3x4 &m)
        {
            loopj(numverts)
            {
                vec v = m.transform(verts[j].pos);
                loopi(3)
                {
                    bbmin[i] = min(bbmin[i], v[i]);
                    bbmax[i] = max(bbmax[i], v[i]);
                }
            }
        }

        void gentris(Texture *tex, vector<BIH::tri> *out, const matrix3x4 &m)
        {
            loopj(numtris)
            {
                BIH::tri &t = out[noclip ? 1 : 0].add();
                t.tex = tex;
                t.a = m.transform(verts[tris[j].vert[0]].pos);
                t.b = m.transform(verts[tris[j].vert[1]].pos);
                t.c = m.transform(verts[tris[j].vert[2]].pos);
                tcvert &av = tcverts[tris[j].vert[0]],
                       &bv = tcverts[tris[j].vert[1]],
                       &cv = tcverts[tris[j].vert[2]];
                t.tc[0] = av.tc;
                t.tc[1] = bv.tc;
                t.tc[2] = cv.tc;
            }
        }

        static inline bool comparevert(vvert &w, int j, tcvert &tc, vert &v)
        {
            return tc.tc==w.tc && v.pos==w.pos && v.norm==w.norm;
        }

        inline bool comparevert(vvertbump &w, int j, tcvert &tc, vert &v)
        {
            return tc.tc==w.tc && v.pos==w.pos && v.norm==w.norm && (!bumpverts || (bumpverts[j].tangent==w.tangent && bumpverts[j].bitangent==w.bitangent));
        }

        static inline void assignvert(vvert &vv, int j, tcvert &tc, vert &v)
        {
            vv.pos = v.pos;
            vv.norm = v.norm;
            vv.tc = tc.tc;
        }

        inline void assignvert(vvertbump &vv, int j, tcvert &tc, vert &v)
        {
            vv.pos = v.pos;
            vv.norm = v.norm;
            vv.tc = tc.tc;
            if(bumpverts)
            {
                vv.tangent = bumpverts[j].tangent;
                vv.bitangent = bumpverts[j].bitangent;
            }
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
                    tcvert &tc = tcverts[index];
                    vert &v = verts[index];
                    int htidx = hthash(v.pos)&(htlen-1);
                    loopk(htlen)
                    {
                        int &vidx = htdata[(htidx+k)&(htlen-1)];
                        if(vidx < 0) { vidx = idxs.add(ushort(vverts.length())); assignvert(vverts.add(), index, tc, v); break; }
                        else if(comparevert(vverts[vidx], index, tc, v)) { minvert = min(minvert, idxs.add(ushort(vidx))); break; }
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

        void filltc(uchar *vdata, size_t stride)
        {
            vdata = (uchar *)((vvert *)&vdata[voffset*stride])->tc.v;
            loopi(numverts)
            {
                *(tcvert *)vdata = tcverts[i];
                vdata += stride;
            }
        }

        void interpverts(const animstate &as, bool tangents, void * RESTRICT vdata, skin &s)
        {
            const vert * RESTRICT vert1 = &verts[as.cur.fr1 * numverts],
                       * RESTRICT vert2 = &verts[as.cur.fr2 * numverts],
                       * RESTRICT pvert1 = as.interp<1 ? &verts[as.prev.fr1 * numverts] : NULL, 
                       * RESTRICT pvert2 = as.interp<1 ? &verts[as.prev.fr2 * numverts] : NULL;
            #define ipvert(attrib)   v.attrib.lerp(vert1[i].attrib, vert2[i].attrib, as.cur.t)
            #define ipbvert(attrib)  v.attrib.lerp(bvert1[i].attrib, bvert2[i].attrib, as.cur.t)
            #define ipvertp(attrib)  v.attrib.lerp(pvert1[i].attrib, pvert2[i].attrib, as.prev.t).lerp(vec().lerp(vert1[i].attrib, vert2[i].attrib, as.cur.t), as.interp)
            #define ipbvertp(attrib) v.attrib.lerp(bpvert1[i].attrib, bpvert2[i].attrib, as.prev.t).lerp(vec().lerp(bvert1[i].attrib, bvert2[i].attrib, as.cur.t), as.interp)
            #define iploop(type, body) \
                loopi(numverts) \
                { \
                    type &v = ((type * RESTRICT)vdata)[i]; \
                    body; \
                }
            if(tangents)
            {
                const bumpvert * RESTRICT bvert1 = &bumpverts[as.cur.fr1 * numverts],
                               * RESTRICT bvert2 = &bumpverts[as.cur.fr2 * numverts],
                               * RESTRICT bpvert1 = as.interp<1 ? &bumpverts[as.prev.fr1 * numverts] : NULL, 
                               * RESTRICT bpvert2 = as.interp<1 ? &bumpverts[as.prev.fr2 * numverts] : NULL;
                if(as.interp<1) iploop(vvertbump, { ipvertp(pos); ipvertp(norm); ipbvertp(tangent); v.bitangent = bvert1[i].bitangent; })
                else iploop(vvertbump, { ipvert(pos); ipvert(norm); ipbvert(tangent); v.bitangent = bvert1[i].bitangent; })
            }
            else
            {
                if(as.interp<1) iploop(vvert, { ipvertp(pos); ipvertp(norm); })
                else iploop(vvert, { ipvert(pos); ipvert(norm); })
            }
            #undef iploop
            #undef ipvert
            #undef ipbvert
            #undef ipvertp
            #undef ipbvertp
        }

        void render(const animstate *as, skin &s, vbocacheentry &vc)
        {
            if(!(as->cur.anim&ANIM_NOSKIN))
            {
                if(s.tangents())
                {
                    if(!enabletangents || lastxbuf!=lastvbuf)
                    {
                        if(!enabletangents) glEnableVertexAttribArray_(1);
                        if(lastxbuf!=lastvbuf)
                        {
                            vvertbump *vverts = 0;
                            glVertexAttribPointer_(1, 4, GL_FLOAT, GL_FALSE, ((vertmeshgroup *)group)->vertsize, &vverts->tangent.x);
                        }
                        lastxbuf = lastvbuf;
                        enabletangents = true;
                    }
                }
                else if(enabletangents) disabletangents();
            }

            glDrawRangeElements_(GL_TRIANGLES, minvert, maxvert, elen, GL_UNSIGNED_SHORT, &((vertmeshgroup *)group)->edata[eoffset]);
            glde++;
            xtravertsva += numverts;
        }
    };

    struct tag
    {
        char *name;
        matrix3x4 transform;

        tag() : name(NULL) {}
        ~tag() { DELETEA(name); }
    };

    struct vertmeshgroup : meshgroup
    {
        int numframes;
        tag *tags;
        int numtags;

        static const int MAXVBOCACHE = 16;
        vbocacheentry vbocache[MAXVBOCACHE];

        ushort *edata;
        GLuint ebuf;
        bool vtangents;
        int vlen, vertsize;
        uchar *vdata;

        vertmeshgroup() : numframes(0), tags(NULL), numtags(0), edata(NULL), ebuf(0), vtangents(false), vlen(0), vertsize(0), vdata(NULL) 
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

        int totalframes() const { return numframes; }

        void concattagtransform(part *p, int i, const matrix3x4 &m, matrix3x4 &n)
        {
            n.mul(m, tags[numtags + i].transform);
            n.translate(m.transformnormal(p->translate).mul(p->model->scale));
        }

        void calctagmatrix(part *p, int i, const animstate &as, modelattach *attached, glmatrixf &matrix)
        {
            const matrix3x4 &tag1 = tags[as.cur.fr1*numtags + i].transform,
                            &tag2 = tags[as.cur.fr2*numtags + i].transform;
            matrix3x4 tag;
            tag.lerp(tag1, tag2, as.cur.t);
            if(as.interp<1)
            {
                const matrix3x4 &tag1p = tags[as.prev.fr1*numtags + i].transform,
                                &tag2p = tags[as.prev.fr2*numtags + i].transform;
                matrix3x4 tagp;
                tagp.lerp(tag1p, tag2p, as.prev.t);
                tag.lerp(tagp, tag, as.interp);
            }
            float resize = p->model->scale * (attached && attached->sizescale >= 0 ? attached->sizescale : sizescale);
            matrix = glmatrixf(tag);
            matrix[12] = (matrix[12] + p->translate.x) * resize;
            matrix[13] = (matrix[13] + p->translate.y) * resize;
            matrix[14] = (matrix[14] + p->translate.z) * resize;
        }

        void genvbo(bool tangents, vbocacheentry &vc)
        {
            if(!vc.vbuf) glGenBuffers_(1, &vc.vbuf);
            if(ebuf) return;
                
            #define ALLOCVDATA(vdata) \
                do \
                { \
                    DELETEA(vdata); \
                    vdata = new uchar[vlen*vertsize]; \
                    loopv(meshes) ((vertmesh *)meshes[i])->filltc(vdata, vertsize); \
                } while(0)

            vector<ushort> idxs;

            vtangents = tangents;
            vertsize = tangents ? sizeof(vvertbump) : sizeof(vvert);
            vlen = 0;
            if(numframes>1)
            {
                loopv(meshes) vlen += ((vertmesh *)meshes[i])->genvbo(idxs, vlen);
                DELETEA(vdata);
                ALLOCVDATA(vdata); 
            } 
            else 
            {
                glBindBuffer_(GL_ARRAY_BUFFER_ARB, vc.vbuf);
                #define GENVBO(type) \
                    do \
                    { \
                        vector<type> vverts; \
                        loopv(meshes) vlen += ((vertmesh *)meshes[i])->genvbo(idxs, vlen, vverts, htdata, htlen); \
                        glBufferData_(GL_ARRAY_BUFFER_ARB, vverts.length()*sizeof(type), vverts.getbuf(), GL_STATIC_DRAW_ARB); \
                    } while(0)
                int numverts = 0, htlen = 128;
                loopv(meshes) numverts += ((vertmesh *)meshes[i])->numverts;
                while(htlen < numverts) htlen *= 2;
                if(numverts*4 > htlen*3) htlen *= 2;  
                int *htdata = new int[htlen];
                memset(htdata, -1, htlen*sizeof(int));
                if(tangents) GENVBO(vvertbump);
                else GENVBO(vvert);
                delete[] htdata;
                glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
            }

            glGenBuffers_(1, &ebuf);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, ebuf);
            glBufferData_(GL_ELEMENT_ARRAY_BUFFER_ARB, idxs.length()*sizeof(ushort), idxs.getbuf(), GL_STATIC_DRAW_ARB);
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
            #undef GENVBO
            #undef ALLOCVDATA
        }

        void bindvbo(const animstate *as, vbocacheentry &vc)
        {
            vvert *vverts = 0;
            if(lastebuf!=ebuf)
            {
                glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, ebuf);
                lastebuf = ebuf;
            }
            if(lastvbuf != (void *)(size_t)vc.vbuf)
            {
                glBindBuffer_(GL_ARRAY_BUFFER_ARB, vc.vbuf);
                if(!lastvbuf) glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(3, GL_FLOAT, vertsize, &vverts->pos);
                lastvbuf = (void *)(size_t)vc.vbuf;
            }
            if(as->cur.anim&ANIM_NOSKIN)
            {
                if(enabletc) disabletc();
                if(enablenormals) disablenormals();
            }
            else
            {
                if(!enablenormals) 
                {
                    glEnableClientState(GL_NORMAL_ARRAY);
                    enablenormals = true;
                }
                if(lastnbuf!=lastvbuf) 
                {
                    glNormalPointer(GL_FLOAT, vertsize, vverts->norm.v);
                    lastnbuf = lastvbuf;
                }

                if(!enabletc) 
                {
                    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    enabletc = true;
                }
                if(lasttcbuf!=lastvbuf) 
                {
                    glTexCoordPointer(2, GL_FLOAT, vertsize, vverts->tc.v);
                    lasttcbuf = lastvbuf;
                }
            }
            if(enablebones) disablebones();
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
            bool tangents = false;
            loopv(p->skins) if(p->skins[i].tangents()) tangents = true;
            if(tangents!=vtangents) cleanup();
            if(!vbocache->vbuf) genvbo(tangents, *vbocache);
        }

        void render(const animstate *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p, modelattach *attached)
        {
            if(as->cur.anim&ANIM_NORENDER)
            {
                loopv(p->links) calctagmatrix(p, p->links[i].tag, *as, attached, p->links[i].matrix);
                return;
            }

            bool tangents = false;
            loopv(p->skins) if(p->skins[i].tangents()) tangents = true;
            if(tangents!=vtangents) { cleanup(); disablevbo(); }
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
            if(!vc->vbuf) genvbo(tangents, *vc);
            if(numframes>1)
            {
                if(vc->as!=*as)
                {
                    vc->as = *as;
                    vc->millis = lastmillis;
                    loopv(meshes) 
                    {
                        vertmesh &m = *(vertmesh *)meshes[i];
                        m.interpverts(*as, tangents, vdata + m.voffset*vertsize, p->skins[i]);
                    }
                    glBindBuffer_(GL_ARRAY_BUFFER_ARB, vc->vbuf);
                    glBufferData_(GL_ARRAY_BUFFER_ARB, vlen*vertsize, vdata, GL_STREAM_DRAW_ARB); 
                }
                vc->millis = lastmillis;
            }
        
            bindvbo(as, *vc);
            loopv(meshes)
            {
                vertmesh *m = (vertmesh *)meshes[i];
                p->skins[i].bind(m, as, attached);
                m->render(as, p->skins[i], *vc);
            }
            
            loopv(p->links) calctagmatrix(p, p->links[i].tag, *as, attached, p->links[i].matrix);
        }
    };

    vertmodel(const char *name) : animmodel(name)
    {
    }
};

template<class MDL> struct vertloader : modelloader<MDL>
{
};

template<class MDL> struct vertcommands : modelcommands<MDL, struct MDL::vertmesh>
{
    typedef struct MDL::part part;
    typedef struct MDL::skin skin;

    static void loadpart(char *model, float *smooth)
    {
        if(!MDL::loading) { conoutf("\frnot loading an %s", MDL::formatname()); return; }
        defformatstring(filename)("%s/%s", MDL::dir, model);
        part &mdl = *new part;
        MDL::loading->parts.add(&mdl);
        mdl.model = MDL::loading;
        mdl.index = MDL::loading->parts.length()-1;
        if(mdl.index) mdl.pitchscale = mdl.pitchoffset = mdl.pitchmin = mdl.pitchmax = 0;
        mdl.meshes = MDL::loading->sharemeshes(path(filename), double(*smooth > 0 ? cos(clamp(*smooth, 0.0f, 180.0f)*RAD) : 2));
        if(!mdl.meshes) conoutf("\frcould not load %s", filename);
        else mdl.initskins();
    }
    
    static void setpitch(float *pitchscale, float *pitchoffset, float *pitchmin, float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frnot loading an %s", MDL::formatname()); return; }
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
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frnot loading an %s", MDL::formatname()); return; }
        vector<int> anims;
        game::findanims(anim, anims);
        if(anims.empty()) conoutf("\frcould not find animation %s", anim);
        else loopv(anims)
        {
            MDL::loading->parts.last()->setanim(0, anims[i], *frame, *range, *speed, *priority);
        }
    }

    vertcommands()
    {
        if(MDL::multiparted()) this->modelcommand(loadpart, "load", "sf"); 
        this->modelcommand(setpitch, "pitch", "ffff");
        if(MDL::animated()) this->modelcommand(setanim, "anim", "siiff");
    }
};

