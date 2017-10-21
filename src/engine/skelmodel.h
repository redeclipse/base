VAR(IDF_PERSIST, gpuskel, 0, 1, 1);

VAR(0, maxskelanimdata, 1, 192, 0);

#define BONEMASK_NOT  0x8000
#define BONEMASK_END  0xFFFF
#define BONEMASK_BONE 0x7FFF

struct skelhitdata;

struct skelmodel : animmodel
{
    struct vert { vec pos, norm; vec2 tc; quat tangent; int blend, interpindex; };
    struct vvert { vec pos; hvec2 tc; squat tangent; };
    struct vvertg { hvec4 pos; hvec2 tc; squat tangent; };
    struct vvertgw : vvertg { uchar weights[4]; uchar bones[4]; };
    struct tri { ushort vert[3]; };

    struct blendcombo
    {
        int uses, interpindex;
        float weights[4];
        uchar bones[4], interpbones[4];

        blendcombo() : uses(1)
        {
        }

        bool operator==(const blendcombo &c) const
        {
            loopk(4) if(bones[k] != c.bones[k]) return false;
            loopk(4) if(weights[k] != c.weights[k]) return false;
            return true;
        }

        int size() const
        {
            int i = 1;
            while(i < 4 && weights[i]) i++;
            return i;
        }

        static bool sortcmp(const blendcombo &x, const blendcombo &y)
        {
            loopi(4)
            {
                if(x.weights[i])
                {
                    if(!y.weights[i]) return true;
                }
                else if(y.weights[i]) return false;
                else break;
            }
            return false;
        }

        int addweight(int sorted, float weight, int bone)
        {
            if(weight <= 1e-3f) return sorted;
            loopk(sorted) if(weight > weights[k])
            {
                for(int l = min(sorted-1, 2); l >= k; l--)
                {
                    weights[l+1] = weights[l];
                    bones[l+1] = bones[l];
                }
                weights[k] = weight;
                bones[k] = bone;
                return sorted<4 ? sorted+1 : sorted;
            }
            if(sorted>=4) return sorted;
            weights[sorted] = weight;
            bones[sorted] = bone;
            return sorted+1;
        }

        void finalize(int sorted)
        {
            loopj(4-sorted) { weights[sorted+j] = 0; bones[sorted+j] = 0; }
            if(sorted <= 0) return;
            float total = 0;
            loopj(sorted) total += weights[j];
            total = 1.0f/total;
            loopj(sorted) weights[j] *= total;
        }

        template<class T>
        void serialize(T &v)
        {
            if(interpindex >= 0)
            {
                v.weights[0] = 255;
                loopk(3) v.weights[k+1] = 0;
                v.bones[0] = 2*interpindex;
                loopk(3) v.bones[k+1] = v.bones[0];
            }
            else
            {
                int total = 0;
                loopk(4) total += (v.weights[k] = uchar(0.5f + weights[k]*255));
                while(total > 255)
                {
                    loopk(4) if(v.weights[k] > 0 && total > 255) { v.weights[k]--; total--; }
                }
                while(total < 255)
                {
                    loopk(4) if(v.weights[k] < 255 && total < 255) { v.weights[k]++; total++; }
                }
                loopk(4) v.bones[k] = 2*interpbones[k];
            }
        }
    };


    struct animcacheentry
    {
        animstate as[MAXANIMPARTS];
        float pitch;
        int millis;
        uchar *partmask;
        ragdolldata *ragdoll;

        animcacheentry() : ragdoll(NULL)
        {
            loopk(MAXANIMPARTS) as[k].cur.fr1 = as[k].prev.fr1 = -1;
        }

        bool operator==(const animcacheentry &c) const
        {
            loopi(MAXANIMPARTS) if(as[i]!=c.as[i]) return false;
            return pitch==c.pitch && partmask==c.partmask && ragdoll==c.ragdoll && (!ragdoll || min(millis, c.millis) >= ragdoll->lastmove);
        }

        bool operator!=(const animcacheentry &c) const
        {
            return !operator==(c);
        }
    };

    struct vbocacheentry : animcacheentry
    {
        GLuint vbuf;
        int owner;

        vbocacheentry() : vbuf(0), owner(-1) {}
    };

    struct skelcacheentry : animcacheentry
    {
        dualquat *bdata;
        int version;

        skelcacheentry() : bdata(NULL), version(-1) {}

        void nextversion()
        {
            version = Shader::uniformlocversion();
        }
    };

    struct blendcacheentry : skelcacheentry
    {
        int owner;

        blendcacheentry() : owner(-1) {}
    };

    struct skelmeshgroup;

    struct skelmesh : mesh
    {
        vert *verts;
        tri *tris;
        int numverts, numtris, maxweights;

        int voffset, eoffset, elen;
        ushort minvert, maxvert;

        skelmesh() : verts(NULL), tris(NULL), numverts(0), numtris(0), maxweights(0)
        {
        }

        virtual ~skelmesh()
        {
            DELETEA(verts);
            DELETEA(tris);
        }

        int addblendcombo(const blendcombo &c)
        {
            maxweights = max(maxweights, c.size());
            return ((skelmeshgroup *)group)->addblendcombo(c);
        }

        void smoothnorms(float limit = 0, bool areaweight = true)
        {
            mesh::smoothnorms(verts, numverts, tris, numtris, limit, areaweight);
        }

        void buildnorms(bool areaweight = true)
        {
            mesh::buildnorms(verts, numverts, tris, numtris, areaweight);
        }

        void calctangents(bool areaweight = true)
        {
            mesh::calctangents(verts, verts, numverts, tris, numtris, areaweight);
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
            m.tc = (const uchar *)&verts->tc;
            m.tcstride = sizeof(vert);
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

        static inline void assignvert(vvertg &vv, int j, vert &v, blendcombo &c)
        {
            vv.pos = hvec4(v.pos, 1);
            vv.tc = v.tc;
            vv.tangent = v.tangent;
        }

        static inline void assignvert(vvertgw &vv, int j, vert &v, blendcombo &c)
        {
            vv.pos = hvec4(v.pos, 1);
            vv.tc = v.tc;
            vv.tangent = v.tangent;
            c.serialize(vv);
        }

        template<class T>
        int genvbo(vector<ushort> &idxs, int offset, vector<T> &vverts)
        {
            voffset = offset;
            eoffset = idxs.length();
            loopi(numverts)
            {
                vert &v = verts[i];
                assignvert(vverts.add(), i, v, ((skelmeshgroup *)group)->blendcombos[v.blend]);
            }
            loopi(numtris) loopj(3) idxs.add(voffset + tris[i].vert[j]);
            elen = idxs.length()-eoffset;
            minvert = voffset;
            maxvert = voffset + numverts-1;
            return numverts;
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
                    T vv;
                    assignvert(vv, index, v, ((skelmeshgroup *)group)->blendcombos[v.blend]);
                    int htidx = hthash(v.pos)&(htlen-1);
                    loopk(htlen)
                    {
                        int &vidx = htdata[(htidx+k)&(htlen-1)];
                        if(vidx < 0) { vidx = idxs.add(ushort(vverts.length())); vverts.add(vv); break; }
                        else if(!memcmp(&vverts[vidx], &vv, sizeof(vv))) { minvert = min(minvert, idxs.add(ushort(vidx))); break; }
                    }
                }
            }
            elen = idxs.length()-eoffset;
            minvert = min(minvert, ushort(voffset));
            maxvert = max(minvert, ushort(vverts.length()-1));
            return vverts.length()-voffset;
        }

        int genvbo(vector<ushort> &idxs, int offset)
        {
            loopi(numverts) verts[i].interpindex = ((skelmeshgroup *)group)->remapblend(verts[i].blend);

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
        static inline void fillvert(T &vv, int j, vert &v)
        {
            vv.tc = v.tc;
        }

        template<class T>
        void fillverts(T *vdata)
        {
            vdata += voffset;
            loopi(numverts) fillvert(vdata[i], i, verts[i]);
        }

        template<class T>
        void interpverts(const dualquat * RESTRICT bdata1, const dualquat * RESTRICT bdata2, T * RESTRICT vdata, skin &s)
        {
            const int blendoffset = ((skelmeshgroup *)group)->skel->numgpubones;
            bdata2 -= blendoffset;
            vdata += voffset;
            loopi(numverts)
            {
                const vert &src = verts[i];
                T &dst = vdata[i];
                const dualquat &b = (src.interpindex < blendoffset ? bdata1 : bdata2)[src.interpindex];
                dst.pos = b.transform(src.pos);
                quat q = b.transform(src.tangent);
                fixqtangent(q, src.tangent.w);
                dst.tangent = q;
            }
        }

        void setshader(Shader *s, int row)
        {
            skelmeshgroup *g = (skelmeshgroup *)group;
            if(row) s->setvariant(g->skel->usegpuskel ? min(maxweights, g->vweights) : 0, row);
            else if(g->skel->usegpuskel) s->setvariant(min(maxweights, g->vweights)-1, 0);
            else s->set();
        }

        void render(const animstate *as, skin &s, vbocacheentry &vc)
        {
            glDrawRangeElements_(GL_TRIANGLES, minvert, maxvert, elen, GL_UNSIGNED_SHORT, &((skelmeshgroup *)group)->edata[eoffset]);
            glde++;
            xtravertsva += numverts;
        }
    };


    struct tag
    {
        char *name;
        int bone;
        matrix4x3 matrix;

        tag() : name(NULL) {}
        ~tag() { DELETEA(name); }
    };

    struct skelanimspec
    {
        char *name;
        int frame, range;

        skelanimspec() : name(NULL), frame(0), range(0) {}
        ~skelanimspec()
        {
            DELETEA(name);
        }
    };

    struct boneinfo
    {
        const char *name;
        int parent, children, next, group, scheduled, interpindex, interpparent, ragdollindex, correctindex;
        float pitchscale, pitchoffset, pitchmin, pitchmax;
        dualquat base, invbase;

        boneinfo() : name(NULL), parent(-1), children(-1), next(-1), group(INT_MAX), scheduled(-1), interpindex(-1), interpparent(-1), ragdollindex(-1), correctindex(-1), pitchscale(0), pitchoffset(0), pitchmin(0), pitchmax(0) {}
        ~boneinfo()
        {
            DELETEA(name);
        }
    };

    struct antipode
    {
        int parent, child;

        antipode(int parent, int child) : parent(parent), child(child) {}
    };

    struct pitchdep
    {
        int bone, parent;
        dualquat pose;
    };

    struct pitchtarget
    {
        int bone, frame, corrects, deps;
        float pitchmin, pitchmax, deviated;
        dualquat pose;
    };

    struct pitchcorrect
    {
        int bone, target, parent;
        float pitchmin, pitchmax, pitchscale, pitchangle, pitchtotal;

        pitchcorrect() : parent(-1), pitchangle(0), pitchtotal(0) {}
    };

    struct skeleton
    {
        char *name;
        int shared;
        vector<skelmeshgroup *> users;
        boneinfo *bones;
        int numbones, numinterpbones, numgpubones, numframes;
        dualquat *framebones;
        vector<skelanimspec> skelanims;
        vector<tag> tags;
        vector<antipode> antipodes;
        ragdollskel *ragdoll;
        vector<pitchdep> pitchdeps;
        vector<pitchtarget> pitchtargets;
        vector<pitchcorrect> pitchcorrects;

        bool usegpuskel;
        vector<skelcacheentry> skelcache;
        hashtable<GLuint, int> blendoffsets;

        skeleton() : name(NULL), shared(0), bones(NULL), numbones(0), numinterpbones(0), numgpubones(0), numframes(0), framebones(NULL), ragdoll(NULL), usegpuskel(false), blendoffsets(32)
        {
        }

        ~skeleton()
        {
            DELETEA(name);
            DELETEA(bones);
            DELETEA(framebones);
            DELETEP(ragdoll);
            loopv(skelcache)
            {
                DELETEA(skelcache[i].bdata);
            }
        }

        skelanimspec *findskelanim(const char *name, char sep = '\0')
        {
            int len = sep ? strlen(name) : 0;
            loopv(skelanims)
            {
                if(skelanims[i].name)
                {
                    if(sep)
                    {
                        const char *end = strchr(skelanims[i].name, ':');
                        if(end && end - skelanims[i].name == len && !memcmp(name, skelanims[i].name, len)) return &skelanims[i];
                    }
                    if(!strcmp(name, skelanims[i].name)) return &skelanims[i];
                }
            }
            return NULL;
        }

        skelanimspec &addskelanim(const char *name)
        {
            skelanimspec &sa = skelanims.add();
            sa.name = name ? newstring(name) : NULL;
            return sa;
        }

        int findbone(const char *name)
        {
            loopi(numbones) if(bones[i].name && !strcmp(bones[i].name, name)) return i;
            return -1;
        }

        int findtag(const char *name)
        {
            loopv(tags) if(!strcmp(tags[i].name, name)) return i;
            return -1;
        }

        bool addtag(const char *name, int bone, const matrix4x3 &matrix)
        {
            int idx = findtag(name);
            if(idx >= 0)
            {
                if(!testtags) return false;
                tag &t = tags[idx];
                t.bone = bone;
                t.matrix = matrix;
            }
            else
            {
                tag &t = tags.add();
                t.name = newstring(name);
                t.bone = bone;
                t.matrix = matrix;
            }
            return true;
        }

        void calcantipodes()
        {
            antipodes.shrink(0);
            vector<int> schedule;
            loopi(numbones)
            {
                if(bones[i].group >= numbones)
                {
                    bones[i].scheduled = schedule.length();
                    schedule.add(i);
                }
                else bones[i].scheduled = -1;
            }
            loopv(schedule)
            {
                int bone = schedule[i];
                const boneinfo &info = bones[bone];
                loopj(numbones) if(abs(bones[j].group) == bone && bones[j].scheduled < 0)
                {
                    antipodes.add(antipode(info.interpindex, bones[j].interpindex));
                    bones[j].scheduled = schedule.length();
                    schedule.add(j);
                }
                if(i + 1 == schedule.length())
                {
                    int conflict = INT_MAX;
                    loopj(numbones) if(bones[j].group < numbones && bones[j].scheduled < 0) conflict = min(conflict, abs(bones[j].group));
                    if(conflict < numbones)
                    {
                        bones[conflict].scheduled = schedule.length();
                        schedule.add(conflict);
                    }
                }
            }
        }

        void remapbones()
        {
            loopi(numbones)
            {
                boneinfo &info = bones[i];
                info.interpindex = -1;
                info.ragdollindex = -1;
            }
            numgpubones = 0;
            loopv(users)
            {
                skelmeshgroup *group = users[i];
                loopvj(group->blendcombos)
                {
                    blendcombo &c = group->blendcombos[j];
                    loopk(4)
                    {
                        if(!c.weights[k]) { c.interpbones[k] = k > 0 ? c.interpbones[k-1] : 0; continue; }
                        boneinfo &info = bones[c.bones[k]];
                        if(info.interpindex < 0) info.interpindex = numgpubones++;
                        c.interpbones[k] = info.interpindex;
                        if(info.group < 0) continue;
                        loopl(4)
                        {
                            if(!c.weights[l]) break;
                            if(l == k) continue;
                            int parent = c.bones[l];
                            if(info.parent == parent || (info.parent >= 0 && info.parent == bones[parent].parent)) { info.group = -info.parent; break; }
                            if(info.group <= parent) continue;
                            int child = c.bones[k];
                            while(parent > child) parent = bones[parent].parent;
                            if(parent != child) info.group = c.bones[l];
                        }
                    }
                }
            }
            numinterpbones = numgpubones;
            loopv(tags)
            {
                boneinfo &info = bones[tags[i].bone];
                if(info.interpindex < 0) info.interpindex = numinterpbones++;
            }
            if(ragdoll)
            {
                loopv(ragdoll->joints)
                {
                    boneinfo &info = bones[ragdoll->joints[i].bone];
                    if(info.interpindex < 0) info.interpindex = numinterpbones++;
                    info.ragdollindex = i;
                }
            }
            loopi(numbones)
            {
                boneinfo &info = bones[i];
                if(info.interpindex < 0) continue;
                for(int parent = info.parent; parent >= 0 && bones[parent].interpindex < 0; parent = bones[parent].parent)
                    bones[parent].interpindex = numinterpbones++;
            }
            loopi(numbones)
            {
                boneinfo &info = bones[i];
                if(info.interpindex < 0) continue;
                info.interpparent = info.parent >= 0 ? bones[info.parent].interpindex : -1;
            }
            if(ragdoll)
            {
                loopi(numbones)
                {
                    boneinfo &info = bones[i];
                    if(info.interpindex < 0 || info.ragdollindex >= 0) continue;
                    for(int parent = info.parent; parent >= 0; parent = bones[parent].parent)
                    {
                        if(bones[parent].ragdollindex >= 0) { ragdoll->addreljoint(i, bones[parent].ragdollindex); break; }
                    }
                }
            }
            calcantipodes();
        }


        void addpitchdep(int bone, int frame)
        {
            for(; bone >= 0; bone = bones[bone].parent)
            {
                int pos = pitchdeps.length();
                loopvj(pitchdeps) if(bone <= pitchdeps[j].bone)
                {
                    if(bone == pitchdeps[j].bone) goto nextbone;
                    pos = j;
                    break;
                }
                {
                    pitchdep d;
                    d.bone = bone;
                    d.parent = -1;
                    d.pose = framebones[frame*numbones + bone];
                    pitchdeps.insert(pos, d);
                }
            nextbone:;
            }
        }

        int findpitchdep(int bone)
        {
            loopv(pitchdeps) if(bone <= pitchdeps[i].bone) return bone == pitchdeps[i].bone ? i : -1;
            return -1;
        }

        int findpitchcorrect(int bone)
        {
            loopv(pitchcorrects) if(bone <= pitchcorrects[i].bone) return bone == pitchcorrects[i].bone ? i : -1;
            return -1;
        }

        void initpitchdeps()
        {
            pitchdeps.setsize(0);
            if(pitchtargets.empty()) return;
            loopv(pitchtargets)
            {
                pitchtarget &t = pitchtargets[i];
                t.deps = -1;
                addpitchdep(t.bone, t.frame);
            }
            loopv(pitchdeps)
            {
                pitchdep &d = pitchdeps[i];
                int parent = bones[d.bone].parent;
                if(parent >= 0)
                {
                    int j = findpitchdep(parent);
                    if(j >= 0)
                    {
                        d.parent = j;
                        d.pose.mul(pitchdeps[j].pose, dualquat(d.pose));
                    }
                }
            }
            loopv(pitchtargets)
            {
                pitchtarget &t = pitchtargets[i];
                int j = findpitchdep(t.bone);
                if(j >= 0)
                {
                    t.deps = j;
                    t.pose = pitchdeps[j].pose;
                }
                t.corrects = -1;
                for(int parent = t.bone; parent >= 0; parent = bones[parent].parent)
                {
                    t.corrects = findpitchcorrect(parent);
                    if(t.corrects >= 0) break;
                }
            }
            loopv(pitchcorrects)
            {
                pitchcorrect &c = pitchcorrects[i];
                bones[c.bone].correctindex = i;
                c.parent = -1;
                for(int parent = c.bone;;)
                {
                    parent = bones[parent].parent;
                    if(parent < 0) break;
                    c.parent = findpitchcorrect(parent);
                    if(c.parent >= 0) break;
                }
            }
        }

        void optimize()
        {
            cleanup();
            if(ragdoll) ragdoll->setup();
            remapbones();
            initpitchdeps();
        }

        void expandbonemask(uchar *expansion, int bone, int val)
        {
            expansion[bone] = val;
            bone = bones[bone].children;
            while(bone>=0) { expandbonemask(expansion, bone, val); bone = bones[bone].next; }
        }

        void applybonemask(ushort *mask, uchar *partmask, int partindex)
        {
            if(!mask || *mask==BONEMASK_END) return;
            uchar *expansion = new uchar[numbones];
            memset(expansion, *mask&BONEMASK_NOT ? 1 : 0, numbones);
            while(*mask!=BONEMASK_END)
            {
                expandbonemask(expansion, *mask&BONEMASK_BONE, *mask&BONEMASK_NOT ? 0 : 1);
                mask++;
            }
            loopi(numbones) if(expansion[i]) partmask[i] = partindex;
            delete[] expansion;
        }

        void linkchildren()
        {
            loopi(numbones)
            {
                boneinfo &b = bones[i];
                b.children = -1;
                if(b.parent<0) b.next = -1;
                else
                {
                    b.next = bones[b.parent].children;
                    bones[b.parent].children = i;
                }
            }
        }

        int availgpubones() const { return min(maxvsuniforms, maxskelanimdata) / 2; }
        bool gpuaccelerate() const { return numframes && gpuskel && numgpubones<=availgpubones(); }

        float calcdeviation(const vec &axis, const vec &forward, const dualquat &pose1, const dualquat &pose2)
        {
            vec forward1 = pose1.transformnormal(forward).project(axis).normalize(),
                forward2 = pose2.transformnormal(forward).project(axis).normalize(),
                daxis = vec().cross(forward1, forward2);
            float dx = clamp(forward1.dot(forward2), -1.0f, 1.0f), dy = clamp(daxis.magnitude(), -1.0f, 1.0f);
            if(daxis.dot(axis) < 0) dy = -dy;
            return atan2f(dy, dx)/RAD;
        }

        void calcpitchcorrects(float pitch, const vec &axis, const vec &forward)
        {
            loopv(pitchtargets)
            {
                pitchtarget &t = pitchtargets[i];
                t.deviated = calcdeviation(axis, forward, t.pose, pitchdeps[t.deps].pose);
            }
            loopv(pitchcorrects)
            {
                pitchcorrect &c = pitchcorrects[i];
                c.pitchangle = c.pitchtotal = 0;
            }
            loopvj(pitchtargets)
            {
                pitchtarget &t = pitchtargets[j];
                float tpitch = pitch - t.deviated;
                for(int parent = t.corrects; parent >= 0; parent = pitchcorrects[parent].parent)
                    tpitch -= pitchcorrects[parent].pitchangle;
                if(t.pitchmin || t.pitchmax) tpitch = clamp(tpitch, t.pitchmin, t.pitchmax);
                loopv(pitchcorrects)
                {
                    pitchcorrect &c = pitchcorrects[i];
                    if(c.target != j) continue;
                    float total = c.parent >= 0 ? pitchcorrects[c.parent].pitchtotal : 0,
                          avail = tpitch - total,
                          used = tpitch*c.pitchscale;
                    if(c.pitchmin || c.pitchmax)
                    {
                        if(used < 0) used = clamp(c.pitchmin, used, 0.0f);
                        else used = clamp(c.pitchmax, 0.0f, used);
                    }
                    if(used < 0) used = clamp(avail, used, 0.0f);
                    else used = clamp(avail, 0.0f, used);
                    c.pitchangle = used;
                    c.pitchtotal = used + total;
                }
            }
        }

        #define INTERPBONE(bone) \
            const animstate &s = as[partmask[bone]]; \
            const framedata &f = partframes[partmask[bone]]; \
            dualquat d; \
            (d = f.fr1[bone]).mul((1-s.cur.t)*s.interp); \
            d.accumulate(f.fr2[bone], s.cur.t*s.interp); \
            if(s.interp<1) \
            { \
                d.accumulate(f.pfr1[bone], (1-s.prev.t)*(1-s.interp)); \
                d.accumulate(f.pfr2[bone], s.prev.t*(1-s.interp)); \
            }

        void interpbones(const animstate *as, float pitch, const vec &axis, const vec &forward, int numanimparts, const uchar *partmask, skelcacheentry &sc)
        {
            if(!sc.bdata) sc.bdata = new dualquat[numinterpbones];
            sc.nextversion();
            struct framedata
            {
                const dualquat *fr1, *fr2, *pfr1, *pfr2;
            } partframes[MAXANIMPARTS];
            loopi(numanimparts)
            {
                partframes[i].fr1 = &framebones[as[i].cur.fr1*numbones];
                partframes[i].fr2 = &framebones[as[i].cur.fr2*numbones];
                if(as[i].interp<1)
                {
                    partframes[i].pfr1 = &framebones[as[i].prev.fr1*numbones];
                    partframes[i].pfr2 = &framebones[as[i].prev.fr2*numbones];
                }
            }
            loopv(pitchdeps)
            {
                pitchdep &p = pitchdeps[i];
                INTERPBONE(p.bone);
                d.normalize();
                if(p.parent >= 0) p.pose.mul(pitchdeps[p.parent].pose, d);
                else p.pose = d;
            }
            calcpitchcorrects(pitch, axis, forward);
            loopi(numbones) if(bones[i].interpindex>=0)
            {
                INTERPBONE(i);
                d.normalize();
                const boneinfo &b = bones[i];
                if(b.interpparent<0) sc.bdata[b.interpindex] = d;
                else sc.bdata[b.interpindex].mul(sc.bdata[b.interpparent], d);
                float angle;
                if(b.pitchscale) { angle = b.pitchscale*pitch + b.pitchoffset; if(b.pitchmin || b.pitchmax) angle = clamp(angle, b.pitchmin, b.pitchmax); }
                else if(b.correctindex >= 0) angle = pitchcorrects[b.correctindex].pitchangle;
                else continue;
                if(as->cur.anim&ANIM_NOPITCH || (as->interp < 1 && as->prev.anim&ANIM_NOPITCH))
                    angle *= (as->cur.anim&ANIM_NOPITCH ? 0 : as->interp) + (as->interp < 1 && as->prev.anim&ANIM_NOPITCH ? 0 : 1-as->interp);
                sc.bdata[b.interpindex].mulorient(quat(axis, angle*RAD), b.base);
            }
            loopv(antipodes) sc.bdata[antipodes[i].child].fixantipodal(sc.bdata[antipodes[i].parent]);
        }

        void initragdoll(ragdolldata &d, skelcacheentry &sc, part *p)
        {
            const dualquat *bdata = sc.bdata;
            loopv(ragdoll->joints)
            {
                const ragdollskel::joint &j = ragdoll->joints[i];
                const boneinfo &b = bones[j.bone];
                const dualquat &q = bdata[b.interpindex];
                loopk(3) if(j.vert[k] >= 0)
                {
                    ragdollskel::vert &v = ragdoll->verts[j.vert[k]];
                    ragdolldata::vert &dv = d.verts[j.vert[k]];
                    dv.pos.add(q.transform(v.pos).mul(v.weight));
                }
            }
            if(ragdoll->animjoints) loopv(ragdoll->joints)
            {
                const ragdollskel::joint &j = ragdoll->joints[i];
                const boneinfo &b = bones[j.bone];
                const dualquat &q = bdata[b.interpindex];
                d.calcanimjoint(i, matrix4x3(q));
            }
            loopv(ragdoll->verts)
            {
                ragdolldata::vert &dv = d.verts[i];
                matrixstack[matrixpos].transform(vec(dv.pos).mul(p->model->scale), dv.pos);
            }
            loopv(ragdoll->reljoints)
            {
                const ragdollskel::reljoint &r = ragdoll->reljoints[i];
                const ragdollskel::joint &j = ragdoll->joints[r.parent];
                const boneinfo &br = bones[r.bone], &bj = bones[j.bone];
                d.reljoints[i].mul(dualquat(bdata[bj.interpindex]).invert(), bdata[br.interpindex]);
            }
        }

        void genragdollbones(ragdolldata &d, skelcacheentry &sc, part *p)
        {
            if(!sc.bdata) sc.bdata = new dualquat[numinterpbones];
            sc.nextversion();
            vec trans = vec(d.center).div(p->model->scale).add(p->model->translate);
            loopv(ragdoll->joints)
            {
                const ragdollskel::joint &j = ragdoll->joints[i];
                const boneinfo &b = bones[j.bone];
                vec pos(0, 0, 0);
                loopk(3) if(j.vert[k]>=0) pos.add(d.verts[j.vert[k]].pos);
                pos.mul(j.weight/p->model->scale).sub(trans);
                matrix4x3 m;
                m.mul(d.tris[j.tri], pos, d.animjoints ? d.animjoints[i] : j.orient);
                sc.bdata[b.interpindex] = dualquat(m);
            }
            loopv(ragdoll->reljoints)
            {
                const ragdollskel::reljoint &r = ragdoll->reljoints[i];
                const ragdollskel::joint &j = ragdoll->joints[r.parent];
                const boneinfo &br = bones[r.bone], &bj = bones[j.bone];
                sc.bdata[br.interpindex].mul(sc.bdata[bj.interpindex], d.reljoints[i]);
            }
            loopv(antipodes) sc.bdata[antipodes[i].child].fixantipodal(sc.bdata[antipodes[i].parent]);
        }

        void concattagtransform(part *p, int i, const matrix4x3 &m, matrix4x3 &n)
        {
            matrix4x3 t;
            t.mul(bones[tags[i].bone].base, tags[i].matrix);
            n.mul(m, t);
        }

        void calctags(part *p, skelcacheentry *sc = NULL)
        {
            loopv(p->links)
            {
                linkedpart &l = p->links[i];
                tag &t = tags[l.tag];
                dualquat q;
                if(sc) q.mul(sc->bdata[bones[t.bone].interpindex], bones[t.bone].base);
                else q = bones[t.bone].base;
                matrix4x3 m;
                m.mul(q, t.matrix);
                m.d.mul(p->model->scale * sizescale);
                l.matrix = m;
            }
        }

        void cleanup(bool full = true)
        {
            loopv(skelcache)
            {
                skelcacheentry &sc = skelcache[i];
                loopj(MAXANIMPARTS) sc.as[j].cur.fr1 = -1;
                DELETEA(sc.bdata);
            }
            skelcache.setsize(0);
            blendoffsets.clear();
            if(full) loopv(users) users[i]->cleanup();
        }

        bool canpreload() { return !numframes || gpuaccelerate(); }

        void preload()
        {
            if(!numframes) return;
            if(skelcache.empty())
            {
                usegpuskel = gpuaccelerate();
            }
        }

        skelcacheentry &checkskelcache(part *p, const animstate *as, float pitch, const vec &axis, const vec &forward, ragdolldata *rdata)
        {
            if(skelcache.empty())
            {
                usegpuskel = gpuaccelerate();
            }

            int numanimparts = ((skelpart *)as->owner)->numanimparts;
            uchar *partmask = ((skelpart *)as->owner)->partmask;
            skelcacheentry *sc = NULL;
            bool match = false;
            loopv(skelcache)
            {
                skelcacheentry &c = skelcache[i];
                loopj(numanimparts) if(c.as[j]!=as[j]) goto mismatch;
                if(c.pitch != pitch || c.partmask != partmask || c.ragdoll != rdata || (rdata && c.millis < rdata->lastmove)) goto mismatch;
                match = true;
                sc = &c;
                break;
            mismatch:
                if(c.millis < lastmillis) { sc = &c; break; }
            }
            if(!sc) sc = &skelcache.add();
            if(!match)
            {
                loopi(numanimparts) sc->as[i] = as[i];
                sc->pitch = pitch;
                sc->partmask = partmask;
                sc->ragdoll = rdata;
                if(rdata) genragdollbones(*rdata, *sc, p);
                else interpbones(as, pitch, axis, forward, numanimparts, partmask, *sc);
            }
            sc->millis = lastmillis;
            return *sc;
        }

        int getblendoffset(UniformLoc &u)
        {
            int &offset = blendoffsets.access(Shader::lastshader->program, -1);
            if(offset < 0)
            {
                defformatstring(offsetname, "%s[%d]", u.name, 2*numgpubones);
                offset = glGetUniformLocation_(Shader::lastshader->program, offsetname);
            }
            return offset;
        }

        void setglslbones(UniformLoc &u, skelcacheentry &sc, skelcacheentry &bc, int count)
        {
            if(u.version == bc.version && u.data == bc.bdata) return;
            glUniform4fv_(u.loc, 2*numgpubones, sc.bdata[0].real.v);
            if(count > 0)
            {
                int offset = getblendoffset(u);
                if(offset >= 0) glUniform4fv_(offset, 2*count, bc.bdata[0].real.v);
            }
            u.version = bc.version;
            u.data = bc.bdata;
        }

        void setgpubones(skelcacheentry &sc, blendcacheentry *bc, int count)
        {
            if(!Shader::lastshader) return;
            if(Shader::lastshader->uniformlocs.length() < 1) return;
            UniformLoc &u = Shader::lastshader->uniformlocs[0];
            setglslbones(u, sc, bc ? *bc : sc, count);
        }

        bool shouldcleanup() const
        {
            return numframes && (skelcache.empty() || gpuaccelerate()!=usegpuskel);
        }
    };

    static hashnameset<skeleton *> skeletons;

    struct skelmeshgroup : meshgroup
    {
        skeleton *skel;

        vector<blendcombo> blendcombos;
        int numblends[4];

        static const int MAXBLENDCACHE = 16;
        blendcacheentry blendcache[MAXBLENDCACHE];

        static const int MAXVBOCACHE = 16;
        vbocacheentry vbocache[MAXVBOCACHE];

        ushort *edata;
        GLuint ebuf;
        int vlen, vertsize, vblends, vweights;
        uchar *vdata;

        skelhitdata *hitdata;

        skelmeshgroup() : skel(NULL), edata(NULL), ebuf(0), vlen(0), vertsize(0), vblends(0), vweights(0), vdata(NULL), hitdata(NULL)
        {
            memset(numblends, 0, sizeof(numblends));
        }

        virtual ~skelmeshgroup()
        {
            if(skel)
            {
                if(skel->shared) skel->users.removeobj(this);
                else DELETEP(skel);
            }
            if(ebuf) glDeleteBuffers_(1, &ebuf);
            loopi(MAXBLENDCACHE)
            {
                DELETEA(blendcache[i].bdata);
            }
            loopi(MAXVBOCACHE)
            {
                if(vbocache[i].vbuf) glDeleteBuffers_(1, &vbocache[i].vbuf);
            }
            DELETEA(vdata);
            deletehitdata();
        }

        void shareskeleton(const char *name)
        {
            if(!name)
            {
                skel = new skeleton;
                skel->users.add(this);
                return;
            }

            if(skeletons.access(name)) skel = skeletons[name];
            else
            {
                skel = new skeleton;
                skel->name = newstring(name);
                skeletons.add(skel);
            }
            skel->users.add(this);
            skel->shared++;
        }

        int findtag(const char *name)
        {
            return skel->findtag(name);
        }

        void *animkey() { return skel; }
        int totalframes() const { return max(skel->numframes, 1); }

        virtual skelanimspec *loadanim(const char *filename) { return NULL; }

        void genvbo(vbocacheentry &vc)
        {
            if(!vc.vbuf) glGenBuffers_(1, &vc.vbuf);
            if(ebuf) return;

            vector<ushort> idxs;

            vlen = 0;
            vblends = 0;
            if(skel->numframes && !skel->usegpuskel)
            {
                vweights = 1;
                loopv(blendcombos)
                {
                    blendcombo &c = blendcombos[i];
                    c.interpindex = c.weights[1] ? skel->numgpubones + vblends++ : -1;
                }

                vertsize = sizeof(vvert);
                looprendermeshes(skelmesh, m, vlen += m.genvbo(idxs, vlen));
                DELETEA(vdata);
                vdata = new uchar[vlen*vertsize];
                looprendermeshes(skelmesh, m,
                {
                    m.fillverts((vvert *)vdata);
                });
            }
            else
            {
                if(skel->numframes)
                {
                    vweights = 4;
                    int availbones = skel->availgpubones() - skel->numgpubones;
                    while(vweights > 1 && availbones >= numblends[vweights-1]) availbones -= numblends[--vweights];
                    loopv(blendcombos)
                    {
                        blendcombo &c = blendcombos[i];
                        c.interpindex = c.size() > vweights ? skel->numgpubones + vblends++ : -1;
                    }
                }
                else
                {
                    vweights = 0;
                    loopv(blendcombos) blendcombos[i].interpindex = -1;
                }

                gle::bindvbo(vc.vbuf);
                #define GENVBO(type, args) \
                    do \
                    { \
                        vertsize = sizeof(type); \
                        vector<type> vverts; \
                        looprendermeshes(skelmesh, m, vlen += m.genvbo args); \
                        glBufferData_(GL_ARRAY_BUFFER, vverts.length()*sizeof(type), vverts.getbuf(), GL_STATIC_DRAW); \
                    } while(0)
                #define GENVBOANIM(type) GENVBO(type, (idxs, vlen, vverts))
                #define GENVBOSTAT(type) GENVBO(type, (idxs, vlen, vverts, htdata, htlen))
                if(skel->numframes) GENVBOANIM(vvertgw);
                else
                {
                    int numverts = 0, htlen = 128;
                    looprendermeshes(skelmesh, m, numverts += m.numverts);
                    while(htlen < numverts) htlen *= 2;
                    if(numverts*4 > htlen*3) htlen *= 2;
                    int *htdata = new int[htlen];
                    memset(htdata, -1, htlen*sizeof(int));
                    GENVBOSTAT(vvertg);
                    delete[] htdata;
                }
                #undef GENVBO
                #undef GENVBOANIM
                #undef GENVBOSTAT
                gle::clearvbo();
            }

            glGenBuffers_(1, &ebuf);
            gle::bindebo(ebuf);
            glBufferData_(GL_ELEMENT_ARRAY_BUFFER, idxs.length()*sizeof(ushort), idxs.getbuf(), GL_STATIC_DRAW);
            gle::clearebo();
        }

        template<class T>
        void bindbones(T *vverts) { if(enablebones) disablebones(); }
        void bindbones(vvertgw *vverts) { meshgroup::bindbones(vverts->weights, vverts->bones, vertsize); }

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
            bindbones(vverts);
        }

        void bindvbo(const animstate *as, part *p, vbocacheentry &vc, skelcacheentry *sc = NULL, blendcacheentry *bc = NULL)
        {
            if(!skel->numframes) bindvbo<vvertg>(as, p, vc);
            else if(skel->usegpuskel) bindvbo<vvertgw>(as, p, vc);
            else bindvbo<vvert>(as, p, vc);
        }

        void concattagtransform(part *p, int i, const matrix4x3 &m, matrix4x3 &n)
        {
            skel->concattagtransform(p, i, m, n);
        }

        int addblendcombo(const blendcombo &c)
        {
            loopv(blendcombos) if(blendcombos[i]==c)
            {
                blendcombos[i].uses += c.uses;
                return i;
            }
            numblends[c.size()-1]++;
            blendcombo &a = blendcombos.add(c);
            return a.interpindex = blendcombos.length()-1;
        }

        void sortblendcombos()
        {
            blendcombos.sort(blendcombo::sortcmp);
            int *remap = new int[blendcombos.length()];
            loopv(blendcombos) remap[blendcombos[i].interpindex] = i;
            looprendermeshes(skelmesh, m,
            {
                loopj(m.numverts)
                {
                    vert &v = m.verts[j];
                    v.blend = remap[v.blend];
                }
            });
            delete[] remap;
        }

        int remapblend(int blend)
        {
            const blendcombo &c = blendcombos[blend];
            return c.weights[1] ? c.interpindex : c.interpbones[0];
        }

        static inline void blendbones(dualquat &d, const dualquat *bdata, const blendcombo &c)
        {
            d = bdata[c.interpbones[0]];
            d.mul(c.weights[0]);
            d.accumulate(bdata[c.interpbones[1]], c.weights[1]);
            if(c.weights[2])
            {
                d.accumulate(bdata[c.interpbones[2]], c.weights[2]);
                if(c.weights[3]) d.accumulate(bdata[c.interpbones[3]], c.weights[3]);
            }
        }

        void blendbones(const skelcacheentry &sc, blendcacheentry &bc)
        {
            bc.nextversion();
            if(!bc.bdata) bc.bdata = new dualquat[vblends];
            dualquat *dst = bc.bdata - skel->numgpubones;
            bool normalize = !skel->usegpuskel || vweights<=1;
            loopv(blendcombos)
            {
                const blendcombo &c = blendcombos[i];
                if(c.interpindex<0) break;
                dualquat &d = dst[c.interpindex];
                blendbones(d, sc.bdata, c);
                if(normalize) d.normalize();
            }
        }

        static inline void blendbones(const dualquat *bdata, dualquat *dst, const blendcombo *c, int numblends)
        {
            loopi(numblends)
            {
                dualquat &d = dst[i];
                blendbones(d, bdata, c[i]);
                d.normalize();
            }
        }

        void cleanup()
        {
            loopi(MAXBLENDCACHE)
            {
                blendcacheentry &c = blendcache[i];
                DELETEA(c.bdata);
                c.owner = -1;
            }
            loopi(MAXVBOCACHE)
            {
                vbocacheentry &c = vbocache[i];
                if(c.vbuf) { glDeleteBuffers_(1, &c.vbuf); c.vbuf = 0; }
                c.owner = -1;
            }
            if(ebuf) { glDeleteBuffers_(1, &ebuf); ebuf = 0; }
            if(skel) skel->cleanup(false);
            cleanuphitdata();
        }

        #define SEARCHCACHE(cachesize, cacheentry, cache, reusecheck) \
            loopi(cachesize) \
            { \
                cacheentry &c = cache[i]; \
                if(c.owner==owner) \
                { \
                     if(c==sc) return c; \
                     else c.owner = -1; \
                     break; \
                } \
            } \
            loopi(cachesize-1) \
            { \
                cacheentry &c = cache[i]; \
                if(reusecheck c.owner < 0 || c.millis < lastmillis) \
                    return c; \
            } \
            return cache[cachesize-1];

        vbocacheentry &checkvbocache(skelcacheentry &sc, int owner)
        {
            SEARCHCACHE(MAXVBOCACHE, vbocacheentry, vbocache, !c.vbuf || );
        }

        blendcacheentry &checkblendcache(skelcacheentry &sc, int owner)
        {
            SEARCHCACHE(MAXBLENDCACHE, blendcacheentry, blendcache, )
        }

        void cleanuphitdata();
        void deletehitdata();
        void buildhitdata(const uchar *hitzones);
        void intersect(skelhitdata *z, part *p, const skelmodel::skelcacheentry &sc, const vec &o, const vec &ray);

        void intersect(const animstate *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p, const vec &o, const vec &ray)
        {
            if(!hitdata) return;

            if(skel->shouldcleanup()) skel->cleanup();

            skelcacheentry &sc = skel->checkskelcache(p, as, pitch, axis, forward, !d || !d->ragdoll || d->ragdoll->skel != skel->ragdoll || d->ragdoll->millis == lastmillis ? NULL : d->ragdoll);

            intersect(hitdata, p, sc, o, ray);

            skel->calctags(p, &sc);
        }

        void preload(part *p)
        {
            if(!skel->canpreload()) return;
            if(skel->shouldcleanup()) skel->cleanup();
            skel->preload();
            if(!vbocache->vbuf) genvbo(*vbocache);
        }

        void render(const animstate *as, float pitch, const vec &axis, const vec &forward, dynent *d, part *p)
        {
            if(skel->shouldcleanup()) { skel->cleanup(); disablevbo(); }

            if(!skel->numframes)
            {
                if(!(as->cur.anim&ANIM_NORENDER))
                {
                    if(!vbocache->vbuf) genvbo(*vbocache);
                    bindvbo(as, p, *vbocache);
                    looprendermeshes(skelmesh, m,
                    {
                        p->skins[i].bind(m, as);
                        m.render(as, p->skins[i], *vbocache);
                    });
                }
                skel->calctags(p);
                return;
            }

            skelcacheentry &sc = skel->checkskelcache(p, as, pitch, axis, forward, !d || !d->ragdoll || d->ragdoll->skel != skel->ragdoll || d->ragdoll->millis == lastmillis ? NULL : d->ragdoll);
            if(!(as->cur.anim&ANIM_NORENDER))
            {
                int owner = &sc-&skel->skelcache[0];
                vbocacheentry &vc = skel->usegpuskel ? *vbocache : checkvbocache(sc, owner);
                vc.millis = lastmillis;
                if(!vc.vbuf) genvbo(vc);
                blendcacheentry *bc = NULL;
                if(vblends)
                {
                    bc = &checkblendcache(sc, owner);
                    bc->millis = lastmillis;
                    if(bc->owner!=owner)
                    {
                        bc->owner = owner;
                        *(animcacheentry *)bc = sc;
                        blendbones(sc, *bc);
                    }
                }
                if(!skel->usegpuskel && vc.owner != owner)
                {
                    vc.owner = owner;
                    (animcacheentry &)vc = sc;
                    looprendermeshes(skelmesh, m,
                    {
                        m.interpverts(sc.bdata, bc ? bc->bdata : NULL, (vvert *)vdata, p->skins[i]);
                    });
                    gle::bindvbo(vc.vbuf);
                    glBufferData_(GL_ARRAY_BUFFER, vlen*vertsize, vdata, GL_STREAM_DRAW);
                }

                bindvbo(as, p, vc, &sc, bc);

                looprendermeshes(skelmesh, m,
                {
                    p->skins[i].bind(m, as);
                    if(skel->usegpuskel) skel->setgpubones(sc, bc, vblends);
                    m.render(as, p->skins[i], vc);
                });
            }

            skel->calctags(p, &sc);

            if(as->cur.anim&ANIM_RAGDOLL && skel->ragdoll && !d->ragdoll)
            {
                d->ragdoll = new ragdolldata(skel->ragdoll, p->model->scale);
                skel->initragdoll(*d->ragdoll, sc, p);
                d->ragdoll->init(d);
            }
        }

        virtual bool load(const char *name, float smooth) = 0;
    };

    virtual skelmeshgroup *newmeshes() = 0;

    meshgroup *loadmeshes(const char *name, const char *skelname = NULL, float smooth = 2)
    {
        skelmeshgroup *group = newmeshes();
        group->shareskeleton(skelname);
        if(!group->load(name, smooth)) { delete group; return NULL; }
        return group;
    }

    meshgroup *sharemeshes(const char *name, const char *skelname = NULL, float smooth = 2)
    {
        if(!meshgroups.access(name))
        {
            meshgroup *group = loadmeshes(name, skelname, smooth);
            if(!group) return NULL;
            meshgroups.add(group);
        }
        return meshgroups[name];
    }

    struct animpartmask
    {
        animpartmask *next;
        int numbones;
        uchar bones[1];
    };

    struct skelpart : part
    {
        animpartmask *buildingpartmask;

        uchar *partmask;

        skelpart(animmodel *model, int index = 0) : part(model, index), buildingpartmask(NULL), partmask(NULL)
        {
        }

        virtual ~skelpart()
        {
            DELETEA(buildingpartmask);
        }

        uchar *sharepartmask(animpartmask *o)
        {
            static animpartmask *partmasks = NULL;
            animpartmask *p = partmasks;
            for(; p; p = p->next) if(p->numbones==o->numbones && !memcmp(p->bones, o->bones, p->numbones))
            {
                delete[] (uchar *)o;
                return p->bones;
            }

            o->next = p;
            partmasks = o;
            return o->bones;
        }

        animpartmask *newpartmask()
        {
            animpartmask *p = (animpartmask *)new uchar[sizeof(animpartmask) + ((skelmeshgroup *)meshes)->skel->numbones-1];
            p->numbones = ((skelmeshgroup *)meshes)->skel->numbones;
            memset(p->bones, 0, p->numbones);
            return p;
        }

        void initanimparts()
        {
            DELETEA(buildingpartmask);
            buildingpartmask = newpartmask();
        }

        bool addanimpart(ushort *bonemask)
        {
            if(!buildingpartmask || numanimparts>=MAXANIMPARTS) return false;
            ((skelmeshgroup *)meshes)->skel->applybonemask(bonemask, buildingpartmask->bones, numanimparts);
            numanimparts++;
            return true;
        }

        void endanimparts()
        {
            if(buildingpartmask)
            {
                partmask = sharepartmask(buildingpartmask);
                buildingpartmask = NULL;
            }

            ((skelmeshgroup *)meshes)->skel->optimize();
        }

        void loaded()
        {
            endanimparts();
            part::loaded();
        }
    };

    skelmodel(const char *name) : animmodel(name)
    {
    }

    int linktype(animmodel *m, part *p) const
    {
        return type()==m->type() &&
            ((skelmeshgroup *)parts[0]->meshes)->skel == ((skelmeshgroup *)p->meshes)->skel ?
                LINK_REUSE :
                LINK_TAG;
    }

    bool skeletal() const { return true; }

    skelpart &addpart()
    {
        skelpart *p = new skelpart(this, parts.length());
        parts.add(p);
        return *p;
    }
};

hashnameset<skelmodel::skeleton *> skelmodel::skeletons;

struct skeladjustment
{
    float yaw, pitch, roll;
    vec translate;

    skeladjustment(float yaw, float pitch, float roll, const vec &translate) : yaw(yaw), pitch(pitch), roll(roll), translate(translate) {}

    void adjust(dualquat &dq)
    {
        if(yaw) dq.mulorient(quat(vec(0, 0, 1), yaw*RAD));
        if(pitch) dq.mulorient(quat(vec(0, -1, 0), pitch*RAD));
        if(roll) dq.mulorient(quat(vec(-1, 0, 0), roll*RAD));
        if(!translate.iszero()) dq.translate(translate);
    }
};

template<class MDL> struct skelloader : modelloader<MDL>
{
    static vector<skeladjustment> adjustments;
    static vector<uchar> hitzones;

    void flushpart()
    {
        if(MDL::loading && MDL::loading->parts.length())
        {
            skelmodel::skelpart *p = (skelmodel::skelpart *)MDL::loading->parts.last();
            skelmodel::skelmeshgroup *m = (skelmodel::skelmeshgroup *)p->meshes;
            if(hitzones.length() && m) m->buildhitdata(hitzones.getbuf());
        }
        adjustments.setsize(0);
        hitzones.setsize(0);
    }
};

template<class MDL> vector<skeladjustment> skelloader<MDL>::adjustments;
template<class MDL> vector<uchar> skelloader<MDL>::hitzones;

template<class MDL> struct skelcommands : modelcommands<MDL, struct MDL::skelmesh>
{
    typedef modelcommands<MDL, struct MDL::skelmesh> commands;
    typedef struct MDL::skeleton skeleton;
    typedef struct MDL::skelmeshgroup meshgroup;
    typedef struct MDL::skelpart part;
    typedef struct MDL::skin skin;
    typedef struct MDL::boneinfo boneinfo;
    typedef struct MDL::skelanimspec animspec;
    typedef struct MDL::pitchdep pitchdep;
    typedef struct MDL::pitchtarget pitchtarget;
    typedef struct MDL::pitchcorrect pitchcorrect;

    static void loadpart(char *meshfile, char *skelname, float *smooth)
    {
        if(!MDL::loading) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        defformatstring(filename, "%s/%s", MDL::dir, meshfile);
        part &mdl = MDL::loading->addpart();
        MDL::adjustments.setsize(0);
        mdl.meshes = MDL::loading->sharemeshes(path(filename), skelname[0] ? skelname : NULL, *smooth > 0 ? cosf(clamp(*smooth, 0.0f, 180.0f)*RAD) : 2);
        if(!mdl.meshes) conoutf("\frCould not load %s", filename);
        else
        {
            if(mdl.meshes && ((meshgroup *)mdl.meshes)->skel->numbones > 0) mdl.disablepitch();  
            mdl.initanimparts();
            mdl.initskins();
        }
    }

    static void settag(char *name, char *tagname, float *tx, float *ty, float *tz, float *rx, float *ry, float *rz)
    {
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        part &mdl = *(part *)MDL::loading->parts.last();
        int i = mdl.meshes ? ((meshgroup *)mdl.meshes)->skel->findbone(name) : -1;
        if(i >= 0)
        {
            float cx = *rx ? cosf(*rx/2*RAD) : 1, sx = *rx ? sinf(*rx/2*RAD) : 0,
                  cy = *ry ? cosf(*ry/2*RAD) : 1, sy = *ry ? sinf(*ry/2*RAD) : 0,
                  cz = *rz ? cosf(*rz/2*RAD) : 1, sz = *rz ? sinf(*rz/2*RAD) : 0;
            matrix4x3 m(matrix3(quat(sx*cy*cz - cx*sy*sz, cx*sy*cz + sx*cy*sz, cx*cy*sz - sx*sy*cz, cx*cy*cz + sx*sy*sz)),
                        vec(*tx, *ty, *tz));
            ((meshgroup *)mdl.meshes)->skel->addtag(tagname, i, m);
            return;
        }
        conoutf("\frCould not find bone %s for tag %s", name, tagname);
    }

    static void setpitch(char *name, float *pitchscale, float *pitchoffset, float *pitchmin, float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        part &mdl = *(part *)MDL::loading->parts.last();

        if(name[0])
        {
            int i = mdl.meshes ? ((meshgroup *)mdl.meshes)->skel->findbone(name) : -1;
            if(i>=0)
            {
                boneinfo &b = ((meshgroup *)mdl.meshes)->skel->bones[i];
                b.pitchscale = *pitchscale;
                b.pitchoffset = *pitchoffset;
                if(*pitchmin || *pitchmax)
                {
                    b.pitchmin = *pitchmin;
                    b.pitchmax = *pitchmax;
                }
                else
                {
                    b.pitchmin = -360*fabs(b.pitchscale) + b.pitchoffset;
                    b.pitchmax = 360*fabs(b.pitchscale) + b.pitchoffset;
                }
                return;
            }
            conoutf("\frCould not find bone %s to pitch", name);
            return;
        }

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

    static void setpitchtarget(char *name, char *animfile, int *frameoffset, float *pitchmin, float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        part &mdl = *(part *)MDL::loading->parts.last();
        if(!mdl.meshes) return;
        defformatstring(filename, "%s/%s", MDL::dir, animfile);
        animspec *sa = ((meshgroup *)mdl.meshes)->loadanim(path(filename));
        if(!sa) { conoutf("\frCould not load %s anim file %s", MDL::formatname(), filename); return; }
        skeleton *skel = ((meshgroup *)mdl.meshes)->skel;
        int bone = skel ? skel->findbone(name) : -1;
        if(bone < 0)
        {
            conoutf("\frCould not find bone %s to pitch target", name);
            return;
        }
        loopv(skel->pitchtargets) if(skel->pitchtargets[i].bone == bone) return;
        pitchtarget &t = skel->pitchtargets.add();
        t.bone = bone;
        t.frame = sa->frame + clamp(*frameoffset, 0, sa->range-1);
        t.pitchmin = *pitchmin;
        t.pitchmax = *pitchmax;
    }

    static void setpitchcorrect(char *name, char *targetname, float *scale, float *pitchmin, float *pitchmax)
    {
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        part &mdl = *(part *)MDL::loading->parts.last();
        if(!mdl.meshes) return;
        skeleton *skel = ((meshgroup *)mdl.meshes)->skel;
        int bone = skel ? skel->findbone(name) : -1;
        if(bone < 0)
        {
            conoutf("\frCould not find bone %s to pitch correct", name);
            return;
        }
        if(skel->findpitchcorrect(bone) >= 0) return;
        int targetbone = skel->findbone(targetname), target = -1;
        if(targetbone >= 0) loopv(skel->pitchtargets) if(skel->pitchtargets[i].bone == targetbone) { target = i; break; }
        if(target < 0)
        {
            conoutf("\frCould not find pitch target %s to pitch correct %s", targetname, name);
            return;
        }
        pitchcorrect c;
        c.bone = bone;
        c.target = target;
        c.pitchmin = *pitchmin;
        c.pitchmax = *pitchmax;
        c.pitchscale = *scale;
        int pos = skel->pitchcorrects.length();
        loopv(skel->pitchcorrects) if(bone <= skel->pitchcorrects[i].bone) { pos = i; break; }
        skel->pitchcorrects.insert(pos, c);
    }

    static void setanim(char *anim, char *animfile, float *speed, int *priority, int *startoffset, int *endoffset)
    {
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; }

        vector<int> anims;
        game::findanims(anim, anims);
        if(anims.empty()) conoutf("\frCould not find animation %s", anim);
        else
        {
            part *p = (part *)MDL::loading->parts.last();
            if(!p->meshes) return;
            defformatstring(filename, "%s/%s", MDL::dir, animfile);
            animspec *sa = ((meshgroup *)p->meshes)->loadanim(path(filename));
            if(!sa) conoutf("\frCould not load %s anim file %s", MDL::formatname(), filename);
            else loopv(anims)
            {
                int start = sa->frame, end = sa->range;
                if(*startoffset > 0) start += min(*startoffset, end-1);
                else if(*startoffset < 0) start += max(end + *startoffset, 0);
                end -= start - sa->frame;
                if(*endoffset > 0) end = min(end, *endoffset);
                else if(*endoffset < 0) end = max(end + *endoffset, 1);
                MDL::loading->parts.last()->setanim(p->numanimparts-1, anims[i], start, end, *speed, *priority);
            }
        }
    }

    static void setanimpart(char *maskstr)
    {
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; }

        part *p = (part *)MDL::loading->parts.last();

        vector<char *> bonestrs;
        explodelist(maskstr, bonestrs);
        vector<ushort> bonemask;
        loopv(bonestrs)
        {
            char *bonestr = bonestrs[i];
            int bone = p->meshes ? ((meshgroup *)p->meshes)->skel->findbone(bonestr[0]=='!' ? bonestr+1 : bonestr) : -1;
            if(bone<0) { conoutf("\frCould not find bone %s for anim part mask [%s]", bonestr, maskstr); bonestrs.deletearrays(); return; }
            bonemask.add(bone | (bonestr[0]=='!' ? BONEMASK_NOT : 0));
        }
        bonestrs.deletearrays();
        bonemask.sort();
        if(bonemask.length()) bonemask.add(BONEMASK_END);

        if(!p->addanimpart(bonemask.getbuf())) conoutf("\frToo many animation parts");
    }

    static void setadjust(char *name, float *yaw, float *pitch, float *roll, float *tx, float *ty, float *tz)
    {
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        part &mdl = *(part *)MDL::loading->parts.last();

        if(!name[0]) return;
        int i = mdl.meshes ? ((meshgroup *)mdl.meshes)->skel->findbone(name) : -1;
        if(i < 0) {  conoutf("\frCould not find bone %s to adjust", name); return; }
        while(!MDL::adjustments.inrange(i)) MDL::adjustments.add(skeladjustment(0, 0, 0, vec(0, 0, 0)));
        MDL::adjustments[i] = skeladjustment(*yaw, *pitch, *roll, vec(*tx/4, *ty/4, *tz/4));
    }

    static void sethitzone(int *id, char *maskstr)
    {
        if(!MDL::loading || MDL::loading->parts.empty()) { conoutf("\frNot loading an %s", MDL::formatname()); return; }
        if(*id >= 0x80) { conoutf("\frInvalid hit zone id %d", *id); return; }

        part *p = (part *)MDL::loading->parts.last();
        meshgroup *m = (meshgroup *)p->meshes;
        if(!m || m->hitdata) return;

        vector<char *> bonestrs;
        explodelist(maskstr, bonestrs);
        vector<ushort> bonemask;
        loopv(bonestrs)
        {
            char *bonestr = bonestrs[i];
            int bone = p->meshes ? ((meshgroup *)p->meshes)->skel->findbone(bonestr[0]=='!' ? bonestr+1 : bonestr) : -1;
            if(bone<0) { conoutf("\frCould not find bone %s for hit zone mask [%s]", bonestr, maskstr); bonestrs.deletearrays(); return; }
            bonemask.add(bone | (bonestr[0]=='!' ? BONEMASK_NOT : 0));
        }
        bonestrs.deletearrays();
        if(bonemask.empty()) return;
        bonemask.sort();
        bonemask.add(BONEMASK_END);

        while(MDL::hitzones.length() < m->skel->numbones) MDL::hitzones.add(0xFF);
        m->skel->applybonemask(bonemask.getbuf(), MDL::hitzones.getbuf(), *id < 0 ? 0xFF : *id);
    }

    skelcommands()
    {
        if(MDL::multiparted()) this->modelcommand(loadpart, "load", "ssf");
        this->modelcommand(settag, "tag", "ssffffff");
        this->modelcommand(setpitch, "pitch", "sffff");
        this->modelcommand(setpitchtarget, "pitchtarget", "ssiff");
        this->modelcommand(setpitchcorrect, "pitchcorrect", "ssfff");
        this->modelcommand(sethitzone, "hitzone", "is");
        if(MDL::cananimate())
        {
            this->modelcommand(setanim, "anim", "ssfiii");
            this->modelcommand(setanimpart, "animpart", "s");
            this->modelcommand(setadjust, "adjust", "sffffff");
        }
    }
};

