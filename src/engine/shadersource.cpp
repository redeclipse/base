// shadersource.cpp: the shader_new loader, see shadersource.h.

#include "engine.h"
#include "shadersource.h"

namespace shadersource
{
    bool validpath(const char *path)
    {
        static const char prefix[] = "config/glsl/";
        static const int prefixlen = sizeof(prefix) - 1;
        if(strncmp(path, prefix, prefixlen) || strpbrk(path, "\\:")) return false;
        for(const char *seg = path + prefixlen;;)
        {
            const char *end = strchr(seg, '/');
            int len = end ? int(end - seg) : int(strlen(seg));
            if(!len || (len == 1 && seg[0] == '.') || (len == 2 && seg[0] == '.' && seg[1] == '.')) return false;
            if(!end) return true;
            seg = end + 1;
        }
    }

    bool validdefine(const char *name, const char *value)
    {
        if(!isalpha(uchar(name[0])) && name[0] != '_') return false;
        for(const char *c = name + 1; *c; c++) if(!isalnum(uchar(*c)) && *c != '_') return false;
        if(strpbrk(value, "\r\n")) return false;
        size_t len = strlen(value);
        // A trailing '\' splices the next assembled line into the macro, and
        // GLSL 4.20+ honours the splice.
        return !len || value[len-1] != '\\';
    }

    void appenddefine(vector<char> &out, const char *name, const char *value)
    {
        out.put("#define ", 8);
        out.put(name, strlen(name));
        if(*value)
        {
            out.add(' ');
            out.put(value, strlen(value));
        }
        out.add('\n');
    }

    void appendtext(vector<char> &out, const char *text)
    {
        char last = '\n';
        for(const char *c = text; *c; c++) if(*c != '\r')
        {
            out.add(*c);
            last = *c;
        }
        if(last != '\n') out.add('\n');
    }

    void assemblestage(vector<char> &out, const vector<char> &defines, const vector<const char *> &includes, const char *body)
    {
        out.setsize(0);
        if(body)
        {
            if(defines.length()) out.put(defines.getbuf(), defines.length());
            loopv(includes) appendtext(out, includes[i]);
            appendtext(out, body);
        }
        out.add('\0');
    }
}

// What a shader_new/variantshader_new body has described so far. Bodies can
// nest (a body may run a generator that defines another shader), so each run
// keeps its own record and restores the outer one afterwards.
struct shaderbuild
{
    vector<char> defines;
    vector<char *> includes[2]; // paths; [0] vertex, [1] fragment
    string source[2];
    bool failed;

    shaderbuild() : failed(false) { source[0][0] = source[1][0] = '\0'; }
    ~shaderbuild() { loopi(2) includes[i].deletearrays(); }
};
static shaderbuild *curbuild = NULL;

static shaderbuild *getbuild(const char *cmd)
{
    if(!curbuild) conoutf(colourred, "%s: only valid inside a shader_new or variantshader_new body", cmd);
    return curbuild;
}

static bool checkpath(shaderbuild &b, const char *cmd, const char *path)
{
    // source[2] is a fixed 'string' (MAXSTRLEN), unlike the newstring'd
    // include paths, so it would otherwise truncate silently instead of
    // being refused like an invalid path.
    if(shadersource::validpath(path) && strlen(path) < MAXSTRLEN) return true;
    conoutf(colourred, "%s: refusing \"%s\": shader sources must be relative paths under config/glsl/", cmd, path);
    b.failed = true;
    return false;
}

ICOMMAND(0, shader_define, "ss", (char *name, char *value),
{
    shaderbuild *b = getbuild("shader_define");
    if(!b) return;
    if(!shadersource::validdefine(name, value))
    {
        conoutf(colourred, "shader_define: invalid define \"%s\"", name);
        b->failed = true;
        return;
    }
    shadersource::appenddefine(b->defines, name, value);
});

static void includesource(int stage, const char *cmd, const char *path)
{
    shaderbuild *b = getbuild(cmd);
    if(!b || !checkpath(*b, cmd, path)) return;
    b->includes[stage].add(newstring(path));
}
ICOMMAND(0, shader_include_vs, "s", (char *path), includesource(0, "shader_include_vs", path));
ICOMMAND(0, shader_include_fs, "s", (char *path), includesource(1, "shader_include_fs", path));

ICOMMAND(0, shader_source, "ss", (char *vs, char *fs),
{
    shaderbuild *b = getbuild("shader_source");
    if(!b) return;
    bool vsok = !vs[0] || checkpath(*b, "shader_source", vs);
    bool fsok = !fs[0] || checkpath(*b, "shader_source", fs);
    if(!vsok || !fsok) return;
    copystring(b->source[0], vs);
    copystring(b->source[1], fs);
});

// Reads one file of the build; NULL, with the reason logged, if it cannot.
// 'label' names the build in log messages -- the plain name for shader_new,
// or a variant-specific label for variantshader_new (see F4 in the review).
static char *loadsource(const char *label, const char *path)
{
    char *text = loadfile(path, NULL);
    if(!text) conoutf(colourred, "shader %s: cannot read %s", label, path);
    return text;
}

// Assembles one stage of the build into out. False if it could not be.
static bool buildstage(const char *label, shaderbuild &b, int stage, vector<char> &out)
{
    const char *kind = stage ? "fragment" : "vertex";
    if(!b.source[stage][0])
    {
        if(b.includes[stage].length())
        {
            conoutf(colourred, "shader %s: %s includes given but no %s source", label, kind, kind);
            return false;
        }
        out.setsize(0);
        out.add('\0');
        return true;
    }
    vector<const char *> texts;
    bool ok = true;
    loopv(b.includes[stage])
    {
        char *text = loadsource(label, b.includes[stage][i]);
        if(!text) { ok = false; break; }
        texts.add(text);
    }
    char *body = ok ? loadsource(label, b.source[stage]) : NULL;
    ok = body != NULL;
    if(ok)
    {
        shadersource::assemblestage(out, b.defines, texts, body);
        // No defines, no includes and an empty file all assemble to just the
        // terminating NUL. Left alone this reads as "no source for this
        // stage": variantshader_new would silently reuse the parent's stage,
        // and shader_new would report the misleading "needs both a vertex
        // and a fragment source" instead of naming the empty file.
        if(out.length() == 1)
        {
            conoutf(colourred, "shader %s: %s is empty", label, b.source[stage]);
            ok = false;
        }
    }
    texts.deletearrays();
    DELETEA(body);
    return ok;
}

// Runs a body with a fresh build record and assembles both stages. False if
// the body or a file failed; the reason is already logged.
static bool runbuild(const char *label, uint *body, vector<char> &vs, vector<char> &ps)
{
    shaderbuild b, *outer = curbuild;
    curbuild = &b;
    execute(body);
    curbuild = outer;
    if(!b.failed && buildstage(label, b, 0, vs) && buildstage(label, b, 1, ps)) return true;
    conoutf(colourred, "shader %s: not created", label);
    return false;
}

static void shadernew(int type, char *name, uint *body)
{
    // shader() would keep an existing shader anyway. resetshaders
    // invalidates standard shaders (Shader::cleanup -> SHADER_INVALID), so
    // the body still runs and re-reads its files on every resetshaders;
    // this early return only saves work when the shader is still loaded,
    // e.g. the same generator firing twice.
    if(lookupshaderbyname(name)) return;
    vector<char> vs, ps;
    if(!runbuild(name, body, vs, ps)) return;
    if(!vs[0] || !ps[0])
    {
        conoutf(colourred, "shader %s: needs both a vertex and a fragment source", name);
        return;
    }
    shader(type, name, vs.getbuf(), ps.getbuf());
}
ICOMMAND(0, shader_new, "ise", (int *type, char *name, uint *body), shadernew(*type, name, body));

static void variantshadernew(int type, char *name, int row, int maxvariants, uint *body)
{
    if(row < 0) { shadernew(type, name, body); return; }
    // variantshader() drops these too; checking first skips the body and files.
    if(row >= MAXVARIANTROWS || !lookupshaderbyname(name)) return;
    // A plain name in these messages would read as if the parent shader
    // failed; name the variant instead.
    defformatstring(label, "<variant row %d>%s", row, name);
    vector<char> vs, ps;
    if(!runbuild(label, body, vs, ps)) return;
    // An empty stage makes newshader reuse the parent's.
    variantshader(type, name, row, vs.getbuf(), ps.getbuf(), maxvariants);
}
ICOMMAND(0, variantshader_new, "isiie", (int *type, char *name, int *row, int *maxvariants, uint *body), variantshadernew(*type, name, *row, *maxvariants, body));
