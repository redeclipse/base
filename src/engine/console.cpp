// console.cpp: the console buffer, its display, and command line control

#include "engine.h"

reversequeue<cline, MAXCONLINES> conlines;

int commandmillis = -1;
bigstring commandbuf;
char *commandaction = NULL, *commandicon = NULL;
enum { CF_COMPLETE = 1<<0, CF_EXECUTE = 1<<1, CF_MESSAGE = 1<<2 };
int commandflags = 0, commandpos = -1, commandcolour = 0;

void conline(int type, const char *sf, int n)
{
    char *buf = conlines.length() >= MAXCONLINES ? conlines.remove().cref : newstring("", BIGSTRLEN-1);
    cline &cl = conlines.add();
    cl.type = type;
    cl.cref = buf;
    cl.reftime = cl.outtime = totalmillis;
    cl.realtime = clocktime;

    if(n)
    {
        copystring(cl.cref, "  ", BIGSTRLEN);
        concatstring(cl.cref, sf, BIGSTRLEN);
        loopj(2)
        {
            int off = n+j;
            if(conlines.inrange(off))
            {
                if(j) concatstring(conlines[off].cref, "\fs", BIGSTRLEN);
                else prependstring(conlines[off].cref, "\fS", BIGSTRLEN);
            }
        }
    }
    else copystring(cl.cref, sf, BIGSTRLEN);
}

// keymap is defined externally in keymap.cfg
struct keym
{
    enum
    {
        ACTION_DEFAULT = 0,
        ACTION_SPECTATOR,
        ACTION_EDITING,
        ACTION_WAITING,
        NUMACTIONS
    };

    int code;
    char *name;
    char *actions[NUMACTIONS];
    bool pressed, persist[NUMACTIONS];

    keym() : code(-1), name(NULL), pressed(false) { loopi(NUMACTIONS) { actions[i] = newstring(""); persist[i] = false; } }
    ~keym() { DELETEA(name); loopi(NUMACTIONS) { DELETEA(actions[i]); persist[i] = false; } }
};

hashtable<int, keym> keyms(128);

void keymap(int *code, char *key)
{
    if(identflags&IDF_WORLD) { conoutf("\frCannot override keymap"); return; }
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

void searchbindlist(const char *action, int type, int limit, const char *s1, const char *s2, const char *sep1, const char *sep2, vector<char> &names, bool force)
{
    const char *name1 = NULL, *name2 = NULL, *lastname = NULL;
    int found = 0;
    enumerate(keyms, keym, km,
    {
        char *act = type && force && (!km.actions[type] || !*km.actions[type]) ? km.actions[keym::ACTION_DEFAULT] : km.actions[type];
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

const char *searchbind(const char *action, int type)
{
    enumerate(keyms, keym, km,
    {
        char *act = ((!km.actions[type] || !*km.actions[type]) && type ? km.actions[keym::ACTION_DEFAULT] : km.actions[type]);
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

void getbind(char *key, int type)
{
    keym *km = findbind(key);
    result(km ? ((!km->actions[type] || !*km->actions[type]) && type ? km->actions[keym::ACTION_DEFAULT] : km->actions[type]) : "");
}

int changedkeys = 0;

void bindkey(char *key, char *action, int state, const char *cmd)
{
    if(identflags&IDF_WORLD) { conoutf("\frCannot override %s \"%s\"", cmd, key); return; }
    keym *km = findbind(key);
    if(!km) { conoutf("\frUnknown key \"%s\"", key); return; }
    char *&binding = km->actions[state];
    bool *persist = &km->persist[state];
    if(!keypressed || keyaction!=binding) delete[] binding;
    // trim white-space to make searchbinds more reliable
    while(iscubespace(*action)) action++;
    int len = strlen(action);
    while(len>0 && iscubespace(action[len-1])) len--;
    binding = newstring(action, len);
    *persist = initing != INIT_DEFAULTS;
    changedkeys = totalmillis;
}

ICOMMAND(0, bind,     "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_DEFAULT, "bind"));
ICOMMAND(0, specbind, "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_SPECTATOR, "specbind"));
ICOMMAND(0, editbind, "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_EDITING, "editbind"));
ICOMMAND(0, waitbind, "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_WAITING, "waitbind"));
ICOMMAND(0, getbind,     "s", (char *key), getbind(key, keym::ACTION_DEFAULT));
ICOMMAND(0, getspecbind, "s", (char *key), getbind(key, keym::ACTION_SPECTATOR));
ICOMMAND(0, geteditbind, "s", (char *key), getbind(key, keym::ACTION_EDITING));
ICOMMAND(0, getwaitbind, "s", (char *key), getbind(key, keym::ACTION_WAITING));
ICOMMAND(0, searchbinds,     "sissssb", (char *action, int *limit, char *s1, char *s2, char *sep1, char *sep2, int *force), { vector<char> list; searchbindlist(action, keym::ACTION_DEFAULT, max(*limit, 0), s1, s2, sep1, sep2, list, *force!=0); result(list.getbuf()); });
ICOMMAND(0, searchspecbinds, "sissssb", (char *action, int *limit, char *s1, char *s2, char *sep1, char *sep2, int *force), { vector<char> list; searchbindlist(action, keym::ACTION_SPECTATOR, max(*limit, 0), s1, s2, sep1, sep2, list, *force!=0); result(list.getbuf()); });
ICOMMAND(0, searcheditbinds, "sissssb", (char *action, int *limit, char *s1, char *s2, char *sep1, char *sep2, int *force), { vector<char> list; searchbindlist(action, keym::ACTION_EDITING, max(*limit, 0), s1, s2, sep1, sep2, list, *force!=0); result(list.getbuf()); });
ICOMMAND(0, searchwaitbinds, "sissssb", (char *action, int *limit, char *s1, char *s2, char *sep1, char *sep2, int *force), { vector<char> list; searchbindlist(action, keym::ACTION_WAITING, max(*limit, 0), s1, s2, sep1, sep2, list, *force!=0); result(list.getbuf()); });

ICOMMAND(0, keyspressed, "issss", (int *limit, char *s1, char *s2, char *sep1, char *sep2), { vector<char> list; getkeypressed(max(*limit, 0), s1, s2, sep1, sep2, list); result(list.getbuf()); });

void inputcommand(char *init, char *action = NULL, char *icon = NULL, int colour = 0, char *flags = NULL) // turns input to the command line on or off
{
    commandmillis = init ? totalmillis : -totalmillis;
    textinput(commandmillis >= 0, TI_CONSOLE);
    keyrepeat(commandmillis >= 0, KR_CONSOLE);
    copystring(commandbuf, init ? init : "", BIGSTRLEN);
    DELETEA(commandaction);
    DELETEA(commandicon);
    commandpos = -1;
    if(action && action[0]) commandaction = newstring(action);
    if(icon && icon[0]) commandicon = newstring(icon);
    commandcolour = colour;
    commandflags = 0;
    if(flags) while(*flags) switch(*flags++)
    {
        case 'c': commandflags |= CF_COMPLETE; break;
        case 'x': commandflags |= CF_EXECUTE; break;
        case 'm': commandflags |= CF_MESSAGE; break;
        case 's': commandflags |= CF_COMPLETE|CF_EXECUTE|CF_MESSAGE; break;
    }
    else if(init) commandflags |= CF_COMPLETE|CF_EXECUTE;
}

ICOMMAND(0, saycommand, "C", (char *init), inputcommand(init));
ICOMMAND(0, inputcommand, "sssis", (char *init, char *action, char *icon, int *colour, char *flags), inputcommand(init, action, icon, *colour, flags));

char *pastetext()
{
    if(!SDL_HasClipboardText()) return NULL;
    char *cb = SDL_GetClipboardText();
    if(!cb) return NULL;
    char *buf = NULL;
    if(*cb)
    {
        size_t cblen = strlen(cb);
        char *pbuf = newstring(cb, cblen);
        size_t decoded = decodeutf8((uchar *)&pbuf[0], cblen, (const uchar *)cb, cblen);
        pbuf[decoded] = '\0';
        buf = newstring(pbuf);
    }
    SDL_free(cb);
    return buf;
}

SVAR(0, commandbuffer, "");

struct hline
{
    char *buf, *action, *icon;
    int colour, flags;

    hline() : buf(NULL), action(NULL), icon(NULL), colour(0), flags(0) {}
    ~hline()
    {
        DELETEA(buf);
        DELETEA(action);
        DELETEA(icon);
    }

    void restore()
    {
        copystring(commandbuf, buf);
        if(commandpos >= (int)strlen(commandbuf)) commandpos = -1;
        DELETEA(commandaction);
        DELETEA(commandicon);
        if(action) commandaction = newstring(action);
        if(icon) commandicon = newstring(icon);
        commandcolour = colour;
        commandflags = flags;
    }

    bool shouldsave()
    {
        return strcmp(commandbuf, buf) ||
               (commandaction ? !action || strcmp(commandaction, action) : action!=NULL) ||
               (commandicon ? !icon || strcmp(commandicon, icon) : icon!=NULL) ||
               commandcolour != colour ||
               commandflags != flags;
    }

    void save()
    {
        buf = newstring(commandbuf);
        if(commandaction) action = newstring(commandaction);
        if(commandicon) icon = newstring(commandicon);
        colour = commandcolour;
        flags = commandflags;
    }

    void run()
    {
        if(flags&CF_EXECUTE && buf[0]=='/') execute(buf+1); // above all else
        else if(action)
        {
            setsvar("commandbuffer", buf, true);
            execute(action);
        }
        else client::toserver(0, buf);
    }
};
vector<hline *> history;
int histpos = 0;

VAR(IDF_PERSIST, maxhistory, 0, 1000, 10000);

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

        int state = keym::ACTION_DEFAULT;
        switch(client::state())
        {
            case CS_ALIVE: case CS_DEAD: default: break;
            case CS_SPECTATOR: state = keym::ACTION_SPECTATOR; break;
            case CS_EDITING: state = keym::ACTION_EDITING; break;
            case CS_WAITING: state = keym::ACTION_WAITING; break;
        }
        char *&action = k.actions[state][0] ? k.actions[state] : k.actions[keym::ACTION_DEFAULT];
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
    if(commandmillis < 0) return false;

    resetcomplete();
    int maxlen = int(sizeof(commandbuf));
    if(commandflags&CF_MESSAGE) maxlen = min(client::maxmsglen(), maxlen);
    int cmdlen = (int)strlen(commandbuf), cmdspace = maxlen - (cmdlen+1);
    len = min(len, cmdspace);
    if(len <= 0) return true;

    if(commandpos<0)
    {
        memcpy(&commandbuf[cmdlen], str, len);
    }
    else
    {
        memmove(&commandbuf[commandpos+len], &commandbuf[commandpos], cmdlen - commandpos);
        memcpy(&commandbuf[commandpos], str, len);
        commandpos += len;
    }
    commandbuf[cmdlen + len] = '\0';

    return true;
}

bool consolekey(int code, bool isdown)
{
    if(commandmillis < 0) return false;

    if(isdown)
    {
        switch(code)
        {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                break;

            case SDLK_HOME:
                if(strlen(commandbuf)) commandpos = 0;
                break;

            case SDLK_END:
                commandpos = -1;
                break;

            case SDLK_DELETE:
            {
                int len = (int)strlen(commandbuf);
                if(commandpos<0) break;
                memmove(&commandbuf[commandpos], &commandbuf[commandpos+1], len - commandpos);
                resetcomplete();
                if(commandpos>=len-1) commandpos = -1;
                break;
            }

            case SDLK_BACKSPACE:
            {
                int len = (int)strlen(commandbuf), i = commandpos>=0 ? commandpos : len;
                if(i<1) break;
                memmove(&commandbuf[i-1], &commandbuf[i], len - i + 1);
                resetcomplete();
                if(commandpos>0) commandpos--;
                else if(!commandpos && len<=1) commandpos = -1;
                break;
            }

            case SDLK_LEFT:
                if(commandpos>0) commandpos--;
                else if(commandpos<0) commandpos = (int)strlen(commandbuf)-1;
                break;

            case SDLK_RIGHT:
                if(commandpos>=0 && ++commandpos>=(int)strlen(commandbuf)) commandpos = -1;
                break;

            case SDLK_UP:
                if(histpos > history.length()) histpos = history.length();
                if(histpos > 0) history[--histpos]->restore();
                break;

            case SDLK_DOWN:
                if(histpos + 1 < history.length()) history[++histpos]->restore();
                break;

            case SDLK_TAB:
                if(commandflags&CF_COMPLETE)
                {
                    complete(commandbuf, commandflags&CF_EXECUTE ? "/" : NULL);
                    if(commandpos>=0 && commandpos>=(int)strlen(commandbuf)) commandpos = -1;
                }
                break;

            case SDLK_v:
                if(SDL_GetModState()&MOD_KEYS)
                {
                    char *pastebuf = pastetext();
                    if(pastebuf)
                    {
                        concatstring(commandbuf, pastebuf);
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
            if(commandbuf[0])
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
            inputcommand(NULL);
            if(h)
            {
                interactive = true;
                h->run();
                interactive = false;
            }
        }
        else if(code==SDLK_ESCAPE || code < 0)
        {
            histpos = history.length();
            inputcommand(NULL);
        }
    }

    return true;
}

void processtextinput(const char *str, int len)
{
    if(!hud::textinput(str, len))
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
#ifdef __APPLE__
        case SDLK_q:
#else
        case SDLK_F4:
#endif
            keyintercept(quit, quit());
            break;
        case SDLK_RETURN:
            keyintercept(fullscreen, setfullscreen(!(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN)));
            break;
        case SDLK_TAB:
            keyintercept(iconify, SDL_MinimizeWindow(screen));
            break;
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
    else if(!consolekey(code, isdown) && !hud::keypress(code, isdown) && haskey) execbind(*haskey, isdown);
}

char *getcurcommand()
{
    return commandmillis > 0 ? commandbuf : (char *)NULL;
}

void clear_console()
{
    keyms.clear();
}

void writebinds(stream *f)
{
    static const char * const cmds[4] = { "bind", "specbind", "editbind", "waitbind" };
    vector<keym *> binds;
    enumerate(keyms, keym, km, binds.add(&km));
    binds.sortname();
    loopj(4)
    {
        bool found = false;
        loopv(binds)
        {
            keym &km = *binds[i];
            if(km.persist[j])
            {
                if(!*km.actions[j]) f->printf("%s %s []\n", cmds[j], escapestring(km.name));
                else if(validateblock(km.actions[j])) f->printf("%s %s [%s]\n", cmds[j], escapestring(km.name), km.actions[j]);
                else f->printf("%s %s %s\n", cmds[j], escapestring(km.name), escapestring(km.actions[j]));
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
    ~filesval() { DELETEA(dir); DELETEA(ext); loopv(files) DELETEA(files[i]); files.shrink(0); }

    void update()
    {
        if(type!=FILES_DIR || millis >= commandmillis) return;
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
static hashtable<char *, filesval *> completions;

int completeoffset = -1, completesize = 0;
bigstring lastcomplete;

void resetcomplete() { completesize = 0; }

void addcomplete(char *command, int type, char *dir, char *ext)
{
    if(identflags&IDF_WORLD)
    {
        conoutf("\frCannot override complete %s", command);
        return;
    }
    if(!dir[0])
    {
        filesval **hasfiles = completions.access(command);
        if(hasfiles) *hasfiles = NULL;
        return;
    }
    if(type==FILES_DIR)
    {
        int dirlen = (int)strlen(dir);
        while(dirlen > 0 && (dir[dirlen-1] == '/' || dir[dirlen-1] == '\\')) dir[--dirlen] = '\0';
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
    filesval **hasfiles = completions.access(command);
    if(hasfiles) *hasfiles = *val;
    else completions[newstring(command)] = *val;
}

void addfilecomplete(char *command, char *dir, char *ext)
{
    addcomplete(command, FILES_DIR, dir, ext);
}

void addlistcomplete(char *command, char *list)
{
    addcomplete(command, FILES_LIST, list, NULL);
}

COMMANDN(0, complete, addfilecomplete, "sss");
COMMANDN(0, listcomplete, addlistcomplete, "ss");

void complete(char *s, const char *cmdprefix)
{
    char *start = s;
    if(cmdprefix)
    {
        int cmdlen = strlen(cmdprefix);
        if(strncmp(s, cmdprefix, cmdlen)) prependstring(s, cmdprefix, BIGSTRLEN);
        start = &s[cmdlen];
    }
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
    if(completesize)
    {
        char *end = strchr(start, ' ');
        if(end) f = completions.find(stringslice(start, end), NULL);
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
                strcmp(f->files[i], lastcomplete) > 0 && (!nextcomplete || strcmp(f->files[i], nextcomplete) < 0))
                nextcomplete = f->files[i];
        }
    }
    else // complete using command names
    {
        enumerate(idents, ident, id,
            if((variable ? id.type == ID_VAR || id.type == ID_SVAR || id.type == ID_FVAR || id.type == ID_ALIAS: id.flags&IDF_COMPLETE) && strncmp(id.name, start, completesize)==0 &&
               strcmp(id.name, lastcomplete) > 0 && (!nextcomplete || strcmp(id.name, nextcomplete) < 0))
                nextcomplete = id.name;
        );
    }
    if(nextcomplete)
    {
        copystring(&s[prefixlen], nextcomplete, BIGSTRLEN-prefixlen);
        copystring(lastcomplete, nextcomplete, BIGSTRLEN);
    }
    else
    {
        if((int)strlen(start) > completesize) start[completesize] = '\0';
        completesize = 0;
    }
}

void setidflag(const char *s, const char *v, int flag, const char *msg, bool alias)
{
    ident *id = idents.access(s);
    if(!id || (alias && id->type != ID_ALIAS))
    {
        if(verbose) conoutf("\frAdding %s of %s failed as it is not available", msg, s);
        return;
    }
    bool on = false;
    if(isnumeric(*v)) on = atoi(v) != 0;
    else if(!strcasecmp("false", v)) on = false;
    else if(!strcasecmp("true", v)) on = true;
    else if(!strcasecmp("on", v)) on = true;
    else if(!strcasecmp("off", v)) on = false;
    if(on && !(id->flags&flag)) id->flags |= flag;
    else if(!on && id->flags&flag) id->flags &= ~flag;
}

ICOMMAND(0, setcomplete, "ss", (char *s, char *t), setidflag(s, t, IDF_COMPLETE, "complete", false));
ICOMMAND(0, setpersist, "ss", (char *s, char *t), setidflag(s, t, IDF_PERSIST, "persist", true));

void setiddesc(const char *s, const char *v, const char *f)
{
    ident *id = idents.access(s);
    if(!id)
    {
        if(verbose) conoutf("\frAdding description of %s failed as it is not available", s);
        return;
    }
    DELETEA(id->desc);
    if(v && *v) id->desc = newstring(v);
    loopvrev(id->fields)
    {
        DELETEA(id->fields[i]);
        id->fields.remove(i);
    }
    if(f && *f) explodelist(f, id->fields);
}
ICOMMAND(0, setdesc, "sss", (char *s, char *t, char *f), setiddesc(s, t, f));

void writecompletions(stream *f)
{
    vector<char *> cmds;
    enumeratekt(completions, char *, k, filesval *, v, { if(v) cmds.add(k); });
    cmds.sort();
    loopv(cmds)
    {
        char *k = cmds[i];
        filesval *v = completions[k];
        if(v->type==FILES_LIST)
        {
            if(validateblock(v->dir)) f->printf("    listcomplete %s [%s]\n", escapeid(k), v->dir);
            else f->printf("    listcomplete %s %s\n", escapeid(k), escapestring(v->dir));
        }
        else f->printf("    complete %s %s %s\n", escapeid(k), escapestring(v->dir), escapestring(v->ext ? v->ext : "*"));
    }
}

#ifdef __APPLE__
extern bool mac_capslock();
extern bool mac_numlock();
#endif

bool capslockon = false, numlockon = false;
#if !defined(WIN32) && !defined(__APPLE__)
#include <X11/XKBlib.h>
#endif
bool capslocked()
{
    #ifdef WIN32
    if(GetKeyState(VK_CAPITAL)) return true;
    #elif defined(__APPLE__)
    if(mac_capslock()) return true;
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
ICOMMAND(0, getcapslock, "", (), intret(capslockon ? 1 : 0));

bool numlocked()
{
    #ifdef WIN32
    if(GetKeyState(VK_NUMLOCK)) return true;
    #elif defined(__APPLE__)
    if(mac_numlock()) return true;
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
ICOMMAND(0, getnumlock, "", (), intret(numlockon ? 1 : 0));
