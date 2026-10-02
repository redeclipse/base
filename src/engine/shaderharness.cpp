// Shader equivalence harness: corpus dump (shaderdumpall) and pixel bench
// (shaderbench). See tools/harness/shaders.ps1. Test-only, like ui.cpp's
// uidumptree: nothing here runs unless the harness asks for it.
#include "engine.h"
#include "shaderharness.h"

#ifdef DEBUG_UTILS
namespace shaderharness
{
    ullong fnv1a(const void *data, size_t len, ullong h)
    {
        const uchar *p = (const uchar *)data;
        for(size_t i = 0; i < len; i++) { h ^= p[i]; h *= 0x100000001b3ULL; }
        return h;
    }

    ullong fnv1astr(const char *str, ullong h) { return fnv1a(str, strlen(str), h); }

    void hexhash(ullong h, char out[17])
    {
        static const char digits[] = "0123456789abcdef";
        for(int i = 15; i >= 0; i--) { out[i] = digits[h&0xF]; h >>= 4; }
        out[16] = '\0';
    }

    static ullong benchhash(const char *name, int index, int seed)
    {
        ullong h = fnv1astr(name);
        h = fnv1a(&index, sizeof(index), h);
        return fnv1a(&seed, sizeof(seed), h);
    }

    float benchfloat(const char *name, int index, int seed)
    {
        return 0.1f + 0.9f*float((benchhash(name, index, seed)>>40)&0xFFFFFF)/float(0xFFFFFF);
    }

    int benchint(const char *name, int index, int seed)
    {
        return 1 + int((benchhash(name, index, seed)>>32)&3);
    }

    static const struct { GLenum type; const char *name; int components; bool sampler; } gltypes[] =
    {
        { GL_FLOAT, "float", 1, false }, { GL_FLOAT_VEC2, "vec2", 2, false }, { GL_FLOAT_VEC3, "vec3", 3, false }, { GL_FLOAT_VEC4, "vec4", 4, false },
        { GL_INT, "int", 1, false }, { GL_INT_VEC2, "ivec2", 2, false }, { GL_INT_VEC3, "ivec3", 3, false }, { GL_INT_VEC4, "ivec4", 4, false },
        { GL_UNSIGNED_INT, "uint", 1, false }, { GL_UNSIGNED_INT_VEC2, "uvec2", 2, false }, { GL_UNSIGNED_INT_VEC3, "uvec3", 3, false }, { GL_UNSIGNED_INT_VEC4, "uvec4", 4, false },
        { GL_BOOL, "bool", 1, false }, { GL_BOOL_VEC2, "bvec2", 2, false }, { GL_BOOL_VEC3, "bvec3", 3, false }, { GL_BOOL_VEC4, "bvec4", 4, false },
        { GL_FLOAT_MAT2, "mat2", 4, false }, { GL_FLOAT_MAT3, "mat3", 9, false }, { GL_FLOAT_MAT4, "mat4", 16, false },
        { GL_SAMPLER_2D, "sampler2D", 1, true }, { GL_SAMPLER_3D, "sampler3D", 1, true }, { GL_SAMPLER_CUBE, "samplerCube", 1, true },
        { GL_SAMPLER_2D_SHADOW, "sampler2DShadow", 1, true }, { GL_SAMPLER_2D_RECT, "sampler2DRect", 1, true },
        { GL_SAMPLER_2D_RECT_SHADOW, "sampler2DRectShadow", 1, true }, { GL_SAMPLER_2D_ARRAY, "sampler2DArray", 1, true },
        { GL_SAMPLER_2D_ARRAY_SHADOW, "sampler2DArrayShadow", 1, true }, { GL_SAMPLER_CUBE_SHADOW, "samplerCubeShadow", 1, true },
        { GL_SAMPLER_2D_MULTISAMPLE, "sampler2DMS", 1, true }, { GL_SAMPLER_2D_MULTISAMPLE_ARRAY, "sampler2DMSArray", 1, true },
        { GL_SAMPLER_BUFFER, "samplerBuffer", 1, true },
        { GL_INT_SAMPLER_2D, "isampler2D", 1, true }, { GL_UNSIGNED_INT_SAMPLER_2D, "usampler2D", 1, true },
        { GL_INT_SAMPLER_2D_RECT, "isampler2DRect", 1, true }, { GL_UNSIGNED_INT_SAMPLER_2D_RECT, "usampler2DRect", 1, true }
    };

    const char *gltypename(GLenum type)
    {
        loopi(sizeof(gltypes)/sizeof(gltypes[0])) if(gltypes[i].type == type) return gltypes[i].name;
        static string unknown;
        formatstring(unknown, "0x%X", type);
        return unknown;
    }

    int gltypecomponents(GLenum type)
    {
        loopi(sizeof(gltypes)/sizeof(gltypes[0])) if(gltypes[i].type == type) return gltypes[i].components;
        return 0;
    }

    bool issamplertype(GLenum type)
    {
        loopi(sizeof(gltypes)/sizeof(gltypes[0])) if(gltypes[i].type == type) return gltypes[i].sampler;
        return false;
    }
}

using namespace shaderharness;

// ---------------------------------------------------------------- dump ----

static void addline(vector<char *> &lines, const char *fmt, ...)
{
    defvformatstring(line, fmt, fmt);
    lines.add(newstring(line));
}

static bool linesort(const char *a, const char *b) { return strcmp(a, b) < 0; }

static void putlines(vector<char> &buf, vector<char *> &lines, bool sorted)
{
    if(sorted) lines.sort(linesort);
    loopv(lines) { buf.put(lines[i], strlen(lines[i])); buf.add('\n'); }
    lines.deletearrays();
}

static void writemeta(Shader &s, vector<char> &buf)
{
    vector<char *> lines;
    addline(lines, "type %d", s.type);
    addline(lines, "mapdef %d", s.mapdef ? 1 : 0);
    addline(lines, "variantof %s", s.variantshader ? s.variantshader->name : "-");
    addline(lines, "reusevs %s", s.reusevs ? s.reusevs->name : "-");
    addline(lines, "reuseps %s", s.reuseps ? s.reuseps->name : "-");
    loopi(MAXVARIANTROWS) if(s.numvariants(i)) addline(lines, "variants %d %d", i, s.numvariants(i));
    putlines(buf, lines, false);
    // Declaration order is part of the contract: setslotparams indexes by it.
    loopv(s.defaultparams)
    {
        SlotShaderParamState &p = s.defaultparams[i];
        addline(lines, "param %s %.9g %.9g %.9g %.9g %d %d %d", p.name, p.val[0], p.val[1], p.val[2], p.val[3], p.flags, p.palette, p.palindex);
    }
    putlines(buf, lines, false);
    loopv(s.attriblocs) addline(lines, "attribloc %s %d", s.attriblocs[i].name, s.attriblocs[i].loc);
    loopv(s.uniformlocs)
    {
        UniformLoc &u = s.uniformlocs[i];
        addline(lines, "uniformloc %s %s %d %d", u.name, u.blockname ? u.blockname : "-", u.binding, u.stride);
    }
    putlines(buf, lines, true);
}

static void writereflect(Shader &s, vector<char> &buf)
{
    GLuint p = s.program;
    vector<char *> lines;
    GLchar name[256], bname[256];
    GLsizei len;
    GLint n = 0, size;
    GLenum type;

    glGetProgramiv_(p, GL_ACTIVE_ATTRIBUTES, &n);
    loopi(n)
    {
        glGetActiveAttrib_(p, i, sizeof(name), &len, &size, &type, name);
        addline(lines, "attrib %s %s %d %d", name, gltypename(type), size, glGetAttribLocation_(p, name));
    }

    glGetProgramiv_(p, GL_ACTIVE_UNIFORMS, &n);
    loopi(n)
    {
        glGetActiveUniform_(p, i, sizeof(name), &len, &size, &type, name);
        GLint block = -1, offset = -1;
        if(glGetActiveUniformsiv_)
        {
            GLuint idx = i;
            glGetActiveUniformsiv_(p, 1, &idx, GL_UNIFORM_BLOCK_INDEX, &block);
            glGetActiveUniformsiv_(p, 1, &idx, GL_UNIFORM_OFFSET, &offset);
        }
        if(block >= 0 && glGetActiveUniformBlockName_)
        {
            bname[0] = '\0';
            glGetActiveUniformBlockName_(p, block, sizeof(bname), NULL, bname);
            addline(lines, "blockuniform %s %s %s %d %d", bname, name, gltypename(type), size, offset);
        }
        else if(issamplertype(type))
        {
            GLint unit = -1;
            glGetUniformiv_(p, glGetUniformLocation_(p, name), &unit);
            addline(lines, "sampler %s %s %d", name, gltypename(type), unit);
        }
        else addline(lines, "uniform %s %s %d", name, gltypename(type), size);
    }

    if(glGetActiveUniformBlockiv_ && glGetActiveUniformBlockName_)
    {
        glGetProgramiv_(p, GL_ACTIVE_UNIFORM_BLOCKS, &n);
        loopi(n)
        {
            bname[0] = '\0';
            glGetActiveUniformBlockName_(p, i, sizeof(bname), NULL, bname);
            GLint datasize = 0, binding = 0;
            glGetActiveUniformBlockiv_(p, i, GL_UNIFORM_BLOCK_DATA_SIZE, &datasize);
            glGetActiveUniformBlockiv_(p, i, GL_UNIFORM_BLOCK_BINDING, &binding);
            addline(lines, "block %s %d %d", bname, datasize, binding);
        }
    }

    vector<FragDataLoc> outs;
    scanfragdatalocs(s, outs);
    loopv(outs) addline(lines, "fragdata %s %s %d %d", outs[i].name, gltypename(outs[i].format), outs[i].loc, outs[i].index);

    putlines(buf, lines, true);
}

// Settings ids and run names become path components.
static bool validfield(const char *s)
{
    if(!*s) return false;
    for(; *s; s++) if(!isalnum((uchar)*s) && *s != '_' && *s != '-') return false;
    return true;
}

static void writecorpusfile(const char *run, const char *hash, const char *file, const char *data, int len)
{
    defformatstring(path, "shadercorpus/%s/blobs/%s/%s", run, hash, file);
    stream *f = openrawfile(path, "wb");
    if(!f) { conoutf(colourred, "shaderdumpall: cannot write %s", path); return; }
    f->write(data, len);
    delete f;
}

static void writeglinfo(const char *run)
{
    defformatstring(path, "shadercorpus/%s/gl.txt", run);
    stream *f = openrawfile(path, "wb");
    if(!f) return;
    f->printf("vendor %s\nrenderer %s\nversion %s\nglslversion %d\n",
        (const char *)glGetString(GL_VENDOR), (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION), glslversion);
    delete f;
}

static bool modelfamily(Shader &s)
{
    return s.origin && (!strncmp(s.origin, "modelshader ", 12) || !strncmp(s.origin, "rsmmodelshader ", 15));
}

// A model's shaders are generated when it is drawn, and which models get
// drawn after a resetshaders depends on the camera, occlusion query timing
// and item fades. So the per-map pass generates them itself, for every model
// the map can draw: the map models it uses (the set preloadusedmapmodels
// loads) and the models of its other entities (items, player starts, actors).
static void mapmodelshaders(vector<Shader *> &out)
{
    vector<model *> mdls;
    vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.flags&EF_VIRTUAL) continue;
        model *m = NULL;
        if(e.type == ET_MAPMODEL)
        {
            if(e.attrs[0] < 0 || !entities::isallowed(e)) continue;
            m = loadmodel(NULL, e.attrs[0]);
        }
        else
        {
            const char *name = entities::entmdlname(e.type, e.attrs);
            if(name && *name) m = loadmodel(name);
        }
        if(m && mdls.find(m) < 0) mdls.add(m);
    }
    loopv(mdls) mdls[i]->harnessshaders(out);
}

// Map content decides these, so the per-map pass dumps only them: map
// shaders, and everything a generateshader command made (grass, models,
// deferred lights and the rest), whose options C++ formats from what the
// map contains. Model shaders count only if mapmodelshaders produced them,
// so what happened to be drawn does not change the rows.
static bool mapdependent(Shader &s, vector<Shader *> &mdlshaders)
{
    if(modelfamily(s)) return mdlshaders.find(&s) >= 0 || (s.variantshader && mdlshaders.find(s.variantshader) >= 0);
    return s.mapdef || s.generated;
}

// Manifest fields are tab-separated, one row per line.
static void manifestfield(const char *in, char *out, size_t outlen)
{
    size_t i = 0;
    for(; *in && i + 1 < outlen; in++) out[i++] = (*in == '\t' || *in == '\n' || *in == '\r') ? ' ' : *in;
    out[i] = '\0';
}

static bool shadersort(Shader *a, Shader *b) { return strcmp(a->name, b->name) < 0; }

ICOMMAND(0, shaderdumpall, "ssi", (char *run, char *sid, int *mapsonly),
{
    if(identflags&IDF_MAP) return;
    if(!validfield(run) || !validfield(sid))
    {
        conoutf(colourred, "shaderdumpall: run and settings id must be [A-Za-z0-9_-]+");
        intret(-1);
        return;
    }
    vector<Shader *> mdlshaders;
    if(*mapsonly) mapmodelshaders(mdlshaders);
    forceallshaders();
    vector<Shader *> all;
    collectshaders(all);
    all.sort(shadersort);

    defformatstring(manifestpath, "shadercorpus/%s/manifest.tsv", run);
    stream *manifest = openrawfile(manifestpath, "ab");
    if(!manifest) { conoutf(colourred, "shaderdumpall: cannot open %s", manifestpath); intret(-1); return; }

    int rows = 0;
    int blobs = 0;
    loopv(all)
    {
        Shader &s = *all[i];
        if(*mapsonly && !mapdependent(s, mdlshaders)) continue;
        string origin;
        manifestfield(s.origin ? s.origin : "-", origin, sizeof(origin));
        rows++;
        if(!s.loaded() || !s.program)
        {
            manifest->printf("%s\t%s\t-\t%s\n", s.name, sid, origin);
            continue;
        }

        vector<char> vsfull;
        vector<char> fsfull;
        vector<char> meta;
        vector<char> reflect;
        if(!composeglslsource(s, GL_VERTEX_SHADER, vsfull)) vsfull.add('\0');
        if(!composeglslsource(s, GL_FRAGMENT_SHADER, fsfull)) fsfull.add('\0');
        writemeta(s, meta);
        writereflect(s, reflect);

        // Hash exactly the bytes written to each blob file (vsfull/fsfull carry
        // composeglslsource's trailing NUL, which is not written -- see the
        // length()-1 below and in writecorpusfile), plus one separator NUL
        // byte after each of the vs/fs stages so moving text across that
        // stage boundary changes the hash instead of leaving it unchanged.
        ullong h = fnv1a(vsfull.getbuf(), vsfull.length()-1);
        h = fnv1a("", 1, h);
        h = fnv1a(fsfull.getbuf(), fsfull.length()-1, h);
        h = fnv1a("", 1, h);
        h = fnv1a(meta.getbuf(), meta.length(), h);
        h = fnv1a(reflect.getbuf(), reflect.length(), h);
        char hex[17];
        hexhash(h, hex);

        defformatstring(metapath, "shadercorpus/%s/blobs/%s/meta.txt", run, hex);
        if(!fileexists(findfile(metapath, "r"), "r"))
        {
            Shader *vsrc = findstagesource(s, GL_VERTEX_SHADER);
            Shader *fsrc = findstagesource(s, GL_FRAGMENT_SHADER);
            const char *vsbody = vsrc ? vsrc->vsstr : "";
            const char *fsbody = fsrc ? fsrc->psstr : "";
            writecorpusfile(run, hex, "vs.glsl", vsbody, strlen(vsbody));
            writecorpusfile(run, hex, "fs.glsl", fsbody, strlen(fsbody));
            writecorpusfile(run, hex, "vs.full.glsl", vsfull.getbuf(), vsfull.length()-1);
            writecorpusfile(run, hex, "fs.full.glsl", fsfull.getbuf(), fsfull.length()-1);
            writecorpusfile(run, hex, "reflect.txt", reflect.getbuf(), reflect.length());
            // meta.txt last: its presence marks the blob complete.
            writecorpusfile(run, hex, "meta.txt", meta.getbuf(), meta.length());
            blobs++;
        }
        manifest->printf("%s\t%s\t%s\t%s\n", s.name, sid, hex, origin);
    }
    delete manifest;
    writeglinfo(run);
    conoutf(colourwhite, "SHADERDUMP %s %s %d %d", run, sid, rows, blobs);
    intret(rows);
});

// --------------------------------------------------------------- bench ----
//
// Renders the corpus copy of a shader (compiled from its composed source, so
// no header is injected) and the live shader into RGBA32F targets with the
// same seeded inputs, and compares the pixels. Inputs come from the live
// program's reflection; tier 0 has already shown the two interfaces match.

static const int BENCHSIZE = 256, BENCHGRID = 32, BENCHTARGETS = 4, BENCHVERTS = BENCHGRID*BENCHGRID*6;
static const float BENCHCLEAR = -12345.0f;
static const double BENCHTOLERANCE = 1e-5;

struct benchtex { char *name; GLenum target; GLuint tex; };
struct benchattrib { char *name; int loc, comps; GLuint vbo; };
struct benchblock { char *name; GLuint buf; };

struct benchinputs
{
    vector<benchtex> textures;
    vector<benchattrib> attribs;
    vector<benchblock> blocks;
    string reason;

    benchinputs() { reason[0] = '\0'; }
    ~benchinputs()
    {
        loopv(textures) { glDeleteTextures(1, &textures[i].tex); delete[] textures[i].name; }
        loopv(attribs) { glDeleteBuffers_(1, &attribs[i].vbo); delete[] attribs[i].name; }
        loopv(blocks) { glDeleteBuffers_(1, &blocks[i].buf); delete[] blocks[i].name; }
    }

    void unsupported(const char *what, GLenum type)
    {
        if(!reason[0]) formatstring(reason, "unsupported %s %s", what, gltypename(type));
    }
};

static void benchnoise(vector<float> &data, int count, const char *name, int seed, float lo, float hi)
{
    data.setsize(0);
    loopi(count) data.add(lo + (hi - lo)*(benchfloat(name, i, seed) - 0.1f)/0.9f);
}

static GLuint benchtexture(GLenum samplertype, const char *name, int seed, GLenum &target)
{
    bool shadow = false;
    int w = 64, h = 64, d = 1;
    switch(samplertype)
    {
        case GL_SAMPLER_2D: target = GL_TEXTURE_2D; break;
        case GL_SAMPLER_2D_SHADOW: target = GL_TEXTURE_2D; shadow = true; break;
        case GL_SAMPLER_2D_RECT: target = GL_TEXTURE_RECTANGLE; w = h = BENCHSIZE; break;
        case GL_SAMPLER_2D_RECT_SHADOW: target = GL_TEXTURE_RECTANGLE; w = h = BENCHSIZE; shadow = true; break;
        case GL_SAMPLER_3D: target = GL_TEXTURE_3D; w = h = d = 16; break;
        case GL_SAMPLER_2D_ARRAY: target = GL_TEXTURE_2D_ARRAY; d = 4; break;
        case GL_SAMPLER_2D_ARRAY_SHADOW: target = GL_TEXTURE_2D_ARRAY; d = 4; shadow = true; break;
        case GL_SAMPLER_CUBE: target = GL_TEXTURE_CUBE_MAP; w = h = 32; break;
        default: return 0;
    }
    int comps = shadow ? 1 : 4, faces = target == GL_TEXTURE_CUBE_MAP ? 6 : 1;
    vector<float> data;
    benchnoise(data, w*h*d*comps*faces, name, seed, shadow ? 0.1f : 0.0f, shadow ? 0.9f : 1.0f);
    GLenum ifmt = shadow ? GL_DEPTH_COMPONENT32F : GL_RGBA32F, fmt = shadow ? GL_DEPTH_COMPONENT : GL_RGBA;
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(target, tex);
    switch(target)
    {
        case GL_TEXTURE_3D: case GL_TEXTURE_2D_ARRAY:
            glTexImage3D_(target, 0, ifmt, w, h, d, 0, fmt, GL_FLOAT, data.getbuf());
            break;
        case GL_TEXTURE_CUBE_MAP:
            loopi(6) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, ifmt, w, h, 0, fmt, GL_FLOAT, &data[i*w*h*comps]);
            break;
        default:
            glTexImage2D(target, 0, ifmt, w, h, 0, fmt, GL_FLOAT, data.getbuf());
            break;
    }
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if(target == GL_TEXTURE_3D || target == GL_TEXTURE_CUBE_MAP) glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    if(shadow)
    {
        glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    }
    return tex;
}

static bool uniforminblock(GLuint p, int i)
{
    if(!glGetActiveUniformsiv_) return false;
    GLuint idx = i;
    GLint block = -1;
    glGetActiveUniformsiv_(p, 1, &idx, GL_UNIFORM_BLOCK_INDEX, &block);
    return block >= 0;
}

// Textures, vertex arrays and uniform buffers for one seed, shared by both
// programs so they read identical data.
static void prepareinputs(GLuint live, int seed, benchinputs &in)
{
    GLchar name[256];
    GLsizei len;
    GLint n = 0, size;
    GLenum type;

    glGetProgramiv_(live, GL_ACTIVE_UNIFORMS, &n);
    loopi(n)
    {
        glGetActiveUniform_(live, i, sizeof(name), &len, &size, &type, name);
        if(!issamplertype(type) || uniforminblock(live, i)) continue;
        benchtex &t = in.textures.add();
        t.name = newstring(name);
        t.tex = benchtexture(type, name, seed, t.target);
        if(!t.tex) in.unsupported("sampler", type);
    }

    // A grid of triangles covering most of clip space. Every matrix uniform
    // is identity, so shaders that transform vvertex keep it on screen.
    vector<float> xy;
    loopi(BENCHGRID) loopj(BENCHGRID)
    {
        float x0 = -0.95f + 1.9f*i/BENCHGRID, x1 = -0.95f + 1.9f*(i+1)/BENCHGRID,
              y0 = -0.95f + 1.9f*j/BENCHGRID, y1 = -0.95f + 1.9f*(j+1)/BENCHGRID;
        const float quad[12] = { x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1 };
        loopk(12) xy.add(quad[k]);
    }

    glGetProgramiv_(live, GL_ACTIVE_ATTRIBUTES, &n);
    loopi(n)
    {
        glGetActiveAttrib_(live, i, sizeof(name), &len, &size, &type, name);
        GLint loc = glGetAttribLocation_(live, name);
        if(loc < 0) continue;
        int comps = 0;
        switch(type)
        {
            case GL_FLOAT: comps = 1; break;
            case GL_FLOAT_VEC2: comps = 2; break;
            case GL_FLOAT_VEC3: comps = 3; break;
            case GL_FLOAT_VEC4: comps = 4; break;
        }
        if(!comps) { in.unsupported("attribute", type); continue; }
        vector<float> data;
        for(int v = 0; v < BENCHVERTS; v++) loopk(comps)
        {
            float val;
            if(!strcmp(name, "vvertex")) val = k < 2 ? xy[v*2+k] : (k == 2 ? 0.5f : 1.0f);
            else if(!strcmp(name, "vboneindex")) val = float(int((benchfloat(name, v*4+k, seed) - 0.1f)/0.9f*3.999f));
            else val = benchfloat(name, v*4+k, seed);
            data.add(val);
        }
        benchattrib &a = in.attribs.add();
        a.name = newstring(name);
        a.loc = loc;
        a.comps = comps;
        glGenBuffers_(1, &a.vbo);
        glBindBuffer_(GL_ARRAY_BUFFER, a.vbo);
        glBufferData_(GL_ARRAY_BUFFER, data.length()*sizeof(float), data.getbuf(), GL_STATIC_DRAW);
    }
    glBindBuffer_(GL_ARRAY_BUFFER, 0);

    if(glGetActiveUniformBlockiv_ && glGetActiveUniformBlockName_)
    {
        glGetProgramiv_(live, GL_ACTIVE_UNIFORM_BLOCKS, &n);
        loopi(n)
        {
            glGetActiveUniformBlockName_(live, i, sizeof(name), NULL, name);
            GLint datasize = 0;
            glGetActiveUniformBlockiv_(live, i, GL_UNIFORM_BLOCK_DATA_SIZE, &datasize);
            vector<float> data;
            benchnoise(data, (datasize + 3)/4, name, seed, 0.1f, 1.0f);
            benchblock &b = in.blocks.add();
            b.name = newstring(name);
            glGenBuffers_(1, &b.buf);
            glBindBuffer_(GL_UNIFORM_BUFFER, b.buf);
            glBufferData_(GL_UNIFORM_BUFFER, data.length()*sizeof(float), data.getbuf(), GL_STATIC_DRAW);
        }
        glBindBuffer_(GL_UNIFORM_BUFFER, 0);
    }
}

static const float benchidentity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

// Sets program p's uniforms from the live program's list, so both programs
// get the same values for the same names.
static void bindinputs(GLuint p, GLuint live, int seed, benchinputs &in)
{
    GLchar name[256];
    GLsizei len;
    GLint n = 0, size;
    GLenum type;
    int sampler = 0;

    glGetProgramiv_(live, GL_ACTIVE_UNIFORMS, &n);
    loopi(n)
    {
        glGetActiveUniform_(live, i, sizeof(name), &len, &size, &type, name);
        if(uniforminblock(live, i)) continue;
        GLint loc = glGetUniformLocation_(p, name);
        if(issamplertype(type))
        {
            int unit = sampler++;
            if(!in.textures.inrange(unit)) continue;
            benchtex &t = in.textures[unit];
            glActiveTexture_(GL_TEXTURE0 + unit);
            if(t.tex) glBindTexture(t.target, t.tex);
            if(loc >= 0) glUniform1i_(loc, unit);
            continue;
        }
        if(loc < 0) continue;
        int comps = gltypecomponents(type), count = size*comps;
        if(!comps) { in.unsupported("uniform", type); continue; }
        if(type == GL_FLOAT_MAT2 || type == GL_FLOAT_MAT3 || type == GL_FLOAT_MAT4)
        {
            vector<float> m;
            loopj(size)
            {
                if(type == GL_FLOAT_MAT4) loopk(16) m.add(benchidentity[k]);
                else if(type == GL_FLOAT_MAT3) loopk(9) m.add(k%4 == 0 ? 1.0f : 0.0f);
                else loopk(4) m.add(k == 0 || k == 3 ? 1.0f : 0.0f);
            }
            if(type == GL_FLOAT_MAT4) glUniformMatrix4fv_(loc, size, GL_FALSE, m.getbuf());
            else if(type == GL_FLOAT_MAT3) glUniformMatrix3fv_(loc, size, GL_FALSE, m.getbuf());
            else glUniformMatrix2fv_(loc, size, GL_FALSE, m.getbuf());
            continue;
        }
        switch(type)
        {
            case GL_FLOAT: case GL_FLOAT_VEC2: case GL_FLOAT_VEC3: case GL_FLOAT_VEC4:
            {
                vector<float> v;
                loopj(count) v.add(benchfloat(name, j, seed));
                if(comps == 1) glUniform1fv_(loc, size, v.getbuf());
                else if(comps == 2) glUniform2fv_(loc, size, v.getbuf());
                else if(comps == 3) glUniform3fv_(loc, size, v.getbuf());
                else glUniform4fv_(loc, size, v.getbuf());
                break;
            }
            case GL_INT: case GL_INT_VEC2: case GL_INT_VEC3: case GL_INT_VEC4:
            case GL_BOOL: case GL_BOOL_VEC2: case GL_BOOL_VEC3: case GL_BOOL_VEC4:
            {
                bool isbool = type == GL_BOOL || type == GL_BOOL_VEC2 || type == GL_BOOL_VEC3 || type == GL_BOOL_VEC4;
                vector<GLint> v;
                loopj(count) v.add(isbool ? benchint(name, j, seed)&1 : benchint(name, j, seed));
                if(comps == 1) glUniform1iv_(loc, size, v.getbuf());
                else if(comps == 2) glUniform2iv_(loc, size, v.getbuf());
                else if(comps == 3) glUniform3iv_(loc, size, v.getbuf());
                else glUniform4iv_(loc, size, v.getbuf());
                break;
            }
            case GL_UNSIGNED_INT: case GL_UNSIGNED_INT_VEC2: case GL_UNSIGNED_INT_VEC3: case GL_UNSIGNED_INT_VEC4:
            {
                vector<GLuint> v;
                loopj(count) v.add(GLuint(benchint(name, j, seed)));
                if(comps == 1) glUniform1uiv_(loc, size, v.getbuf());
                else if(comps == 2) glUniform2uiv_(loc, size, v.getbuf());
                else if(comps == 3) glUniform3uiv_(loc, size, v.getbuf());
                else glUniform4uiv_(loc, size, v.getbuf());
                break;
            }
            default: in.unsupported("uniform", type); break;
        }
    }

    loopv(in.blocks)
    {
        GLuint idx = glGetUniformBlockIndex_(p, in.blocks[i].name);
        if(idx == GL_INVALID_INDEX) continue;
        glUniformBlockBinding_(p, idx, i);
        glBindBufferBase_(GL_UNIFORM_BUFFER, i, in.blocks[i].buf);
    }

    loopv(in.attribs)
    {
        benchattrib &a = in.attribs[i];
        glBindBuffer_(GL_ARRAY_BUFFER, a.vbo);
        glVertexAttribPointer_(a.loc, a.comps, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray_(a.loc);
    }
    glBindBuffer_(GL_ARRAY_BUFFER, 0);
}

static GLuint compilebenchstage(GLenum type, const char *src)
{
    GLuint obj = glCreateShader_(type);
    glShaderSource_(obj, 1, (const GLchar **)&src, NULL);
    glCompileShader_(obj);
    GLint ok = 0;
    glGetShaderiv_(obj, GL_COMPILE_STATUS, &ok);
    if(!ok) { glDeleteShader_(obj); return 0; }
    return obj;
}

// Links the corpus copy with the live program's attribute and fragment
// output locations, so both read the same arrays and write the same targets.
static GLuint linkoldprogram(const char *vs, const char *fs, Shader &live)
{
    GLuint vsobj = compilebenchstage(GL_VERTEX_SHADER, vs), fsobj = compilebenchstage(GL_FRAGMENT_SHADER, fs);
    if(!vsobj || !fsobj)
    {
        if(vsobj) glDeleteShader_(vsobj);
        if(fsobj) glDeleteShader_(fsobj);
        return 0;
    }
    GLuint p = glCreateProgram_();
    glAttachShader_(p, vsobj);
    glAttachShader_(p, fsobj);
    GLchar name[256];
    GLsizei len;
    GLint n = 0, size;
    GLenum type;
    glGetProgramiv_(live.program, GL_ACTIVE_ATTRIBUTES, &n);
    loopi(n)
    {
        glGetActiveAttrib_(live.program, i, sizeof(name), &len, &size, &type, name);
        GLint loc = glGetAttribLocation_(live.program, name);
        if(loc >= 0) glBindAttribLocation_(p, loc, name);
    }
    if(glBindFragDataLocation_)
    {
        vector<FragDataLoc> outs;
        scanfragdatalocs(live, outs);
        loopv(outs) if(!outs[i].index) glBindFragDataLocation_(p, outs[i].loc, outs[i].name);
    }
    glLinkProgram_(p);
    // Flagged for deletion; freed with the program.
    glDeleteShader_(vsobj);
    glDeleteShader_(fsobj);
    GLint ok = 0;
    glGetProgramiv_(p, GL_LINK_STATUS, &ok);
    if(!ok) { glDeleteProgram_(p); return 0; }
    return p;
}

// glClear only touches the current draw buffers, so every attachment is
// cleared through allbufs before narrowing to the ones the shader writes.
static void benchrender(GLuint fbo, const GLenum *allbufs, const GLenum *bufs, GLuint p, GLuint live, int seed, benchinputs &in, vector<float> &out)
{
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, BENCHSIZE, BENCHSIZE);
    glDrawBuffers_(BENCHTARGETS, allbufs);
    glClearColor(BENCHCLEAR, BENCHCLEAR, BENCHCLEAR, BENCHCLEAR);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawBuffers_(BENCHTARGETS, bufs);
    glUseProgram_(p);
    bindinputs(p, live, seed, in);
    glDrawArrays(GL_TRIANGLES, 0, BENCHVERTS);
    loopv(in.attribs) glDisableVertexAttribArray_(in.attribs[i].loc);
    out.setsize(0);
    loopi(BENCHTARGETS)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
        glReadPixels(0, 0, BENCHSIZE, BENCHSIZE, GL_RGBA, GL_FLOAT, out.pad(BENCHSIZE*BENCHSIZE*4));
    }
}

static void benchcompare(const vector<float> &a, const vector<float> &b, double &maxerr, int &written)
{
    const int pixels = BENCHSIZE*BENCHSIZE;
    written = 0;
    loopi(pixels)
    {
        bool w = false;
        loopk(BENCHTARGETS) loopj(4)
        {
            int idx = (k*pixels + i)*4 + j;
            float x = a[idx], y = b[idx];
            if(x != BENCHCLEAR || y != BENCHCLEAR) w = true;
            if(x == y || (isnan(x) && isnan(y))) continue;
            double err = 1e30;
            if(!isnan(x) && !isnan(y) && !isinf(x) && !isinf(y)) err = fabs(double(x) - double(y))/max(1.0, fabs(double(x)));
            maxerr = max(maxerr, err);
        }
        if(w) written++;
    }
}

static const GLenum benchtextargets[] = { GL_TEXTURE_2D, GL_TEXTURE_RECTANGLE, GL_TEXTURE_3D, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_CUBE_MAP };
static const GLenum benchtexbindings[] = { GL_TEXTURE_BINDING_2D, GL_TEXTURE_BINDING_RECTANGLE, GL_TEXTURE_BINDING_3D, GL_TEXTURE_BINDING_2D_ARRAY, GL_TEXTURE_BINDING_CUBE_MAP };
static const int BENCHTEXTARGETS = sizeof(benchtextargets)/sizeof(benchtextargets[0]);

struct benchglstate
{
    GLint vao, fbo, viewport[4], activetex;
    GLfloat clear[4];
    GLboolean blend, depth, cull, stencil, scissor, colormask[4];
    vector<GLint> textures;

    // units: how many texture units the bench binds to (one per sampler).
    // The render targets are created on whichever unit is active on entry.
    void save(int units)
    {
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
        glGetBooleanv(GL_COLOR_WRITEMASK, colormask);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activetex);
        units = max(units, int(activetex - GL_TEXTURE0) + 1);
        loopi(units)
        {
            glActiveTexture_(GL_TEXTURE0 + i);
            loopj(BENCHTEXTARGETS) glGetIntegerv(benchtexbindings[j], &textures.add());
        }
        glActiveTexture_(activetex);
        blend = glIsEnabled(GL_BLEND); depth = glIsEnabled(GL_DEPTH_TEST); cull = glIsEnabled(GL_CULL_FACE);
        stencil = glIsEnabled(GL_STENCIL_TEST); scissor = glIsEnabled(GL_SCISSOR_TEST);
        glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_STENCIL_TEST); glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    static void setcap(GLenum cap, GLboolean on) { if(on) glEnable(cap); else glDisable(cap); }

    void restore()
    {
        glUseProgram_(0);
        Shader::lastshader = NULL;
        loopi(textures.length()/BENCHTEXTARGETS)
        {
            glActiveTexture_(GL_TEXTURE0 + i);
            loopj(BENCHTEXTARGETS) glBindTexture(benchtextargets[j], textures[i*BENCHTEXTARGETS + j]);
        }
        glActiveTexture_(activetex);
        glBindBuffer_(GL_ARRAY_BUFFER, 0);
        glBindVertexArray_(vao);
        glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glColorMask(colormask[0], colormask[1], colormask[2], colormask[3]);
        setcap(GL_BLEND, blend); setcap(GL_DEPTH_TEST, depth); setcap(GL_CULL_FACE, cull);
        setcap(GL_STENCIL_TEST, stencil); setcap(GL_SCISSOR_TEST, scissor);
    }
};

// The bench writes the live program's uniforms, and the engine does not
// rewrite them all: sampler units and block bindings are set once at link
// time, and global params are re-sent only when their version changes. So
// every value the bench can touch is read back first and put back after.
struct benchuniform
{
    GLint loc;
    GLenum type;
    union { GLfloat f[16]; GLint i[16]; GLuint u[16]; };
};

static bool benchfloattype(GLenum type)
{
    switch(type)
    {
        case GL_FLOAT: case GL_FLOAT_VEC2: case GL_FLOAT_VEC3: case GL_FLOAT_VEC4:
        case GL_FLOAT_MAT2: case GL_FLOAT_MAT3: case GL_FLOAT_MAT4:
            return true;
    }
    return false;
}

static bool benchuinttype(GLenum type)
{
    return type == GL_UNSIGNED_INT || type == GL_UNSIGNED_INT_VEC2 || type == GL_UNSIGNED_INT_VEC3 || type == GL_UNSIGNED_INT_VEC4;
}

struct benchprogramstate
{
    GLuint program;
    vector<benchuniform> uniforms;
    vector<GLint> blockbindings;
    int samplers;

    void save(GLuint p)
    {
        program = p;
        samplers = 0;
        GLchar name[256];
        GLsizei len;
        GLint n = 0, size;
        GLenum type;
        glGetProgramiv_(p, GL_ACTIVE_UNIFORMS, &n);
        loopi(n)
        {
            glGetActiveUniform_(p, i, sizeof(name), &len, &size, &type, name);
            if(uniforminblock(p, i)) continue;
            if(issamplertype(type)) samplers++;
            if(!gltypecomponents(type) || (benchuinttype(type) && !glGetUniformuiv_)) continue;
            // Arrays report "name[0]"; each element has its own location.
            char *brak = strchr(name, '[');
            if(brak) *brak = '\0';
            loopj(size)
            {
                defformatstring(elem, "%s[%d]", name, j);
                GLint loc = glGetUniformLocation_(p, brak ? elem : name);
                if(loc < 0) continue;
                benchuniform &u = uniforms.add();
                u.loc = loc;
                u.type = type;
                if(benchfloattype(type)) glGetUniformfv_(p, loc, u.f);
                else if(benchuinttype(type)) glGetUniformuiv_(p, loc, u.u);
                else glGetUniformiv_(p, loc, u.i);
            }
        }
        if(glGetActiveUniformBlockiv_)
        {
            glGetProgramiv_(p, GL_ACTIVE_UNIFORM_BLOCKS, &n);
            loopi(n) glGetActiveUniformBlockiv_(p, i, GL_UNIFORM_BLOCK_BINDING, &blockbindings.add());
        }
    }

    void restore()
    {
        glUseProgram_(program);
        loopv(uniforms)
        {
            benchuniform &u = uniforms[i];
            switch(u.type)
            {
                case GL_FLOAT: glUniform1fv_(u.loc, 1, u.f); break;
                case GL_FLOAT_VEC2: glUniform2fv_(u.loc, 1, u.f); break;
                case GL_FLOAT_VEC3: glUniform3fv_(u.loc, 1, u.f); break;
                case GL_FLOAT_VEC4: glUniform4fv_(u.loc, 1, u.f); break;
                case GL_FLOAT_MAT2: glUniformMatrix2fv_(u.loc, 1, GL_FALSE, u.f); break;
                case GL_FLOAT_MAT3: glUniformMatrix3fv_(u.loc, 1, GL_FALSE, u.f); break;
                case GL_FLOAT_MAT4: glUniformMatrix4fv_(u.loc, 1, GL_FALSE, u.f); break;
                case GL_UNSIGNED_INT: glUniform1uiv_(u.loc, 1, u.u); break;
                case GL_UNSIGNED_INT_VEC2: glUniform2uiv_(u.loc, 1, u.u); break;
                case GL_UNSIGNED_INT_VEC3: glUniform3uiv_(u.loc, 1, u.u); break;
                case GL_UNSIGNED_INT_VEC4: glUniform4uiv_(u.loc, 1, u.u); break;
                default:
                    switch(gltypecomponents(u.type))
                    {
                        case 1: glUniform1iv_(u.loc, 1, u.i); break;
                        case 2: glUniform2iv_(u.loc, 1, u.i); break;
                        case 3: glUniform3iv_(u.loc, 1, u.i); break;
                        case 4: glUniform4iv_(u.loc, 1, u.i); break;
                    }
                    break;
            }
        }
        loopv(blockbindings) glUniformBlockBinding_(program, i, blockbindings[i]);
    }
};

static void reportbench(const char *name, const char *status, double maxerr, int cov, int seeds, const char *reason)
{
    conoutf(colourwhite, "SHADERBENCH %s %s maxerr=%g cov=%d seeds=%d%s%s", name, status, maxerr, cov, seeds, reason && reason[0] ? " reason=" : "", reason ? reason : "");
}

// Not inline in the ICOMMAND: a top-level comma in the body would split the
// macro's arguments.
static void shaderbench(const char *run, const char *hash, const char *name, int seeds)
{
    int numseeds = seeds > 0 ? min(seeds, 16) : 4;
    if(!validfield(run) || !validfield(hash)) { reportbench(name, "FAIL", 0, 0, numseeds, "oldsource"); return; }
    Shader *live = lookupshaderbyname(name);
    if(!live || !live->program) { reportbench(name, "FAIL", 0, 0, numseeds, "missing"); return; }

    defformatstring(vspath, "shadercorpus/%s/blobs/%s/vs.full.glsl", run, hash);
    defformatstring(fspath, "shadercorpus/%s/blobs/%s/fs.full.glsl", run, hash);
    size_t vslen = 0, fslen = 0;
    char *vs = loadfile(vspath, &vslen, false), *fs = loadfile(fspath, &fslen, false);
    if(!vs || !fs) { DELETEA(vs); DELETEA(fs); reportbench(name, "FAIL", 0, 0, numseeds, "oldsource"); return; }

    gle::disable();
    benchprogramstate livestate;
    livestate.save(live->program);
    benchglstate state;
    state.save(livestate.samplers);
    GLuint vao = 0;
    glGenVertexArrays_(1, &vao);
    glBindVertexArray_(vao);

    GLuint old = linkoldprogram(vs, fs, *live);
    delete[] vs;
    delete[] fs;

    GLuint fbo = 0, targets[BENCHTARGETS];
    glGenFramebuffers_(1, &fbo);
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
    glGenTextures(BENCHTARGETS, targets);
    // Draw only to the attachments the shader declares an output for: an
    // enabled attachment no output writes is undefined by the spec, so a
    // driver could fill it differently for the two programs. The rest stay
    // at BENCHCLEAR in both renders (benchrender clears all of them first).
    // No declared outputs at all (gl_FragColor): draw to every attachment.
    GLenum allbufs[BENCHTARGETS], bufs[BENCHTARGETS];
    vector<FragDataLoc> outs;
    scanfragdatalocs(*live, outs);
    bool declared = false;
    loopi(BENCHTARGETS) bufs[i] = GL_NONE;
    loopv(outs) if(!outs[i].index && outs[i].loc >= 0 && outs[i].loc < BENCHTARGETS)
    {
        bufs[outs[i].loc] = GL_COLOR_ATTACHMENT0 + outs[i].loc;
        declared = true;
    }
    loopi(BENCHTARGETS)
    {
        glBindTexture(GL_TEXTURE_2D, targets[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, BENCHSIZE, BENCHSIZE, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, targets[i], 0);
        allbufs[i] = GL_COLOR_ATTACHMENT0 + i;
        if(!declared) bufs[i] = allbufs[i];
    }
    glDrawBuffers_(BENCHTARGETS, allbufs);
    bool fbook = glCheckFramebufferStatus_(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    const char *status = "PASS";
    string reason;
    reason[0] = '\0';
    double maxerr = 0;
    int bestcov = 0;
    if(!old) { status = "FAIL"; copystring(reason, "oldcompile"); }
    else if(!fbook) { status = "FAIL"; copystring(reason, "fbo"); }
    else
    {
        loopi(numseeds)
        {
            benchinputs in;
            prepareinputs(live->program, i + 1, in);
            vector<float> a, b;
            benchrender(fbo, allbufs, bufs, old, live->program, i + 1, in, a);
            benchrender(fbo, allbufs, bufs, live->program, live->program, i + 1, in, b);
            int written = 0;
            benchcompare(a, b, maxerr, written);
            bestcov = max(bestcov, written*100/(BENCHSIZE*BENCHSIZE));
            if(in.reason[0] && !reason[0]) copystring(reason, in.reason);
        }
        if(maxerr > BENCHTOLERANCE) status = "FAIL";
        else if(reason[0]) status = "WEAK";
        else if(bestcov < 50) { status = "WEAK"; copystring(reason, "coverage"); }
    }

    if(old) glDeleteProgram_(old);
    glDeleteTextures(BENCHTARGETS, targets);
    glDeleteFramebuffers_(1, &fbo);
    if(vao) glDeleteVertexArrays_(1, &vao);
    livestate.restore();
    state.restore();
    reportbench(name, status, maxerr, bestcov, numseeds, reason);
}

ICOMMAND(0, shaderbench, "sssi", (char *run, char *hash, char *name, int *seeds),
{
    if(identflags&IDF_MAP) return;
    shaderbench(run, hash, name, *seeds);
});
#endif
