// main.cpp: initialisation & main loop

#include "engine.h"
#include <signal.h>

SDL_Surface *screen = NULL;

#ifdef FAKESHOWCURSOR
SDL_Cursor *scursor = NULL, *ncursor = NULL;
#endif

void showcursor(bool show)
{
#ifdef FAKESHOWCURSOR
    if(show)
    {
        if(scursor) SDL_FreeCursor(scursor);
        scursor = NULL;
        SDL_SetCursor(ncursor);
    }
    else
    {
        if(!ncursor) ncursor = SDL_GetCursor();
        if(!scursor)
        {
            Uint8 sd[1] = { 0 };
            if(!(scursor = SDL_CreateCursor(sd, sd, 1, 1, 0, 0)))
                fatal("could not create blank cursor");
        }

        SDL_SetCursor(scursor);
    }
#else
    SDL_ShowCursor(show ? 1 : 0);
#endif
}

void setcaption(const char *text, const char *text2)
{
    static string prevtext = "", prevtext2 = "";
    if(strcmp(text, prevtext) || strcmp(text2, prevtext2))
    {
        copystring(prevtext, text);
        copystring(prevtext2, text2);
        defformatstring(caption)("%s v%s-%s%d (%s)%s%s%s%s", versionname, versionstring, versionplatname, versionarch, versionrelease, text[0] ? ": " : "", text, text2[0] ? " - " : "", text2);
        SDL_WM_SetCaption(caption, NULL);
    }
}

void keyrepeat(bool on)
{
    SDL_EnableKeyRepeat(on ? SDL_DEFAULT_REPEAT_DELAY : 0, SDL_DEFAULT_REPEAT_INTERVAL);
}

void inputgrab(bool on)
{
#ifndef WIN32
    if(!(screen->flags & SDL_FULLSCREEN)) SDL_WM_GrabInput(SDL_GRAB_OFF);
    else
#endif
    SDL_WM_GrabInput(on ? SDL_GRAB_ON : SDL_GRAB_OFF);
    showcursor(!on);
}

VARF(0, grabinput, 0, 0, 1, inputgrab(grabinput!=0));
VAR(IDF_PERSIST, autograbinput, 0, 1, 1);

extern void cleargamma();

void cleanup()
{
    recorder::stop();
    cleanupserver();
    showcursor(true);
//#ifdef FAKESHOWCURSOR
//    if(scursor) SDL_FreeCursor(scursor);
//    scursor = ncursor = NULL;
//#endif
    SDL_WM_GrabInput(SDL_GRAB_OFF);
    cleargamma();
    freeocta(worldroot);
    extern void clear_command();    clear_command();
    extern void clear_console();    clear_console();
    extern void clear_mdls();       clear_mdls();
    stopsound();
    closelogfile();
    SDL_Quit();
}

void quit()                  // normal exit
{
    extern void writeinitcfg();
    extern void writeservercfg();
    inbetweenframes = false;
    writeinitcfg();
    writeservercfg();
    writecfg();
    abortconnect();
    disconnect(1);
    cleanup();
    exit(EXIT_SUCCESS);
}

volatile int errors = 0;
void fatal(const char *s, ...)    // failure exit
{
    if(++errors <= 2) // print up to one extra recursive error
    {
        defvformatbigstring(msg, s, s);
        if(logfile) logoutf("%s", msg);
        #ifndef WIN32
        fprintf(stderr, "%s\n", msg);
        #endif
        if(errors <= 1) // avoid recursion
        {
            if(SDL_WasInit(SDL_INIT_VIDEO))
            {
                showcursor(true);
                SDL_WM_GrabInput(SDL_GRAB_OFF);
                cleargamma();
            }
            #ifdef WIN32
            defformatstring(cap)("%s: Error", versionname);
            MessageBox(NULL, msg, cap, MB_OK|MB_SYSTEMMODAL);
            #endif
            SDL_Quit();
        }
    }
    exit(EXIT_FAILURE);
}

volatile bool fatalsig = false;
void fatalsignal(int signum)
{
    if(!fatalsig)
    {
        fatalsig = true;
        const char *str = "";
        switch(signum)
        {
            case SIGINT: str = "Interrupt signal %d (Exiting)"; break;
            case SIGILL: str = "Fatal signal %d (Illegal Instruction)"; break;
            case SIGABRT: str = "Fatal signal %d (Aborted)"; break;
            case SIGFPE: str = "Fatal signal %d (Floating-point Exception)"; break;
            case SIGSEGV: str = "Fatal signal %d (Segmentation Violation)"; break;
            case SIGTERM: str = "Fatal signal %d (Terminated)"; break;
#if !defined(WIN32) && !defined(__APPLE__)
            case SIGQUIT: str = "Fatal signal %d (Quit)"; break;
            case SIGKILL: str = "Fatal signal %d (Killed)"; break;
            case SIGPIPE: str = "Fatal signal %d (Broken Pipe)"; break;
            case SIGALRM: str = "Fatal signal %d (Alarm)"; break;
#endif
            default: str = "Error: Fatal signal %d (Unknown Error)"; break;
        }
        fatal(str, signum);
    }
}

void reloadsignal(int signum)
{
    rehash(true);
}

int initing = NOT_INITING;

bool initwarning(const char *desc, int level, int type, bool force)
{
    if(initing < level)
    {
        addchange(desc, type, force);
        return true;
    }
    return false;
}

#define SCR_MINW 320
#define SCR_MINH 200
#define SCR_MAXW 10000
#define SCR_MAXH 10000
#define SCR_DEFAULTW 1024
#define SCR_DEFAULTH 768
VARF(0, scr_w, SCR_MINW, -1, SCR_MAXW, initwarning("screen resolution"));
VARF(0, scr_h, SCR_MINH, -1, SCR_MAXH, initwarning("screen resolution"));
VARF(0, colorbits, 0, 0, 32, initwarning("color depth"));
VARF(0, depthbits, 0, 0, 32, initwarning("depth-buffer precision"));
VARF(0, stencilbits, 0, 0, 32, initwarning("stencil-buffer precision"));
VARF(0, fsaa, -1, -1, 16, initwarning("anti-aliasing"));
int actualvsync = -1;
VARF(0, vsync, -1, -1, 1, initwarning("vertical sync"));

void writeinitcfg()
{
    stream *f = openutf8file("init.cfg", "w");
    if(!f) return;
    f->printf("// automatically written on exit, DO NOT MODIFY\n// modify settings in game\n");
    extern int fullscreen;
    f->printf("fullscreen %d\n", fullscreen);
    f->printf("scr_w %d\n", scr_w);
    f->printf("scr_h %d\n", scr_h);
    f->printf("colorbits %d\n", colorbits);
    f->printf("depthbits %d\n", depthbits);
    f->printf("stencilbits %d\n", stencilbits);
    f->printf("fsaa %d\n", fsaa);
    f->printf("vsync %d\n", vsync);
    extern int useshaders, shaderprecision, forceglsl;
    f->printf("shaders %d\n", useshaders);
    f->printf("shaderprecision %d\n", shaderprecision);
    f->printf("forceglsl %d\n", forceglsl);
    extern int soundmono, soundmixchans, soundbuflen, soundfreq;
    f->printf("soundmono %d\n", soundmono);
    f->printf("soundmixchans %d\n", soundmixchans);
    f->printf("soundbuflen %d\n", soundbuflen);
    f->printf("soundfreq %d\n", soundfreq);
    f->printf("verbose %d\n", verbose);
    delete f;
}

VAR(IDF_PERSIST, compresslevel, 0, 9, 9);
VAR(IDF_PERSIST, imageformat, IFMT_NONE+1, IFMT_PNG, IFMT_MAX-1);

void screenshot(char *sname)
{
    ImageData image(screen->w, screen->h, 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, screen->w, screen->h, GL_RGB, GL_UNSIGNED_BYTE, image.data);
    string fname;
    if(sname && *sname) copystring(fname, sname);
    else formatstring(fname)("screenshots/%s", *filetimeformat ? gettime(filetimelocal ? currenttime : clocktime, filetimeformat) : (*mapname ? mapname : "screen"));
    saveimage(fname, image, imageformat, compresslevel, true);
}

ICOMMAND(0, screenshot, "s", (char *s), if(!(identflags&IDF_WORLD)) screenshot(s));
ICOMMAND(0, quit, "", (void), if(!(identflags&IDF_WORLD)) quit());

void setfullscreen(bool enable, bool force)
{
    if(!screen) return;
    fullscreen = enable ? 1 : 0;
#if defined(WIN32) || defined(__APPLE__)
    initwarning(enable ? "fullscreen" : "windowed", INIT_RESET, CHANGE_GFX, force);
#else
    if(enable == !(screen->flags&SDL_FULLSCREEN))
    {
        SDL_WM_ToggleFullScreen(screen);
        inputgrab(grabinput!=0);
    }
#endif
}

VARF(0, fullscreen, 0, 1, 1, if(!(identflags&IDF_WORLD)) setfullscreen(fullscreen!=0));

void screenres(int *w, int *h)
{
#if !defined(WIN32) && !defined(__APPLE__)
    if(initing >= INIT_RESET)
    {
#endif
        scr_w = clamp(*w, SCR_MINW, SCR_MAXW);
        scr_h = clamp(*h, SCR_MINH, SCR_MAXH);
#if defined(WIN32) || defined(__APPLE__)
        initwarning("screen resolution");
#else
        return;
    }
    SDL_Surface *surf = SDL_SetVideoMode(clamp(*w, SCR_MINW, SCR_MAXW), clamp(*h, SCR_MINH, SCR_MAXH), 0, SDL_OPENGL|(screen->flags&SDL_FULLSCREEN ? SDL_FULLSCREEN : SDL_RESIZABLE));
    if(!surf) return;
    screen = surf;
    scr_w = screen->w;
    scr_h = screen->h;
    glViewport(0, 0, scr_w, scr_h);
#endif
}

COMMAND(0, screenres, "ii");

static int curgamma = 100;
VARF(IDF_PERSIST, gamma, 30, 100, 300,
{
    if(gamma == curgamma) return;
    curgamma = gamma;
    float f = gamma/100.0f;
    if(SDL_SetGamma(f,f,f)==-1)
    {
        conoutf("\frcould not set gamma: %s", SDL_GetError());
    }
});

void restoregamma()
{
    if(curgamma == 100) return;
    float f = curgamma/100.0f;
    SDL_SetGamma(1, 1, 1);
    SDL_SetGamma(f, f, f);
}

void cleargamma()
{
    if(curgamma != 100) SDL_SetGamma(1, 1, 1);
}

int desktopw = 0, desktoph = 0;

void setupscreen(int &usedcolorbits, int &useddepthbits, int &usedfsaa)
{
    int flags = SDL_RESIZABLE;
    #if defined(WIN32) || defined(__APPLE__)
    flags = 0;
    #endif
    if(fullscreen) flags = SDL_FULLSCREEN;
    SDL_Rect **modes = SDL_ListModes(NULL, SDL_OPENGL|flags);
    if(modes && modes!=(SDL_Rect **)-1)
    {
        int widest = -1, best = -1;
        for(int i = 0; modes[i]; i++)
        {
            if(widest < 0 || modes[i]->w > modes[widest]->w || (modes[i]->w == modes[widest]->w && modes[i]->h > modes[widest]->h))
                widest = i;
        }
        if(scr_w < 0 || scr_h < 0)
        {
            int w = scr_w, h = scr_h, ratiow = desktopw, ratioh = desktoph;
            if(w < 0 && h < 0) { w = SCR_DEFAULTW; h = SCR_DEFAULTH; }
            if(ratiow <= 0 || ratioh <= 0) { ratiow = modes[widest]->w; ratioh = modes[widest]->h; }
            for(int i = 0; modes[i]; i++) if(modes[i]->w*ratioh == modes[i]->h*ratiow)
            {
                if(w <= modes[i]->w && h <= modes[i]->h && (best < 0 || modes[i]->w < modes[best]->w))
                    best = i;
            }
        }
        if(best < 0)
        {
            int w = scr_w, h = scr_h;
            if(w < 0 && h < 0) { w = SCR_DEFAULTW; h = SCR_DEFAULTH; }
            else if(w < 0) w = (h*SCR_DEFAULTW)/SCR_DEFAULTH;
            else if(h < 0) h = (w*SCR_DEFAULTH)/SCR_DEFAULTW;
            for(int i = 0; modes[i]; i++)
            {
                if(w <= modes[i]->w && h <= modes[i]->h && (best < 0 || modes[i]->w < modes[best]->w || (modes[i]->w == modes[best]->w && modes[i]->h < modes[best]->h)))
                    best = i;
            }
        }
        if(flags&SDL_FULLSCREEN)
        {
            if(best >= 0) { scr_w = modes[best]->w; scr_h = modes[best]->h; }
            else if(desktopw > 0 && desktoph > 0) { scr_w = desktopw; scr_h = desktoph; }
            else if(widest >= 0) { scr_w = modes[widest]->w; scr_h = modes[widest]->h; }
        }
        else if(best < 0)
        {
            scr_w = min(scr_w >= 0 ? scr_w : (scr_h >= 0 ? (scr_h*SCR_DEFAULTW)/SCR_DEFAULTH : SCR_DEFAULTW), (int)modes[widest]->w);
            scr_h = min(scr_h >= 0 ? scr_h : (scr_w >= 0 ? (scr_w*SCR_DEFAULTH)/SCR_DEFAULTW : SCR_DEFAULTH), (int)modes[widest]->h);
        }
    }
    if(scr_w < 0 && scr_h < 0) { scr_w = SCR_DEFAULTW; scr_h = SCR_DEFAULTH; }
    else if(scr_w < 0) scr_w = (scr_h*SCR_DEFAULTW)/SCR_DEFAULTH;
    else if(scr_h < 0) scr_h = (scr_w*SCR_DEFAULTH)/SCR_DEFAULTW;

    bool hasbpp = true;
    if(colorbits)
        hasbpp = SDL_VideoModeOK(scr_w, scr_h, colorbits, SDL_OPENGL|flags)==colorbits;

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#if SDL_VERSION_ATLEAST(1, 2, 11)
    SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, vsync >= 0 ? vsync : 0);
#endif
    static int configs[] =
    {
        0x7, /* try everything */
        0x6, 0x5, 0x3, /* try disabling one at a time */
        0x4, 0x2, 0x1, /* try disabling two at a time */
        0 /* try disabling everything */
    };
    int config = 0;
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    if(!depthbits) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    if(!fsaa)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    }
    loopi(sizeof(configs)/sizeof(configs[0]))
    {
        config = configs[i];
        if(!depthbits && config&1) continue;
        if(!stencilbits && config&2) continue;
        if(fsaa<=0 && config&4) continue;
        if(depthbits) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, config&1 ? depthbits : 16);
        if(stencilbits)
        {
            hasstencil = config&2 ? stencilbits : 0;
            SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, hasstencil);
        }
        else hasstencil = 0;
        if(fsaa>0)
        {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, config&4 ? 1 : 0);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, config&4 ? fsaa : 0);
        }
        screen = SDL_SetVideoMode(scr_w, scr_h, hasbpp ? colorbits : 0, SDL_OPENGL|flags);
        if(screen) break;
    }
    if(!screen) fatal("Unable to create OpenGL screen: %s", SDL_GetError());
    else
    {
        if(!hasbpp) conoutf("\fr%d bit color buffer not supported - disabling", colorbits);
        if(depthbits && (config&1)==0) conoutf("\fr%d bit z-buffer not supported - disabling", depthbits);
        if(stencilbits && (config&2)==0) conoutf("\frStencil buffer not supported - disabling");
        if(fsaa>0 && (config&4)==0) conoutf("\fr%dx anti-aliasing not supported - disabling", fsaa);
#if SDL_VERSION_ATLEAST(1, 2, 11)
        if(SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &actualvsync) >= 0 && actualvsync >= 0) // could be forced on
            conoutf("vsync is %s", actualvsync ? "enabled" : "disabled");
#endif
    }

    scr_w = screen->w;
    scr_h = screen->h;

    usedcolorbits = hasbpp ? colorbits : 0;
    useddepthbits = config&1 ? depthbits : 0;
    usedfsaa = config&4 ? fsaa : 0;
}

void resetgl()
{
    clearchanges(CHANGE_GFX);

    progress(0, "resetting OpenGL..");

    extern void cleanupva();
    extern void cleanupparticles();
    extern void cleanupsky();
    extern void cleanupmodels();
    extern void cleanuptextures();
    extern void cleanuplightmaps();
    extern void cleanupblendmap();
    extern void cleanshadowmap();
    extern void cleanreflections();
    extern void cleanupglare();
    extern void cleanupdepthfx();
    extern void cleanupshaders();
    extern void cleanupgl();
    recorder::cleanup();
    cleanupva();
    cleanupparticles();
    cleanupsky();
    cleanupmodels();
    cleanuptextures();
    cleanuplightmaps();
    cleanupblendmap();
    cleanshadowmap();
    cleanreflections();
    cleanupglare();
    cleanupdepthfx();
    cleanupshaders();
    cleanupgl();

    SDL_SetVideoMode(0, 0, 0, 0);

    int usedcolorbits = 0, useddepthbits = 0, usedfsaa = 0;
    setupscreen(usedcolorbits, useddepthbits, usedfsaa);
    gl_init(scr_w, scr_h, usedcolorbits, useddepthbits, usedfsaa);

    extern void reloadfonts();
    extern void reloadtextures();
    extern void reloadshaders();

    inbetweenframes = false;
    if(!reloadtexture(notexturetex) || !reloadtexture(blanktex) || !reloadtexture(logotex) || !reloadtexture(badgetex))
        fatal("failed to reload core textures");
    reloadfonts();
    inbetweenframes = true;
    progress(0, "initializing...");
    restoregamma();
    reloadshaders();
    reloadtextures();
    initlights();
    allchanged(true);
}

ICOMMAND(0, resetgl, "", (void), if(!(identflags&IDF_WORLD)) resetgl());

bool activewindow = true, warping = false, minimized = false;

vector<SDL_Event> events;

void pushevent(const SDL_Event &e)
{
    events.add(e);
}

static bool filterevent(const SDL_Event &event)
{
    switch(event.type)
    {
        case SDL_MOUSEMOTION:
            if(grabinput || activewindow)
            {
                if(warping && event.motion.x == screen->w / 2 && event.motion.y == screen->h / 2)
                    return false;  // ignore any motion events generated by SDL_WarpMouse
                #ifdef __APPLE__
                if(event.motion.y == 0)
                    return false;  // let mac users drag windows via the title bar
                #endif
            }
            break;
    }
    return true;
}

static inline bool pollevent(SDL_Event &event)
{
    while(SDL_PollEvent(&event))
    {
        if(filterevent(event)) return true;
    }
    return false;
}

bool interceptkey(int sym, int mod)
{
    SDL_Event event;
    while(pollevent(event)) switch(event.type)
    {
        case SDL_KEYDOWN:
            if(event.key.keysym.sym == sym && (!mod || SDL_GetModState()&mod))
                return true;
        default:
            pushevent(event);
            break;
    }
    return false;
}

static void ignoremousemotion()
{
    SDL_Event e;
    SDL_PumpEvents();
    while(SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_EVENTMASK(SDL_MOUSEMOTION)));
}

void resetcursor(bool warp, bool reset)
{
    if(warp && grabinput)
    {
        SDL_WarpMouse(screen->w/2, screen->h/2);
        warping = true;
    }
    if(reset) cursorx = cursory = 0.5f;
}

static void checkmousemotion(int &dx, int &dy)
{
    loopv(events)
    {
        SDL_Event &event = events[i];
        if(event.type != SDL_MOUSEMOTION)
        {
            if(i > 0) events.remove(0, i);
            return;
        }
        dx += event.motion.xrel;
        dy += event.motion.yrel;
    }
    events.setsize(0);
    SDL_Event event;
    while(pollevent(event))
    {
        if(event.type != SDL_MOUSEMOTION)
        {
            events.add(event);
            return;
        }
        dx += event.motion.xrel;
        dy += event.motion.yrel;
    }
}

void checkinput()
{
    SDL_Event event;
    int lasttype = 0, lastbut = 0;
    bool mousemoved = false, shouldwarp = false;
    while(events.length() || pollevent(event))
    {
        if(events.length()) event = events.remove(0);

        switch (event.type)
        {
            case SDL_QUIT:
                quit();
                return;

#if !defined(WIN32) && !defined(__APPLE__)
            case SDL_VIDEORESIZE:
                screenres(&event.resize.w, &event.resize.h);
                break;
#endif

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                keypress(event.key.keysym.sym, event.key.state==SDL_PRESSED, uni2cube(event.key.keysym.unicode));
                break;

            case SDL_ACTIVEEVENT:
            {
                if(event.active.state & SDL_APPINPUTFOCUS)
                {
                    activewindow = event.active.gain ? true : false;

                    if(autograbinput)
                        setvar("grabinput", event.active.gain ? 1 : 0, true);
                }
                if(event.active.state & SDL_APPACTIVE)
                    minimized = !event.active.gain;
                break;
            }
            case SDL_MOUSEMOTION:
            {
                if(grabinput || activewindow)
                {
                    //conoutf("mouse: %d %d, %d %d [%s]", event.motion.xrel, event.motion.yrel, event.motion.x, event.motion.y, warping ? "true" : "false");
                    int dx = event.motion.xrel, dy = event.motion.yrel;
                    checkmousemotion(dx, dy);
                    shouldwarp = game::mousemove(dx, dy, event.motion.x, event.motion.y, screen->w, screen->h); // whether game controls engine cursor
                    mousemoved = true;
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                if(lasttype==event.type && lastbut==event.button.button) break; // why?? get event twice without it
                keypress(-event.button.button, event.button.state!=0, 0);
                lasttype = event.type;
                lastbut = event.button.button;
                break;
            }
        }
    }
    if(mousemoved)
    {
        warping = false;
        if((grabinput || activewindow) && shouldwarp) resetcursor(true, false);
    }
}

void swapbuffers(bool overlay)
{
    recorder::capture(overlay);
    SDL_GL_SwapBuffers();
}

VAR(IDF_PERSIST, maxfps, 0, 200, 1000);
VAR(IDF_PERSIST, menufps, 0, 60, 1000);

void limitfps(int &millis, int curmillis)
{
    int limit = (hasnoview() || minimized) && menufps ? (maxfps ? min(maxfps, menufps) : menufps) : maxfps;
    if(!limit) return;
    static int fpserror = 0;
    int delay = 1000/limit - (millis-curmillis);
    if(delay < 0) fpserror = 0;
    else
    {
        fpserror += 1000%limit;
        if(fpserror >= limit)
        {
            ++delay;
            fpserror -= limit;
        }
        if(delay > 0)
        {
            SDL_Delay(delay);
            millis += delay;
        }
    }
}

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
void stackdumper(unsigned int type, EXCEPTION_POINTERS *ep)
{
    if(!ep) fatal("unknown type");
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *context = ep->ContextRecord;
    bigstring out, t;
    formatbigstring(out)("%s Win32 Exception: 0x%x [0x%x]\n\n", versionname, er->ExceptionCode, er->ExceptionCode==EXCEPTION_ACCESS_VIOLATION ? er->ExceptionInformation[1] : -1);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
#ifdef _AMD64_
    STACKFRAME64 sf = {{context->Rip, 0, AddrModeFlat}, {}, {context->Rbp, 0, AddrModeFlat}, {context->Rsp, 0, AddrModeFlat}, 0};
    while(::StackWalk64(IMAGE_FILE_MACHINE_AMD64, GetCurrentProcess(), GetCurrentThread(), &sf, context, NULL, ::SymFunctionTableAccess, ::SymGetModuleBase, NULL))
    {
        union { IMAGEHLP_SYMBOL64 sym; char symext[sizeof(IMAGEHLP_SYMBOL64) + sizeof(bigstring)]; };
        sym.SizeOfStruct = sizeof(sym);
        sym.MaxNameLength = sizeof(symext) - sizeof(sym);
        IMAGEHLP_LINE64 line;
        line.SizeOfStruct = sizeof(line);
        DWORD64 symoff;
        DWORD lineoff;
        if(SymGetSymFromAddr64(GetCurrentProcess(), sf.AddrPC.Offset, &symoff, &sym) && SymGetLineFromAddr64(GetCurrentProcess(), sf.AddrPC.Offset, &lineoff, &line))
#else
    STACKFRAME sf = {{context->Eip, 0, AddrModeFlat}, {}, {context->Ebp, 0, AddrModeFlat}, {context->Esp, 0, AddrModeFlat}, 0};
    while(::StackWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(), &sf, context, NULL, ::SymFunctionTableAccess, ::SymGetModuleBase, NULL))
    {
        union { IMAGEHLP_SYMBOL sym; char symext[sizeof(IMAGEHLP_SYMBOL) + sizeof(bigstring)]; };
        sym.SizeOfStruct = sizeof(sym);
        sym.MaxNameLength = sizeof(symext) - sizeof(sym);
        IMAGEHLP_LINE line;
        line.SizeOfStruct = sizeof(line);
        DWORD symoff, lineoff;
        if(SymGetSymFromAddr(GetCurrentProcess(), sf.AddrPC.Offset, &symoff, &sym) && SymGetLineFromAddr(GetCurrentProcess(), sf.AddrPC.Offset, &lineoff, &line))
#endif
        {
            char *del = strrchr(line.FileName, '\\');
            formatbigstring(t)("%s - %s [%d]\n", sym.Name, del ? del + 1 : line.FileName, line.LineNumber);
            concatbigstring(out, t);
        }
    }
    fatal(out);
}
#endif

#define MAXFPSHISTORY 60

int fpspos = 0, fpshistory[MAXFPSHISTORY];

void getfps(int &fps, int &bestdiff, int &worstdiff)
{
    int total = fpshistory[MAXFPSHISTORY-1], best = total, worst = total;
    loopi(MAXFPSHISTORY-1)
    {
        int millis = fpshistory[i];
        total += millis;
        if(millis < best) best = millis;
        if(millis > worst) worst = millis;
    }

    fps = (1000*MAXFPSHISTORY)/total;
    bestdiff = 1000/best-fps;
    worstdiff = fps-1000/worst;
}

void getfps_(int *raw)
{
    int fps, bestdiff, worstdiff;
    if(*raw) fps = 1000/fpshistory[(fpspos+MAXFPSHISTORY-1)%MAXFPSHISTORY];
    else getfps(fps, bestdiff, worstdiff);
    intret(fps);
}

COMMANDN(0, getfps, getfps_, "i");

VAR(0, curfps, 1, 0, -1);
VAR(0, bestfps, 1, 0, -1);
VAR(0, bestfpsdiff, 1, 0, -1);
VAR(0, worstfps, 1, 0, -1);
VAR(0, worstfpsdiff, 1, 0, -1);

void resetfps()
{
    loopi(MAXFPSHISTORY) fpshistory[i] = 1;
    fpspos = 0;
}

void updatefps(int frames, int millis)
{
    fpshistory[fpspos++] = max(1, min(1000, millis));
    if(fpspos >= MAXFPSHISTORY) fpspos = 0;

    int fps, bestdiff, worstdiff;
    getfps(fps, bestdiff, worstdiff);

    curfps = fps;
    bestfps = fps+bestdiff;
    bestfpsdiff = bestdiff;
    worstfps = fps-worstdiff;
    worstfpsdiff = worstdiff;
}

bool inbetweenframes = false, renderedframe = true;

static bool findarg(int argc, char **argv, const char *str)
{
    for(int i = 1; i<argc; i++) if(strstr(argv[i], str)==argv[i]) return true;
    return false;
}

bool progressing = false;

FVAR(0, loadprogress, 0, 0, 1);
SVAR(0, progresstitle, "");
SVAR(0, progresstext, "");
FVAR(0, progressamt, 0, 0, 1);
FVAR(0, progresspart, 0, 0, 1);
VAR(IDF_PERSIST, progressdelay, 0, 100, VAR_MAX);
VAR(IDF_PERSIST, progressupdate, 0, 0, 1);
int lastprogress = 0;

void progress(float bar1, const char *text1, float bar2, const char *text2)
{
    if(progressing || !inbetweenframes || envmapping) return;
    int ticks = SDL_GetTicks();
    if(lastprogress > 0 && ticks < 0) lastprogress = 1-INT_MAX;
    if((actualvsync || !progressupdate || bar1 > 0) && ticks-lastprogress < progressdelay) return;
    lastprogress = ticks;
    clientkeepalive();

    #ifdef __APPLE__
    interceptkey(SDLK_UNKNOWN); // keep the event queue awake to avoid 'beachball' cursor
    #endif

    setsvar("progresstitle", text1 ? text1 : "please wait..");
    setfvar("progressamt", bar1);
    setsvar("progresstext", text2 ? text2 : "");
    setfvar("progresspart", bar2);
    if(verbose >= 4)
    {
        if(text2) conoutf("%s [%.2f%%], %s [%.2f%%]", text1, bar1*100.f, text2, bar2*100.f);
        else if(text1) conoutf("%s [%.2f%%]", text1, bar1*100.f);
        else conoutf("progressing [%.2f%%]", bar1*100.f);
    }

    progressing = true;
    loopi(2) { drawnoview(); swapbuffers(false); }
    progressing = false;
}


bool pixeling = false;
bvec pixel(0, 0, 0);
char *pixelact = NULL;

ICOMMAND(0, printpixel, "", (void), conoutft(CON_SELF, "pixel = 0x%.6X (%d, %d, %d)", pixel.tohexcolor(), pixel.r, pixel.g, pixel.b));
ICOMMAND(0, getpixel, "i", (int *n), {
    switch(*n)
    {
        case 1: intret(pixel.r); break;
        case 2: intret(pixel.g); break;
        case 3: intret(pixel.b); break;
        case 0: default: intret(pixel.tohexcolor()); break;
    }
});

void readpixel(char *act)
{
    if(pixeling) return;
    if(!editmode) { conoutf("\froperation only allowed in edit mode"); return; }
    if(pixelact) delete[] pixelact;
    pixelact = act && *act ? newstring(act) : NULL;
    pixeling = true;
}
ICOMMAND(0, readpixel, "s", (char *act), readpixel(act));

VAR(0, numcpus, 1, 1, 16);

int main(int argc, char **argv)
{
    #ifdef WIN32
    //atexit((void (__cdecl *)(void))_CrtDumpMemoryLeaks);
    #ifndef _DEBUG
    #ifndef __GNUC__
    __try {
    #endif
    #endif
    #endif

    currenttime = time(NULL); // initialise
    clocktime = mktime(gmtime(&currenttime));
    clockoffset = currenttime-clocktime;

    setlogfile(NULL);
    setlocations(true);

    char *initscript = NULL;
    initing = INIT_RESET;
    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'h': serveroption(argv[i]); break;
        }
    }
    setlogfile("log.txt");
    setcrc(argv[0]);
    execfile("init.cfg", false);
    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'h': /* parsed first */ break;
            case 'r': /* compat, ignore */ break;
            case 'd':
            {
                switch(argv[i][2])
                {
                    case 'w': scr_w = clamp(atoi(&argv[i][3]), SCR_MINW, SCR_MAXW); if(!findarg(argc, argv, "-dh")) scr_h = -1; break;
                    case 'h': scr_h = clamp(atoi(&argv[i][3]), SCR_MINH, SCR_MAXH); if(!findarg(argc, argv, "-dw")) scr_w = -1; break;
                    case 'd': depthbits = atoi(&argv[i][3]); break;
                    case 'c': colorbits = atoi(&argv[i][3]); break;
                    case 'a': fsaa = atoi(&argv[i][3]); break;
                    case 'v': vsync = atoi(&argv[i][3]); break;
                    case 'f': fullscreen = atoi(&argv[i][3]); break;
                    case 's': stencilbits = atoi(&argv[i][3]); break;
                    case 'u':
                    {
                        extern int useshaders, shaderprecision, forceglsl;
                        int sh = -1, prec = shaderprecision;
                        for(int j = 3; argv[i][j]; j++) switch(argv[i][j])
                        {
                            case 'a': case 'A': forceglsl = 0; sh = 1; break;
                            case 'g': case 'G': forceglsl = 1; sh = 1; break;
                            case 'f': case 'F': case '0': sh = 0; break;
                            case '1': case '2': case '3': if(sh < 0) sh = 1; prec = argv[i][j] - '1'; break;
                            default: break;
                        }
                        useshaders = sh > 0 ? 1 : 0;
                        shaderprecision = prec;
                        break;
                    }
                    default: conoutf("\frunknown display option %c", argv[i][2]); break;
                }
                break;
            }
            case 'x': initscript = &argv[i][2]; break;
            default:
                if(!serveroption(argv[i])) gameargs.add(argv[i]);
                break;
        }
        else gameargs.add(argv[i]);
    }

    initing = NOT_INITING;

    numcpus = clamp(guessnumcpus(), 1, 16);

    conoutf("loading enet..");
    if(enet_initialize()<0) fatal("Unable to initialise network module");
    atexit(enet_deinitialize);
    enet_time_set(0);

    conoutf("loading game..");
    initgame();

    conoutf("loading sdl..");
    #ifdef WIN32
    SetEnvironmentVariable("SDL_DISABLE_LOCK_KEYS", "1");
    SetEnvironmentVariable("SDL_VIDEO_ALLOW_SCREENSAVER", "0");
    #else
    setenv("SDL_DISABLE_LOCK_KEYS", "1", 1);
    setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "0", 1);
    #endif
    int par = 0;
    #ifdef _DEBUG
    par = SDL_INIT_NOPARACHUTE;
    #ifdef WIN32
    SetEnvironmentVariable("SDL_DEBUG", "1");
    #endif
    #endif

    //#ifdef WIN32
    //SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    //#endif

    par |= SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_NOPARACHUTE;
    if(SDL_Init(par) < 0) fatal("error initialising SDL: %s", SDL_GetError());

    conoutf("loading video..");
    const SDL_VideoInfo *video = SDL_GetVideoInfo();
    if(video)
    {
        desktopw = video->current_w;
        desktoph = video->current_h;
    }
    int usedcolorbits = 0, useddepthbits = 0, usedfsaa = 0;
    setupscreen(usedcolorbits, useddepthbits, usedfsaa);

    showcursor(false);
    keyrepeat(false);
    setcaption("please wait..");

    signal(SIGINT, fatalsignal);
    signal(SIGILL, fatalsignal);
    signal(SIGABRT, fatalsignal);
    signal(SIGFPE, fatalsignal);
    signal(SIGSEGV, fatalsignal);
    signal(SIGTERM, fatalsignal);
#if !defined(WIN32) && !defined(__APPLE__)
    signal(SIGHUP, reloadsignal);
    signal(SIGQUIT, fatalsignal);
    signal(SIGKILL, fatalsignal);
    signal(SIGPIPE, fatalsignal);
    signal(SIGALRM, fatalsignal);
#endif

    conoutf("loading gl..");
    gl_checkextensions();
    gl_init(scr_w, scr_h, usedcolorbits, useddepthbits, usedfsaa);
    if(!(notexture = textureload(notexturetex)) || !(blanktexture = textureload(blanktex)))
        fatal("could not find core textures");

    conoutf("loading sound..");
    initsound();

    game::start();

    conoutf("loading defaults..");
    if(!execfile("config/stdlib.cfg", false)) fatal("cannot find data files");
    if(!setfont("default")) fatal("no default font specified");
    inbetweenframes = true;
    progress(0, "please wait..");

    conoutf("loading gl effects..");
    progress(0, "loading gl effects..");
    loadshaders();

    conoutf("loading world..");
    progress(0, "loading world..");
    emptymap(0, true, NULL, true);

    conoutf("loading config..");
    progress(0, "loading config..");
    rehash(false);
    smartmusic(true, false);

    conoutf("loading required data..");
    progress(0, "loading required data..");
    preloadtextures();
    particleinit();
    initdecals();

    trytofindocta();
    conoutf("loading main..");
    progress(0, "loading main..");
    if(initscript) execute(initscript, true);

#ifdef WIN32
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    if(SDL_GetWMInfo(&wminfo)) ShowWindow(wminfo.window, SW_SHOW);
#endif
    if(autograbinput) setvar("grabinput", 1, true);
    capslockon = capslocked();
    numlockon = numlocked();
    ignoremousemotion();

    localconnect(false);
    resetfps();

    for(int frameloops = 0; ; frameloops = frameloops >= INT_MAX-1 ? MAXFPSHISTORY+1 : frameloops+1)
    {
        int elapsed = updatetimer(true);
        updatefps(frameloops, elapsed);
        checkinput();
        menuprocess();

        if(frameloops)
        {
            RUNWORLD("on_update");
            game::updateworld();
        }

        checksleep(lastmillis);
        serverslice();
        ircslice();
        if(frameloops)
        {
            game::recomputecamera(screen->w, screen->h);
            hud::update(screen->w, screen->h);
            setviewcell(camera1->o);
            updatetextures();
            updateparticles();
            updatesounds();
            UI::update();
            if(!minimized)
            {
                inbetweenframes = renderedframe = false;
                gl_drawframe(screen->w, screen->h);
                renderedframe = true;
                swapbuffers();
                inbetweenframes = true;
            }
            if(pixeling)
            {
                if(editmode)
                {
                    glReadPixels(screen->w/2, screen->h/2, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &pixel.v[0]);
                    if(pixelact) execute(pixelact);
                }
                if(pixelact) delete[] pixelact;
                pixelact = NULL;
                pixeling = false;
            }
            if(*progresstitle || progressamt > 0)
            {
                setsvar("progresstitle", "");
                setsvar("progresstext", "");
                setfvar("progressamt", 0.f);
                setfvar("progresspart", 0.f);
            }
            setcaption(game::gametitle(), game::gametext());
        }
    }

    ASSERT(0);
    return EXIT_FAILURE;

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
    } __except(stackdumper(0, GetExceptionInformation()), EXCEPTION_CONTINUE_SEARCH) { return 0; }
#endif
}
