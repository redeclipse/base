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

// Map content decides these, so the per-map pass dumps only them.
static bool mapdependent(Shader &s)
{
    return s.mapdef || (s.origin && !strncmp(s.origin, "grassshader", strlen("grassshader")));
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
        if(*mapsonly && !mapdependent(s)) continue;
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
#endif
