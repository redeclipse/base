// shader.cpp: OpenGL GLSL shader management

#include "engine.h"

Shader *Shader::lastshader = NULL;

Shader *nullshader = NULL, *hudshader = NULL, *hudnotextureshader = NULL, *textureshader = NULL, *notextureshader = NULL, *nocolorshader = NULL, *foggedshader = NULL, *foggednotextureshader = NULL, *stdworldshader = NULL;

static hashnameset<GlobalShaderParamState> globalparams(256);
static hashtable<const char *, int> localparams(256);
static hashnameset<Shader> shaders(256);
static Shader *slotshader = NULL;
static vector<SlotShaderParam> slotparams;
static bool standardshaders = false, forceshaders = true, loadedshaders = false;

VAR(0, reservevpparams, 1, 16, 0);
VAR(0, maxvsuniforms, 1, 0, 0);
VAR(0, maxfsuniforms, 1, 0, 0);
VAR(0, maxvaryings, 1, 0, 0);
VAR(0, dbgshader, 0, 0, 2);

void loadshaders()
{
    standardshaders = true;
    execfile("config/glsl.cfg");
    standardshaders = false;

    nullshader = lookupshaderbyname("null");
    hudshader = lookupshaderbyname("hud");
    hudnotextureshader = lookupshaderbyname("hudnotexture");
    stdworldshader = lookupshaderbyname("stdworld");
    if(!nullshader || !hudshader || !hudnotextureshader || !stdworldshader) fatal("cannot find shader definitions");

    dummyslot.shader = stdworldshader;

    textureshader = lookupshaderbyname("texture");
    notextureshader = lookupshaderbyname("notexture");
    nocolorshader = lookupshaderbyname("nocolor");
    foggedshader = lookupshaderbyname("fogged");
    foggednotextureshader = lookupshaderbyname("foggednotexture");

    nullshader->set();

    loadedshaders = true;
}

Shader *lookupshaderbyname(const char *name)
{
    Shader *s = shaders.access(name);
    return s && s->loaded() ? s : NULL;
}

Shader *generateshader(const char *name, const char *fmt, ...)
{
    if(!loadedshaders) return NULL; 
    Shader *s = name ? lookupshaderbyname(name) : NULL;
    if(!s)
    {
        defvformatstring(cmd, fmt, fmt);
        standardshaders = true;
        execute(cmd, true); 
        standardshaders = false;
        s = name ? lookupshaderbyname(name) : NULL;
        if(!s) s = nullshader;
    }
    return s;
}

static void showglslinfo(GLenum type, GLuint obj, const char *name, const char **parts = NULL, int numparts = 0)
{
    GLint length = 0;
    if(type) glGetShaderiv_(obj, GL_INFO_LOG_LENGTH, &length);
    else glGetProgramiv_(obj, GL_INFO_LOG_LENGTH, &length);
    if(length > 1)
    {
        conoutf("\frGLSL ERROR (%s:%s)", type == GL_VERTEX_SHADER ? "VS" : (type == GL_FRAGMENT_SHADER ? "FS" : "PROG"), name);
        FILE *l = getlogfile();
        if(l)
        {
            GLchar *log = new GLchar[length];
            if(type) glGetShaderInfoLog_(obj, length, &length, log);
            else glGetProgramInfoLog_(obj, length, &length, log);
            fprintf(l, "%s\n", log);
            bool partlines = log[0] != '0';
            int line = 0;
            loopi(numparts)
            {
                const char *part = parts[i];
                int startline = line;
                while(*part)
                {
                    const char *next = strchr(part, '\n');
                    if(++line > 1000) goto done;
                    if(partlines) fprintf(l, "%d(%d): ", i, line - startline); else fprintf(l, "%d: ", line);
                    fwrite(part, 1, next ? next - part + 1 : strlen(part), l);
                    if(!next) { fputc('\n', l); break; }
                    part = next + 1;
                }
            }
        done:
            delete[] log;
        }
    }
}

static void compileglslshader(GLenum type, GLuint &obj, const char *def, const char *name, bool msg = true)
{
    const char *source = def + strspn(def, " \t\r\n");
    const char *parts[16];
    int numparts = 0;
    static const struct { int version; const char * const header; } glslversions[] =
    {
        { 120, "#version 120\n" }
    };
    loopi(sizeof(glslversions)/sizeof(glslversions[0])) if(glslversion >= glslversions[i].version)
    {
        parts[numparts++] = glslversions[i].header;
        break;
    }

    parts[numparts++] = "#extension GL_ARB_texture_rectangle : enable\n";

    parts[numparts++] = source;

    obj = glCreateShader_(type);
    glShaderSource_(obj, numparts, (const GLchar **)parts, NULL);
    glCompileShader_(obj);
    GLint success;
    glGetShaderiv_(obj, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        if(msg) showglslinfo(type, obj, name, parts, numparts);
        glDeleteShader_(obj);
        obj = 0;
    }
    else if(dbgshader > 1 && msg) showglslinfo(type, obj, name, parts, numparts);
}

VAR(0, dbgubo, 0, 0, 1);

static void bindglsluniform(Shader &s, UniformLoc &u)
{
    u.loc = glGetUniformLocation_(s.program, u.name);
    if(!u.blockname || !hasUBO) return;
    GLuint bidx = glGetUniformBlockIndex_(s.program, u.blockname);
    GLuint uidx = GL_INVALID_INDEX;
    glGetUniformIndices_(s.program, 1, &u.name, &uidx);
    if(bidx != GL_INVALID_INDEX && uidx != GL_INVALID_INDEX)
    {
        GLint sizeval = 0, offsetval = 0, strideval = 0;
        glGetActiveUniformBlockiv_(s.program, bidx, GL_UNIFORM_BLOCK_DATA_SIZE, &sizeval);
        if(sizeval <= 0) return;
        glGetActiveUniformsiv_(s.program, 1, &uidx, GL_UNIFORM_OFFSET, &offsetval);
        if(u.stride > 0)
        {
            glGetActiveUniformsiv_(s.program, 1, &uidx, GL_UNIFORM_ARRAY_STRIDE, &strideval);
            if(strideval > u.stride) return;
        }
        u.offset = offsetval;
        u.size = sizeval;
        glUniformBlockBinding_(s.program, bidx, u.binding);
        if(dbgubo) conoutf("UBO: %s:%s:%d, offset: %d, size: %d, stride: %d", u.name, u.blockname, u.binding, offsetval, sizeval, strideval);
    }
}

static void linkglslprogram(Shader &s, bool msg = true)
{
    s.program = s.vsobj && s.psobj ? glCreateProgram_() : 0;
    GLint success = 0;
    if(s.program)
    {
        glAttachShader_(s.program, s.vsobj);
        glAttachShader_(s.program, s.psobj);
        loopv(s.attriblocs)
        {
            AttribLoc &a = s.attriblocs[i];
            glBindAttribLocation_(s.program, a.loc, a.name);
        }
        glLinkProgram_(s.program);
        glGetProgramiv_(s.program, GL_LINK_STATUS, &success);
    }
    if(success)
    {
        glUseProgram_(s.program);
        loopi(8)
        {
            static const char * const texnames[8] = { "tex0", "tex1", "tex2", "tex3", "tex4", "tex5", "tex6", "tex7" };
            GLint loc = glGetUniformLocation_(s.program, texnames[i]);
            if(loc != -1) glUniform1i_(loc, i);
        }
        loopv(s.defaultparams)
        {
            SlotShaderParamState &param = s.defaultparams[i];
            param.loc = glGetUniformLocation_(s.program, param.name);
        }
        loopv(s.uniformlocs) bindglsluniform(s, s.uniformlocs[i]);
        glUseProgram_(0);
    }
    else if(s.program)
    {
        if(msg) showglslinfo(GL_FALSE, s.program, s.name);
        glDeleteProgram_(s.program);
        s.program = 0;
    }
}

int getlocalparam(const char *name)
{
    return localparams.access(name, int(localparams.numelems));
}

static int addlocalparam(Shader &s, const char *name, int loc, int size, GLenum format)
{
    int idx = getlocalparam(name);
    if(idx >= s.localparamremap.length())
    {
        int n = idx + 1 - s.localparamremap.length();
        memset(s.localparamremap.pad(n), 0xFF, n);
    }
    s.localparamremap[idx] = s.localparams.length();

    LocalShaderParamState &l = s.localparams.add();
    l.name = name;
    l.loc = loc;
    l.size = size;
    l.format = format;
    return idx;
}

GlobalShaderParamState *getglobalparam(const char *name)
{
    GlobalShaderParamState *param = globalparams.access(name);
    if(!param)
    {
        param = &globalparams[name];
        param->name = name;
        memset(param->buf, -1, sizeof(param->buf));
        param->version = -1;
    }
    return param;
}

static GlobalShaderParamUse *addglobalparam(Shader &s, GlobalShaderParamState *param, int loc, int size, GLenum format)
{
    GlobalShaderParamUse &g = s.globalparams.add();
    g.param = param;
    g.version = -2;
    g.loc = loc;
    g.size = size;
    g.format = format;
    return &g;
}

static void setglsluniformformat(Shader &s, const char *name, GLenum format, int size)
{
    switch(format)
    {
        case GL_FLOAT:
        case GL_FLOAT_VEC2:
        case GL_FLOAT_VEC3:
        case GL_FLOAT_VEC4:
        case GL_INT:
        case GL_INT_VEC2:
        case GL_INT_VEC3:
        case GL_INT_VEC4:
        case GL_BOOL:
        case GL_BOOL_VEC2:
        case GL_BOOL_VEC3:
        case GL_BOOL_VEC4:
        case GL_FLOAT_MAT2:
        case GL_FLOAT_MAT3:
        case GL_FLOAT_MAT4:
            break;
        default:
            return;
    }
    if(!strncmp(name, "gl_", 3)) return;

    int loc = glGetUniformLocation_(s.program, name);
    if(loc < 0) return;
    loopvj(s.defaultparams) if(s.defaultparams[j].loc == loc)
    {
        s.defaultparams[j].format = format;
        return;
    }
    loopvj(s.uniformlocs) if(s.uniformlocs[j].loc == loc) return;
    loopvj(s.globalparams) if(s.globalparams[j].loc == loc) return;
    loopvj(s.localparams) if(s.localparams[j].loc == loc) return;

    name = getshaderparamname(name);
    GlobalShaderParamState *param = globalparams.access(name);
    if(param) addglobalparam(s, param, loc, size, format);
    else addlocalparam(s, name, loc, size, format);
}

static void allocglslactiveuniforms(Shader &s)
{
    GLint numactive = 0;
    glGetProgramiv_(s.program, GL_ACTIVE_UNIFORMS, &numactive);
    string name;
    loopi(numactive)
    {
        GLsizei namelen = 0;
        GLint size = 0;
        GLenum format = GL_FLOAT_VEC4;
        name[0] = '\0';
        glGetActiveUniform_(s.program, i, sizeof(name)-1, &namelen, &size, &format, name);
        if(namelen <= 0 || size <= 0) continue;
        name[clamp(int(namelen), 0, (int)sizeof(name)-2)] = '\0';
        char *brak = strchr(name, '[');
        if(brak) *brak = '\0';
        setglsluniformformat(s, name, format, size);
    }
}

void Shader::allocparams(Slot *slot)
{
    if(slot)
    {
#define UNIFORMTEX(name, tmu) \
        { \
            loc = glGetUniformLocation_(program, name); \
            int val = tmu; \
            if(loc != -1) glUniform1i_(loc, val); \
        }
        int loc, tmu = 2;
        if(type & SHADER_NORMALSLMS)
        {
            UNIFORMTEX("lmcolor", 1);
            UNIFORMTEX("lmdir", 2);
            tmu++;
        }
        else UNIFORMTEX("lightmap", 1);
        if(type & SHADER_ENVMAP) UNIFORMTEX("envmap", tmu++);
        UNIFORMTEX("shadowmap", 7);
        int stex = 0;
        loopv(slot->sts)
        {
            Slot::Tex &t = slot->sts[i];
            switch(t.type)
            {
                case TEX_DIFFUSE: UNIFORMTEX("diffusemap", 0); break;
                case TEX_NORMAL: UNIFORMTEX("normalmap", tmu++); break;
                case TEX_GLOW: UNIFORMTEX("glowmap", tmu++); break;
                case TEX_DECAL: UNIFORMTEX("decal", tmu++); break;
                case TEX_SPEC: if(t.combined<0) UNIFORMTEX("specmap", tmu++); break;
                case TEX_DEPTH: if(t.combined<0) UNIFORMTEX("depthmap", tmu++); break;
                case TEX_UNKNOWN:
                {
                    defformatstring(sname, "stex%d", stex++);
                    UNIFORMTEX(sname, tmu++);
                    break;
                }
            }
        }
    }
    allocglslactiveuniforms(*this);
}

int GlobalShaderParamState::nextversion = 0;

void GlobalShaderParamState::resetversions()
{
    enumerate(shaders, Shader, s,
    {
        loopv(s.globalparams)
        {
            GlobalShaderParamUse &u = s.globalparams[i];
            if(u.version != u.param->version) u.version = -2;
        }
    });
    nextversion = 0;
    enumerate(globalparams, GlobalShaderParamState, g, { g.version = ++nextversion; });
    enumerate(shaders, Shader, s,
    {
        loopv(s.globalparams)
        {
            GlobalShaderParamUse &u = s.globalparams[i];
            if(u.version >= 0) u.version = u.param->version;
        }
    });
}

static SlotShaderParamValue *findslotparam(Slot &s, const char *name)
{
    loopv(s.params)
    {
        SlotShaderParam &param = s.params[i];
        if(name == param.name) return &param;
    }
    loopv(s.shader->defaultparams)
    {
        SlotShaderParamState &param = s.shader->defaultparams[i];
        if(name == param.name) return &param;
    }
    return NULL;
}

static SlotShaderParamValue *findslotparam(VSlot &s, const char *name)
{
    loopv(s.params)
    {
        SlotShaderParam &param = s.params[i];
        if(name == param.name) return &param;
    }
    return findslotparam(*s.slot, name);
}

static inline float *findslotparam(VSlot &s, const char *name, float *noval)
{
    SlotShaderParamValue *p = findslotparam(s, name);
    return p ? p->val : noval;
}
 
static inline void setslotparam(SlotShaderParamState &l, const float *val)
{
    switch(l.format)
    {
        case GL_BOOL:
        case GL_FLOAT:      glUniform1fv_(l.loc, 1, val); break;
        case GL_BOOL_VEC2:
        case GL_FLOAT_VEC2: glUniform2fv_(l.loc, 1, val); break;
        case GL_BOOL_VEC3:
        case GL_FLOAT_VEC3: glUniform3fv_(l.loc, 1, val); break;
        case GL_BOOL_VEC4:
        case GL_FLOAT_VEC4: glUniform4fv_(l.loc, 1, val); break;
        case GL_INT:      glUniform1i_(l.loc, int(val[0])); break;
        case GL_INT_VEC2: glUniform2i_(l.loc, int(val[0]), int(val[1])); break;
        case GL_INT_VEC3: glUniform3i_(l.loc, int(val[0]), int(val[1]), int(val[2])); break;
        case GL_INT_VEC4: glUniform4i_(l.loc, int(val[0]), int(val[1]), int(val[2]), int(val[3])); break;
    }
}

#define SETSLOTPARAM(l, mask, i, val) do { \
    if(!(mask&(1<<i))) { \
        mask |= 1<<i; \
        setslotparam(l, val); \
    } \
} while(0)

#define SETSLOTPARAMS(slotparams) \
    loopv(slotparams) \
    { \
        SlotShaderParam &p = slotparams[i]; \
        if(!defaultparams.inrange(p.loc)) continue; \
        SlotShaderParamState &l = defaultparams[p.loc]; \
        SETSLOTPARAM(l, unimask, p.loc, p.val); \
    }
#define SETDEFAULTPARAMS \
    loopv(defaultparams) \
    { \
        SlotShaderParamState &l = defaultparams[i]; \
        SETSLOTPARAM(l, unimask, i, l.val); \
    }

void Shader::setslotparams(Slot &slot)
{
    uint unimask = 0;
    SETSLOTPARAMS(slot.params)
    SETDEFAULTPARAMS
}

void Shader::setslotparams(Slot &slot, VSlot &vslot)
{
    uint unimask = 0;
    if(vslot.slot == &slot)
    {
        SETSLOTPARAMS(vslot.params)
        SETSLOTPARAMS(slot.params)
        SETDEFAULTPARAMS
    }
    else
    {
        SETSLOTPARAMS(slot.params)
        SETDEFAULTPARAMS
    }
}

void Shader::bindprograms()
{
    if(this == lastshader || type&(SHADER_DEFERRED|SHADER_INVALID)) return;
    glUseProgram_(program);
    lastshader = this;
}

VARF(0, shaderprecision, 0, 0, 2, initwarning("shader quality"));

bool Shader::compile()
{
    if(!vsstr) vsobj = !reusevs || reusevs->invalid() ? 0 : reusevs->vsobj;
    else compileglslshader(GL_VERTEX_SHADER,   vsobj, vsstr, name, dbgshader || !variantshader);
    if(!psstr) psobj = !reuseps || reuseps->invalid() ? 0 : reuseps->psobj;
    else compileglslshader(GL_FRAGMENT_SHADER, psobj, psstr, name, dbgshader || !variantshader);
    linkglslprogram(*this, !variantshader);
    return program!=0;
}

void Shader::cleanup(bool invalid)
{
    detailshader = NULL;
    used = false;
    if(vsobj) { if(!reusevs) glDeleteShader_(vsobj); vsobj = 0; }
    if(psobj) { if(!reuseps) glDeleteShader_(psobj); psobj = 0; }
    if(program) { glDeleteProgram_(program); program = 0; }
    localparams.setsize(0);
    localparamremap.setsize(0);
    globalparams.setsize(0);
    if(standard || invalid)
    {
        type = SHADER_INVALID;
        DELETEA(vsstr);
        DELETEA(psstr);
        DELETEA(defer);
        variants.setsize(0);
        DELETEA(variantrows);
        defaultparams.setsize(0);
        attriblocs.setsize(0);
        uniformlocs.setsize(0);
        altshader = NULL;
        loopi(MAXSHADERDETAIL) fastshader[i] = this;
        reusevs = reuseps = NULL;
    }
    else loopv(defaultparams) defaultparams[i].loc = -1;
}

static void genattriblocs(Shader &s, const char *vs, const char *ps, Shader *reusevs, Shader *reuseps)
{
    static int len = strlen("#pragma CUBE2_attrib");
    string name;
    int loc;
    if(reusevs) s.attriblocs = reusevs->attriblocs;
    else while((vs = strstr(vs, "#pragma CUBE2_attrib")))
    {
        if(sscanf(vs, "#pragma CUBE2_attrib %100s %d", name, &loc) == 2)
            s.attriblocs.add(AttribLoc(getshaderparamname(name), loc));
        vs += len;
    }
}

static void genuniformlocs(Shader &s, const char *vs, const char *ps, Shader *reusevs, Shader *reuseps)
{
    static int len = strlen("#pragma CUBE2_uniform");
    string name, blockname;
    int binding, stride;
    if(reusevs) s.uniformlocs = reusevs->uniformlocs;
    else while((vs = strstr(vs, "#pragma CUBE2_uniform")))
    {
        int numargs = sscanf(vs, "#pragma CUBE2_uniform %100s %100s %d %d", name, blockname, &binding, &stride);
        if(numargs >= 3) s.uniformlocs.add(UniformLoc(getshaderparamname(name), getshaderparamname(blockname), binding, numargs >= 4 ? stride : 0));
        else if(numargs >= 1) s.uniformlocs.add(UniformLoc(getshaderparamname(name)));
        vs += len;
    }
}

Shader *newshader(int type, const char *name, const char *vs, const char *ps, Shader *variant = NULL, int row = 0)
{
    if(Shader::lastshader)
    {
        glUseProgram_(0);
        Shader::lastshader = NULL;
    }

    Shader *exists = shaders.access(name);
    char *rname = exists ? exists->name : newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    s.vsstr = newstring(vs);
    s.psstr = newstring(ps);
    DELETEA(s.defer);
    s.type = type;
    s.variantshader = variant;
    s.standard = standardshaders;
    if(forceshaders) s.forced = true;
    s.reusevs = s.reuseps = NULL;
    if(variant)
    {
        int row = 0, col = 0;
        if(!vs[0] || sscanf(vs, "%d , %d", &row, &col) >= 1)
        {
            DELETEA(s.vsstr);
            s.reusevs = !vs[0] ? variant : variant->getvariant(col, row);
        }
        row = col = 0;
        if(!ps[0] || sscanf(ps, "%d , %d", &row, &col) >= 1)
        {
            DELETEA(s.psstr);
            s.reuseps = !ps[0] ? variant : variant->getvariant(col, row);
        }
    }
    if(variant) loopv(variant->defaultparams) s.defaultparams.add(variant->defaultparams[i]);
    else loopv(slotparams) s.defaultparams.add(slotparams[i]);
    s.attriblocs.setsize(0);
    s.uniformlocs.setsize(0);
    genattriblocs(s, vs, ps, s.reusevs, s.reuseps);
    genuniformlocs(s, vs, ps, s.reusevs, s.reuseps);
    if(!s.compile())
    {
        s.cleanup(true);
        if(variant) shaders.remove(rname);
        return NULL;
    }
    if(variant) variant->addvariant(row, &s);
    s.fixdetailshader();
    return &s;
}

void setupshaders()
{
    GLint val;
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &val);
    maxvsuniforms = val/4;
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &val);
    maxfsuniforms = val/4;
    glGetIntegerv(GL_MAX_VARYING_FLOATS, &val);
    maxvaryings = val;

    standardshaders = true;
    nullshader = newshader(0, "<init>null",
        "void main(void) {\n"
        "    gl_Position = gl_Vertex;\n"
        "}\n",
        "void main(void) {\n"
        "    gl_FragColor = vec4(1.0, 0.0, 1.0, 0.0);\n"
        "}\n");
    hudshader = newshader(0, "<init>hud",
       "uniform mat4 hudmatrix;\n"
       "varying vec2 texcoord0;\n"
       "varying vec4 color;\n"
       "void main(void) {\n"
        "    gl_Position = hudmatrix * gl_Vertex;\n"
        "    texcoord0 = gl_MultiTexCoord0.xy;\n"
        "    color = gl_Color;\n"
        "}\n",
        "varying vec2 texcoord0;\n"
        "varying vec4 color;\n"
        "uniform sampler2D tex0;\n"
        "void main(void) {\n"
        "    gl_FragColor = color * texture2D(tex0, texcoord0);\n"
        "}\n");
    hudnotextureshader = newshader(0, "<init>hudnotexture",
        "uniform mat4 hudmatrix;\n"
        "varying vec4 color;\n"
        "void main(void) {\n"
        "    gl_Position = hudmatrix * gl_Vertex;\n"
        "    color = gl_Color;\n"
        "}\n",
        "varying vec4 color;\n"
        "void main(void) {\n"
        "    gl_FragColor = color;\n"
        "}\n");
    standardshaders = false;

    if(!nullshader || !hudshader || !hudnotextureshader) fatal("failed to setup shaders");

    dummyslot.shader = nullshader;
}

static const char *findglslmain(const char *s)
{
    const char *main = strstr(s, "main");
    if(!main) return NULL;
    for(; main >= s; main--) switch(*main) { case '\r': case '\n': case ';': return main + 1; }
    return s;
}

static void gengenericvariant(Shader &s, const char *sname, const char *vs, const char *ps, int row = 0)
{
    int rowoffset = 0;
    bool vschanged = false, pschanged = false;
    vector<char> vsv, psv;
    vsv.put(vs, strlen(vs)+1);
    psv.put(ps, strlen(ps)+1);

    static const int len = strlen("#pragma CUBE2_variant"), olen = strlen("override");
    for(char *vspragma = vsv.getbuf();; vschanged = true)
    {
        vspragma = strstr(vspragma, "#pragma CUBE2_variant");
        if(!vspragma) break;
        if(sscanf(vspragma + len, "row %d", &rowoffset) == 1) continue;
        memset(vspragma, ' ', len);
        vspragma += len;
        if(!strncmp(vspragma, "override", olen))
        {
            memset(vspragma, ' ', olen);
            vspragma += olen;
            char *end = vspragma + strcspn(vspragma, "\n\r");
            end += strspn(end, "\n\r");
            int endlen = strcspn(end, "\n\r");
            memset(end, ' ', endlen);
        }
    }
    for(char *pspragma = psv.getbuf();; pschanged = true)
    {
        pspragma = strstr(pspragma, "#pragma CUBE2_variant");
        if(!pspragma) break;
        if(sscanf(pspragma + len, "row %d", &rowoffset) == 1) continue;
        memset(pspragma, ' ', len);
        pspragma += len;
        if(!strncmp(pspragma, "override", olen))
        {
            memset(pspragma, ' ', olen);
            pspragma += olen;
            char *end = pspragma + strcspn(pspragma, "\n\r");
            end += strspn(end, "\n\r");
            int endlen = strcspn(end, "\n\r");
            memset(end, ' ', endlen);
        }
    }
    row += rowoffset; 
    if(row < 0 || row >= MAXVARIANTROWS) return;
    int col = s.numvariants(row);
    defformatstring(varname, "<variant:%d,%d>%s", col, row, sname);
    string reuse;
    if(col) formatstring(reuse, "%d", row);
    else copystring(reuse, "");
    newshader(s.type, varname, vschanged ? vsv.getbuf() : reuse, pschanged ? psv.getbuf() : reuse, &s, row);
}

static bool genwatervariant(Shader &s, const char *sname, const char *vs, const char *ps, int row = 2)
{
    if(!strstr(vs, "#pragma CUBE2_water") && !strstr(ps, "#pragma CUBE2_water")) return false;

    vector<char> vsw, psw;

    const char *vsmain = findglslmain(vs), *vsend = strrchr(vs, '}');
    if(!vsmain || !vsend) return false;
    vsw.put(vs, vsmain - vs);
    const char *fadeparams = "\nuniform vec4 waterfadeparams;\nvarying float fadedepth;\n";
    vsw.put(fadeparams, strlen(fadeparams));
    vsw.put(vsmain, vsend - vsmain);
    const char *fadedef = "\nfadedepth = gl_Vertex.z*waterfadeparams.x + waterfadeparams.y;\n";
    vsw.put(fadedef, strlen(fadedef));
    vsw.put(vsend, strlen(vsend)+1);
        
    const char *psmain = findglslmain(ps), *psend = strrchr(ps, '}');
    if(!psmain || !psend) return false;
    psw.put(ps, psmain - ps);
    const char *fadeinterp = "\nvarying float fadedepth;\n";
    psw.put(fadeinterp, strlen(fadeinterp));
    psw.put(psmain, psend - psmain);
    const char *fade = "\ngl_FragColor.a = fadedepth;\n";
    psw.put(fade, strlen(fade));
    psw.put(psend, strlen(psend)+1);

    defformatstring(name, "<water>%s", sname);
    Shader *variant = newshader(s.type, name, vsw.getbuf(), psw.getbuf(), &s, row);
    return variant!=NULL;
}

bool minimizedynlighttcusage() { return maxvaryings < 48; }

static void gendynlightvariant(Shader &s, const char *sname, const char *vs, const char *ps, int row = 0)
{
    int numlights = minimizedynlighttcusage() ? 1 : MAXDYNLIGHTS;

    const char *vspragma = strstr(vs, "#pragma CUBE2_dynlight"), *pspragma = strstr(ps, "#pragma CUBE2_dynlight");
    string pslight;
    vspragma += strcspn(vspragma, "\n");
    if(*vspragma) vspragma++;

    if(sscanf(pspragma, "#pragma CUBE2_dynlight %100s", pslight)!=1) return;

    pspragma += strcspn(pspragma, "\n");
    if(*pspragma) pspragma++;

    const char *vsmain = findglslmain(vs), *psmain = findglslmain(ps);
    if(vsmain > vspragma) vsmain = vs;
    if(psmain > pspragma) psmain = ps;

    vector<char> vsdl, psdl;
    loopi(MAXDYNLIGHTS)
    {
        vsdl.setsize(0);
        psdl.setsize(0);
        if(vsmain >= vs) vsdl.put(vs, vsmain - vs);
        if(psmain >= ps) psdl.put(ps, psmain - ps);

        defformatstring(pos, "uniform vec4 dynlightpos[%d];\n", i+1);
        vsdl.put(pos, strlen(pos));
        psdl.put(pos, strlen(pos));
        defformatstring(color, "uniform vec3 dynlightcolor[%d];\n", i+1);
        psdl.put(color, strlen(color));

        loopk(min(i+1, numlights))
        {
            defformatstring(dir, "%sdynlight%ddir%s", !k ? "varying vec3 " : " ", k, k==i || k+1==numlights ? ";\n" : ",");
            vsdl.put(dir, strlen(dir));
            psdl.put(dir, strlen(dir));
        }

        vsdl.put(vsmain, vspragma-vsmain);
        psdl.put(psmain, pspragma-psmain);

        loopk(i+1)
        {
            defformatstring(tc, 
                k<numlights ? 
                    "dynlight%ddir = gl_Vertex.xyz*dynlightpos[%d].w + dynlightpos[%d].xyz;\n" :
                    "vec3 dynlight%ddir = dynlight0dir*dynlightpos[%d].w + dynlightpos[%d].xyz;\n",     
                k, k, k);
            if(k < numlights) vsdl.put(tc, strlen(tc));
            else psdl.put(tc, strlen(tc));

            defformatstring(dl, 
                "%s.rgb += dynlightcolor[%d] * (1.0 - clamp(dot(dynlight%ddir, dynlight%ddir), 0.0, 1.0));\n",
                pslight, k, k, k);
            psdl.put(dl, strlen(dl));
        }

        vsdl.put(vspragma, strlen(vspragma)+1);
        psdl.put(pspragma, strlen(pspragma)+1);

        defformatstring(name, "<dynlight %d>%s", i+1, sname);
        Shader *variant = newshader(s.type, name, vsdl.getbuf(), psdl.getbuf(), &s, row);
        if(!variant) return;
        if(row < 4) genwatervariant(s, name, vsdl.getbuf(), psdl.getbuf(), row+2);
    }
}

static void genshadowmapvariant(Shader &s, const char *sname, const char *vs, const char *ps, int row = 1)
{
    const char *vspragma = strstr(vs, "#pragma CUBE2_shadowmap"), *pspragma = strstr(ps, "#pragma CUBE2_shadowmap");
    string pslight;
    vspragma += strcspn(vspragma, "\n");
    if(*vspragma) vspragma++;

    if(sscanf(pspragma, "#pragma CUBE2_shadowmap %100s", pslight)!=1) return;

    pspragma += strcspn(pspragma, "\n");
    if(*pspragma) pspragma++;

    const char *vsmain = findglslmain(vs), *psmain = findglslmain(ps);
    if(vsmain > vspragma) vsmain = vs;
    if(psmain > pspragma) psmain = ps;

    vector<char> vssm, pssm;
    if(vsmain >= vs) vssm.put(vs, vsmain - vs);
    if(psmain >= ps) pssm.put(ps, psmain - ps);

    const char *vsdecl =
        "uniform mat4 shadowmapproject;\n"
        "varying vec3 shadowmaptc;\n";
    vssm.put(vsdecl, strlen(vsdecl));

    const char *psdecl =
        "uniform sampler2D shadowmap;\n"
        "uniform vec4 shadowmapambient;\n"
        "varying vec3 shadowmaptc;\n";
    pssm.put(psdecl, strlen(psdecl));

    vssm.put(vsmain, vspragma-vsmain);
    pssm.put(psmain, pspragma-psmain);

    extern int smoothshadowmappeel;
    const char *tcgen =
        "shadowmaptc = vec3(shadowmapproject * gl_Vertex);\n";
    vssm.put(tcgen, strlen(tcgen));
    const char *sm =
        smoothshadowmappeel ?
            "vec4 smvals = texture2D(shadowmap, shadowmaptc.xy);\n"
            "vec2 smdiff = clamp(smvals.xz - shadowmaptc.zz*smvals.y, 0.0, 1.0);\n"
            "float shadowed = clamp((smdiff.x > 0.0 ? smvals.w : 0.0) - 8.0*smdiff.y, 0.0, 1.0);\n" :

            "vec4 smvals = texture2D(shadowmap, shadowmaptc.xy);\n"
            "float smtest = shadowmaptc.z*smvals.y;\n"
            "float shadowed = smtest < smvals.x && smtest > smvals.z ? smvals.w : 0.0;\n";
    pssm.put(sm, strlen(sm));
    defformatstring(smlight, 
        "%s.rgb -= shadowed*clamp(%s.rgb - shadowmapambient.rgb, 0.0, 1.0);\n",
        pslight, pslight);
    pssm.put(smlight, strlen(smlight));

    vssm.put(vspragma, strlen(vspragma)+1);
    pssm.put(pspragma, strlen(pspragma)+1);

    defformatstring(name, "<shadowmap>%s", sname);
    Shader *variant = newshader(s.type, name, vssm.getbuf(), pssm.getbuf(), &s, row);
    if(!variant) return;
    genwatervariant(s, name, vssm.getbuf(), pssm.getbuf(), row+2);

    if(strstr(vs, "#pragma CUBE2_dynlight")) gendynlightvariant(s, name, vssm.getbuf(), pssm.getbuf(), row);
}

static void genfogshader(vector<char> &vsbuf, vector<char> &psbuf, const char *vs, const char *ps)
{
    const char *vspragma = strstr(vs, "#pragma CUBE2_fog"), *pspragma = strstr(ps, "#pragma CUBE2_fog");
    if(!vspragma && !pspragma) return;
    static const int pragmalen = strlen("#pragma CUBE2_fog");
    const char *vsmain = findglslmain(vs), *vsend = strrchr(vs, '}');
    if(vsmain && vsend)
    {
        vsbuf.put(vs, vsmain - vs);
        const char *fogparams = "\nuniform vec4 fogplane;\nvarying float fogcoord;\n";
        vsbuf.put(fogparams, strlen(fogparams));
        vsbuf.put(vsmain, vsend - vsmain);
        const char *vsfog = "\nfogcoord = dot(fogplane, gl_Position);\n";
        vsbuf.put(vsfog, strlen(vsfog));
        vsbuf.put(vsend, strlen(vsend)+1);
    }
    const char *psmain = findglslmain(ps), *psend = strrchr(ps, '}');
    if(psmain && psend)
    {
        psbuf.put(ps, psmain - ps);
        const char *fogparams =
            "\nuniform vec3 fogcolor;\n"
            "uniform vec2 fogparams;\n"
            "varying float fogcoord;\n";
        psbuf.put(fogparams, strlen(fogparams));
        psbuf.put(psmain, psend - psmain);
        const char *psdef = "\n#define FOG_COLOR ";
        const char *psfog =
            pspragma && !strncmp(pspragma+pragmalen, "rgba", 4) ?
                "\ngl_FragColor = mix((FOG_COLOR), gl_FragColor, clamp(fogcoord*fogparams.x + fogparams.y, 0.0, 1.0));\n" :
                "\ngl_FragColor.rgb = mix((FOG_COLOR).rgb, gl_FragColor.rgb, clamp(fogcoord*fogparams.x + fogparams.y, 0.0, 1.0));\n";
        int clen = 0;
        if(pspragma)
        {
            pspragma += pragmalen;
            while(iscubealpha(*pspragma)) pspragma++;
            while(*pspragma && !iscubespace(*pspragma)) pspragma++;
            pspragma += strspn(pspragma, " \t\v\f");
            clen = strcspn(pspragma, "\r\n");
        }
        if(clen <= 0) { pspragma = "fogcolor"; clen = strlen(pspragma); }
        psbuf.put(psdef, strlen(psdef));
        psbuf.put(pspragma, clen);
        psbuf.put(psfog, strlen(psfog));
        psbuf.put(psend, strlen(psend)+1);
    }
}

static void genuniformdefs(vector<char> &vsbuf, vector<char> &psbuf, const char *vs, const char *ps, Shader *variant = NULL)
{
    if(variant ? variant->defaultparams.empty() : slotparams.empty()) return;
    const char *vsmain = findglslmain(vs), *psmain = findglslmain(ps);
    if(!vsmain || !psmain) return;
    vsbuf.put(vs, vsmain - vs);
    psbuf.put(ps, psmain - ps);
    if(variant) loopv(variant->defaultparams)
    {
        defformatstring(uni, "\nuniform vec4 %s;\n", variant->defaultparams[i].name);
        vsbuf.put(uni, strlen(uni));
        psbuf.put(uni, strlen(uni));
    }
    else loopv(slotparams)
    {
        defformatstring(uni, "\nuniform vec4 %s;\n", slotparams[i].name);
        vsbuf.put(uni, strlen(uni));
        psbuf.put(uni, strlen(uni));
    }
    vsbuf.put(vsmain, strlen(vsmain)+1);
    psbuf.put(psmain, strlen(psmain)+1);
}

VAR(0, defershaders, 0, 1, 1);

void defershader(int *type, const char *name, const char *contents)
{
    Shader *exists = shaders.access(name);
    if(exists && !exists->invalid()) return;
    if(!defershaders) { execute(contents, true); return; }
    char *rname = exists ? exists->name : newstring(name);
    Shader &s = shaders[rname];
    s.name = rname;
    DELETEA(s.defer);
    s.defer = newstring(contents);
    s.type = SHADER_DEFERRED | *type;
    s.standard = standardshaders;
}

void Shader::force()
{
    if(!deferred()) return;

    char *cmd = defer;
    defer = NULL;
    bool wasstandard = standardshaders, wasforcing = forceshaders;
    standardshaders = standard;
    forceshaders = false;
    slotparams.shrink(0);
    execute(cmd, true);
    forceshaders = wasforcing;
    standardshaders = wasstandard;
    delete[] cmd;

    if(deferred())
    {
        DELETEA(defer);
        type = SHADER_INVALID;
    }
}

void fixshaderdetail()
{
    // must null out separately because fixdetailshader can recursively set it
    enumerate(shaders, Shader, s, { if(!s.forced) s.detailshader = NULL; });
    enumerate(shaders, Shader, s, { if(s.forced) s.fixdetailshader(); });
    linkslotshaders();
}

int Shader::uniformlocversion()
{
    static int version = 0;
    if(++version >= 0) return version;
    version = 0;
    enumerate(shaders, Shader, s, { loopvj(s.uniformlocs) s.uniformlocs[j].version = -1; });
    return version;
}

VARF(IDF_PERSIST, shaderdetail, 0, MAXSHADERDETAIL, MAXSHADERDETAIL, fixshaderdetail());

void Shader::fixdetailshader(bool shouldforce, bool recurse)
{
    Shader *alt = this;
    detailshader = NULL;
    do
    {
        Shader *cur = shaderdetail < MAXSHADERDETAIL ? alt->fastshader[shaderdetail] : alt;
        if(cur->deferred() && shouldforce) cur->force();
        if(!cur->invalid())
        {
            if(cur->deferred()) break;
            detailshader = cur;
            break;
        }
        alt = alt->altshader;
    } while(alt && alt!=this);

    if(recurse && detailshader) loopv(detailshader->variants) detailshader->variants[i]->fixdetailshader(shouldforce, false);
}

Shader *useshaderbyname(const char *name)
{
    Shader *s = shaders.access(name);
    if(!s) return NULL;
    if(!s->detailshader) s->fixdetailshader();
    s->forced = true;
    return s;
}

void shader(int *type, char *name, char *vs, char *ps)
{
    if(lookupshaderbyname(name)) return;

    defformatstring(info, "shader %s", name);
    progress(loadprogress, info);

    vector<char> vsbuf, psbuf, vsbak, psbak;
#define GENSHADER(cond, body) \
    if(cond) \
    { \
        if(vsbuf.length()) { vsbak.setsize(0); vsbak.put(vs, strlen(vs)+1); vs = vsbak.getbuf(); vsbuf.setsize(0); } \
        if(psbuf.length()) { psbak.setsize(0); psbak.put(ps, strlen(ps)+1); ps = psbak.getbuf(); psbuf.setsize(0); } \
        body; \
        if(vsbuf.length()) vs = vsbuf.getbuf(); \
        if(psbuf.length()) ps = psbuf.getbuf(); \
    }
    GENSHADER(slotparams.length(), genuniformdefs(vsbuf, psbuf, vs, ps));
    GENSHADER(strstr(vs, "#pragma CUBE2_fog") || strstr(ps, "#pragma CUBE2_fog"), genfogshader(vsbuf, psbuf, vs, ps));
    Shader *s = newshader(*type, name, vs, ps);
    if(s)
    {
        if(strstr(vs, "#pragma CUBE2_water")) genwatervariant(*s, s->name, vs, ps);
        if(strstr(vs, "#pragma CUBE2_shadowmap")) genshadowmapvariant(*s, s->name, vs, ps);
        if(strstr(vs, "#pragma CUBE2_dynlight")) gendynlightvariant(*s, s->name, vs, ps);
    }
    slotparams.shrink(0);
}

void variantshader(int *type, char *name, int *row, char *vs, char *ps)
{
    if(*row < 0)
    {
        shader(type, name, vs, ps);
        return;
    }
    else if(*row >= MAXVARIANTROWS) return;

    Shader *s = lookupshaderbyname(name);
    if(!s) return;

    defformatstring(varname, "<variant:%d,%d>%s", s->numvariants(*row), *row, name);
    //defformatstring(info, "shader %s", varname);
    //progress(loadprogress, info);
    vector<char> vsbuf, psbuf, vsbak, psbak;
    GENSHADER(s->defaultparams.length(), genuniformdefs(vsbuf, psbuf, vs, ps, s));
    GENSHADER(strstr(vs, "#pragma CUBE2_fog") || strstr(ps, "#pragma CUBE2_fog"), genfogshader(vsbuf, psbuf, vs, ps));
    Shader *v = newshader(*type, varname, vs, ps, s, *row);
    if(v)
    {
        if(strstr(vs, "#pragma CUBE2_dynlight")) gendynlightvariant(*s, varname, vs, ps, *row);
        if(strstr(ps, "#pragma CUBE2_variant") || strstr(vs, "#pragma CUBE2_variant")) gengenericvariant(*s, varname, vs, ps, *row);
    }
}

void setshader(char *name)
{
    slotparams.shrink(0);
    Shader *s = shaders.access(name);
    if(!s)
    {
        conoutf("\frno such shader: %s", name);
    }
    else slotshader = s;
}

void resetslotshader()
{
    slotshader = NULL;
    slotparams.shrink(0);
}

void setslotshader(Slot &s)
{
    s.shader = slotshader;
    if(!s.shader)
    {
        s.shader = stdworldshader;
        return;
    }
    loopv(slotparams) s.params.add(slotparams[i]);
}

static void linkslotshaderparams(vector<SlotShaderParam> &params, Shader *sh, bool load)
{
    if(sh) loopv(params)
    {
        int loc = -1;
        SlotShaderParam &param = params[i];
        loopv(sh->defaultparams)
        {
            SlotShaderParamState &dparam = sh->defaultparams[i];
            if(dparam.name==param.name)
            {
                if(param.palette != dparam.palette || param.palindex != dparam.palindex ||
                   memcmp(param.val, dparam.val, sizeof(param.val)))
                    loc = i;
                break;
            }
        }
        param.loc = loc;
    }
    else if(load) loopv(params) params[i].loc = -1;
}

void linkslotshader(Slot &s, bool load)
{
    if(!s.shader) return;

    if(load && !s.shader->detailshader) s.shader->fixdetailshader();

    linkslotshaderparams(s.params, s.shader->detailshader, load);
}

void linkvslotshader(VSlot &s, bool load)
{
    if(!s.slot->shader) return;

    Shader *sh = s.slot->shader->detailshader;
    linkslotshaderparams(s.params, sh, load);

    if(!sh) return;

    if(s.slot->texmask&(1<<TEX_GLOW))
    {
        static const char *paramname = getshaderparamname("glowcolor");
        s.glowcolor = findslotparam(s, paramname);
    }
}

void altshader(char *origname, char *altname)
{
    Shader *orig = shaders.access(origname), *alt = shaders.access(altname);
    if(!orig || !alt) return;
    orig->altshader = alt;
    orig->fixdetailshader(false);
}

void fastshader(char *nice, char *fast, int *detail)
{
    Shader *ns = shaders.access(nice), *fs = shaders.access(fast);
    if(!ns || !fs) return;
    loopi(min(*detail+1, MAXSHADERDETAIL)) ns->fastshader[i] = fs;
    ns->fixdetailshader(false);
}

COMMAND(0, shader, "isss");
COMMAND(0, variantshader, "isiss");
COMMAND(0, setshader, "s");
COMMAND(0, altshader, "ss");
COMMAND(0, fastshader, "ssi");
COMMAND(0, defershader, "iss");
ICOMMAND(0, forceshader, "s", (const char *name), useshaderbyname(name));

ICOMMAND(0, isshaderdefined, "s", (char *name), intret(lookupshaderbyname(name) ? 1 : 0));

static hashset<const char *> shaderparamnames(256);

const char *getshaderparamname(const char *name, bool insert)
{
    const char *exists = shaderparamnames.find(name, NULL);
    if(exists || !insert) return exists;
    return shaderparamnames.add(newstring(name));
}

void addslotparam(const char *name, float x, float y, float z, float w, int palette = 0, int palindex = 0)
{   
    if(name) name = getshaderparamname(name);
    loopv(slotparams)
    {
        SlotShaderParam &param = slotparams[i];
        if(param.name==name)
        {
            param.palette = palette;
            param.palindex = palindex;
            param.val[0] = x;
            param.val[1] = y;
            param.val[2] = z;
            param.val[3] = w;
            return;
        }
    }
    slotparams.add(SlotShaderParam(name, palette, palindex, x, y, z, w));
}

ICOMMAND(0, setuniformparam, "sffffii", (char *name, float *x, float *y, float *z, float *w, int *p, int *pidx), addslotparam(name, *x, *y, *z, *w, *p, *pidx));
ICOMMAND(0, setshaderparam, "sffffii", (char *name, float *x, float *y, float *z, float *w, int *p, int *pidx), addslotparam(name, *x, *y, *z, *w, *p, *pidx));
ICOMMAND(0, defuniformparam, "sffffii", (char *name, float *x, float *y, float *z, float *w, int *p, int *pidx), addslotparam(name, *x, *y, *z, *w, *p, *pidx));

#define NUMPOSTFXBINDS 10

struct postfxtex
{
    GLuint id;
    int scale, used;

    postfxtex() : id(0), scale(0), used(-1) {}
};
vector<postfxtex> postfxtexs;
int postfxbinds[NUMPOSTFXBINDS];
GLuint postfxfb = 0;
int postfxw = 0, postfxh = 0;

struct postfxpass
{
    Shader *shader;
    vec4 params;
    uint inputs, freeinputs;
    int outputbind, outputscale;

    postfxpass() : shader(NULL), inputs(1), freeinputs(1), outputbind(0), outputscale(0) {}
};
vector<postfxpass> postfxpasses;

static int allocatepostfxtex(int scale)
{
    loopv(postfxtexs)
    {
        postfxtex &t = postfxtexs[i];
        if(t.scale==scale && t.used < 0) return i;
    }
    postfxtex &t = postfxtexs.add();
    t.scale = scale;
    glGenTextures(1, &t.id);
    createtexture(t.id, max(screen->w>>scale, 1), max(screen->h>>scale, 1), NULL, 3, 1, GL_RGB, GL_TEXTURE_RECTANGLE);
    return postfxtexs.length()-1;
}

void cleanuppostfx(bool fullclean)
{
    if(fullclean && postfxfb)
    {
        glDeleteFramebuffers_(1, &postfxfb);
        postfxfb = 0;
    }

    loopv(postfxtexs) glDeleteTextures(1, &postfxtexs[i].id);
    postfxtexs.shrink(0);

    postfxw = 0;
    postfxh = 0;
}

void renderpostfx()
{
    extern int viewtype;
    if(postfxpasses.empty() || viewtype) return;

    if(postfxw != screen->w || postfxh != screen->h)
    {
        cleanuppostfx(false);
        postfxw = screen->w;
        postfxh = screen->h;
    }

    int binds[NUMPOSTFXBINDS];
    loopi(NUMPOSTFXBINDS) binds[i] = -1;
    loopv(postfxtexs) postfxtexs[i].used = -1;

    binds[0] = allocatepostfxtex(0);
    postfxtexs[binds[0]].used = 0;
    glBindTexture(GL_TEXTURE_RECTANGLE, postfxtexs[binds[0]].id);
    glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, 0, 0, screen->w, screen->h);

    if(postfxpasses.length() > 1)
    {
        if(!postfxfb) glGenFramebuffers_(1, &postfxfb);
        glBindFramebuffer_(GL_FRAMEBUFFER, postfxfb);
    }

    GLOBALPARAMF(millis, lastmillis/1000.0f);

    loopv(postfxpasses)
    {
        postfxpass &p = postfxpasses[i];

        int tex = -1;
        if(!postfxpasses.inrange(i+1))
        {
            if(postfxpasses.length() > 1) glBindFramebuffer_(GL_FRAMEBUFFER, 0);
        }
        else
        {
            tex = allocatepostfxtex(p.outputscale);
            glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, postfxtexs[tex].id, 0);
        }

        int w = tex >= 0 ? max(screen->w>>postfxtexs[tex].scale, 1) : screen->w,
            h = tex >= 0 ? max(screen->h>>postfxtexs[tex].scale, 1) : screen->h;
        glViewport(0, 0, w, h);
        p.shader->set();
        LOCALPARAM(params, p.params);
        int tw = w, th = h, tmu = 0;
        loopj(NUMPOSTFXBINDS) if(p.inputs&(1<<j) && binds[j] >= 0)
        {
            if(!tmu)
            {
                tw = max(screen->w>>postfxtexs[binds[j]].scale, 1);
                th = max(screen->h>>postfxtexs[binds[j]].scale, 1);
            }
            else glActiveTexture_(GL_TEXTURE0 + tmu);
            glBindTexture(GL_TEXTURE_RECTANGLE, postfxtexs[binds[j]].id);
            ++tmu;
        }
        if(tmu) glActiveTexture_(GL_TEXTURE0);
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0,  0);  glVertex2f(-1, -1);
        glTexCoord2f(tw, 0);  glVertex2f( 1, -1);
        glTexCoord2f(0,  th); glVertex2f(-1,  1);
        glTexCoord2f(tw, th); glVertex2f( 1,  1);
        glEnd();

        loopj(NUMPOSTFXBINDS) if(p.freeinputs&(1<<j) && binds[j] >= 0)
        {
            postfxtexs[binds[j]].used = -1;
            binds[j] = -1;
        }
        if(tex >= 0)
        {
            if(binds[p.outputbind] >= 0) postfxtexs[binds[p.outputbind]].used = -1;
            binds[p.outputbind] = tex;
            postfxtexs[tex].used = p.outputbind;
        }
    }
}

static bool addpostfx(const char *name, int outputbind, int outputscale, uint inputs, uint freeinputs, const vec4 &params)
{
    if(!*name) return false;
    Shader *s = useshaderbyname(name);
    if(!s)
    {
        conoutf("no such postfx shader: %s", name);
        return false;
    }
    postfxpass &p = postfxpasses.add();
    p.shader = s;
    p.outputbind = outputbind;
    p.outputscale = outputscale;
    p.inputs = inputs;
    p.freeinputs = freeinputs;
    p.params = params;
    return true;
}

void clearpostfx()
{
    postfxpasses.shrink(0);
    cleanuppostfx(false);
}

COMMAND(0, clearpostfx, "");

ICOMMAND(0, addpostfx, "siisffff", (char *name, int *bind, int *scale, char *inputs, float *x, float *y, float *z, float *w),
{
    int inputmask = inputs[0] ? 0 : 1;
    int freemask = inputs[0] ? 0 : 1;
    bool freeinputs = true;
    for(; *inputs; inputs++) if(isdigit(*inputs))
    {
        inputmask |= 1<<(*inputs-'0');
        if(freeinputs) freemask |= 1<<(*inputs-'0');
    }
    else if(*inputs=='+') freeinputs = false;
    else if(*inputs=='-') freeinputs = true;
    inputmask &= (1<<NUMPOSTFXBINDS)-1;
    freemask &= (1<<NUMPOSTFXBINDS)-1;
    addpostfx(name, clamp(*bind, 0, NUMPOSTFXBINDS-1), max(*scale, 0), inputmask, freemask, vec4(*x, *y, *z, *w));
});

ICOMMAND(0, setpostfx, "sffff", (char *name, float *x, float *y, float *z, float *w),
{
    clearpostfx();
    if(name[0]) addpostfx(name, 0, 0, 1, 1, vec4(*x, *y, *z, *w));
});

void cleanupshaders()
{
    cleanuppostfx(true);

    nullshader = hudshader = hudnotextureshader = textureshader = notextureshader = nocolorshader = foggedshader = foggednotextureshader = stdworldshader = NULL;
    enumerate(shaders, Shader, s, s.cleanup());
    Shader::lastshader = NULL;
    glUseProgram_(0);
}

void reloadshaders()
{
    loadshaders();
    linkslotshaders();
    enumerate(shaders, Shader, s,
    {
        if(!s.standard && !(s.type&(SHADER_DEFERRED|SHADER_INVALID)) && !s.variantshader)
        {
            defformatstring(info, "shader %s", s.name);
            progress(0.0, info);
            if(!s.compile()) s.cleanup(true);
            loopv(s.variants)
            {
                Shader *v = s.variants[i];
                if((v->reusevs && v->reusevs->invalid()) ||
                   (v->reuseps && v->reuseps->invalid()) ||
                   !v->compile())
                    v->cleanup(true);
            }
        }
        if(s.forced && !s.detailshader) s.fixdetailshader();
    });
}

void setupblurkernel(int radius, float sigma, float *weights, float *offsets)
{
    if(radius<1 || radius>MAXBLURRADIUS) return;
    sigma *= 2*radius;
    float total = 1.0f/sigma;
    weights[0] = total;
    offsets[0] = 0;
    // rely on bilinear filtering to sample 2 pixels at once
    // transforms a*X + b*Y into (u+v)*[X*u/(u+v) + Y*(1 - u/(u+v))]
    loopi(radius)
    {
        float weight1 = exp(-((2*i)*(2*i)) / (2*sigma*sigma)) / sigma,
              weight2 = exp(-((2*i+1)*(2*i+1)) / (2*sigma*sigma)) / sigma,
              scale = weight1 + weight2,
              offset = 2*i+1 + weight2 / scale;
        weights[i+1] = scale;
        offsets[i+1] = offset;
        total += 2*scale;
    }
    loopi(radius+1) weights[i] /= total;
    for(int i = radius+1; i <= MAXBLURRADIUS; i++) weights[i] = offsets[i] = 0;
}

void setblurshader(int pass, int size, int radius, float *weights, float *offsets, GLenum target)
{
    if(radius<1 || radius>MAXBLURRADIUS) return;
    static Shader *blurshader[7][2] = { { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL } },
                  *blurrectshader[7][2] = { { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL }, { NULL, NULL } };
    Shader *&s = (target == GL_TEXTURE_RECTANGLE ? blurrectshader : blurshader)[radius-1][pass];
    if(!s)
    {
        defformatstring(name, "blur%c%d%s", 'x'+pass, radius, target == GL_TEXTURE_RECTANGLE ? "rect" : "");
        s = lookupshaderbyname(name);
    }
    s->set();
    LOCALPARAMV(weights, weights, 8);
    float scaledoffsets[8];
    loopk(8) scaledoffsets[k] = offsets[k]/size;
    LOCALPARAMV(offsets, scaledoffsets, 8);
}

