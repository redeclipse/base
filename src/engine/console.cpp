// console.cpp: the console buffer, its display, and control

#include "engine.h"

reversequeue<cline, MAXCONLINES> conlines[CON_MAX];

bigstring consolebuf;
int consolemillis = 0, consolepos = -1;
char *consoleaction = NULL, *consoleprompt = NULL;
bool consolerun = false;
SVAR(0, consolestr, "");

VAR(IDF_PERSIST, consolestay, 0, 1, 1);
VAR(IDF_PERSIST, consoleecho, 0, 1, 1);
VAR(IDF_PERSIST, consolevars, 0, 1, 2);

void conline(int color, const char *sf, int type)
{
    if(type < 0 || type >= CON_MAX) type = CON_DEBUG;
    char *buf = conlines[type].length() >= MAXCONLINES ? conlines[type].remove().cref : newstring("", BIGSTRLEN-1);
    cline &cl = conlines[type].add();
    cl.cref = buf;
    cl.color = color;
    cl.reftime = cl.outtime = totalmillis;
    cl.realtime = clocktime;
    copystring(cl.cref, sf, BIGSTRLEN);
}

#define LOOPCONLINES(type,str,name,op) \
    ICOMMAND(0, loopcon##str##name, "iire", (int *count, int *skip, ident *id, uint *body), \
    { \
        if(conlines[type].empty()) return; \
        loopstart(id, stack); \
        op(conlines[type], *count, *skip, \
        { \
            loopiter(id, stack, i); \
            execute(body); \
        }); \
        loopend(id, stack); \
    }); \
    ICOMMAND(0, loopcon##str##name##if, "iiree", (int *count, int *skip, ident *id, uint *cond, uint *body), \
    { \
        if(conlines[type].empty()) return; \
        loopstart(id, stack); \
        op(conlines[type], *count, *skip, \
        { \
            loopiter(id, stack, i); \
            if(executebool(cond)) execute(body); \
        }); \
        loopend(id, stack); \
    }); \
    ICOMMAND(0, loopcon##str##name##while, "iiree", (int *count, int *skip, ident *id, uint *cond, uint *body), \
    { \
        if(conlines[type].empty()) return; \
        loopstart(id, stack); \
        op(conlines[type], *count, *skip, \
        { \
            loopiter(id, stack, i); \
            if(!executebool(cond)) break; \
            execute(body); \
        }); \
        loopend(id, stack); \
    });

LOOPCONLINES(CON_DEBUG, debug,,loopcsv);
LOOPCONLINES(CON_DEBUG, debug,rev,loopcsvrev);
LOOPCONLINES(CON_EVENT, event,,loopcsv);
LOOPCONLINES(CON_EVENT, event,rev,loopcsvrev);

#define CONLINEVARS(type,str) \
    ICOMMANDV(0, con##str##lines, conlines[type].length()); \
    ICOMMAND(0, getcon##str##cref, "b", (int *n), result(conlines[type].inrange(*n) ? conlines[type][*n].cref : "")); \
    ICOMMAND(0, getcon##str##colour, "b", (int *n), intret(conlines[type].inrange(*n) ? conlines[type][*n].color : colourwhite)); \
    ICOMMAND(0, getcon##str##reftime, "b", (int *n), intret(conlines[type].inrange(*n) ? conlines[type][*n].reftime : 0)); \
    ICOMMAND(0, getcon##str##outtime, "b", (int *n), intret(conlines[type].inrange(*n) ? conlines[type][*n].outtime : 0)); \
    ICOMMAND(0, getcon##str##realtime, "b", (int *n), intret(conlines[type].inrange(*n) ? conlines[type][*n].realtime : 0));

CONLINEVARS(CON_DEBUG, debug);
CONLINEVARS(CON_EVENT, event);

// keymap is defined externally in keymap.cfg
struct keym
{
    enum
    {
        ACTION_DEFAULT = 0,
        ACTION_SPECTATOR,
        ACTION_WAITING,
        NUM_GAME_ACTIONS,

        ACTION_EDITING = NUM_GAME_ACTIONS,
        NUM_ACTIONS
    };

    enum
    {
        ACTION_MOD_CTRL = 0,
        ACTION_MOD_ALT,
        ACTION_MOD_SHIFT,

        ACTION_MODS,

        NUM_EDIT_ACTIONS = ACTION_MODS * ACTION_MODS
    };

    int code;
    char *name;
    char *gameactions[NUM_GAME_ACTIONS];
    char *editactions[NUM_EDIT_ACTIONS];
    bool pressed, gamepersist[NUM_GAME_ACTIONS], editpersist[NUM_EDIT_ACTIONS];

    keym() : code(-1), name(NULL), pressed(false)
    {
        loopi(NUM_GAME_ACTIONS)
        {
            gameactions[i] = newstring("");
            gamepersist[i] = false;
        }

        loopi(NUM_EDIT_ACTIONS)
        {
            editactions[i] = newstring("");
            editpersist[i] = false;
        }
    }
    ~keym()
    {
        DELETEA(name);
        loopi(NUM_GAME_ACTIONS)
        {
            DELETEA(gameactions[i]);
            gamepersist[i] = false;
        }

        loopi(NUM_EDIT_ACTIONS)
        {
            DELETEA(editactions[i]);
            editpersist[i] = false;
        }
    }

    bool &getpersist(int type, int modifiers = 0)
    {
        if(type == ACTION_EDITING) return editpersist[modifiers];
        else return gamepersist[type];
    }

    char *&getbinding(int type, int modifiers = 0)
    {
        if(type == ACTION_EDITING) return editactions[modifiers];
        else return gameactions[type];
    }

    char *&getbestbinding(int type, int modifiers = 0, bool force = false)
    {
        char **act = &getbinding(type, modifiers);

        if((!*act || !**act) && !force)
        {
            act = &getbinding(type);
            if(!*act || !**act) act = &getbinding(ACTION_DEFAULT);
        }

        return *act;
    }

    void clear(int type);
    void clear() { loopi(NUM_ACTIONS) clear(i); }
};

hashtable<int, keym> keyms(128);

void keymap(int *code, char *key)
{
    if(identflags&IDF_MAP) { conoutf(colourred, "Cannot override keymap"); return; }
    keym &km = keyms[*code];
    km.code = *code;
    DELETEA(km.name);
    km.name = newstring(key);
}

COMMAND(0, keymap, "is");

keym *keypressed = NULL;
char *keyaction = NULL;

const char *getkeyname(int code)
{
    keym *km = keyms.access(code);
    return km ? km->name : NULL;
}

void searchbindlist(const char *action, int type, int modifiers, int limit, const char *s1, const char *s2, const char *sep1, const char *sep2, vector<char> &names, bool force)
{
    const char *name1 = NULL, *name2 = NULL, *lastname = NULL;
    int found = 0;
    enumerate(keyms, keym, km,
    {
        char *act = km.getbestbinding(type, modifiers, force);

        if(act && !strcmp(act, action))
        {
            if(!name1) name1 = km.name;
            else if(!name2) name2 = km.name;
            else
            {
                if(lastname)
                {
                    if(sep1 && *sep1) names.put(sep1, strlen(sep1));
                    if(s1 && *s1) names.put(s1, strlen(s1));
                    names.put(lastname, strlen(lastname));
                    if(s2 && *s2) names.put(s2, strlen(s2));
                }
                else
                {
                    if(s1 && *s1) names.put(s1, strlen(s1));
                    names.put(name1, strlen(name1));
                    if(s2 && *s2) names.put(s2, strlen(s2));
                    if(sep1 && *sep1) names.put(sep1, strlen(sep1));
                    if(s1 && *s1) names.put(s1, strlen(s1));
                    names.put(name2, strlen(name2));
                    if(s2 && *s2) names.put(s2, strlen(s2));
                }
                lastname = km.name;
            }
            ++found;
            if(limit > 0 && found >= limit) break;
        }
    });
    if(lastname)
    {
        if(sep2 && *sep2) names.put(sep2, strlen(sep2));
        else if(sep1 && *sep1) names.put(sep1, strlen(sep1));
        if(s1 && *s1) names.put(s1, strlen(s1));
        names.put(lastname, strlen(lastname));
        if(s2 && *s2) names.put(s2, strlen(s2));
    }
    else
    {
        if(name1)
        {
            if(s1 && *s1) names.put(s1, strlen(s1));
            names.put(name1, strlen(name1));
            if(s2 && *s2) names.put(s2, strlen(s2));
        }
        if(name2)
        {
            if(sep2 && *sep2) names.put(sep2, strlen(sep2));
            else if(sep1 && *sep1) names.put(sep1, strlen(sep1));
            if(s1 && *s1) names.put(s1, strlen(s1));
            names.put(name2, strlen(name2));
            if(s2 && *s2) names.put(s2, strlen(s2));
        }
    }
    names.add('\0');
}

const char *searchbind(const char *action, int type, int modifiers)
{
    enumerate(keyms, keym, km,
    {
        char *act = km.getbestbinding(type, modifiers);
        if(!strcmp(act, action)) return km.name;
    });
    return NULL;
}

void getkeypressed(int limit, const char *s1, const char *s2, const char *sep1, const char *sep2, vector<char> &names)
{
    const char *name1 = NULL, *name2 = NULL, *lastname = NULL;
    int found = 0;
    enumerate(keyms, keym, km,
    {
        if(km.pressed)
        {
            if(!name1) name1 = km.name;
            else if(!name2) name2 = km.name;
            else
            {
                if(lastname)
                {
                    if(sep1 && *sep1) names.put(sep1, strlen(sep1));
                    if(s1 && *s1) names.put(s1, strlen(s1));
                    names.put(lastname, strlen(lastname));
                    if(s2 && *s2) names.put(s2, strlen(s2));
                }
                else
                {
                    if(s1 && *s1) names.put(s1, strlen(s1));
                    names.put(name1, strlen(name1));
                    if(s2 && *s2) names.put(s2, strlen(s2));
                    if(sep1 && *sep1) names.put(sep1, strlen(sep1));
                    if(s1 && *s1) names.put(s1, strlen(s1));
                    names.put(name2, strlen(name2));
                    if(s2 && *s2) names.put(s2, strlen(s2));
                }
                lastname = km.name;
            }
            ++found;
            if(limit > 0 && found >= limit) break;
        }
    });
    if(lastname)
    {
        if(sep2 && *sep2) names.put(sep2, strlen(sep2));
        else if(sep1 && *sep1) names.put(sep1, strlen(sep1));
        if(s1 && *s1) names.put(s1, strlen(s1));
        names.put(lastname, strlen(lastname));
        if(s2 && *s2) names.put(s2, strlen(s2));
    }
    else
    {
        if(name1)
        {
            if(s1 && *s1) names.put(s1, strlen(s1));
            names.put(name1, strlen(name1));
            if(s2 && *s2) names.put(s2, strlen(s2));
        }
        if(name2)
        {
            if(sep2 && *sep2) names.put(sep2, strlen(sep2));
            else if(sep1 && *sep1) names.put(sep1, strlen(sep1));
            if(s1 && *s1) names.put(s1, strlen(s1));
            names.put(name2, strlen(name2));
            if(s2 && *s2) names.put(s2, strlen(s2));
        }
    }
    names.add('\0');
}

int findkeycode(char *key)
{
    enumerate(keyms, keym, km,
    {
        if(!strcasecmp(km.name, key)) return i;
    });
    return 0;
}

keym *findbind(char *key)
{
    enumerate(keyms, keym, km,
    {
        if(!strcasecmp(km.name, key)) return &km;
    });
    return NULL;
}

void getbind(char *key, int type, int modifiers = 0)
{
    keym *km = findbind(key);
    result(km ? (km->getbestbinding(type, modifiers, true)) : "");
}

int changedkeys = 0;

void bindkey(char *key, char *action, int state, const char *cmd, int modifiers = 0)
{
    if(identflags&IDF_MAP) { conoutf(colourred, "Cannot override %s \"%s\"", cmd, key); return; }
    keym *km = findbind(key);
    if(!km) { conoutf(colourred, "Unknown key \"%s\"", key); return; }
    char *&binding = km->getbinding(state, modifiers);
    bool &persist = km->getpersist(state, modifiers);
    if(!keypressed || keyaction!=binding) delete[] binding;
    // trim white-space to make searchbinds more reliable
    while(iscubespace(*action)) action++;
    int len = strlen(action);
    while(len>0 && iscubespace(action[len-1])) len--;
    binding = newstring(action, len);
    persist = initing != INIT_DEFAULTS;
    changedkeys = totalmillis;
}

ICOMMAND(0, bind,     "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_DEFAULT, "bind"));
ICOMMAND(0, specbind, "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_SPECTATOR, "specbind"));
ICOMMAND(0, editbind, "ssi", (char *key, char *action, int *modifiers), bindkey(key, action, keym::ACTION_EDITING, "editbind", *modifiers));
ICOMMAND(0, waitbind, "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_WAITING, "waitbind"));
ICOMMAND(0, getbind,     "s", (char *key), getbind(key, keym::ACTION_DEFAULT));
ICOMMAND(0, getspecbind, "s", (char *key), getbind(key, keym::ACTION_SPECTATOR));
ICOMMAND(0, geteditbind, "si", (char *key, int *modifiers), getbind(key, keym::ACTION_EDITING, *modifiers));
ICOMMAND(0, getwaitbind, "s", (char *key), getbind(key, keym::ACTION_WAITING));
ICOMMAND(0, searchbinds,     "sissssb", (char *action, int *limit, char *s1, char *s2, char *sep1, char *sep2, int *force), { vector<char> list; searchbindlist(action, keym::ACTION_DEFAULT, 0, max(*limit, 0), s1, s2, sep1, sep2, list, *force!=0); result(list.getbuf()); });
ICOMMAND(0, searchspecbinds, "sissssb", (char *action, int *limit, char *s1, char *s2, char *sep1, char *sep2, int *force), { vector<char> list; searchbindlist(action, keym::ACTION_SPECTATOR, 0, max(*limit, 0), s1, s2, sep1, sep2, list, *force!=0); result(list.getbuf()); });
ICOMMAND(0, searcheditbinds, "sissssbi", (char *action, int *limit, char *s1, char *s2, char *sep1, char *sep2, int *force, int *modifiers), { vector<char> list; searchbindlist(action, keym::ACTION_EDITING, *modifiers, max(*limit, 0), s1, s2, sep1, sep2, list, *force!=0); result(list.getbuf()); });
ICOMMAND(0, searchwaitbinds, "sissssb", (char *action, int *limit, char *s1, char *s2, char *sep1, char *sep2, int *force), { vector<char> list; searchbindlist(action, keym::ACTION_WAITING, 0, max(*limit, 0), s1, s2, sep1, sep2, list, *force!=0); result(list.getbuf()); });

void keym::clear(int type)
{
    char *&binding = getbinding(type);
    if(binding[0])
    {
        if(!keypressed || keyaction!=binding) delete[] binding;
        binding = newstring("");
    }
}

ICOMMAND(0, clearbinds, "", (), enumerate(keyms, keym, km, km.clear(keym::ACTION_DEFAULT)));
ICOMMAND(0, clearspecbinds, "", (), enumerate(keyms, keym, km, km.clear(keym::ACTION_SPECTATOR)));
ICOMMAND(0, cleareditbinds, "", (), enumerate(keyms, keym, km, km.clear(keym::ACTION_EDITING)));
ICOMMAND(0, clearwaitbinds, "", (), enumerate(keyms, keym, km, km.clear(keym::ACTION_WAITING)));
ICOMMAND(0, clearallbinds, "", (), enumerate(keyms, keym, km, km.clear()));

ICOMMAND(0, keyspressed, "issss", (int *limit, char *s1, char *s2, char *sep1, char *sep2), { vector<char> list; getkeypressed(max(*limit, 0), s1, s2, sep1, sep2, list); result(list.getbuf()); });

void closeconsole()
{
    if(consolemillis <= 0) return;
    consolemillis = -totalmillis;
    textinput(false, TI_CONSOLE);
    keyrepeat(false, KR_CONSOLE);
    copystring(consolebuf, "", BIGSTRLEN);
    DELETEA(consoleaction);
    DELETEA(consoleprompt);
    consolepos = -1;
}

void inputconsole(const char *init, const char *prompt = NULL, const char *action = NULL) // turns input to the console on or off
{
    if(consolemillis <= 0)
    {
        consolemillis = totalmillis ? totalmillis : 1;
        textinput(true, TI_CONSOLE);
        keyrepeat(true, KR_CONSOLE);
    }
    copystring(consolebuf, init ? init : "", BIGSTRLEN);
    DELETEA(consoleaction);
    DELETEA(consoleprompt);
    consolepos = -1;
    if(action && action[0]) consoleaction = newstring(action);
    if(prompt && prompt[0]) consoleprompt = newstring(prompt);
    for(int surf = 0; surf <= SURFACE_FOREGROUND && !UI::showui("console", surf); ++surf);
}

ICOMMAND(0, inputconsole, "sss", (char *init, char *prompt, char *action), inputconsole(init, prompt, action));
ICOMMAND(0, closeconsole, "", (), closeconsole());

ICOMMANDV(0, consolemillis, consolemillis);
ICOMMANDVS(0, consolebuf, consolemillis > 0 ? consolebuf : "");
ICOMMANDVS(0, consoleaction, consolemillis > 0 && consoleaction ? consoleaction : "");
ICOMMANDVS(0, consoleprompt, consolemillis > 0 && consoleprompt ? consoleprompt : "");
ICOMMANDV(0, consolepos, consolemillis > 0 ? (consolepos >= 0 ? consolepos : strlen(consolebuf)) : -1);

char *pastetext(char *buf, size_t len)
{
    if(!SDL_HasClipboardText()) return NULL;
    char *cb = SDL_GetClipboardText();
    if(!cb) return NULL;
    size_t cblen = strlen(cb);
    if(cblen)
    {
        size_t start = 0;
        if(buf) start = strlen(buf);
        else { len = len ? min(len, cblen+1) : cblen+1; buf = newstring(len); }
        size_t decoded = decodeutf8((uchar *)&buf[start], len-1-start, (const uchar *)cb, cblen);
        buf[start + decoded] = '\0';
    }
    SDL_free(cb);
    return buf;
}

struct hline
{
    char *buf, *action, *prompt;

    hline() : buf(NULL), action(NULL), prompt(NULL) {}
    ~hline()
    {
        DELETEA(buf);
        DELETEA(action);
        DELETEA(prompt);
    }

    void restore()
    {
        copystring(consolebuf, buf);
        if(consolepos >= (int)strlen(consolebuf)) consolepos = -1;
        DELETEA(consoleaction);
        DELETEA(consoleprompt);
        if(action) consoleaction = newstring(action);
        if(prompt) consoleprompt = newstring(prompt);
    }

    bool shouldsave()
    {
        return strcmp(consolebuf, buf) || (consoleaction ? !action || strcmp(consoleaction, action) : action!=NULL) || (consoleprompt ? !prompt || strcmp(consoleprompt, prompt) : prompt!=NULL);
    }

    void create(const char *b, const char *a, const char *p)
    {
        buf = newstring(b);
        if(a && *a) action = newstring(a);
        if(p && *p) prompt = newstring(p);
    }

    void save()
    {
        create(consolebuf, consoleaction, consoleprompt);
    }

    void run()
    {
        const char *str = action && *action ? action : buf;
        if(str && *str == '/') str++; // workaround for old style
        if(consoleecho) conoutf(colourwhite, "\fs\fa>\fS %s", str);

        bool oldconsolerun = consolerun;
        consolerun = true;
        setsvar("consolestr", buf);
        execute(str);
        setsvar("consolestr", "");
        consolerun = oldconsolerun;
    }
};
vector<hline *> history;
int histpos = 0;

VAR(IDF_PERSIST, maxhistory, 1, 1000, 10000);

void history_(int *n)
{
    static bool inhistory = false;
    if(!inhistory && history.inrange(*n))
    {
        inhistory = true;
        history[history.length()-*n-1]->run();
        inhistory = false;
    }
}

COMMANDN(0, history, history_, "i");

void addhistory(char *buf, char *action, char *prompt)
{
    if(!buf || !*buf) return;
    if(maxhistory && history.length() >= maxhistory)
    {
        loopi(history.length()-maxhistory+1) delete history[i];
        history.remove(0, history.length()-maxhistory+1);
    }
    history.add(new hline)->create(buf, action, prompt);
    histpos = history.length();
}

COMMAND(0, addhistory, "sss");

void writehistory()
{
    stream *f = openfile("history.cfg", "w");
    if(!f) return;
    f->printf("// console history written automatically by Red Eclipse\n\n");
    loopv(history)
    {
        hline *h = history[i];
        if(!h->buf) continue;
        f->printf("addhistory %s", escapestring(h->buf));
        if(h->action || h->prompt) f->printf("%s", h->action ? escapestring(h->action) : "[]");
        if(h->prompt) f->printf("%s", escapestring(h->prompt));
        f->printf("\n");
    }
    delete f;
}

struct releaseaction
{
    keym *key;
    union
    {
        char *action;
        ident *id;
    };
    int numargs;
    tagval args[3];
};
vector<releaseaction> releaseactions;

const char *addreleaseaction(char *s)
{
    if(!keypressed) { delete[] s; return NULL; }
    releaseaction &ra = releaseactions.add();
    ra.key = keypressed;
    ra.action = s;
    ra.numargs = -1;
    return keypressed->name;
}

tagval *addreleaseaction(ident *id, int numargs)
{
    if(!keypressed || numargs > 3) return NULL;
    releaseaction &ra = releaseactions.add();
    ra.key = keypressed;
    ra.id = id;
    ra.numargs = numargs;
    return ra.args;
}

void onrelease(const char *s)
{
    addreleaseaction(newstring(s));
}

COMMAND(0, onrelease, "s");

static inline bool iskeypressed(int code)
{
    keym *haskey = keyms.access(code);
    return haskey && haskey->pressed;
}

static int getkeymodifiers()
{
    int modifiers = 0;

    if(iskeypressed(SDLK_LCTRL) || iskeypressed(SDLK_RCTRL))
        modifiers |= BIT(keym::ACTION_MOD_CTRL);

    if(iskeypressed(SDLK_LALT) || iskeypressed(SDLK_RALT))
        modifiers |= BIT(keym::ACTION_MOD_ALT);

    if(iskeypressed(SDLK_LSHIFT) || iskeypressed(SDLK_RSHIFT))
        modifiers |= BIT(keym::ACTION_MOD_SHIFT);

    return modifiers;
}

void execbind(keym &k, bool isdown)
{
    loopv(releaseactions)
    {
        releaseaction &ra = releaseactions[i];
        if(ra.key==&k)
        {
            if(ra.numargs < 0)
            {
                if(!isdown) execute(ra.action);
                delete[] ra.action;
            }
            else execute(isdown ? NULL : ra.id, ra.args, ra.numargs);
            releaseactions.remove(i--);
        }
    }
    if(isdown)
    {

        int state = keym::ACTION_DEFAULT, modifiers = 0;
        switch(client::state())
        {
            case CS_ALIVE: case CS_DEAD: default: break;
            case CS_SPECTATOR: state = keym::ACTION_SPECTATOR; break;
            case CS_WAITING: state = keym::ACTION_WAITING; break;
            case CS_EDITING:
                state = keym::ACTION_EDITING;
                modifiers = getkeymodifiers();
                break;
        }
        char *&action = k.getbestbinding(state, modifiers);
        keyaction = action;
        keypressed = &k;
        execute(keyaction);
        keypressed = NULL;
        if(keyaction!=action) delete[] keyaction;
    }
    k.pressed = isdown;
}

bool consoleinput(const char *str, int len)
{
    if(consolemillis <= 0) return false;

    resetcomplete();
    int maxlen = int(sizeof(consolebuf)), cmdlen = (int)strlen(consolebuf), cmdspace = maxlen - (cmdlen+1);
    len = min(len, cmdspace);
    if(len <= 0) return true;

    if(consolepos<0)
    {
        memcpy(&consolebuf[cmdlen], str, len);
    }
    else
    {
        memmove(&consolebuf[consolepos+len], &consolebuf[consolepos], cmdlen - consolepos);
        memcpy(&consolebuf[consolepos], str, len);
        consolepos += len;
    }
    consolebuf[cmdlen + len] = '\0';

    return true;
}

static char *skipword(char *s)
{
    while(int c = *s++) if(!iscubespace(c))
    {
        while(int c = *s++) if(iscubespace(c)) break;
        break;
    }
    return s-1;
}

static char *skipwordrev(char *s, int n = -1)
{
    char *e = s + strlen(s);
    if(n >= 0) e = min(e, &s[n]);
    while(--e >= s) if(!iscubespace(*e))
    {
        while(--e >= s && !iscubespace(*e));
        break;
    }
    return e+1;
}

bool consolekey(int code, bool isdown)
{
    if(consolemillis <= 0 || code < 0) return false;

    if(isdown)
    {
        switch(code)
        {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                break;

            case SDLK_HOME:
                if(consolebuf[0]) consolepos = 0;
                break;

            case SDLK_END:
                consolepos = -1;
                break;

            case SDLK_DELETE:
            {
                int len = (int)strlen(consolebuf);
                if(consolepos<0) break;
                int end = consolepos+1;
                if(SDL_GetModState()&SKIP_KEYS) end = skipword(&consolebuf[consolepos]) - consolebuf;
                memmove(&consolebuf[consolepos], &consolebuf[end], len + 1 - end);
                resetcomplete();
                if(consolepos>=len-1) consolepos = -1;
                break;
            }

            case SDLK_BACKSPACE:
            {
                int len = (int)strlen(consolebuf), i = consolepos>=0 ? consolepos : len;
                if(i<1) break;
                int start = i-1;
                if(SDL_GetModState()&SKIP_KEYS) start = skipwordrev(consolebuf, i) - consolebuf;
                memmove(&consolebuf[start], &consolebuf[i], len - i + 1);
                resetcomplete();
                if(consolepos>0) consolepos = start;
                else if(!consolepos && len<=1) consolepos = -1;
                break;
            }

            case SDLK_LEFT:
                if(SDL_GetModState()&SKIP_KEYS) consolepos = skipwordrev(consolebuf, consolepos) - consolebuf;
                else if(consolepos>0) consolepos--;
                else if(consolepos<0) consolepos = (int)strlen(consolebuf)-1;
                break;

            case SDLK_RIGHT:
                if(consolepos>=0)
                {
                    if(SDL_GetModState()&SKIP_KEYS) consolepos = skipword(&consolebuf[consolepos]) - consolebuf;
                    else ++consolepos;
                    if(consolepos>=(int)strlen(consolebuf)) consolepos = -1;
                }
                break;

            case SDLK_UP:
                if(histpos > history.length()) histpos = history.length();
                if(histpos > 0)
                {
                    if(SDL_GetModState()&SKIP_KEYS) histpos = 0;
                    else --histpos;
                    history[histpos]->restore();
                }
                break;

            case SDLK_DOWN:
                if(histpos + 1 < history.length())
                {
                    if(SDL_GetModState()&SKIP_KEYS) histpos = history.length()-1;
                    else ++histpos;
                    history[histpos]->restore();
                }
                break;

            case SDLK_TAB:
                complete(consolebuf, SDL_GetModState()&KMOD_SHIFT);
                if(consolepos>=0 && consolepos>=(int)strlen(consolebuf)) consolepos = -1;
                break;

            case SDLK_v:
                if(SDL_GetModState()&MOD_KEYS)
                {
                    char *pastebuf = pastetext();
                    if(pastebuf)
                    {
                        consoleinput(pastebuf, strlen(pastebuf));
                        delete[] pastebuf;
                    }
                }
                break;
        }
    }
    else
    {
        if(code==SDLK_RETURN || code==SDLK_KP_ENTER)
        {
            hline *h = NULL;
            if(consolebuf[0])
            {
                if(history.empty() || history.last()->shouldsave())
                {
                    if(maxhistory && history.length() >= maxhistory)
                    {
                        loopi(history.length()-maxhistory+1) delete history[i];
                        history.remove(0, history.length()-maxhistory+1);
                    }
                    history.add(h = new hline)->save();
                }
                else h = history.last();
            }
            histpos = history.length();

            if(consolestay) inputconsole(NULL);
            else closeconsole();

            if(h) h->run();
        }
        else if(code==SDLK_ESCAPE || code < 0)
        {
            histpos = history.length();
            closeconsole();
        }
    }

    return true;
}

void processtextinput(const char *str, int len)
{
    if(!hud::textinput(str, len) && !UI::textinput(str, len))
        consoleinput(str, len);
}

#define keyintercept(name,body) \
{ \
    static bool key##name = false; \
    if(SDL_GetModState()&MOD_ALTS && isdown) \
    { \
        if(!key##name) \
        { \
            body; \
            key##name = true; \
        } \
        return; \
    } \
    if(!isdown && key##name) \
    { \
        key##name = false; \
        return; \
    } \
}
void processkey(int code, bool isdown)
{
    switch(code)
    {
        case SDLK_F4:
            keyintercept(quit, quit());
            break;
        case SDLK_RETURN:
            keyintercept(fullscreen, setfullscreen(!(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN)));
            break;
#ifdef WIN32
        case SDLK_TAB:
            keyintercept(iconify, SDL_MinimizeWindow(screen));
            break;
#endif
        case SDLK_CAPSLOCK:
            if(!isdown) capslockon = capslocked();
            break;
        case SDLK_NUMLOCKCLEAR:
            if(!isdown) numlockon = numlocked();
            break;
        default: break;
    }
    keym *haskey = keyms.access(code);
    if(haskey && haskey->pressed) execbind(*haskey, isdown); // allow pressed keys to release
    else if(!consolekey(code, isdown) && !hud::keypress(code, isdown) && !UI::keypress(code, isdown) && haskey) execbind(*haskey, isdown);
}

void clear_binds()
{
    keyms.clear();
}

ICOMMAND(0, clearconsole, "b", (int *n), if(*n < 0 || *n >= CON_MAX) { loopi(CON_MAX) conlines[i].clear(); } else { conlines[*n].clear(); });

static void writebind(stream *f, keym &km, int type, int modifiers = 0)
{
    static const char * const cmds[4] = { "bind", "specbind", "waitbind", "editbind" };
    char *act = km.getbinding(type, modifiers);

    if(!*act)
        f->printf("%s %s []", cmds[type], escapestring(km.name));
    else if(validateblock(act))
        f->printf("%s %s [%s]", cmds[type], escapestring(km.name), act);
    else
        f->printf("%s %s %s", cmds[type], escapestring(km.name), escapestring(act));

    if(type == keym::ACTION_EDITING && modifiers) f->printf(" %d", modifiers);
    f->printf("\n");
}

void writebinds(stream *f)
{
    vector<keym *> binds;
    enumerate(keyms, keym, km, binds.add(&km));
    binds.sortname();
    loopj(keym::NUM_ACTIONS)
    {
        bool found = false;
        loopv(binds)
        {
            keym &km = *binds[i];
            if(j == keym::ACTION_EDITING) loopk(keym::NUM_EDIT_ACTIONS)
            {
                if(km.getpersist(j, k))
                {
                    writebind(f, km, j, k);
                    found = true;
                }
            }
            else if (km.getpersist(j))
            {
                writebind(f, km, j);
                found = true;
            }
        }
        if(found) f->printf("\n");
    }
}

// tab-completion of all idents and base maps

enum { FILES_DIR = 0, FILES_LIST };

struct fileskey
{
    int type;
    const char *dir, *ext;

    fileskey() {}
    fileskey(int type, const char *dir, const char *ext) : type(type), dir(dir), ext(ext) {}
};

struct filesval
{
    int type;
    char *dir, *ext;
    vector<char *> files;
    int millis;

    filesval(int type, const char *dir, const char *ext) : type(type), dir(newstring(dir)), ext(ext && ext[0] ? newstring(ext) : NULL), millis(-1) {}
    ~filesval() { DELETEA(dir); DELETEA(ext); files.deletearrays(); }

    void update()
    {
        if(type!=FILES_DIR || millis >= consolemillis) return;
        files.deletearrays();
        listfiles(dir, ext, files);
        files.sort();
        loopv(files) if(i && !strcmp(files[i], files[i-1])) delete[] files.remove(i--);
        millis = totalmillis;
    }
};

static inline bool htcmp(const fileskey &x, const fileskey &y)
{
    return x.type==y.type && !strcmp(x.dir, y.dir) && (x.ext == y.ext || (x.ext && y.ext && !strcmp(x.ext, y.ext)));
}

static inline uint hthash(const fileskey &k)
{
    return hthash(k.dir);
}

static hashtable<fileskey, filesval *> completefiles;
static hashtable<char *, filesval *> filecompletions;
static hashtable<char *, bool> playercompletions;

int completeoffset = -1, completesize = 0;
bigstring completeescaped = "";
bigstring lastcomplete;

void resetcomplete()
{
    completesize = 0;
    completeescaped[0] = '\0';
}

void addcomplete(char *command, int type, char *dir, char *ext)
{
    if(identflags&IDF_MAP)
    {
        conoutf(colourred, "Cannot override complete %s", command);
        return;
    }
    if(!dir[0])
    {
        filesval **hasfiles = filecompletions.access(command);
        if(hasfiles) *hasfiles = NULL;
        return;
    }
    if(type==FILES_DIR)
    {
        int dirlen = (int)strlen(dir);
        while(dirlen > 0 && (dir[dirlen-1] == '/' || dir[dirlen-1] == '\\'))
            dir[--dirlen] = '\0';
        if(ext)
        {
            if(strchr(ext, '*')) ext[0] = '\0';
            if(!ext[0]) ext = NULL;
        }
    }
    fileskey key(type, dir, ext);
    filesval **val = completefiles.access(key);
    if(!val)
    {
        filesval *f = new filesval(type, dir, ext);
        if(type==FILES_LIST) explodelist(dir, f->files);
        val = &completefiles[fileskey(type, f->dir, f->ext)];
        *val = f;
    }
    filesval **hasfiles = filecompletions.access(command);
    if(hasfiles) *hasfiles = *val;
    else filecompletions[newstring(command)] = *val;
}

void addfilecomplete(char *command, char *dir, char *ext)
{
    addcomplete(command, FILES_DIR, dir, ext);
}

void addlistcomplete(char *command, char *list)
{
    addcomplete(command, FILES_LIST, list, NULL);
}

void addplayercomplete(char *command, int *remove)
{
    bool *current = playercompletions.access(command);
    if(current) *current = !*remove;
    else if(!*remove) playercompletions[newstring(command)] = true;
}

COMMANDN(0, complete, addfilecomplete, "sss");
COMMANDN(0, listcomplete, addlistcomplete, "ss");
COMMANDN(0, playercomplete, addplayercomplete, "si");

void complete(char *str, bool reverse)
{
    char *start = str;
    if(start && *start == '/') start++; // workaround for old style
    char *s = start;

    if(completeescaped[0]) copystring(s, completeescaped, BIGSTRLEN);

    const char chrlist[7] = { ';', '(', ')', '[', ']', '\"', '$', };
    bool variable = false;
    loopi(7)
    {
        char *semi = strrchr(start, chrlist[i]);
        if(semi)
        {
            start = semi+1;
            if(chrlist[i] == '$') variable = true;
        }
    }
    while(*start == ' ') start++;
    if(!start[0]) return;
    if(start-s != completeoffset || !completesize)
    {
        completeoffset = start-s;
        completesize = (int)strlen(start);
        lastcomplete[0] = '\0';
    }
    filesval *f = NULL;
    bool p = false;
    if(completesize)
    {
        char *end = strchr(start, ' ');
        if(end)
        {
            stringslice slice = stringslice(start, end);
            f = filecompletions.find(slice, NULL);
            p = playercompletions.find(slice, false);
            if(!p) enumerate(idents, ident, id,
                if((id.flags&IDF_NAMECOMPLETE) && !strncmp(id.name, start, end-start))
                {
                    p = true;
                    break;
                }
            );
        }
    }
    const char *nextcomplete = NULL;
    int prefixlen = start-s;
    if(f) // complete using filenames
    {
        int commandsize = strchr(start, ' ')+1-start;
        prefixlen += commandsize;
        f->update();
        loopv(f->files)
        {
            if(strncmp(f->files[i], &start[commandsize], completesize-commandsize)==0 &&
                strcmp(f->files[i], lastcomplete)*(reverse ? -1 : 1) > 0 && (!nextcomplete || strcmp(f->files[i], nextcomplete)*(reverse ? -1 : 1) < 0))
                    nextcomplete = f->files[i];
        }
    }
    else if(p) // complete using player names
    {
        int commandsize = strchr(start, ' ')+1-start;
        prefixlen += commandsize;
        client::completeplayers(&nextcomplete, start, commandsize, lastcomplete, reverse);
    }
    else // complete using command names
    {
        enumerate(idents, ident, id,
            if((variable ? id.type == ID_VAR || id.type == ID_SVAR || id.type == ID_FVAR || id.type == ID_ALIAS: id.flags&IDF_COMPLETE) && strncmp(id.name, start, completesize)==0 &&
                strcmp(id.name, lastcomplete)*(reverse ? -1 : 1) > 0 && (!nextcomplete || strcmp(id.name, nextcomplete)*(reverse ? -1 : 1) < 0))
                    nextcomplete = id.name;
        );
    }
    if(nextcomplete)
    {
        if(p) copystring(completeescaped, s);
        copystring(&s[prefixlen], p ? escapestring(nextcomplete) : nextcomplete, BIGSTRLEN-prefixlen);
        copystring(lastcomplete, nextcomplete, BIGSTRLEN);
    }
    else
    {
        if((int)strlen(start) > completesize) start[completesize] = '\0';
        completesize = 0;
    }
}

bool capslockon = false, numlockon = false;
#ifndef WIN32
#include <X11/XKBlib.h>
#endif
bool capslocked()
{
    #ifdef WIN32
    if(GetKeyState(VK_CAPITAL)) return true;
    #else
    Display *d = XOpenDisplay((char*)0);
    if(d)
    {
        uint n = 0;
        XkbGetIndicatorState(d, XkbUseCoreKbd, &n);
        XCloseDisplay(d);
        return (n&0x01)!=0;
    }
    #endif
    return false;
}
ICOMMANDV(0, getcapslock, capslockon ? 1 : 0);

bool numlocked()
{
    #ifdef WIN32
    if(GetKeyState(VK_NUMLOCK)) return true;
    #else
    Display *d = XOpenDisplay((char*)0);
    if(d)
    {
        uint n = 0;
        XkbGetIndicatorState(d, XkbUseCoreKbd, &n);
        XCloseDisplay(d);
        return (n&0x02)!=0;
    }
    #endif
    return false;
}
ICOMMANDV(0, getnumlock, numlockon ? 1 : 0);
