FVAR(IDF_PERSIST, compasssize, 0, 0.15f, 1000);
FVAR(IDF_PERSIST, compassblend, 0, 0.75f, 1);
VAR(IDF_PERSIST, compassfade, 0, 250, VAR_MAX);
FVAR(IDF_PERSIST, compassfadeamt, 0, 0.75f, 1);
TVAR(IDF_PERSIST, compasstex, "<grey>textures/hud/compass", 3);
TVAR(IDF_PERSIST, compassringtex, "<grey>textures/hud/progress", 3);

struct cstate
{
    char *name, *contents;
    cstate() : name(NULL), contents(NULL) {}
    ~cstate()
    {
        DELETEA(name);
        DELETEA(contents);
    }
};
struct caction : cstate
{
    char code;
    caction() : code(0) {}
    ~caction() {}
};
struct cmenu : cstate
{
    Texture *icon;
    vector<caction> actions;
    bool keep;
    cmenu() : icon(NULL), keep(false) {}
    ~cmenu() { reset(); }
    void reset()
    {
        loopvrev(actions) actions.remove(i);
        actions.shrink(0);
        keep = false;
    }

    int locate(const char code)
    {
        loopv(actions) if(actions[i].code == code || actions[i].code == toupper(code) || actions[i].code == tolower(code))
            return i;
        return -1;
    }
};

int compassmillis = 0, compasspos = 0;
cmenu *curcompass = NULL;
vector<cmenu> cmenus;

void clearcmenu()
{
    compasspos = 0;
    if(compassmillis > 0) compassmillis = -totalmillis;
    if(curcompass)
    {
        curcompass->reset();
        curcompass = NULL;
    }
}
ICOMMAND(0, clearcompass, "", (), clearcmenu());

void resetcmenus()
{
    clearcmenu();
    loopvrev(cmenus) cmenus.remove(i);
    cmenus.shrink(0);
}
ICOMMAND(0, resetcompass, "", (), resetcmenus());

void addcmenu(const char *name, const char *a, const char *b)
{
    if(!name || !*name || !a || !*a) return;
    if(curcompass) clearcmenu();
    loopvrev(cmenus) if(!strcmp(name, cmenus[i].name)) cmenus.remove(i);
    cmenu &c = cmenus.add();
    c.name = newstring(name);
    if(b && *b)
    {
        c.icon = textureload(a, 3);
        c.contents = newstring(b);
    }
    else c.contents = newstring(a);
}
ICOMMAND(0, newcompass, "sss", (char *n, char *c, char *d), addcmenu(n, c, d));

void addaction(const char *name, char code, const char *contents)
{
    if(!name || !*name || !contents || !*contents || !curcompass || curcompass->locate(code) >= 0) return;
    caction &a = curcompass->actions.add();
    a.name = newstring(strlen(name) > 1 ? name : contents);
    a.contents = newstring(contents);
    a.code = code;
}
ICOMMAND(0, compass, "sss", (char *n, char *a, char *b), if(curcompass)
{
    if(*b)
    {
        int code = findkeycode(a);
        if(code) addaction(n, code, b);
    }
    else
    {
        char code = '1';
        while(true)
        {
            if(curcompass->locate(code) < 0) break;
            code++;
            if(code > '9' && code < 'A') code = 'A';
            else if(code > 'Z')
            {
                code = 0;
                break;
            }
        }
        if(code) addaction(n, code, a);
    }
});

ICOMMAND(0, keepcompass, "", (void), if(curcompass) curcompass->keep = true);

void showcmenu(const char *name)
{
    if(!name || !*name) return;
    loopv(cmenus) if(!strcmp(name, cmenus[i].name))
    {
        if(compassmillis <= 0) compassmillis = totalmillis;
        curcompass = &cmenus[i];
        compasspos = 0;
        curcompass->reset();
        execute(curcompass->contents);
        playsound(S_GUIPRESS, camera1->o, camera1, SND_FORCED);
        return;
    }
    conoutft(CON_DEBUG, "\frno such compass menu: %s", name);
}
ICOMMAND(0, showcompass, "s", (char *n), showcmenu(n));

ICOMMAND(0, compassactive, "", (), result(curcompass ? curcompass->name : ""));

const struct compassdirs
{
    int x, y, align;
} compassdir[9] = {
    { 0, 0, TEXT_CENTERED }, // special cancel case
    { 0, -1, TEXT_CENTERED },       { 1, -1, TEXT_LEFT_JUSTIFY },
    { 1, 0, TEXT_LEFT_JUSTIFY },    { 1, 1, TEXT_LEFT_JUSTIFY  },
    { 0, 1, TEXT_CENTERED },        { -1, 1, TEXT_RIGHT_JUSTIFY  },
    { -1, 0, TEXT_RIGHT_JUSTIFY },  { -1, -1, TEXT_RIGHT_JUSTIFY  }
};

int cmenuhit()
{
    if(curcompass)
    {
        if(compasspos > 0) return compasspos;
        float cx = (cursorx-0.5f)*2.f, cy = (cursory-0.5f)*2.f;
        if(sqrtf(cx*cx+cy*cy) <= compasssize*0.5f) return -1;
        else
        {
            float yaw = -(float)atan2(cx, cy)/RAD+202.5f; if(yaw >= 360) yaw -= 360;
            loopi(min(curcompass->actions.length(), 8)) if(yaw > i*45 && yaw <= (i+1)*45)
                return i;
        }
    }
    return -1;
}

void renderaction(int idx, int size, Texture *t, char code, const char *name, bool hit)
{
    pushfont(!idx || hit ? "super" : "default");
    int s = compassdir[idx].x && compassdir[idx].y ? size*2/3 : size,
        x = hudwidth/2+(s*compassdir[idx].x), y = hudheight/2+(s*compassdir[idx].y)-(((0-compassdir[idx].y)+1)*FONTH),
        r = 255, g = hit && idx ? 0 : 255, b = hit && idx ? 0 : 255, f = hit || !idx ? 255 : 128;
    if(t && t != notexture)
    {
        glBindTexture(GL_TEXTURE_2D, t->id);
        gle::colorf(r/255.f, g/255.f, b/255.f, f/255.f*compassblend);
        if(idx) drawslice(0.5f/8+(idx-2)/float(8), 1/float(8), hudwidth/2, hudheight/2, size);
        else drawsized(hudwidth/2-size*3/8, hudheight/2-size*3/8, size*3/4);
    }
    if(code) y += draw_textx("\f{%s}", x, y, r, g, b, int((idx ? 255 : f)*compassblend), compassdir[idx].align, -1, -1, getkeyname(code));
    popfont();
    pushfont(!idx || hit ? "emphasis" : "reduced");
    draw_textx("%s", x, y, r, g, b, int(f*compassblend), compassdir[idx].align, -1, -1, name);
    popfont();
}

void rendercmenu()
{
    if(compassmillis <= 0 || !curcompass) return;
    int size = int(compasssize*hudsize), hit = cmenuhit();
    Texture *t = NULL;
    if(curcompass->icon && curcompass->icon != notexture) t = curcompass->icon;
    else if(*compassringtex) t = textureload(compassringtex, 3);
    renderaction(0, size, t, 0, curcompass->name, hit < 0);
    t = *compasstex ? textureload(compasstex, 3) : NULL;
    loopi(min(curcompass->actions.length(), 8))
        renderaction(i+1, size, t, curcompass->actions[i].code, curcompass->actions[i].name, hit == i);
    if(curcompass->actions.length() > 8)
    {
        pushfont("reduced");
        int x = hudwidth/2, y = hudheight/2+size+FONTH*4, maxy = hudheight-FONTH*2;
        loopi(curcompass->actions.length()-8)
        {
            caction &c = curcompass->actions[i+8];
            y += draw_textx("\f{%s} %s", x, y, 255, 255, 255, int(192*compassblend), TEXT_CENTERED, -1, -1, getkeyname(c.code), c.name);
            if(y >= maxy) break;
        }
        popfont();
    }
}

bool runcmenu(int idx)
{
    bool foundmenu = false;
    cmenu *oldcompass = curcompass;
    if(curcompass)
    {
        curcompass->keep = false;
        if(idx < 0)
        {
            foundmenu = true;
            clearcmenu();
        }
        else if(curcompass->actions.inrange(idx))
        {
            foundmenu = interactive = true;
            execute(curcompass->actions[idx].contents);
            interactive = false;
            if(oldcompass == curcompass && !curcompass->keep) clearcmenu();
        }
    }
    return foundmenu;
}

bool keycmenu(int code, bool isdown, int cooked)
{
    switch(code)
    {
        case SDLK_RIGHT: case SDLK_UP: case SDLK_TAB: case -2: case -4:
        {
            if(!isdown && ++compasspos > curcompass->actions.length()) compasspos = 0;
            return true;
            break;
        }
        case SDLK_LEFT: case SDLK_DOWN: case -5:
        {
            if(!isdown && --compasspos < 0) compasspos = 8;
            return true;
            break;
        }
        case SDLK_RETURN: case -1:
        {
            if(!isdown) runcmenu(cmenuhit());
            return true;
            break;
        }
        case SDLK_ESCAPE: case -3:
        {
            if(!isdown) clearcmenu();
            return true;
            break;
        }
        default:
        {
            int idx = code != '0' ? curcompass->locate(code) : -1;
            if(idx >= 0 || code == '0') return isdown || runcmenu(idx);
            break;
        }
    }
    return false;
}
