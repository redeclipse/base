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

VAR(IDF_PERSIST, visormode, 0, 1, 7); // bitwise: 1 = in-game, 2 = loading screen, 4 = while editing
FVAR(IDF_PERSIST, visordistort, -2, 2, 2);
FVAR(IDF_PERSIST, visornormal, -2, 1.175f, 2);
FVAR(IDF_PERSIST, visorscalex, FVAR_NONZERO, 0.9075f, 2);
FVAR(IDF_PERSIST, visorscaley, FVAR_NONZERO, 0.9075f, 2);

ICOMMANDV(0, visorenabled, visorsurf.check() ? 1 : 0);

VAR(IDF_PERSIST, showloadingaspect, 0, 1, 1);
VAR(IDF_PERSIST, showloadingmapbg, 0, 1, 1);
VAR(IDF_PERSIST, showloadinglogos, 0, 1, 1);

CVAR(IDF_PERSIST, backgroundcolour, 0x200000);
TVAR(IDF_PERSIST, backgroundtex, "<nocompress>textures/menubg", 3); // static fallback, a snapshot of the scene from backgroundsnapshot

// scene layout in screen height units from the centre of the screen, y down, fixed as the baked still is made from it
static const float BACKGROUNDECLIPSEY = -0.26f, BACKGROUNDECLIPSESIZE = 0.085f;
static const float BACKGROUNDECLIPSEOFFSET = 0.37f; // how far the sun peeks over the planet, in planet radii
static const float BACKGROUNDECLIPSESUN = 0.9f; // sun size relative to the planet
FVARR(backgroundhorizoncurve, 0.3f); // how far the horizon rises towards the sides, a var for the menumist composite
static const float BACKGROUNDHORIZONGLOW = 0.09f;
FVARR(backgroundhorizony, 0.1f);
FVAR(0, backgroundbeat, 0, 0, 1); // how hard the horizon glow is pulsing with the beat of the music, set by the menu UI each frame
// where the eclipse and horizon are centred on the screen, how far the scene is zoomed out about the eclipse across and its middle
// down, how far apart the figures either side of the eclipse are, and where the scene's middle is down the screen, from
// calcbackgroundlayout
FVAR(0, backgroundx, -2, -0.38f, 2);
FVAR(0, backgroundzoom, 0.1f, 1, 1);
FVAR(0, backgroundspread, 0, 1, 1);
FVAR(0, backgroundmiddle, 0, 0.5f, 1);
static const float BACKGROUNDLIFT = 0.05f; // how far the scene is raised up the screen, leaving room for the lyrics under the figures
// where the main menu sits, given by the menu UI to backgroundlayout as gameui_panel_calc_pos places it: it starts at most this far
// across as a fraction of the screen's width plus a shift in screen heights, and is this wide in screen heights; kept with the init
// vars so the loading screen lays out the baked still and the lettering the way the menu will from the start, whatever size the
// screen is now
FVAR(IDF_INIT, backgroundmenuleft, 0, 0.65f, 1);
FVAR(IDF_INIT, backgroundmenushift, -10, 0.1f, 10);
FVAR(IDF_INIT, backgroundmenuwidth, 0, 0.4f, 10);

// the logo's lettering beside the eclipse, as in the full logo: two planet radii tall, a little way past the planet and centred
// on the eclipse as a whole, sun and all, which runs from 1.27 radii above the planet's centre to one below
static const float BACKGROUNDLOGOGAP = 1.2f, BACKGROUNDLOGOWIDTH = 5.3f, BACKGROUNDLOGOMIDDLE = -0.135f; // in planet radii

// the baked still's shape, how much of the scene it holds top to bottom in screen heights, centred on the middle of the screen,
// and how far its eclipse is from its left edge: wide so it can be slid across to wherever the menu puts the eclipse, which is
// always left of the middle with the menu on the right, tall enough for the scene zoomed out to two fifths, as far as it goes on a
// square screen, and fixed so the part of it the view shows is known before it's loaded; the eclipse is placed so the lettering
// is in the middle of it, as it is in the middle of screens up to 3.2 wide, where the eclipse is furthest from the left edge
static const float BACKGROUNDSTILLASPECT = 3.2f/2.5f, BACKGROUNDSTILLSPAN = 2.5f,
                   BACKGROUNDSTILLECLIPSE = BACKGROUNDSTILLASPECT*BACKGROUNDSTILLSPAN*0.5f - BACKGROUNDECLIPSESIZE*(BACKGROUNDLOGOGAP + BACKGROUNDLOGOWIDTH*0.5f);
// where the lettering is within the logotext image, which is 1292x560
static const float BACKGROUNDLOGOASPECT = 1292/560.0f, BACKGROUNDLOGOLEFT = 94/1292.0f, BACKGROUNDLOGOTALL = 422/560.0f, BACKGROUNDLOGOCENTRE = 261/560.0f;
// the figures either side of the eclipse, see menubg_figures: how far out the outermost stand, how far their arms reach past that,
// and how close together they can be squeezed before the scene is zoomed out instead
static const float BACKGROUNDFIGUREREACH = 0.47f, BACKGROUNDFIGUREARMS = 0.2f, BACKGROUNDFIGURESPREAD = 0.6f;
static const float BACKGROUNDEDGEMARGIN = 0.02f; // the outermost figures are kept this far inside the left edge
static const float BACKGROUNDMENUGAP = 0.08f; // and the figures and lettering this far clear of the menu

// the logotext image's place beside an eclipse at x, y of radius r, in view heights
static void calcbackgroundlogo(float x, float y, float r, float &lx, float &ly, float &lw, float &lh)
{
    lh = r*2/BACKGROUNDLOGOTALL;
    lw = lh*BACKGROUNDLOGOASPECT;
    lx = x + r*BACKGROUNDLOGOGAP - lw*BACKGROUNDLOGOLEFT;
    ly = y + r*BACKGROUNDLOGOMIDDLE - lh*BACKGROUNDLOGOCENTRE;
}

// lays the scene out left of the menu on a screen of the given aspect: the figures either side of the eclipse and the lettering to
// its right fitted between the left edge and a gap before the menu, the figures squeezed together, and only once they can't be
// squeezed any further the whole scene zoomed out, so the lettering stays the same size beside the eclipse; then the eclipse is
// moved over so the lettering is in the middle of the screen, squeezing the figures further to make room for it on the left
static void calcbackgroundlayout(float aspect)
{
    float right = min(backgroundmenuleft*aspect + backgroundmenushift, aspect - backgroundmenuwidth),
          room = right - BACKGROUNDMENUGAP - BACKGROUNDEDGEMARGIN, logo = BACKGROUNDECLIPSESIZE*(BACKGROUNDLOGOGAP + BACKGROUNDLOGOWIDTH);
    #define FIGURESIDE(spread) (BACKGROUNDFIGUREREACH*(spread) + BACKGROUNDFIGUREARMS) // how far the figures reach either side of the eclipse
    #define SCENEWIDTH(spread) (FIGURESIDE(spread) + max(FIGURESIDE(spread), logo))
    backgroundzoom = backgroundspread = 1;
    if(room < SCENEWIDTH(1.0f))
    {
        if(room >= SCENEWIDTH(BACKGROUNDFIGURESPREAD))
        {   // squeezed as little as fits, the lettering reaching further to the right than the figures once they're close enough
            float lo = BACKGROUNDFIGURESPREAD, hi = 1;
            loopi(16)
            {
                float mid = (lo + hi)*0.5f;
                if(SCENEWIDTH(mid) <= room) lo = mid;
                else hi = mid;
            }
            backgroundspread = lo;
        }
        else
        {
            backgroundspread = BACKGROUNDFIGURESPREAD;
            backgroundzoom = clamp(room/SCENEWIDTH(BACKGROUNDFIGURESPREAD), 0.1f, 1.0f);
        }
    }
    float rightmost = right - BACKGROUNDMENUGAP - backgroundzoom*max(FIGURESIDE(backgroundspread), logo),
          middle = min(aspect*0.5f - backgroundzoom*BACKGROUNDECLIPSESIZE*(BACKGROUNDLOGOGAP + BACKGROUNDLOGOWIDTH*0.5f), rightmost),
          squeeze = ((middle - BACKGROUNDEDGEMARGIN)/backgroundzoom - BACKGROUNDFIGUREARMS)/BACKGROUNDFIGUREREACH;
    backgroundspread = clamp(squeeze, BACKGROUNDFIGURESPREAD, backgroundspread);
    backgroundx = max(middle, BACKGROUNDEDGEMARGIN + backgroundzoom*FIGURESIDE(backgroundspread)) - aspect*0.5f;
    backgroundmiddle = 0.5f - BACKGROUNDLIFT*backgroundzoom;
    #undef FIGURESIDE
    #undef SCENEWIDTH
}

// left: how far across the menu starts at most, as a fraction of the screen's width; width: how wide it is in screen heights;
// shift: how much further across it starts, in screen heights
ICOMMAND(0, backgroundlayout, "fff", (float *left, float *width, float *shift),
{
    backgroundmenuleft = clamp(*left, 0.0f, 1.0f);
    backgroundmenuwidth = clamp(*width, 0.0f, 10.0f);
    backgroundmenushift = clamp(*shift, -10.0f, 10.0f);
    calcbackgroundlayout(hudw/float(hudh));
});

// where the eclipse is on a screen of the given aspect, in screen heights from the top left
static void calcbackgroundeclipse(float aspect, float &x, float &y, float &r)
{
    x = aspect*0.5f + backgroundx;
    y = backgroundmiddle + BACKGROUNDECLIPSEY*backgroundzoom;
    r = BACKGROUNDECLIPSESIZE*backgroundzoom;
}

// the eclipse's place in UI coordinates, as x y r
ICOMMAND(0, backgroundeclipse, "", (),
{
    vec e;
    calcbackgroundeclipse(hudw/float(hudh), e.x, e.y, e.z);
    defformatstring(str, "%f %f %f", e.x, e.y, e.z);
    result(str);
});

// the lettering's place in UI coordinates, as x y w h
ICOMMAND(0, backgroundlogo, "", (),
{
    vec e;
    vec4 logo;
    calcbackgroundeclipse(hudw/float(hudh), e.x, e.y, e.z);
    calcbackgroundlogo(e.x, e.y, e.z, logo.x, logo.y, logo.z, logo.w);
    defformatstring(str, "%f %f %f %f", logo.x, logo.y, logo.z, logo.w);
    result(str);
});

// figures standing in the scene, set by the menu UI each frame
#define MAXBACKGROUNDFIGURES 6
static vec4 backgroundfigures[MAXBACKGROUNDFIGURES];
// w is the fade
ICOMMAND(0, backgroundfigure, "ifff", (int *n, float *x, float *y, float *h, float *fade),
    if(*n >= 0 && *n < MAXBACKGROUNDFIGURES) backgroundfigures[*n] = vec4(*x, *y, max(*h, 0.0f), clamp(*fade, 0.0f, 1.0f)));

// layer 0 is the scene, layer 1 is the fog drawn over anything standing in it
static const int BACKGROUNDFADEIN = 250; // millis to fade from the baked still into the live scene
static const int BACKGROUNDFADESTEP = 33; // the most the fade moves on in a frame, so slow first frames can't skip it
static bool backgroundlive = false;
static int backgroundfadelast = 0;
static float backgroundfade = 0; // how far the live scene has faded in

// how much of the baked still is left showing over the live scene
static float backgroundintro()
{
    return backgroundlive ? 1.0f - backgroundfade : 0.0f;
}

// noise the scene samples, precomputed in composites: the shared lattice, the sky across the screen and the god rays' veil around the sun
// the sky's wisps drift a pixel every 30ms or so, the veil barely moves, so they update only as often as that shows,
// at fixed rates so compositemindelay can't make them step visibly
#define BACKGROUNDNOISE "<comp:0,-256,2>noiselut"
#define BACKGROUNDSKY "<comp:-16,-512,3>menusky"
#define BACKGROUNDVEIL "<comp:-50,-256,1>fbmpolar [params2 = 3.6; drift1 = 0.05; drift2 = -0.03]"
// and the figures' mist across the screen, worked out once for all of them rather than again under each figure
#define BACKGROUNDMIST "<comp:-16,-512,2>menumist"
FVARR(backgroundaspect, 1.0f); // of the view the menusky and menumist composites are drawn for
VAR(0, debugbackground, 0, 0, 1); // 1 = compute the sky noise and mist in place rather than from the composites

static bool bindbackgroundnoise(bool precomputed)
{
    Texture *noise = textureload(BACKGROUNDNOISE, 0, true, false), *sky = NULL, *veil = NULL, *mist = NULL;
    if(precomputed && !debugbackground)
    {
        sky = textureload(BACKGROUNDSKY, 3, true, false);
        veil = textureload(BACKGROUNDVEIL, 2, true, false);
        mist = textureload(BACKGROUNDMIST, 3, true, false);
        if(!sky->rendered || !veil->rendered || !mist->rendered) sky = veil = mist = NULL; // not drawn yet
    }
    glActiveTexture_(GL_TEXTURE1);
    settexture(noise);
    if(sky && veil && mist)
    {
        glActiveTexture_(GL_TEXTURE2);
        settexture(sky);
        glActiveTexture_(GL_TEXTURE3);
        settexture(veil);
        glActiveTexture_(GL_TEXTURE5);
        settexture(mist);
    }
    glActiveTexture_(GL_TEXTURE0);
    return sky && veil && mist;
}

// whether the composites the live scene samples have all been drawn, starting them off if they haven't been asked for yet
static bool backgroundcompositesready()
{
    if(!textureload(BACKGROUNDNOISE, 0, true, false)->rendered) return false;
    if(debugbackground) return true;
    return textureload(BACKGROUNDSKY, 3, true, false)->rendered && textureload(BACKGROUNDVEIL, 2, true, false)->rendered &&
           textureload(BACKGROUNDMIST, 3, true, false)->rendered;
}

// the snapshot is not precomputed, the composites are only drawn for the view's aspect
// area is the part of the screen drawn over, in [(0, 0), (1, 1)]
static void drawbackgroundscene(float aspect, float time, float level, int layer = 0, bool precomputed = true, int figure = 0, const vec4 &area = vec4(0, 0, 1, 1))
{
    bool usecomp = bindbackgroundnoise(precomputed);
    hudbackgroundshader->set();
    LOCALPARAMF(scenefade, 1.0f - backgroundintro());
    LOCALPARAMF(usecomp, usecomp ? 1 : 0);
    LOCALPARAMF(time, time);
    LOCALPARAMF(aspect, aspect);
    LOCALPARAMF(level, level);
    LOCALPARAMF(layer, layer);
    LOCALPARAMF(eclipse, backgroundx, BACKGROUNDECLIPSEY, BACKGROUNDECLIPSESIZE, BACKGROUNDECLIPSEOFFSET);
    LOCALPARAMF(sunsize, BACKGROUNDECLIPSESUN);
    LOCALPARAMF(horizon, backgroundx, backgroundhorizony, backgroundhorizoncurve, BACKGROUNDHORIZONGLOW);
    LOCALPARAMF(beat, backgroundbeat);
    LOCALPARAMF(zoom, backgroundzoom);
    LOCALPARAMF(middle, backgroundmiddle);
    LOCALPARAMV(figures, backgroundfigures, MAXBACKGROUNDFIGURES);
    LOCALPARAMF(mistfigure, figure);
    // the lettering beside the eclipse, in the scene, for the corona's flames to wisp off
    vec4 letters;
    calcbackgroundlogo(backgroundx, BACKGROUNDECLIPSEY, BACKGROUNDECLIPSESIZE, letters.x, letters.y, letters.z, letters.w);
    LOCALPARAMF(letters, letters.x, letters.y, letters.z, letters.w);
    glActiveTexture_(GL_TEXTURE4);
    settexture(textureload(logotexttex, 3, true, false));
    glActiveTexture_(GL_TEXTURE0);
    hudquad(area.x, area.y, area.z, area.w);
}

static bool backgroundscene = false; // whether drawnoview drew the scene this frame

static void cleanupbackgroundstill();

void cleanupbackground()
{
    cleanupbackgroundstill();
}

// called by the menu UI just after each figure, so its mist is covered by those drawn in front of it
void drawbackgroundmist(int n)
{
    if(!backgroundscene || n < 0 || n >= MAXBACKGROUNDFIGURES || backgroundfigures[n].z <= 0) return;

    // only over where its mist can show, out to where it fades below what the shader draws, see layer 2 of hudbackground
    const vec4 &f = backgroundfigures[n];
    float aspect = viewh / float(vieww), reach = 2.45f, base = f.y - f.z*0.1f,
          x1 = max((f.x - f.z*0.95f*reach)*aspect + 0.5f, 0.0f), x2 = min((f.x + f.z*0.95f*reach)*aspect + 0.5f, 1.0f),
          y1 = max(base - f.z*0.17f*reach + backgroundmiddle, 0.0f), y2 = min(base + f.z*0.28f*reach + backgroundmiddle, 1.0f);
    if(x2 <= x1 || y2 <= y1) return;

    pushhudmatrix();
    hudmatrix.ortho(0, 1, 1, 0, -1, 1);
    flushhudmatrix();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawbackgroundscene(viewh / float(vieww), lastmillis / 1000.0f, game::darkness(DARK_UI), 2, true, n, vec4(x1, y1, x2 - x1, y2 - y1));
    pophudmatrix();
    resethudshader();
}

static void drawbackgroundfog()
{
    if(!backgroundscene) return;

    pushhudmatrix();
    hudmatrix.ortho(0, 1, 1, 0, -1, 1);
    flushhudmatrix();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawbackgroundscene(viewh / float(vieww), lastmillis / 1000.0f, game::darkness(DARK_UI), 1);
    pophudmatrix();
    resethudshader();

    loopi(MAXBACKGROUNDFIGURES) backgroundfigures[i] = vec4(0, 0, 0, 0); // registered again by the next frame's UI
}

// maps a scene position to UI coordinates, which are one unit high and uiaspect wide
ICOMMAND(0, backgroundpos, "ff", (float *x, float *y),
{
    defformatstring(str, "%f %f", hudw / float(hudh) * 0.5f + *x, backgroundmiddle + *y);
    result(str);
});

static void backgroundsnapshot(const char *name, int w)
{
    if(!engineready || !hudbackgroundshader || !*name || w <= 0) return;

    int sw = min(w, hwtexsize), sh = max(int(sw / BACKGROUNDSTILLASPECT + 0.5f), 1);
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
        // zoomed out to hold the scene's span top to bottom about its middle, with the eclipse as far in from the left as the still says
        float oldx = backgroundx, oldzoom = backgroundzoom, oldmiddle = backgroundmiddle, oldbeat = backgroundbeat;
        backgroundbeat = 0; // the still is caught between beats
        backgroundzoom = 1 / BACKGROUNDSTILLSPAN;
        backgroundmiddle = 0.5f;
        backgroundx = BACKGROUNDSTILLECLIPSE * backgroundzoom - sw / float(sh) * 0.5f;
        glDisable(GL_BLEND);
        drawbackgroundscene(sh / float(sw), 0, 1, 0, false);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawbackgroundscene(sh / float(sw), 0, 1, 1, false);
        backgroundx = oldx;
        backgroundzoom = oldzoom;
        backgroundmiddle = oldmiddle;
        backgroundbeat = oldbeat;
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
ICOMMAND(0, backgroundsnapshot, "si", (char *name, int *w), backgroundsnapshot(name, *w)); // the height follows from BACKGROUNDSTILLASPECT

// the part of the baked still the view shows, lined up with the live scene, as texture coordinates, and the part of the view that
// covers as fractions of it, which falls short of the whole view only where it shows more of the scene than the still holds
struct backgroundcrop { float tx, ty, tw, th, x, y, w, h; };

static bool calcbackgroundcrop(backgroundcrop &c)
{
    float aspect = vieww / float(viewh), ex = aspect * 0.5f + backgroundx, wide = BACKGROUNDSTILLASPECT * BACKGROUNDSTILLSPAN;
    float u0 = (BACKGROUNDSTILLECLIPSE - ex / backgroundzoom) / wide, u1 = (BACKGROUNDSTILLECLIPSE + (aspect - ex) / backgroundzoom) / wide,
          v0 = 0.5f - backgroundmiddle / (backgroundzoom * BACKGROUNDSTILLSPAN), v1 = 0.5f + (1 - backgroundmiddle) / (backgroundzoom * BACKGROUNDSTILLSPAN);
    c.tx = max(u0, 0.0f);
    c.ty = max(v0, 0.0f);
    c.tw = min(u1, 1.0f) - c.tx;
    c.th = min(v1, 1.0f) - c.ty;
    if(c.tw <= 0 || c.th <= 0) return false;
    c.x = (c.tx - u0) / (u1 - u0);
    c.y = (c.ty - v0) / (v1 - v0);
    c.w = c.tw / (u1 - u0);
    c.h = c.th / (v1 - v0);
    return true;
}

// the baked still, loaded as only the part the view shows at no more than the size it's shown so that's all that's kept in VRAM,
// and let go once the live scene has replaced it; that part is kept in memory too while it's in use, so when resetting GL takes
// the texture away it's put straight back rather than loaded all over again
static string backgroundstillname = "";
static bool backgroundstillfailed = false;
static ImageData backgroundstillimage;
static GLuint backgroundstilltex = 0;
static backgroundcrop backgroundstillcrop;

// lets the texture go when resetting GL, keeping the image to put back
static void cleanupbackgroundstill()
{
    if(backgroundstilltex) { glDeleteTextures(1, &backgroundstilltex); backgroundstilltex = 0; }
}

static void freebackgroundstill()
{
    if(backgroundstilltex) { glDeleteTextures(1, &backgroundstilltex); backgroundstilltex = 0; }
    backgroundstillimage.cleanup();
    backgroundstillname[0] = '\0';
    backgroundstillfailed = false;
}

// the still's texture, or 0 when there isn't one to show
static GLuint loadbackgroundstill()
{
    calcbackgroundlayout(vieww / float(viewh)); // the menu may not have laid the scene out for this screen yet
    backgroundcrop &c = backgroundstillcrop;
    if(!calcbackgroundcrop(c)) return 0;
    defformatstring(name, "<region:%.4f,%.4f,%.4f,%.4f><scale:%d,%d>%s", c.tx, c.ty, c.tw, c.th, max(int(c.w * vieww + 0.5f), 1), max(int(c.h * viewh + 0.5f), 1), backgroundtex);
    if(strcmp(name, backgroundstillname)) // the view or layout changed, so the part shown has too
    {
        freebackgroundstill();
        copystring(backgroundstillname, name);
        if(!loadimagedata(backgroundstillname, backgroundstillimage)) backgroundstillfailed = true; // not tried again until it changes
    }
    if(backgroundstillfailed) return 0;
    if(!backgroundstilltex) uploadimagedata(backgroundstilltex, backgroundstillimage, 3);
    return backgroundstilltex;
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

        // the live scene once the composites it samples are ready, the baked still is held until then
        backgroundaspect = vieww / float(viewh);
        if(engineready && hudbackgroundshader && (backgroundlive || backgroundcompositesready()))
        {
            if(!backgroundlive)
            {
                backgroundlive = true;
                backgroundfade = 0;
            }
            else backgroundfade = min(backgroundfade + clamp(totalmillis - backgroundfadelast, 0, BACKGROUNDFADESTEP) / float(BACKGROUNDFADEIN), 1.0f);
            backgroundfadelast = totalmillis;
            drawbackgroundscene(viewh / float(vieww), lastmillis / 1000.0f, level);
            backgroundscene = true;

            // dissolve from the baked still into the live scene, so nothing pops in
            float intro = backgroundintro();
            if(intro <= 0) freebackgroundstill();
            else if(GLuint still = loadbackgroundstill())
            {
                const backgroundcrop &c = backgroundstillcrop;
                hudshader->set();
                gle::colorf(level, level, level, intro);
                glBindTexture(GL_TEXTURE_2D, still);
                hudquad(c.x, c.y, c.w, c.h);
            }

            pophudmatrix();
            resethudshader();

            return false;
        }
        backgroundlive = false;

        // the background colour shows around the still where the view takes in more of the scene than it holds
        GLuint still = loadbackgroundstill();
        const backgroundcrop &c = backgroundstillcrop;
        if(hudnotextureshader && (!still || c.x > 0 || c.y > 0 || c.w < 1 || c.h < 1))
        {
            hudnotextureshader->set();
            gle::color(backgroundcolour.tocolor().mul(level), 1.f);
            hudquad(0, 0, 1, 1);
        }
        if(still)
        {
            hudshader->set();
            gle::colorf(level, level, level, 1);
            glActiveTexture_(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, still);
            hudquad(c.x, c.y, c.w, c.h);
        }

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

        // the lettering beside the eclipse in the baked still, as the menu lays it out over the live scene
        Texture *t = textureload(logotexttex, 3, true, false);
        if(loadbackgroundstill())
        {
            float ex, ey, er, x, y, w, h;
            calcbackgroundeclipse(vieww / float(viewh), ex, ey, er);
            calcbackgroundlogo(ex, ey, er, x, y, w, h);
            settexture(t);
            hudquad(x*viewh, y*viewh, w*viewh, h*viewh);
        }
        else
        {
            settexture(t);
            float w = vieww/3.0f, h = t && t->w ? w*t->h/float(t->w) : w/2;
            hudquad(vieww*2/3.0f - w/2, viewh/2 - vieww/24.0f - h/2, w, h);
        }
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

static int visorcurmode()
{
    if(progressing || !connected() || game::getprogresswait() != PROGRESS_NONE) return 2; // loading or waiting for the match
    return game::focusedent()->isediting() ? 4 : 1;
}

bool VisorSurface::check()
{
    return engineready && visormode&visorcurmode();
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

        if(visormode&visorcurmode() && !hud::hasinput(true))
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
