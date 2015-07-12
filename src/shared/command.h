// script binding functionality

enum { VAL_NULL = 0, VAL_INT, VAL_FLOAT, VAL_STR, VAL_ANY, VAL_CODE, VAL_MACRO, VAL_IDENT };

enum
{
    CODE_START = 0,
    CODE_OFFSET,
    CODE_POP,
    CODE_ENTER,
    CODE_EXIT,
    CODE_VAL,
    CODE_VALI,
    CODE_MACRO,
    CODE_BOOL,
    CODE_BLOCK,
    CODE_COMPILE,
    CODE_FORCE,
    CODE_RESULT,
    CODE_IDENT, CODE_IDENTU, CODE_IDENTARG,
    CODE_COM, CODE_COMD, CODE_COMC, CODE_COMV,
    CODE_CONC, CODE_CONCW, CODE_CONCM, CODE_DOWN,
    CODE_SVAR, CODE_SVAR1,
    CODE_IVAR, CODE_IVAR1, CODE_IVAR2, CODE_IVAR3,
    CODE_FVAR, CODE_FVAR1,
    CODE_LOOKUP, CODE_LOOKUPU, CODE_LOOKUPARG, CODE_ALIAS, CODE_ALIASU, CODE_ALIASARG, CODE_CALL, CODE_CALLU, CODE_CALLARG,
    CODE_PRINT,
    CODE_LOCAL,

    CODE_OP_MASK = 0x3F,
    CODE_RET = 6,
    CODE_RET_MASK = 0xC0,

    /* return type flags */
    RET_NULL   = VAL_NULL<<CODE_RET,
    RET_STR    = VAL_STR<<CODE_RET,
    RET_INT    = VAL_INT<<CODE_RET,
    RET_FLOAT  = VAL_FLOAT<<CODE_RET,
};

enum { ID_VAR, ID_FVAR, ID_SVAR, ID_COMMAND, ID_ALIAS, ID_LOCAL };
#define VAR_MIN INT_MIN+1
#define VAR_MAX INT_MAX-1
#define FVAR_MIN -1e6f
#define FVAR_MAX 1e6f
#define FVAR_NONZERO 1e-6f

enum { IDF_PERSIST = 1<<0, IDF_READONLY = 1<<1, IDF_REWRITE = 1<<2, IDF_WORLD = 1<<3, IDF_COMPLETE = 1<<4, IDF_TEXTURE = 1<<5, IDF_CLIENT = 1<<6, IDF_SERVER = 1<<7, IDF_HEX = 1<<8, IDF_ADMIN = 1<<9, IDF_UNKNOWN = 1<<10, IDF_ARG = 1<<11, IDF_PRELOAD = 1<<12, IDF_GAMEPRELOAD = 1<<13 };

struct ident;

struct identval
{
    union
    {
        int i;      // ID_VAR, VAL_INT
        float f;    // ID_FVAR, VAL_FLOAT
        char *s;    // ID_SVAR, VAL_STR
        const uint *code; // VAL_CODE
        ident *id;  // VAL_IDENT
    };
};

struct tagval : identval
{
    int type;

    void setint(int val) { type = VAL_INT; i = val; }
    void setfloat(float val) { type = VAL_FLOAT; f = val; }
    void setstr(char *val) { type = VAL_STR; s = val; }
    void setnull() { type = VAL_NULL; i = 0; }
    void setcode(const uint *val) { type = VAL_CODE; code = val; }
    void setmacro(const uint *val) { type = VAL_MACRO; code = val; }
    void setident(ident *val) { type = VAL_IDENT; id = val; }

    const char *getstr() const;
    int getint() const;
    float getfloat() const;
    bool getbool() const;

    void cleanup();
};

struct identstack
{
    identval val;
    int valtype;
    identstack *next;
};

union identvalptr
{
    int *i;   // ID_VAR
    float *f; // ID_FVAR
    char **s; // ID_SVAR
};

typedef void (__cdecl *identfun)();

struct ident
{
    uchar type; // one of ID_* above
    union
    {
        uchar valtype; // ID_ALIAS
        uchar numargs; // ID_COMMAND
    };
    ushort flags;
    int index;
    const char *name;
    union
    {
        struct // ID_VAR, ID_FVAR, ID_SVAR
        {
            union
            {
                struct { int minval, maxval; };     // ID_VAR
                struct { float minvalf, maxvalf; }; // ID_FVAR
            };
            identvalptr storage;
            identval overrideval;
            identval def; // declared-default (by *init.cfg)
            identval bin; // builtin-default (hard coded or version.cfg)
        };
        struct // ID_ALIAS
        {
            uint *code;
            identval val;
            identstack *stack;
        };
        struct // ID_COMMAND
        {
            const char *args;
            uint argmask;
        };
    };
    identfun fun; // ID_VAR, ID_FVAR, ID_SVAR, ID_COMMAND
    char *desc, *usage;

    ident() {}
    // ID_VAR
    ident(int t, const char *n, int m, int c, int x, int *s, void *f = NULL, int flags = IDF_COMPLETE)
        : type(t), flags(flags | (m > x ? IDF_READONLY : 0)), name(n), minval(m), maxval(x), fun((identfun)f), desc(NULL), usage(NULL)
    { def.i = c; bin.i = c; storage.i = s; }
    // ID_FVAR
    ident(int t, const char *n, float m, float c, float x, float *s, void *f = NULL, int flags = IDF_COMPLETE)
        : type(t), flags(flags | (m > x ? IDF_READONLY : 0)), name(n), minvalf(m), maxvalf(x), fun((identfun)f), desc(NULL), usage(NULL)
    { def.f = c; bin.f = c; storage.f = s; }
    // ID_SVAR
    ident(int t, const char *n, char *c, char **s, void *f = NULL, int flags = IDF_COMPLETE)
        : type(t), flags(flags), name(n), fun((identfun)f), desc(NULL), usage(NULL)
    { def.s = c; bin.s = newstring(c); storage.s = s; }
    // ID_ALIAS
    ident(int t, const char *n, char *a, int flags)
        : type(t), valtype(VAL_STR), flags(flags), name(n), code(NULL), stack(NULL), desc(NULL), usage(NULL)
    { val.s = a; }
    ident(int t, const char *n, int a, int flags)
        : type(t), valtype(VAL_INT), flags(flags), name(n), code(NULL), stack(NULL), desc(NULL), usage(NULL)
    { val.i = a; }
    ident(int t, const char *n, float a, int flags)
        : type(t), valtype(VAL_FLOAT), flags(flags), name(n), code(NULL), stack(NULL), desc(NULL), usage(NULL)
    { val.f = a; }
    ident(int t, const char *n, int flags)
        : type(t), valtype(VAL_NULL), flags(flags), name(n), code(NULL), stack(NULL), desc(NULL), usage(NULL)
    {}
    ident(int t, const char *n, const tagval &v, int flags)
        : type(t), valtype(v.type), flags(flags), name(n), code(NULL), stack(NULL), desc(NULL), usage(NULL)
    { val = v; }
    // ID_COMMAND
    ident(int t, const char *n, const char *args, uint argmask, int numargs, void *f = NULL, int flags = IDF_COMPLETE)
        : type(t), numargs(numargs), flags(flags), name(n), args(args), argmask(argmask), fun((identfun)f), desc(NULL), usage(NULL)
    {}

    void changed() { if(fun) fun(); }

    void setval(const tagval &v)
    {
        valtype = v.type;
        val = v;
    }

    void setval(const identstack &v)
    {
        valtype = v.valtype;
        val = v.val;
    }

    void forcenull()
    {
        if(valtype==VAL_STR) delete[] val.s;
        valtype = VAL_NULL;
    }

    float getfloat() const;
    int getint() const;
    const char *getstr() const;
    void getval(tagval &v) const;

    static bool compare(ident *x, ident *y) { return strcmp(x->name, y->name) < 0; }
};

static inline bool htcmp(const char *key, const ident &id) { return !strcmp(key, id.name); }
extern hashset<ident> idents;

extern void addident(ident *id);

extern tagval *commandret;
extern const char *intstr(int v);
extern const char *intstr(ident *id);
extern void intret(int v);
extern const char *floatstr(float v);
extern void floatret(float v);
extern void stringret(char *s);
extern void result(tagval &v);
extern void result(const char *s);
extern char *conc(tagval *v, int n, bool space, const char *prefix, int prefixlen);

static inline char *conc(tagval *v, int n, bool space)
{
    return conc(v, n, space, NULL, 0);
}

static inline char *conc(tagval *v, int n, bool space, const char *prefix)
{
    return conc(v, n, space, prefix, strlen(prefix));
}

static inline int parseint(const char *s)
{
    return int(strtoul(s, NULL, 0));
}

static inline float parsefloat(const char *s)
{
    // not all platforms (windows) can parse hexadecimal integers via strtod
    char *end;
    double val = strtod(s, &end);
    return val || end==s || (*end!='x' && *end!='X') ? float(val) : float(parseint(s));
}

static inline void intformat(char *buf, int v, int len = 20) { nformatstring(buf, len, "%d", v); }
static inline void floatformat(char *buf, float v, int len = 20) { nformatstring(buf, len, v==int(v) ? "%.1f" : "%.7g", v); }

static inline const char *getstr(const identval &v, int type)
{
    switch(type)
    {
        case VAL_STR: case VAL_MACRO: return v.s;
        case VAL_INT: return intstr(v.i);
        case VAL_FLOAT: return floatstr(v.f);
        default: return "";
    }
}
inline const char *tagval::getstr() const { return ::getstr(*this, type); }
inline const char *ident::getstr() const { return ::getstr(val, valtype); }

static inline int getint(const identval &v, int type)
{
    switch(type)
    {
        case VAL_INT: return v.i;
        case VAL_FLOAT: return int(v.f);
        case VAL_STR: case VAL_MACRO: return parseint(v.s);
        default: return 0;
    }
}
inline int tagval::getint() const { return ::getint(*this, type); }
inline int ident::getint() const { return ::getint(val, valtype); }

static inline float getfloat(const identval &v, int type)
{
    switch(type)
    {
        case VAL_FLOAT: return v.f;
        case VAL_INT: return float(v.i);
        case VAL_STR: case VAL_MACRO: return parsefloat(v.s);
        default: return 0.0f;
    }
}
inline float tagval::getfloat() const { return ::getfloat(*this, type); }
inline float ident::getfloat() const { return ::getfloat(val, valtype); }

inline void ident::getval(tagval &v) const
{
    switch(valtype)
    {
        case VAL_STR: case VAL_MACRO: v.setstr(newstring(val.s)); break;
        case VAL_INT: v.setint(val.i); break;
        case VAL_FLOAT: v.setfloat(val.f); break;
        default: v.setnull(); break;
    }
}
extern int variable(const char *name, int min, int cur, int max, int *storage, identfun fun, int flags);
extern float fvariable(const char *name, float min, float cur, float max, float *storage, identfun fun, int flags);
extern char *svariable(const char *name, const char *cur, char **storage, identfun fun, int flags);
extern void setvar(const char *name, int i, bool dofunc = false, bool def = false);
extern void setfvar(const char *name, float f, bool dofunc = false, bool def = false);
extern void setsvar(const char *name, const char *str, bool dofunc = false, bool def = false);
extern void setvarchecked(ident *id, int val);
extern void setfvarchecked(ident *id, float val);
extern void setsvarchecked(ident *id, const char *val);
extern void touchvar(const char *name);
extern int getvar(const char *name);
extern int getvarmin(const char *name);
extern int getvarmax(const char *name);
extern int getvardef(const char *name, bool rb = false);
extern float getfvarmin(const char *name);
extern float getfvarmax(const char *name);
extern bool identexists(const char *name);
extern ident *getident(const char *name);
extern ident *newident(const char *name, int flags = 0);
extern ident *readident(const char *name);
extern ident *writeident(const char *name, int flags = 0);
extern bool addcommand(const char *name, identfun fun, const char *narg, int flags = IDF_COMPLETE);
extern bool addkeyword(int type, const char *name, int flags = 0);

extern uint *compilecode(const char *p);
extern void keepcode(uint *p);
extern void freecode(uint *p);
extern void executeret(const uint *code, tagval &result = *commandret);
extern void executeret(const char *p, tagval &result = *commandret);
extern char *executestr(const uint *code);
extern char *executestr(const char *p);
extern int execute(const uint *code);
extern int execute(const char *p);
extern int execute(const char *p, bool nonworld);
extern bool executebool(const uint *code);
extern bool executebool(const char *p);
enum { EXEC_NOWORLD = 1<<0, EXEC_VERSION = 1<<1, EXEC_BUILTIN = 1<<2 };
extern bool execfile(const char *cfgfile, bool msg = true, int flags = 0);
extern void alias(const char *name, const char *action);
extern void alias(const char *name, tagval &v);
extern void worldalias(const char *name, const char *action);
extern const char *getalias(const char *name);
extern void loopiter(ident *id, identstack &stack, const tagval &v);
extern void loopend(ident *id, identstack &stack);

#define loopstart(id, stack) if((id)->type != ID_ALIAS) return; identstack stack;
static inline void loopiter(ident *id, identstack &stack, int i) { tagval v; v.setint(i); loopiter(id, stack, v); }
static inline void loopiter(ident *id, identstack &stack, float f) { tagval v; v.setfloat(f); loopiter(id, stack, v); }
static inline void loopiter(ident *id, identstack &stack, const char *s) { tagval v; v.setstr(newstring(s)); loopiter(id, stack, v); }

extern int identflags;
extern bool interactive;

extern const char *escapestring(const char *s);
extern const char *escapeid(const char *s);
static inline const char *escapeid(ident &id) { return escapeid(id.name); }
extern bool validateblock(const char *s);
extern char *parsetext(const char *&p);
extern void explodelist(const char *s, vector<char *> &elems, int limit = -1);
extern int listlen(const char *s);
extern char *indexlist(const char *s, int pos);
extern const char *indexlist(const char *s, int pos, int &len);
extern int listincludes(const char *list, const char *needl, int needlelen);
extern char *shrinklist(const char *list, const char *limit, int failover = 0, bool invert = false);

extern void checksleep(int millis);
extern void clearsleep(bool clearworlds = true);

extern char *logtimeformat, *filetimeformat;
extern int filetimelocal;
extern const char *gettime(time_t ctime = 0, const char *format = NULL);

// nasty macros for registering script functions, abuses globals to avoid excessive infrastructure
#define KEYWORD(flags, name, type) UNUSED static bool __dummy_##name = addkeyword(type, #name, flags)
#define COMMANDN(flags, name, fun, nargs) UNUSED static bool __dummy_##fun = addcommand(#name, (identfun)fun, nargs, flags|IDF_COMPLETE)
#define COMMAND(flags, name, nargs) COMMANDN(flags, name, name, nargs)

// anonymous inline commands, uses nasty template trick with line numbers to keep names unique
#define ICOMMANDNS(flags, name, cmdname, nargs, proto, b) template<int N> struct cmdname; template<> struct cmdname<__LINE__> { static bool init; static void run proto; }; bool cmdname<__LINE__>::init = addcommand(name, (identfun)cmdname<__LINE__>::run, nargs, flags|IDF_COMPLETE); void cmdname<__LINE__>::run proto \
    { b; }
#define ICOMMANDN(flags, name, cmdname, nargs, proto, b) ICOMMANDNS(flags, #name, cmdname, nargs, proto, b)
#define ICOMMANDNAME(name) _icmd_##name
#define ICOMMAND(flags, name, nargs, proto, b) ICOMMANDN(flags, name, ICOMMANDNAME(name), nargs, proto, b)
#define ICOMMANDSNAME _icmds_
#define ICOMMANDS(flags, name, nargs, proto, b) ICOMMANDNS(flags, name, ICOMMANDSNAME, nargs, proto, b)

#define _VAR(name, global, min, cur, max, flags) int global = variable(#name, min, cur, max, &global, NULL, flags|IDF_COMPLETE)
#define VARN(flags, name, global, min, cur, max) _VAR(name, global, min, cur, max, flags)
#define VAR(flags, name, min, cur, max) _VAR(name, name, min, cur, max, flags)
#define _VARF(name, global, min, cur, max, body, flags)  void var_##name(); int global = variable(#name, min, cur, max, &global, var_##name, flags|IDF_COMPLETE); void var_##name() { body; }
#define VARFN(flags, name, global, min, cur, max, body) _VARF(name, global, min, cur, max, body, flags)
#define VARF(flags, name, min, cur, max, body) _VARF(name, name, min, cur, max, body, flags)

#define _FVAR(name, global, min, cur, max, flags) float global = fvariable(#name, min, cur, max, &global, NULL, flags|IDF_COMPLETE)
#define FVARN(flags, name, global, min, cur, max) _FVAR(name, global, min, cur, max, flags)
#define FVAR(flags, name, min, cur, max) _FVAR(name, name, min, cur, max, flags)
#define _FVARF(name, global, min, cur, max, body, flags) void var_##name(); float global = fvariable(#name, min, cur, max, &global, var_##name, flags|IDF_COMPLETE); void var_##name() { body; }
#define FVARFN(flags, name, global, min, cur, max, body) _FVARF(name, global, min, cur, max, body, flags)
#define FVARF(flags, name, min, cur, max, body) _FVARF(name, name, min, cur, max, body, flags)

#define _SVAR(name, global, cur, flags) char *global = svariable(#name, cur, &global, NULL, flags|IDF_COMPLETE)
#define SVARN(flags, name, global, cur) _SVAR(name, global, cur, flags)
#define SVAR(flags, name, cur) _SVAR(name, name, cur, flags)
#define _SVARF(name, global, cur, body, flags) void var_##name(); char *global = svariable(#name, cur, &global, var_##name, flags|IDF_COMPLETE); void var_##name() { body; }
#define SVARFN(flags, name, global, cur, body) _SVARF(name, global, cur, body, flags)
#define SVARF(flags, name, cur, body) _SVARF(name, name, cur, body, flags)

// game world controlling stuff
#define WITHWORLD(body) { int _oldflags = identflags; identflags |= IDF_WORLD; body; identflags = _oldflags; }
#define RUNWORLD(n) { ident *wid = idents.access(n); if(wid && wid->type==ID_ALIAS && wid->flags&IDF_WORLD) { WITHWORLD(execute(wid->getstr())); } }

#if defined(GAMEWORLD)
#define IDF_GAME (IDF_CLIENT|IDF_REWRITE)
#define G(name) (name)
#define PHYS(name) (G(name)*G(name##scale))
#define GICOMMAND(flags, n, g, proto, svbody, ccbody) ICOMMAND(flags|IDF_GAME, n, g, proto, ccbody)
#define GVARN(flags, name, global, min, cur, max) _VAR(name, global, min, cur, max, flags|IDF_GAME)
#define GVAR(flags, name, min, cur, max) _VAR(name, name, min, cur, max, flags|IDF_GAME)
#define GVARF(flags, name, min, cur, max, svbody, ccbody) _VARF(name, name, min, cur, max, ccbody, flags|IDF_GAME)
#define GFVARN(flags, name, global, min, cur, max) _FVAR(name, global, min, cur, max, flags|IDF_GAME)
#define GFVAR(flags, name, min, cur, max) _FVAR(name, name, min, cur, max, flags|IDF_GAME)
#define GFVARF(flags, name, min, cur, max, svbody, ccbody) _FVARF(name, name, min, cur, max, ccbody, flags|IDF_GAME)
#define GSVARN(flags, name, global, cur) _SVAR(name, global, cur, flags|IDF_GAME)
#define GSVAR(flags, name, cur) _SVAR(name, name, cur, flags|IDF_GAME)
#define GSVARF(flags, name, cur, svbody, ccbody) _SVARF(name, name, cur, ccbody, flags|IDF_GAME)
#elif defined(GAMESERVER)
#define G(name) (sv_##name)
#define IDF_GAME (IDF_SERVER|IDF_REWRITE)
#define GICOMMAND(flags, n, g, proto, svbody, ccbody) ICOMMAND(flags|(IDF_GAME&~IDF_REWRITE), sv_##n, g, proto, svbody)
#define GVARN(flags, name, global, min, cur, max) _VAR(sv_##name, global, min, cur, max, flags|IDF_GAME)
#define GVAR(flags, name, min, cur, max) _VAR(sv_##name, sv_##name, min, cur, max, flags|IDF_GAME)
#define GVARF(flags, name, min, cur, max, svbody, ccbody) _VARF(sv_##name, sv_##name, min, cur, max, svbody, flags|IDF_GAME)
#define GFVARN(flags, name, global, min, cur, max) _FVAR(sv_##name, global, min, cur, max, flags|IDF_GAME)
#define GFVAR(flags, name, min, cur, max) _FVAR(sv_##name, sv_##name, min, cur, max, flags|IDF_GAME)
#define GFVARF(flags, name, min, cur, max, svbody, ccbody) _FVARF(sv_##name, sv_##name, min, cur, max, svbody, flags|IDF_GAME)
#define GSVARN(flags, name, global, cur) _SVAR(sv_##name, global, cur, flags|IDF_GAME)
#define GSVAR(flags, name, cur) _SVAR(sv_##name, sv_##name, cur, flags|IDF_GAME)
#define GSVARF(flags, name, cur, svbody, ccbody) _SVARF(sv_##name, sv_##name, cur, svbody, flags|IDF_GAME)
#else
#define G(name) (name)
#define PHYS(name) (G(name)*G(name##scale))
#define GICOMMAND(flags, n, g, proto, svbody, ccbody)
#define GVARN(flags, name, global, min, cur, max) extern int name
#define GVAR(flags, name, min, cur, max) extern int name
#define GVARF(flags, name, min, cur, max, svbody, ccbody) extern int name
#define GVARN(flags, name, global, min, cur, max) extern int name
#define GVAR(flags, name, min, cur, max) extern int name
#define GVARF(flags, name, min, cur, max, svbody, ccbody) extern int name
#define GFVARN(flags, name, global, min, cur, max) extern float name
#define GFVAR(flags, name, min, cur, max) extern float name
#define GFVARF(flags, name, min, cur, max, svbody, ccbody) extern float name
#define GSVARN(flags, name, global, cur) extern char *name
#define GSVAR(flags, name, cur) extern char *name
#define GSVARF(flags, name, cur, svbody, ccbody) extern char *name
#endif
