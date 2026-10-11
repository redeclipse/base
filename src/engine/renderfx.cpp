#include "engine.h"

static void gscaledims(int &w, int &h, float scale = 1.0f)
{
    w = max(int(ceilf(w * scale)), 1);
    h = max(int(ceilf(h * scale)), 1);

    if(gscale != 100)
    {
        w = max((w * gscale + 99) / 100, 1);
        h = max((h * gscale + 99) / 100, 1);
    }
}

bool RenderSurface::cleanup()
{
    buffers.deletecontents();
    return true;
}

void RenderSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n)
{
    w = max(int(w > 0 ? w : vieww), 1);
    h = max(int(h > 0 ? h : viewh), 1);
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
    if(hasnoview() || buffers.empty()) return;

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
        if(targ == GL_TEXTURE_2D) debugquad(sx, 0, sw, sh, 0, 0, 1, 1);
        else debugquad(sx, 0, sw, sh, 0, 0, buffers[i]->width, buffers[i]->height);
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

bool RenderSurface::copy(int index, GLuint fbo, int w, int h, bool linear, bool restore)
{
    if(!buffers.inrange(index)) return false;
    GLuint curfbo = renderfbo;

    GLERROR;
    glBindFramebuffer_(GL_READ_FRAMEBUFFER, fbo);
    glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, buffers[index]->fbo);
    glBlitFramebuffer_(0, 0, w, h, 0, 0, buffers[index]->width, buffers[index]->height, GL_COLOR_BUFFER_BIT, linear ? GL_LINEAR : GL_NEAREST);

    GLERROR;
    if(restore) glBindFramebuffer_(GL_FRAMEBUFFER, curfbo);

    return true;
}


static void setcompositedblend(bool precomposited)
{
    if(precomposited) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    else glBlendFuncSeparate_(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
}

VAR(0, debugaura, 0, 0, 2);
VAR(IDF_PERSIST, auras, 0, 1, 1);
VAR(IDF_PERSIST, auradist, 32, 2048, VAR_MAX);
FVAR(IDF_PERSIST, aurablend, 0, 1, 1);
FVAR(IDF_PERSIST, aurabright, FVAR_NONZERO, 1.5f, 16); // gain applied to the glow so it saturates near the model
FVAR(IDF_PERSIST, aurasize, FVAR_NONZERO, 0.008f, 0.1f); // glow width as a fraction of the view height
FVAR(IDF_PERSIST, auramasktolerance, 0, 2, FVAR_MAX);
FVAR(IDF_PERSIST, aurahidden, 0, 0, 1); // 0 = hole where an on-top model is hidden, otherwise colourise it

AuraSurface aurasurf;

void AuraSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n)
{
    w = renderw;
    h = renderh;
    gscaledims(w, h);
    t = GL_TEXTURE_2D;
    n = 1;
}

int AuraSurface::create(int w, int h, GLenum f, GLenum t, int count)
{
    useshaderbyname("hudaura");

    checkformat(w, h, f, t, count);

    int n = setup(w, h, f, t, count);
    if(buffers.inrange(0))
    {   // the glow is read from the mipmaps of the mask
        glBindTexture(GL_TEXTURE_2D, buffers[0]->tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }

    return n;
}

bool AuraSurface::check()
{
    return auras != 0;
}

void AuraSurface::reset()
{
    clearauramodels();
    numauras = 0;
    boundsmin = vec2(1, 1);
    boundsmax = vec2(0, 0);
}

void AuraSurface::add(const vec &center, float radius)
{
    if(!auras) return;

    vec eye;
    cammatrix.transform(center, eye);
    float depth = -eye.z;
    if(depth <= -radius) return;

    vec2 lo(0, 0), hi(1, 1);
    if(depth > radius + nearplane)
    {   // screen bounds of the model as fractions of the view
        vec4 pos;
        camprojmatrix.transform(center, pos);
        vec2 ndc = vec2(pos.x, pos.y).div(pos.w), size = vec2(fabs(projmatrix.a.x), fabs(projmatrix.b.y)).mul(radius / depth);
        lo = vec2(ndc).sub(size).mul(0.5f).add(0.5f);
        hi = vec2(ndc).add(size).mul(0.5f).add(0.5f);
        if(hi.x < 0 || hi.y < 0 || lo.x > 1 || lo.y > 1) return;
    }

    boundsmin.min(lo);
    boundsmax.max(hi);
    numauras++;
}

// mipmap level whose texels match the glow width, so it is the same at any resolution
static float auraglowlod(int height)
{
    return log2f(max(aurasize * height, 1.0f)) - 1;
}

static void bindearlydepth()
{
    glActiveTexture_(GL_TEXTURE0 + TEX_EARLY_DEPTH);
    if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msearlydepthtex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, earlydepthtex);
    glActiveTexture_(GL_TEXTURE0);
}

bool AuraSurface::render(int w, int h, GLenum f, GLenum t, int count)
{
    if(!auras || hasnoview() || !numauras || !create(w, h, f, t, count)) return false;

    timer *auratimer = begintimer("Aura Mask");

    savefbo();
    bindfbo(0);

    // the projection the g-buffer used, including any jitter, so the mask lines up with its depth
    matrix4 oldprojmatrix = projmatrix, oldnojittermatrix = nojittermatrix, oldcamprojmatrix = camprojmatrix;
    projmatrix.perspective(fovy, aspect, nearplane, farplane);
    setcamprojmatrix(false);

    int olddrawtex = drawtex;
    drawtex = DRAWTEX_AURA;

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    bindearlydepth();
    vec2 depthscale = renderdepthscale(vieww, viewh);
    GLOBALPARAMF(auramask, depthscale.x, depthscale.y, auramasktolerance);

    // models are drawn in no particular order, so visible pixels take priority over hidden ones
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation_(GL_MAX);
    glEnable(GL_CULL_FACE);

    renderauramaskbatches();

    glDisable(GL_CULL_FACE);
    glBlendEquation_(GL_FUNC_ADD);
    glDisable(GL_BLEND);

    drawtex = olddrawtex;

    projmatrix = oldprojmatrix;
    nojittermatrix = oldnojittermatrix;
    camprojmatrix = oldcamprojmatrix;
    GLOBALPARAM(camprojmatrix, camprojmatrix);
    GLOBALPARAM(lineardepthscale, projmatrix.lineardepthscale());

    restorefbo();

    // the glow never reads past two levels above its own, so skip generating the rest
    glBindTexture(GL_TEXTURE_2D, buffers[0]->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, max(int(ceilf(auraglowlod(buffers[0]->height))) + 2, 0));
    glGenerateMipmap_(GL_TEXTURE_2D);

    endtimer(auratimer);

    return true;
}

bool AuraSurface::draw(int x, int y, int w, int h)
{
    if(!auras || hasnoview() || !numauras || buffers.empty()) return false;

    // one quad over every aura so overlapping glows are not added twice
    float glow = aurasize * viewh;
    vec2 viewsize(vieww, viewh),
         lo = vec2(boundsmin).mul(viewsize).sub(glow).max(vec2(0, 0)),
         hi = vec2(boundsmax).mul(viewsize).add(glow).min(viewsize);
    if(lo.x >= hi.x || lo.y >= hi.y) return false;

    hudmatrix.ortho(0, vieww, 0, viewh, -1, 1);
    flushhudmatrix();

    SETSHADER(hudaura);
    bindtex(0, 0);

    LOCALPARAMF(auraparams, 1.0f / vieww, 1.0f / viewh, auraglowlod(buffers[0]->height), aurabright);
    LOCALPARAMF(auracolour, aurablend, aurahidden);

    // auras only ever brighten what is behind them
    glEnable(GL_BLEND);
    glBlendFuncSeparate_(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);

    hudquad(lo.x, lo.y, hi.x - lo.x, hi.y - lo.y);

    setcompositedblend(false);

    hudmatrix.ortho(0, vieww, viewh, 0, -1, 1);
    flushhudmatrix();

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
    w = max(int((w > 0 ? w : vieww)), 1);
    h = max(int((h > 0 ? h : viewh)), 1);
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

        if(tex)
        {
            glActiveTexture_(GL_TEXTURE0 + TEX_CURRENT_LIGHT);
            glBindTexture(GL_TEXTURE_RECTANGLE, buffers[0]->tex);
        }

        glActiveTexture_(GL_TEXTURE0 + TEX_EARLY_DEPTH);
        if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msearlydepthtex);
        else glBindTexture(GL_TEXTURE_RECTANGLE, earlydepthtex);

        glActiveTexture_(GL_TEXTURE0);

        const bvec &color = worldcols[WORLDCOL_F_HAZE];
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
            SETSHADER(hazetex);
        }
        else { SETSHADER(haze); }

        screenquad();

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    if(hazeparticles) renderhazeparticles(buffers[0]->tex, hazemix);

    return true;
}

VAR(0, debugvisor, 0, 0, 2);

VAR(IDF_PERSIST, visorglassallow, 0, 2, 2);
bool hasglass() { return visorglassallow && (visorglassallow >= 2 || hud::hasinput(true)); }

FVAR(IDF_PERSIST, visorglasssize, 0, 0.016f, 0.1f); // blur width as a fraction of the view height
FVAR(IDF_PERSIST, visorglasssizeload, 0, 0.008f, 0.1f);
FVAR(IDF_PERSIST, visorglassmix, FVAR_NONZERO, 4, FVAR_MAX);
FVAR(IDF_PERSIST, visorglassmin, 0, 0, 1);
FVAR(IDF_PERSIST, visorglassmax, 0, 1, 1);

VAR(IDF_PERSIST, visorrender, 0, 1, 2); // 0 = off, 1 = on except editing, 2 = always on
VAR(IDF_PERSIST, visorhud, 0, 1, 1);
FVAR(IDF_PERSIST, visordistort, -2, 2, 2);
FVAR(IDF_PERSIST, visornormal, -2, 1.175f, 2);
FVAR(IDF_PERSIST, visorscalex, FVAR_NONZERO, 0.9075f, 2);
FVAR(IDF_PERSIST, visorscaley, FVAR_NONZERO, 0.9075f, 2);

ICOMMANDV(0, visorenabled, visorsurf.check() ? 1 : 0);

VAR(IDF_PERSIST, showloadingaspect, 0, 1, 1);
VAR(IDF_PERSIST, showloadingmapbg, 0, 1, 1);
VAR(IDF_PERSIST, showloadinglogos, 0, 1, 1);

CVAR(IDF_PERSIST, backgroundcolour, 0x200000);
TVAR(IDF_PERSIST|IDF_PRELOAD, backgroundtex, "<nocompress>textures/menubg", 3); // static fallback, a snapshot of the scene from backgroundsnapshot

// scene layout in screen height units from the centre of the screen, y down, fixed as the baked still is made from it
static const float BACKGROUNDECLIPSEY = -0.3f, BACKGROUNDECLIPSESIZE = 0.085f;
static const float BACKGROUNDECLIPSEOFFSET = 0.37f; // how far the sun peeks over the planet, in planet radii
static const float BACKGROUNDECLIPSESUN = 0.9f; // sun size relative to the planet
static const float BACKGROUNDHORIZONCURVE = 0.3f; // how far the horizon rises towards the sides
static const float BACKGROUNDHORIZONGLOW = 0.09f;
FVARR(backgroundhorizony, 0.1f);
FVAR(0, backgroundx, -2, -0.38f, 2); // where the eclipse and horizon are centred, moved by the menu UI to keep clear of the menu

// figures standing in the scene, set by the menu UI each frame: glitched ones are cut up, others are kept clear of it
#define MAXBACKGROUNDFIGURES 6
static vec4 backgroundfigures[MAXBACKGROUNDFIGURES];
// w is minus the fade for glitched figures, or one plus the fade for those the glitch keeps clear of
ICOMMAND(0, backgroundfigure, "ifffif", (int *n, float *x, float *y, float *h, int *glitch, float *fade),
    if(*n >= 0 && *n < MAXBACKGROUNDFIGURES) backgroundfigures[*n] = vec4(*x, *y, max(*h, 0.0f), (*glitch ? -1 : 1)*clamp(*fade, 0.0f, 1.0f) + (*glitch ? 0 : 1)));

// layer 0 is the scene, layer 1 is the fog and glow drawn over anything standing in it, layer 2 is the glitch
static const int BACKGROUNDFADEIN = 250; // millis to fade from the baked still into the live scene
static int backgroundlivestart = 0;

// how much of the baked still is left showing over the live scene
static float backgroundintro()
{
    if(!backgroundlivestart) return 0;
    return 1.0f - clamp((totalmillis - backgroundlivestart) / float(BACKGROUNDFADEIN), 0.0f, 1.0f);
}

static void drawbackgroundscene(float aspect, float time, float level, int layer = 0, bool glitch = false)
{
    hudbackgroundshader->set();
    LOCALPARAMF(scenefade, 1.0f - backgroundintro());
    LOCALPARAMF(glitch, glitch ? 1 : 0);
    LOCALPARAMF(time, time);
    LOCALPARAMF(aspect, aspect);
    LOCALPARAMF(level, level);
    LOCALPARAMF(layer, layer);
    LOCALPARAMF(eclipse, backgroundx, BACKGROUNDECLIPSEY, BACKGROUNDECLIPSESIZE, BACKGROUNDECLIPSEOFFSET);
    LOCALPARAMF(sunsize, BACKGROUNDECLIPSESUN);
    LOCALPARAMF(horizon, backgroundx, backgroundhorizony, BACKGROUNDHORIZONCURVE, BACKGROUNDHORIZONGLOW);
    LOCALPARAMV(figures, backgroundfigures, MAXBACKGROUNDFIGURES);
    hudquad(0, 0, 1, 1);
}

static bool backgroundscene = false; // whether drawnoview drew the scene this frame
static GLuint backgroundcopytex = 0;
static int backgroundcopyw = 0, backgroundcopyh = 0;

void cleanupbackground()
{
    if(backgroundcopytex) { glDeleteTextures(1, &backgroundcopytex); backgroundcopytex = 0; }
    backgroundcopyw = backgroundcopyh = 0;
}

// copies what has been drawn so far, figures included, for the glitch and glow to work from
static bool copybackgroundframe()
{
    bool figures = false;
    loopi(MAXBACKGROUNDFIGURES) if(backgroundfigures[i].z > 0) figures = true;
    if(!figures) return false;

    if(!backgroundcopytex) glGenTextures(1, &backgroundcopytex);
    if(backgroundcopyw != vieww || backgroundcopyh != viewh)
    {
        createtexture(backgroundcopytex, vieww, viewh, NULL, 3, 1, GL_RGB8, GL_TEXTURE_2D);
        backgroundcopyw = vieww;
        backgroundcopyh = viewh;
    }
    glActiveTexture_(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, backgroundcopytex);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, vieww, viewh);
    return true;
}

static void drawbackgroundlayer(int layer)
{
    bool copied = copybackgroundframe();

    pushhudmatrix();
    hudmatrix.ortho(0, 1, 1, 0, -1, 1);
    flushhudmatrix();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawbackgroundscene(viewh / float(vieww), lastmillis / 1000.0f, game::darkness(DARK_UI), layer, copied);
    pophudmatrix();
    resethudshader();
}

// called by the menu UI between figures, so the glitch cuts up those drawn before it and none after
void drawbackgroundglitch()
{
    if(backgroundscene) drawbackgroundlayer(2);
}

static void drawbackgroundfog()
{
    if(!backgroundscene) return;
    drawbackgroundlayer(1);
    loopi(MAXBACKGROUNDFIGURES) backgroundfigures[i] = vec4(0, 0, 0, 0); // registered again by the next frame's UI
}

// maps a scene position to UI coordinates, which are one unit high and uiaspect wide
ICOMMAND(0, backgroundpos, "ff", (float *x, float *y),
{
    defformatstring(str, "%f %f", hudw / float(hudh) * 0.5f + *x, 0.5f + *y);
    result(str);
});

static void backgroundsnapshot(const char *name, int w, int h)
{
    if(!engineready || !hudbackgroundshader || !*name || w <= 0 || h <= 0) return;

    int sw = min(w, hwtexsize), sh = min(h, hwtexsize);
    GLuint tex, fbo;
    glGenTextures(1, &tex);
    glGenFramebuffers_(1, &fbo);
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
    createtexture(tex, sw, sh, NULL, 3, 1, GL_RGB8, GL_TEXTURE_2D);
    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
    {
        glViewport(0, 0, sw, sh);

        pushhudmatrix();
        hudmatrix.ortho(0, 1, 0, 1, -1, 1); // flipped so the rows read back top down
        flushhudmatrix();
        glDisable(GL_BLEND);
        drawbackgroundscene(sh / float(sw), 0, 1);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawbackgroundscene(sh / float(sw), 0, 1, 1);
        pophudmatrix();
        resethudshader();

        ImageData image(sw, sh, 3);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, sw, sh, GL_RGB, GL_UNSIGNED_BYTE, image.data);
        saveimage(name, image, imageformat, compresslevel, false);
        conoutf(colourwhite, "Saved background snapshot: %s (%dx%d)", name, sw, sh);
    }
    else conoutf(colourred, "Error: Failed allocating background snapshot buffer");

    glBindFramebuffer_(GL_FRAMEBUFFER, renderfbo);
    glViewport(0, 0, hudw, hudh);
    glDeleteFramebuffers_(1, &fbo);
    glDeleteTextures(1, &tex);
}
ICOMMAND(0, backgroundsnapshot, "sii", (char *name, int *w, int *h), backgroundsnapshot(name, *w, *h));

// crops the background texture to cover the view without stretching
static void calcbackgroundcrop(Texture *t, float &offsetx, float &offsety)
{
    float viewratio = viewh / float(vieww), bgratio = t->h / float(t->w);
    if(viewratio < bgratio) offsety = (1.0f - viewh / (t->h * vieww / float(t->w))) * 0.5f;
    else offsetx = (1.0f - vieww / (t->w * viewh / float(t->h))) * 0.5f;
}

VisorSurface visorsurf;
VARR(rendervisor, -1);

bool VisorSurface::drawnoview()
{
    float level = game::darkness(DARK_UI);

    gle::colorf(level, level, level, 1);

    Texture *t = NULL;
    if(engineready && showloadingmapbg && *mapname && strcmp(mapname, "maps/untitled"))
        t = textureload(mapname, 3, true, false);

    config.reset();
    backgroundscene = false;

    if(!engineready || !t || t == notexture)
    {
        pushhudmatrix();
        hudmatrix.ortho(0, 1, 1, 0, -1, 1);
        flushhudmatrix();

        if(engineready && hudbackgroundshader)
        {
            if(!backgroundlivestart) backgroundlivestart = max(totalmillis, 1);
            drawbackgroundscene(viewh / float(vieww), lastmillis / 1000.0f, level);
            backgroundscene = true;

            // dissolve from the baked still into the live scene, so nothing pops in
            float intro = backgroundintro();
            if(intro > 0 && (t = textureload(backgroundtex, 3, true, false)) && t != notexture)
            {
                hudshader->set();
                gle::colorf(level, level, level, intro);
                settexture(t);
                float ox = 0, oy = 0;
                calcbackgroundcrop(t, ox, oy);
                hudquad(0, 0, 1, 1, ox, oy, 1.0f - ox - ox, 1.0f - oy - oy);
            }

            pophudmatrix();
            resethudshader();

            return false;
        }
        backgroundlivestart = 0;

        t = textureload(backgroundtex, 3, true, false);

        if(t && t != notexture)
        {
            glActiveTexture_(GL_TEXTURE0);
            settexture(t);
            calcbackgroundcrop(t, config.offsetx, config.offsety);
        }
        else if(hudnotextureshader)
        {
            hudnotextureshader->set();
            gle::color(backgroundcolour.tocolor().mul(level), 1.f);
        }
        else
        {
            pophudmatrix();
            resethudshader();

            return false;
        }

        hudquad(0, 0, 1, 1, config.offsetx, config.offsety, 1.0f - config.offsetx - config.offsetx, 1.0f - config.offsety - config.offsety);

        pophudmatrix();
        resethudshader();

        return false;
    }

    settexture(t);

    if(showloadingaspect)
    {
        if(vieww > viewh) config.offsety = ((vieww - viewh) / float(vieww)) * 0.5f;
        else if(viewh > vieww) config.offsetx = ((viewh - vieww) / float(viewh)) * 0.5f;
    }

    hudquad(0, 0, vieww, viewh, config.offsetx, config.offsety, 1.0f - config.offsetx, 1.0f - config.offsety);

    return true;
}

void VisorSurface::drawprogress()
{
    if(!progressing || engineready) return;

    if(showloadinglogos)
    {
        gle::colorf(1, 1, 1, 1);

        Texture *t = textureload(logotexttex, 3, true, false); // the background has the eclipse
        settexture(t);
        float w = vieww/3.0f, h = t && t->w ? w*t->h/float(t->w) : w/2;
        hudquad(vieww*2/3.0f - w/2, viewh/2 - vieww/24.0f - h/2, w, h);
    }

    float oldtextscale = curtextscale;
    pushfont(textfontlogo);
    curtextscale = 0.75f;
    draw_textf("%s", FONTH/2, viewh - FONTH * 5 / 4, 0, 0, 255, 255, 255, 255, TEXT_LEFT_JUSTIFY, -1, -1, 1, game::getprogresstitle());
    if(progressamt > 0) draw_textf("[ %.1f%% ]", vieww - FONTH/2, viewh - FONTH * 5/4, 0, 0, 255, 255, 255, 255, TEXT_RIGHT_JUSTIFY, -1, -1, 1, progressamt*100);
    curtextscale = oldtextscale;
    popfont();
    resethudshader();
}

void VisorSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n)
{
    w = max(int((w > 0 ? w : vieww)), 1);
    h = max(int((h > 0 ? h : viewh)), 1);
}

int VisorSurface::create(int w, int h, GLenum f, GLenum t, int count)
{
    checkformat(w, h, f, t, count);

    GLuint curfbo = renderfbo;
    bool restore = false;

    loopi(count)
    {
        int cw = w, ch = h;
        GLenum format = f, target = t;

        switch(i)
        {
            case WORLD:
                gscaledims(cw, ch);
                break;
            case GLASS:
            {   // quarter size, the downsample averages each 4x4 block and the mipmaps do the rest
                cw = max(w / 4, 1);
                ch = max(h / 4, 1);
                target = GL_TEXTURE_2D;
                break;
            }
            default: break;
        }

        if(buffers.inrange(i))
        {
            if(!buffers[i]->check(cw, ch, format, target)) continue;
        }
        else buffers.add(new RenderBuffer(cw, ch, format, target));

        if(i == GLASS)
        {   // the blur is read from the mipmaps
            glBindTexture(GL_TEXTURE_2D, buffers[i]->tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        }
        restore = true;
    }

    if(restore) glBindFramebuffer_(GL_FRAMEBUFFER, curfbo);

    return buffers.length();
}

bool VisorSurface::check()
{
    return engineready && visorhud;
}

void VisorSurface::coords(float cx, float cy, float &vx, float &vy, bool back)
{
    // WARNING: This function MUST produce the same
    // results as the 'hudvisor' shader for cursor projection.

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
        case -1: return cursorx; // force cursor
        case 1: return config.cursorx; // force visor
        default: break;
    }
    return rendervisor == VISOR ? config.cursorx : cursorx;
}

float VisorSurface::getcursory(int type)
{
    switch(type)
    {
        case -1: return ::cursory; // force cursor
        case 1: return cursory; // force visor
        default: break;
    }
    return rendervisor == VISOR ? cursory : ::cursory;
}

bool VisorSurface::render(int w, int h, GLenum f, GLenum t, int count)
{
    bool noview = hasnoview(), wantvisor = visorsurf.check();

    if(engineready && create(w, h, f, t, count))
    {
        // setup our coordinate system for the visor if we're ok to proceed
        config.reset();

        if(wantvisor) visorsurf.coords(cursorx, cursory, config.cursorx, config.cursory, true);
        else
        {
            config.cursorx = cursorx;
            config.cursory = cursory;
        }

        hud::visorinfo(config, noview);

        if(config.offsetx) config.cursorx += config.offsetx / buffers[0]->width;
        if(config.offsety) config.cursory += config.offsety / buffers[0]->height;

        config.enabled = true;
    }
    else config.clear();

    glEnable(GL_BLEND);

    if(config.enabled)
    {
        // initialise some stuff we use at different stages below
      
        bool wantblur = false; // force the blur for things like the progress screen
        savefbo();

        setcompositedblend(false);

        // render out the various layers so we can composite them together later
        loopi(BUFFERS)
        {
            if((i == WORLD && noview) || !bindfbo(i)) continue;

            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            hudmatrix.ortho(0, vieww, viewh, 0, -1, 1);
            flushhudmatrix();
            resethudshader();

            rendervisor = wantvisor ? i : -1;
            switch(i)
            {
                case WORLD:
                {
                    // world UI's use gdepth for depth testing

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
                    // main visor rendering

                    UI::render(progressing ? SURFACE_PROGRESS : SURFACE_VISOR);
                    hud::visorrender(vieww, viewh, wantvisor, noview);

                    break;
                }
                case HUD:
                {
                    if(noview) drawprogress();
                    else
                    {
                        UI::render(SURFACE_BACKGROUND);
                        hud::startrender(vieww, viewh, wantvisor, noview);

                        hudmatrix.ortho(0, vieww, viewh, 0, -1, 1);
                        flushhudmatrix();
                        resethudshader();

                        hudrectshader->set();
                        setcompositedblend(true); // WORLD buffer contains composited content
                        bindtex(WORLD, 0);
                        hudquad(0, 0, vieww, viewh, 0, buffers[WORLD]->height, buffers[WORLD]->width, -buffers[WORLD]->height);
                        setcompositedblend(false);
                    }

                    if(wantvisor)
                    {
                        SETSHADER(hudvisor);
                        LOCALPARAMF(visorsize, vieww, viewh, 1.0f / vieww, 1.0f / viewh);
                        LOCALPARAMF(visorparams, visordistort, visornormal, visorscalex, visorscaley);
                    }
                    else hudrectshader->set();

                    setcompositedblend(true); // VISOR buffer contains composited content
                    bindtex(VISOR, 0);
                    if(!noview && !hud::hasinput(true))
                        hudquad(config.offsetx, config.offsety, vieww, viewh, 0, buffers[VISOR]->height, buffers[VISOR]->width, -buffers[VISOR]->height);
                    else hudquad(0, 0, vieww, viewh, 0, buffers[VISOR]->height, buffers[VISOR]->width, -buffers[VISOR]->height);
                    
                    setcompositedblend(false);

                    if(!progressing) UI::render(SURFACE_FOREGROUND);

                    hud::endrender(vieww, viewh, wantvisor, noview);

                    if(!progressing)
                    {
                        hudmatrix.ortho(0, vieww, viewh, 0, -1, 1);
                        flushhudmatrix();
                        resethudshader();

                        hud::drawpointers(vieww, viewh, getcursorx(), getcursory());
                    }

                    break;
                }
                case BLIT:
                {
                    // this contains our final image of the viewport
                    if(noview)
                    {
                        wantblur = drawnoview();
                        if(engineready && !wantblur && !progressing)
                        {   // menu scene UI's stand in the background, behind its fog
                            UI::render(SURFACE_BACKGROUND);
                            drawbackgroundfog();
                        }
                    }
                    else
                    {
                        doscale(renderfbo, vieww, viewh);
                        aurasurf.draw(); // part of the world image, so it sits under the visor and glass
                    }

                    break;
                }
            }

            rendervisor = -1;
        }

        glBlendFunc(GL_ONE, GL_ZERO);

        // create a blurred copy of the BLIT buffer
        float glasslod = 0;
        if(wantblur || hasglass())
        {
            if(bindfbo(GLASS))
            {
                SETSHADER(hudglassdown);
                LOCALPARAMF(glassscale, buffers[BLIT]->width / float(buffers[GLASS]->width), buffers[BLIT]->height / float(buffers[GLASS]->height));
                bindtex(BLIT, 0);
                screenquad();
            }

            // mipmap level whose texels match the blur width, the copy is already quarter size
            glasslod = max(log2f(max((wantblur ? visorglasssizeload : visorglasssize) * buffers[BLIT]->height, 1.0f)) - 2, 0.0f);
            glBindTexture(GL_TEXTURE_2D, buffers[GLASS]->tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, int(glasslod) + 1);
            glGenerateMipmap_(GL_TEXTURE_2D);

            if(wantblur && bindfbo(BLIT))
            {
                SETSHADER(hudglass);
                LOCALPARAMF(glassparams, 1.0f / buffers[BLIT]->width, 1.0f / buffers[BLIT]->height, buffers[GLASS]->width, buffers[GLASS]->height);
                LOCALPARAMF(glasslod, glasslod);
                bindtex(GLASS, 0);
                screenquad();
            }
        }

        restorefbo();

        hudmatrix.ortho(0, vieww, viewh, 0, -1, 1);
        flushhudmatrix();
        resethudshader();

        GLOBALPARAMF(visortime, lastmillis / 1000.f);
        GLOBALPARAMF(visorsize, vieww, viewh, 1.0f / vieww, 1.0f / viewh);
        GLOBALPARAMF(visorfx, clamp(config.glitch, 0.0f, 1.0f), 1.0f + config.saturate, config.narrow > 0.0f ? 1.0f / config.narrow : (config.narrow < 0.0f ? 0.0f : 1e16f));

        // final operations on the viewport before overlaying the UI/visor elements
        if(!engineready)
        {
            hudrectshader->set();
            bindtex(BLIT, 0);
        }
        else if(wantblur || !hasglass())
        {
            // want blur or don't have glass turned on
            SETSHADER(hudblit);
            bindtex(BLIT, 0);
        }
        else
        {
            // glass does alpha blurring and other visor effects
            SETSHADER(hudblitglass);

            LOCALPARAMF(blitglass, visorglassmin, visorglassmax, visorglassmix, config.bluramt);
            LOCALPARAMF(glassparams, 1.0f / buffers[BLIT]->width, 1.0f / buffers[BLIT]->height, buffers[GLASS]->width, buffers[GLASS]->height);
            LOCALPARAMF(glasslod, glasslod);
            
            loopi(COUNT) bindtex(START + i, i);
        }
        
        hudquad(0, 0, vieww, viewh, 0, buffers[BLIT]->height, buffers[BLIT]->width, -buffers[BLIT]->height);

        setcompositedblend(true); // HUD buffer contains composited content

        if(visorrender >= (game::focusedent()->isediting() ? 2 : 1) && !hud::hasinput(true))
            SETSHADER(hudrender);
        else hudrectshader->set();

        bindtex(HUD, 0);
        hudquad(0, 0, vieww, viewh, 0, buffers[HUD]->height, buffers[HUD]->width, -buffers[HUD]->height);
    }
    else
    {
        // failsafe for when we can't render the visor
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        hudmatrix.ortho(0, vieww, viewh, 0, -1, 1);
        flushhudmatrix();
        resethudshader();

        if(noview)
        {
            drawnoview();
            drawprogress();
        }

        hud::startrender(vieww, viewh, false, noview);
        hud::visorrender(vieww, viewh, false, noview);
        hud::endrender(vieww, viewh, false, noview);
    }

    glDisable(GL_BLEND);

    return true;
}

void ViewSurface::checkformat(int &w, int &h, GLenum &f, GLenum &t, int &n)
{
    w = max(int(w > 0 ? w : vieww), 1);
    h = max(int(h > 0 ? h : viewh), 1);
    gscaledims(w, h);

    if(gw > 0 && gh > 0 && (w > gw || h > gh))
    {   // the view is drawn into the corner of the g-buffer, so it has to fit inside it
        float scale = min(gw / float(w), gh / float(h));
        w = clamp(int(w * scale), 1, gw);
        h = clamp(int(h * scale), 1, gh);
    }
}

bool ViewSurface::destroy()
{
    if(oqstate)
    {
        delete oqstate;
        oqstate = NULL;
    }
    return cleanup();
}

bool ViewSurface::render(int w, int h, GLenum f, GLenum t, int count)
{
    if(!create(w, h, f, t, count)) return false;

    savefbo();
    if(!bindfbo()) return false;

    //if(!oqstate) oqstate = new OQState();
    //pushoqstate(oqstate);

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

    aspect = ratio > 0.0f ? ratio : buffers[0]->width / float(buffers[0]->height);
    curfov = fov;
    fovy = 2.0f * atan2(tan(curfov * 0.5f * RAD), aspect) / RAD;

    nearplane = nearpoint;
    farplane = worldsize * farscale;

    projmatrix.perspective(fovy, aspect, nearplane, farplane);
    setcamprojmatrix();
    setviewcell(camera1->o);

    gl_setupframe(true);

    vieww = buffers[0]->width;
    viewh = buffers[0]->height;

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    saveocclusion();
    gl_drawview();
    restoreocclusion();
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
    restorefbo();
    //popoqstate();

    return true;
}
