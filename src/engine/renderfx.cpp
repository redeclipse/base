#include "engine.h"

bool RenderSurface::cleanup()
{
    buffers.deletecontents();
    return true;
}

void RenderSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n)
{
    w = max(int(w > 0 ? w : vieww), 2);
    h = max(int(h > 0 ? h : viewh), 2);
}

int RenderSurface::setup(int w, int h, GLenum f, GLenum t, int count)
{
    GLuint curfbo = renderfbo;
    bool restore = false;

    loopi(count)
    {
        if(buffers.inrange(i))
        {
            if(buffers[i]->check(w, h, f, t))
                restore = true;
            continue;
        }

        buffers.add(new RenderBuffer(w, h, f, t));
        restore = true;
    }

    if(restore) glBindFramebuffer_(GL_FRAMEBUFFER, curfbo);

    return buffers.length();
}

bool RenderSurface::bindtex(int index, int tmu)
{
    if(!buffers.inrange(index)) return false;

    if(tmu >= 0) glActiveTexture_(GL_TEXTURE0 + tmu);
    glBindTexture(buffers[index]->target, buffers[index]->tex);

    return true;
}

void RenderSurface::savefbo()
{
    origfbo = renderfbo;
    origvieww = vieww;
    origviewh = viewh;
}

bool RenderSurface::bindfbo(int index, int w, int h)
{
    if(!buffers.inrange(index)) return false;
    if(renderfbo == buffers[index]->fbo) return true;

    renderfbo = buffers[index]->fbo;
    vieww = w > 0 && w < buffers[index]->width ? w : buffers[index]->width;
    viewh = h > 0 && h < buffers[index]->height ? h : buffers[index]->height;

    GLERROR;
    glBindFramebuffer_(GL_FRAMEBUFFER, renderfbo);
    glViewport(0, 0, vieww, viewh);

    return true;
}

int RenderSurface::create(int w, int h, GLenum f, GLenum t, int count)
{
    checkformat(w, h, f, t, count);
    return setup(w, h, f, t, count);
}

bool RenderSurface::destroy() { return cleanup(); }

bool RenderSurface::render(int w, int h, GLenum f, GLenum t, int count) { return false; }

bool RenderSurface::swap(int index) { return bindfbo(index); }

bool RenderSurface::draw(int x, int y, int w, int h) { return false; }

void RenderSurface::debug(int w, int h, int index, bool large)
{
    index = max(index > 0 ? clamp(index, 0, buffers.length()) : buffers.length(), 1);

    int sw = w / (large ? index : index * 2), sx = 0, sh = (sw * h) / w;

    loopi(index)
    {
        bool hastex = buffers.inrange(i);
        GLuint tex = hastex ? buffers[i]->tex : notexture->id;
        GLenum targ = hastex ? buffers[i]->target : GL_TEXTURE_2D;

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
        debugquad(sx, 0, sw, sh, 0, 0, buffers[i]->width, buffers[i]->height);
        sx += sw;
    }
}

bool RenderSurface::save(const char *name, int w, int h, int index)
{
    savefbo();
    if(!bindfbo(index)) return false;

    GLuint tex;
    glGenTextures(1, &tex);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    ImageData image(vieww, viewh, 3);
    memset(image.data, 0, 3*vieww*viewh);

    glReadPixels(0, 0, vieww, viewh, GL_RGB, GL_UNSIGNED_BYTE, image.data);

    if(w != vieww || h != viewh) scaleimage(image, w, h);
    saveimage(name, image, imageformat, compresslevel, true);
    glDeleteTextures(1, &tex);

    restorefbo();

    return true;
}

void RenderSurface::restorefbo()
{
    renderfbo = origfbo;
    vieww = origvieww;
    viewh = origviewh;

    GLERROR;
    glBindFramebuffer_(GL_FRAMEBUFFER, renderfbo);
    glViewport(0, 0, vieww, viewh);
}

bool RenderSurface::copy(int index, GLuint fbo, int w, int h, bool restore)
{
    if(!buffers.inrange(index)) return false;

    if(restore) savefbo();

    glBindFramebuffer_(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, buffers[index]->fbo);
    glBlitFramebuffer_(0, 0, w, h, 0, 0, buffers[index]->width, buffers[index]->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    if(restore) restorefbo();

    return true;
}


VAR(0, debughalo, 0, 0, 2);
VAR(IDF_PERSIST, halos, 0, 1, 1);
FVAR(IDF_PERSIST, halowireframe, 0, 0, FVAR_MAX);
VAR(IDF_PERSIST, halodist, 32, 2048, VAR_MAX);
FVARF(IDF_PERSIST, haloscale, FVAR_NONZERO, 0.5f, 1, halosurf.destroy());
FVAR(IDF_PERSIST, haloblend, 0, 0.5f, 1);
CVAR(IDF_PERSIST, halocolour, 0xFFFFFF);
FVAR(IDF_PERSIST, halotolerance, FVAR_MIN, -16, FVAR_MAX);
FVAR(IDF_PERSIST, haloaddz, FVAR_MIN, 0, FVAR_MAX);

VARF(IDF_PERSIST, halosamples, 1, 3, 5, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // number of samples
FVARF(IDF_PERSIST, halooffset, FVAR_NONZERO, 0.5f, 4, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // the offset multiplier of each sample
FVARF(IDF_PERSIST, halooutlinemix, 0, 1, 1, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // mix between first/closest sample and accumulation of all samples
FVARF(IDF_PERSIST, halooutlinecol, 0, 1, FVAR_MAX, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // multiply resulting rgb by this
FVARF(IDF_PERSIST, halooutlineblend, 0, 1, FVAR_MAX, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // multiply resulting a by this
FVARF(IDF_PERSIST, halooutlineshadow, 0, 0, 1, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // apply highlight/shadowing with an extra sample
FVARF(IDF_PERSIST, haloinfillmix, 0, 0, 1, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS));
FVARF(IDF_PERSIST, haloinfillcol, 0, 0.5f, FVAR_MAX, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS));
FVARF(IDF_PERSIST, haloinfillblend, 0, 0.5f, FVAR_MAX, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS));
FVARF(IDF_PERSIST, halonoisesample, 0, 1, 8, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // apply random noise to sampling by this multiplier
FVARF(IDF_PERSIST, halonoisemixcol, 0, 0, 1, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // mix noise with the output colour
FVARF(IDF_PERSIST, halonoisemixblend, 0, 0, 1, initwarning("Halos", INIT_LOAD, CHANGE_SHADERS)); // mix noise with the output alpha

FVAR(IDF_PERSIST, haloscanlines, FVAR_MIN, 8.0f, FVAR_MAX);
VAR(IDF_PERSIST|IDF_HEX, haloscanlinemixcolour, 0, 0xFFFFFF, 0xFFFFFF);
FVAR(IDF_PERSIST, haloscanlinemixblend, 0.0f, 0.5f, 1.0f);
FVAR(IDF_PERSIST, haloscanlineblend, 0.0f, 0.5f, 16.0f);
FVAR(IDF_PERSIST, halonoiseblend, 0.0f, 0.15f, 16.0f);
FVAR(IDF_PERSIST, haloflickerblend, 0.0f, 0.075f, 16.0f);

HaloSurface halosurf;

void HaloSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n)
{
    w = max(int((w > 0 ? w : vieww * haloscale)), 2);
    h = max(int((h > 0 ? h : viewh * haloscale)), 2);
    n = MAX;
}

int HaloSurface::create(int w, int h, GLenum f, GLenum t, int count)
{
    useshaderbyname("hudhalodepth");
    useshaderbyname("hudhalotop");
    useshaderbyname("hudhalodepthref");
    useshaderbyname("hudhalotopref");

    halotype = -1;
    checkformat(w, h, f, t, count);

    return setup(w, h, f, t, count);
}

bool HaloSurface::check(bool check, bool val)
{
    if(!halos || buffers.empty()) return false;
    if(check && drawtex != DRAWTEX_HALO) return true;
    return halos && val;
}

bool HaloSurface::destroy()
{
    halotype = -1;
    return cleanup();
}

bool HaloSurface::swap(int index)
{
    if(index < 0 || index >= HaloSurface::MAX || halotype == index) return false;

    if(!bindfbo(index)) return false;

    halotype = index;
    return true;
}

bool HaloSurface::render(int w, int h, GLenum f, GLenum t, int count)
{
    if(!halos || hasnoview() || !create(w, h, f, t, count)) return false;

    int olddrawtex = drawtex;
    drawtex = DRAWTEX_HALO;

    projmatrix.perspective(fovy, aspect, nearplane, farplane);
    setcamprojmatrix();

    halotype = -1;
    savefbo();

    loopirev(MAX)
    {   // reverse order to avoid unnecessary swaps
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

    drawtex = olddrawtex;
    restorefbo();

    return true;
}

bool HaloSurface::draw(int x, int y, int w, int h)
{
    if(!halos || buffers.empty()) return false;

    if(w <= 0) w = vieww;
    if(h <= 0) h = viewh;

    hudmatrix.ortho(0, vieww, viewh, 0, -1, 1);
    resethudmatrix();
    resethudshader();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gle::color(halocolour.tocolor().mul(game::darkness(DARK_HALO)), haloblend);

    float maxdist = hud::radarlimit(halodist), scanlines = haloscanlines >= 0 ? haloscanlines : visorscanlines * fabs(haloscanlines);

    loopirev(MAX)
    {
        switch(i)
        {
            case DEPTH:
                if(hasrefractmask) SETSHADER(hudhalodepthref);
                else SETSHADER(hudhalodepth);
                break;
            case ONTOP:
                if(hasrefractmask) SETSHADER(hudhalotopref);
                else SETSHADER(hudhalotop);
                break;
            default: continue;
        }

        if(hasrefractmask)
        {
            glActiveTexture_(GL_TEXTURE0 + TEX_REFRACT_MASK);
            if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msrefracttex);
            else glBindTexture(GL_TEXTURE_RECTANGLE, refracttex);
        }

        glActiveTexture_(GL_TEXTURE0 + TEX_REFRACT_DEPTH);
        if(msaasamples) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
        else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);

        bindtex(i, 0);

        LOCALPARAMF(millis, lastmillis / 1000.0f);
        LOCALPARAMF(halosize, vieww, viewh, 1.0f / vieww, 1.0f / viewh);
        LOCALPARAMF(haloparams, maxdist, 1.0f / maxdist);
        LOCALPARAMF(halofx, scanlines, haloscanlineblend, halonoiseblend, haloflickerblend);
        vec4 color = vec4::fromcolor(haloscanlinemixcolour, haloscanlinemixblend);
        LOCALPARAMF(halofxcol, color.r, color.g, color.b, color.a);

        hudquad(x, y, w, h, 0, buffers[i]->height, buffers[i]->width, -buffers[i]->height);
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

void HazeSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n)
{
    w = max(int((w > 0 ? w : vieww)), 2);
    h = max(int((h > 0 ? h : viewh)), 2);
    f = hdrformat;
}

bool HazeSurface::check()
{
    return gethaze() != 0 || hazeparticles;
}

int HazeSurface::create(int w, int h, GLenum f, GLenum t, int count)
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

    checkformat(w, h, f, t, count);

    return setup(w, h, f, t, count);
}

bool HazeSurface::render(int w, int h, GLenum f, GLenum t, int count)
{
    if(!create(w, h, f, t, count)) return false;

    if(tex || hazeparticles)
    {
        if(msaalight)
        {
            glBindFramebuffer_(GL_READ_FRAMEBUFFER, mshdrfbo);
            glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, buffers[0]->fbo);
            glBlitFramebuffer_(0, 0, buffers[0]->width, buffers[0]->height, 0, 0, buffers[0]->width, buffers[0]->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer_(GL_FRAMEBUFFER, mshdrfbo);
        }
        else
        {
            bindtex(0, 0);
            glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, 0, 0, buffers[0]->width, buffers[0]->height);
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
            glBindTexture(GL_TEXTURE_RECTANGLE, buffers[0]->tex);
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

    if(hazeparticles) renderhazeparticles(buffers[0]->tex, hazemix);

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

void VisorSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n)
{
    w = max(int((w > 0 ? w : vieww)), 2);
    h = max(int((h > 0 ? h : viewh)), 2);
}

int VisorSurface::create(int w, int h, GLenum f, GLenum t, int count)
{
    checkformat(w, h, f, t, count);

    int sw = renderw, sh = renderh;
    if(gscale != 100)
    {   // world UI's use gdepth, so it needs to be at gscale
        sw = max((renderw*gscale + 99)/100, 1);
        sh = max((renderh*gscale + 99)/100, 1);
    }

    GLuint curfbo = renderfbo;
    bool restore = false;

    loopi(count)
    {
        if(buffers.inrange(i))
        {
            if(buffers[i]->check(i == WORLD ? sw : w, i == WORLD ? sh : h, f, t))
                restore = true;
            continue;
        }

        buffers.add(new RenderBuffer(i == WORLD ? sw : w, i == WORLD ? sh : h, f, t));
        restore = true;
    }

    if(restore) glBindFramebuffer_(GL_FRAMEBUFFER, curfbo);

    return buffers.length();
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

bool VisorSurface::render(int w, int h, GLenum f, GLenum t, int count)
{
    bool noview = hasnoview(), wantvisor = visorsurf.check();

    if(engineready && create(w, h, f, t, count))
    {
        if(wantvisor) visorsurf.coords(::cursorx, ::cursory, cursorx, cursory, true);
        else
        {
            cursorx = ::cursorx;
            cursory = ::cursory;
        }

        offsetx = offsety = 0;
        hud::visorinfo(offsetx, offsety);

        if(offsetx) cursorx += offsetx / buffers[0]->width;
        if(offsety) cursory += offsety / buffers[0]->height;

        enabled = true;
    }
    else
    {
        enabled = false;
        cursorx = cursory = offsetx = offsety = 0;
    }

    glEnable(GL_BLEND);

    if(enabled)
    {
        savefbo();

        glBlendFuncSeparate_(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE);

        loopi(MAX)
        {
            if(!bindfbo(i)) continue;

            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            hudmatrix.ortho(0, vieww, viewh, 0, -1, 1);
            flushhudmatrix();
            resethudshader();

            rendervisor = wantvisor ? i : -1;
            switch(i)
            {
                case BACKGROUND:
                {
                    UI::render(SURFACE_BACKGROUND);

                    hud::startrender(vieww, viewh, wantvisor, noview);

                    break;
                }
                case WORLD:
                {
                    if(progressing || noview) break; // skip world UI's when in progress or noview

                    bindgdepth();

                    glEnable(GL_DEPTH_TEST);
                    glDepthMask(GL_FALSE);

                    UI::render(SURFACE_WORLD);

                    glDepthMask(GL_TRUE);
                    glDisable(GL_DEPTH_TEST);

                    break;
                }
                case VISOR:
                {
                    if(progressing && wantvisor) UI::render(SURFACE_PROGRESS);
                    else UI::render(SURFACE_VISOR);

                    hud::visorrender(vieww, viewh, wantvisor, noview);

                    break;
                }
                case FOREGROUND:
                {
                    if(progressing && !wantvisor) UI::render(SURFACE_PROGRESS);
                    else UI::render(SURFACE_FOREGROUND);

                    hud::endrender(vieww, viewh, wantvisor, noview);

                    hudmatrix.ortho(0, vieww, viewh, 0, -1, 1);
                    flushhudmatrix();
                    resethudshader();

                    hud::drawpointers(vieww, viewh, getcursorx(), getcursory());

                    break;
                }
            }

            rendervisor = -1;
        }

        restorefbo();

        hudmatrix.ortho(0, vieww, viewh, 0, -1, 1);
        flushhudmatrix();
        resethudshader();

        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        gle::colorf(1, 1, 1, 1);

        loopi(MAX)
        {
            if(i == WORLD && (progressing || noview)) continue; // skip world UI's when in progress or noview

            bool visorok = i != 0 || (!noview && !progressing);

            if(wantvisor && i == 2 && visorok)
            {
                SETSHADER(hudvisorview);
                LOCALPARAMF(visorparams, visordistort, visornormal, visorscalex, visorscaley);
            }
            else SETSHADER(hudvisor);

            LOCALPARAMF(time, lastmillis / 1000.f);
            LOCALPARAMF(visorsize, vieww, viewh, 1.0f / vieww, 1.0f / viewh);

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

            bindtex(i, i ? -1 : 0);

            if(visortiltsurfaces&(1<<i) && !hud::hasinput(true))
                hudquad(offsetx, offsety, vieww, viewh, 0, buffers[i]->height, buffers[i]->width, -buffers[i]->height);
            else hudquad(0, 0, vieww, viewh, 0, buffers[i]->height, buffers[i]->width, -buffers[i]->height);
        }
    }
    else
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        hudmatrix.ortho(0, vieww, viewh, 0, -1, 1);
        flushhudmatrix();
        resethudshader();

        hud::startrender(vieww, viewh, false, noview);
        hud::visorrender(vieww, viewh, false, noview);
        hud::endrender(vieww, viewh, false, noview);
    }

    glDisable(GL_BLEND);

    return true;
}

void ViewSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n)
{
    int sw = renderw, sh = renderh;
    if(gscale != 100)
    { // make sure surface is not bigger than the gbuffer
        sw = max((renderw * gscale + 99)/100, 1);
        sh = max((renderh * gscale + 99)/100, 1);
    }

    w = min(sw, max(int(w > 0 ? w : vieww), 2));
    h = min(sh, max(int(h > 0 ? h : viewh), 2));
}

bool ViewSurface::render(int w, int h, GLenum f, GLenum t, int count)
{
    if(!create(w, h, f, t, count)) return false;

    savefbo();
    if(!bindfbo()) return false;

    savevfcP();
    float oldaspect = aspect, oldfovy = fovy, oldfov = curfov, oldnear = nearplane, oldfar = farplane, oldldrscale = ldrscale, oldldrscaleb = ldrscaleb;
    int olddrawtex = drawtex;
    drawtex = texmode;

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
    fixfullrange(camera1->yaw, camera1->pitch, camera1->roll);

    aspect = ratio > 0.0f ? ratio : buffers[0]->width/float(buffers[0]->height);
    curfov = fov;
    fovy = 2*atan2(tan(curfov/2*RAD), aspect)/RAD;
    setviewcell(camera1->o);

    nearplane = nearpoint;
    farplane = worldsize * farscale;

    gl_setupframe(true);

    vieww = buffers[0]->width;
    viewh = buffers[0]->height;

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    gl_drawview();
    copyhdr(buffers[0]->width, buffers[0]->height, buffers[0]->fbo);

    drawtex = olddrawtex;
    ldrscale = oldldrscale;
    ldrscaleb = oldldrscaleb;
    farplane = oldfar;
    nearplane = oldnear;
    aspect = oldaspect;
    fovy = oldfovy;
    curfov = oldfov;
    camera1 = oldcamera;

    projmatrix.perspective(fovy, aspect, nearplane, farplane);
    setcamprojmatrix();
    restorevfcP();
    restorefbo();

    return true;
}
