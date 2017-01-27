// command.cpp: implements the parsing and execution of a tiny script language which
// is largely backwards compatible with the quake console language.

#include "engine.h"

bool interactive = false;

hashnameset<ident> idents; // contains ALL vars/commands/aliases
vector<ident *> identmap;
ident *dummyident = NULL;

int identflags = 0;

static const int MAXARGS = 25;

VARN(0, numargs, _numargs, MAXARGS, 0, 0);

static inline void freearg(tagval &v)
{
    switch(v.type)
    {
        case VAL_STR: delete[] v.s; break;
        case VAL_CODE: if(v.code[-1] == CODE_START) delete[] (uchar *)&v.code[-1]; break;
    }
}

static inline void forcenull(tagval &v)
{
    switch(v.type)
    {
        case VAL_NULL: return;
    }
    freearg(v);
    v.setnull();
}

static inline float forcefloat(tagval &v)
{
    float f = 0.0f;
    switch(v.type)
    {
        case VAL_INT: f = v.i; break;
        case VAL_STR: f = parsefloat(v.s); break;
        case VAL_MACRO: f = parsefloat(v.s); break;
        case VAL_FLOAT: return v.f;
    }
    freearg(v);
    v.setfloat(f);
    return f;
}

static inline int forceint(tagval &v)
{
    int i = 0;
    switch(v.type)
    {
        case VAL_FLOAT: i = v.f; break;
        case VAL_STR: i = parseint(v.s); break;
        case VAL_MACRO: i = parseint(v.s); break;
        case VAL_INT: return v.i;
    }
    freearg(v);
    v.setint(i);
    return i;
}

static inline const char *forcestr(tagval &v)
{
    const char *s = "";
    switch(v.type)
    {
        case VAL_FLOAT: s = floatstr(v.f); break;
        case VAL_INT: s = intstr(v.i); break;
        case VAL_STR: case VAL_MACRO: return v.s;
    }
    freearg(v);
    v.setstr(newstring(s));
    return s;
}

static inline void forcearg(tagval &v, int type)
{
    switch(type)
    {
        case RET_STR: if(v.type != VAL_STR) forcestr(v); break;
        case RET_INT: if(v.type != VAL_INT) forceint(v); break;
        case RET_FLOAT: if(v.type != VAL_FLOAT) forcefloat(v); break;
    }
}

static inline ident *forceident(tagval &v)
{
    switch(v.type)
    {
        case VAL_IDENT: return v.id;
        case VAL_MACRO:
        {
            ident *id = newident(v.s, IDF_UNKNOWN);
            v.setident(id);
            return id;
        }
        case VAL_STR:
        {
            ident *id = newident(v.s, IDF_UNKNOWN);
            delete[] v.s;
            v.setident(id);
            return id;
        }
    }
    freearg(v);
    v.setident(dummyident);
    return dummyident;
}

void tagval::cleanup()
{
    freearg(*this);
}

static inline void freeargs(tagval *args, int &oldnum, int newnum)
{
    for(int i = newnum; i < oldnum; i++) freearg(args[i]);
    oldnum = newnum;
}

static inline void cleancode(ident &id)
{
    if(id.code)
    {
        id.code[0] -= 0x100;
        if(int(id.code[0]) < 0x100) delete[] id.code;
        id.code = NULL;
    }
}

struct nullval : tagval
{
    nullval() { setnull(); }
} nullval;
tagval noret = nullval, *commandret = &noret;

void clear_command()
{
    enumerate(idents, ident, i,
    {
        if(i.type==ID_ALIAS)
        {
            DELETEA(i.name);
            i.forcenull();
            DELETEA(i.code);
        }
    });
}

void clearoverride(ident &i)
{
    switch(i.type)
    {
        case ID_ALIAS:
            if(i.valtype==VAL_STR)
            {
                if(!i.val.s[0]) break;
                delete[] i.val.s;
            }
            cleancode(i);
            i.valtype = VAL_STR;
            i.val.s = newstring(i.def.s);
            break;
        case ID_VAR:
            *i.storage.i = i.def.i;
            i.changed();
            break;
        case ID_FVAR:
            *i.storage.f = i.def.f;
            i.changed();
            break;
        case ID_SVAR:
            delete[] *i.storage.s;
            *i.storage.s = i.def.s;
            i.changed();
            break;
    }
}

void clearoverrides()
{
    enumerate(idents, ident, i, clearoverride(i));
}

static bool initedidents = false;
static vector<ident> *identinits = NULL;

static inline ident *addident(const ident &id)
{
    if(!initedidents)
    {
        if(!identinits) identinits = new vector<ident>;
        identinits->add(id);
        return NULL;
    }
    ident &def = idents.access(id.name, id);
    def.index = identmap.length();
    return identmap.add(&def);
}

static bool initidents()
{
    initedidents = true;
    for(int i = 0; i < MAXARGS; i++)
    {
        defformatstring(argname, "arg%d", i+1);
        newident(argname, IDF_ARG);
    }
    dummyident = newident("//dummy", IDF_UNKNOWN);
    if(identinits)
    {
        loopv(*identinits) addident((*identinits)[i]);
        DELETEP(identinits);
    }
    return true;
}
UNUSED static bool forceinitidents = initidents();

static const char *sourcefile = NULL, *sourcestr = NULL;

static const char *debugline(const char *p, const char *fmt)
{
    if(!sourcestr) return fmt;
    int num = 1;
    const char *line = sourcestr;
    for(;;)
    {
        const char *end = strchr(line, '\n');
        if(!end) end = line + strlen(line);
        if(p >= line && p <= end)
        {
            static bigstring buf;
            char color[] = { '\0', '\0', '\0' };
            if(fmt[0] == '\f') { memcpy(color, fmt, 2); fmt += strlen(color); }
            if(sourcefile) formatstring(buf, "%s%s:%d: %s", color, sourcefile, num, fmt);
            else formatstring(buf, "%s%d: %s", color, num, fmt);
            return buf;
        }
        if(!*end) break;
        line = end + 1;
        num++;
    }
    return fmt;
}

static struct identlink
{
    ident *id;
    identlink *next;
    int usedargs;
    identstack *argstack;
} noalias = { NULL, NULL, (1<<MAXARGS)-1, NULL }, *aliasstack = &noalias;

VAR(0, dbgalias, 0, 4, 1000);

static void debugalias()
{
    if(!dbgalias) return;
    int total = 0, depth = 0;
    for(identlink *l = aliasstack; l != &noalias; l = l->next) total++;
    for(identlink *l = aliasstack; l != &noalias; l = l->next)
    {
        ident *id = l->id;
        ++depth;
        if(depth < dbgalias) conoutft(CON_MESG, "  %d) %s", total-depth+1, id->name);
        else if(l->next == &noalias) conoutft(CON_MESG, depth == dbgalias ? "  %d) %s" : "  ..%d) %s", total-depth+1, id->name);
    }
}

static int nodebug = 0;

static void debugcode(const char *fmt, ...) PRINTFARGS(1, 2);

static void debugcode(const char *fmt, ...)
{
    if(nodebug) return;

    defvformatbigstring(msg, fmt, fmt);
    conoutft(CON_MESG, "%s", msg);

    debugalias();
}

static void debugcodeline(const char *p, const char *fmt, ...) PRINTFARGS(2, 3);

static void debugcodeline(const char *p, const char *fmt, ...)
{
    if(nodebug) return;

    defvformatbigstring(msg, fmt, debugline(p, fmt));
    conoutft(CON_MESG, "%s", msg);

    debugalias();
}

ICOMMAND(0, nodebug, "e", (uint *body), { nodebug++; executeret(body, *commandret); nodebug--; });

void addident(ident *id)
{
    addident(*id);
}

static inline void pusharg(ident &id, const tagval &v, identstack &stack)
{
    stack.val = id.val;
    stack.valtype = id.valtype;
    stack.next = id.stack;
    id.stack = &stack;
    id.setval(v);
    cleancode(id);
}

static inline void poparg(ident &id)
{
    if(!id.stack) return;
    identstack *stack = id.stack;
    if(id.valtype == VAL_STR) delete[] id.val.s;
    id.setval(*stack);
    cleancode(id);
    id.stack = stack->next;
}

ICOMMAND(0, push, "rte", (ident *id, tagval *v, uint *code),
{
    if(id->type != ID_ALIAS || id->index < MAXARGS) return;
    identstack stack;
    pusharg(*id, *v, stack);
    v->type = VAL_NULL;
    id->flags &= ~IDF_UNKNOWN;
    executeret(code, *commandret);
    poparg(*id);
});

static inline void pushalias(ident &id, identstack &stack)
{
    if(id.type == ID_ALIAS && id.index >= MAXARGS)
    {
        pusharg(id, nullval, stack);
        id.flags &= ~IDF_UNKNOWN;
    }
}

static inline void popalias(ident &id)
{
    if(id.type == ID_ALIAS && id.index >= MAXARGS) poparg(id);
}

KEYWORD(0, local, ID_LOCAL);

static inline bool checknumber(const char *s)
{
    if(isdigit(s[0])) return true;
    else switch(s[0])
    {
        case '+': case '-': return isdigit(s[1]) || (s[1] == '.' && isdigit(s[2]));
        case '.': return isdigit(s[1]) != 0;
        default: return false;
    }
}

ident *newident(const char *name, int flags)
{
    ident *id = idents.access(name);
    if(!id)
    {
        if(checknumber(name))
        {
            debugcode("\frnumber %s is not a valid identifier name", name);
            return dummyident;
        }
        id = addident(ident(ID_ALIAS, newstring(name), flags));
    }
    return id;
}

ident *writeident(const char *name, int flags)
{
    ident *id = newident(name, flags);
    if(id->index < MAXARGS && !(aliasstack->usedargs&(1<<id->index)))
    {
        pusharg(*id, nullval, aliasstack->argstack[id->index]);
        aliasstack->usedargs |= 1<<id->index;
    }
    return id;
}

ident *readident(const char *name)
{
    ident *id = idents.access(name);
    if(id && id->index < MAXARGS && !(aliasstack->usedargs&(1<<id->index)))
       return NULL;
    return id;
}

void resetvar(char *name)
{
    ident *id = idents.access(name);
    if(!id) return;
    if(id->flags&IDF_READONLY || id->flags&IDF_CLIENT || id->flags&IDF_SERVER) debugcode("\frvariable %s is read-only or remote", id->name);
    else if(id->flags&IDF_WORLD) debugcode("\frvariable %s is only directly modifiable in editmode", id->name);
    else clearoverride(*id);
}

COMMAND(0, resetvar, "s");

static inline void setarg(ident &id, tagval &v)
{
    if(aliasstack->usedargs&(1<<id.index))
    {
        if(id.valtype == VAL_STR) delete[] id.val.s;
        id.setval(v);
        cleancode(id);
    }
    else
    {
        pusharg(id, v, aliasstack->argstack[id.index]);
        aliasstack->usedargs |= 1<<id.index;
    }
}

static inline void setalias(ident &id, tagval &v)
{
    if(id.valtype == VAL_STR) delete[] id.val.s;
    id.setval(v);
    cleancode(id);
    id.flags = (id.flags & (identflags|IDF_WORLD)) | (id.flags & (identflags|IDF_PERSIST)) | identflags;
#ifndef STANDALONE
    client::editvar(&id, interactive && !(identflags&IDF_WORLD));
#endif
}

static void setalias(const char *name, tagval &v)
{
    ident *id = idents.access(name);
    if(id)
    {
        if(id->type == ID_ALIAS)
        {
            if(id->index < MAXARGS) setarg(*id, v); else setalias(*id, v);
        }
        else
        {
            debugcode("\frcannot redefine builtin %s with an alias", id->name);
            freearg(v);
        }
    }
    else if(checknumber(name))
    {
        debugcode("\frcannot alias number %s", name);
        freearg(v);
    }
    else
    {
        id = addident(ident(ID_ALIAS, newstring(name), v, identflags));
#ifndef STANDALONE
        client::editvar(id, interactive && !(identflags&IDF_WORLD));
#endif
    }
}

void alias(const char *name, const char *str)
{
    tagval v;
    v.setstr(newstring(str));
    setalias(name, v);
}

void alias(const char *name, tagval &v)
{
    setalias(name, v);
}

ICOMMAND(0, alias, "st", (const char *name, tagval *v),
{
    setalias(name, *v);
    v->type = VAL_NULL;
});

void worldalias(const char *name, const char *action)
{
    WITHWORLD(alias(name, action));
}
COMMAND(0, worldalias, "ss");

void loadalias(const char *name, const char *fname)
{
    string s;
    copystring(s, fname);
    char *buf = loadfile(s, NULL);
    if(!buf)
    {
        conoutf("\frcould not read %s", fname);
        return;
    }
    tagval v;
    v.setstr(buf);
    setalias(name, v);
}
COMMAND(0, loadalias, "ss");

// variable's and commands are registered through globals, see cube.h

int variable(const char *name, int min, int cur, int max, int *storage, identfun fun, int flags)
{
    addident(ident(ID_VAR, name, min, cur, max, storage, (void *)fun, flags));
    return cur;
}

float fvariable(const char *name, float min, float cur, float max, float *storage, identfun fun, int flags)
{
    addident(ident(ID_FVAR, name, min, cur, max, storage, (void *)fun, flags));
    return cur;
}

char *svariable(const char *name, const char *cur, char **storage, identfun fun, int flags)
{
    addident(ident(ID_SVAR, name, newstring(cur), storage, (void *)fun, flags));
    return newstring(cur);
}

#define GETVAR(id, vartype, name, retval) \
    ident *id = idents.access(name); \
    if(!id || id->type!=vartype) return retval;

void setvar(const char *name, int i, bool dofunc, bool def)
{
    GETVAR(id, ID_VAR, name, );
    *id->storage.i = clamp(i, id->minval, id->maxval);
    if(def || versioning)
    {
        id->def.i = i;
        if(versioning == 2) id->bin.i = i;
    }
    if(dofunc) id->changed();
#ifndef STANDALONE
    if(versioning && id->flags&IDF_SERVER) setvar(&id->name[3], i, dofunc, def);
#endif
}
void setfvar(const char *name, float f, bool dofunc, bool def)
{
    GETVAR(id, ID_FVAR, name, );
    *id->storage.f = clamp(f, id->minvalf, id->maxvalf);
    if(def || versioning)
    {
        id->def.f = f;
        if(versioning == 2) id->bin.f = f;
    }
    if(dofunc) id->changed();
#ifndef STANDALONE
    if(versioning && id->flags&IDF_SERVER) setfvar(&id->name[3], f, dofunc, def);
#endif
}
void setsvar(const char *name, const char *str, bool dofunc, bool def)
{
    GETVAR(id, ID_SVAR, name, );
    delete[] *id->storage.s;
    *id->storage.s = newstring(str);
    if(def || versioning)
    {
        delete[] id->def.s;
        id->def.s = newstring(str);
        if(versioning == 2)
        {
            delete[] id->bin.s;
            id->bin.s = newstring(str);
        }
    }
    if(dofunc) id->changed();
#ifndef STANDALONE
    if(versioning && id->flags&IDF_SERVER) setsvar(&id->name[3], str, dofunc, def);
#endif
}
int getvar(const char *name)
{
    GETVAR(id, ID_VAR, name, 0);
    switch(id->type)
    {
        case ID_VAR: return *id->storage.i;
        case ID_FVAR: return int(*id->storage.f);
        case ID_SVAR: return atoi(*id->storage.s);
        case ID_ALIAS: return id->getint();
        default: break;
    }
    return 0;
}
int getvartype(const char *name)
{
    ident *id = idents.access(name);
    if(!id) return -1;
    return id->type;
}
int getvarflags(const char *name)
{
    ident *id = idents.access(name);
    if(!id) return -1;
    return id->flags;
}
int getvarmin(const char *name)
{
    ident *id = idents.access(name);
    if(!id) return 0;
    if(id) switch(id->type)
    {
        case ID_VAR: return id->minval;
        case ID_FVAR: return int(id->minvalf);
    }
    return 0;
}
int getvarmax(const char *name)
{
    ident *id = idents.access(name);
    if(!id) return 0;
    if(id) switch(id->type)
    {
        case ID_VAR: return id->maxval;
        case ID_FVAR: return int(id->maxvalf);
    }
    return 0;
}
float getfvarmin(const char *name)
{
    ident *id = idents.access(name);
    if(id) switch(id->type)
    {
        case ID_VAR: return id->minval;
        case ID_FVAR: return id->minvalf;
    }
    return 0;
}
float getfvarmax(const char *name)
{
    ident *id = idents.access(name);
    if(id) switch(id->type)
    {
        case ID_VAR: return id->maxval;
        case ID_FVAR: return id->maxvalf;
    }
    return 0;
}
int getvardef(const char *name, bool rb)
{
    ident *id = getident(name);
    if(!id) return 0;
    switch(id->type)
    {
        case ID_VAR: return (rb ? id->bin : id->def).i;
        case ID_FVAR: return int((rb ? id->bin : id->def).f);
        case ID_SVAR: return atoi((rb ? id->bin : id->def).s);
        case ID_ALIAS: return id->getint();
        default: break;
    }
    return 0;
}

float getfvardef(const char *name, bool rb)
{
    ident *id = getident(name);
    if(!id) return 0.f;
    switch(id->type)
    {
        case ID_VAR: return float((rb ? id->bin : id->def).i);
        case ID_FVAR: return (rb ? id->bin : id->def).f;
        case ID_SVAR: return atof((rb ? id->bin : id->def).s);
        case ID_ALIAS: return id->getfloat();
        default: break;
    }
    return 0.f;
}

const char *getsvardef(const char *name, bool rb)
{
    ident *id = getident(name);
    if(!id) return "";
    switch(id->type)
    {
        case ID_VAR: return intstr((rb ? id->bin : id->def).i);
        case ID_FVAR: return floatstr((rb ? id->bin : id->def).f);
        case ID_SVAR: return (rb ? id->bin : id->def).s;
        case ID_ALIAS: return id->getstr();
        default: break;
    }
    return "";
}

const char *getvardesc(const char *name)
{
    ident *id = getident(name);
    if(!id || !id->desc) return "";
    return id->desc;
}

void getvarfields(const char *name, int prop)
{
    ident *id = getident(name);
    if(!id) result("");
    if(prop < 0) intret(id->fields.length());
    else if(id->fields.inrange(prop)) result(id->fields[prop]);
    else result("");
}

ICOMMAND(0, getvar, "s", (char *n), intret(getvar(n)));
ICOMMAND(0, getvartype, "s", (char *n), intret(getvartype(n)));
ICOMMAND(0, getvarflags, "s", (char *n), intret(getvarflags(n)));
ICOMMAND(0, getvarmin, "s", (char *n), intret(getvarmin(n)));
ICOMMAND(0, getvarmax, "s", (char *n), intret(getvarmax(n)));
ICOMMAND(0, getfvarmin, "s", (char *s), floatret(getfvarmin(s)));
ICOMMAND(0, getfvarmax, "s", (char *s), floatret(getfvarmax(s)));
ICOMMAND(0, getvardef, "si", (char *n, int *b), intret(getvardef(n, *b!=0)));
ICOMMAND(0, getfvardef, "si", (char *n, int *b), floatret(getfvardef(n, *b!=0)));
ICOMMAND(0, getsvardef, "si", (char *n, int *b), result(getsvardef(n, *b!=0)));
ICOMMAND(0, getvardesc, "s", (char *n), result(getvardesc(n)));
ICOMMAND(0, getvarfields, "sb", (char *n, int *p), getvarfields(n, *p));

bool identexists(const char *name) { return idents.access(name)!=NULL; }
ident *getident(const char *name) { return idents.access(name); }

void touchvar(const char *name)
{
    ident *id = idents.access(name);
    if(id) switch(id->type)
    {
        case ID_VAR:
        case ID_FVAR:
        case ID_SVAR:
            id->changed();
            break;
    }
}

const char *getalias(const char *name)
{
    ident *i = idents.access(name);
    return i && i->type==ID_ALIAS && (i->index >= MAXARGS || aliasstack->usedargs&(1<<i->index)) ? i->getstr() : "";
}

ICOMMAND(0, getalias, "s", (char *s), result(getalias(s)));

#ifndef STANDALONE
#define CHECKVAR(argstr) \
    if(!versioning) \
    { \
        if(!(identflags&IDF_WORLD) && !editmode && id->flags&IDF_WORLD && !(id->flags&IDF_REWRITE)) \
        { \
            debugcode("\frcannot set world variable %s outside editmode", id->name); \
            return; \
        } \
        if(id->flags&IDF_CLIENT) \
        { \
            if((identflags&IDF_WORLD) && !(id->flags&IDF_WORLD)) \
            { \
                debugcode("\frcannot set variable %s from map config", id->name); \
                return; \
            } \
            if(client::sendcmd(2, id->name, argstr)) return; \
        } \
    }
#endif

void setvarchecked(ident *id, int val)
{
    if(id->flags&IDF_READONLY) debugcode("\frvariable %s is read-only", id->name);
    else
    {
#ifndef STANDALONE
        CHECKVAR(intstr(val))
#endif
        if(val<id->minval || val>id->maxval)
        {
            val = val<id->minval ? id->minval : id->maxval;                // clamp to valid range
            debugcode(
                id->flags&IDF_HEX ?
                    (id->minval <= 255 ? "\frvalid range for %s is %d..0x%X" : "\frvalid range for %s is 0x%X..0x%X") :
                    "\frvalid range for %s is %d..%d",
                id->name, id->minval, id->maxval);
        }
        *id->storage.i = val;
        if(versioning)
        {
            id->def.i = val;
            if(versioning == 2) id->bin.i = val;
        }
        id->changed();                                             // call trigger function if available
#ifndef STANDALONE
        client::editvar(id, interactive && !(identflags&IDF_WORLD));
        if(versioning && id->flags&IDF_SERVER) setvar(&id->name[3], val, true);
#endif
    }
}

void setfvarchecked(ident *id, float val)
{
    if(id->flags&IDF_READONLY) debugcode("\frvariable %s is read-only", id->name);
    else
    {
#ifndef STANDALONE
        CHECKVAR(floatstr(val))
#endif
        if(val<id->minvalf || val>id->maxvalf)
        {
            val = val<id->minvalf ? id->minvalf : id->maxvalf;                // clamp to valid range
            debugcode("\frvalid range for %s is %s..%s", id->name, floatstr(id->minvalf), floatstr(id->maxvalf));
        }
        *id->storage.f = val;
        if(versioning)
        {
            id->def.f = val;
            if(versioning == 2) id->bin.f = val;
        }
        id->changed();
#ifndef STANDALONE
        client::editvar(id, interactive && !(identflags&IDF_WORLD));
        if(versioning && id->flags&IDF_SERVER) setfvar(&id->name[3], val, true);
#endif
    }
}

void setsvarchecked(ident *id, const char *val)
{
    if(id->flags&IDF_READONLY) debugcode("\frvariable %s is read-only", id->name);
    else
    {
#ifndef STANDALONE
        CHECKVAR(val)
#endif
        delete[] *id->storage.s;
        *id->storage.s = newstring(val);
        if(versioning)
        {
            delete[] id->def.s;
            id->def.s = newstring(val);
            if(versioning == 2)
            {
                delete[] id->bin.s;
                id->bin.s = newstring(val);
            }
        }
        id->changed();
#ifndef STANDALONE
        client::editvar(id, interactive && !(identflags&IDF_WORLD));
        if(versioning && id->flags&IDF_SERVER) setsvar(&id->name[3], val, true);
#endif
    }
}

bool addcommand(const char *name, identfun fun, const char *args, int flags)
{
    uint argmask = 0;
    int numargs = 0;
    bool limit = true;
    for(const char *fmt = args; *fmt; fmt++) switch(*fmt)
    {
        case 'i': case 'b': case 'f': case 'g': case 't': case 'N': case 'D': if(numargs < MAXARGS) numargs++; break;
        case 's': case 'e': case 'r': case '$': if(numargs < MAXARGS) { argmask |= 1<<numargs; numargs++; } break;
        case '1': case '2': case '3': case '4': if(numargs < MAXARGS) fmt -= *fmt-'0'+1; break;
        case 'C': case 'V': limit = false; break;
        default: fatal("builtin %s declared with illegal type: %s", name, args); break;
    }
    if(limit && numargs > 16) fatal("builtin %s declared with too many args: %d", name, numargs);
    addident(ident(ID_COMMAND, name, args, argmask, numargs, (void *)fun, flags));
    return false;
}

bool addkeyword(int type, const char *name, int flags)
{
    addident(ident(type, name, "", 0, 0, NULL, flags));
    return true;
}

const char *parsestring(const char *p)
{
    for(; *p; p++) switch(*p)
    {
        case '\r':
        case '\n':
        case '\"':
            return p;
        case '^':
            if(*++p) break;
            return p;
    }
    return p;
}

int unescapestring(char *dst, const char *src, const char *end)
{
    char *start = dst;
    while(src < end)
    {
        int c = *src++;
        if(c == '^')
        {
            if(src >= end) break;
            int e = *src++;
            switch(e)
            {
                case 'n': *dst++ = '\n'; break;
                case 't': *dst++ = '\t'; break;
                case 'f': *dst++ = '\f'; break;
                default: *dst++ = e; break;
            }
        }
        else *dst++ = c;
    }
    *dst = '\0';
    return dst - start;
}

static char *conc(vector<char> &buf, tagval *v, int n, bool space, const char *prefix = NULL, int prefixlen = 0)
{
    if(prefix)
    {
        buf.put(prefix, prefixlen);
        if(space && n) buf.add(' ');
    }
    loopi(n)
    {
        const char *s = "";
        int len = 0;
        switch(v[i].type)
        {
            case VAL_INT: s = intstr(v[i].i); break;
            case VAL_FLOAT: s = floatstr(v[i].f); break;
            case VAL_STR: s = v[i].s; break;
            case VAL_MACRO: s = v[i].s; len = v[i].code[-1]>>8; goto haslen;
        }
        len = int(strlen(s));
    haslen:
        buf.put(s, len);
        if(i == n-1) break;
        if(space) buf.add(' ');
    }
    buf.add('\0');
    return buf.getbuf();
}

char *conc(tagval *v, int n, bool space, const char *prefix, int prefixlen)
{
    static int vlen[MAXARGS];
    static char numbuf[3*MAXSTRLEN];
    int len = prefixlen, numlen = 0, i = 0;
    for(; i < n; i++) switch(v[i].type)
    {
        case VAL_MACRO: len += (vlen[i] = v[i].code[-1]>>8); break;
        case VAL_STR: len += (vlen[i] = int(strlen(v[i].s))); break;
        case VAL_INT:
            if(numlen + MAXSTRLEN > int(sizeof(numbuf))) goto overflow;
            intformat(&numbuf[numlen], v[i].i);
            numlen += (vlen[i] = strlen(&numbuf[numlen]));
            break;
        case VAL_FLOAT:
            if(numlen + MAXSTRLEN > int(sizeof(numbuf))) goto overflow;
            floatformat(&numbuf[numlen], v[i].f);
            numlen += (vlen[i] = strlen(&numbuf[numlen]));
            break;
        default: vlen[i] = 0; break;
    }
overflow:
    if(space) len += max(prefix ? i : i-1, 0);
    char *buf = newstring(len + numlen);
    int offset = 0, numoffset = 0;
    if(prefix)
    {
        memcpy(buf, prefix, prefixlen);
        offset += prefixlen;
        if(space && i) buf[offset++] = ' ';
    }
    loopj(i)
    {
        if(v[j].type == VAL_INT || v[j].type == VAL_FLOAT)
        {
            memcpy(&buf[offset], &numbuf[numoffset], vlen[j]);
            numoffset += vlen[j];
        }
        else if(vlen[j]) memcpy(&buf[offset], v[j].s, vlen[j]);
        offset += vlen[j];
        if(j==i-1) break;
        if(space) buf[offset++] = ' ';
    }
    buf[offset] = '\0';
    if(i < n)
    {
        char *morebuf = conc(&v[i], n-i, space, buf, offset);
        delete[] buf;
        return morebuf;
    }
    return buf;
}

static inline void skipcomments(const char *&p)
{
    for(;;)
    {
        p += strspn(p, " \t\r");
        if(p[0]!='/' || p[1]!='/') break;
        p += strcspn(p, "\n\0");
    }
}

static inline char *cutstring(const char *&p, int &len)
{
    p++;
    const char *end = parsestring(p);
    char *s = newstring(end - p);
    len = unescapestring(s, p, end);
    s[len] = '\0';
    p = end;
    if(*p=='\"') p++;
    return s;
}

char *parsetext(const char *&p)
{
    p += strspn(p, " \t\r");
    if(*p == '"')
    {
        int len;
        return cutstring(p, len);
    }
    else
    {
        const char *start = p;
        p += strcspn(p, " ;\t\r\n\0");
        return newstring(start, p-start);
    }
}

static inline const char *parseword(const char *p)
{
    const int maxbrak = 100;
    static char brakstack[maxbrak];
    int brakdepth = 0;
    for(;; p++)
    {
        p += strcspn(p, "\"/;()[] \t\r\n\0");
        switch(p[0])
        {
            case '"': case ';': case ' ': case '\t': case '\r': case '\n': case '\0': return p;
            case '/': if(p[1] == '/') return p; break;
            case '[': case '(': if(brakdepth >= maxbrak) return p; brakstack[brakdepth++] = p[0]; break;
            case ']': if(brakdepth <= 0 || brakstack[--brakdepth] != '[') return p; break;
            case ')': if(brakdepth <= 0 || brakstack[--brakdepth] != '(') return p; break;
        }
    }
    return p;
}

static inline char *cutword(const char *&p, int &len)
{
    const char *word = p;
    p = parseword(p);
    len = p-word;
    if(!len) return NULL;
    return newstring(word, len);
}

static inline void compilestr(vector<uint> &code, const char *word, int len, bool macro = false)
{
    if(len <= 3 && !macro)
    {
        uint op = CODE_VALI|RET_STR;
        for(int i = 0; i < len; i++) op |= uint(uchar(word[i]))<<((i+1)*8);
        code.add(op);
        return;
    }
    code.add((macro ? CODE_MACRO : CODE_VAL|RET_STR)|(len<<8));
    code.put((const uint *)word, len/sizeof(uint));
    size_t endlen = len%sizeof(uint);
    union
    {
        char c[sizeof(uint)];
        uint u;
    } end;
    end.u = 0;
    memcpy(end.c, word + len - endlen, endlen);
    code.add(end.u);
}

static inline void compilestr(vector<uint> &code, const char *word = NULL)
{
    if(!word) { code.add(CODE_VALI|RET_STR); return; }
    compilestr(code, word, int(strlen(word)));
}

static inline void compileint(vector<uint> &code, int i)
{
    if(i >= -0x800000 && i <= 0x7FFFFF)
        code.add(CODE_VALI|RET_INT|(i<<8));
    else
    {
        code.add(CODE_VAL|RET_INT);
        code.add(i);
    }
}

static inline void compilenull(vector<uint> &code)
{
    code.add(CODE_VALI|RET_NULL);
}

static inline void compileblock(vector<uint> &code)
{
    int start = code.length();
    code.add(CODE_BLOCK);
    code.add(CODE_OFFSET|((start+2)<<8));
    code.add(CODE_EXIT);
    code[start] |= uint(code.length() - (start + 1))<<8;
}

static inline void compileident(vector<uint> &code, ident *id)
{
    code.add((id->index < MAXARGS ? CODE_IDENTARG : CODE_IDENT)|(id->index<<8));
}

static inline void compileident(vector<uint> &code, const char *word = NULL)
{
    compileident(code, word ? newident(word, IDF_UNKNOWN) : dummyident);
}

static inline void compileint(vector<uint> &code, const char *word = NULL)
{
    return compileint(code, word ? parseint(word) : 0);
}

static inline void compilefloat(vector<uint> &code, float f)
{
    if(int(f) == f && f >= -0x800000 && f <= 0x7FFFFF)
        code.add(CODE_VALI|RET_FLOAT|(int(f)<<8));
    else
    {
        union { float f; uint u; } conv;
        conv.f = f;
        code.add(CODE_VAL|RET_FLOAT);
        code.add(conv.u);
    }
}

static inline void compilefloat(vector<uint> &code, const char *word = NULL)
{
    return compilefloat(code, word ? parsefloat(word) : 0.0f);
}

static bool compilearg(vector<uint> &code, const char *&p, int wordtype);
static void compilestatements(vector<uint> &code, const char *&p, int rettype, int brak = '\0');

static inline void compileval(vector<uint> &code, int wordtype, char *word, int wordlen)
{
    switch(wordtype)
    {
        case VAL_STR: compilestr(code, word, wordlen, true); break;
        case VAL_ANY: compilestr(code, word, wordlen); break;
        case VAL_FLOAT: compilefloat(code, word); break;
        case VAL_INT: compileint(code, word); break;
        case VAL_CODE:
        {
            int start = code.length();
            code.add(CODE_BLOCK);
            code.add(CODE_OFFSET|((start+2)<<8));
            const char *p = word;
            if(p) compilestatements(code, p, VAL_ANY);
            code.add(CODE_EXIT|RET_STR);
            code[start] |= uint(code.length() - (start + 1))<<8;
            break;
        }
        case VAL_IDENT: compileident(code, word); break;
        default:
            break;
    }
}

static bool compileword(vector<uint> &code, const char *&p, int wordtype, char *&word, int &wordlen);

static void compilelookup(vector<uint> &code, const char *&p, int ltype)
{
    char *lookup = NULL;
    int lookuplen = 0;
    switch(*++p)
    {
        case '(':
        case '[':
            if(!compileword(code, p, VAL_STR, lookup, lookuplen)) goto invalid;
            break;
        case '$':
            compilelookup(code, p, VAL_STR);
            break;
        case '\"':
            lookup = cutstring(p, lookuplen);
            goto lookupid;
        default:
        {
            lookup = cutword(p, lookuplen);
            if(!lookup) goto invalid;
        lookupid:
            ident *id = newident(lookup, IDF_UNKNOWN);
            if(id) switch(id->type)
            {
                case ID_VAR: code.add(CODE_IVAR|((ltype >= VAL_ANY ? VAL_INT : ltype)<<CODE_RET)|(id->index<<8)); goto done;
                case ID_FVAR: code.add(CODE_FVAR|((ltype >= VAL_ANY ? VAL_FLOAT : ltype)<<CODE_RET)|(id->index<<8)); goto done;
                case ID_SVAR: code.add(CODE_SVAR|((ltype >= VAL_ANY ? VAL_STR : ltype)<<CODE_RET)|(id->index<<8)); goto done;
                case ID_ALIAS: code.add((id->index < MAXARGS ? CODE_LOOKUPARG : CODE_LOOKUP)|((ltype >= VAL_ANY ? VAL_STR : ltype)<<CODE_RET)|(id->index<<8)); goto done;
                case ID_COMMAND:
                {
                    int comtype = CODE_COM, numargs = 0;
                    code.add(CODE_ENTER);
                    for(const char *fmt = id->args; *fmt; fmt++) switch(*fmt)
                    {
                        case 's': compilestr(code, NULL, 0, true); numargs++; break;
                        case 'i': compileint(code); numargs++; break;
                        case 'b': compileint(code, INT_MIN); numargs++; break;
                        case 'f': compilefloat(code); numargs++; break;
                        case 'g': compilefloat(code, -FLT_MAX); numargs++; break;
                        case 't': compilenull(code); numargs++; break;
                        case 'e': compileblock(code); numargs++; break;
                        case 'r': compileident(code); numargs++; break;
                        case '$': compileident(code, id); numargs++; break;
                        case 'N': compileint(code, -1); numargs++; break;
#ifndef STANDALONE
                        case 'D': comtype = CODE_COMD; numargs++; break;
#endif
                        case 'C': comtype = CODE_COMC; numargs = 1; goto endfmt;
                        case 'V': comtype = CODE_COMV; numargs = 2; goto endfmt;
                        case '1': case '2': case '3': case '4': break;
                    }
                endfmt:
                    code.add(comtype|(ltype < VAL_ANY ? ltype<<CODE_RET : 0)|(id->index<<8));
                    code.add(CODE_EXIT|(ltype < VAL_ANY ? ltype<<CODE_RET : 0));
                    goto done;
                }
                default: goto invalid;
            }
            compilestr(code, lookup, lookuplen, true);
            break;
        }
    }
    code.add(CODE_LOOKUPU|((ltype < VAL_ANY ? ltype<<CODE_RET : 0)));
done:
    delete[] lookup;
    switch(ltype)
    {
        case VAL_CODE: code.add(CODE_COMPILE); break;
        case VAL_IDENT: code.add(CODE_IDENTU); break;
    }
    return;
invalid:
    delete[] lookup;
    switch(ltype)
    {
        case VAL_NULL: case VAL_ANY: compilenull(code); break;
        default: compileval(code, ltype, NULL, 0); break;
    }
}

static bool compileblockstr(vector<uint> &code, const char *str, const char *end, bool macro)
{
    int start = code.length();
    code.add(macro ? CODE_MACRO : CODE_VAL|RET_STR);
    char *buf = (char *)code.reserve((end-str)/sizeof(uint)+1).buf;
    int len = 0;
    while(str < end)
    {
        int n = strcspn(str, "\r/\"@]\0");
        memcpy(&buf[len], str, n);
        len += n;
        str += n;
        switch(*str)
        {
            case '\r': str++; break;
            case '\"':
            {
                const char *start = str;
                str = parsestring(str+1);
                if(*str=='\"') str++;
                memcpy(&buf[len], start, str-start);
                len += str-start;
                break;
            }
            case '/':
                if(str[1] == '/')
                {
                    size_t comment = strcspn(str, "\n\0");
                    if (iscubepunct(str[2]))
                    {
                        memcpy(&buf[len], str, comment);
                        len += comment;
                    }
                    str += comment;
                }
                else buf[len++] = *str++;
                break;
            case '@':
            case ']':
                if(str < end) { buf[len++] = *str++; break; }
            case '\0': goto done;
        }
    }
done:
    memset(&buf[len], '\0', sizeof(uint)-len%sizeof(uint));
    code.advance(len/sizeof(uint)+1);
    code[start] |= len<<8;
    return true;
}

static bool compileblocksub(vector<uint> &code, const char *&p)
{
    char *lookup = NULL;
    int lookuplen = 0;
    switch(*p)
    {
        case '(':
            if(!compilearg(code, p, VAL_STR)) return false;
            break;
        case '[':
            if(!compilearg(code, p, VAL_STR)) return false;
            code.add(CODE_LOOKUPU|RET_STR);
            break;
        case '\"':
            lookup = cutstring(p, lookuplen);
            goto lookupid;
        default:
        {
            {
                const char *start = p;
                while(iscubealnum(*p) || *p=='_') p++;
                lookuplen = p-start;
                if(!lookuplen) return false;
                lookup = newstring(start, lookuplen);
            }
        lookupid:
            ident *id = newident(lookup, IDF_UNKNOWN);
            if(id) switch(id->type)
            {
                case ID_VAR: code.add(CODE_IVAR|RET_STR|(id->index<<8)); goto done;
                case ID_FVAR: code.add(CODE_FVAR|RET_STR|(id->index<<8)); goto done;
                case ID_SVAR: code.add(CODE_SVAR|RET_STR|(id->index<<8)); goto done;
                case ID_ALIAS: code.add((id->index < MAXARGS ? CODE_LOOKUPARG : CODE_LOOKUP)|RET_STR|(id->index<<8)); goto done;
            }
            compilestr(code, lookup, lookuplen, true);
            code.add(CODE_LOOKUPU|RET_STR);
        done:
            delete[] lookup;
            break;
        }
    }
    return true;
}

static void compileblock(vector<uint> &code, const char *&p, int wordtype)
{
    const char *line = p, *start = p;
    int concs = 0;
    for(int brak = 1; brak;)
    {
        p += strcspn(p, "@\"/[]\0");
        int c = *p++;
        switch(c)
        {
            case '\0':
                debugcodeline(line, "\frmissing \"]\"");
                p--;
                goto done;
            case '\"':
                p = parsestring(p);
                if(*p=='\"') p++;
                break;
            case '/':
                if(*p=='/') p += strcspn(p, "\n\0");
                break;
            case '[': brak++; break;
            case ']': brak--; break;
            case '@':
            {
                const char *esc = p;
                while(*p == '@') p++;
                int level = p - (esc - 1);
                if(brak > level) continue;
                else if(brak < level) debugcodeline(line, "\frtoo many @s");
                if(!concs) code.add(CODE_ENTER);
                if(concs + 2 > MAXARGS)
                {
                    code.add(CODE_CONCW|RET_STR|(concs<<8));
                    concs = 1;
                }
                if(compileblockstr(code, start, esc-1, true)) concs++;
                if(compileblocksub(code, p)) concs++;
                if(!concs) code.pop();
                else start = p;
                break;
            }
        }
    }
done:
    if(p-1 > start)
    {
        if(!concs) switch(wordtype)
        {
            case VAL_CODE:
            {
                p = start;
                int inst = code.length();
                code.add(CODE_BLOCK);
                code.add(CODE_OFFSET|((inst+2)<<8));
                compilestatements(code, p, VAL_ANY, ']');
                code.add(CODE_EXIT);
                code[inst] |= uint(code.length() - (inst + 1))<<8;
                return;
            }
            case VAL_IDENT:
            {
                char *name = newstring(start, p-1-start);
                compileident(code, name);
                delete[] name;
                return;
            }
        }
        compileblockstr(code, start, p-1, concs > 0);
        if(concs > 1) concs++;
    }
    if(concs)
    {
        code.add(CODE_CONCM|(wordtype < VAL_ANY ? wordtype<<CODE_RET : RET_STR)|(concs<<8));
        code.add(CODE_EXIT|(wordtype < VAL_ANY ? wordtype<<CODE_RET : RET_STR));
    }
    switch(wordtype)
    {
        case VAL_CODE: if(!concs && p-1 <= start) compileblock(code); else code.add(CODE_COMPILE); break;
        case VAL_IDENT: if(!concs && p-1 <= start) compileident(code); else code.add(CODE_IDENTU); break;
        case VAL_STR: case VAL_NULL: case VAL_ANY:
            if(!concs && p-1 <= start) compilestr(code);
            break;
        default:
            if(!concs)
            {
                if(p-1 <= start) compileval(code, wordtype, NULL, 0);
                else code.add(CODE_FORCE|(wordtype<<CODE_RET));
            }
            break;
    }
}

static bool compileword(vector<uint> &code, const char *&p, int wordtype, char *&word, int &wordlen)
{
    skipcomments(p);
    switch(*p)
    {
        case '\"': word = cutstring(p, wordlen); break;
        case '$': compilelookup(code, p, wordtype); return true;
        case '(':
            p++;
            code.add(CODE_ENTER);
            compilestatements(code, p, VAL_ANY, ')');
            code.add(CODE_EXIT|(wordtype < VAL_ANY ? wordtype<<CODE_RET : 0));
            switch(wordtype)
            {
                case VAL_CODE: code.add(CODE_COMPILE); break;
                case VAL_IDENT: code.add(CODE_IDENTU); break;
            }
            return true;
        case '[':
            p++;
            compileblock(code, p, wordtype);
            return true;
        default: word = cutword(p, wordlen); break;
    }
    return word!=NULL;
}

static inline bool compilearg(vector<uint> &code, const char *&p, int wordtype)
{
    char *word = NULL;
    int wordlen = 0;
    bool more = compileword(code, p, wordtype, word, wordlen);
    if(!more) return false;
    if(word)
    {
        compileval(code, wordtype, word, wordlen);
        delete[] word;
    }
    return true;
}

static void compilestatements(vector<uint> &code, const char *&p, int rettype, int brak)
{
    const char *line = p;
    char *idname = NULL;
    int idlen = 0;
    ident *id = NULL;
    int numargs = 0;
    for(;;)
    {
        skipcomments(p);
        idname = NULL;
        bool more = compileword(code, p, VAL_ANY, idname, idlen);
        if(!more) goto endstatement;
        skipcomments(p);
        if(p[0] == '=') switch(p[1])
        {
            case '/':
                if(p[2] != '/') break;
            case ';': case ' ': case '\t': case '\r': case '\n': case '\0':
                p++;
                if(idname)
                {
                    id = newident(idname, IDF_UNKNOWN);
                    if(!id || id->type != ID_ALIAS) { compilestr(code, idname, idlen, true); id = NULL; }
                    delete[] idname;
                }
                if(!(more = compilearg(code, p, VAL_ANY))) compilestr(code);
                code.add(id && idname ? (id->index < MAXARGS ? CODE_ALIASARG : CODE_ALIAS)|(id->index<<8) : CODE_ALIASU);
                goto endstatement;
        }
        numargs = 0;
        if(!idname)
        {
        noid:
            while(numargs < MAXARGS && (more = compilearg(code, p, VAL_ANY))) numargs++;
            code.add(CODE_CALLU);
        }
        else
        {
            id = idents.access(idname);
            if(!id || (id->flags&IDF_REWRITE && (!(identflags&IDF_WORLD) || !(id->flags&IDF_WORLD))))
            {
                if(!checknumber(idname)) { compilestr(code, idname, idlen); delete[] idname; goto noid; }
                char *end = idname;
                int val = int(strtoul(idname, &end, 0));
                if(*end) compilestr(code, idname, idlen);
                else compileint(code, val);
                code.add(CODE_RESULT);
            }
            else switch(id->type)
            {
                case ID_ALIAS:
                    while(numargs < MAXARGS && (more = compilearg(code, p, VAL_ANY))) numargs++;
                    code.add((id->index < MAXARGS ? CODE_CALLARG : CODE_CALL)|(id->index<<8));
                    break;
                case ID_COMMAND:
                {
                    int comtype = CODE_COM, fakeargs = 0;
                    bool rep = false;
                    for(const char *fmt = id->args; *fmt; fmt++) switch(*fmt)
                    {
                    case 's':
                        if(more) more = compilearg(code, p, VAL_STR);
                        if(!more)
                        {
                            if(rep) break;
                            compilestr(code, NULL, 0, true);
                            fakeargs++;
                        }
                        else if(!fmt[1])
                        {
                            int numconc = 0;
                            while(numargs + numconc < MAXARGS && (more = compilearg(code, p, VAL_STR))) numconc++;
                            if(numconc > 0) code.add(CODE_CONC|RET_STR|((numconc+1)<<8));
                        }
                        numargs++;
                        break;
                    case 'i': if(more) more = compilearg(code, p, VAL_INT); if(!more) { if(rep) break; compileint(code); fakeargs++; } numargs++; break;
                    case 'b': if(more) more = compilearg(code, p, VAL_INT); if(!more) { if(rep) break; compileint(code, INT_MIN); fakeargs++; } numargs++; break;
                    case 'f': if(more) more = compilearg(code, p, VAL_FLOAT); if(!more) { if(rep) break; compilefloat(code); fakeargs++; } numargs++; break;
                    case 'g': if(more) more = compilearg(code, p, VAL_FLOAT); if(!more) { if(rep) break; compilefloat(code, -FLT_MAX); fakeargs++; } numargs++; break;
                    case 't': if(more) more = compilearg(code, p, VAL_ANY); if(!more) { if(rep) break; compilenull(code); fakeargs++; } numargs++; break;
                    case 'e': if(more) more = compilearg(code, p, VAL_CODE); if(!more) { if(rep) break; compileblock(code); fakeargs++; } numargs++; break;
                    case 'r': if(more) more = compilearg(code, p, VAL_IDENT); if(!more) { if(rep) break; compileident(code); fakeargs++; } numargs++; break;
                    case '$': compileident(code, id); numargs++; break;
                    case 'N': compileint(code, numargs-fakeargs); numargs++; break;
#ifndef STANDALONE
                    case 'D': comtype = CODE_COMD; numargs++; break;
#endif
                    case 'C': comtype = CODE_COMC; if(more) while(numargs < MAXARGS && (more = compilearg(code, p, VAL_ANY))) numargs++; numargs = 1; goto endfmt;
                    case 'V': comtype = CODE_COMV; if(more) while(numargs < MAXARGS && (more = compilearg(code, p, VAL_ANY))) numargs++; numargs = 2; goto endfmt;
                    case '1': case '2': case '3': case '4':
                        if(more && numargs < MAXARGS)
                        {
                            int numrep = *fmt-'0'+1;
                            fmt -= numrep;
                            rep = true;
                        }
                        else for(; numargs > MAXARGS; numargs--) code.add(CODE_POP);
                    }
                endfmt:
                    code.add(comtype|(rettype < VAL_ANY ? rettype<<CODE_RET : 0)|(id->index<<8));
                    break;
                }
                case ID_LOCAL:
                    if(more) while(numargs < MAXARGS && (more = compilearg(code, p, VAL_IDENT))) numargs++;
                    if(more) while((more = compilearg(code, p, VAL_ANY))) code.add(CODE_POP);
                    code.add(CODE_LOCAL);
                    break;
                case ID_VAR:
                    if(!(more = compilearg(code, p, VAL_INT))) code.add(CODE_PRINT|(id->index<<8));
                    else if(!(id->flags&IDF_HEX) || !(more = compilearg(code, p, VAL_INT))) code.add(CODE_IVAR1|(id->index<<8));
                    else if(!(more = compilearg(code, p, VAL_INT))) code.add(CODE_IVAR2|(id->index<<8));
                    else code.add(CODE_IVAR3|(id->index<<8));
                    break;
                case ID_FVAR:
                    if(!(more = compilearg(code, p, VAL_FLOAT))) code.add(CODE_PRINT|(id->index<<8));
                    else code.add(CODE_FVAR1|(id->index<<8));
                    break;
                case ID_SVAR:
                    if(!(more = compilearg(code, p, VAL_STR))) code.add(CODE_PRINT|(id->index<<8));
                    else
                    {
                        int numconc = 0;
                        while(numconc+1 < MAXARGS && (more = compilearg(code, p, VAL_ANY))) numconc++;
                        if(numconc > 0) code.add(CODE_CONC|RET_STR|((numconc+1)<<8));
                        code.add(CODE_SVAR1|(id->index<<8));
                    }
                    break;
            }
            delete[] idname;
        }
    endstatement:
        if(more) while(compilearg(code, p, VAL_ANY)) code.add(CODE_POP);
        p += strcspn(p, ")];/\n\0");
        int c = *p++;
        switch(c)
        {
            case '\0':
                if(c != brak) debugcodeline(line, "\frmissing \"%c\"", brak);
                p--;
                return;

            case ')':
            case ']':
                if(c == brak) return;
                debugcodeline(line, "\frunexpected \"%c\"", c);
                break;

            case '/':
                if(*p == '/') p += strcspn(p, "\n\0");
                goto endstatement;
        }
    }
}

static void compilemain(vector<uint> &code, const char *p, int rettype = VAL_ANY)
{
    code.add(CODE_START);
    compilestatements(code, p, VAL_ANY);
    code.add(CODE_EXIT|(rettype < VAL_ANY ? rettype<<CODE_RET : 0));
}

uint *compilecode(const char *p)
{
    vector<uint> buf;
    buf.reserve(64);
    compilemain(buf, p);
    uint *code = new uint[buf.length()];
    memcpy(code, buf.getbuf(), buf.length()*sizeof(uint));
    code[0] += 0x100;
    return code;
}

void keepcode(uint *code)
{
    if(!code) return;
    switch(*code&CODE_OP_MASK)
    {
        case CODE_START:
            *code += 0x100;
            return;
    }
    switch(code[-1]&CODE_OP_MASK)
    {
        case CODE_START:
            code[-1] += 0x100;
            break;
        case CODE_OFFSET:
            code -= int(code[-1]>>8);
            *code += 0x100;
            break;
    }
}

void freecode(uint *code)
{
    if(!code) return;
    switch(*code&CODE_OP_MASK)
    {
        case CODE_START:
            *code -= 0x100;
            if(int(*code) < 0x100) delete[] code;
            return;
    }
    switch(code[-1]&CODE_OP_MASK)
    {
        case CODE_START:
            code[-1] -= 0x100;
            if(int(code[-1]) < 0x100) delete[] &code[-1];
            break;
        case CODE_OFFSET:
            code -= int(code[-1]>>8);
            *code -= 0x100;
            if(int(*code) < 0x100) delete[] code;
            break;
    }
}

static void printvar(ident *id)
{
    switch(id->type)
    {
        case ID_VAR:
        {
            int i = *id->storage.i;
            if(i < 0) conoutft(CON_MESG, "%s = %d", id->name, i);
            else if(id->flags&IDF_HEX && id->maxval==0xFFFFFF)
                conoutft(CON_MESG, "%s = 0x%.6X (%d, %d, %d)", id->name, i, (i>>16)&0xFF, (i>>8)&0xFF, i&0xFF);
            else
                conoutft(CON_MESG, id->flags&IDF_HEX ? "%s = 0x%X" : "%s = %d", id->name, i);
            break;
        }
        case ID_FVAR:
            conoutft(CON_MESG, "%s = %s", id->name, floatstr(*id->storage.f));
            break;
        case ID_SVAR:
            conoutft(CON_MESG, strchr(*id->storage.s, '"') ? "%s = [%s]" : "%s = \"%s\"", id->name, *id->storage.s);
            break;
    }
}

typedef void (__cdecl *comfun)();
typedef void (__cdecl *comfun1)(void *);
typedef void (__cdecl *comfun2)(void *, void *);
typedef void (__cdecl *comfun3)(void *, void *, void *);
typedef void (__cdecl *comfun4)(void *, void *, void *, void *);
typedef void (__cdecl *comfun5)(void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun6)(void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun7)(void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun8)(void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun9)(void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun10)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun11)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun12)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun13)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun14)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun15)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfun16)(void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *, void *);
typedef void (__cdecl *comfunv)(tagval *, int);

static const uint *skipcode(const uint *code, tagval &result)
{
    int depth = 0;
    for(;;)
    {
        uint op = *code++;
        switch(op&0xFF)
        {
            case CODE_MACRO:
            case CODE_VAL|RET_STR:
            {
                uint len = op>>8;
                code += len/sizeof(uint) + 1;
                continue;
            }
            case CODE_BLOCK:
            {
                uint len = op>>8;
                code += len;
                continue;
            }
            case CODE_ENTER:
                ++depth;
                continue;
            case CODE_EXIT|RET_NULL: case CODE_EXIT|RET_STR: case CODE_EXIT|RET_INT: case CODE_EXIT|RET_FLOAT:
                if(depth <= 0)
                {
                    forcearg(result, op&CODE_RET_MASK);
                    return code;
                }
                --depth;
                continue;
        }
    }
}

static inline void callcommand(ident *id, tagval *args, int numargs, bool lookup = false)
{
    int i = -1, fakeargs = 0;
    bool rep = false;
    for(const char *fmt = id->args; *fmt; fmt++) switch(*fmt)
    {
        case 'i': if(++i >= numargs) { if(rep) break; args[i].setint(0); fakeargs++; } else forceint(args[i]); break;
        case 'b': if(++i >= numargs) { if(rep) break; args[i].setint(INT_MIN); fakeargs++; } else forceint(args[i]); break;
        case 'f': if(++i >= numargs) { if(rep) break; args[i].setfloat(0.0f); fakeargs++; } else forcefloat(args[i]); break;
        case 'g': if(++i >= numargs) { if(rep) break; args[i].setfloat(-FLT_MAX); fakeargs++; } else forcefloat(args[i]); break;
        case 's': if(++i >= numargs) { if(rep) break; args[i].setstr(newstring("")); fakeargs++; } else forcestr(args[i]); break;
        case 't': if(++i >= numargs) { if(rep) break; args[i].setnull(); fakeargs++; } break;
        case 'e':
            if(++i >= numargs)
            {
                if(rep) break;
                static uint buf[2] = { CODE_START + 0x100, CODE_EXIT };
                args[i].setcode(buf);
                fakeargs++;
            }
            else
            {
                vector<uint> buf;
                buf.reserve(64);
                compilemain(buf, numargs <= i ? "" : args[i].getstr());
                freearg(args[i]);
                args[i].setcode(buf.getbuf()+1);
                buf.disown();
            }
            break;
        case 'r': if(++i >= numargs) { if(rep) break; args[i].setident(dummyident); fakeargs++; } else forceident(args[i]); break;
        case '$': if(++i < numargs) freearg(args[i]); args[i].setident(id); break;
        case 'N': if(++i < numargs) freearg(args[i]); args[i].setint(lookup ? -1 : i-fakeargs); break;
#ifndef STANDALONE
        case 'D': if(++i < numargs) freearg(args[i]); args[i].setint(addreleaseaction(conc(args, i, true, id->name)) ? 1 : 0); fakeargs++; break;
#endif
        case 'C': { i = max(i+1, numargs); vector<char> buf; ((comfun1)id->fun)(conc(buf, args, i, true)); goto cleanup; }
        case 'V': i = max(i+1, numargs); ((comfunv)id->fun)(args, i); goto cleanup;
        case '1': case '2': case '3': case '4': if(i+1 < numargs) { fmt -= *fmt-'0'+1; rep = true; } break;
    }
    #define ARG(n) (id->argmask&(1<<n) ? (void *)args[n].s : (void *)&args[n].i)
    #define CALLCOM(n) \
        switch(n) \
        { \
            case 0: ((comfun)id->fun)(); break; \
            case 1: ((comfun1)id->fun)(ARG(0)); break; \
            case 2: ((comfun2)id->fun)(ARG(0), ARG(1)); break; \
            case 3: ((comfun3)id->fun)(ARG(0), ARG(1), ARG(2)); break; \
            case 4: ((comfun4)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3)); break; \
            case 5: ((comfun5)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4)); break; \
            case 6: ((comfun6)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5)); break; \
            case 7: ((comfun7)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6)); break; \
            case 8: ((comfun8)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7)); break; \
            case 9: ((comfun9)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8)); break; \
            case 10: ((comfun10)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9)); break; \
            case 11: ((comfun11)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9), ARG(10)); break; \
            case 12: ((comfun12)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9), ARG(10), ARG(11)); break; \
            case 13: ((comfun13)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9), ARG(10), ARG(11), ARG(12)); break; \
            case 14: ((comfun14)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9), ARG(10), ARG(11), ARG(12), ARG(13)); break; \
            case 15: ((comfun15)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9), ARG(10), ARG(11), ARG(12), ARG(13), ARG(14)); break; \
            case 16: ((comfun16)id->fun)(ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8), ARG(9), ARG(10), ARG(11), ARG(12), ARG(13), ARG(14), ARG(15)); break; \
        }
    ++i;
    CALLCOM(i)
cleanup:
    loopk(i) freearg(args[k]);
    for(; i < numargs; i++) freearg(args[i]);
}

#define MAXRUNDEPTH 255
static int rundepth = 0;

static const uint *runcode(const uint *code, tagval &result)
{
    result.setnull();
    if(rundepth >= MAXRUNDEPTH)
    {
        debugcode("exceeded recursion limit");
        return skipcode(code, result);
    }
    ++rundepth;
    ident *id = NULL;
    int numargs = 0;
    tagval args[MAXARGS+1], *prevret = commandret;
    commandret = &result;
    for(;;)
    {
        uint op = *code++;
        switch(op&0xFF)
        {
            case CODE_START: case CODE_OFFSET: continue;

            case CODE_POP:
                freearg(args[--numargs]);
                continue;
            case CODE_ENTER:
                code = runcode(code, args[numargs++]);
                continue;
            case CODE_EXIT|RET_NULL: case CODE_EXIT|RET_STR: case CODE_EXIT|RET_INT: case CODE_EXIT|RET_FLOAT:
                forcearg(result, op&CODE_RET_MASK);
                goto exit;
            case CODE_PRINT:
                printvar(identmap[op>>8]);
                continue;
            case CODE_LOCAL:
            {
                identstack locals[MAXARGS];
                freearg(result);
                loopi(numargs) pushalias(*args[i].id, locals[i]);
                code = runcode(code, result);
                loopi(numargs) popalias(*args[i].id);
                goto exit;
            }

            case CODE_MACRO:
            {
                uint len = op>>8;
                args[numargs++].setmacro(code);
                code += len/sizeof(uint) + 1;
                continue;
            }

            case CODE_VAL|RET_STR:
            {
                uint len = op>>8;
                args[numargs++].setstr(newstring((const char *)code, len));
                code += len/sizeof(uint) + 1;
                continue;
            }
            case CODE_VALI|RET_STR:
            {
                char s[4] = { char((op>>8)&0xFF), char((op>>16)&0xFF), char((op>>24)&0xFF), '\0' };
                args[numargs++].setstr(newstring(s));
                continue;
            }
            case CODE_VAL|RET_NULL:
            case CODE_VALI|RET_NULL: args[numargs++].setnull(); continue;
            case CODE_VAL|RET_INT: args[numargs++].setint(int(*code++)); continue;
            case CODE_VALI|RET_INT: args[numargs++].setint(int(op)>>8); continue;
            case CODE_VAL|RET_FLOAT: args[numargs++].setfloat(*(const float *)code++); continue;
            case CODE_VALI|RET_FLOAT: args[numargs++].setfloat(float(int(op)>>8)); continue;

            case CODE_FORCE|RET_STR: forcestr(args[numargs-1]); continue;
            case CODE_FORCE|RET_INT: forceint(args[numargs-1]); continue;
            case CODE_FORCE|RET_FLOAT: forcefloat(args[numargs-1]); continue;

            case CODE_RESULT|RET_NULL: case CODE_RESULT|RET_STR: case CODE_RESULT|RET_INT: case CODE_RESULT|RET_FLOAT:
            litval:
                freearg(result);
                result = args[0];
                forcearg(result, op&CODE_RET_MASK);
                args[0].setnull();
                freeargs(args, numargs, 0);
                continue;

            case CODE_BLOCK:
            {
                uint len = op>>8;
                args[numargs++].setcode(code+1);
                code += len;
                continue;
            }
            case CODE_COMPILE:
            {
                tagval &arg = args[numargs-1];
                vector<uint> buf;
                switch(arg.type)
                {
                    case VAL_INT: buf.reserve(8); buf.add(CODE_START); compileint(buf, arg.i); buf.add(CODE_RESULT); buf.add(CODE_EXIT); break;
                    case VAL_FLOAT: buf.reserve(8); buf.add(CODE_START); compilefloat(buf, arg.f); buf.add(CODE_RESULT); buf.add(CODE_EXIT); break;
                    case VAL_STR: case VAL_MACRO: buf.reserve(64); compilemain(buf, arg.s); freearg(arg); break;
                    default: buf.reserve(8); buf.add(CODE_START); compilenull(buf); buf.add(CODE_RESULT); buf.add(CODE_EXIT); break;
                }
                arg.setcode(buf.getbuf()+1);
                buf.disown();
                continue;
            }

            case CODE_IDENT:
                args[numargs++].setident(identmap[op>>8]);
                continue;
            case CODE_IDENTARG:
            {
                ident *id = identmap[op>>8];
                if(!(aliasstack->usedargs&(1<<id->index)))
                {
                    pusharg(*id, nullval, aliasstack->argstack[id->index]);
                    aliasstack->usedargs |= 1<<id->index;
                }
                args[numargs++].setident(id);
                continue;
            }
            case CODE_IDENTU:
            {
                tagval &arg = args[numargs-1];
                ident *id = arg.type == VAL_STR || arg.type == VAL_MACRO ? newident(arg.s, IDF_UNKNOWN) : dummyident;
                if(id->index < MAXARGS && !(aliasstack->usedargs&(1<<id->index)))
                {
                    pusharg(*id, nullval, aliasstack->argstack[id->index]);
                    aliasstack->usedargs |= 1<<id->index;
                }
                freearg(arg);
                arg.setident(id);
                continue;
            }

            case CODE_LOOKUPU|RET_STR:
                #define LOOKUPU(aval, sval, ival, fval, nval) { \
                    tagval &arg = args[numargs-1]; \
                    if(arg.type != VAL_STR && arg.type != VAL_MACRO) continue; \
                    id = idents.access(arg.s); \
                    if(id) switch(id->type) \
                    { \
                        case ID_ALIAS: \
                            if(id->flags&IDF_UNKNOWN) break; \
                            freearg(arg); \
                            if(id->index < MAXARGS && !(aliasstack->usedargs&(1<<id->index))) { nval; continue; } \
                            aval; \
                            continue; \
                        case ID_SVAR: freearg(arg); sval; continue; \
                        case ID_VAR: freearg(arg); ival; continue; \
                        case ID_FVAR: freearg(arg); fval; continue; \
                        case ID_COMMAND: \
                        { \
                            freearg(arg); \
                            arg.setnull(); \
                            commandret = &arg; \
                            tagval buf[MAXARGS]; \
                            callcommand(id, buf, 0, true); \
                            forcearg(arg, op&CODE_RET_MASK); \
                            commandret = &result; \
                            continue; \
                        } \
                        default: freearg(arg); nval; continue; \
                    } \
                    debugcode("\frunknown alias lookup: %s", arg.s); \
                    freearg(arg); \
                    nval; \
                    continue; \
                }
                LOOKUPU(arg.setstr(newstring(id->getstr())),
                        arg.setstr(newstring(*id->storage.s)),
                        arg.setstr(newstring(intstr(*id->storage.i))),
                        arg.setstr(newstring(floatstr(*id->storage.f))),
                        arg.setstr(newstring("")));
            case CODE_LOOKUP|RET_STR:
                #define LOOKUP(aval) { \
                    id = identmap[op>>8]; \
                    if(id->flags&IDF_UNKNOWN) debugcode("\frunknown alias lookup: %s", id->name); \
                    aval; \
                    continue; \
                }
                LOOKUP(args[numargs++].setstr(newstring(id->getstr())));
            case CODE_LOOKUPARG|RET_STR:
                #define LOOKUPARG(aval, nval) { \
                    id = identmap[op>>8]; \
                    if(!(aliasstack->usedargs&(1<<id->index))) { nval; continue; } \
                    aval; \
                    continue; \
                }
                LOOKUPARG(args[numargs++].setstr(newstring(id->getstr())), args[numargs++].setstr(newstring("")));
            case CODE_LOOKUPU|RET_INT:
                LOOKUPU(arg.setint(id->getint()),
                        arg.setint(parseint(*id->storage.s)),
                        arg.setint(*id->storage.i),
                        arg.setint(int(*id->storage.f)),
                        arg.setint(0));
            case CODE_LOOKUP|RET_INT:
                LOOKUP(args[numargs++].setint(id->getint()));
            case CODE_LOOKUPARG|RET_INT:
                LOOKUPARG(args[numargs++].setint(id->getint()), args[numargs++].setint(0));
            case CODE_LOOKUPU|RET_FLOAT:
                LOOKUPU(arg.setfloat(id->getfloat()),
                        arg.setfloat(parsefloat(*id->storage.s)),
                        arg.setfloat(float(*id->storage.i)),
                        arg.setfloat(*id->storage.f),
                        arg.setfloat(0.0f));
            case CODE_LOOKUP|RET_FLOAT:
                LOOKUP(args[numargs++].setfloat(id->getfloat()));
            case CODE_LOOKUPARG|RET_FLOAT:
                LOOKUPARG(args[numargs++].setfloat(id->getfloat()), args[numargs++].setfloat(0.0f));
            case CODE_LOOKUPU|RET_NULL:
                LOOKUPU(id->getval(arg),
                        arg.setstr(newstring(*id->storage.s)),
                        arg.setint(*id->storage.i),
                        arg.setfloat(*id->storage.f),
                        arg.setnull());
            case CODE_LOOKUP|RET_NULL:
                LOOKUP(id->getval(args[numargs++]));
            case CODE_LOOKUPARG|RET_NULL:
                LOOKUPARG(id->getval(args[numargs++]), args[numargs++].setnull());

            case CODE_SVAR|RET_STR: case CODE_SVAR|RET_NULL: args[numargs++].setstr(newstring(*identmap[op>>8]->storage.s)); continue;
            case CODE_SVAR|RET_INT: args[numargs++].setint(parseint(*identmap[op>>8]->storage.s)); continue;
            case CODE_SVAR|RET_FLOAT: args[numargs++].setfloat(parsefloat(*identmap[op>>8]->storage.s)); continue;
            case CODE_SVAR1: setsvarchecked(identmap[op>>8], args[0].s); freeargs(args, numargs, 0); continue;

            case CODE_IVAR|RET_INT: case CODE_IVAR|RET_NULL: args[numargs++].setint(*identmap[op>>8]->storage.i); continue;
            case CODE_IVAR|RET_STR: args[numargs++].setstr(newstring(intstr(*identmap[op>>8]->storage.i))); continue;
            case CODE_IVAR|RET_FLOAT: args[numargs++].setfloat(float(*identmap[op>>8]->storage.i)); continue;
            case CODE_IVAR1: setvarchecked(identmap[op>>8], args[0].i); numargs = 0; continue;
            case CODE_IVAR2: setvarchecked(identmap[op>>8], (args[0].i<<16)|(args[1].i<<8)); numargs = 0; continue;
            case CODE_IVAR3: setvarchecked(identmap[op>>8], (args[0].i<<16)|(args[1].i<<8)|args[2].i); numargs = 0; continue;

            case CODE_FVAR|RET_FLOAT: case CODE_FVAR|RET_NULL: args[numargs++].setfloat(*identmap[op>>8]->storage.f); continue;
            case CODE_FVAR|RET_STR: args[numargs++].setstr(newstring(floatstr(*identmap[op>>8]->storage.f))); continue;
            case CODE_FVAR|RET_INT: args[numargs++].setint(int(*identmap[op>>8]->storage.f)); continue;
            case CODE_FVAR1: setfvarchecked(identmap[op>>8], args[0].f); numargs = 0; continue;

            case CODE_COM|RET_NULL: case CODE_COM|RET_STR: case CODE_COM|RET_FLOAT: case CODE_COM|RET_INT:
                id = identmap[op>>8];
#ifndef STANDALONE
            callcom:
#endif
                forcenull(result);
                CALLCOM(numargs)
            forceresult:
                freeargs(args, numargs, 0);
                forcearg(result, op&CODE_RET_MASK);
                continue;
#ifndef STANDALONE
            case CODE_COMD|RET_NULL: case CODE_COMD|RET_STR: case CODE_COMD|RET_FLOAT: case CODE_COMD|RET_INT:
                id = identmap[op>>8];
                args[numargs].setint(addreleaseaction(conc(args, numargs, true, id->name)) ? 1 : 0);
                numargs++;
                goto callcom;
#endif
            case CODE_COMV|RET_NULL: case CODE_COMV|RET_STR: case CODE_COMV|RET_FLOAT: case CODE_COMV|RET_INT:
                id = identmap[op>>8];
                forcenull(result);
                ((comfunv)id->fun)(args, numargs);
                goto forceresult;
            case CODE_COMC|RET_NULL: case CODE_COMC|RET_STR: case CODE_COMC|RET_FLOAT: case CODE_COMC|RET_INT:
                id = identmap[op>>8];
                forcenull(result);
                {
                    vector<char> buf;
                    buf.reserve(MAXSTRLEN);
                    ((comfun1)id->fun)(conc(buf, args, numargs, true));
                }
                goto forceresult;

            case CODE_CONC|RET_NULL: case CODE_CONC|RET_STR: case CODE_CONC|RET_FLOAT: case CODE_CONC|RET_INT:
            case CODE_CONCW|RET_NULL: case CODE_CONCW|RET_STR: case CODE_CONCW|RET_FLOAT: case CODE_CONCW|RET_INT:
            {
                int numconc = op>>8;
                char *s = conc(&args[numargs-numconc], numconc, (op&CODE_OP_MASK)==CODE_CONC);
                freeargs(args, numargs, numargs-numconc);
                args[numargs++].setstr(s);
                forcearg(args[numargs-1], op&CODE_RET_MASK);
                continue;
            }

            case CODE_CONCM|RET_NULL: case CODE_CONCM|RET_STR: case CODE_CONCM|RET_FLOAT: case CODE_CONCM|RET_INT:
            {
                int numconc = op>>8;
                char *s = conc(&args[numargs-numconc], numconc, false);
                freeargs(args, numargs, numargs-numconc);
                result.setstr(s);
                forcearg(result, op&CODE_RET_MASK);
                continue;
            }

            case CODE_ALIAS:
                setalias(*identmap[op>>8], args[--numargs]);
                freeargs(args, numargs, 0);
                continue;
            case CODE_ALIASARG:
                setarg(*identmap[op>>8], args[--numargs]);
                freeargs(args, numargs, 0);
                continue;
            case CODE_ALIASU:
                forcestr(args[0]);
                setalias(args[0].s, args[--numargs]);
                freeargs(args, numargs, 0);
                continue;

            case CODE_CALL|RET_NULL: case CODE_CALL|RET_STR: case CODE_CALL|RET_FLOAT: case CODE_CALL|RET_INT:
                #define CALLALIAS(offset) { \
                    identstack argstack[MAXARGS]; \
                    for(int i = 0; i < numargs-offset; i++) \
                        pusharg(*identmap[i], args[i+offset], argstack[i]); \
                    int oldargs = _numargs, newargs = numargs-offset; \
                    _numargs = newargs; \
                    identlink aliaslink = { id, aliasstack, (1<<newargs)-1, argstack }; \
                    aliasstack = &aliaslink; \
                    if(!id->code) id->code = compilecode(id->getstr()); \
                    uint *code = id->code; \
                    code[0] += 0x100; \
                    runcode(code+1, result); \
                    code[0] -= 0x100; \
                    if(int(code[0]) < 0x100) delete[] code; \
                    aliasstack = aliaslink.next; \
                    for(int i = 0; i < newargs; i++) \
                        poparg(*identmap[i]); \
                    for(int argmask = aliaslink.usedargs&(~0<<newargs), i = newargs; argmask; i++) \
                        if(argmask&(1<<i)) { poparg(*identmap[i]); argmask &= ~(1<<i); } \
                    forcearg(result, op&CODE_RET_MASK); \
                    _numargs = oldargs; \
                    numargs = 0; \
                }
                forcenull(result);
                id = identmap[op>>8];
                if(id->flags&IDF_UNKNOWN)
                {
                    debugcode("\frunknown command: %s", id->name);
                    goto forceresult;
                }
                CALLALIAS(0);
                continue;
            case CODE_CALLARG|RET_NULL: case CODE_CALLARG|RET_STR: case CODE_CALLARG|RET_FLOAT: case CODE_CALLARG|RET_INT:
                forcenull(result);
                id = identmap[op>>8];
                if(!(aliasstack->usedargs&(1<<id->index))) goto forceresult;
                CALLALIAS(0);
                continue;

            case CODE_CALLU|RET_NULL: case CODE_CALLU|RET_STR: case CODE_CALLU|RET_FLOAT: case CODE_CALLU|RET_INT:
                if(args[0].type != VAL_STR) goto litval;
                id = idents.access(args[0].s);
                if(!id || (id->flags&IDF_REWRITE && (!(identflags&IDF_WORLD) || !(id->flags&IDF_WORLD))))
                {
                noid:
                    if(checknumber(args[0].s)) goto litval;
                    if(!id || (id->flags&IDF_REWRITE && (!(identflags&IDF_WORLD) || !(id->flags&IDF_WORLD))))
                    {
                        if(server::rewritecommand(id, args, numargs)) goto forceresult;
                    }
                    debugcode("\frunknown command: %s", args[0].s);
                    forcenull(result);
                    goto forceresult;
                }
                forcenull(result);
                switch(id->type)
                {
                    case ID_COMMAND:
                        freearg(args[0]);
                        callcommand(id, args+1, numargs-1);
                        forcearg(result, op&CODE_RET_MASK);
                        numargs = 0;
                        continue;
                    case ID_LOCAL:
                        identstack locals[MAXARGS];
                        freearg(args[0]);
                        loopj(numargs-1) pushalias(*forceident(args[j+1]), locals[j]);
                        code = runcode(code, result);
                        loopj(numargs-1) popalias(*args[j+1].id);
                        goto exit;
                    case ID_VAR:
                        if(numargs <= 1) printvar(id);
                        else
                        {
                            int val = forceint(args[1]);
                            if(id->flags&IDF_HEX && numargs > 2)
                            {
                                val = (val << 16) | (forceint(args[2])<<8);
                                if(numargs > 3) val |= forceint(args[3]);
                            }
                            setvarchecked(id, val);
                        }
                        goto forceresult;
                    case ID_FVAR:
                        if(numargs <= 1) printvar(id); else setfvarchecked(id, forcefloat(args[1]));
                        goto forceresult;
                    case ID_SVAR:
                        if(numargs <= 1) printvar(id); else setsvarchecked(id, forcestr(args[1]));
                        goto forceresult;
                    case ID_ALIAS:
                        if(id->index < MAXARGS && !(aliasstack->usedargs&(1<<id->index))) goto forceresult;
                        if(id->valtype==VAL_NULL) goto noid;
                        freearg(args[0]);
                        CALLALIAS(1);
                        continue;
                    default:
                        goto forceresult;
                }
        }
    }
exit:
    commandret = prevret;
    --rundepth;
    return code;
}

void executeret(const uint *code, tagval &result)
{
    runcode(code, result);
}

void executeret(const char *p, tagval &result)
{
    vector<uint> code;
    code.reserve(64);
    compilemain(code, p, VAL_ANY);
    runcode(code.getbuf()+1, result);
    if(int(code[0]) >= 0x100) code.disown();
}

char *executestr(const uint *code)
{
    tagval result;
    runcode(code, result);
    if(result.type == VAL_NULL) return NULL;
    forcestr(result);
    return result.s;
}

char *executestr(const char *p)
{
    tagval result;
    executeret(p, result);
    if(result.type == VAL_NULL) return NULL;
    forcestr(result);
    return result.s;
}

int execute(const uint *code)
{
    tagval result;
    runcode(code, result);
    int i = result.getint();
    freearg(result);
    return i;
}

int execute(const char *p)
{
    vector<uint> code;
    code.reserve(64);
    compilemain(code, p, VAL_INT);
    tagval result;
    runcode(code.getbuf()+1, result);
    if(int(code[0]) >= 0x100) code.disown();
    int i = result.getint();
    freearg(result);
    return i;
}

int execute(const char *p, bool nonworld)
{
    int oldflags = identflags;
    if(nonworld) identflags &= ~IDF_WORLD;
    int result = execute(p);
    if(nonworld) identflags = oldflags;
    return result;
}

static inline bool getbool(const char *s)
{
    switch(s[0])
    {
        case '+': case '-':
            switch(s[1])
            {
                case '0': break;
                case '.': return !isdigit(s[2]) || parsefloat(s) != 0;
                default: return true;
            }
            // fall through
        case '0':
        {
            char *end;
            int val = int(strtoul((char *)s, &end, 0));
            if(val) return true;
            switch(*end)
            {
                case 'e': case '.': return parsefloat(s) != 0;
                default: return false;
            }
        }
        case '.': return !isdigit(s[1]) || parsefloat(s) != 0;
        case '\0': return false;
        default: return true;
    }
}

static inline bool getbool(const tagval &v)
{
    switch(v.type)
    {
        case VAL_FLOAT: return v.f!=0;
        case VAL_INT: return v.i!=0;
        case VAL_STR: case VAL_MACRO: return getbool(v.s);
        default: return false;
    }
}

bool executebool(const uint *code)
{
    tagval result;
    runcode(code, result);
    bool b = getbool(result);
    freearg(result);
    return b;
}

bool executebool(const char *p)
{
    tagval result;
    executeret(p, result);
    bool b = getbool(result);
    freearg(result);
    return b;
}

bool execfile(const char *cfgfile, bool msg, int flags)
{
    string s;
    copystring(s, cfgfile);
    char *buf = loadfile(s, NULL);
    if(!buf)
    {
        if(msg || verbose >= 2) conoutf("\frcould not read %s", cfgfile);
        return false;
    }
    int oldflags = identflags;
    int oldversion = versioning;
    if(flags&EXEC_NOWORLD) identflags &= ~IDF_WORLD;
    if(flags&EXEC_VERSION) versioning = flags&EXEC_BUILTIN ? 2 : 1;
    const char *oldsourcefile = sourcefile, *oldsourcestr = sourcestr;
    sourcefile = cfgfile;
    sourcestr = buf;
    execute(buf);
    sourcefile = oldsourcefile;
    sourcestr = oldsourcestr;
    if(flags&EXEC_NOWORLD) identflags = oldflags;
    if(flags&EXEC_VERSION) versioning = oldversion;
    delete[] buf;
    if(verbose >= 2) conoutf("\faloaded script %s", cfgfile);
    return true;
}
ICOMMAND(0, exec, "sib", (char *file, int *flags, int *msg), intret(execfile(file, *msg != 0, *flags) ? 1 : 0));

const char *escapestring(const char *s)
{
    static vector<char> strbuf[16];
    static int stridx = 0;
    stridx = (stridx+1)%16;
    vector<char> &buf = strbuf[stridx];
    buf.setsize(0);
    buf.add('"');
    for(; *s; s++) switch(*s)
    {
        case '\n': buf.put("^n", 2); break;
        case '\t': buf.put("^t", 2); break;
        case '\f': buf.put("^f", 2); break;
        case '"': buf.put("^\"", 2); break;
        case '^': buf.put("^^", 2); break;
        default: buf.add(*s); break;
    }
    buf.put("\"\0", 2);
    return buf.getbuf();
}

ICOMMAND(0, escape, "s", (char *s), result(escapestring(s)));
ICOMMAND(0, unescape, "s", (char *s),
{
    int len = strlen(s);
    char *d = newstring(len);
    d[unescapestring(d, s, &s[len])] = '\0';
    stringret(d);
});

const char *escapeid(const char *s)
{
    const char *end = s + strcspn(s, "\"/;()[]@ \f\t\r\n\0");
    return *end ? escapestring(s) : s;
}

bool validateblock(const char *s)
{
    const int maxbrak = 100;
    static char brakstack[maxbrak];
    int brakdepth = 0;
    for(; *s; s++) switch(*s)
    {
        case '[': case '(': if(brakdepth >= maxbrak) return false; brakstack[brakdepth++] = *s; break;
        case ']': if(brakdepth <= 0 || brakstack[--brakdepth] != '[') return false; break;
        case ')': if(brakdepth <= 0 || brakstack[--brakdepth] != '(') return false; break;
        case '"': s = parsestring(s + 1); if(*s != '"') return false; break;
        case '/': if(s[1] == '/') return false; break;
        case '@': case '\f': return false;
    }
    return brakdepth == 0;
}

// below the commands that implement a small imperative language. thanks to the semantics of
// () and [] expressions, any control construct can be defined trivially.

static string retbuf[4];
static int retidx = 0;

const char *intstr(int v)
{
    retidx = (retidx + 1)%4;
    intformat(retbuf[retidx], v);
    return retbuf[retidx];
}

const char *intstr(ident *id)
{
    retidx = (retidx + 1)%4;
    formatstring(retbuf[retidx], id->flags&IDF_HEX && *id->storage.i >= 0 ? (id->maxval==0xFFFFFF ? "0x%.6X" : "0x%X") : "%d", *id->storage.i);
    return retbuf[retidx];
}

void intret(int v)
{
    commandret->setint(v);
}

const char *floatstr(float v)
{
    retidx = (retidx + 1)%4;
    floatformat(retbuf[retidx], v);
    return retbuf[retidx];
}

void floatret(float v)
{
    commandret->setfloat(v);
}

#undef ICOMMANDNAME
#define ICOMMANDNAME(name) _stdcmd

ICOMMAND(0, do, "e", (uint *body), executeret(body, *commandret));
ICOMMAND(0, if, "tee", (tagval *cond, uint *t, uint *f), executeret(getbool(*cond) ? t : f, *commandret));
ICOMMAND(0, ?, "ttt", (tagval *cond, tagval *t, tagval *f), result(*(getbool(*cond) ? t : f)));

ICOMMAND(0, pushif, "rte", (ident *id, tagval *v, uint *code),
{
    if(id->type != ID_ALIAS || id->index < MAXARGS) return;
    if(getbool(*v))
    {
        identstack stack;
        pusharg(*id, *v, stack);
        v->type = VAL_NULL;
        id->flags &= ~IDF_UNKNOWN;
        executeret(code, *commandret);
        poparg(*id);
    }
});

void loopiter(ident *id, identstack &stack, const tagval &v)
{
    if(id->stack != &stack)
    {
        pusharg(*id, v, stack);
        id->flags &= ~IDF_UNKNOWN;
    }
    else
    {
        if(id->valtype == VAL_STR) delete[] id->val.s;
        cleancode(*id);
        id->setval(v);
    }
}

void loopend(ident *id, identstack &stack)
{
    if(id->stack == &stack) poparg(*id);
}

static inline void setiter(ident &id, int i, identstack &stack)
{
    if(id.stack == &stack)
    {
        if(id.valtype != VAL_INT)
        {
            if(id.valtype == VAL_STR) delete[] id.val.s;
            cleancode(id);
            id.valtype = VAL_INT;
        }
        id.val.i = i;
    }
    else
    {
        tagval t;
        t.setint(i);
        pusharg(id, t, stack);
        id.flags &= ~IDF_UNKNOWN;
    }
}
ICOMMAND(0, loop, "rie", (ident *id, int *n, uint *body),
{
    if(*n <= 0 || id->type!=ID_ALIAS) return;
    identstack stack;
    loopi(*n)
    {
        setiter(*id, i, stack);
        execute(body);
    }
    poparg(*id);
});
ICOMMAND(0, loopwhile, "riee", (ident *id, int *n, uint *cond, uint *body),
{
    if(*n <= 0 || id->type!=ID_ALIAS) return;
    identstack stack;
    loopi(*n)
    {
        setiter(*id, i, stack);
        if(!executebool(cond)) break;
        execute(body);
    }
    poparg(*id);
});
ICOMMAND(0, while, "ee", (uint *cond, uint *body), while(executebool(cond)) execute(body));

char *loopconc(ident *id, int n, uint *body, bool space)
{
    identstack stack;
    vector<char> s;
    loopi(n)
    {
        setiter(*id, i, stack);
        tagval v;
        executeret(body, v);
        const char *vstr = v.getstr();
        int len = strlen(vstr);
        if(space && i) s.add(' ');
        s.put(vstr, len);
        freearg(v);
    }
    if(n > 0) poparg(*id);
    s.add('\0');
    return newstring(s.getbuf(), s.length()-1);
}

ICOMMAND(0, loopconcat, "rie", (ident *id, int *n, uint *body),
{
    if(*n > 0 && id->type==ID_ALIAS) commandret->setstr(loopconc(id, *n, body, true));
});

ICOMMAND(0, loopconcatword, "rie", (ident *id, int *n, uint *body),
{
    if(*n > 0 && id->type==ID_ALIAS) commandret->setstr(loopconc(id, *n, body, false));
});

void concat(tagval *v, int n)
{
    commandret->setstr(conc(v, n, true));
}
COMMAND(0, concat, "V");

void concatword(tagval *v, int n)
{
    commandret->setstr(conc(v, n, false));
}
COMMAND(0, concatword, "V");

void result(tagval &v)
{
    *commandret = v;
    v.type = VAL_NULL;
}

void stringret(char *s)
{
    commandret->setstr(s);
}

void result(const char *s)
{
    commandret->setstr(newstring(s));
}

ICOMMAND(0, result, "t", (tagval *v),
{
    *commandret = *v;
    v->type = VAL_NULL;
});

void format(tagval *args, int numargs)
{
    vector<char> s;
    const char *f = args[0].getstr();
    while(*f)
    {
        int c = *f++;
        if(c == '%')
        {
            int i = *f++;
            if(i >= '1' && i <= '9')
            {
                i -= '0';
                const char *sub = i < numargs ? args[i].getstr() : "";
                while(*sub) s.add(*sub++);
            }
            else s.add(i);
        }
        else s.add(c);
    }
    s.add('\0');
    result(s.getbuf());
}
COMMAND(0, format, "V");

static const char *liststart = NULL, *listend = NULL, *listquotestart = NULL, *listquoteend = NULL;

static inline void skiplist(const char *&p)
{
    for(;;)
    {
        p += strspn(p, " \t\r\n");
        if(p[0]!='/' || p[1]!='/') break;
        p += strcspn(p, "\n\0");
    }
}

static bool parselist(const char *&s, const char *&start = liststart, const char *&end = listend, const char *&quotestart = listquotestart, const char *&quoteend = listquoteend)
{
    skiplist(s);
    switch(*s)
    {
        case '"': quotestart = s++; start = s; s = parsestring(s); end = s; if(*s == '"') s++; quoteend = s; break;
        case '(': case '[':
            quotestart = s;
            start = s+1;
            for(int braktype = *s++, brak = 1;;)
            {
                s += strcspn(s, "\"/;()[]\0");
                int c = *s++;
                switch(c)
                {
                    case '\0': s--; quoteend = end = s; return true;
                    case '"': s = parsestring(s); if(*s == '"') s++; break;
                    case '/': if(*s == '/') s += strcspn(s, "\n\0"); break;
                    case '(': case '[': if(c == braktype) brak++; break;
                    case ')': if(braktype == '(' && --brak <= 0) goto endblock; break;
                    case ']': if(braktype == '[' && --brak <= 0) goto endblock; break;
                }
            }
        endblock:
            end = s-1;
            quoteend = s;
            break;
        case '\0': case ')': case ']': return false;
        default: quotestart = start = s; s = parseword(s); quoteend = end = s; break;
    }
    skiplist(s);
    if(*s == ';') s++;
    return true;
}

void explodelist(const char *s, vector<char *> &elems, int limit)
{
    const char *start, *end;
    while((limit < 0 || elems.length() < limit) && parselist(s, start, end))
        elems.add(newstring(start, end-start));
}

char *indexlist(const char *s, int pos)
{
    loopi(pos) if(!parselist(s)) return newstring("");
    const char *start, *end;
    return parselist(s, start, end) ? newstring(start, end-start) : newstring("");
}

int listlen(const char *s)
{
    int n = 0;
    while(parselist(s)) n++;
    return n;
}
ICOMMAND(0, listlen, "s", (char *s), intret(listlen(s)));

const char *indexlist(const char *s, int pos, int &len)
{
    loopi(pos) if(!parselist(s)) break;
    const char *start = s, *end = s;
    parselist(s, start, end);
    len = end-start;
    return start;
}

void at(tagval *args, int numargs)
{
    if(!numargs) return;
    const char *start = args[0].getstr(), *end = start + strlen(start);
    for(int i = 1; i < numargs; i++)
    {
        const char *list = start;
        int pos = args[i].getint();
        for(; pos > 0; pos--) if(!parselist(list)) break;
        if(pos > 0 || !parselist(list, start, end)) start = end = "";
    }
    commandret->setstr(newstring(start, end-start));
}
COMMAND(0, at, "si1V");

void substring(char *s, int *start, int *count, int *numargs)
{
    int len = strlen(s), offset = clamp(*start, 0, len);
    commandret->setstr(newstring(&s[offset], *numargs >= 3 ? clamp(*count, 0, len - offset) : len - offset));
}
COMMAND(0, substring, "siiN");

void sublist(const char *s, int *skip, int *count, int *numargs)
{
    int offset = max(*skip, 0), len = *numargs >= 3 ? max(*count, 0) : -1;
    loopi(offset) if(!parselist(s)) break;
    if(len < 0) { if(offset > 0) skiplist(s); commandret->setstr(newstring(s)); return; }
    const char *list = s, *start, *end, *qstart, *qend = s;
    if(len > 0 && parselist(s, start, end, list, qend)) while(--len > 0 && parselist(s, start, end, qstart, qend));
    commandret->setstr(newstring(list, qend - list));
}
COMMAND(0, sublist, "siiN");

ICOMMAND(0, stripcolors, "s", (char *s),
{
    size_t len = strlen(s);
    char *d = newstring(len);
    filterstring(d, s, false, true, true, false, len);
    stringret(d);
});

static inline void setiter(ident &id, char *val, identstack &stack)
{
    if(id.stack == &stack)
    {
        if(id.valtype == VAL_STR) delete[] id.val.s;
        else id.valtype = VAL_STR;
        cleancode(id);
        id.val.s = val;
    }
    else
    {
        tagval t;
        t.setstr(val);
        pusharg(id, t, stack);
        id.flags &= ~IDF_UNKNOWN;
    }
}

void listfind(ident *id, const char *list, const uint *body)
{
    if(id->type!=ID_ALIAS) { intret(-1); return; }
    identstack stack;
    int n = -1;
    for(const char *s = list, *start, *end; parselist(s, start, end);)
    {
        ++n;
        char *val = newstring(start, end-start);
        setiter(*id, val, stack);
        if(executebool(body)) { intret(n); goto found; }
    }
    intret(-1);
found:
    if(n >= 0) poparg(*id);
}
COMMAND(0, listfind, "rse");

void looplist(ident *id, const char *list, const uint *body)
{
    if(id->type!=ID_ALIAS) return;
    identstack stack;
    int n = 0;
    for(const char *s = list, *start, *end; parselist(s, start, end); n++)
    {
        char *val = newstring(start, end-start);
        setiter(*id, val, stack);
        execute(body);
    }
    if(n) poparg(*id);
}
COMMAND(0, looplist, "rse");

void looplistconc(ident *id, const char *list, const uint *body, bool space)
{
    if(id->type!=ID_ALIAS) return;
    identstack stack;
    vector<char> r;
    int n = 0;
    for(const char *s = list, *start, *end; parselist(s, start, end); n++)
    {
        char *val = newstring(start, end-start);
        setiter(*id, val, stack);

        if(n && space) r.add(' ');

        tagval v;
        executeret(body, v);
        const char *vstr = v.getstr();
        int len = strlen(vstr);
        r.put(vstr, len);
        freearg(v);
    }
    if(n) poparg(*id);
    r.add('\0');
    commandret->setstr(newstring(r.getbuf(), r.length()-1));
}
ICOMMAND(0, looplistconcat, "rse", (ident *id, char *list, uint *body), looplistconc(id, list, body, true));
ICOMMAND(0, looplistconcatword, "rse", (ident *id, char *list, uint *body), looplistconc(id, list, body, false));

void listfilter(ident *id, const char *list, const uint *body)
{
    if(id->type!=ID_ALIAS) return;
    identstack stack;
    vector<char> r;
    int n = 0;
    for(const char *s = list, *start, *end, *quotestart, *quoteend; parselist(s, start, end, quotestart, quoteend); n++)
    {
        char *val = newstring(start, end-start);
        setiter(*id, val, stack);

        if(executebool(body))
        {
            if(r.length()) r.add(' ');
            r.put(quotestart, quoteend-quotestart);
        }
    }
    if(n) poparg(*id);
    r.add('\0');
    commandret->setstr(newstring(r.getbuf(), r.length()-1));
}
COMMAND(0, listfilter, "rse");

void prettylist(const char *s, const char *conj)
{
    vector<char> p;
    const char *start, *end;
    for(int len = listlen(s), n = 0; parselist(s, start, end); n++)
    {
        p.put(start, end - start);
        if(n+1 < len)
        {
            if(len > 2 || !conj[0]) p.add(',');
            if(n+2 == len && conj[0])
            {
                p.add(' ');
                p.put(conj, strlen(conj));
            }
            p.add(' ');
        }
    }
    p.add('\0');
    result(p.getbuf());
}
COMMAND(0, prettylist, "ss");

int listincludes(const char *list, const char *needle, int needlelen)
{
    int offset = 0;
    for(const char *s = list, *start, *end; parselist(s, start, end);)
    {
        int len = end - start;
        if(needlelen == len && !strncmp(needle, start, len)) return offset;
        offset++;
    }
    return -1;
}
ICOMMAND(0, indexof, "ss", (char *list, char *elem), intret(listincludes(list, elem, strlen(elem))));

char *listdel(const char *s, const char *del)
{
    vector<char> p;
    for(const char *start, *end, *qstart, *qend; parselist(s, start, end, qstart, qend);)
    {
        if(listincludes(del, start, end-start) < 0)
        {
            if(!p.empty()) p.add(' ');
            p.put(qstart, qend-qstart);
        }
    }
    p.add('\0');
    return newstring(p.getbuf(), p.length()-1);
}
ICOMMAND(0, listdel, "ss", (char *list, char *del), commandret->setstr(listdel(list, del)));

char *shrinklist(const char *list, const char *limit, int failover, bool invert)
{
    vector<char> p;
    for(const char *s = list, *start, *end, *qstart, *qend; parselist(s, start, end, qstart, qend);)
    {
        int inlist = listincludes(limit, start, end-start);
        if(invert ? inlist < 0 : inlist >= 0)
        {
            if(!p.empty()) p.add(' ');
            p.put(qstart, qend-qstart);
        }
    }
    if(failover && p.empty())
    {
        const char *all = "";
        switch(failover)
        {
            case 2: all = limit; break;
            case 1: default: all = list; break;
        }
        return newstring(all);
    }
    p.add('\0');
    return newstring(p.getbuf(), p.length()-1);
}
ICOMMAND(0, shrinklist, "ssii", (char *s, char *t, int *n, int *v), commandret->setstr(shrinklist(s, t, *n, *v!=0)));

void listsplice(const char *s, const char *vals, int *skip, int *count)
{
    int offset = max(*skip, 0), len = max(*count, 0);
    const char *list = s, *start, *end, *qstart, *qend = s;
    loopi(offset) if(!parselist(s, start, end, qstart, qend)) break;
    vector<char> p;
    if(qend > list) p.put(list, qend-list);
    if(*vals)
    {
        if(!p.empty()) p.add(' ');
        p.put(vals, strlen(vals));
    }
    loopi(len) if(!parselist(s)) break;
    skiplist(s);
    switch(*s)
    {
        case '\0': case ')': case ']': break;
        default:
            if(!p.empty()) p.add(' ');
            p.put(s, strlen(s));
            break;
    }
    p.add('\0');
    commandret->setstr(newstring(p.getbuf(), p.length()-1));
}
COMMAND(0, listsplice, "ssii");

ICOMMAND(0, listfiles, "ss", (char *dir, char *ext),
{
    vector<char *> files;
    listfiles(dir, ext[0] ? ext : NULL, files);
    vector<char> p;
    loopv(files)
    {
        if(i) p.put(' ');
        p.put(files[i], strlen(files[i]));
    }
    p.add('\0');
    commandret->setstr(newstring(p.getbuf(), p.length()-1));
});

ICOMMAND(0, loopfiles, "rsse", (ident *id, char *dir, char *ext, uint *body),
{
    if(id->type!=ID_ALIAS) return;
    identstack stack;
    vector<char *> files;
    listfiles(dir, ext[0] ? ext : NULL, files);
//    loopvrev(files)
//    {
//        char *file = files[i];
//        bool redundant = false;
//        loopj(i) if(!strcmp(files[j], file)) { redundant = true; break; }
//        if(redundant) delete[] files.removeunordered(i);
//    }
    loopv(files)
    {
        char *file = files[i];
        if(i)
        {
            if(id->valtype == VAL_STR) delete[] id->val.s;
            else id->valtype = VAL_STR;
            id->val.s = file;
        }
        else
        {
            tagval t;
            t.setstr(file);
            pusharg(*id, t, stack);
            id->flags &= ~IDF_UNKNOWN;
        }
        execute(body);
    }
    if(files.length()) poparg(*id);
});

ICOMMAND(0, findfile, "s", (char *name),
{
    string fname;
    copystring(fname, name);
    path(fname);
    intret(findzipfile(fname) || fileexists(fname, "e") || findfile(fname, "e") ? 1 : 0);
});

struct sortitem
{
    const char *str, *quotestart, *quoteend;
};

struct sortfun
{
    ident *x, *y;
    uint *body;

    bool operator()(const sortitem &xval, const sortitem &yval)
    {
        if(x->valtype != VAL_MACRO) x->valtype = VAL_MACRO;
        cleancode(*x);
        x->val.code = (const uint *)xval.str;
        if(y->valtype != VAL_MACRO) y->valtype = VAL_MACRO;
        cleancode(*y);
        y->val.code = (const uint *)yval.str;
        return executebool(body);
    }
};

void sortlist(char *list, ident *x, ident *y, uint *body)
{
    if(x == y || x->type != ID_ALIAS || y->type != ID_ALIAS) return;

    vector<sortitem> items;
    int macrolen = strlen(list), total = 0;
    char *macros = newstring(list, macrolen);
    const char *curlist = list, *start, *end, *quotestart, *quoteend;
    while(parselist(curlist, start, end, quotestart, quoteend))
    {
        macros[end - list] = '\0';
        sortitem item = { &macros[start - list], quotestart, quoteend };
        items.add(item);
        total += int(quoteend - quotestart);
    }

    identstack xstack, ystack;
    pusharg(*x, nullval, xstack); x->flags &= ~IDF_UNKNOWN;
    pusharg(*y, nullval, ystack); y->flags &= ~IDF_UNKNOWN;

    sortfun f = { x, y, body };
    items.sort(f);

    poparg(*x);
    poparg(*y);

    char *sorted = macros;
    int sortedlen = total + max(items.length() - 1, 0);
    if(macrolen < sortedlen)
    {
        delete[] macros;
        sorted = newstring(sortedlen);
    }

    int offset = 0;
    loopv(items)
    {
        sortitem &item = items[i];
        int len = int(item.quoteend - item.quotestart);
        if(i) sorted[offset++] = ' ';
        memcpy(&sorted[offset], item.quotestart, len);
        offset += len;
    }
    sorted[offset] = '\0';

    commandret->setstr(sorted);
}
COMMAND(0, sortlist, "srre");

ICOMMAND(0, +, "V", (tagval *args, int numargs),
{
    int val = numargs > 0 ? args[0].getint() : 0;
    loopi(numargs - 1) val += args[i + 1].getint();
    intret(val);
});
ICOMMAND(0, -, "V", (tagval *args, int numargs),
{
    int val = numargs > 0 ? args[0].getint() : 0;
    loopi(numargs - 1) val -= args[i + 1].getint();
    intret(val);
});
ICOMMAND(0, *, "V", (tagval *args, int numargs),
{
    int val = numargs > 0 ? args[0].getint() : 0;
    loopi(numargs - 1) val *= args[i + 1].getint();
    intret(val);
});
ICOMMAND(0, div, "V", (tagval *args, int numargs),
{
    int val = numargs > 0 ? args[0].getint() : 0;
    loopi(numargs - 1)
        if(args[i + 1].getint() != 0) val /= args[i + 1].getint();
        else val = 0;
    intret(val);
});
ICOMMAND(0, +f, "V", (tagval *args, int numargs),
{
    float val = numargs > 0 ? args[0].getfloat() : 0;
    loopi(numargs - 1) val += args[i + 1].getfloat();
    floatret(val);
});
ICOMMAND(0, -f, "V", (tagval *args, int numargs),
{
    float val = numargs > 0 ? args[0].getfloat() : 0;
    loopi(numargs - 1) val -= args[i + 1].getfloat();
    floatret(val);
});
ICOMMAND(0, *f, "V", (tagval *args, int numargs),
{
    float val = numargs > 0 ? args[0].getfloat() : 0;
    loopi(numargs - 1) val *= args[i + 1].getfloat();
    floatret(val);
});
ICOMMAND(0, divf, "V", (tagval *args, int numargs),
{
    float val = numargs > 0 ? args[0].getfloat() : 0;
    loopi(numargs - 1)
        if(args[i + 1].getfloat() != 0) val /= args[i + 1].getfloat();
        else val = 0;
    floatret(val);
});
ICOMMAND(0, =, "ii", (int *a, int *b), intret((int)(*a == *b)));
ICOMMAND(0, !=, "ii", (int *a, int *b), intret((int)(*a != *b)));
ICOMMAND(0, <, "ii", (int *a, int *b), intret((int)(*a < *b)));
ICOMMAND(0, >, "ii", (int *a, int *b), intret((int)(*a > *b)));
ICOMMAND(0, <=, "ii", (int *a, int *b), intret((int)(*a <= *b)));
ICOMMAND(0, >=, "ii", (int *a, int *b), intret((int)(*a >= *b)));
ICOMMAND(0, =f, "ff", (float *a, float *b), intret((int)(*a == *b)));
ICOMMAND(0, !=f, "ff", (float *a, float *b), intret((int)(*a != *b)));
ICOMMAND(0, <f, "ff", (float *a, float *b), intret((int)(*a < *b)));
ICOMMAND(0, >f, "ff", (float *a, float *b), intret((int)(*a > *b)));
ICOMMAND(0, <=f, "ff", (float *a, float *b), intret((int)(*a <= *b)));
ICOMMAND(0, >=f, "ff", (float *a, float *b), intret((int)(*a >= *b)));
ICOMMAND(0, ^, "ii", (int *a, int *b), intret(*a ^ *b));
ICOMMAND(0, !, "t", (tagval *a), intret(!getbool(*a)));
ICOMMAND(0, &, "ii", (int *a, int *b), intret(*a & *b));
ICOMMAND(0, |, "ii", (int *a, int *b), intret(*a | *b));
ICOMMAND(0, ~, "i", (int *a), intret(~*a));
ICOMMAND(0, ^~, "ii", (int *a, int *b), intret(*a ^ ~*b));
ICOMMAND(0, &~, "ii", (int *a, int *b), intret(*a & ~*b));
ICOMMAND(0, |~, "ii", (int *a, int *b), intret(*a | ~*b));
ICOMMAND(0, <<, "ii", (int *a, int *b), intret(*b < 32 ? *a << max(*b, 0) : 0));
ICOMMAND(0, >>, "ii", (int *a, int *b), intret(*a >> clamp(*b, 0, 31)));
ICOMMAND(0, &&, "e1V", (tagval *args, int numargs),
{
    if(!numargs) intret(1);
    else loopi(numargs)
    {
        if(i) freearg(*commandret);
        executeret(args[i].code, *commandret);
        if(!getbool(*commandret)) break;
    }
});
ICOMMAND(0, ||, "e1V", (tagval *args, int numargs),
{
    if(!numargs) intret(0);
    else loopi(numargs)
    {
        if(i) freearg(*commandret);
        executeret(args[i].code, *commandret);
        if(getbool(*commandret)) break;
    }
});

ICOMMAND(0, mod, "ii", (int *a, int *b), intret(*b ? *a % *b : 0));
ICOMMAND(0, modf, "ff", (float *a, float *b), floatret(*b ? fmod(*a, *b) : 0));
ICOMMAND(0, sin, "f", (float *a), floatret(sin(*a*RAD)));
ICOMMAND(0, cos, "f", (float *a), floatret(cos(*a*RAD)));
ICOMMAND(0, tan, "f", (float *a), floatret(tan(*a*RAD)));
ICOMMAND(0, asin, "f", (float *a), floatret(asin(*a)/RAD));
ICOMMAND(0, acos, "f", (float *a), floatret(acos(*a)/RAD));
ICOMMAND(0, atan, "f", (float *a), floatret(atan(*a)/RAD));
ICOMMAND(0, atan2, "ff", (float *y, float *x), floatret(atan2(*y, *x)/RAD));
ICOMMAND(0, sqrt, "f", (float *a), floatret(sqrt(*a)));
ICOMMAND(0, pow, "ff", (float *a, float *b), floatret(pow(*a, *b)));
ICOMMAND(0, loge, "f", (float *a), floatret(log(*a)));
ICOMMAND(0, log2, "f", (float *a), floatret(log(*a)/M_LN2));
ICOMMAND(0, log10, "f", (float *a), floatret(log10(*a)));
ICOMMAND(0, exp, "f", (float *a), floatret(exp(*a)));
ICOMMAND(0, min, "V", (tagval *args, int numargs),
{
    int val = numargs > 0 ? args[numargs - 1].getint() : 0;
    loopi(numargs - 1) val = min(val, args[i].getint());
    intret(val);
});
ICOMMAND(0, max, "V", (tagval *args, int numargs),
{
    int val = numargs > 0 ? args[numargs - 1].getint() : 0;
    loopi(numargs - 1) val = max(val, args[i].getint());
    intret(val);
});
ICOMMAND(0, minf, "V", (tagval *args, int numargs),
{
    float val = numargs > 0 ? args[numargs - 1].getfloat() : 0.0f;
    loopi(numargs - 1) val = min(val, args[i].getfloat());
    floatret(val);
});
ICOMMAND(0, maxf, "V", (tagval *args, int numargs),
{
    float val = numargs > 0 ? args[numargs - 1].getfloat() : 0.0f;
    loopi(numargs - 1) val = max(val, args[i].getfloat());
    floatret(val);
});
ICOMMAND(0, abs, "i", (int *n), intret(abs(*n)));
ICOMMAND(0, absf, "f", (float *n), floatret(fabs(*n)));
ICOMMAND(0, precf, "fi", (float *a, int *b),
{
    defformatstring(format, "%%.%df", max(*b, 0));
    defformatstring(retval, format, *a);
    result(retval);
});

#define LISTAVGCOMMAND(name, type) \
    ICOMMAND(0, name, "V", (tagval *args, int numargs), \
    { \
        type val = 0; \
        loopi(numargs) val += args[i].get##type(); \
        if(numargs > 0) val /= type(numargs); \
        type##ret(val); \
    });

LISTAVGCOMMAND(listavg, int);
LISTAVGCOMMAND(listavgf, float);

ICOMMAND(0, cond, "ee2V", (tagval *args, int numargs),
{
    for(int i = 0; i < numargs; i += 2)
    {
        if(i+1 < numargs)
        {
            if(executebool(args[i].code))
            {
                executeret(args[i+1].code, *commandret);
                break;
            }
        }
        else
        {
            executeret(args[i].code, *commandret);
            break;
        }
    }
});
#define CASECOMMAND(name, fmt, type, acc, compare) \
    ICOMMAND(0, name, fmt "te2V", (tagval *args, int numargs), \
    { \
        type val = acc; \
        int i; \
        for(i = 1; i+1 < numargs; i += 2) \
        { \
            if(compare) \
            { \
                executeret(args[i+1].code, *commandret); \
                return; \
            } \
        } \
    })
CASECOMMAND(case, "i", int, args[0].getint(), args[i].type == VAL_NULL || args[i].getint() == val);
CASECOMMAND(casef, "f", float, args[0].getfloat(), args[i].type == VAL_NULL || args[i].getfloat() == val);
CASECOMMAND(cases, "s", const char *, args[0].getstr(), args[i].type == VAL_NULL || !strcmp(args[i].getstr(), val));

ICOMMAND(0, rnd, "ii", (int *a, int *b), intret(*a - *b > 0 ? rnd(*a - *b) + *b : *b));
ICOMMAND(0, stringcmp, "ss", (char *a, char *b), intret(strcmp(a,b)==0));
ICOMMAND(0, =s, "ss", (char *a, char *b), intret(strcmp(a,b)==0));
ICOMMAND(0, !=s, "ss", (char *a, char *b), intret(strcmp(a,b)!=0));
ICOMMAND(0, <s, "ss", (char *a, char *b), intret(strcmp(a,b)<0));
ICOMMAND(0, >s, "ss", (char *a, char *b), intret(strcmp(a,b)>0));
ICOMMAND(0, <=s, "ss", (char *a, char *b), intret(strcmp(a,b)<=0));
ICOMMAND(0, >=s, "ss", (char *a, char *b), intret(strcmp(a,b)>=0));
ICOMMAND(0, echo, "C", (char *s), conoutft(CON_MESG, "%s", s));
ICOMMAND(0, error, "C", (char *s), conoutft(CON_DEBUG, "\fr%s", s));
ICOMMAND(0, stringlen, "s", (char *s), intret(strlen(s)));
ICOMMAND(0, stringcode, "si", (char *s, int *i), intret(*i > 0 ? (memchr(s, 0, *i) ? 0 : uchar(s[*i])) : uchar(s[0])));
ICOMMAND(0, codestring, "i", (int *i), { char *s = newstring(1); s[0] = char(*i); s[1] = '\0'; stringret(s); });
ICOMMAND(0, stringuni, "si", (char *s, int *i), intret(*i > 0 ? (memchr(s, 0, *i) ? 0 : cube2uni(s[*i])) : cube2uni(s[0])));
ICOMMAND(0, unistring, "i", (int *i), { char *s = newstring(1); s[0] = uni2cube(*i); s[1] = '\0'; stringret(s); });

// some dirty hacks to get around macro expansion issues
int rigstr(char *s, char *n)
{
    char *v = strstr(s, n);
    return v ? v-s : -1;
}
ICOMMAND(0, stringstr, "ss", (char *a, char *b), intret(rigstr(a, b)));

int rigcasecmp(char *s, char *n)
{
    return strcasecmp(s, n);
}
ICOMMAND(0, stringcasecmp, "ss", (char *a, char *b), intret(rigcasecmp(a,b)==0));

int rigncmp(char *s, char *n, int len)
{
    return strncmp(s, n, len);
}
ICOMMAND(0, stringncmp, "ssi", (char *a, char *b, int *n), intret(rigncmp(a,b,*n)==0));

int rigncasecmp(char *s, char *n, int len)
{
    return strncasecmp(s, n, len);
}
ICOMMAND(0, stringncasecmp, "ssi", (char *a, char *b, int *n), intret(rigncasecmp(a,b,*n)==0));

#define STRMAPCOMMAND(name, map) \
    ICOMMAND(0, name, "s", (char *s), \
    { \
        int len = strlen(s); \
        char *m = newstring(len); \
        loopi(len) m[i] = map(s[i]); \
        m[len] = '\0'; \
        stringret(m); \
    })

STRMAPCOMMAND(stringlower, cubelower);
STRMAPCOMMAND(stringupper, cubeupper);

char *rigcasestr(const char *s, const char *n)
{
    bool passed = true;
    char *start = newstring(s), *needle = newstring(n), *a = start, *b = needle, *ret = NULL;
    while(*a && *b)
    {
        *a = cubelower(*a);
        *b = cubelower(*b);
        if(passed && *a != *b) passed = false;
        a++;
        b++;
    }
    if(!*b)
    {
        if(passed) ret = (char *)s;
        else
        {
            while(*a)
            {
                *a = cubelower(*a);
                a++;
            }
            char *p = strstr(start, needle);
            if(p) ret = (char *)(s+(p-start));
        }
    }
    delete[] start;
    delete[] needle;
    return ret;
}

ICOMMAND(0, stringcasestr, "ss", (char *a, char *b), { char *s = rigcasestr(a, b); intret(s ? s-a : -1); });

char *strreplace(const char *s, const char *oldval, const char *newval)
{
    vector<char> buf;

    int oldlen = strlen(oldval);
    if(!oldlen) return newstring(s);
    for(;;)
    {
        const char *found = strstr(s, oldval);
        if(found)
        {
            while(s < found) buf.add(*s++);
            for(const char *n = newval; *n; n++) buf.add(*n);
            s = found + oldlen;
        }
        else
        {
            while(*s) buf.add(*s++);
            buf.add('\0');
            return newstring(buf.getbuf(), buf.length());
        }
    }
}

ICOMMAND(0, stringreplace, "sss", (char *s, char *o, char *n), commandret->setstr(strreplace(s, o, n)));

void stringsplice(const char *s, const char *vals, int *skip, int *count)
{
    int slen = strlen(s), vlen = strlen(vals),
        offset = clamp(*skip, 0, slen),
        len = clamp(*count, 0, slen - offset);
    char *p = newstring(slen - len + vlen);
    if(offset) memcpy(p, s, offset);
    if(vlen) memcpy(&p[offset], vals, vlen);
    if(offset + len < slen) memcpy(&p[offset + vlen], &s[offset + len], slen - (offset + len));
    p[slen - len + vlen] = '\0';
    commandret->setstr(p);
}
COMMAND(0, stringsplice, "ssii");

struct sleepcmd
{
    int delay, millis;
    char *command;
    int flags;
};
vector<sleepcmd> sleepcmds;

void addsleep(int *msec, char *cmd)
{
    sleepcmd &s = sleepcmds.add();
    s.delay = max(*msec, 1);
    s.millis = lastmillis;
    s.command = newstring(cmd);
    s.flags = identflags;
}

ICOMMAND(0, sleep, "is", (int *a, char *b), addsleep(a, b));

void checksleep(int millis)
{
    loopv(sleepcmds)
    {
        sleepcmd &s = sleepcmds[i];
        if(millis - s.millis >= s.delay)
        {
            int oldflags = identflags;
            identflags = s.flags;
            char *cmd = s.command; // execute might create more sleep commands
            s.command = NULL;
            execute(cmd);
            delete[] cmd;
            identflags = oldflags;
            if(sleepcmds.inrange(i) && !sleepcmds[i].command) sleepcmds.remove(i--);
        }
    }
}

void clearsleep(bool clearworlds)
{
    int len = 0;
    loopv(sleepcmds) if(sleepcmds[i].command)
    {
        if(!clearworlds || sleepcmds[i].flags&IDF_WORLD)
            delete[] sleepcmds[i].command;
        else sleepcmds[len++] = sleepcmds[i];
    }
    sleepcmds.shrink(len);
}

ICOMMAND(0, clearsleep, "i", (int *worlds), clearsleep(*worlds!=0 || identflags&IDF_WORLD));
ICOMMAND(0, exists, "ss", (char *a, char *b), intret(fileexists(a, *b ? b : "r")));
ICOMMAND(0, getmillis, "i", (int *total), intret(*total ? totalmillis : lastmillis));

void getvariable(int num)
{
    static vector<ident *> ids;
    static int lastupdate = 0;
    if(ids.empty() || !lastupdate || totalmillis-lastupdate >= 60000)
    {
        ids.setsize(0);
        enumerate(idents, ident, id, ids.add(&id));
        lastupdate = totalmillis;
        ids.sortname();
    }
    string text;
    num--;
    if(ids.inrange(num)) formatstring(text, "%s", ids[num]->name);
    else formatstring(text, "%d", ids.length());
    result(text);
}
ICOMMAND(0, getvariable, "i", (int *n), getvariable(*n));

void getvarinfo(int n, int types, int notypes, int flags, int noflags, char *str)
{
    static vector<ident *> ids[2];
    static int lastupdate = 0, lasttypes = 0, lastnotypes = 0, lastflags = 0, lastnoflags = 0, curids = 0;
    if(ids[0].empty() || !lastupdate || types != lasttypes || notypes != lastnotypes || flags != lastflags || noflags != lastnoflags || totalmillis-lastupdate >= 60000)
    {
        loopi(2) ids[i].setsize(0);
        enumerate(idents, ident, id, if((!types || (1<<id.type)&types) && (!notypes || !((1<<id.type)&notypes)) && (!flags || id.flags&flags) && (!noflags || !(id.flags&noflags))) ids[0].add(&id));
        lastupdate = totalmillis;
        lasttypes = types;
        lastnotypes = notypes;
        lastflags = flags;
        lastnoflags = noflags;
        ids[0].sortname();
    }
    if(str && *str)
    {
        static char *laststr = NULL;
        if(ids[1].empty() || !laststr || strcmp(str, laststr))
        {
            ids[1].setsize(0);
            loopv(ids[0]) if(rigcasestr(ids[0][i]->name, str)) ids[1].add(ids[0][i]);
            if(laststr) DELETEA(laststr);
            laststr = newstring(str);
        }
        curids = 1;
    }
    else curids = 0;
    if(n < 0) intret(ids[curids].length());
    else if(ids[curids].inrange(n)) result(ids[curids][n]->name);
}

ICOMMAND(0, getvarinfo, "biiiis", (int *n, int *w, int *x, int *t, int *o, char *s), getvarinfo(*n, *w, *x, *t, *o, s));

void hexcolour(int *n)
{
    defformatstring(s, *n >= 0 && *n <= 0xFFFFFF ? "0x%.6X" : "%d", *n);
    result(s);
}

COMMAND(0, hexcolour, "i");

void genkey(char *s)
{
    vector<char> privkey, pubkey;
    genprivkey(s, privkey, pubkey);
    defformatstring(keybuf, "%s %s", privkey.getbuf(), pubkey.getbuf());
    result(keybuf);
}
COMMAND(0, genkey, "s");

bool hasflag(const char *flags, char f)
{
    for(const char *c = flags; *c; c++)
        if(*c == f) return true;
    return false;
}
ICOMMAND(0, hasflag, "ss", (char *s, char *f), intret(*s && *f && hasflag(s, *f) ? 1 : 0));
