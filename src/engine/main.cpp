// main.cpp: initialisation & main loop

#include "engine.h"
#include <signal.h>

#ifdef SDL_VIDEO_DRIVER_X11
#include "SDL_syswm.h"
#endif

void getsdlversion_compiled()
{
    SDL_version compiled;
    SDL_VERSION(&compiled);
    defformatstring(str, "%u.%u.%u", compiled.major, compiled.minor, compiled.patch);
    result(str);
}
COMMAND(0, getsdlversion_compiled, "");

void getsdlversion_linked()
{
    SDL_version linked;
    SDL_GetVersion(&linked);
    defformatstring(str, "%u.%u.%u", linked.major, linked.minor, linked.patch);
    result(str);
}
COMMAND(0, getsdlversion_linked, "");

#ifndef STANDALONE
#include "SDL_image.h"

void getsdlimgversion_compiled()
{
    SDL_version compiled;
    SDL_IMAGE_VERSION(&compiled);
    defformatstring(str, "%u.%u.%u", compiled.major, compiled.minor, compiled.patch);
    result(str);
}
COMMAND(0, getsdlimgversion_compiled, "");

void getsdlimgversion_linked()
{
    SDL_version linked;
    const SDL_version *version = IMG_Linked_Version();
    SDL_VERSION(&linked);
    defformatstring(str, "%u.%u.%u", version->major, version->minor, version->patch);
    result(str);
}
COMMAND(0, getsdlimgversion_linked, "");

#endif // STANDALONE

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

#ifdef DEBUG_UTILS
void writetofile(const char *filename, const char *buf)
{
    stream *f = openutf8file(filename, "w");
    if(!f)
    {
        intret(0);
        return;
    }
    f->write(buf, strlen(buf));
    delete f;
    intret(1);
}
COMMAND(0, writetofile, "ss");
#endif

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
            textinputtime = getclockticks();
        }
        textinputmask |= mask;
    }
    else if(textinputmask)
    {
        textinputmask &= ~mask;
        if(!textinputmask) SDL_StopTextInput();
    }
}

#ifdef WIN32
// SDL_WarpMouseInWindow behaves erratically on Windows, so force relative mouse instead.
VARN(IDF_READONLY, relativemouse, userelativemouse, 1, 1, 0);
#else
VARN(IDF_PERSIST, relativemouse, userelativemouse, 0, 1, 1);
#endif

bool windowfocus = true, shouldgrab = false, grabinput = false, canrelativemouse = true, relativemouse = false;

#ifdef SDL_VIDEO_DRIVER_X11
VAR(0, sdl_xgrab_bug, 0, 0, 1);
#endif

void inputgrab(bool on, bool delay = false)
{
#ifdef SDL_VIDEO_DRIVER_X11
    bool wasrelativemouse = relativemouse;
#endif
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
            SDL_SetWindowGrab(screen, SDL_FALSE);
            SDL_SetRelativeMouseMode(SDL_FALSE);
            relativemouse = false;
        }
    }
    shouldgrab = false;

#ifdef SDL_VIDEO_DRIVER_X11
    if((relativemouse || wasrelativemouse) && sdl_xgrab_bug)
    {
        // Workaround for buggy SDL X11 pointer grabbing
        union { SDL_SysWMinfo info; uchar buf[sizeof(SDL_SysWMinfo) + 128]; };
        SDL_GetVersion(&info.version);
        if(SDL_GetWindowWMInfo(screen, &info) && info.subsystem == SDL_SYSWM_X11)
        {
            if(relativemouse)
            {
                uint mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask;
                XGrabPointer(info.info.x11.display, info.info.x11.window, True, mask, GrabModeAsync, GrabModeAsync, info.info.x11.window, None, CurrentTime);
            }
            else XUngrabPointer(info.info.x11.display, CurrentTime);
        }
    }
#endif
}

extern void cleargamma();
bool engineready = false, inbetweenframes = false, renderedframe = false;

void cleanup()
{
    engineready = false;
    cleanupserver();
    SDL_ShowCursor(SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    if(screen) SDL_SetWindowGrab(screen, SDL_FALSE);
    cleargamma();

    freeocta(worldroot);
    UI::cleanup();
    fx::cleanup();
    cleanupwind();
    extern void clear_command(); clear_command();
    extern void clear_binds(); clear_binds();
    extern void clear_models(); clear_models();

    stopsound();
    SDL_Quit();
}

void quit()                  // normal exit
{
    inbetweenframes = engineready = false;
    initing = INIT_QUIT;
    writecfg("init.cfg", IDF_INIT);
    writeservercfg();
    if(!noconfigfile) writecfg("config.cfg", IDF_PERSIST);
    writehistory();
    client::writecfg();
    abortconnect();
    disconnect(true);
    cleanup();
    exit(EXIT_SUCCESS);
}

volatile int errors = 0;
void fatal(const char *s, ...)    // failure exit
{
    engineready = false;
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
            }
            SDL_Quit();
            defformatstring(cap, "%s: Fatal error", versionfname);
#ifdef WIN32 // bug: https://github.com/libsdl-org/SDL/issues/1380
            MessageBox(NULL, msg, cap, MB_OK|MB_SYSTEMMODAL);
#else
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, cap, msg, screen);
#endif
        }
    }
    exit(EXIT_FAILURE);
}

int screenw = 0, screenh = 0;
VARR(desktopw, 0);
VARR(desktoph, 0);
VARR(refreshrate, 0);

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

ICOMMAND(0, screenshot, "s", (char *s), if(!(identflags&IDF_MAP)) screenshot(s));
ICOMMAND(IDF_NOECHO, quit, "", (void), if(!(identflags&IDF_MAP)) quit());

#define SCR_MINW 320
#define SCR_MINH 200
#define SCR_MAXW 10000
#define SCR_MAXH 10000
#define SCR_DEFAULTW 1024
#define SCR_DEFAULTH 768

VARFN(IDF_INIT, screenw, scr_w, SCR_MINW, -1, SCR_MAXW, initwarning("screen resolution"));
VARFN(IDF_INIT, screenh, scr_h, SCR_MINH, -1, SCR_MAXH, initwarning("screen resolution"));
bool wantdisplaysetup = false;

void resetfullscreen();

int getdisplaymode()
{
    int index = SDL_GetWindowDisplayIndex(screen);
    if(SDL_GetCurrentDisplayMode(index, &display) < 0) fatal("Failed querying monitor %d display mode: %s", index, SDL_GetError());
    desktopw = display.w;
    desktoph = display.h;
    refreshrate = display.refresh_rate;
    return index;
}

void setupdisplay(bool dogl = true, bool msg = true)
{
    SDL_GetWindowSize(screen, &screenw, &screenh);
    SDL_GL_GetDrawableSize(screen, &renderw, &renderh);

    int index = getdisplaymode();
    if(windowfocus && SDL_GetWindowFlags(screen)&SDL_WINDOW_FULLSCREEN && (display.w != screenw || display.h != screenh))
    {
        scr_w = clamp(display.w, SCR_MINW, SCR_MAXW);
        scr_h = clamp(display.h, SCR_MINH, SCR_MAXH);
        resetfullscreen();
        SDL_GetWindowSize(screen, &screenw, &screenh);
        SDL_GL_GetDrawableSize(screen, &renderw, &renderh);
    }
    scr_w = screenw;
    scr_h = screenh;
    hudw = renderw;
    hudh = renderh;
    if(dogl) gl_resize();

    if(msg) conoutf(colourwhite, "Display [%d]: %dx%d [%d Hz] %s: %dx%d [%dx%d]", index, display.w, display.h, display.refresh_rate, SDL_GetWindowFlags(screen)&SDL_WINDOW_FULLSCREEN ? (fullscreendesktop ? "Fullscreen" : "Exclusive") : "Windowed", screenw, screenh, renderw, renderh);

    wantdisplaysetup = false;

    triggereventcallbacks(CMD_EVENT_SETUPDISPLAY);
}

void setfullscreen(bool enable)
{
    if(!screen) return;
    SDL_SetWindowFullscreen(screen, enable ? (fullscreendesktop ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN) : 0);
    if(!enable)
    {
        SDL_SetWindowSize(screen, scr_w, scr_h);
        SDL_SetWindowPosition(screen, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    wantdisplaysetup = true;
}

VARF(IDF_INIT, fullscreen, 0, 1, 1, if(!(identflags&IDF_MAP)) setfullscreen(fullscreen!=0));

void resetfullscreen()
{
    setfullscreen(false);
    setfullscreen(true);
}

VARF(IDF_INIT, fullscreendesktop, 0, 1, 1, if(!(identflags&IDF_MAP) && fullscreen) resetfullscreen());

void screenres(int w, int h)
{
    scr_w = clamp(w, SCR_MINW, SCR_MAXW);
    scr_h = clamp(h, SCR_MINH, SCR_MAXH);
    if(screen)
    {
        if(fullscreendesktop)
        {
            getdisplaymode();
            scr_w = min(scr_w, desktopw);
            scr_h = min(scr_h, desktoph);
        }
        if(SDL_GetWindowFlags(screen)&SDL_WINDOW_FULLSCREEN) resetfullscreen();
        else
        {
            SDL_SetWindowSize(screen, scr_w, scr_h);
            SDL_SetWindowPosition(screen, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }
        wantdisplaysetup = true;
    }
    else initwarning("screen resolution");
}

ICOMMAND(0, screenres, "ii", (int *w, int *h), screenres(*w, *h));

static void setgamma(int val)
{
    if(screen && SDL_SetWindowBrightness(screen, val/100.0f) < 0) conoutf(colourred, "Could not set gamma: %s", SDL_GetError());
}

ICOMMAND(0, enumresolutions, "", (),
{
    if(!screen) return;
    int index = SDL_GetWindowDisplayIndex(screen);
    int modes = SDL_GetNumDisplayModes(index);
    if(modes <= 0) return;

    vector<int> resolutions;

    // Fill list with unique resolutions
    loopi(modes)
    {
        SDL_DisplayMode mode;
        if(SDL_GetDisplayMode(index, i, &mode)) continue;

        // Pack resolution into a single int
        int res = mode.w | (mode.h << 16);

        // Add to list if not already present
        if(resolutions.find(res) < 0) resolutions.add(res);
    }

    // Make a list of resolutions
    string reslist;
    reslist[0] = 0;

    loopvrev(resolutions)
    {
        int res = resolutions[i];
        int w = res & 0xFFFF;
        int h = res >> 16;

        if(reslist[0]) concatstring(reslist, " ");
        concatstring(reslist, intstr(w));
        concatstring(reslist, "x");
        concatstring(reslist, intstr(h));
    }

    result(reslist);
});

static int curgamma = 100;
VARFN(IDF_PERSIST, gamma, reqgamma, 30, 100, 300,
{
    if(initing || reqgamma == curgamma) return;
    curgamma = reqgamma;
    setgamma(curgamma);
});

void restoregamma()
{
    if(initing || reqgamma == 100) return;
    curgamma = reqgamma;
    setgamma(curgamma);
}

void cleargamma()
{
    if(curgamma != 100 && screen) SDL_SetWindowBrightness(screen, 1.0f);
}

VARR(hasvsynctear, -1);

int curvsync = -1;
void restorevsync()
{
    if(initing || !glcontext) return;
    extern int vsync, vsynctear;
    int err = 0;

    if(hasvsynctear < 0 || (vsync && vsynctear))
    {
        err = SDL_GL_SetSwapInterval(-1);
        hasvsynctear = err ? 0 : 1;
    }

    if(err || !vsynctear || !vsync)
    {
        err = SDL_GL_SetSwapInterval(vsync);
    }

    if(!err) curvsync = vsync;
}

VARF(IDF_PERSIST, vsync, 0, 0, 1, restorevsync());
VARF(IDF_PERSIST, vsynctear, 0, 1, 1, { if(vsync) restorevsync(); });

void setupscreen(bool dogl = true)
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
    }

    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#ifndef WIN32
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
#endif
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

    static const int glversions[] = { 40, 33, 32, 31, 30, 20 };
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
    setupdisplay(dogl, engineready);
}

void resetgl()
{
    clearchanges(CHANGE_GFX|CHANGE_SHADERS);
    progress(0, "Resetting OpenGL..");

    bool oldengineready = engineready;
    engineready = false;

    UI::cleangl();
    game::cleangl();
    cleanupva();
    cleanupparticles();
    cleanupstains();
    cleanupsky();
    cleanupmodels();
    cleanupprefabs();
    cleanuptextures();
    cleanupblendmap();
    cleanuplights();
    halosurf.destroy();
    hazesurf.destroy();
    cleanupshaders();
    cleanupgl();

    setupscreen(false);
    inputgrab(grabinput);
    gl_init();

    inbetweenframes = false;
    if(!reloadtexture(notexturetex) || !reloadtexture(blanktex) || !reloadtexture(logotex))
        fatal("Failed to reload core textures");
    reloadfonts();
    inbetweenframes = true;
    progress(0, "Initializing..");
    restoregamma();
    restorevsync();
    initgbuffer();
    reloadshaders();
    reloadtextures();
    allchanged(true);

    engineready = oldengineready;
    if(engineready) game::preload();
}

ICOMMAND(IDF_NOECHO, resetgl, "", (void), if(!(identflags&IDF_MAP)) resetgl());

bool warping = false, minimized = false;
VAR(IDF_PERSIST, renderunfocused, 0, 0, 1);

static queue<SDL_Event, 32> events;

static inline bool filterevent(const SDL_Event &event)
{
    switch(event.type)
    {
        case SDL_MOUSEMOTION:
            if(grabinput && !relativemouse && !(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
            {
                if(warping && event.motion.x == screenw / 2 && event.motion.y == screenh / 2)
                    return false;  // ignore any motion events generated by SDL_WarpMouse
            }
            break;
    }
    return true;
}

template <int SIZE> static inline bool pumpevents(queue<SDL_Event, SIZE> &events)
{
    while(events.empty())
    {
        SDL_PumpEvents();
        databuf<SDL_Event> buf = events.reserve(events.capacity());
        int n = SDL_PeepEvents(buf.getbuf(), buf.remaining(), SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
        if(n <= 0) return false;
        loopi(n) if(filterevent(buf.buf[i])) buf.put(buf.buf[i]);
        events.addbuf(buf);
    }
    return true;
}

static int interceptkeysym = 0;

static int interceptevents(void *data, SDL_Event *event)
{
    switch(event->type)
    {
        case SDL_KEYDOWN:
            if(event->key.keysym.sym == interceptkeysym)
            {
                interceptkeysym = -interceptkeysym;
                return 0;
            }
            break;
    }
    return 1;
}

static void clearinterceptkey()
{
    SDL_DelEventWatch(interceptevents, NULL);
    interceptkeysym = 0;
}

bool interceptkey(int sym)
{
    if(!interceptkeysym)
    {
        interceptkeysym = sym;
        SDL_FilterEvents(interceptevents, NULL);
        if(interceptkeysym < 0)
        {
            interceptkeysym = 0;
            return true;
        }
        SDL_AddEventWatch(interceptevents, NULL);
    }
    else if(abs(interceptkeysym) != sym) interceptkeysym = sym;
    SDL_PumpEvents();
    if(interceptkeysym < 0)
    {
        clearinterceptkey();
        interceptkeysym = sym;
        SDL_FilterEvents(interceptevents, NULL);
        interceptkeysym = 0;
        return true;
    }
    return false;
}

static void ignoremousemotion()
{
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_MOUSEMOTION);
}

void resetcursor(bool warp, bool reset)
{
    if(warp && grabinput && !relativemouse && !(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
    {
        SDL_WarpMouseInWindow(screen, screenw/2, screenh/2);
        warping = true;
    }
    if(reset) cursorx = cursory = 0.5f;
}

static void checkmousemotion(int &dx, int &dy)
{
    while(pumpevents(events))
    {
        SDL_Event &event = events.removing();
        if(event.type != SDL_MOUSEMOTION) return;
        dx += event.motion.xrel;
        dy += event.motion.yrel;
        events.remove();
    }
}

void checkinput()
{
    if(interceptkeysym) clearinterceptkey();
    //int lasttype = 0, lastbut = 0;
    bool mousemoved = false, shouldwarp = false;
    int focused = 0;
    while(pumpevents(events))
    {
        SDL_Event &event = events.remove();

        if(focused && event.type!=SDL_WINDOWEVENT) { if(grabinput != (focused>0)) inputgrab(grabinput = focused>0, shouldgrab); focused = 0; }

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
                        windowfocus = shouldgrab = true;
                        break;
                    case SDL_WINDOWEVENT_ENTER:
                        shouldgrab = false;
                        focused = 1;
                        break;

                    case SDL_WINDOWEVENT_FOCUS_LOST: windowfocus = false; // fall through
                    case SDL_WINDOWEVENT_LEAVE:
                        shouldgrab = false;
                        focused = -1;
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
                        wantdisplaysetup = true;
                        break;
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
                if(button >= 6) button += 4; // skip mousewheel X (-4,-5) & Y (-8, 9)
                else if(button >= 4) button += 2; // skip mousewheel X (-4,-5)
                processkey(-button, event.button.state==SDL_PRESSED);
                break;
            }
            case SDL_MOUSEWHEEL:
                if(event.wheel.y > 0) { processkey(-4, true); processkey(-4, false); }
                else if(event.wheel.y < 0) { processkey(-5, true); processkey(-5, false); }
                else if(event.wheel.x > 0) { processkey(-8, true); processkey(-8, false); }
                else if(event.wheel.x < 0) { processkey(-9, true); processkey(-9, false); }
                break;
        }
    }
    if(focused) { if(grabinput != (focused>0)) inputgrab(grabinput = focused>0, shouldgrab); focused = 0; }
    if(mousemoved)
    {
        warping = false;
        if(grabinput && shouldwarp) resetcursor(true, false);
    }
}

void swapbuffers(bool overlay)
{
    gle::disable();
    SDL_GL_SwapWindow(screen);
}

int frameloops = 0;

VAR(IDF_PERSIST, menufps, -1, -1, VAR_MAX);
FVAR(IDF_PERSIST, menufpsrefresh, 0.1f, 1, 100);
VAR(IDF_PERSIST, menufpsrefreshoffset, 0, 1, VAR_MAX);
VAR(IDF_PERSIST, maxfps, -1, -1, VAR_MAX);
FVAR(IDF_PERSIST, maxfpsrefresh, 0.1f, 1, 100);
VAR(IDF_PERSIST, maxfpsrefreshoffset, 0, 1, VAR_MAX);

#define GETFPS(a) (a >= 0 ? a : int((refreshrate*a##refresh)+a##refreshoffset))

void limitfps(int &millis, int curmillis)
{
    int curmax = GETFPS(maxfps), curmenu = GETFPS(menufps),
        limit = (hasnoview() || (minimized && !renderunfocused)) && curmenu ? (curmax > 0 ? min(curmax, curmenu) : curmenu) : curmax;
    if(!limit || (limit >= refreshrate && vsync)) return;
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

VARR(curfps, 0);
VARR(bestfps, 0);
VARR(bestfpsdiff, 0);
VARR(worstfps, 0);
VARR(worstfpsdiff, 0);

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

ICOMMANDV(0, engineready, engineready ? 1 : 0);
ICOMMANDV(0, inbetweenframes, inbetweenframes ? 1 : 0);
ICOMMANDV(0, renderedframe, renderedframe ? 1 : 0);

static bool findarg(int argc, char **argv, const char *str)
{
    for(int i = 1; i<argc; i++) if(strstr(argv[i], str)==argv[i]) return true;
    return false;
}

bool progressing = false;
ICOMMANDV(0, progresswait, client::waiting());
ICOMMANDV(0, progressing, progressing ? 1 : 0);
ICOMMANDV(0, progresstype, game::getprogresswait());

FVAR(0, loadprogress, 0, 0, 1);
SVAR(0, progresstitle, "");
FVAR(0, progressamt, -1, 0, 1);
VAR(IDF_PERSIST, progressfps, -1, -1, VAR_MAX);
int lastprogress = 0, progsteps = 0;

void progress(float amt, const char *s, ...)
{
    if(amt < 0.0f)
    {
        amt = 0;
        progsteps = int(amt);
    }
    else if(progsteps && (amt == 0.0f || amt == 1.0f)) progsteps = 0;

    bool oldconvars = consolevars;
    int oldflags = identflags;
    if(consolevars == 1 && consolerun) consolevars = 0;

    string sf;
    if(s != NULL)
    {
        va_list args;
        va_start(args, s);
        vformatstring(sf, s, args);
        va_end(args);
    }
    else copystring(sf, "Loading..");

    setsvar("progresstitle", sf);
    setfvar("progressamt", amt);

    if(progressing || !inbetweenframes || drawtex) goto progresskip;

    if(progressfps && lastprogress)
    {
        int curprog = progressfps >= 0 ? progressfps : refreshrate, diff = getclockticks() - lastprogress;
        if(curprog > 0 && amt > 0 && diff >= 0 && diff < (1000 + curprog-1)/curprog) goto progresskip;
    }

    clientkeepalive();
    SDL_PumpEvents(); // keep the event queue awake to avoid appearing unresponsive

    if(verbose >= 4) conoutf(colourwhite, "%s [%.2f%%]", sf, amt*100.f);

    identflags &= ~IDF_MAP;
    progressing = true;

    updatetextures();
    gl_drawnoview();
    swapbuffers(false);

    lastprogress = getclockticks();

    updatesounds();

    progressing = false;
    identflags = oldflags;
progresskip:
    consolevars = oldconvars;
}


bool pixeling = false;
int pixelingx, pixelingy;
bvec pixel(0, 0, 0);
char *pixelact = NULL;

ICOMMAND(0, printpixel, "", (void), conoutf(colourwhite, "Pixel = 0x%.6X (%d, %d, %d)", pixel.tohexcolor(), pixel.r, pixel.g, pixel.b));
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

void readpixel(char *act, int x, int y, int numargs)
{
    if(pixeling) return;
    if(!editmode) { conoutf(colourred, "Operation only allowed in edit mode"); return; }
    if(pixelact) delete[] pixelact;
    pixelact = act && *act ? newstring(act) : NULL;
    pixeling = true;

    if(numargs > 1)
    {
        pixelingx = x;
        pixelingy = y;
    }
    else
    {
        pixelingx = screenw/2;
        pixelingy = screenh/2;
    }
}
ICOMMAND(0, readpixel, "siiN", (char *act, int *x, int *y, int *numargs), readpixel(act, *x, *y, *numargs));

static void mapslots()
{
    mapsoundslots();
    game::mapslots();
}

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

    #ifdef _DEBUG
        // Run unit tests
        extern void testslotmanager();
        testslotmanager();
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
    setlogfile(LOG_FILE);
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
                    case 'F': fullscreendesktop = atoi(&argv[i][3]); break;
                    case 's': /* compat, ignore */ break;
                    case 'u': /* compat, ignore */ break;
                    default: conoutf(colourred, "Unknown display option: '%c'", argv[i][2]); break;
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

    conoutf(colourwhite, "Loading SDL..");
    int sdlflags = SDL_INIT_TIMER|SDL_INIT_VIDEO;
    if(SDL_Init(sdlflags) < 0)
        fatal("Unable to initialize SDL: %s", SDL_GetError());

#ifdef SDL_VIDEO_DRIVER_X11
    SDL_version version;
    SDL_GetVersion(&version);
    if (SDL_VERSIONNUM(version.major, version.minor, version.patch) <= SDL_VERSIONNUM(2, 0, 12))
        sdl_xgrab_bug = 1;
#endif

    setcaption("Loading, please wait..");

    conoutf(colourwhite, "Loading eNet..");
    if(enet_initialize()<0) fatal("Unable to initialise network module");
    atexit(enet_deinitialize);
    enet_time_set(0);

    conoutf(colourwhite, "Loading game..");
    bool shouldload = initgame();

    //#ifdef WIN32
    //SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    //#endif

    conoutf(colourwhite, "Loading video..");
    SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "0");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "0");
    #ifndef WIN32
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    #endif

    setupscreen();
    SDL_ShowCursor(SDL_FALSE);
    SDL_StopTextInput(); // workaround for spurious text-input events getting sent on first text input toggle?

#if !defined(_DEBUG)
    signal(SIGILL, fatalsignal);
    signal(SIGSEGV, fatalsignal);
    signal(SIGFPE, fatalsignal);
    signal(SIGINT, fatalsignal);
    signal(SIGABRT, fatalsignal);
    signal(SIGTERM, shutdownsignal);
#ifndef WIN32
    signal(SIGHUP, reloadsignal);
    signal(SIGQUIT, fatalsignal);
    signal(SIGKILL, fatalsignal);
    signal(SIGPIPE, fatalsignal);
    signal(SIGALRM, fatalsignal);
#endif
#endif

    conoutf(colourwhite, "Loading GL..");
    gl_checkextensions();
    gl_init();

    conoutf(colourwhite, "Loading sound..");
    initsound();

    game::start();

    conoutf(colourwhite, "Loading defaults..");
    if(!execfile("config/stdlib.cfg", false)) fatal("Cannot find data files");
    if(!setfont()) fatal("No default font specified");
    UI::setup();

    if(!(notexture = textureload(notexturetex)))
        fatal("Could not find core textures");

    inbetweenframes = true;
    progress(0, "Please wait..");

    conoutf(colourwhite, "Loading world..");
    progress(0, "Loading world..");
    setupwind();
    fx::setup();
    emptymap(0, true, NULL, false);

    conoutf(colourwhite, "Loading config..");
    progress(0, "Loading config..");
    initing = INIT_LOAD;
    rehash(false);
    if(shouldload) smartmusic(0, true);
    mapslots();

    initing = NOT_INITING;

    if(shouldload)
    {
        conoutf(colourwhite, "Loading required data..");
        progress(0, "Loading required data..");

        restoregamma();
        restorevsync();
        initgbuffer();
        loadshaders();
        preloadtextures();
        initparticles();
        initstains();

        if(firstrun || noconfigfile)
        {
            conoutf(colourwhite, "First run!");
            firstrun = 0;
            triggereventcallbacks(CMD_EVENT_FIRSTRUN);
        }

        conoutf(colourwhite, "Loading main..");
        progress(0, "Loading main..");

        capslockon = capslocked();
        numlockon = numlocked();
        ignoremousemotion();
        engineready = true;

        localconnect(false);
        resetfps();

        if(reprotoarg)
        {
            if(connecthost && *connecthost) connectserv(connecthost, connectport, connectpassword);
            else conoutf(colourred, "Malformed commandline argument: %s", reprotoarg);
        }

        // housekeeping
        DELETEA(connectstr);
        DELETEA(reprotoarg);

        if(initscript) execute(initscript, true);

        for(frameloops = 0; ; frameloops = frameloops >= INT_MAX-1 ? MAXFPSHISTORY+1 : frameloops+1)
        {
            fx::startframe();
            if(wantdisplaysetup) setupdisplay();
            curtextscale = textscale;

            int elapsed = updatetimer(true);
            updatefps(frameloops, elapsed);

            cdpi::runframe();
            UI::poke();
            checkinput();
            tryedit();

            checksleep(lastmillis);
            serverslice();
            ircslice();

            if(frameloops)
            {
                RUNMAP("on_update");
                game::updateworld();

                game::recomputecamera();
                setviewcell(camera1->o);
                if(!hasnoview()) halosurf.render(); // need halos to be first in pipline..

                cleardynlights();

                fx::update();
                updatewind();
                UI::update();
                updatetextures();
                updateparticles();
                updatesounds();

                if(!minimized || renderunfocused)
                {
                    inbetweenframes = renderedframe = false;

                    if(UI::processviewports())
                    {   // .. and the camera needs to be restored for the rest of the rendering
                        game::recomputecamera();
                        setviewcell(camera1->o);
                        cleardynlights();
                    }

                    gl_drawframe();
                    renderedframe = true;
                    swapbuffers();
                    inbetweenframes = true;

                    if(pixeling)
                    {
                        if(editmode)
                        {
                            glReadPixels(pixelingx, pixelingy, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &pixel.v[0]);
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
