// main.cpp: initialisation & main loop

#include "engine.h"
#include <signal.h>

string caption = "";

void setcaption(const char *text, const char *text2)
{
    static string prevtext = "", prevtext2 = "";
    if(strcmp(text, prevtext) || strcmp(text2, prevtext2))
    {
        copystring(prevtext, text);
        copystring(prevtext2, text2);
        formatstring(caption, "%s%s%s%s%s", getverstr(), text[0] ? ": " : "", text, text2[0] ? " - " : "", text2);
        if(screen) SDL_SetWindowTitle(screen, caption);
    }
}

int keyrepeatmask = 0, textinputmask = 0;
Uint32 textinputtime = 0;
VAR(0, textinputfilter, 0, 5, 1000);

void keyrepeat(bool on, int mask)
{
    if(on) keyrepeatmask |= mask;
    else keyrepeatmask &= ~mask;
}

void textinput(bool on, int mask)
{
    if(on)
    {
        if(!textinputmask)
        {
            SDL_StartTextInput();
            textinputtime = SDL_GetTicks();
        }
        textinputmask |= mask;
    }
    else
    {
        textinputmask &= ~mask;
        if(!textinputmask) SDL_StopTextInput();
    }
}

VARN(IDF_PERSIST, relativemouse, userelativemouse, 0, 1, 1);

bool shouldgrab = false, grabinput = false, canrelativemouse = true, relativemouse = false;

void inputgrab(bool on)
{
    if(on)
    {
        SDL_ShowCursor(SDL_FALSE);
        if(canrelativemouse && userelativemouse)
        {
            if(SDL_SetRelativeMouseMode(SDL_TRUE) >= 0)
            {
                SDL_SetWindowGrab(screen, SDL_TRUE);
                relativemouse = true;
            }
            else
            {
                SDL_SetWindowGrab(screen, SDL_FALSE);
                canrelativemouse = false;
                relativemouse = false;
            }
        }
    }
    else
    {
        SDL_ShowCursor(SDL_TRUE);
        if(relativemouse)
        {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_SetWindowGrab(screen, SDL_FALSE);
            relativemouse = false;
        }
    }
    shouldgrab = false;
}

extern void cleargamma();

void cleanup()
{
    recorder::stop();
    cleanupserver();
    SDL_ShowCursor(SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    if(screen) SDL_SetWindowGrab(screen, SDL_FALSE);
    cleargamma();
    freeocta(worldroot);
    UI::cleanup();
    cleanupwind();
    extern void clear_command(); clear_command();
    extern void clear_console(); clear_console();
    extern void clear_models();  clear_models();

    stopsound();
    #ifdef __APPLE__
        if(screen) SDL_SetWindowFullscreen(screen, 0);
    #endif
    SDL_Quit();
}

void quit()                  // normal exit
{
    inbetweenframes = false;
    initing = INIT_QUIT;
    writecfg("init.cfg", IDF_INIT);
    writeservercfg();
    if(!noconfigfile) writecfg("config.cfg", IDF_PERSIST);
    client::writecfg();
    abortconnect();
    disconnect(true);
    cleanup();
    exit(EXIT_SUCCESS);
}

volatile int errors = 0;
void fatal(const char *s, ...)    // failure exit
{
    if(!errors) initing = INIT_QUIT;
    if(++errors <= 2) // print up to one extra recursive error
    {
        defvformatbigstring(msg, s, s);
        if(logfile) logoutf("%s", msg);
#ifndef WIN32
        fprintf(stderr, "Fatal error: %s\n", msg);
#endif
        if(errors <= 1) // avoid recursion
        {
            if(SDL_WasInit(SDL_INIT_VIDEO))
            {
                SDL_ShowCursor(SDL_TRUE);
                SDL_SetRelativeMouseMode(SDL_FALSE);
                if(screen) SDL_SetWindowGrab(screen, SDL_FALSE);
                cleargamma();
#ifdef __APPLE__
                if(screen) SDL_SetWindowFullscreen(screen, 0);
#endif
            }
            SDL_Quit();
            defformatstring(cap, "%s: Fatal error", versionfname);
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, cap, msg, NULL);
        }
    }
    exit(EXIT_FAILURE);
}

VAR(IDF_READONLY, desktopw, 1, 0, 0);
VAR(IDF_READONLY, desktoph, 1, 0, 0);
int screenw = 0, screenh = 0, refresh = 60;
SDL_Window *screen = NULL;
SDL_GLContext glcontext = NULL;
SDL_DisplayMode display;

int initing = NOT_INITING;
bool initwarning(const char *desc, int level, int type)
{
    if(initing < level)
    {
        addchange(desc, type);
        return true;
    }
    return false;
}

VAR(IDF_PERSIST, compresslevel, 0, 9, 9);
VAR(IDF_PERSIST, imageformat, IFMT_NONE+1, IFMT_PNG, IFMT_MAX-1);

void screenshot(char *sname)
{
    ImageData image(renderw, renderh, 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, renderw, renderh, GL_RGB, GL_UNSIGNED_BYTE, image.data);
    string fname;
    if(sname && *sname) copystring(fname, sname);
    else formatstring(fname, "screenshots/%s", *filetimeformat ? gettime(filetimelocal ? currenttime : clocktime, filetimeformat) : (*mapname ? mapname : "screen"));
    saveimage(fname, image, imageformat, compresslevel, true);
}

ICOMMAND(0, screenshot, "s", (char *s), if(!(identflags&IDF_WORLD)) screenshot(s));
ICOMMAND(0, quit, "", (void), if(!(identflags&IDF_WORLD)) quit());

#define SCR_MINW 320
#define SCR_MINH 200
#define SCR_MAXW 10000
#define SCR_MAXH 10000
#define SCR_DEFAULTW 1024
#define SCR_DEFAULTH 768

VARFN(IDF_INIT, screenw, scr_w, SCR_MINW, -1, SCR_MAXW, initwarning("screen resolution"));
VARFN(IDF_INIT, screenh, scr_h, SCR_MINH, -1, SCR_MAXH, initwarning("screen resolution"));
bool initwindowpos = false, wantdisplaysetup = false;

void setupdisplay(bool msg = true)
{
    SDL_GetWindowSize(screen, &screenw, &screenh);
    SDL_GL_GetDrawableSize(screen, &renderw, &renderh);
    scr_w = screenw;
    scr_h = screenh;
    hudw = renderw;
    hudh = renderh;
    gl_resize();

    int index = SDL_GetWindowDisplayIndex(screen);
    if(SDL_GetCurrentDisplayMode(index, &display) < 0)
    {
        if(msg) conoutf("Failed querying monitor %d display mode: %s", index, SDL_GetError());
        refresh = 60; // sane-ish default
    }
    else
    {
        if(msg) conoutf("Display [%d]: %dx%d [%d Hz] %s: %dx%d, Renderer: %dx%d", index, display.w, display.h, display.refresh_rate, SDL_GetWindowFlags(screen)&SDL_WINDOW_FULLSCREEN ? (fullscreendesktop ? "FS-Desktop" : "FullScreen") : "Windowed", screenw, screenh, renderw, renderh);
        refresh = display.refresh_rate;
    }
    wantdisplaysetup = false;
}

void setfullscreen(bool enable)
{
    if(!screen) return;
    //initwarning(enable ? "fullscreen" : "windowed");
    SDL_SetWindowFullscreen(screen, enable ? (fullscreendesktop ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN) : 0);
    if(!enable)
    {
        SDL_SetWindowSize(screen, scr_w, scr_h);
        if(initwindowpos)
        {
            int winx = SDL_WINDOWPOS_CENTERED, winy = SDL_WINDOWPOS_CENTERED;
            SDL_SetWindowPosition(screen, winx, winy);
            initwindowpos = false;
        }
    }
    wantdisplaysetup = true;
}

VARF(IDF_INIT, fullscreen, 0, 1, 1, if(!(identflags&IDF_WORLD)) setfullscreen(fullscreen!=0));

void resetfullscreen()
{
    setfullscreen(false);
    setfullscreen(true);
}

VARF(IDF_INIT, fullscreendesktop, 0, 1, 1, if(!(identflags&IDF_WORLD) && fullscreen) resetfullscreen());

void screenres(int w, int h)
{
    scr_w = clamp(w, SCR_MINW, SCR_MAXW);
    scr_h = clamp(h, SCR_MINH, SCR_MAXH);
    if(screen)
    {
        if(fullscreendesktop)
        {
            scr_w = min(scr_w, desktopw);
            scr_h = min(scr_h, desktoph);
        }
        if(SDL_GetWindowFlags(screen)&SDL_WINDOW_FULLSCREEN) resetfullscreen();
        else SDL_SetWindowSize(screen, scr_w, scr_h);
        wantdisplaysetup = true;
    }
    else initwarning("screen resolution");
}

ICOMMAND(0, screenres, "ii", (int *w, int *h), screenres(*w, *h));

static void setgamma(int val)
{
    if(screen && SDL_SetWindowBrightness(screen, val/100.0f) < 0) conoutf("\frCould not set gamma: %s", SDL_GetError());
}

static int curgamma = 100;
VARFN(IDF_PERSIST, gamma, reqgamma, 30, 100, 300,
{
    if(initing || reqgamma == curgamma) return;
    curgamma = reqgamma;
    setgamma(curgamma);
});

void restoregamma()
{
    if(initing || curgamma == 100) return;
    setgamma(curgamma);
}

void cleargamma()
{
    if(curgamma != 100 && screen) SDL_SetWindowBrightness(screen, 1.0f);
}

int curvsync = -1;
void restorevsync()
{
    if(initing || !glcontext) return;
    extern int vsync, vsynctear;
    if(!SDL_GL_SetSwapInterval(vsync ? (vsynctear ? -1 : 1) : 0)) curvsync = vsync;
    else if(vsync && vsynctear && !SDL_GL_SetSwapInterval(vsync ? 1 : 0)) curvsync = vsync;
}

VARF(IDF_PERSIST, vsync, 0, 0, 1, restorevsync());
VARF(IDF_PERSIST, vsynctear, 0, 1, 1, { if(vsync) restorevsync(); });

void setupscreen()
{
    if(glcontext)
    {
        SDL_GL_DeleteContext(glcontext);
        glcontext = NULL;
    }
    if(screen)
    {
        SDL_DestroyWindow(screen);
        screen = NULL;
    }
    curvsync = -1;

    SDL_Rect desktop;
    if(SDL_GetDisplayBounds(0, &desktop) < 0) fatal("Failed querying desktop bounds: %s", SDL_GetError());
    desktopw = desktop.w;
    desktoph = desktop.h;

    if(scr_h < 0) scr_h = fullscreen ? desktoph : SCR_DEFAULTH;
    if(scr_w < 0) scr_w = (scr_h*desktopw)/desktoph;
    scr_w = clamp(scr_w, SCR_MINW, SCR_MAXW);
    scr_h = clamp(scr_h, SCR_MINH, SCR_MAXH);
    if(fullscreendesktop)
    {
        scr_w = min(scr_w, desktopw);
        scr_h = min(scr_h, desktoph);
    }

    int winx = SDL_WINDOWPOS_UNDEFINED, winy = SDL_WINDOWPOS_UNDEFINED, winw = scr_w, winh = scr_h,
        flags = SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_INPUT_FOCUS|SDL_WINDOW_MOUSE_FOCUS|SDL_WINDOW_RESIZABLE;//|SDL_WINDOW_ALLOW_HIGHDPI;
    if(fullscreen)
    {
        if(fullscreendesktop)
        {
            winw = desktopw;
            winh = desktoph;
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        else flags |= SDL_WINDOW_FULLSCREEN;
        initwindowpos = true;
    }

    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    screen = SDL_CreateWindow(caption, winx, winy, winw, winh, flags);
    if(!screen) fatal("Failed to create OpenGL window: %s", SDL_GetError());
    SDL_Surface *s = loadsurface("textures/icon");
    if(s)
    {
        SDL_SetWindowIcon(screen, s);
        SDL_FreeSurface(s);
    }

    SDL_SetWindowMinimumSize(screen, SCR_MINW, SCR_MINH);
    SDL_SetWindowMaximumSize(screen, SCR_MAXW, SCR_MAXH);

#ifdef __APPLE__
    static const int glversions[] = { 32, 20 };
#else
    static const int glversions[] = { 40, 33, 32, 31, 30, 20 };
#endif
    loopi(sizeof(glversions)/sizeof(glversions[0]))
    {
        glcompat = glversions[i] <= 30 ? 1 : 0;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glversions[i] / 10);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glversions[i] % 10);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, glversions[i] >= 32 ? SDL_GL_CONTEXT_PROFILE_CORE : 0);
        glcontext = SDL_GL_CreateContext(screen);
        if(glcontext) break;
    }
    if(!glcontext) fatal("Failed to create OpenGL context: %s", SDL_GetError());
    setupdisplay(false);
}

void resetgl()
{
    clearchanges(CHANGE_GFX|CHANGE_SHADERS);

    progress(0, "Resetting OpenGL..");

    recorder::cleanup();
    cleanupva();
    cleanupparticles();
    cleanupstains();
    cleanupsky();
    cleanupmodels();
    cleanupprefabs();
    cleanuptextures();
    cleanupblendmap();
    cleanuplights();
    cleanupshaders();
    cleanupgl();

    setupscreen();
    inputgrab(grabinput);
    gl_init();

    inbetweenframes = false;
    if(!reloadtexture(notexturetex) || !reloadtexture(blanktex) || !reloadtexture(logotex))
        fatal("Failed to reload core textures");
    reloadfonts();
    inbetweenframes = true;
    progress(0, "Initializing...");
    restoregamma();
    restorevsync();
    initgbuffer();
    reloadshaders();
    reloadtextures();
    allchanged(true);
}

ICOMMAND(0, resetgl, "", (void), if(!(identflags&IDF_WORLD)) resetgl());

bool warping = false, minimized = false;

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
            if(grabinput && !relativemouse && !(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
            {
                if(warping && event.motion.x == screenw / 2 && event.motion.y == screenh / 2)
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
    while(SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEMOTION));
}

void resetcursor(bool warp, bool reset)
{
    if(warp && grabinput && !(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
    {
        SDL_WarpMouseInWindow(screen, screenw/2, screenh/2);
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
    //int lasttype = 0, lastbut = 0;
    bool mousemoved = false, shouldwarp = false;
    while(events.length() || pollevent(event))
    {
        if(events.length()) event = events.remove(0);

        switch(event.type)
        {
            case SDL_QUIT:
                quit();
                return;

            case SDL_TEXTINPUT:
                if(textinputmask && int(event.text.timestamp-textinputtime) >= textinputfilter)
                {
                    uchar buf[SDL_TEXTINPUTEVENT_TEXT_SIZE+1];
                    size_t len = decodeutf8(buf, sizeof(buf)-1, (const uchar *)event.text.text, strlen(event.text.text));
                    if(len > 0) { buf[len] = '\0'; processtextinput((const char *)buf, len); }
                }
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                if(keyrepeatmask || !event.key.repeat)
                    processkey(event.key.keysym.sym, event.key.state==SDL_PRESSED);
                break;

            case SDL_WINDOWEVENT:
                switch(event.window.event)
                {
                    case SDL_WINDOWEVENT_CLOSE:
                        quit();
                        break;

                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        shouldgrab = true;
                        break;
                    case SDL_WINDOWEVENT_ENTER:
                        inputgrab(grabinput = true);
                        break;

                    case SDL_WINDOWEVENT_LEAVE:
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        inputgrab(grabinput = false);
                        break;

                    case SDL_WINDOWEVENT_MINIMIZED:
                        minimized = true;
                        break;

                    case SDL_WINDOWEVENT_MAXIMIZED:
                    case SDL_WINDOWEVENT_RESTORED:
                        minimized = false;
                        break;

                    case SDL_WINDOWEVENT_RESIZED:
                        break;

                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                    {
                        wantdisplaysetup = true;
                        break;
                    }
                }
                break;

            case SDL_MOUSEMOTION:
                if(grabinput)
                {
                    int dx = event.motion.xrel, dy = event.motion.yrel;
                    checkmousemotion(dx, dy);
                    shouldwarp = game::mousemove(dx, dy, event.motion.x, event.motion.y, screenw, screenh); // whether game controls engine cursor
                    mousemoved = true;
                }
                else if(shouldgrab) inputgrab(grabinput = true);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                //if(lasttype==event.type && lastbut==event.button.button) break; // why?? get event twice without it
                //switch(event.button.button)
                //{
                //    case SDL_BUTTON_LEFT: processkey(-1, event.button.state==SDL_PRESSED); break;
                //    case SDL_BUTTON_MIDDLE: processkey(-2, event.button.state==SDL_PRESSED); break;
                //    case SDL_BUTTON_RIGHT: processkey(-3, event.button.state==SDL_PRESSED); break;
                //    case SDL_BUTTON_X1: processkey(-6, event.button.state==SDL_PRESSED); break;
                //    case SDL_BUTTON_X2: processkey(-7, event.button.state==SDL_PRESSED); break;
                //}
                //lasttype = event.type;
                //lastbut = event.button.button;
                int button = event.button.button;
                if(button >= 4) button += 2;
                processkey(-button, event.button.state==SDL_PRESSED);
                break;
            }
            case SDL_MOUSEWHEEL:
                if(event.wheel.y > 0) { processkey(-4, true); processkey(-4, false); }
                else if(event.wheel.y < 0) { processkey(-5, true); processkey(-5, false); }
                break;
        }
    }
    if(mousemoved)
    {
        warping = false;
        if(grabinput && shouldwarp) resetcursor(true, false);
    }
}

void swapbuffers(bool overlay)
{
    recorder::capture(overlay);
    gle::disable();
    SDL_GL_SwapWindow(screen);
}

VAR(IDF_PERSIST, menufps, -1, 60, 1000);
FVAR(IDF_PERSIST, menufpsrefresh, FVAR_NONZERO, 1, FVAR_MAX);
VAR(IDF_PERSIST, maxfps, -1, -1, 1000);
FVAR(IDF_PERSIST, maxfpsrefresh, FVAR_NONZERO, 2, FVAR_MAX);

#define GETFPS(a) (a >= 0 ? a : int(ceilf(refresh*a##refresh)))

void limitfps(int &millis, int curmillis)
{
    int curmax = GETFPS(maxfps), curmenu = GETFPS(menufps),
        limit = (hasnoview() || minimized) && curmenu ? (curmax > 0 ? min(curmax, curmenu) : curmenu) : curmax;
    if(!limit || (limit == refresh && vsync)) return;
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

#ifdef WIN32
// Force Optimus setups to use the NVIDIA GPU
extern "C"
{
#ifdef __GNUC__
__attribute__((dllexport))
#else
__declspec(dllexport)
#endif
    DWORD NvOptimusEnablement = 1;

#ifdef __GNUC__
__attribute__((dllexport))
#else
__declspec(dllexport)
#endif
    DWORD AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
void stackdumper(unsigned int type, EXCEPTION_POINTERS *ep)
{
    if(!ep) fatal("Unknown type");
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *context = ep->ContextRecord;
    bigstring out;
    formatstring(out, "%s Win32 Exception: 0x%x [0x%x]\n\n", versionfname, er->ExceptionCode, er->ExceptionCode==EXCEPTION_ACCESS_VIOLATION ? er->ExceptionInformation[1] : -1);
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
            concformatstring(out, "%s - %s [%d]\n", sym.Name, del ? del + 1 : line.FileName, line.LineNumber);
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

#define ENGINEBOOL(name,val) \
    bool name = val; \
    ICOMMAND(0, get##name, "", (), intret(name ? 1 : 0));
ENGINEBOOL(engineready, false);
ENGINEBOOL(inbetweenframes, false);
ENGINEBOOL(renderedframe, true);

static bool findarg(int argc, char **argv, const char *str)
{
    for(int i = 1; i<argc; i++) if(strstr(argv[i], str)==argv[i]) return true;
    return false;
}

bool progressing = false;
bool checkconn()
{
    if(!curpeer) return connpeer != NULL;
    else return client::waiting() > 0;
    return false;
}
ICOMMAND(0, getprogresswait, "", (), intret(client::waiting()));
ICOMMAND(0, getprogressing, "", (), intret(progressing || checkconn() ? 1 : 0));
ICOMMAND(0, getprogresstype, "", (), intret(maploading ? 1 : (checkconn() ? 2 : 0)));
FVAR(0, loadprogress, 0, 0, 1);
SVAR(0, progresstitle, "");
FVAR(0, progressamt, -1, 0, 1);
VAR(IDF_PERSIST, progressfps, -1, -1, VAR_MAX);
int lastprogress = 0;

void progress(float amt, const char *s, ...)
{
    if(progressing || !inbetweenframes || drawtex) return;
    if(amt < 0) // signals the start of a long process, hide the UI
    {
        if(engineready) UI::hideui(NULL);
        amt = 0;
    }
    if(progressfps)
    {
        int curprog = progressfps >= 0 ? progressfps : refresh, ticks = SDL_GetTicks(), diff = ticks - lastprogress;
        if(amt > 0 && diff >= 0 && diff < (1000 + curprog-1)/curprog) return;
        lastprogress = ticks;
    }
    clientkeepalive();

    interceptkey(SDLK_UNKNOWN); // keep the event queue awake to avoid appearing unresponsive

    string sf;
    if(s != NULL)
    {
        va_list args;
        va_start(args, s);
        vformatstring(sf, s, args);
        va_end(args);
    }
    else copystring(sf, "Loading...");
    setsvar("progresstitle", sf);
    setfvar("progressamt", amt);
    if(verbose >= 4) conoutf("%s [%.2f%%]", sf, amt*100.f);

    int oldflags = identflags;
    identflags &= ~IDF_WORLD;
    progressing = true;
    if(engineready) UI::update();
    gl_drawnoview();
    swapbuffers(false);
    progressing = false;
    identflags = oldflags;
}


bool pixeling = false;
bvec pixel(0, 0, 0);
char *pixelact = NULL;

ICOMMAND(0, printpixel, "", (void), conoutft(CON_MESG, "Pixel = 0x%.6X (%d, %d, %d)", pixel.tohexcolor(), pixel.r, pixel.g, pixel.b));
ICOMMAND(0, getpixel, "i", (int *n),
{
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
    if(!editmode) { conoutf("\frOperation only allowed in edit mode"); return; }
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
    setlocations(argv[0]);

    char *initscript = NULL;
    initing = INIT_RESET;

    // URI support
    // *://password@hostname:port
    // *://hostname:port
    // *://hostname
    const char reprotoprefix[] = VERSION_UNAME "://";
    const int reprotolen = sizeof(reprotoprefix) - 1;
    char *reprotoarg = NULL, *connectstr = NULL, *connectpassword = NULL, *connecthost = NULL;
    int connectport = SERVER_PORT;

    // try to parse home directory argument
    // (has to be parsed first to be able to set the logfile path correctly)
    for(int i = 1; i < argc; i++)
    {
        if(argv[i][0] == '-') switch(argv[i][1])
        {
            case 'h': serveroption(argv[i]); break;
        }
    }
    setlogfile("log.txt");
    execfile("init.cfg", false);

    for(int i = 1; i < argc; i++)
    { // parse the rest of the arguments
        if(argv[i][0] == '-') switch(argv[i][1])
        { // switches that can modify both server and client behavior
            case 'h': /* parsed first */ break;
            case 'r': /* compat, ignore */ break;
            case 'd':
            {
                switch(argv[i][2])
                {
                    case 'w': scr_w = clamp(atoi(&argv[i][3]), SCR_MINW, SCR_MAXW); if(!findarg(argc, argv, "-dh")) scr_h = -1; break;
                    case 'h': scr_h = clamp(atoi(&argv[i][3]), SCR_MINH, SCR_MAXH); if(!findarg(argc, argv, "-dw")) scr_w = -1; break;
                    case 'd': /* compat, ignore */ break;
                    case 'c': /* compat, ignore */ break;
                    case 'a': /* compat, ignore */ break;
                    case 'v': /* compat, ignore */ break;
                    case 'f': fullscreen = atoi(&argv[i][3]); break;
                    case 's': /* compat, ignore */ break;
                    case 'u': /* compat, ignore */ break;
                    default: conoutf("\frUnknown display option: '%c'", argv[i][2]); break;
                }
                break;
            }
            case 'x': initscript = &argv[i][2]; break;
            default:
                if(!serveroption(argv[i])) gameargs.add(argv[i]);
                break;
        }
        else if(!strncmp(argv[i], reprotoprefix, reprotolen) && !reprotoarg)
        { // will only parse the first argument that is possibly a *:// URL argument and ignore any following
            reprotoarg = newstring(argv[i]);
            connectstr = newstring(reprotoarg + reprotolen);
            if(!*connectstr) continue; // check if there's actually text after the protocol prefix
            char *slashchr = strchr(connectstr, '/'); // skip trailing slashes (if any)
            if(slashchr) *slashchr = '\0';
            connecthost = strchr(connectstr, '@');
            if(connecthost)
            {
                connectpassword = connectstr;
                *connecthost++ = '\0';
            }
            else connecthost = connectstr;
            char *portbuf = strchr(connecthost, ':');
            if(portbuf)
            {
                *portbuf++ = '\0';
                connectport = parseint(portbuf);
                if(!connectport) connectport = SERVER_PORT;
            }
        }
        else gameargs.add(argv[i]);
    }

    initing = NOT_INITING;

    numcpus = clamp(SDL_GetCPUCount(), 1, 16);

    conoutf("Loading SDL..");
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_AUDIO)<0) fatal("Unable to initialize SDL: %s", SDL_GetError());
    setcaption("Loading, please wait..");

    conoutf("Loading eNet..");
    if(enet_initialize()<0) fatal("Unable to initialise network module");
    atexit(enet_deinitialize);
    enet_time_set(0);

    conoutf("Loading game..");
    bool shouldload = initgame();

    //#ifdef WIN32
    //SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    //#endif

    conoutf("Loading video..");
    SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "0");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "0");
    #if !defined(WIN32) && !defined(__APPLE__)
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    #endif

    setupscreen();
    SDL_ShowCursor(SDL_FALSE);
    SDL_StopTextInput(); // workaround for spurious text-input events getting sent on first text input toggle?

    signal(SIGINT, fatalsignal);
    signal(SIGILL, fatalsignal);
    signal(SIGABRT, fatalsignal);
    signal(SIGFPE, fatalsignal);
    signal(SIGSEGV, fatalsignal);
    signal(SIGTERM, shutdownsignal);
#if !defined(WIN32) && !defined(__APPLE__)
    signal(SIGHUP, reloadsignal);
    signal(SIGQUIT, fatalsignal);
    signal(SIGKILL, fatalsignal);
    signal(SIGPIPE, fatalsignal);
    signal(SIGALRM, fatalsignal);
#endif

    conoutf("Loading GL..");
    gl_checkextensions();
    gl_init();
    if(!(notexture = textureload(notexturetex)) || !(blanktexture = textureload(blanktex)))
        fatal("Could not find core textures");

    conoutf("Loading sound..");
    initsound();

    game::start();

    conoutf("Loading defaults..");
    if(!execfile("config/stdlib.cfg", false)) fatal("Cannot find data files");
    if(!setfont("default")) fatal("No default font specified");

    UI::setup();

    inbetweenframes = true;
    progress(0, "Please wait..");

    conoutf("Loading world..");
    progress(0, "Loading world..");
    setupwind();
    emptymap(0, true, NULL, false);

    conoutf("Loading config..");
    progress(0, "Loading config..");
    initing = INIT_LOAD;
    rehash(false);
    if(shouldload) smartmusic(true, true);

    initing = NOT_INITING;

    if(shouldload)
    {
        conoutf("Loading required data..");
        progress(0, "Loading required data..");
        restoregamma();
        restorevsync();
        initgbuffer();
        loadshaders();
        preloadtextures();
        initparticles();
        initstains();

        conoutf("Loading main..");
        progress(0, "Loading main..");

        capslockon = capslocked();
        numlockon = numlocked();
        ignoremousemotion();

        localconnect(false);
        resetfps();

        engineready = true;
        hud::checkui();
        if(reprotoarg)
        {
            if(connecthost && *connecthost) connectserv(connecthost, connectport, connectpassword);
            else conoutf("\frMalformed commandline argument: %s", reprotoarg);
        }

        // housekeeping
        if(connectstr)
        {
            delete[] connectstr;
            connectstr = NULL;
        }
        if(reprotoarg)
        {
            delete[] reprotoarg;
            reprotoarg = NULL;
        }
        if(initscript) execute(initscript, true);

        for(int frameloops = 0; ; frameloops = frameloops >= INT_MAX-1 ? MAXFPSHISTORY+1 : frameloops+1)
        {
            if(wantdisplaysetup) setupdisplay();
            curtextscale = textscale;
            int elapsed = updatetimer(true);
            updatefps(frameloops, elapsed);
            cdpi::runframe();
            checkinput();
            hud::checkui();
            tryedit();

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
                game::recomputecamera();
                setviewcell(camera1->o);
                updatetextures();
                updateparticles();
                updatesounds();
                if(!minimized)
                {
                    inbetweenframes = renderedframe = false;
                    gl_drawframe();
                    renderedframe = true;
                    swapbuffers();
                    inbetweenframes = true;
                    if(pixeling)
                    {
                        if(editmode)
                        {
                            glReadPixels(screenw/2, screenh/2, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &pixel.v[0]);
                            if(pixelact) execute(pixelact);
                        }
                        if(pixelact) delete[] pixelact;
                        pixelact = NULL;
                        pixeling = false;
                    }
                }
                if(*progresstitle || progressamt >= 0)
                {
                    setsvar("progresstitle", "");
                    setfvar("progressamt", -1.f);
                }
                setcaption(game::gametitle(), game::gametext());
            }
        }
    }

    ASSERT(0);
    return EXIT_FAILURE;

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
    } __except(stackdumper(0, GetExceptionInformation()), EXCEPTION_CONTINUE_SEARCH) { return 0; }
#endif
}
