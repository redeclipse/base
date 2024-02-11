#include "engine.h"

void RenderSurface::reset()
{
    tclamp = 3;
    filter = 1;
    width = height = 0;
    format = GL_RGBA;
    target = GL_TEXTURE_RECTANGLE;
    texs.shrink(0);
    fbos.shrink(0);
}

bool RenderSurface::cleanup()
{
    loopv(texs) if(texs[i]) { glDeleteTextures(1, &texs[i]); texs[i] = 0; }
    loopv(fbos) if(fbos[i]) { glDeleteFramebuffers_(1, &fbos[i]); fbos[i] = 0; }

    reset();

    return true;
}

int RenderSurface::genbuffers(int wanttex, int wantfbo)
{
    if(width < 2 || height < 2) return 0;

    int texcount = 0, numfbos = wantfbo < 0 ? wanttex : min(wanttex, wantfbo);
    bool swapped = false;
    GLuint curfbo = renderfbo;

    GLERROR;
    loopi(wanttex)
    {
        GLuint tex = 0;
        if(texs.inrange(i)) tex = texs[i];
        else
        {
            glGenTextures(1, &tex);
            createtexture(tex, width, height, NULL, tclamp, filter, format, target);
            texs.add(tex);
            GLERROR;
        }
        texcount++;

        if(fbos.length() >= numfbos) continue;

        GLuint fbo = 0;
        if(fbos.inrange(i)) fbo = fbos[i];
        else
        {
            glGenFramebuffers_(1, &fbo);
            glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, tex, 0);
            fbos.add(fbo);
            GLERROR;
            swapped = true;
        }
    }

    if(swapped) glBindFramebuffer_(GL_FRAMEBUFFER, curfbo);

    return texcount;
}

void RenderSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n, int &o)
{
    w = max(int(w > 0 ? w : vieww), 2);
    h = max(int(h > 0 ? h : viewh), 2);
}

int RenderSurface::setup(int w, int h, GLenum f, GLenum t, int wanttex, int wantfbo)
{
    cleanup();

    width = w;
    height = h;
    format = f;
    target = t;

    return genbuffers(wanttex, wantfbo);
}

int RenderSurface::init(int w, int h, GLenum f, GLenum t, int wanttex, int wantfbo)
{
    checkformat(w, h, f, t, wanttex, wantfbo);

    if(width == w && height == h && format == f && target == t && texs.length() == wanttex && fbos.length() == (wantfbo < 0 ? wanttex : wantfbo))
        return -1;

    return setup(w, h, f, t, wanttex, wantfbo);
}

bool RenderSurface::bindtex(int wanttex, int tmu)
{
    if(!texs.inrange(wanttex) || !texs[wanttex]) return false;

    glActiveTexture_(GL_TEXTURE0 + tmu);
    glBindTexture(target, texs[wanttex]);

    return true;
}

bool RenderSurface::savefbo(int wantfbo)
{
    if(!fbos.inrange(wantfbo) || !fbos[wantfbo]) return false;

    origfbo = renderfbo;

    return true;
}

bool RenderSurface::bindfbo(int wantfbo)
{
    if(!fbos.inrange(wantfbo) || !fbos[wantfbo]) return false;

    savefbo(wantfbo);
    glBindFramebuffer_(GL_FRAMEBUFFER, fbos[wantfbo]);
    glViewport(0, 0, width, height);

    return true;
}

int RenderSurface::create(int w, int h, GLenum f, GLenum t, int wanttex, int wantfbo)
{
    checkformat(w, h, f, t, wanttex, wantfbo);
    return init(w, h, f, t, wanttex, wantfbo);
}

bool RenderSurface::destroy() { return cleanup(); }

bool RenderSurface::render() { return false; }

bool RenderSurface::swap(int wantfbo) { return bindfbo(wantfbo); }

bool RenderSurface::draw(int x, int y, int w, int h) { return false; }

void RenderSurface::debug(int w, int h, int wanttex, bool large)
{
    wanttex = max(wanttex > 0 ? clamp(wanttex, 0, texs.length()) : texs.length(), 1);

    int sw = w / (large ? wanttex : wanttex * 2), sx = 0, sh = (sw * h) / w;

    loopi(wanttex)
    {
        bool hastex = texs.inrange(i);
        GLuint tex = hastex ? texs[i] : notexture->id;
        GLenum targ = hastex ? target : GL_TEXTURE_2D;

        switch(targ)
        {
            case GL_TEXTURE_RECTANGLE:
            {
                SETSHADER(hudrectrgb);
                break;
            }
            case GL_TEXTURE_2D:
            {
                SETSHADER(hudrgb);
                break;
            }
            default: continue;
        }

        gle::colorf(1, 1, 1);
        glBindTexture(targ, tex);
        debugquad(sx, 0, sw, sh, 0, 0, width, height);
        sx += sw;
    }
}

bool RenderSurface::save(const char *name, int w, int h)
{
    if(!bindfbo()) return false;

    GLuint tex;
    glGenTextures(1, &tex);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    ImageData image(width, height, 3);
    memset(image.data, 0, 3*width*height);

    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, image.data);

    scaleimage(image, w, h);
    saveimage(name, image, imageformat, compresslevel, true);
    glDeleteTextures(1, &tex);

    restorefbo();

    return false;
}

void RenderSurface::restorefbo()
{
    GLERROR;
    glBindFramebuffer_(GL_FRAMEBUFFER, origfbo);

    GLERROR;
    gl_setupframe();

    vieww = hudw;
    viewh = hudh;

    glViewport(0, 0, hudw, hudh);
}

VAR(0, debughalo, 0, 0, 2);
VAR(IDF_PERSIST, halos, 0, 1, 1);
FVAR(IDF_PERSIST, halowireframe, 0, 0, FVAR_MAX);
VAR(IDF_PERSIST, halodist, 32, 2048, VAR_MAX);
FVARF(IDF_PERSIST, haloscale, FVAR_NONZERO, 0.5f, 1, halosurf.destroy());
FVAR(IDF_PERSIST, haloblend, 0, 0.65f, 1);
CVAR(IDF_PERSIST, halocolour, 0xFFFFFF);
FVAR(IDF_PERSIST, halotolerance, FVAR_MIN, -16, FVAR_MAX);
FVAR(IDF_PERSIST, haloaddz, FVAR_MIN, 2, FVAR_MAX);

VARF(IDF_PERSIST, halosamples, 1, 3, 5, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // number of samples
FVARF(IDF_PERSIST, halooffset, FVAR_NONZERO, 0.5f, 4, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // the offset multiplier of each sample
FVARF(IDF_PERSIST, halooutlinemix, 0, 1, 1, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // mix between first/closest sample and accumulation of all samples
FVARF(IDF_PERSIST, halooutlinecol, 0, 1, FVAR_MAX, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // multiply resulting rgb by this
FVARF(IDF_PERSIST, halooutlineblend, 0, 1, FVAR_MAX, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // multiply resulting a by this
FVARF(IDF_PERSIST, halooutlineshadow, 0, 0, 1, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // apply highlight/shadowing with an extra sample
FVARF(IDF_PERSIST, haloinfillmix, 0, 0, 1, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS));
FVARF(IDF_PERSIST, haloinfillcol, 0, 0.5f, FVAR_MAX, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS));
FVARF(IDF_PERSIST, haloinfillblend, 0, 0.5f, FVAR_MAX, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS));
FVARF(IDF_PERSIST, halonoisesample, 0, 0.5f, 8, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // apply random noise to sampling by this multiplier
FVARF(IDF_PERSIST, halonoisemixcol, 0, 0, 1, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // mix noise with the output colour
FVARF(IDF_PERSIST, halonoisemixblend, 0, 0, 1, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // mix noise with the output alpha

FVAR(IDF_PERSIST, haloscanlines, -16.0f, -4.0f, 16.0f);
VAR(IDF_PERSIST|IDF_HEX, haloscanlinemixcolour, 0, 0xFFFFFF, 0xFFFFFF);
FVAR(IDF_PERSIST, haloscanlinemixblend, 0.0f, 0.5f, 1.0f);
FVAR(IDF_PERSIST, haloscanlineblend, 0.0f, 0.75f, 16.0f);
FVAR(IDF_PERSIST, halonoiseblend, 0.0f, 0.15f, 16.0f);
FVAR(IDF_PERSIST, haloflickerblend, 0.0f, 0.1f, 16.0f);

HaloSurface halosurf;

void HaloSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n, int &o)
{
    w = max(int((w > 0 ? w : vieww * haloscale)), 2);
    h = max(int((h > 0 ? h : viewh * haloscale)), 2);
    n = o = MAX;
}

int HaloSurface::create(int w, int h, GLenum f, GLenum t, int wanttex, int wantfbo)
{
    useshaderbyname("hudhalodepth");
    useshaderbyname("hudhalotop");
    useshaderbyname("hudhalodepthref");
    useshaderbyname("hudhalotopref");

    halotype = -1;
    checkformat(w, h, f, t, wanttex, wantfbo);

    return init(w, h, f, t, wanttex, wantfbo);
}

bool HaloSurface::check(bool check, bool val)
{
    if(texs.empty()) return false;
    if(check && drawtex != DRAWTEX_HALO) return true;
    return halos && val;
}

bool HaloSurface::destroy()
{
    halotype = -1;
    return cleanup();
}

bool HaloSurface::swap(int wantfbo)
{
    if(wantfbo < 0 || wantfbo >= HaloSurface::MAX || halotype == wantfbo) return false;

    if(!bindfbo(wantfbo)) return false;

    halotype = wantfbo;
    return true;
}

bool HaloSurface::render()
{
    if(hasnoview() || !create()) return false;

    GLERROR;
    gl_setupframe();
    vieww = width;
    viewh = height;

    drawtex = DRAWTEX_HALO;

    projmatrix.perspective(fovy, aspect, nearplane, farplane);
    setcamprojmatrix();

    halotype = -1;
    loopirev(HaloSurface::MAX)
    {
        swap(i);

        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }


    glEnable(GL_CULL_FACE);

    if(halowireframe > 0)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(halowireframe);
    }

    loopi(2)
    {
        resetmodelbatches();
        game::render(i + 1);
        renderhalomodelbatches();
    }
    renderavatar();

    if(halowireframe > 0)
    {
        glLineWidth(1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glDisable(GL_CULL_FACE);

    drawtex = 0;
    restorefbo();

    return true;
}

bool HaloSurface::draw(int x, int y, int w, int h)
{
    if(!halos || texs.empty()) return false;

    if(w <= 0) w = hudw;
    if(h <= 0) h = hudh;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gle::color(halocolour.tocolor().mul(game::darkness(DARK_HALO)), haloblend);

    float maxdist = hud::radarlimit(halodist), scanlines = haloscanlines >= 0 ? haloscanlines : visorscanlines * fabs(haloscanlines);

    loopirev(HaloSurface::MAX)
    {
        switch(i)
        {
            case HaloSurface::DEPTH:
                if(hasrefractmask) SETSHADER(hudhalodepthref);
                else SETSHADER(hudhalodepth);
                break;
            case HaloSurface::ONTOP:
                if(hasrefractmask) SETSHADER(hudhalotopref);
                else SETSHADER(hudhalotop);
                break;
            default: continue;
        }
        glBindTexture(GL_TEXTURE_RECTANGLE, texs[i]);

        if(hasrefractmask)
        {
            glActiveTexture_(GL_TEXTURE0 + TEX_REFRACT_MASK);
            if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msrefracttex);
            else glBindTexture(GL_TEXTURE_RECTANGLE, refracttex);
        }

        glActiveTexture_(GL_TEXTURE0 + TEX_REFRACT_DEPTH);
        if(msaasamples) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
        else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);

        glActiveTexture_(GL_TEXTURE0);

        LOCALPARAMF(millis, lastmillis / 1000.0f);
        LOCALPARAMF(halosize, w, h, 1.0f / w, 1.0f / h);
        LOCALPARAMF(haloparams, maxdist, 1.0f / maxdist);
        LOCALPARAMF(halofx, scanlines, haloscanlineblend, halonoiseblend, haloflickerblend);
        vec4 color = vec4::fromcolor(haloscanlinemixcolour, haloscanlinemixblend);
        LOCALPARAMF(halofxcol, color.r, color.g, color.b, color.a);

        hudquad(x, y, w, h, 0, height, width, -height);
    }

    glDisable(GL_BLEND);

    return true;
}

#define MPVVARS(name, type) \
    VARF(IDF_MAP, haze##name, 0, 0, 1, hazesurf.create()); \
    CVAR(IDF_MAP, hazecolour##name, 0); \
    FVAR(IDF_MAP, hazecolourmix##name, 0, 0.5f, 1); \
    FVAR(IDF_MAP, hazeblend##name, 0, 1, 1); \
    SVARF(IDF_MAP, hazetex##name, "textures/watern", hazesurf.create()); \
    FVAR(IDF_MAP, hazemindist##name, 0, 256, FVAR_MAX); \
    FVAR(IDF_MAP, hazemaxdist##name, 0, 1024, FVAR_MAX); \
    FVAR(IDF_MAP, hazemargin##name, FVAR_NONZERO, 32, FVAR_MAX); \
    FVAR(IDF_MAP, hazescalex##name, FVAR_NONZERO, 1, FVAR_MAX); \
    FVAR(IDF_MAP, hazescaley##name, FVAR_NONZERO, 2, FVAR_MAX); \
    FVAR(IDF_MAP, hazerefract##name, FVAR_NONZERO, 2, 10); \
    FVAR(IDF_MAP, hazerefract2##name, FVAR_NONZERO, 4, 10); \
    FVAR(IDF_MAP, hazerefract3##name, FVAR_NONZERO, 8, 10); \
    FVAR(IDF_MAP, hazescrollx##name, FVAR_MIN, 0, FVAR_MAX); \
    FVAR(IDF_MAP, hazescrolly##name, FVAR_MIN, -0.5f, FVAR_MAX);

MPVVARS(, MPV_DEFAULT);
MPVVARS(alt, MPV_ALTERNATE);

#define GETMPV(name, type) \
    type get##name() \
    { \
        if(checkmapvariant(MPV_ALTERNATE)) return name##alt; \
        return name; \
    }

GETMPV(haze, int);
GETMPV(hazetex, const char *);
GETMPV(hazecolour, const bvec &);
GETMPV(hazecolourmix, float);
GETMPV(hazeblend, float);
GETMPV(hazemindist, float);
GETMPV(hazemaxdist, float);
GETMPV(hazemargin, float);
GETMPV(hazescalex, float);
GETMPV(hazescaley, float);
GETMPV(hazerefract, float);
GETMPV(hazerefract2, float);
GETMPV(hazerefract3, float);
GETMPV(hazescrollx, float);
GETMPV(hazescrolly, float);

VAR(0, debughaze, 0, 0, 2);

HazeSurface hazesurf;

void HazeSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n, int &o)
{
    w = max(int((w > 0 ? w : vieww)), 2);
    h = max(int((h > 0 ? h : viewh)), 2);
    f = hdrformat;
    o = msaalight != 0 ? 1 : 0;
}

bool HazeSurface::check()
{
    return gethaze() != 0 || hazeparticles;
}

int HazeSurface::create(int w, int h, GLenum f, GLenum t, int wanttex, int wantfbo)
{
    tex = NULL;
    if(!check()) return -1;

    const char *hazename = gethazetex();
    if(hazename[0]) tex = textureload(hazename, 0, true, false);
    if(tex == notexture) tex = NULL;

    if(tex)
    {
        useshaderbyname("hazetex");
        useshaderbyname("hazetexref");
    }
    else
    {
        useshaderbyname("haze");
        useshaderbyname("hazeref");
    }

    checkformat(w, h, f, t, wanttex, wantfbo);

    return init(w, h, f, t, wanttex, wantfbo);
}

bool HazeSurface::render()
{
    if(!create()) return false;

    if(tex || hazeparticles)
    {
        if(msaalight)
        {
            glBindFramebuffer_(GL_READ_FRAMEBUFFER, mshdrfbo);
            glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, fbos[0]);
            glBlitFramebuffer_(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer_(GL_FRAMEBUFFER, mshdrfbo);
        }
        else
        {
            glBindTexture(GL_TEXTURE_RECTANGLE, texs[0]);
            glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, 0, 0, width, height);
        }
    }

    bool hazemix = false;
    if(gethaze() != 0)
    {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if(hasrefractmask)
        {
            glActiveTexture_(GL_TEXTURE0 + TEX_REFRACT_MASK);
            if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msrefracttex);
            else glBindTexture(GL_TEXTURE_RECTANGLE, refracttex);
        }

        if(tex)
        {
            glActiveTexture_(GL_TEXTURE0 + TEX_REFRACT_LIGHT);
            glBindTexture(GL_TEXTURE_RECTANGLE, texs[0]);
        }

        glActiveTexture_(GL_TEXTURE0 + TEX_REFRACT_DEPTH);
        if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
        else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);

        glActiveTexture_(GL_TEXTURE0);

        const bvec &color = gethazecolour();
        float colormix = gethazecolourmix();
        if(tex && color.iszero()) colormix = 0;
        else hazemix = true;
        GLOBALPARAMF(hazecolor, color.x*ldrscaleb, color.y*ldrscaleb, color.z*ldrscaleb, colormix);
        float refract = gethazerefract(), refract2 = gethazerefract2(), refract3 = gethazerefract3();
        GLOBALPARAMF(hazerefract, refract, refract2, refract3);
        float margin = gethazemargin(), mindist = gethazemindist(), maxdist = max(max(mindist, gethazemaxdist())-mindist, margin), blend = gethazeblend();
        GLOBALPARAMF(hazeparams, mindist, 1.0f/maxdist, 1.0f/margin, blend);

        if(tex)
        {
            float xscale = gethazescalex(), yscale = gethazescaley(), scroll = lastmillis/1000.0f, xscroll = gethazescrollx()*scroll, yscroll = gethazescrolly()*scroll;
            GLOBALPARAMF(hazetexgen, xscale, yscale, xscroll, yscroll);
            settexture(tex);
            if(hasrefractmask) SETSHADER(hazetexref);
            else SETSHADER(hazetex);
        }
        else if(hasrefractmask) SETSHADER(hazeref);
        else SETSHADER(haze);

        screenquad();

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    if(hazeparticles) renderhazeparticles(texs[0], hazemix);

    return true;
}

VAR(IDF_PERSIST, visorhud, 0, 13, 15); // bit: 1 = normal, 2 = edit, 4 = progress, 8 = noview
FVAR(IDF_PERSIST, visordistort, -2, 2.0f, 2);
FVAR(IDF_PERSIST, visornormal, -2, 1.175f, 2);
FVAR(IDF_PERSIST, visorscalex, FVAR_NONZERO, 0.9075f, 2);
FVAR(IDF_PERSIST, visorscaley, FVAR_NONZERO, 0.9075f, 2);
VAR(IDF_PERSIST, visorscanedit, 0, 1, 7); // bit: 1 = scanlines, 2 = noise, 4 = flicker
FVAR(IDF_PERSIST, visorscanlines, 0.0, 2.66f, 16.0f);
VAR(IDF_PERSIST|IDF_HEX, visorscanlinemixcolour, 0, 0xFFFFFF, 0xFFFFFF);
FVAR(IDF_PERSIST, visorscanlinemixblend, 0.0, 0.67f, 1.0f);
FVAR(IDF_PERSIST, visorscanlineblend, 0.0, 0.25f, 16.0f);
FVAR(IDF_PERSIST, visornoiseblend, 0.0, 0.125f, 16.0f);
FVAR(IDF_PERSIST, visorflickerblend, 0.0, 0.015f, 16.0f);

VAR(IDF_PERSIST, visortiltsurfaces, 0, 4, 15); // bit: 1 = background, 2 = world UI's, 4 = visor, 8 = foreground
VAR(IDF_PERSIST, visorscansurfaces, 0, 15, 15); // bit: 1 = background, 2 = world UI's, 4 = visor, 8 = foreground

ICOMMANDV(0, visorenabled, visorsurf.check() ? 1 : 0);

VisorSurface visorsurf;
VARR(rendervisor, -1);

void VisorSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n, int &o)
{
    w = max(int((w > 0 ? w : vieww)), 2);
    h = max(int((h > 0 ? h : viewh)), 2);
    n = o = 2;
}

bool VisorSurface::check()
{
    if(!engineready) return false;

    if(hasnoview()) return (visorhud&8)!=0;
    else if(progressing) return (visorhud&4)!=0;
    else if(editmode) return (visorhud&2)!=0;
    else return (visorhud&1)!=0;
    return false;
}

void VisorSurface::coords(float cx, float cy, float &vx, float &vy, bool back)
{
    // WARNING: This function MUST produce the same
    // results as the 'hudvisorview' shader for cursor projection.

    vec2 from(cx, cy), to = from;

    to.sub(vec2(0.5f));
    to.mul(vec2(visorscalex, visorscaley));

    float mag = to.magnitude();

    to.mul(1.0 + visordistort * visornormal * visornormal);
    to.div(1.0 + visordistort + mag * mag);

    to.add(vec2(0.5f));
    if(!back) to.sub(from);

    vx = back ? to.x : from.x - to.x; // what we get is an offset from requested position
    vy = back ? to.y : from.y - to.y; // that is then returned or subtracted from it
}

float VisorSurface::getcursorx(int type)
{
    switch(type)
    {
        case -1: return ::cursorx; // force cursor
        case 1: return cursorx; // force visor
        default: break;
    }
    return rendervisor == 2 ? cursorx : ::cursorx;
}

float VisorSurface::getcursory(int type)
{
    switch(type)
    {
        case -1: return ::cursory; // force cursor
        case 1: return cursory; // force visor
        default: break;
    }
    return rendervisor == 2 ? cursory : ::cursory;
}

bool VisorSurface::render()
{
    bool noview = hasnoview(), wantvisor = visorsurf.check();

    GLERROR;
    gl_setupframe();

    int sw = renderw, sh = renderh;
    if(engineready)
    {
        if(gscale != 100)
        {
            sw = max((renderw*gscale + 99)/100, 1);
            sh = max((renderh*gscale + 99)/100, 1);
        }

        vieww = max(sw, hudw);
        viewh = max(sh, hudh);
    }
    else
    {
        vieww = hudw;
        viewh = hudh;
    }

    if(engineready && create())
    {
        if(wantvisor) visorsurf.coords(::cursorx, ::cursory, cursorx, cursory, true);
        else
        {
            cursorx = ::cursorx;
            cursory = ::cursory;
        }

        offsetx = offsety = 0;
        hud::visorinfo(offsetx, offsety);

        if(offsetx) cursorx += offsetx / width;
        if(offsety) cursory += offsety / height;

        enabled = true;
    }
    else
    {
        enabled = false;
        cursorx = cursory = offsetx = offsety = 0;
    }

    if(enabled)
    {
        loopi(MAX)
        {
            if(i == WORLD && (progressing || noview)) continue; // skip world UI's when in progress or noview

            int cur = i == WORLD ? 1 : 0, curw = i == WORLD ? sw : width, curh = i == WORLD ? sh : height;

            if(!bindfbo(cur)) continue;

            vieww = curw;
            viewh = curh;

            glViewport(0, 0, curw, curh);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            hudmatrix.ortho(0, curw, curh, 0, -1, 1);
            flushhudmatrix();
            resethudshader();

            glEnable(GL_BLEND);
            glBlendFuncSeparate_(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE);

            rendervisor = wantvisor ? i : -1;
            switch(i)
            {
                case BACKGROUND:
                {
                    UI::render(SURFACE_BACKGROUND, fbos[cur]);

                    hud::startrender(curw, curh, wantvisor, noview, fbos[cur]);

                    break;
                }
                case WORLD:
                {
                    bindgdepth();

                    glEnable(GL_DEPTH_TEST);
                    glDepthMask(GL_FALSE);

                    UI::render(SURFACE_WORLD, fbos[cur]);

                    glDepthMask(GL_TRUE);
                    glDisable(GL_DEPTH_TEST);

                    break;
                }
                case VISOR:
                {
                    if(progressing && wantvisor) UI::render(SURFACE_PROGRESS, fbos[cur]);
                    else UI::render(SURFACE_VISOR, fbos[cur]);

                    hud::visorrender(curw, curh, wantvisor, noview, fbos[cur]);

                    break;
                }
                case FOREGROUND:
                {
                    if(progressing && !wantvisor) UI::render(SURFACE_PROGRESS, fbos[cur]);
                    else UI::render(SURFACE_FOREGROUND, fbos[cur]);

                    hud::endrender(curw, curh, wantvisor, noview, fbos[cur]);

                    hudmatrix.ortho(0, curw, curh, 0, -1, 1);
                    flushhudmatrix();
                    resethudshader();

                    hud::drawpointers(curw, curh, getcursorx(), getcursory());

                    break;
                }
            }
            rendervisor = -1;

            vieww = hudw;
            viewh = hudh;

            restorefbo();

            hudmatrix.ortho(0, hudw, hudh, 0, -1, 1);
            flushhudmatrix();
            resethudshader();
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

            bool visorok = i != 0 || (!noview && !progressing);
            if(wantvisor && i == 2 && visorok)
            {
                SETSHADER(hudvisorview);
                LOCALPARAMF(visorparams, visordistort, visornormal, visorscalex, visorscaley);
            }
            else SETSHADER(hudvisor);

            LOCALPARAMF(time, lastmillis / 1000.f);
            LOCALPARAMF(visorsize, hudw, hudh, 1.0f / hudw, 1.0f / hudh);

            if(visorscansurfaces&(1<<i))
            {
                if(!editmode) LOCALPARAMF(visorfx, visorscanlines, visorscanlineblend, visornoiseblend, visorok ? visorflickerblend : 0.0f);
                else LOCALPARAMF(visorfx, visorscanedit&1 ? visorscanlines : 0.0f, visorscanedit&1 ? visorscanlineblend : 0.0f, visorscanedit&2 ? visornoiseblend : 0.0f, visorscanedit&4 && visorok ? visorflickerblend : 0.0f);

                vec4 color = vec4::fromcolor(visorscanlinemixcolour, visorscanlinemixblend);
                LOCALPARAMF(visorfxcol, color.r, color.g, color.b, color.a);
            }
            else
            {
                LOCALPARAMF(visorfx, 0, 0, 0, 0);
                LOCALPARAMF(visorfxcol, 0, 0, 0, 0);
            }

            gle::colorf(1, 1, 1, 1);
            glBindTexture(GL_TEXTURE_RECTANGLE, texs[cur]);

            if(visortiltsurfaces&(1<<i) && !hud::hasinput(true))
                hudquad(offsetx, offsety, hudw, hudh, 0, curh, curw, -curh);
            else hudquad(0, 0, hudw, hudh, 0, curh, curw, -curh);

            glDisable(GL_BLEND);
        }
    }
    else
    {
        hudmatrix.ortho(0, hudw, hudh, 0, -1, 1);
        flushhudmatrix();
        resethudshader();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        hud::startrender(hudw, hudh, false, noview);
        hud::visorrender(hudw, hudh, false, noview);
        hud::endrender(hudw, hudh, false, noview);

        glDisable(GL_BLEND);
    }

    return true;
}

void ViewSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n, int &o)
{
    w = max(int((w > 0 ? w : vieww)), 2);
    h = max(int((h > 0 ? h : viewh)), 2);
    n = o = 1;
}

int ViewSurface::create(int w, int h, GLenum f, GLenum t, int wanttex, int wantfbo)
{
    useshaderbyname("hudhalodepth");
    useshaderbyname("hudhalotop");
    useshaderbyname("hudhalodepthref");
    useshaderbyname("hudhalotopref");

    checkformat(w, h, f, t, wanttex, wantfbo);

    return init(w, h, f, t, wanttex, wantfbo);
}

bool ViewSurface::destroy()
{
    return cleanup();
}

bool ViewSurface::render()
{
    GLERROR;
    gl_setupframe();

    int sw = renderw, sh = renderh;
    if(engineready)
    {
        if(gscale != 100)
        {
            sw = max((renderw*gscale + 99)/100, 1);
            sh = max((renderh*gscale + 99)/100, 1);
        }

        vieww = max(sw, hudw);
        viewh = max(sh, hudh);
    }
    else
    {
        vieww = hudw;
        viewh = hudh;
    }

    if(!create() || !bindfbo()) return false;

    renderfbo = fbos[0];

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    float oldaspect = aspect, oldfovy = fovy, oldfov = curfov;
    int oldvieww = vieww, oldviewh = viewh;

    physent *oldcamera = camera1;
    static physent cmcamera;
    cmcamera = *camera1;
    cmcamera.reset();
    cmcamera.type = ENT_CAMERA;
    cmcamera.o = worldpos;
    cmcamera.yaw = yaw;
    cmcamera.pitch = pitch;
    cmcamera.roll = roll;
    camera1 = &cmcamera;
    aspect = ratio;
    curfov = fov;
    fovy = 2*atan2(tan(curfov/2*RAD), aspect)/RAD;
    setviewcell(camera1->o);

    int olddrawtex = drawtex;
    drawtex = DRAWTEX_VIEW;

    int fogmat, abovemat;
    float fogbelow;
    getcamfogmat(fogmat, abovemat, fogbelow);
    setfog(abovemat);

    farplane = worldsize * farscale;

    projmatrix.perspective(fovy, aspect, nearplane, farplane);
    setcamprojmatrix();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    ldrscale = 0.5f;
    ldrscaleb = ldrscale/255;

    visiblecubes();
    rendergbuffer();

    renderao();
    GLERROR;

    // render grass after AO to avoid disturbing shimmering patterns
    generategrass();
    rendergrass();
    GLERROR;

    glFlush();

    renderradiancehints();
    GLERROR;

    rendershadowatlas();
    GLERROR;

    shadegbuffer();
    GLERROR;

    if(fogmat)
    {
        setfog(fogmat, fogbelow, 1, abovemat);
        renderdepthfog(fogmat, fogbelow);
        setfog(fogmat, fogbelow, clamp(fogbelow, 0.0f, 1.0f), abovemat);
    }

    rendertransparent();
    GLERROR;
    if(fogmat) setfog(fogmat, fogbelow, 1, abovemat);

    rendervolumetric();
    GLERROR;

    drawenvlayers(false);
    GLERROR;

    if(DRAWTEX_HAZE&(1<<drawtex))
    {
        hazesurf.render();
        GLERROR;
    }

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    if(fogoverlay && fogmat != MAT_AIR && (fogoverlay == 2 || (fogmat&MATF_VOLUME) != MAT_VOLFOG)) drawfogoverlay(fogmat, fogbelow, clamp(fogbelow, 0.0f, 1.0f), abovemat);

    processhdr(fbos[0], AA_UNUSED);

    drawtex = olddrawtex;

    aspect = oldaspect;
    fovy = oldfovy;
    curfov = oldfov;
    vieww = oldvieww;
    viewh = oldviewh;

    camera1 = oldcamera;

    restorefbo();

    renderfbo = origfbo;

    return true;
}
