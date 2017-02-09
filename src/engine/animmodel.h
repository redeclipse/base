VARF(IDF_PERSIST, envmapmodels, 0, 1, 1, preloadmodelshaders(true));
VARF(IDF_PERSIST, bumpmodels, 0, 1, 1, preloadmodelshaders(true));
VAR(IDF_PERSIST, fullbrightmodels, 0, 0, 200);

struct animmodel : model
{
    struct animspec
    {
        int frame, range;
        float speed;
        int priority;
    };

    struct animpos
    {
        int anim, fr1, fr2;
        float t;

        void setframes(const animinfo &info)
        {
            anim = info.anim;
            if(info.range<=1)
            {
                fr1 = 0;
                t = 0;
            }
            else
            {
                int time = info.anim&ANIM_SETTIME ? info.basetime : lastmillis-info.basetime;
                fr1 = (int)(time/info.speed); // round to full frames
                t = (time-fr1*info.speed)/info.speed; // progress of the frame, value from 0.0f to 1.0f
            }
            if(info.anim&ANIM_LOOP)
            {
                fr1 = fr1%info.range+info.frame;
                fr2 = fr1+1;
                if(fr2>=info.frame+info.range) fr2 = info.frame;
            }
            else
            {
                fr1 = min(fr1, info.range-1)+info.frame;
                fr2 = min(fr1+1, info.frame+info.range-1);
            }
            if(info.anim&ANIM_REVERSE)
            {
                fr1 = (info.frame+info.range-1)-(fr1-info.frame);
                fr2 = (info.frame+info.range-1)-(fr2-info.frame);
            }
        }

        bool operator==(const animpos &a) const { return fr1==a.fr1 && fr2==a.fr2 && (fr1==fr2 || t==a.t); }
        bool operator!=(const animpos &a) const { return fr1!=a.fr1 || fr2!=a.fr2 || (fr1!=fr2 && t!=a.t); }
    };

    struct part;

    struct animstate
    {
        part *owner;
        animpos cur, prev;
        float interp;

        bool operator==(const animstate &a) const { return cur==a.cur && (interp<1 ? interp==a.interp && prev==a.prev : a.interp>=1); }
        bool operator!=(const animstate &a) const { return cur!=a.cur || (interp<1 ? interp!=a.interp || prev!=a.prev : a.interp<1); }
    };

    struct linkedpart;
    struct mesh;

    struct shaderparams
    {
        float spec, ambient, glow, glowdelta, glowpulse, specglare, glowglare, fullbright, envmapmin, envmapmax, scrollu, scrollv, alphatest;
        int material, material2;

        shaderparams() : spec(1.0f), ambient(0.3f), glow(3.0f), glowdelta(0), glowpulse(0), specglare(1), glowglare(1), fullbright(0), envmapmin(0), envmapmax(0), scrollu(0), scrollv(0), alphatest(0.9f), material(1), material2(0) {}
    };

    struct shaderparamskey
    {
        static hashtable<shaderparams, shaderparamskey> keys;
        static int firstversion, lastversion;

        int version;

        shaderparamskey() : version(-1) {}

        bool checkversion()
        {
            if(version >= firstversion) return true;
            version = lastversion;
            if(++lastversion <= 0)
            {
                enumerate(keys, shaderparamskey, key, key.version = -1);
                firstversion = 0;
                lastversion = 1;
                version = 0;
            }
            return false;
        }

        static inline void invalidate()
        {
            firstversion = lastversion;
        }
    };

    struct skin : shaderparams
    {
        part *owner;
        Texture *tex, *masks, *envmap, *normalmap;
        Shader *shader;
        bool alphablend, cullface;
        shaderparamskey *key;

        skin() : owner(0), tex(notexture), masks(notexture), envmap(NULL), normalmap(NULL), shader(NULL), alphablend(true), cullface(true), key(NULL) {}

        bool masked() const { return masks != notexture; }
        bool envmapped() { return envmapmax>0 && envmapmodels; }
        bool bumpmapped() { return normalmap && bumpmodels; }
        bool tangents() { return bumpmapped(); }
        bool alphatested() const { return alphatest > 0 && tex->type&Texture::ALPHA; }

        void setkey()
        {
            key = &shaderparamskey::keys[*this];
        }

        void setshaderparams(mesh *m, const animstate *as, float trans)
        {
            float mincolor = as->cur.anim&ANIM_FULLBRIGHT ? fullbrightmodels/100.0f : 0.0f;
            if(fullbright)
            {
                gle::colorf(fullbright/2, fullbright/2, fullbright/2, trans);
            }
            else
            {
                gle::color(vec(lightcolor).max(mincolor), trans);
            }

            if(material > 0 && lightmaterial) LOCALPARAM(lightmaterial, lightmaterial[min(material, int(MAXLIGHTMATERIALS))-1].tocolor().mul(2));
            if(material2 > 0 && lightmaterial) LOCALPARAM(lightmaterial2, lightmaterial[min(material2, int(MAXLIGHTMATERIALS))-1].tocolor().mul(2));

            if(key->checkversion() && Shader::lastshader->owner == key) return;
            Shader::lastshader->owner = key;

            if(alphatested()) LOCALPARAMF(alphatest, alphatest);

            if(fullbright)
            {
                LOCALPARAMF(lightscale, 0, 0, 2);
            }
            else
            {
                float bias = max(mincolor-1.0f, 0.2f), scale = 0.5f*max(0.8f-bias, 0.0f),
                      minshade = scale*max(ambient, mincolor);
                LOCALPARAMF(lightscale, scale - minshade, scale, minshade + bias);
            }
            float curglow = glow;
            if(glowpulse > 0)
            {
                float curpulse = lastmillis*glowpulse;
                curpulse -= floor(curpulse);
                curglow += glowdelta*2*fabs(curpulse - 0.5f);
            }
            if(material <= 0 || !lightmaterial) LOCALPARAMF(lightmaterial, 2, 2, 2);
            if(material2 <= 0 || !lightmaterial) LOCALPARAMF(lightmaterial2, 2, 2, 2);
            LOCALPARAMF(maskscale, 0.5f*spec, 0.5f*curglow, 16*specglare, 4*glowglare);
            LOCALPARAMF(texscroll, scrollu*lastmillis/1000.0f, scrollv*lastmillis/1000.0f);
            if(envmapped()) LOCALPARAMF(envmapscale, envmapmin-envmapmax, envmapmax);
        }

        Shader *loadshader()
        {
            #define DOMODELSHADER(name, body) \
                do { \
                    static Shader *name##shader = NULL; \
                    if(!name##shader) name##shader = useshaderbyname(#name); \
                    body; \
                } while(0)
            #define LOADMODELSHADER(name) DOMODELSHADER(name, return name##shader)
            #define SETMODELSHADER(m, name) DOMODELSHADER(name, (m)->setshader(name##shader))
            if(shader) return shader;

            string opts;
            int optslen = 0;
            if(alphatested()) opts[optslen++] = 'a';
            if(owner->tangents()) opts[optslen++] = 'q';
            if(bumpmapped()) opts[optslen++] = 'n';
            if(envmapped()) opts[optslen++] = 'e';
            if(masked()) opts[optslen++] = 'm';
            if(!fullbright && (masked() || spec>=0.01f)) opts[optslen++] = 's';
            opts[optslen++] = '\0';

            defformatstring(name, "model%s", opts);
            shader = generateshader(name, "modelshader \"%s\"", opts);
            return shader;
        }

        void cleanup()
        {
            if(shader && shader->standard) shader = NULL;
        }

        void preloadBIH()
        {
            if(tex->type&Texture::ALPHA && !tex->alphamask) loadalphamask(tex);
        }

        void preloadshader(bool force)
        {
            if(force) cleanup();
            loadshader();
        }

        void setshader(mesh *m, const animstate *as)
        {
            m->setshader(loadshader());
        }

        void bind(mesh *b, const animstate *as, modelattach *attached)
        {
            if(!cullface && enablecullface) { glDisable(GL_CULL_FACE); enablecullface = false; }
            else if(cullface && !enablecullface) { glEnable(GL_CULL_FACE); enablecullface = true; }

            if(as->cur.anim&ANIM_NOSKIN)
            {
                if(enablealphablend) { glDisable(GL_BLEND); enablealphablend = false; }
                if(shadowmapping) SETMODELSHADER(b, shadowmapcaster);
                else /*if(as->cur.anim&ANIM_SHADOW)*/ SETMODELSHADER(b, notexturemodel); // this shader also gets used with color mask disabled
                return;
            }
            float trans = attached && attached->transparent >= 0 ? attached->transparent : transparent;
            setshader(b, as);
            setshaderparams(b, as, trans);
            int activetmu = 0;
            if(tex!=lasttex)
            {
                glBindTexture(GL_TEXTURE_2D, tex->id);
                lasttex = tex;
            }
            if(bumpmapped() && normalmap!=lastnormalmap)
            {
                glActiveTexture_(GL_TEXTURE3);
                activetmu = 3;
                glBindTexture(GL_TEXTURE_2D, normalmap->id);
                lastnormalmap = normalmap;
            }
            if(trans < 1 && !enablealphablend)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                enablealphablend = true;
            }
            if(tex->type&Texture::ALPHA)
            {
                if(alphablend)
                {
                    if(!enablealphablend && !reflecting && !refracting)
                    {
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        enablealphablend = true;
                    }
                }
                else if(enablealphablend && trans>=1) { glDisable(GL_BLEND); enablealphablend = false; }
            }
            else if(enablealphablend && trans>=1) { glDisable(GL_BLEND); enablealphablend = false; }
            if(masked() && masks!=lastmasks)
            {
                glActiveTexture_(GL_TEXTURE1);
                activetmu = 1;
                glBindTexture(GL_TEXTURE_2D, masks->id);
                lastmasks = masks;
            }
            if(envmapped())
            {
                GLuint emtex = envmap ? envmap->id : closestenvmaptex;
                if(lastenvmaptex!=emtex)
                {
                    glActiveTexture_(GL_TEXTURE2);
                    activetmu = 2;
                    glBindTexture(GL_TEXTURE_CUBE_MAP, emtex);
                    lastenvmaptex = emtex;
                }
            }
            if(activetmu != 0) glActiveTexture_(GL_TEXTURE0);
        }
    };

    struct meshgroup;

    struct mesh
    {
        meshgroup *group;
        char *name;
        bool noclip;

        mesh() : group(NULL), name(NULL), noclip(false)
        {
        }

        virtual ~mesh()
        {
            DELETEA(name);
        }

        virtual void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m) {}

        virtual void genBIH(BIH::mesh &m) {}
        void genBIH(skin &s, vector<BIH::mesh> &bih, const matrix4x3 &t)
        {
            BIH::mesh &m = bih.add();
            m.xform = t;
            m.tex = s.tex;
            if(s.tex->type&Texture::ALPHA) m.flags |= BIH::MESH_ALPHA;
            if(noclip) m.flags |= BIH::MESH_NOCLIP;
            if(s.cullface) m.flags |= BIH::MESH_CULLFACE;
            genBIH(m);
        }

        virtual void setshader(Shader *s)
        {
            if(glaring) s->setvariant(0, 1);
            else s->set();
        }

        template<class V, class T> void smoothnorms(V *verts, int numverts, T *tris, int numtris, float limit, bool areaweight)
        {
            hashtable<vec, int> share;
            int *next = new int[numverts];
            memset(next, -1, numverts*sizeof(int));
            loopi(numverts)
            {
                V &v = verts[i];
                v.norm = vec(0, 0, 0);
                int idx = share.access(v.pos, i);
                if(idx != i) { next[i] = next[idx]; next[idx] = i; }
            }
            loopi(numtris)
            {
                T &t = tris[i];
                V &v1 = verts[t.vert[0]], &v2 = verts[t.vert[1]], &v3 = verts[t.vert[2]];
                vec norm;
                norm.cross(vec(v2.pos).sub(v1.pos), vec(v3.pos).sub(v1.pos));
                if(!areaweight) norm.normalize();
                v1.norm.add(norm);
                v2.norm.add(norm);
                v3.norm.add(norm);
            }
            vec *norms = new vec[numverts];
            memset(norms, 0, numverts*sizeof(vec));
            loopi(numverts)
            {
                V &v = verts[i];
                norms[i].add(v.norm);
                if(next[i] >= 0)
                {
                    float vlimit = limit*v.norm.magnitude();
                    for(int j = next[i]; j >= 0; j = next[j])
                    {
                        V &o = verts[j];
                        if(v.norm.dot(o.norm) >= vlimit*o.norm.magnitude())
                        {
                            norms[i].add(o.norm);
                            norms[j].add(v.norm);
                        }
                    }
                }
            }
            loopi(numverts) verts[i].norm = norms[i].normalize();
            delete[] next;
            delete[] norms;
        }

        template<class V, class T> void buildnorms(V *verts, int numverts, T *tris, int numtris, bool areaweight)
        {
            loopi(numverts) verts[i].norm = vec(0, 0, 0);
            loopi(numtris)
            {
                T &t = tris[i];
                V &v1 = verts[t.vert[0]], &v2 = verts[t.vert[1]], &v3 = verts[t.vert[2]];
                vec norm;
                norm.cross(vec(v2.pos).sub(v1.pos), vec(v3.pos).sub(v1.pos));
                if(!areaweight) norm.normalize();
                v1.norm.add(norm);
                v2.norm.add(norm);
                v3.norm.add(norm);
            }
            loopi(numverts) verts[i].norm.normalize();
        }

        template<class V, class T> void buildnorms(V *verts, int numverts, T *tris, int numtris, bool areaweight, int numframes)
        {
            if(!numverts) return;
            loopi(numframes) buildnorms(&verts[i*numverts], numverts, tris, numtris, areaweight);
        }

        static inline void fixqtangent(quat &q, float bt)
        {
            static const float bias = -1.5f/65535, biasscale = sqrtf(1 - bias*bias);
            if(bt < 0)
            {
                if(q.w >= 0) q.neg();
                if(q.w > bias) { q.mul3(biasscale); q.w = bias; }
            }
            else if(q.w < 0) q.neg();
        }

        template<class V> static inline void calctangent(V &v, const vec &n, const vec &t, float bt)
        {
            matrix3 m;
            m.c = n;
            m.a = t;
            m.b.cross(m.c, m.a);
            quat q(m);
            fixqtangent(q, bt);
            v.tangent = q;
        }

        template<class B, class V, class TC, class T> void calctangents(B *bumpverts, V *verts, TC *tcverts, int numverts, T *tris, int numtris, bool areaweight)
        {
            vec *tangent = new vec[2*numverts], *bitangent = tangent+numverts;
            memset(tangent, 0, 2*numverts*sizeof(vec));
            loopi(numtris)
            {
                const T &t = tris[i];
                const vec &e0 = verts[t.vert[0]].pos;
                vec e1 = vec(verts[t.vert[1]].pos).sub(e0), e2 = vec(verts[t.vert[2]].pos).sub(e0);

                const vec2 &tc0 = tcverts[t.vert[0]].tc,
                           &tc1 = tcverts[t.vert[1]].tc,
                           &tc2 = tcverts[t.vert[2]].tc;
                float u1 = tc1.x - tc0.x, v1 = tc1.y - tc0.y,
                      u2 = tc2.x - tc0.x, v2 = tc2.y - tc0.y;
                vec u(e2), v(e2);
                u.mul(v1).sub(vec(e1).mul(v2));
                v.mul(u1).sub(vec(e1).mul(u2));

                if(vec().cross(e2, e1).dot(vec().cross(v, u)) >= 0)
                {
                    u.neg();
                    v.neg();
                }

                if(!areaweight)
                {
                    u.normalize();
                    v.normalize();
                }

                loopj(3)
                {
                    tangent[t.vert[j]].sub(u);
                    bitangent[t.vert[j]].add(v);
                }
            }
            loopi(numverts)
            {
                const vec &n = verts[i].norm,
                          &t = tangent[i],
                          &bt = bitangent[i];
                B &bv = bumpverts[i];
                matrix3 m;
                m.c = n;
                (m.a = t).project(m.c).normalize();
                m.b.cross(m.c, m.a);
                quat q(m);
                fixqtangent(q, m.b.dot(bt));
                bv.tangent = q;
            }
            delete[] tangent;
        }

        template<class B, class V, class TC, class T> void calctangents(B *bumpverts, V *verts, TC *tcverts, int numverts, T *tris, int numtris, bool areaweight, int numframes)
        {
            loopi(numframes) calctangents(&bumpverts[i*numverts], &verts[i*numverts], tcverts, numverts, tris, numtris, areaweight);
        }
    };

    struct meshgroup
    {
        meshgroup *next;
        int shared;
        char *name;
        vector<mesh *> meshes;

        meshgroup() : next(NULL), shared(0), name(NULL)
        {
        }

        virtual ~meshgroup()
        {
            DELETEA(name);
            meshes.deletecontents();
            DELETEP(next);
        }

        virtual int findtag(const char *name) { return -1; }
        virtual void concattagtransform(part *p, int i, const matrix4x3 &m, matrix4x3 &n) {}

        void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m)
        {
            loopv(meshes) meshes[i]->calcbb(bbmin, bbmax, m);
        }

        void genBIH(vector<skin> &skins, vector<BIH::mesh> &bih, const matrix4x3 &t)
        {
            loopv(meshes) meshes[i]->genBIH(skins[i], bih, t);
        }

        virtual void *animkey() { return this; }
        virtual int totalframes() const { return 1; }
        bool hasframe(int i) const { return i>=0 && i<totalframes(); }
        bool hasframes(int i, int n) const { return i>=0 && i+n<=totalframes(); }
        int clipframes(int i, int n) const { return min(n, totalframes() - i); }

        virtual void cleanup() {}
        virtual void preload(part *p) {}
        virtual void render(const animstate *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p, modelattach *attached) {}

        void bindpos(GLuint ebuf, GLuint vbuf, void *v, int stride)
        {
            if(lastebuf!=ebuf)
            {
                gle::bindebo(ebuf);
                lastebuf = ebuf;
            }
            if(lastvbuf!=vbuf)
            {
                gle::bindvbo(vbuf);
                if(!lastvbuf) gle::enablevertex();
                gle::vertexpointer(stride, v);
                lastvbuf = vbuf;
            }
        }

        void bindtc(void *v, int stride)
        {
            if(!enabletc)
            {
                gle::enabletexcoord0();
                enabletc = true;
            }
            if(lasttcbuf!=lastvbuf)
            {
                gle::texcoord0pointer(stride, v);
                lasttcbuf = lastvbuf;
            }
        }

        void bindnormals(void *v, int stride)
        {
            if(!enablenormals)
            {
                gle::enablenormal();
                enablenormals = true;
            }
            if(lastnbuf!=lastvbuf)
            {
                gle::normalpointer(stride, v);
                lastnbuf = lastvbuf;
            }
        }

        void bindtangents(void *v, int stride)
        {
            if(!enabletangents)
            {
                gle::enabletangent();
                enabletangents = true;
            }
            if(lastxbuf!=lastvbuf)
            {
                gle::tangentpointer(stride, v, GL_SHORT);
                lastxbuf = lastvbuf;
            }
        }

        void bindbones(void *wv, void *bv, int stride)
        {
            if(!enablebones)
            {
                gle::enableboneweight();
                gle::enableboneindex();
                enablebones = true;
            }
            if(lastbbuf!=lastvbuf)
            {
                gle::boneweightpointer(stride, wv);
                gle::boneindexpointer(stride, bv);
                lastbbuf = lastvbuf;
            }
        }
    };

    virtual meshgroup *loadmeshes(const char *name, va_list args) { return NULL; }

    meshgroup *sharemeshes(const char *name, ...)
    {
        static hashnameset<meshgroup *> meshgroups;
        if(!meshgroups.access(name))
        {
            va_list args;
            va_start(args, name);
            meshgroup *group = loadmeshes(name, args);
            va_end(args);
            if(!group) return NULL;
            meshgroups.add(group);
        }
        return meshgroups[name];
    }

    struct linkedpart
    {
        part *p;
        int tag, anim, basetime;
        vec translate, rotate;
        modelattach *attached;
        matrix4 matrix;

        linkedpart() : p(NULL), tag(-1), anim(-1), basetime(0), translate(0, 0, 0), rotate(0, 0, 0), attached(NULL) {}
    };

    struct part
    {
        animmodel *model;
        int index;
        meshgroup *meshes;
        vector<linkedpart> links;
        vector<skin> skins;
        vector<animspec> *anims[MAXANIMPARTS];
        int numanimparts;
        float pitchscale, pitchoffset, pitchmin, pitchmax;
        vec translate;

        part(animmodel *model, int index = 0) : model(model), index(index), meshes(NULL), numanimparts(1), pitchscale(1), pitchoffset(0), pitchmin(0), pitchmax(0), translate(0, 0, 0)
        {
            loopk(MAXANIMPARTS) anims[k] = NULL;
        }
        virtual ~part()
        {
            loopk(MAXANIMPARTS) DELETEA(anims[k]);
        }

        virtual void cleanup()
        {
            if(meshes) meshes->cleanup();
            loopv(skins) skins[i].cleanup();
        }

        void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m)
        {
            matrix4x3 t = m;
            t.translate(translate);
            t.scale(model->scale);
            meshes->calcbb(bbmin, bbmax, t);
            loopv(links)
            {
                matrix4x3 n;
                meshes->concattagtransform(this, links[i].tag, m, n);
                n.translate(links[i].translate, model->scale);
                links[i].p->calcbb(bbmin, bbmax, n);
            }
        }

        void genBIH(vector<BIH::mesh> &bih, const matrix4x3 &m)
        {
            matrix4x3 t = m;
            t.translate(translate);
            t.scale(model->scale);
            meshes->genBIH(skins, bih, t);
            loopv(links)
            {
                matrix4x3 n;
                meshes->concattagtransform(this, links[i].tag, m, n);
                n.translate(links[i].translate, model->scale);
                links[i].p->genBIH(bih, n);
            }
        }

        bool link(part *p, const char *tag, const vec &translate = vec(0, 0, 0), const vec &rotate = vec(0, 0, 0), int anim = -1, int basetime = 0, modelattach *attached = NULL)
        {
            int i = meshes ? meshes->findtag(tag) : -1;
            if(i<0)
            {
                loopv(links) if(links[i].p && links[i].p->link(p, tag, translate, rotate, anim, basetime, attached)) return true;
                return false;
            }
            linkedpart &l = links.add();
            l.p = p;
            l.tag = i;
            l.anim = anim;
            l.basetime = basetime;
            l.translate = translate;
            l.rotate = rotate;
            l.attached = attached;
            return true;
        }

        bool unlink(part *p)
        {
            loopvrev(links) if(links[i].p==p) { links.remove(i, 1); return true; }
            loopv(links) if(links[i].p && links[i].p->unlink(p)) return true;
            return false;
        }

        void initskins(Texture *tex = notexture, Texture *masks = notexture, int limit = 0)
        {
            if(!limit)
            {
                if(!meshes) return;
                limit = meshes->meshes.length();
            }
            while(skins.length() < limit)
            {
                skin &s = skins.add();
                s.owner = this;
                s.tex = tex;
                s.masks = masks;
            }
        }

        bool envmapped()
        {
            loopv(skins) if(skins[i].envmapped()) return true;
            return false;
        }

        bool tangents()
        {
            loopv(skins) if(skins[i].tangents()) return true;
            return false;
        }

        void preloadBIH()
        {
            loopv(skins) skins[i].preloadBIH();
        }

        void preloadshaders(bool force)
        {
            loopv(skins) skins[i].preloadshader(force);
        }

        void preloadmeshes()
        {
            if(meshes) meshes->preload(this);
        }

        virtual void getdefaultanim(animinfo &info, int anim, int basetime, int basetime2, uint varseed, dynent *d)
        {
            info.frame = 0;
            info.range = 1;
        }

        bool calcanim(int animpart, int anim, int basetime, int basetime2, dynent *d, int interp, animinfo &info, int &aitime)
        {
            uint varseed = uint((size_t)d);
            info.anim = anim;
            info.basetime = basetime;
            info.varseed = varseed;
            info.speed = anim&ANIM_SETSPEED ? basetime2 : 100.0f;
            if((anim&ANIM_INDEX)==ANIM_ALL)
            {
                info.frame = 0;
                info.range = meshes->totalframes();
            }
            else
            {
                animspec *spec = NULL;
                if(anims[animpart])
                {
                    int primaryidx = anim&ANIM_INDEX;
                    if(primaryidx < game::numanims())
                    {
                        vector<animspec> &primary = anims[animpart][primaryidx];
                        if(primary.length()) spec = &primary[uint(varseed + basetime)%primary.length()];
                    }
                    if((anim>>ANIM_SECONDARY)&(ANIM_INDEX|ANIM_DIR))
                    {
                        int secondaryidx = (anim>>ANIM_SECONDARY)&ANIM_INDEX;
                        if(secondaryidx < game::numanims())
                        {
                            vector<animspec> &secondary = anims[animpart][secondaryidx];
                            if(secondary.length())
                            {
                                animspec &spec2 = secondary[uint(varseed + basetime2)%secondary.length()];
                                if(!spec || spec2.priority > spec->priority)
                                {
                                    spec = &spec2;
                                    info.anim >>= ANIM_SECONDARY;
                                    info.basetime = basetime2;
                                }
                            }
                        }
                    }
                }
                if(spec)
                {
                    info.frame = spec->frame;
                    info.range = spec->range;
                    if(spec->speed>0) info.speed = 1000.0f/spec->speed;
                }
                else getdefaultanim(info, anim, basetime, basetime2, varseed, d);
            }

            info.anim &= (1<<ANIM_SECONDARY)-1;
            info.anim |= anim&ANIM_FLAGS;
            if((info.anim&ANIM_CLAMP) != ANIM_CLAMP)
            {
                if(info.anim&(ANIM_LOOP|ANIM_START|ANIM_END))
                {
                    info.anim &= ~ANIM_SETTIME;
                    if(!info.basetime) info.basetime = -((int)(size_t)d&0xFFF);
                }
                if(info.anim&(ANIM_START|ANIM_END))
                {
                    if(info.anim&ANIM_END) info.frame += info.range-1;
                    info.range = 1;
                }
            }

            if(!meshes->hasframes(info.frame, info.range))
            {
                if(!meshes->hasframe(info.frame)) return false;
                info.range = meshes->clipframes(info.frame, info.range);
            }

            if(d && interp>=0)
            {
                animinterpinfo &ai = d->animinterp[interp];
                if((info.anim&ANIM_CLAMP)==ANIM_CLAMP) aitime = min(aitime, int(info.range*info.speed*0.5e-3f));
                void *ak = meshes->animkey();
                if(d->ragdoll && !(anim&ANIM_RAGDOLL))
                {
                    ai.prev.range = ai.cur.range = 0;
                    ai.lastswitch = -1;
                }
                else if(ai.lastmodel!=ak || ai.lastswitch<0 || lastmillis-d->lastrendered>aitime)
                {
                    ai.prev = ai.cur = info;
                    ai.lastswitch = lastmillis-aitime*2;
                }
                else if(ai.cur!=info)
                {
                    if(lastmillis-ai.lastswitch>aitime/2) ai.prev = ai.cur;
                    ai.cur = info;
                    ai.lastswitch = lastmillis;
                }
                else if(info.anim&ANIM_SETTIME) ai.cur.basetime = info.basetime;
                ai.lastmodel = ak;
            }
            return true;
        }

        void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *attached)
        {
            animstate as[MAXANIMPARTS];
            render(anim, basetime, basetime2, pitch, axis, forward, d, attached, as);
        }

        void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *attached, animstate *as)
        {
            if(!(anim&ANIM_REUSE)) loopi(numanimparts)
            {
                animinfo info;
                int interp = d && index+numanimparts<=MAXANIMPARTS ? index+i : -1, aitime = animationinterpolationtime;
                if(!calcanim(i, anim, basetime, basetime2, d, interp, info, aitime)) return;
                animstate &p = as[i];
                p.owner = this;
                p.cur.setframes(info);
                p.interp = 1;
                if(interp>=0 && d->animinterp[interp].prev.range>0)
                {
                    int diff = lastmillis-d->animinterp[interp].lastswitch;
                    if(diff<aitime)
                    {
                        p.prev.setframes(d->animinterp[interp].prev);
                        p.interp = diff/float(aitime);
                    }
                }
            }

            vec oaxis, oforward;
            matrixstack[matrixpos].transposedtransformnormal(axis, oaxis);
            float pitchamount = pitchscale*pitch + pitchoffset;
            if(pitchmin || pitchmax) pitchamount = clamp(pitchamount, pitchmin, pitchmax);
            if(as->cur.anim&ANIM_NOPITCH || (as->interp < 1 && as->prev.anim&ANIM_NOPITCH))
                pitchamount *= (as->cur.anim&ANIM_NOPITCH ? 0 : as->interp) + (as->interp < 1 && as->prev.anim&ANIM_NOPITCH ? 0 : 1-as->interp);
            if(pitchamount)
            {
                ++matrixpos;
                matrixstack[matrixpos] = matrixstack[matrixpos-1];
                matrixstack[matrixpos].rotate(pitchamount*RAD, oaxis);
            }
            matrixstack[matrixpos].transposedtransformnormal(forward, oforward);

            float resize = model->scale * (attached && attached->sizescale >= 0 ? attached->sizescale : sizescale);
            if(!(anim&ANIM_NORENDER))
            {
                matrix4 modelmatrix;
                modelmatrix.mul(shadowmapping ? shadowmatrix : camprojmatrix, matrixstack[matrixpos]);
                if(resize!=1) modelmatrix.scale(resize);
                if(!(anim&ANIM_NOTRANS) && !translate.iszero()) modelmatrix.translate(translate);
                GLOBALPARAM(modelmatrix, modelmatrix);

                if(!(anim&ANIM_NOSKIN))
                {
                    if(envmapped()) GLOBALPARAM(modelworld, matrix3(matrixstack[matrixpos]));

                    vec odir, ocampos;
                    matrixstack[matrixpos].transposedtransformnormal(lightdir, odir);
                    GLOBALPARAM(lightdir, odir);
                    matrixstack[matrixpos].transposedtransform(camera1->o, ocampos);
                    ocampos.div(resize);
                    if(!(anim&ANIM_NOTRANS)) ocampos.sub(translate);
                    GLOBALPARAM(modelcamera, ocampos);
                }
            }

            meshes->render(as, pitch, oaxis, oforward, d, this, attached);

            if(!(anim&ANIM_REUSE))
            {
                loopv(links)
                {
                    linkedpart &link = links[i];
                    link.matrix.translate(link.translate, resize);
                    if(link.rotate.x) link.matrix.rotate_around_z(link.rotate.x*RAD);
                    if(link.rotate.z) link.matrix.rotate_around_x(-link.rotate.z*RAD);
                    if(link.rotate.y) link.matrix.rotate_around_y(link.rotate.y*RAD);

                    matrixpos++;
                    matrixstack[matrixpos].mul(matrixstack[matrixpos-1], link.matrix);

                    if(link.attached && link.attached->pos) *link.attached->pos = matrixstack[matrixpos].gettranslation();

                    if(!link.p)
                    {
                        matrixpos--;
                        continue;
                    }

                    int nanim = anim, nbasetime = basetime, nbasetime2 = basetime2;
                    if(link.anim>=0)
                    {
                        nanim = link.anim | (anim&ANIM_FLAGS);
                        nbasetime = link.basetime;
                        nbasetime2 = 0;
                    }
                    link.p->render(nanim, nbasetime, nbasetime2, pitch, axis, forward, d, link.attached);

                    matrixpos--;
                }
            }

            if(pitchamount) matrixpos--;
        }

        void setanim(int animpart, int num, int frame, int range, float speed, int priority = 0)
        {
            if(animpart<0 || animpart>=MAXANIMPARTS) return;
            if(frame<0 || range<=0 || !meshes || !meshes->hasframes(frame, range))
            {
                conoutf("Invalid frame %d, range %d in model %s", frame, range, model->name);
                return;
            }
            if(!anims[animpart]) anims[animpart] = new vector<animspec>[game::numanims()];
            animspec &spec = anims[animpart][num].add();
            spec.frame = frame;
            spec.range = range;
            spec.speed = speed;
            spec.priority = priority;
        }

        virtual void loaded()
        {
            meshes->shared++;
            loopv(skins) skins[i].setkey();
        }
    };

    enum
    {
        LINK_TAG = 0,
        LINK_COOP,
        LINK_REUSE
    };

    virtual int linktype(animmodel *m) const { return LINK_TAG; }

    void render(int anim, int basetime, int basetime2, float pitch, const vec &axis, const vec &forward, dynent *d, modelattach *a)
    {
        int numtags = 0;
        if(a)
        {
            int index = parts.last()->index + parts.last()->numanimparts;
            for(int i = 0; a[i].tag; i++)
            {
                numtags++;

                animmodel *m = (animmodel *)a[i].m;
                if(!m)
                {
                    if(a[i].pos) link(NULL, a[i].tag, vec(0, 0, 0), vec(0, 0, 0), 0, 0, &a[i]);
                    continue;
                }
                part *p = m->parts[0];
                switch(linktype(m))
                {
                    case LINK_TAG:
                        p->index = link(p, a[i].tag, vec(0, 0, 0), vec(m->offsetyaw + m->spinyaw*lastmillis/1000.0f, m->offsetpitch + m->spinpitch*lastmillis/1000.0f, m->offsetroll + m->spinroll*lastmillis/1000.0f), a[i].anim, a[i].basetime, &a[i]) ? index : -1;
                        break;

                    case LINK_COOP:
                        p->index = index;
                        break;

                    default:
                        continue;
                }
                index += p->numanimparts;
            }
        }

        animstate as[MAXANIMPARTS];
        parts[0]->render(anim, basetime, basetime2, pitch, axis, forward, d, NULL, as);

        if(a) for(int i = numtags-1; i >= 0; i--)
        {
            animmodel *m = (animmodel *)a[i].m;
            if(!m)
            {
                if(a[i].pos) unlink(NULL);
                continue;
            }
            part *p = m->parts[0];
            switch(linktype(m))
            {
                case LINK_TAG:
                    if(p->index >= 0) unlink(p);
                    p->index = 0;
                    break;

                case LINK_COOP:
                    p->render(anim, basetime, basetime2, pitch, axis, forward, d, &a[i]);
                    p->index = 0;
                    break;

                case LINK_REUSE:
                    p->render(anim | ANIM_REUSE, basetime, basetime2, pitch, axis, forward, d, &a[i], as);
                    break;
            }
        }
    }

    void render(int anim, int basetime, int basetime2, const vec &o, float yaw, float pitch, float roll, dynent *d, modelattach *a, const vec &color, const vec &dir, const bvec *material, float trans, float size)
    {
        float syaw = spinyaw*lastmillis/1000.0f, sroll = spinroll*lastmillis/1000.0f;
        pitch += offsetpitch + spinpitch*lastmillis/1000.0f;

        vec axis(0, -1, 0), forward(1, 0, 0);

        matrixpos = 0;
        matrixstack[0].identity();
        if(!d || !d->ragdoll || anim&ANIM_RAGDOLL)
        {
            matrixstack[0].settranslation(o);
            if(yaw) matrixstack[0].rotate_around_z(yaw*RAD);
            if(roll) matrixstack[0].rotate_around_x(-roll*RAD);
            matrixstack[0].transformnormal(vec(axis), axis);
            matrixstack[0].transformnormal(vec(forward), forward);
            if(offsetyaw) matrixstack[0].rotate_around_z(offsetyaw*RAD);
            if(offsetroll) matrixstack[0].rotate_around_x(-offsetroll*RAD);
            if(syaw) matrixstack[0].rotate_around_z(syaw*RAD);
            if(sroll) matrixstack[0].rotate_around_x(-sroll*RAD);
        }
        else
        {
            matrixstack[0].settranslation(d->ragdoll->center);
            pitch = 0;
        }

        sizescale = size;

        if(anim&ANIM_NORENDER)
        {
            render(anim, basetime, basetime2, pitch, axis, forward, d, a);
            if(d) d->lastrendered = lastmillis;
            return;
        }

        if(!(anim&ANIM_NOSKIN))
        {
            transparent = trans;
            lightdir = dir;
            lightcolor = color;
            lightmaterial = material;

            if(envmapped())
            {
            setupenvmap:
                closestenvmaptex = lookupenvmap(closestenvmap(o));
                GLOBALPARAM(lightdirworld, dir);
            }
            else if(a) for(int i = 0; a[i].tag; i++) if(a[i].m && a[i].m->envmapped()) goto setupenvmap;
        }

        if(depthoffset && !enabledepthoffset)
        {
            enablepolygonoffset(GL_POLYGON_OFFSET_FILL);
            enabledepthoffset = true;
        }

        if(transparent<1)
        {
            if(alphadepth)
            {
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                render(anim|ANIM_NOSKIN, basetime, basetime2, pitch, axis, forward, d, a);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, fading ? GL_FALSE : GL_TRUE);

                glDepthFunc(GL_LEQUAL);
            }
        }

        render(anim, basetime, basetime2, pitch, axis, forward, d, a);

        if(transparent<1 && alphadepth) glDepthFunc(GL_LESS);

        if(d) d->lastrendered = lastmillis;
    }

    vector<part *> parts;

    animmodel(const char *name) : model(name)
    {
    }

    ~animmodel()
    {
        parts.deletecontents();
    }

    void cleanup()
    {
        loopv(parts) parts[i]->cleanup();
    }

    part &addpart()
    {
        part *p = new part(this, parts.length());
        parts.add(p);
        return *p;
    }

    void initmatrix(matrix4x3 &m)
    {
        m.identity();
        if(offsetyaw) m.rotate_around_z(offsetyaw*RAD);
        if(offsetroll) m.rotate_around_x(-offsetroll*RAD);
        if(offsetpitch) m.rotate_around_y(offsetpitch*RAD);
    }

    void genBIH(vector<BIH::mesh> &bih)
    {
        if(parts.empty()) return;
        matrix4x3 m;
        initmatrix(m);
        parts[0]->genBIH(bih, m);
    }

    void preloadBIH()
    {
        model::preloadBIH();
        if(bih) loopv(parts) parts[i]->preloadBIH();
    }

    BIH *setBIH()
    {
        if(bih) return bih;
        vector<BIH::mesh> meshes;
        genBIH(meshes);
        bih = new BIH(meshes);
        return bih;
    }

    bool link(part *p, const char *tag, const vec &translate = vec(0, 0, 0), const vec &rotate = vec(0, 0, 0), int anim = -1, int basetime = 0, modelattach *attached = NULL)
    {
        if(parts.empty()) return false;
        return parts[0]->link(p, tag, translate, rotate, anim, basetime, attached);
    }

    bool unlink(part *p)
    {
        if(parts.empty()) return false;
        return parts[0]->unlink(p);
    }

    bool envmapped()
    {
        loopv(parts) if(parts[i]->envmapped()) return true;
        return false;
    }

    virtual bool loaddefaultparts()
    {
        return true;
    }

    void preloadshaders(bool force)
    {
        loopv(parts) parts[i]->preloadshaders(force);
    }

    void preloadmeshes()
    {
        loopv(parts) parts[i]->preloadmeshes();
    }

    void setshader(Shader *shader)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].shader = shader;
    }

    void setenvmap(float envmapmin, float envmapmax, Texture *envmap)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins)
        {
            skin &s = parts[i]->skins[j];
            if(envmapmax)
            {
                s.envmapmin = envmapmin;
                s.envmapmax = envmapmax;
            }
            if(envmap) s.envmap = envmap;
        }
    }

    void setspec(float spec)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].spec = spec;
    }

    void setambient(float ambient)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].ambient = ambient;
    }

    void setglow(float glow, float delta, float pulse)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins)
        {
            skin &s = parts[i]->skins[j];
            s.glow = glow;
            s.glowdelta = delta;
            s.glowpulse = pulse;
        }
    }

    void setglare(float specglare, float glowglare)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins)
        {
            skin &s = parts[i]->skins[j];
            s.specglare = specglare;
            s.glowglare = glowglare;
        }
    }

    void setalphatest(float alphatest)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].alphatest = alphatest;
    }

    void setalphablend(bool alphablend)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].alphablend = alphablend;
    }

    void setfullbright(float fullbright)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].fullbright = fullbright;
    }

    void setcullface(bool cullface)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].cullface = cullface;
    }

    void setmaterial(int material, int material2)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins)
        {
            skin &s = parts[i]->skins[j];
            s.material = material;
            s.material2 = material2;
        }
    }

    void calcbb(vec &center, vec &radius)
    {
        if(parts.empty()) return;
        vec bbmin(1e16f, 1e16f, 1e16f), bbmax(-1e16f, -1e16f, -1e16f);
        matrix4x3 m;
        initmatrix(m);
        parts[0]->calcbb(bbmin, bbmax, m);
        radius = bbmax;
        radius.sub(bbmin);
        radius.mul(0.5f);
        center = bbmin;
        center.add(radius);
    }

    virtual void loaded()
    {
        scale /= 4;
        if(parts.length()) parts[0]->translate = translate;
        loopv(parts) parts[i]->loaded();
    }

    static bool enabletc, enablealphablend, enablecullface, enablenormals, enabletangents, enablebones, enabledepthoffset;
    static vec lightdir, lightcolor;
    static const bvec *lightmaterial;
    static float transparent, lastalphatest, sizescale;
    static GLuint lastvbuf, lasttcbuf, lastnbuf, lastxbuf, lastbbuf, lastebuf, lastenvmaptex, closestenvmaptex;
    static Texture *lasttex, *lastmasks, *lastnormalmap;
    static int matrixpos;
    static matrix4 matrixstack[64];

    void startrender()
    {
        enabletc = enablealphablend = enablenormals = enabletangents = enablebones = enabledepthoffset = false;
        enablecullface = true;
        lastalphatest = -1;
        lastvbuf = lasttcbuf = lastxbuf = lastnbuf = lastbbuf = lastebuf = lastenvmaptex = closestenvmaptex = 0;
        lasttex = lastmasks = lastnormalmap = NULL;
        transparent = sizescale = 1;
        shaderparamskey::invalidate();
    }

    static void disablebones()
    {
        gle::disableboneweight();
        gle::disableboneindex();
        enablebones = false;
    }

    static void disabletangents()
    {
        gle::disabletangent();
        enabletangents = false;
    }

    static void disabletc()
    {
        gle::disabletexcoord0();
        enabletc = false;
    }

    static void disablenormals()
    {
        gle::disablenormal();
        enablenormals = false;
    }

    static void disablevbo()
    {
        if(lastebuf) gle::clearebo();
        if(lastvbuf)
        {
            gle::clearvbo();
            gle::disablevertex();
        }
        if(enabletc) disabletc();
        if(enablenormals) disablenormals();
        if(enabletangents) disabletangents();
        if(enablebones) disablebones();
        lastvbuf = lasttcbuf = lastxbuf = lastnbuf = lastbbuf = lastebuf = 0;
    }

    void endrender()
    {
        if(lastvbuf || lastebuf) disablevbo();
        if(enablealphablend) glDisable(GL_BLEND);
        if(!enablecullface) glEnable(GL_CULL_FACE);
        if(enabledepthoffset) disablepolygonoffset(GL_POLYGON_OFFSET_FILL);
    }
};

bool animmodel::enabletc = false, animmodel::enablealphablend = false,
     animmodel::enablecullface = true,
     animmodel::enablenormals = false, animmodel::enabletangents = false, animmodel::enablebones = false, animmodel::enabledepthoffset = false;
vec animmodel::lightdir(0, 0, 1), animmodel::lightcolor(1, 1, 1);
const bvec *animmodel::lightmaterial = NULL;
float animmodel::transparent = 1, animmodel::lastalphatest = -1, animmodel::sizescale = 1;
GLuint animmodel::lastvbuf = 0, animmodel::lasttcbuf = 0, animmodel::lastnbuf = 0, animmodel::lastxbuf = 0, animmodel::lastbbuf = 0,
       animmodel::lastebuf = 0, animmodel::lastenvmaptex = 0, animmodel::closestenvmaptex = 0;
Texture *animmodel::lasttex = NULL, *animmodel::lastmasks = NULL, *animmodel::lastnormalmap = NULL;
int animmodel::matrixpos = 0;
matrix4 animmodel::matrixstack[64];

static inline uint hthash(const animmodel::shaderparams &k)
{
    return memhash(&k, sizeof(k));
}

static inline bool htcmp(const animmodel::shaderparams &x, const animmodel::shaderparams &y)
{
    return !memcmp(&x, &y, sizeof(animmodel::shaderparams));
}

hashtable<animmodel::shaderparams, animmodel::shaderparamskey> animmodel::shaderparamskey::keys;
int animmodel::shaderparamskey::firstversion = 0, animmodel::shaderparamskey::lastversion = 1;

template<class MDL> struct modelloader
{
    static MDL *loading;
    static string dir;

    static bool animated() { return true; }
    static bool multiparted() { return true; }
    static bool multimeshed() { return true; }
};

template<class MDL> MDL *modelloader<MDL>::loading = NULL;
template<class MDL> string modelloader<MDL>::dir = {'\0'}; // crashes clang if "" is used here

template<class MDL, class MESH> struct modelcommands
{
    typedef struct MDL::part part;
    typedef struct MDL::skin skin;

    static void setdir(char *name)
    {
        if(!MDL::loading) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        formatstring(MDL::dir, "%s", name);
    }

    #define loopmeshes(meshname, m, body) \
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; } \
        part &mdl = *MDL::loading->parts.last(); \
        if(!mdl.meshes) return; \
        loopv(mdl.meshes->meshes) \
        { \
            MESH &m = *(MESH *)mdl.meshes->meshes[i]; \
            if(!strcmp(meshname, "*") || (m.name && !strcmp(m.name, meshname))) \
            { \
                body; \
            } \
        }

    #define loopskins(meshname, s, body) loopmeshes(meshname, m, { skin &s = mdl.skins[i]; body; })

    static void setskin(char *meshname, char *tex, char *masks, float *envmapmax, float *envmapmin)
    {
        loopskins(meshname, s,
            s.tex = textureload(makerelpath(MDL::dir, tex), 0, true, false);
            if(*masks)
            {
                s.masks = textureload(makerelpath(MDL::dir, masks), 0, true, false);
                s.envmapmax = *envmapmax;
                s.envmapmin = *envmapmin;
            }
        );
    }

    static void setspec(char *meshname, int *percent)
    {
        float spec = 1.0f;
        if(*percent>0) spec = *percent/100.0f;
        else if(*percent<0) spec = 0.0f;
        loopskins(meshname, s, s.spec = spec);
    }

    static void setambient(char *meshname, int *percent)
    {
        float ambient = 0.3f;
        if(*percent>0) ambient = *percent/100.0f;
        else if(*percent<0) ambient = 0.0f;
        loopskins(meshname, s, s.ambient = ambient);
    }

    static void setglow(char *meshname, int *percent, int *delta, float *pulse)
    {
        float glow = 3.0f, glowdelta = *delta/100.0f, glowpulse = *pulse > 0 ? *pulse/1000.0f : 0;
        if(*percent>0) glow = *percent/100.0f;
        else if(*percent<0) glow = 0.0f;
        glowdelta -= glow;
        loopskins(meshname, s, { s.glow = glow; s.glowdelta = glowdelta; s.glowpulse = glowpulse; });
    }

    static void setglare(char *meshname, float *specglare, float *glowglare)
    {
        loopskins(meshname, s, { s.specglare = *specglare; s.glowglare = *glowglare; });
    }

    static void setalphatest(char *meshname, float *cutoff)
    {
        loopskins(meshname, s, s.alphatest = max(0.0f, min(1.0f, *cutoff)));
    }

    static void setalphablend(char *meshname, int *blend)
    {
        loopskins(meshname, s, s.alphablend = *blend!=0);
    }

    static void setcullface(char *meshname, int *cullface)
    {
        loopskins(meshname, s, s.cullface = *cullface!=0);
    }

    static void setenvmap(char *meshname, char *envmap)
    {
        Texture *tex = cubemapload(envmap);
        loopskins(meshname, s, s.envmap = tex);
    }

    static void setbumpmap(char *meshname, char *normalmapfile)
    {
        Texture *normalmaptex = textureload(makerelpath(MDL::dir, normalmapfile), 0, true, false);
        loopskins(meshname, s, s.normalmap = normalmaptex);
    }

    static void setfullbright(char *meshname, float *fullbright)
    {
        loopskins(meshname, s, s.fullbright = *fullbright);
    }

    static void setshader(char *meshname, char *shader)
    {
        loopskins(meshname, s, s.shader = lookupshaderbyname(shader));
    }

    static void setscroll(char *meshname, float *scrollu, float *scrollv)
    {
        loopskins(meshname, s, { s.scrollu = *scrollu; s.scrollv = *scrollv; });
    }

    static void setnoclip(char *meshname, int *noclip)
    {
        loopmeshes(meshname, m, m.noclip = *noclip!=0);
    }

    static void setmaterial(char *meshname, int *material, int *material2)
    {
        loopskins(meshname, s, { s.material = clamp(*material, 0, int(MAXLIGHTMATERIALS)); s.material2 = clamp(*material2, 0, int(MAXLIGHTMATERIALS)); });
    }

    static void setlink(int *parent, int *child, char *tagname, float *x, float *y, float *z, float *yaw, float *pitch, float *roll)
    {
        if(!MDL::loading) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        if(!MDL::loading->parts.inrange(*parent) || !MDL::loading->parts.inrange(*child)) { conoutf("\frNo models loaded to link"); return; }
        if(!MDL::loading->parts[*parent]->link(MDL::loading->parts[*child], tagname, vec(*x, *y, *z), vec(*yaw, *pitch, *roll))) conoutf("\frCould not link model %s", MDL::loading->name);
    }

    template<class F> void modelcommand(F *fun, const char *suffix, const char *args)
    {
        defformatstring(name, "%s%s", MDL::formatname(), suffix);
        addcommand(newstring(name), (identfun)fun, args);
    }

    modelcommands()
    {
        modelcommand(setdir, "dir", "s");
        if(MDL::multimeshed())
        {
            modelcommand(setskin, "skin", "sssff");
            modelcommand(setspec, "spec", "si");
            modelcommand(setambient, "ambient", "si");
            modelcommand(setglow, "glow", "siif");
            modelcommand(setglare, "glare", "sff");
            modelcommand(setalphatest, "alphatest", "sf");
            modelcommand(setalphablend, "alphablend", "si");
            modelcommand(setcullface, "cullface", "si");
            modelcommand(setenvmap, "envmap", "ss");
            modelcommand(setbumpmap, "bumpmap", "ss");
            modelcommand(setfullbright, "fullbright", "sf");
            modelcommand(setshader, "shader", "ss");
            modelcommand(setscroll, "scroll", "sff");
            modelcommand(setnoclip, "noclip", "si");
            modelcommand(setmaterial, "material", "sii");
        }
        if(MDL::multiparted()) modelcommand(setlink, "link", "iisffffff");
    }
};

