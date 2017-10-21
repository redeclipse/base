struct iqm;

struct iqmheader
{
    char magic[16];
    uint version;
    uint filesize;
    uint flags;
    uint num_text, ofs_text;
    uint num_meshes, ofs_meshes;
    uint num_vertexarrays, num_vertexes, ofs_vertexarrays;
    uint num_triangles, ofs_triangles, ofs_adjacency;
    uint num_joints, ofs_joints;
    uint num_poses, ofs_poses;
    uint num_anims, ofs_anims;
    uint num_frames, num_framechannels, ofs_frames, ofs_bounds;
    uint num_comment, ofs_comment;
    uint num_extensions, ofs_extensions;
};

struct iqmmesh
{
    uint name;
    uint material;
    uint first_vertex, num_vertexes;
    uint first_triangle, num_triangles;
};

enum
{
    IQM_POSITION     = 0,
    IQM_TEXCOORD     = 1,
    IQM_NORMAL       = 2,
    IQM_TANGENT      = 3,
    IQM_BLENDINDEXES = 4,
    IQM_BLENDWEIGHTS = 5,
    IQM_COLOR        = 6,
    IQM_CUSTOM       = 0x10
};

enum
{
    IQM_BYTE   = 0,
    IQM_UBYTE  = 1,
    IQM_SHORT  = 2,
    IQM_USHORT = 3,
    IQM_INT    = 4,
    IQM_UINT   = 5,
    IQM_HALF   = 6,
    IQM_FLOAT  = 7,
    IQM_DOUBLE = 8,
};

struct iqmtriangle
{
    uint vertex[3];
};

struct iqmjoint
{
    uint name;
    int parent;
    vec pos;
    quat orient;
    vec size;
};

struct iqmpose
{
    int parent;
    uint mask;
    vec offsetpos;
    vec4 offsetorient;
    vec offsetsize;
    vec scalepos;
    vec4 scaleorient;
    vec scalesize;
};

struct iqmanim
{
    uint name;
    uint first_frame, num_frames;
    float framerate;
    uint flags;
};

struct iqmvertexarray
{
    uint type;
    uint flags;
    uint format;
    uint size;
    uint offset;
};

struct iqm : skelmodel, skelloader<iqm>
{
    iqm(const char *name) : skelmodel(name) {}

    static const char *formatname() { return "iqm"; }
    int type() const { return MDL_IQM; }

    struct iqmmeshgroup : skelmeshgroup
    {
        iqmmeshgroup()
        {
        }

        bool loadiqmmeshes(const char *filename, const iqmheader &hdr, uchar *buf)
        {
            lilswap((uint *)&buf[hdr.ofs_vertexarrays], hdr.num_vertexarrays*sizeof(iqmvertexarray)/sizeof(uint));
            lilswap((uint *)&buf[hdr.ofs_triangles], hdr.num_triangles*sizeof(iqmtriangle)/sizeof(uint));
            lilswap((uint *)&buf[hdr.ofs_meshes], hdr.num_meshes*sizeof(iqmmesh)/sizeof(uint));
            lilswap((uint *)&buf[hdr.ofs_joints], hdr.num_joints*sizeof(iqmjoint)/sizeof(uint));

            const char *str = hdr.ofs_text ? (char *)&buf[hdr.ofs_text] : "";
            float *vpos = NULL, *vnorm = NULL, *vtan = NULL, *vtc = NULL;
            uchar *vindex = NULL, *vweight = NULL;
            iqmvertexarray *vas = (iqmvertexarray *)&buf[hdr.ofs_vertexarrays];
            loopi(hdr.num_vertexarrays)
            {
                iqmvertexarray &va = vas[i];
                switch(va.type)
                {
                    case IQM_POSITION: if(va.format != IQM_FLOAT || va.size != 3) return false; vpos = (float *)&buf[va.offset]; lilswap(vpos, 3*hdr.num_vertexes); break;
                    case IQM_NORMAL: if(va.format != IQM_FLOAT || va.size != 3) return false; vnorm = (float *)&buf[va.offset]; lilswap(vnorm, 3*hdr.num_vertexes); break;
                    case IQM_TANGENT: if(va.format != IQM_FLOAT || va.size != 4) return false; vtan = (float *)&buf[va.offset]; lilswap(vtan, 4*hdr.num_vertexes); break;
                    case IQM_TEXCOORD: if(va.format != IQM_FLOAT || va.size != 2) return false; vtc = (float *)&buf[va.offset]; lilswap(vtc, 2*hdr.num_vertexes); break;
                    case IQM_BLENDINDEXES: if(va.format != IQM_UBYTE || va.size != 4) return false; vindex = (uchar *)&buf[va.offset]; break;
                    case IQM_BLENDWEIGHTS: if(va.format != IQM_UBYTE || va.size != 4) return false; vweight = (uchar *)&buf[va.offset]; break;
                }
            }
            if(!vpos) return false;

            iqmtriangle *tris = (iqmtriangle *)&buf[hdr.ofs_triangles];
            iqmmesh *imeshes = (iqmmesh *)&buf[hdr.ofs_meshes];
            iqmjoint *joints = (iqmjoint *)&buf[hdr.ofs_joints];

            if(hdr.num_joints)
            {
                if(skel->numbones <= 0)
                {
                    skel->numbones = hdr.num_joints;
                    skel->bones = new boneinfo[skel->numbones];
                    loopi(hdr.num_joints)
                    {
                        iqmjoint &j = joints[i];
                        boneinfo &b = skel->bones[i];
                        if(!b.name) b.name = newstring(&str[j.name]);
                        b.parent = j.parent;
                        if(skel->shared <= 1)
                        {
                            j.pos.y = -j.pos.y;
                            j.orient.x = -j.orient.x;
                            j.orient.z = -j.orient.z;
                            j.orient.normalize();
                            b.base = dualquat(j.orient, j.pos);
                            if(b.parent >= 0) b.base.mul(skel->bones[b.parent].base, dualquat(b.base));
                            (b.invbase = b.base).invert();
                        }
                    }
                }

                if(skel->shared <= 1)
                    skel->linkchildren();
            }

            loopi(hdr.num_meshes)
            {
                iqmmesh &im = imeshes[i];
                skelmesh *m = new skelmesh;
                m->group = this;
                meshes.add(m);
                m->name = newstring(&str[im.name]);
                m->numverts = im.num_vertexes;
                int noblend = -1;
                if(m->numverts)
                {
                    m->verts = new vert[m->numverts];
                    if(!vindex || !vweight)
                    {
                        blendcombo c;
                        c.finalize(0);
                        noblend = m->addblendcombo(c);
                    }
                }
                int fv = im.first_vertex;
                float *mpos = vpos + 3*fv,
                      *mnorm = vnorm ? vnorm + 3*fv : NULL,
                      *mtan = vtan ? vtan + 4*fv : NULL,
                      *mtc = vtc ? vtc + 2*fv : NULL;
                uchar *mindex = vindex ? vindex + 4*fv : NULL, *mweight = vweight ? vweight + 4*fv : NULL;
                loopj(im.num_vertexes)
                {
                    vert &v = m->verts[j];
                    v.pos = vec(mpos[0], -mpos[1], mpos[2]);
                    mpos += 3;
                    if(mtc)
                    {
                        v.tc = vec2(mtc[0], mtc[1]);
                        mtc += 2;
                    }
                    else v.tc = vec2(0, 0);
                    if(mnorm)
                    {
                        v.norm = vec(mnorm[0], -mnorm[1], mnorm[2]);
                        mnorm += 3;
                        if(mtan)
                        {
                            m->calctangent(v, v.norm, vec(mtan[0], -mtan[1], mtan[2]), mtan[3]);
                            mtan += 4;
                        }
                    }
                    else { v.norm = vec(0, 0, 0); v.tangent = quat(0, 0, 0, 1); }
                    if(noblend < 0)
                    {
                        blendcombo c;
                        int sorted = 0;
                        loopk(4) sorted = c.addweight(sorted, mweight[k], mindex[k]);
                        mweight += 4;
                        mindex += 4;
                        c.finalize(sorted);
                        v.blend = m->addblendcombo(c);
                    }
                    else v.blend = noblend;
                }
                m->numtris = im.num_triangles;
                if(m->numtris) m->tris = new tri[m->numtris];
                iqmtriangle *mtris = tris + im.first_triangle;
                loopj(im.num_triangles)
                {
                    tri &t = m->tris[j];
                    t.vert[0] = mtris->vertex[0] - fv;
                    t.vert[1] = mtris->vertex[1] - fv;
                    t.vert[2] = mtris->vertex[2] - fv;
                    ++mtris;
                }
                if(!m->numtris || !m->numverts)
                {
                    conoutf("Empty mesh in %s", filename);
                    meshes.removeobj(m);
                    delete m;
                    continue;
                }
                if(vnorm && !vtan) m->calctangents();
            }

            sortblendcombos();

            return true;
        }

        bool loadiqmanims(const char *filename, const iqmheader &hdr, uchar *buf)
        {
            lilswap((uint *)&buf[hdr.ofs_poses], hdr.num_poses*sizeof(iqmpose)/sizeof(uint));
            lilswap((uint *)&buf[hdr.ofs_anims], hdr.num_anims*sizeof(iqmanim)/sizeof(uint));
            lilswap((ushort *)&buf[hdr.ofs_frames], hdr.num_frames*hdr.num_framechannels);

            const char *str = hdr.ofs_text ? (char *)&buf[hdr.ofs_text] : "";
            iqmpose *poses = (iqmpose *)&buf[hdr.ofs_poses];
            iqmanim *anims = (iqmanim *)&buf[hdr.ofs_anims];
            ushort *frames = (ushort *)&buf[hdr.ofs_frames];
            loopi(hdr.num_anims)
            {
                iqmanim &a = anims[i];
                string name;
                copystring(name, filename);
                concatstring(name, ":");
                concatstring(name, &str[a.name]);
                skelanimspec *sa = skel->findskelanim(name);
                if(sa) continue;
                sa = &skel->addskelanim(name);
                sa->frame = skel->numframes;
                sa->range = a.num_frames;
                dualquat *animbones = new dualquat[(skel->numframes+a.num_frames)*skel->numbones];
                if(skel->bones)
                {
                    memcpy(animbones, skel->framebones, skel->numframes*skel->numbones*sizeof(dualquat));
                    delete[] skel->framebones;
                }
                skel->framebones = animbones;
                animbones += skel->numframes*skel->numbones;
                skel->numframes += a.num_frames;
                ushort *animdata = &frames[a.first_frame*hdr.num_framechannels];
                loopj(a.num_frames)
                {
                    dualquat *frame = &animbones[j*skel->numbones];
                    loopk(skel->numbones)
                    {
                        iqmpose &p = poses[k];
                        vec pos;
                        quat orient;
                        pos.x = p.offsetpos.x; if(p.mask&0x01) pos.x += *animdata++ * p.scalepos.x;
                        pos.y = -p.offsetpos.y; if(p.mask&0x02) pos.y -= *animdata++ * p.scalepos.y;
                        pos.z = p.offsetpos.z; if(p.mask&0x04) pos.z += *animdata++ * p.scalepos.z;
                        orient.x = -p.offsetorient.x; if(p.mask&0x08) orient.x -= *animdata++ * p.scaleorient.x;
                        orient.y = p.offsetorient.y; if(p.mask&0x10) orient.y += *animdata++ * p.scaleorient.y;
                        orient.z = -p.offsetorient.z; if(p.mask&0x20) orient.z -= *animdata++ * p.scaleorient.z;
                        orient.w = p.offsetorient.w; if(p.mask&0x40) orient.w += *animdata++ * p.scaleorient.w;
                        orient.normalize();
                        if(p.mask&0x380)
                        {
                            if(p.mask&0x80) animdata++;
                            if(p.mask&0x100) animdata++;
                            if(p.mask&0x200) animdata++;
                        }
                        dualquat dq(orient, pos);
                        if(adjustments.inrange(k)) adjustments[k].adjust(dq);
                        boneinfo &b = skel->bones[k];
                        dq.mul(b.invbase);
                        dualquat &dst = frame[k];
                        if(p.parent < 0) dst = dq;
                        else dst.mul(skel->bones[p.parent].base, dq);
                        dst.fixantipodal(skel->framebones[k]);
                    }
                }
            }

            return true;
        }

        bool loadiqm(const char *filename, bool doloadmesh, bool doloadanim)
        {
            stream *f = openfile(filename, "rb");
            if(!f) return false;

            uchar *buf = NULL;
            iqmheader hdr;
            if(f->read(&hdr, sizeof(hdr)) != sizeof(hdr) || memcmp(hdr.magic, "INTERQUAKEMODEL", sizeof(hdr.magic))) goto error;
            lilswap(&hdr.version, (sizeof(hdr) - sizeof(hdr.magic))/sizeof(uint));
            if(hdr.version != 2) goto error;
            if(hdr.filesize > (16<<20)) goto error; // sanity check... don't load files bigger than 16 MB
            buf = new (false) uchar[hdr.filesize];
            if(!buf || f->read(buf + sizeof(hdr), hdr.filesize - sizeof(hdr)) != hdr.filesize - sizeof(hdr)) goto error;

            if(doloadmesh && !loadiqmmeshes(filename, hdr, buf)) goto error;
            if(doloadanim && !loadiqmanims(filename, hdr, buf)) goto error;

            delete[] buf;
            delete f;
            return true;

        error:
            if(buf) delete[] buf;
            delete f;
            return false;
        }

        bool load(const char *filename, float smooth)
        {
            name = newstring(filename);

            return loadiqm(filename, true, false);
        }

        skelanimspec *loadanim(const char *animname)
        {
            const char *sep = strchr(animname, ':');
            skelanimspec *sa = skel->findskelanim(animname, sep ? '\0' : ':');
            if(!sa)
            {
                string filename;
                copystring(filename, animname);
                if(sep) filename[sep - animname] = '\0';
                if(loadiqm(filename, false, true))
                    sa = skel->findskelanim(animname, sep ? '\0' : ':');
            }
            return sa;
        }
    };

    skelmeshgroup *newmeshes() { return new iqmmeshgroup; }

    bool loaddefaultparts()
    {
        skelpart &mdl = addpart();
        adjustments.setsize(0);
        const char *fname = name + strlen(name);
        do --fname; while(fname >= name && *fname!='/' && *fname!='\\');
        fname++;
        defformatstring(meshname, "%s/%s.iqm", name, fname);
        mdl.meshes = sharemeshes(path(meshname));
        if(!mdl.meshes) return false;
        mdl.initanimparts();
        mdl.initskins();
        return true;
    }

    bool load()
    {
        formatstring(dir, "%s", name);
        defformatstring(cfgname, "%s/iqm.cfg", name);

        loading = this;
        if(execfile(cfgname, false) && parts.length()) // configured iqm, will call the iqm* commands below
        {
            loading = NULL;
            loopv(parts) if(!parts[i]->meshes) return false;
        }
        else // iqm without configuration, try default tris and skin
        {
            if(!loaddefaultparts())
            {
                loading = NULL;
                return false;
            }
            loading = NULL;
        }
        loaded();
        return true;
    }
};

skelcommands<iqm> iqmcommands;

