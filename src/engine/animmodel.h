VAR(IDF_PERSIST, fullbrightmodels, 0, 0, 200);
VAR(0, testtags, 0, 0, 1);
VARF(0, dbgcolmesh, 0, 0, 1,
{
    extern void cleanupmodels();
    cleanupmodels();
});

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
        float spec, gloss, glow, glowdelta, glowpulse, fullbright, envmapmin, envmapmax, scrollu, scrollv, alphatest;
        vec color;
        int material1, material2;

        shaderparams() : spec(1.0f), gloss(1), glow(3.0f), glowdelta(0), glowpulse(0), fullbright(0), envmapmin(0), envmapmax(0), scrollu(0), scrollv(0), alphatest(0.9f), color(1, 1, 1), material1(1), material2(0) {}
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
        enum
        {
            ALLOW_MIXER    = 1<<0,
            ENABLE_MIXER   = 1<<1,
            ALLOW_PATTERN  = 1<<2,
            ENABLE_PATTERN = 1<<3
        };

        part *owner;
        Texture *tex, *decal, *masks, *envmap, *normalmap;
        Shader *shader, *rsmshader;
        int cullface, flags;
        shaderparamskey *key;

        skin() : owner(0), tex(notexture), decal(NULL), masks(notexture), envmap(NULL), normalmap(NULL), shader(NULL), rsmshader(NULL), cullface(1), flags(0), key(NULL) {}

        bool firstmodel(const animstate *as) const
        {
            return owner->model->parts[0]->index >= 0 && owner->model->parts[0]->index < owner->numanimparts && owner->model == as->owner->model;
        }

        bool masked() const { return masks != notexture; }
        bool envmapped() const { return envmapmax>0; }
        bool bumpmapped() const { return normalmap != NULL; }
        bool alphatested() const { return alphatest > 0 && tex->type&Texture::ALPHA; }
        bool decaled() const { return decal != NULL; }

        bool mixed() const { return (flags&ENABLE_MIXER) != 0; }
        bool canmix(const modelstate *state, const animstate *as) const
        {
            if(!state->mixer || state->mixer == notexture) return false;
            return !(state->flags&MDL_NOMIXER) || firstmodel(as);
        }

        bool patterned() const { return (flags&ENABLE_PATTERN) != 0; }
        bool canpattern(const modelstate *state, const animstate *as) const
        {
            if(!state->pattern || state->pattern == notexture) return false;
            return !(state->flags&MDL_NOPATTERN) || firstmodel(as);
        }

        void setkey()
        {
            key = &shaderparamskey::keys[*this];
        }

        void setshaderparams(mesh &m, const animstate *as, bool skinned = true)
        {
            if(!Shader::lastshader) return;
            if(key->checkversion() && Shader::lastshader->owner == key) return;
            Shader::lastshader->owner = key;

            LOCALPARAMF(texscroll, scrollu*lastmillis/1000.0f, scrollv*lastmillis/1000.0f);
            if(alphatested()) LOCALPARAMF(alphatest, alphatest);

            if(!skinned) return;

            if(color.r < 0) LOCALPARAM(colorscale, colorscale);
            else LOCALPARAMF(colorscale, color.r, color.g, color.b, colorscale.a);

            if(mixed())
            {
                LOCALPARAM(mixercolor, mixercolor);
                LOCALPARAM(mixerglow, mixerglow);
                LOCALPARAMF(mixerscroll, mixerscroll.x*lastmillis/1000.0f, mixerscroll.y*lastmillis/1000.0f);
            }
            if(patterned())
            {
                LOCALPARAMF(patternscale, patternscale);
            }

            if(material1 > 0) LOCALPARAM(material1, modelmaterial[min(material1, int(MAXMDLMATERIALS))-1].tocolor());
            else LOCALPARAMF(material1, 1, 1, 1);
            if(material2 > 0) LOCALPARAM(material2, modelmaterial[min(material2, int(MAXMDLMATERIALS))-1].tocolor());
            else LOCALPARAMF(material2, 1, 1, 1);
            LOCALPARAM(matbright, matbright);

            if(fullbright) LOCALPARAMF(fullbright, 0.0f, fullbright);
            else LOCALPARAMF(fullbright, 1.0f, as->cur.anim&ANIM_FULLBRIGHT ? 0.5f*fullbrightmodels/100.0f : 0.0f);

            float curglow = glow;
            if(glowpulse > 0)
            {
                float curpulse = lastmillis*glowpulse;
                curpulse -= floor(curpulse);
                curglow += glowdelta*2*fabs(curpulse - 0.5f);
            }

            LOCALPARAMF(maskscale, spec, gloss, curglow);
            if(envmapped()) LOCALPARAMF(envmapscale, envmapmin-envmapmax, envmapmax);
        }

        Shader *loadshader(bool force = false)
        {
            #define DOMODELSHADER(name, body) \
                do { \
                    static Shader *name##shader = NULL; \
                    if(!name##shader) name##shader = useshaderbyname(#name); \
                    body; \
                } while(0)
            #define LOADMODELSHADER(name) DOMODELSHADER(name, return name##shader)
            #define SETMODELSHADER(m, name) DOMODELSHADER(name, (m).setshader(name##shader))

            if(shadowmapping == SM_REFLECT)
            {
                if(rsmshader) return rsmshader;

                string opts;
                int optslen = 0;
                if(alphatested()) opts[optslen++] = 'a';
                if(!cullface) opts[optslen++] = 'c';
                opts[optslen++] = '\0';

                defformatstring(name, "rsmmodel%s", opts);
                rsmshader = generateshader(name, "rsmmodelshader \"%s\"", opts);
                return rsmshader;
            }

            if(!force && shader) return shader;

            string opts;
            int optslen = 0;
            if(alphatested()) opts[optslen++] = 'a';
            if(owner->model->wind) opts[optslen++] = 'w';
            if(decaled()) opts[optslen++] = decal->type&Texture::ALPHA ? 'D' : 'd';
            if(bumpmapped()) opts[optslen++] = 'n';
            if(masked() || envmapped()) opts[optslen++] = 'm';
            if(envmapped()) opts[optslen++] = 'e';
            if(mixed()) opts[optslen++] = 'x';
            if(patterned()) opts[optslen++] = 'p';
            if(!cullface) opts[optslen++] = 'c';
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
            if(alphatested() && !tex->alphamask) loadalphamask(tex);
        }

        void preloadshader()
        {
            loadshader();
            if(owner->model->wind) useshaderbyname("windshadowmodel");
            else useshaderbyname(alphatested() && owner->model->alphashadow ? "alphashadowmodel" : "shadowmodel");
            if(useradiancehints()) useshaderbyname(alphatested() ? "rsmalphamodel" : "rsmmodel");
        }

        void setshader(mesh &m, const animstate *as, bool force = false)
        {
            m.setshader(loadshader(force), transparentlayer ? 1 : 0);
        }

        void bind(mesh &b, const animstate *as, modelstate *state)
        {
            if(cullface > 0)
            {
                if(!enablecullface) { glEnable(GL_CULL_FACE); enablecullface = true; }
            }
            else if(enablecullface) { glDisable(GL_CULL_FACE); enablecullface = false; }

            if(as->cur.anim&ANIM_NOSKIN)
            {
                if(alphatested() && owner->model->alphashadow)
                {
                    if(tex!=lasttex)
                    {
                        glBindTexture(GL_TEXTURE_2D, tex->id);
                        lasttex = tex;
                    }
                    if(owner->model->wind) SETMODELSHADER(b, windshadowmodel);
                    else SETMODELSHADER(b, alphashadowmodel);
                    setshaderparams(b, as, false);
                }
                else
                {
                    SETMODELSHADER(b, shadowmodel);
                }
                return;
            }
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
            if(decaled() && decal!=lastdecal)
            {
                glActiveTexture_(GL_TEXTURE4);
                activetmu = 4;
                glBindTexture(GL_TEXTURE_2D, decal->id);
                lastdecal = decal;
            }
            if(masked() && masks!=lastmasks)
            {
                glActiveTexture_(GL_TEXTURE1);
                activetmu = 1;
                glBindTexture(GL_TEXTURE_2D, masks->id);
                lastmasks = masks;
            }
            int oldflags = flags;
            if(flags&ALLOW_MIXER)
            {
                if(canmix(state, as))
                {
                    flags |= ENABLE_MIXER;
                    if(state->mixer != lastmixer)
                    {
                        glActiveTexture_(GL_TEXTURE5);
                        activetmu = 5;
                        glBindTexture(GL_TEXTURE_2D, state->mixer->id);
                        lastmixer = state->mixer;
                    }
                }
                else flags &= ~ENABLE_MIXER;
            }
            if(flags&ALLOW_PATTERN)
            {
                if(canpattern(state, as))
                {
                    flags |= ENABLE_PATTERN;
                    if(state->pattern != lastpattern)
                    {
                        glActiveTexture_(GL_TEXTURE6);
                        activetmu = 6;
                        glBindTexture(GL_TEXTURE_2D, state->pattern->id);
                        lastpattern = state->pattern;
                    }
                }
                else flags &= ~ENABLE_PATTERN;
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
            setshader(b, as, flags != oldflags);
            setshaderparams(b, as);
        }
    };

    struct meshgroup;

    struct mesh
    {
        meshgroup *group;
        char *name;
        bool cancollide, canrender, noclip;

        mesh() : group(NULL), name(NULL), cancollide(true), canrender(true), noclip(false)
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
            if(canrender) m.flags |= BIH::MESH_RENDER;
            if(cancollide) m.flags |= BIH::MESH_COLLIDE;
            if(s.alphatested()) m.flags |= BIH::MESH_ALPHA;
            if(noclip) m.flags |= BIH::MESH_NOCLIP;
            if(s.cullface > 0) m.flags |= BIH::MESH_CULLFACE;
            genBIH(m);
            while(bih.last().numtris > BIH::mesh::MAXTRIS)
            {
                BIH::mesh &overflow = bih.dup();
                overflow.tris += BIH::mesh::MAXTRIS;
                overflow.numtris -= BIH::mesh::MAXTRIS;
                bih[bih.length()-2].numtris = BIH::mesh::MAXTRIS;
            }
        }

        virtual void genshadowmesh(vector<triangle> &tris, const matrix4x3 &m) {}

        virtual void setshader(Shader *s, int row = 0)
        {
            if(row) s->setvariant(0, row);
            else s->set();
        }

        struct smoothdata
        {
            vec norm;
            int next;

            smoothdata() : norm(0, 0, 0), next(-1) {}
        };

        template<class V, class T> void smoothnorms(V *verts, int numverts, T *tris, int numtris, float limit, bool areaweight)
        {
            if(!numverts) return;
            smoothdata *smooth = new smoothdata[numverts];
            hashtable<vec, int> share;
            loopi(numverts)
            {
                V &v = verts[i];
                int &idx = share.access(v.pos, i);
                if(idx != i) { smooth[i].next = idx; idx = i; }
            }
            loopi(numtris)
            {
                T &t = tris[i];
                int v1 = t.vert[0], v2 = t.vert[1], v3 = t.vert[2];
                vec norm;
                norm.cross(verts[v1].pos, verts[v2].pos, verts[v3].pos);
                if(!areaweight) norm.normalize();
                smooth[v1].norm.add(norm);
                smooth[v2].norm.add(norm);
                smooth[v3].norm.add(norm);
            }
            loopi(numverts) verts[i].norm = vec(0, 0, 0);
            loopi(numverts)
            {
                const smoothdata &n = smooth[i];
                verts[i].norm.add(n.norm);
                if(n.next >= 0)
                {
                    float vlimit = limit*n.norm.magnitude();
                    for(int j = n.next; j >= 0;)
                    {
                        const smoothdata &o = smooth[j];
                        if(n.norm.dot(o.norm) >= vlimit*o.norm.magnitude())
                        {
                            verts[i].norm.add(o.norm);
                            verts[j].norm.add(n.norm);
                        }
                        j = o.next;
                    }
                }
            }
            loopi(numverts) verts[i].norm.normalize();
            delete[] smooth;
        }

        template<class V, class T> void buildnorms(V *verts, int numverts, T *tris, int numtris, bool areaweight)
        {
            if(!numverts) return;
            loopi(numverts) verts[i].norm = vec(0, 0, 0);
            loopi(numtris)
            {
                T &t = tris[i];
                V &v1 = verts[t.vert[0]], &v2 = verts[t.vert[1]], &v3 = verts[t.vert[2]];
                vec norm;
                norm.cross(v1.pos, v2.pos, v3.pos);
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

        template<class V, class TC, class T> void calctangents(V *verts, TC *tcverts, int numverts, T *tris, int numtris, bool areaweight)
        {
            vec *tangent = new vec[2*numverts], *bitangent = tangent+numverts;
            memclear(tangent, 2*numverts);
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
                V &v = verts[i];
                const vec &t = tangent[i],
                          &bt = bitangent[i];
                matrix3 m;
                m.c = v.norm;
                (m.a = t).project(m.c).normalize();
                m.b.cross(m.c, m.a);
                quat q(m);
                fixqtangent(q, m.b.dot(bt));
                v.tangent = q;
            }
            delete[] tangent;
        }

        template<class V, class TC, class T> void calctangents(V *verts, TC *tcverts, int numverts, T *tris, int numtris, bool areaweight, int numframes)
        {
            loopi(numframes) calctangents(&verts[i*numverts], tcverts, numverts, tris, numtris, areaweight);
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

        #define looprendermeshes(type, name, body) do { \
            loopv(meshes) \
            { \
                type &name = *(type *)meshes[i]; \
                if(name.canrender || dbgcolmesh) { body; } \
            } \
        } while(0)

        void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &t)
        {
            looprendermeshes(mesh, m, m.calcbb(bbmin, bbmax, t));
        }

        void genBIH(vector<skin> &skins, vector<BIH::mesh> &bih, const matrix4x3 &t)
        {
            loopv(meshes) meshes[i]->genBIH(skins[i], bih, t);
        }

        void genshadowmesh(vector<triangle> &tris, const matrix4x3 &t)
        {
            looprendermeshes(mesh, m, m.genshadowmesh(tris, t));
        }

        virtual void *animkey() { return this; }
        virtual int totalframes() const { return 1; }
        bool hasframe(int i) const { return i>=0 && i<totalframes(); }
        bool hasframes(int i, int n) const { return i>=0 && i+n<=totalframes(); }
        int clipframes(int i, int n) const { return min(n, totalframes() - i); }

        virtual void cleanup() {}
        virtual void preload(part *p) {}
        virtual void render(const animstate *as, float pitch, const vec &axis, const vec &forward, modelstate *state, dynent *d, part *p) {}
        virtual void intersect(const animstate *as, float pitch, const vec &axis, const vec &forward, modelstate *state, dynent *d, part *p, const vec &o, const vec &ray) {}

        void bindpos(GLuint ebuf, GLuint vbuf, void *v, int stride, int type, int size)
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
                gle::vertexpointer(stride, v, type, size);
                lastvbuf = vbuf;
            }
        }
        void bindpos(GLuint ebuf, GLuint vbuf, vec *v, int stride) { bindpos(ebuf, vbuf, v, stride, GL_FLOAT, 3); }
        void bindpos(GLuint ebuf, GLuint vbuf, hvec4 *v, int stride) { bindpos(ebuf, vbuf, v, stride, GL_HALF_FLOAT, 4); }

        void bindtc(void *v, int stride)
        {
            if(!enabletc)
            {
                gle::enabletexcoord0();
                enabletc = true;
            }
            if(lasttcbuf!=lastvbuf)
            {
                gle::texcoord0pointer(stride, v, GL_HALF_FLOAT);
                lasttcbuf = lastvbuf;
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

        void bindcolor(void *v, int stride)
        {
            if(!enablecolor)
            {
                gle::enablecolor();
                enablecolor = true;
            }
            if(lastcolbuf!=lastvbuf)
            {
                gle::colorpointer(stride, v, GL_UNSIGNED_BYTE);
                lastcolbuf = lastvbuf;
            }
        }
    };

    static hashnameset<meshgroup *> meshgroups;

    struct linkedpart
    {
        part *p;
        int tag, anim, basetime;
        float size, speed;
        vec translate, rotate;
        vec *pos;
        matrix4 matrix;

        linkedpart() : p(NULL), tag(-1), anim(-1), basetime(0), size(1), speed(1), translate(0, 0, 0), rotate(0, 0, 0), pos(NULL) {}
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

        part(animmodel *model, int index = 0) : model(model), index(index), meshes(NULL), numanimparts(1), pitchscale(1), pitchoffset(0), pitchmin(0), pitchmax(0)
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

        void disablepitch()
        {
            pitchscale = pitchoffset = pitchmin = pitchmax = 0;
        }

        void calcbb(vec &bbmin, vec &bbmax, const matrix4x3 &m)
        {
            matrix4x3 t = m;
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

        void genshadowmesh(vector<triangle> &tris, const matrix4x3 &m)
        {
            matrix4x3 t = m;
            t.scale(model->scale);
            meshes->genshadowmesh(tris, t);
            loopv(links)
            {
                matrix4x3 n;
                meshes->concattagtransform(this, links[i].tag, m, n);
                n.translate(links[i].translate, model->scale);
                links[i].p->genshadowmesh(tris, n);
            }
        }

        bool link(part *p, const char *tag, const vec &translate = vec(0, 0, 0), const vec &rotate = vec(0, 0, 0), int anim = -1, int basetime = 0, float size = 1, float speed = 1, vec *pos = NULL)
        {
            int i = meshes ? meshes->findtag(tag) : -1;
            if(i<0)
            {
                loopv(links) if(links[i].p && links[i].p->link(p, tag, translate, rotate, anim, basetime, size, speed, pos)) return true;
                return false;
            }
            linkedpart &l = links.add();
            l.p = p;
            l.tag = i;
            l.anim = anim;
            l.basetime = basetime;
            l.speed = speed;
            l.size = size;
            l.translate = translate;
            l.rotate = rotate;
            l.pos = pos;
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

        bool alphatested() const
        {
            loopv(skins) if(skins[i].alphatested()) return true;
            return false;
        }

        void preloadBIH()
        {
            loopv(skins) skins[i].preloadBIH();
        }

        void preloadshaders()
        {
            loopv(skins) skins[i].preloadshader();
        }

        void preloadmeshes()
        {
            if(meshes) meshes->preload(this);
        }

        virtual void getdefaultanim(animinfo &info, int anim, uint varseed, dynent *d)
        {
            info.frame = 0;
            info.range = 1;
        }

        bool calcanim(int animpart, int anim, int basetime, int basetime2, float speed, float speed2, dynent *d, int interp, animinfo &info, int &aitime)
        {
            uint varseed = uint((size_t)d);
            info.anim = anim;
            info.basetime = basetime;
            info.varseed = varseed;
            info.speed = speed*100.f;
            if((anim&ANIM_INDEX)==ANIM_ALL)
            {
                info.frame = 0;
                info.range = meshes->totalframes();
            }
            else
            {
                animspec *spec = NULL;
                float curspeed = speed;
                if(anims[animpart])
                {
                    vector<animspec> &primary = anims[animpart][anim&ANIM_INDEX];
                    if(primary.length()) spec = &primary[uint(varseed + basetime)%primary.length()];
                    if((anim>>ANIM_SECONDARY)&(ANIM_INDEX|ANIM_DIR))
                    {
                        vector<animspec> &secondary = anims[animpart][(anim>>ANIM_SECONDARY)&ANIM_INDEX];
                        if(secondary.length())
                        {
                            animspec &spec2 = secondary[uint(varseed + basetime2)%secondary.length()];
                            if(!spec || spec2.priority > spec->priority)
                            {
                                spec = &spec2;
                                info.anim >>= ANIM_SECONDARY;
                                info.basetime = basetime2;
                                curspeed = speed2;
                            }
                        }
                    }
                }
                if(spec)
                {
                    info.frame = spec->frame;
                    info.range = spec->range;
                    if(spec->speed>0) info.speed = 1000.0f/spec->speed*curspeed;
                }
                else getdefaultanim(info, anim, uint(varseed + info.basetime), d);
            }

            info.anim &= (1<<ANIM_SECONDARY)-1;
            info.anim |= anim&ANIM_FLAGS;
            if(info.anim&ANIM_LOOP)
            {
                info.anim &= ~ANIM_SETTIME;
                if(!info.basetime) info.basetime = -((int)(size_t)d&0xFFF);

                if(info.anim&ANIM_CLAMP)
                {
                    if(info.anim&ANIM_REVERSE) info.frame += info.range-1;
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
                if((info.anim&(ANIM_LOOP|ANIM_CLAMP))==ANIM_CLAMP) aitime = min(aitime, int(info.range*info.speed*0.5e-3f));
                void *ak = meshes->animkey();
                if(d->ragdoll && d->ragdoll->millis != lastmillis)
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

        void intersect(int anim, int basetime, int basetime2, float speed, float speed2, float pitch, const vec &axis, const vec &forward, modelstate *state, dynent *d, const vec &o, const vec &ray)
        {
            animstate as[MAXANIMPARTS];
            intersect(anim, basetime, basetime2, speed, speed2, pitch, axis, forward, state, d, o, ray, as);
        }

        void intersect(int anim, int basetime, int basetime2, float speed, float speed2, float pitch, const vec &axis, const vec &forward, modelstate *state, dynent *d, const vec &o, const vec &ray, animstate *as)
        {
            if((anim&ANIM_REUSE) != ANIM_REUSE) loopi(numanimparts)
            {
                animinfo info;
                int interp = d && index+numanimparts<=MAXANIMPARTS ? index+i : -1, aitime = animationinterpolationtime;
                if(!calcanim(i, anim, basetime, basetime2, speed, speed2, d, interp, info, aitime)) return;
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

            float resize = model->scale * sizescale;
            int oldpos = matrixpos;
            vec oaxis, oforward, oo, oray;
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
            if(this == model->parts[0] && !model->translate.iszero())
            {
                if(oldpos == matrixpos)
                {
                    ++matrixpos;
                    matrixstack[matrixpos] = matrixstack[matrixpos-1];
                }
                matrixstack[matrixpos].translate(model->translate, resize);
            }
            matrixstack[matrixpos].transposedtransformnormal(forward, oforward);
            matrixstack[matrixpos].transposedtransform(o, oo);
            oo.div(resize);
            matrixstack[matrixpos].transposedtransformnormal(ray, oray);

            intersectscale = resize;
            meshes->intersect(as, pitch, oaxis, oforward, state, d, this, oo, oray);

            if((anim&ANIM_REUSE) != ANIM_REUSE)
            {
                loopv(links)
                {
                    linkedpart &link = links[i];
                    if(!link.p) continue;
                    float oldsizescale = sizescale;
                    sizescale *= link.size;
                    link.matrix.translate(links[i].translate, model->scale * sizescale);

                    matrixpos++;
                    matrixstack[matrixpos].mul(matrixstack[matrixpos-1], link.matrix);

                    int nanim = anim, nbasetime = basetime, nbasetime2 = basetime2;
                    float nspeed = speed, nspeed2 = speed2;
                    if(link.anim>=0)
                    {
                        nanim = link.anim | (anim&ANIM_FLAGS);
                        nbasetime = link.basetime;
                        nbasetime2 = 0;
                        nspeed = link.speed;
                        nspeed2 = 1;
                    }
                    link.p->intersect(nanim, nbasetime, nbasetime2, nspeed, nspeed2, pitch, axis, forward, state, d, o, ray);
                    sizescale = oldsizescale;

                    matrixpos--;
                }
            }

            matrixpos = oldpos;
        }

        void render(int anim, int basetime, int basetime2, float speed, float speed2, float pitch, const vec &axis, const vec &forward, modelstate *state, dynent *d)
        {
            animstate as[MAXANIMPARTS];
            render(anim, basetime, basetime2, speed, speed2, pitch, axis, forward, state, d, as);
        }

        void render(int anim, int basetime, int basetime2, float speed, float speed2, float pitch, const vec &axis, const vec &forward, modelstate *state, dynent *d, animstate *as)
        {
            if((anim&ANIM_REUSE) != ANIM_REUSE) loopi(numanimparts)
            {
                animinfo info;
                int interp = d && index+numanimparts<=MAXANIMPARTS ? index+i : -1, aitime = animationinterpolationtime;
                if(!calcanim(i, anim, basetime, basetime2, speed, speed2, d, interp, info, aitime)) return;
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

            float resize = model->scale * sizescale;
            int oldpos = matrixpos;
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
            if(this == model->parts[0] && !model->translate.iszero())
            {
                if(oldpos == matrixpos)
                {
                    ++matrixpos;
                    matrixstack[matrixpos] = matrixstack[matrixpos-1];
                }
                matrixstack[matrixpos].translate(model->translate, resize);
            }
            matrixstack[matrixpos].transposedtransformnormal(forward, oforward);

            if(!(anim&ANIM_NORENDER))
            {
                matrix4 modelmatrix;
                modelmatrix.mul(shadowmapping ? shadowmatrix : camprojmatrix, matrixstack[matrixpos]);
                if(resize!=1) modelmatrix.scale(resize);
                GLOBALPARAM(modelmatrix, modelmatrix);

                if(!(anim&ANIM_NOSKIN))
                {
                    GLOBALPARAM(modelworld, matrix3(matrixstack[matrixpos]));

                    vec modelcamera;
                    matrixstack[matrixpos].transposedtransform(camera1->o, modelcamera);
                    modelcamera.div(resize);
                    GLOBALPARAM(modelcamera, modelcamera);
                }
                if(model->wind)
                {
                    vec pos = matrixstack[matrixpos].gettranslation();
                    int animdist = windanimdist + windanimfalloff;
                    float dist = camera1->o.dist(pos);
                    float falloff = 1.0f;

                    if(windanimfalloff)
                        falloff = 1.0f - clamp((dist-windanimdist)/windanimfalloff, 0.0f, 1.0f);

                    GLOBALPARAMF(windparams, max(1.0f - dist/animdist, 0.0f), d ? 0 : pos.magnitude());
                    GLOBALPARAM(windvec, getwind(pos, d).mul(resize * model->wind * falloff));
                }
            }

            meshes->render(as, pitch, oaxis, oforward, state, d, this);

            if((anim&ANIM_REUSE) != ANIM_REUSE)
            {
                loopv(links)
                {
                    linkedpart &link = links[i];
                    float oldsizescale = sizescale;
                    sizescale *= link.size;
                    link.matrix.translate(links[i].translate, model->scale * sizescale);
                    if(link.rotate.x) link.matrix.rotate_around_z(link.rotate.x*RAD);
                    if(link.rotate.z) link.matrix.rotate_around_x(-link.rotate.z*RAD);
                    if(link.rotate.y) link.matrix.rotate_around_y(link.rotate.y*RAD);

                    matrixpos++;
                    matrixstack[matrixpos].mul(matrixstack[matrixpos-1], link.matrix);

                    if(link.pos) *link.pos = matrixstack[matrixpos].gettranslation();

                    if(!link.p)
                    {
                        matrixpos--;
                        continue;
                    }

                    int nanim = anim, nbasetime = basetime, nbasetime2 = basetime2;
                    float nspeed = speed, nspeed2 = speed2;
                    if(link.anim>=0)
                    {
                        nanim = link.anim | (anim&ANIM_FLAGS);
                        nbasetime = link.basetime;
                        nbasetime2 = 0;
                        nspeed = link.speed;
                        nspeed2 = 1;
                    }

                    link.p->render(nanim, nbasetime, nbasetime2, nspeed, nspeed2, pitch, axis, forward, state, d);
                    sizescale = oldsizescale;
                    matrixpos--;
                }
            }

            matrixpos = oldpos;
        }

        void setanim(int animpart, int num, int frame, int range, float speed, int priority = 0)
        {
            if(animpart<0 || animpart>=MAXANIMPARTS || num<0 || num>=game::numanims()) return;
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

        bool animated() const
        {
            loopi(MAXANIMPARTS) if(anims[i]) return true;
            return false;
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

    virtual int linktype(animmodel *m, part *p) const { return LINK_TAG; }

    void intersect(int anim, int basetime, int basetime2, float speed, float speed2, float pitch, const vec &axis, const vec &forward, modelstate *state, dynent *d, const vec &o, const vec &ray)
    {
        int numtags = 0;
        if(state->attached)
        {
            int index = parts.last()->index + parts.last()->numanimparts;
            for(int i = 0; state->attached[i].tag; i++)
            {
                numtags++;

                animmodel *m = (animmodel *)state->attached[i].m;
                if(!m) continue;
                part *p = m->parts[0];
                switch(linktype(m, p))
                {
                    case LINK_TAG:
                        p->index = link(p, state->attached[i].tag, vec(0, 0, 0), vec(0, 0, 0), state->attached[i].anim, state->attached[i].basetime, state->attached[i].sizescale, state->attached[i].speed, state->attached[i].pos) ? index : -1;
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
        parts[0]->intersect(anim, basetime, basetime2, speed, speed2, pitch, axis, forward, state, d, o, ray, as);

        for(int i = 1; i < parts.length(); i++)
        {
            part *p = parts[i];
            switch(linktype(this, p))
            {
                case LINK_COOP:
                    p->intersect(anim, basetime, basetime2, speed, speed2, pitch, axis, forward, state, d, o, ray);
                    break;

                case LINK_REUSE:
                    p->intersect(anim | ANIM_REUSE, basetime, basetime2, speed, speed2, pitch, axis, forward, state, d, o, ray, as);
                    break;
            }
        }

        if(state->attached) for(int i = numtags-1; i >= 0; i--)
        {
            animmodel *m = (animmodel *)state->attached[i].m;
            if(!m) continue;
            part *p = m->parts[0];
            switch(linktype(m, p))
            {
                case LINK_TAG:
                    if(p->index >= 0) unlink(p);
                    p->index = 0;
                    break;

                case LINK_COOP:
                    p->intersect(anim, basetime, basetime2, speed, speed2, pitch, axis, forward, state, d, o, ray);
                    p->index = 0;
                    break;

                case LINK_REUSE:
                    p->intersect(anim | ANIM_REUSE, basetime, basetime2, speed, speed2, pitch, axis, forward, state, d, o, ray, as);
                    break;
            }
        }
    }

    static int intersectresult, intersectmode;
    static float intersectdist, intersectscale;

    int intersect(int anim, modelstate *state, dynent *d, const vec &o, const vec &ray, float &dist, int mode)
    {
        vec axis(1, 0, 0), forward(0, 1, 0);

        matrixpos = 0;
        matrixstack[0].identity();
        if(!d || !d->ragdoll || d->ragdoll->millis == lastmillis)
        {
            if(!state->lastspin)
            {
                float secs = lastmillis/1000.0f;
                state->yaw += spinyaw*secs;
                state->pitch += spinpitch*secs;
                state->roll += spinroll*secs;
                state->lastspin = lastmillis;
            }

            matrixstack[0].settranslation(state->o);
            matrixstack[0].rotate_around_z(state->yaw*RAD);
            bool usepitch = pitched();
            if(state->roll && !usepitch) matrixstack[0].rotate_around_y(-state->roll*RAD);
            matrixstack[0].transformnormal(vec(axis), axis);
            matrixstack[0].transformnormal(vec(forward), forward);
            if(state->roll && usepitch) matrixstack[0].rotate_around_y(-state->roll*RAD);
            if(offsetyaw) matrixstack[0].rotate_around_z(offsetyaw*RAD);
            if(offsetpitch) matrixstack[0].rotate_around_x(offsetpitch*RAD);
            if(offsetroll) matrixstack[0].rotate_around_y(-offsetroll*RAD);
        }
        else
        {
            matrixstack[0].settranslation(d->ragdoll->center);
            state->pitch = 0;
        }

        sizescale = state->size;
        intersectresult = -1;
        intersectmode = mode;
        intersectdist = dist;

        intersect(anim, state->basetime, state->basetime2, state->speed, state->speed2, state->pitch, axis, forward, state, d, o, ray);

        if(intersectresult >= 0) dist = intersectdist;
        return intersectresult;
    }

    void render(int anim, int basetime, int basetime2, float speed, float speed2, float pitch, const vec &axis, const vec &forward, modelstate *state, dynent *d)
    {
        int numtags = 0;
        if(state->attached)
        {
            int index = parts.last()->index + parts.last()->numanimparts;
            for(int i = 0; state->attached[i].tag; i++)
            {
                numtags++;

                animmodel *m = (animmodel *)state->attached[i].m;
                if(!m)
                {
                    if(state->attached[i].pos) link(NULL, state->attached[i].tag, vec(0, 0, 0), vec(0, 0, 0), 0, 0, state->attached[i].sizescale, state->attached[i].speed, state->attached[i].pos);
                    continue;
                }
                part *p = m->parts[0];
                switch(linktype(m, p))
                {
                    case LINK_TAG:
                        p->index = link(p, state->attached[i].tag, vec(0, 0, 0),
                            vec(m->offsetyaw + m->spinyaw*lastmillis/1000.0f, m->offsetpitch + m->spinpitch*lastmillis/1000.0f, m->offsetroll + m->spinroll*lastmillis/1000.0f),
                                state->attached[i].anim, state->attached[i].basetime, state->attached[i].sizescale, state->attached[i].speed, state->attached[i].pos) ? index : -1;
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
        parts[0]->render(anim, basetime, basetime2, speed, speed2, pitch, axis, forward, state, d, as);

        for(int i = 1; i < parts.length(); i++)
        {
            part *p = parts[i];
            switch(linktype(this, p))
            {
                case LINK_COOP:
                    p->render(anim, basetime, basetime2, speed, speed2, pitch, axis, forward, state, d);
                    break;

                case LINK_REUSE:
                    p->render(anim | ANIM_REUSE, basetime, basetime2, speed, speed2, pitch, axis, forward, state, d, as);
                    break;
            }
        }

        if(state->attached) for(int i = numtags-1; i >= 0; i--)
        {
            animmodel *m = (animmodel *)state->attached[i].m;
            if(!m)
            {
                if(state->attached[i].pos) unlink(NULL);
                continue;
            }
            part *p = m->parts[0];
            switch(linktype(m, p))
            {
                case LINK_TAG:
                    if(p->index >= 0) unlink(p);
                    p->index = 0;
                    break;

                case LINK_COOP:
                    p->render(anim, basetime, basetime2, speed, speed2, pitch, axis, forward, state, d);
                    p->index = 0;
                    break;

                case LINK_REUSE:
                    p->render(anim | ANIM_REUSE, basetime, basetime2, speed, speed2, pitch, axis, forward, state, d, as);
                    break;
            }
        }
    }

    void render(int anim, modelstate *state, dynent *d)
    {
        vec axis(1, 0, 0), forward(0, 1, 0);

        matrixpos = 0;
        matrixstack[0].identity();
        if(!d || !d->ragdoll || d->ragdoll->millis == lastmillis)
        {
            if(!state->lastspin)
            {
                float secs = lastmillis/1000.0f;
                state->yaw += spinyaw*secs;
                state->pitch += spinpitch*secs;
                state->roll += spinroll*secs;
                state->lastspin = lastmillis;
            }

            matrixstack[0].settranslation(state->o);
            matrixstack[0].rotate_around_z(state->yaw*RAD);
            bool usepitch = pitched();
            if(state->roll && !usepitch) matrixstack[0].rotate_around_y(-state->roll*RAD);
            matrixstack[0].transformnormal(vec(axis), axis);
            matrixstack[0].transformnormal(vec(forward), forward);
            if(state->roll && usepitch) matrixstack[0].rotate_around_y(-state->roll*RAD);
            if(offsetyaw) matrixstack[0].rotate_around_z(offsetyaw*RAD);
            if(offsetpitch) matrixstack[0].rotate_around_x(offsetpitch*RAD);
            if(offsetroll) matrixstack[0].rotate_around_y(-offsetroll*RAD);
        }
        else
        {
            matrixstack[0].settranslation(d->ragdoll->center);
            state->pitch = 0;
        }

        sizescale = state->size;

        if(anim&ANIM_NORENDER)
        {
            render(anim, state->basetime, state->basetime2, state->speed, state->speed2, state->pitch, axis, forward, state, d);
            if(d) d->lastrendered = lastmillis;
            return;
        }

        if(!(anim&ANIM_NOSKIN))
        {
            bool invalidate = false;
            if(colorscale != state->color)
            {
                colorscale = state->color;
                invalidate = true;
            }
            if(memcmp(modelmaterial, state->material, sizeof(state->material)))
            {
                memcpy(modelmaterial, state->material, sizeof(state->material));
                invalidate = true;
            }
            if(matbright != state->matbright)
            {
                matbright = state->matbright;
                invalidate = true;
            }
            if(state->mixer && state->mixer != notexture)
            {
                if(mixercolor != state->mixercolor)
                {
                    mixercolor = state->mixercolor;
                    invalidate = true;
                }
                if(mixerglow != state->mixerglow)
                {
                    mixerglow = state->mixerglow;
                    invalidate = true;
                }
                if(mixerscroll != state->mixerscroll)
                {
                    mixerscroll = state->mixerscroll;
                    invalidate = true;
                }
            }
            if(state->pattern && state->pattern != notexture)
            {
                if(patternscale != state->patternscale)
                {
                    patternscale = state->patternscale;
                    invalidate = true;
                }
            }

            if(invalidate) shaderparamskey::invalidate();

            if(envmapped()) closestenvmaptex = lookupenvmap(closestenvmap(state->o));
            else if(state->attached) for(int i = 0; state->attached[i].tag; i++) if(state->attached[i].m && state->attached[i].m->envmapped())
            {
                closestenvmaptex = lookupenvmap(closestenvmap(state->o));
                break;
            }
        }

        if(depthoffset && !enabledepthoffset)
        {
            enablepolygonoffset(GL_POLYGON_OFFSET_FILL);
            enabledepthoffset = true;
        }

        render(anim, state->basetime, state->basetime2, state->speed, state->speed2, state->pitch, axis, forward, state, d);

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

    virtual void flushpart() {}

    part &addpart()
    {
        flushpart();
        part *p = new part(this, parts.length());
        parts.add(p);
        return *p;
    }

    void initmatrix(matrix4x3 &m)
    {
        m.identity();
        if(offsetyaw) m.rotate_around_z(offsetyaw*RAD);
        if(offsetpitch) m.rotate_around_x(offsetpitch*RAD);
        if(offsetroll) m.rotate_around_y(-offsetroll*RAD);
        m.translate(translate, scale);
    }

    void genBIH(vector<BIH::mesh> &bih)
    {
        if(parts.empty()) return;
        matrix4x3 m;
        initmatrix(m);
        parts[0]->genBIH(bih, m);
        for(int i = 1; i < parts.length(); i++)
        {
            part *p = parts[i];
            switch(linktype(this, p))
            {
                case LINK_COOP:
                case LINK_REUSE:
                    p->genBIH(bih, m);
                    break;
            }
        }
    }

    void genshadowmesh(vector<triangle> &tris, const matrix4x3 &orient)
    {
        if(parts.empty()) return;
        matrix4x3 m;
        initmatrix(m);
        m.mul(orient, matrix4x3(m));
        parts[0]->genshadowmesh(tris, m);
        for(int i = 1; i < parts.length(); i++)
        {
            part *p = parts[i];
            switch(linktype(this, p))
            {
                case LINK_COOP:
                case LINK_REUSE:
                    p->genshadowmesh(tris, m);
                    break;
            }
        }
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

    bool link(part *p, const char *tag, const vec &translate = vec(0, 0, 0), const vec &rotate = vec(0, 0, 0), int anim = -1, int basetime = 0, float size = 1, float speed = 1, vec *pos = NULL)
    {
        if(parts.empty()) return false;
        return parts[0]->link(p, tag, translate, rotate, anim, basetime, size, speed, pos);
    }

    bool unlink(part *p)
    {
        if(parts.empty()) return false;
        return parts[0]->unlink(p);
    }

    bool envmapped() const
    {
        loopv(parts) loopvj(parts[i]->skins) if(parts[i]->skins[j].envmapped()) return true;
        return false;
    }

    bool animated() const
    {
        if(spinyaw || spinpitch || spinroll || wind) return true;
        loopv(parts) if(parts[i]->animated()) return true;
        return false;
    }

    bool pitched() const
    {
        return parts[0]->pitchscale != 0;
    }

    bool alphatested() const
    {
        loopv(parts) if(parts[i]->alphatested()) return true;
        return false;
    }

    virtual bool flipy() const { return false; }
    virtual bool loadconfig() { return false; }
    virtual bool loaddefaultparts() { return false; }
    virtual void startload() {}
    virtual void endload() {}

    bool load()
    {
        startload();
        bool success = loadconfig() && parts.length(); // configured model, will call the model commands below
        if(!success)
            success = loaddefaultparts(); // model without configuration, try default tris and skin
        flushpart();
        endload();
        if(flipy()) translate.y = -translate.y;

        if(!success) return false;
        loopv(parts) if(!parts[i]->meshes) return false;

        loaded();
        return true;
    }

    void preloadshaders()
    {
        loopv(parts) parts[i]->preloadshaders();
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

    void setgloss(int gloss)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].gloss = gloss;
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

    void setalphatest(float alphatest)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].alphatest = alphatest;
    }

    void setfullbright(float fullbright)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].fullbright = fullbright;
    }

    void setcullface(int cullface)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].cullface = cullface;
    }

    void setcolor(const vec &color)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins) parts[i]->skins[j].color = color;
    }

    void setmaterial(int material1, int material2)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins)
        {
            skin &s = parts[i]->skins[j];
            s.material1 = material1;
            s.material2 = material2;
        }
    }

    void setmixer(bool val)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins)
        {
            skin &s = parts[i]->skins[j];
            if(val) s.flags |= skin::ALLOW_MIXER;
            else s.flags &= ~skin::ALLOW_MIXER;
        }
    }

    void setpattern(bool val)
    {
        if(parts.empty()) loaddefaultparts();
        loopv(parts) loopvj(parts[i]->skins)
        {
            skin &s = parts[i]->skins[j];
            if(val) s.flags |= skin::ALLOW_PATTERN;
            else s.flags &= ~skin::ALLOW_PATTERN;
        }
    }

    void calcbb(vec &center, vec &radius)
    {
        if(parts.empty()) return;
        vec bbmin(1e16f, 1e16f, 1e16f), bbmax(-1e16f, -1e16f, -1e16f);
        matrix4x3 m;
        initmatrix(m);
        parts[0]->calcbb(bbmin, bbmax, m);
        for(int i = 1; i < parts.length(); i++)
        {
            part *p = parts[i];
            switch(linktype(this, p))
            {
                case LINK_COOP:
                case LINK_REUSE:
                    p->calcbb(bbmin, bbmax, m);
                    break;
            }
        }
        radius = bbmax;
        radius.sub(bbmin);
        radius.mul(0.5f);
        center = bbmin;
        center.add(radius);
    }

    void calctransform(matrix4x3 &m)
    {
        initmatrix(m);
        m.scale(scale);
    }

    virtual void loaded()
    {
        loopv(parts) parts[i]->loaded();
    }

    static bool enabletc, enablecullface, enabletangents, enablebones, enabledepthoffset, enablecolor;
    static float sizescale;
    static vec4 colorscale, mixercolor;
    static vec2 matbright, mixerglow, mixerscroll;
    static float patternscale;
    static bvec modelmaterial[MAXMDLMATERIALS];
    static GLuint lastvbuf, lasttcbuf, lastxbuf, lastbbuf, lastebuf, lastcolbuf, lastenvmaptex, closestenvmaptex;
    static Texture *lasttex, *lastdecal, *lastmasks, *lastmixer, *lastpattern, *lastnormalmap;
    static int matrixpos;
    static matrix4 matrixstack[64];

    void startrender()
    {
        enabletc = enabletangents = enablebones = enabledepthoffset = enablecolor = false;
        enablecullface = true;
        lastvbuf = lasttcbuf = lastxbuf = lastbbuf = lastebuf = lastcolbuf = lastenvmaptex = closestenvmaptex = 0;
        lasttex = lastdecal = lastmasks = lastmixer = lastpattern = lastnormalmap = NULL;
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

    static void disablecolor()
    {
        gle::disablecolor();
        enablecolor = false;
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
        if(enabletangents) disabletangents();
        if(enablebones) disablebones();
        if(enablecolor) disablecolor();
        lastvbuf = lasttcbuf = lastxbuf = lastbbuf = lastebuf = lastcolbuf = 0;
    }

    void endrender()
    {
        if(lastvbuf || lastebuf) disablevbo();
        if(!enablecullface) glEnable(GL_CULL_FACE);
        if(enabledepthoffset) disablepolygonoffset(GL_POLYGON_OFFSET_FILL);
    }

    struct lodmdl
    {
        char *name;
        float dist;

        lodmdl() : name(NULL), dist(0) {}
        ~lodmdl() { DELETEA(name); }
    };
    vector<lodmdl> lod;

    void addlod(const char *str, float dist)
    {
        if(!str || !*str || dist <= 0) return;
        loopv(lod) if(!strcmp(lod[i].name, str) || lod[i].dist == dist) return;
        defformatstring(s, "%s/%s", name, str);
        lodmdl &lm = lod.add();
        lm.name = newstring(s);
        lm.dist = dist;
    }

    const char *lodmodel(float dist)
    {
        if(dist <= 0) return NULL;
        int id = -1;
        loopv(lod) if(dist >= lod[i].dist && (id < 0 || lod[i].dist > lod[id].dist)) id = i;
        return lod.inrange(id) ? lod[id].name : NULL;
    }
};

hashnameset<animmodel::meshgroup *> animmodel::meshgroups;
int animmodel::intersectresult = -1, animmodel::intersectmode = 0;
float animmodel::intersectdist = 0, animmodel::intersectscale = 1;
bool animmodel::enabletc = false, animmodel::enabletangents = false, animmodel::enablebones = false,
     animmodel::enablecullface = true, animmodel::enabledepthoffset = false, animmodel::enablecolor = false;
float animmodel::sizescale = 1;
vec4 animmodel::colorscale(1, 1, 1, 1), animmodel::mixercolor(1, 1, 1, 1);
vec2 animmodel::matbright(1, 1), animmodel::mixerglow(0, 0), animmodel::mixerscroll(0, 0);
float animmodel::patternscale = 1;
bvec animmodel::modelmaterial[MAXMDLMATERIALS] = { bvec(255, 255, 255), bvec(255, 255, 255), bvec(255, 255, 255) };
GLuint animmodel::lastvbuf = 0, animmodel::lasttcbuf = 0, animmodel::lastxbuf = 0, animmodel::lastbbuf = 0, animmodel::lastebuf = 0,
       animmodel::lastcolbuf = 0, animmodel::lastenvmaptex = 0, animmodel::closestenvmaptex = 0;
Texture *animmodel::lasttex = NULL, *animmodel::lastdecal = NULL, *animmodel::lastmasks = NULL, *animmodel::lastmixer = NULL, *animmodel::lastpattern = NULL, *animmodel::lastnormalmap = NULL;
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

template<class MDL, class BASE> struct modelloader : BASE
{
    static MDL *loading;
    static string dir;

    modelloader(const char *name) : BASE(name) {}

    static bool cananimate() { return true; }
    static bool multiparted() { return true; }
    static bool multimeshed() { return true; }

    void startload()
    {
        loading = (MDL *)this;
    }

    void endload()
    {
        loading = NULL;
    }

    bool loadconfig()
    {
        formatstring(dir, "%s", BASE::name);
        defformatstring(cfgname, "%s/%s.cfg", BASE::name, MDL::formatname());
        return execfile(cfgname, false);
    }
};

template<class MDL, class BASE> MDL *modelloader<MDL, BASE>::loading = NULL;
template<class MDL, class BASE> string modelloader<MDL, BASE>::dir = {'\0'}; // crashes clang if "" is used here

template<class MDL, class MESH> struct modelcommands
{
    typedef struct MDL::part part;
    typedef struct MDL::skin skin;

    static void setdir(char *name)
    {
        if(!MDL::loading) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        formatstring(MDL::dir, "%s", name);
    }

    #define loopmeshes(meshname, m, body) do { \
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; } \
        part &mdl = *MDL::loading->parts.last(); \
        if(!mdl.meshes) return; \
        loopv(mdl.meshes->meshes) \
        { \
            MESH &m = *(MESH *)mdl.meshes->meshes[i]; \
            if(cubepattern(m.name, meshname) >= 0) \
            { \
                body; \
            } \
        } \
    } while(0)

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

    static void setspec(char *meshname, float *percent)
    {
        float spec = *percent > 0 ? *percent/100.0f : 0.0f;
        loopskins(meshname, s, s.spec = spec);
    }

    static void setgloss(char *meshname, int *gloss)
    {
        loopskins(meshname, s, s.gloss = clamp(*gloss, 0, 2));
    }

    static void setglow(char *meshname, float *percent, float *delta, float *pulse)
    {
        float glow = *percent > 0 ? *percent/100.0f : 0.0f, glowdelta = *delta/100.0f, glowpulse = *pulse > 0 ? *pulse/1000.0f : 0;
        glowdelta -= glow;
        loopskins(meshname, s, { s.glow = glow; s.glowdelta = glowdelta; s.glowpulse = glowpulse; });
    }

    static void setalphatest(char *meshname, float *cutoff)
    {
        loopskins(meshname, s, s.alphatest = max(0.0f, min(1.0f, *cutoff)));
    }

    static void setcullface(char *meshname, int *cullface)
    {
        loopskins(meshname, s, s.cullface = *cullface);
    }

    static void setcolor(char *meshname, float *r, float *g, float *b)
    {
        loopskins(meshname, s, s.color = vec(*r, *g, *b));
    }

    static void setenvmap(char *meshname, char *envmap, float *envmapmin, float *envmapmax)
    {
        Texture *tex = cubemapload(envmap);
        loopskins(meshname, s,
        {
            s.envmap = tex;
            if(*envmapmin >= 0) s.envmapmin = *envmapmin;
            if(*envmapmax >= 0) s.envmapmax = *envmapmax;
        });
    }

    static void setbumpmap(char *meshname, char *normalmapfile)
    {
        Texture *normalmaptex = textureload(makerelpath(MDL::dir, normalmapfile), 0, true, false);
        loopskins(meshname, s, s.normalmap = normalmaptex);
    }

    static void setdecal(char *meshname, char *decal)
    {
        loopskins(meshname, s,
            s.decal = textureload(makerelpath(MDL::dir, decal), 0, true, false);
        );
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

    static void settricollide(char *meshname)
    {
        bool init = true;
        loopmeshes("*", m, { if(!m.cancollide) init = false; });
        if(init) loopmeshes("*", m, m.cancollide = false);
        loopmeshes(meshname, m, { m.cancollide = true; m.canrender = false; });
        MDL::loading->collide = COLLIDE_TRI;
    }

    static void setmaterial(char *meshname, int *material1, int *material2)
    {
        loopskins(meshname, s, { s.material1 = clamp(*material1, 0, int(MAXMDLMATERIALS)); s.material2 = clamp(*material2, 0, int(MAXMDLMATERIALS)); });
    }

    static void setmixer(char *meshname, int *mixer)
    {
        loopskins(meshname, s, { if(*mixer) s.flags |= skin::ALLOW_MIXER; else s.flags &= ~skin::ALLOW_MIXER; });
    }

    static void setpattern(char *meshname, int *pattern)
    {
        loopskins(meshname, s, { if(*pattern) s.flags |= skin::ALLOW_PATTERN; else s.flags &= ~skin::ALLOW_PATTERN; });
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
            modelcommand(setspec, "spec", "sf");
            modelcommand(setgloss, "gloss", "si");
            modelcommand(setglow, "glow", "sfff");
            modelcommand(setalphatest, "alphatest", "sf");
            modelcommand(setcullface, "cullface", "si");
            modelcommand(setcolor, "color", "sfff");
            modelcommand(setenvmap, "envmap", "ssgg");
            modelcommand(setbumpmap, "bumpmap", "ss");
            modelcommand(setdecal, "decal", "ss");
            modelcommand(setfullbright, "fullbright", "sf");
            modelcommand(setshader, "shader", "ss");
            modelcommand(setscroll, "scroll", "sff");
            modelcommand(setnoclip, "noclip", "si");
            modelcommand(settricollide, "tricollide", "s");
            modelcommand(setmaterial, "material", "sii");
            modelcommand(setmixer, "mixer", "si");
            modelcommand(setpattern, "pattern", "si");
        }
        if(MDL::multiparted()) modelcommand(setlink, "link", "iisffffff");
    }
};

