#include "engine.h"

static GLuint halofbo = 0, halotex = 0;
static int halow = -1, haloh = -1;

VAR(0, debughalo, 0, 0, 2);
VAR(IDF_PERSIST, halos, 0, 1, 1);
FVAR(IDF_PERSIST, halowireframe, 0, 0, FVAR_MAX);
VAR(IDF_PERSIST, halodist, 32, 1024, VAR_MAX);
FVARF(IDF_PERSIST, haloscale, FVAR_NONZERO, 1, 1, cleanuphalo());
FVAR(IDF_PERSIST, haloblend, 0, 1, 1);
CVAR(IDF_PERSIST, halocolour, 0xFFFFFF);
VARF(IDF_PERSIST, halooffset, 1, 2, 4, initwarning("halo setup", INIT_LOAD, CHANGE_SHADERS));
FVAR(IDF_PERSIST, halofade, FVAR_NONZERO, 0.25f, FVAR_MAX);

void setuphalo(int w, int h)
{
    w = max(int(w*haloscale), 2);
    h = max(int(h*haloscale), 2);
    if(w != halow || h != haloh)
    {
        cleanuphalo();
        halow = w;
        haloh = h;
    }

    GLERROR;
    if(!halotex)
    {
        glGenTextures(1, &halotex);
        createtexture(halotex, halow, haloh, NULL, 3, 1, GL_RGB5_A1, GL_TEXTURE_RECTANGLE);
    }

    if(!halofbo)
    {
        glGenFramebuffers_(1, &halofbo);
        glBindFramebuffer_(GL_FRAMEBUFFER, halofbo);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, halotex, 0);
    }
    GLERROR;
}

void cleanuphalo()
{
    if(halofbo)
    {
        glDeleteFramebuffers_(1, &halofbo);
        halofbo = 0;
    }

    if(halotex)
    {
        glDeleteTextures(1, &halotex);
        halotex = 0;
    }
}

void renderhalo()
{
    setuphalo(vieww, viewh);

    glBindFramebuffer_(GL_FRAMEBUFFER, halofbo);
    glViewport(0, 0, halow, haloh);

    resetmodelbatches();

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_CULL_FACE);

    if(halowireframe > 0)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(halowireframe);
    }

    game::render();
    renderhalomodelbatches();
    renderavatar();

    if(halowireframe > 0)
    {
        glLineWidth(1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glDisable(GL_CULL_FACE);

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
}

void viewhalo()
{
    if(!halotex) return;
    int w = min(hudw, hudh)/(debughalo == 2 ? 2 : 3), h = (w*hudh)/hudw;
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, halotex);
    debugquad(0, 0, w, h, 0, 0, halow, haloh);
}

void blendhalos()
{
    if(!halos) return;

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, hudw, hudh);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    gle::color(halocolour.tocolor(), haloblend);
    glEnable(GL_BLEND);

    glBindTexture(GL_TEXTURE_RECTANGLE, halotex);

    SETSHADER(hudhalo);
    LOCALPARAMF(halofade, 1.0f / halofade);

    hudquad(0, 0, hudw, hudh, 0, haloh, halow, -haloh);

    glDisable(GL_BLEND);
}

#define MPVVARS(name, type) \
    VARF(IDF_MAP, haze##name, 0, 0, 1, inithaze()); \
    CVAR(IDF_MAP, hazecolour##name, 0); \
    FVAR(IDF_MAP, hazecolourmix##name, 0, 0.5f, 1); \
    FVAR(IDF_MAP, hazeblend##name, 0, 1, 1); \
    SVARF(IDF_MAP, hazetex##name, "textures/watern", inithaze()); \
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

MPVVARS(, MPV_DEF);
MPVVARS(alt, MPV_ALT);

#define GETMPV(name, type) \
    type get##name() \
    { \
        if(checkmapvariant(MPV_ALT)) return name##alt; \
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

Texture *hazetexture = NULL;
GLuint hazertex = 0, hazerfbo = 0;
GLenum hazerformat = 0;
int hazew = 0, hazeh = 0;
VAR(0, debughaze, 0, 0, 2);

void setuphaze(int w, int h)
{
    if(w != hazew || h != hazeh || hazerformat != hdrformat)
    {
        cleanuphaze();
        hazew = w;
        hazeh = h;
        hazerformat = hdrformat;
    }

    GLERROR;
    if(!hazertex)
    {
        glGenTextures(1, &hazertex);
        createtexture(hazertex, hazew, hazeh, NULL, 3, 1, hazerformat, GL_TEXTURE_RECTANGLE);
    }
    if(msaalight && !hazerfbo)
    {
        glGenFramebuffers_(1, &hazerfbo);
        glBindFramebuffer_(GL_FRAMEBUFFER, hazerfbo);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, hazertex, 0);
        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fatal("Failed allocating haze buffer!");
        glBindFramebuffer_(GL_FRAMEBUFFER, mshdrfbo);
    }
    GLERROR;
}

void cleanuphaze()
{
    if(hazerfbo) { glDeleteFramebuffers_(1, &hazerfbo); hazerfbo = 0; }
    if(hazertex) { glDeleteTextures(1, &hazertex); hazertex = 0; }
}

void inithaze()
{
    hazetexture = NULL;
    if(!gethaze()) return;
    const char *hazename = gethazetex();
    if(hazename[0]) hazetexture = textureload(hazename, 0, true, false);
    if(hazetexture && hazetexture != notexture)
    {
        useshaderbyname("hazetex");
        useshaderbyname("hazetexref");
    }
    else
    {
        useshaderbyname("haze");
        useshaderbyname("hazeref");
    }
}

void renderhaze()
{
    bool hashaze = gethaze() != 0;
    if(!hashaze && !hazeparticles) return;

    setuphaze(vieww, viewh);

    bool textured = hashaze && hazetexture && hazetexture != notexture, hazemix = false;
    if(textured || hazeparticles)
    {
        if(msaalight)
        {
            glBindFramebuffer_(GL_READ_FRAMEBUFFER, mshdrfbo);
            glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, hazerfbo);
            glBlitFramebuffer_(0, 0, hazew, hazeh, 0, 0, hazew, hazeh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer_(GL_FRAMEBUFFER, mshdrfbo);
        }
        else
        {
            glBindTexture(GL_TEXTURE_RECTANGLE, hazertex);
            glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, 0, 0, hazew, hazeh);
        }
    }

    if(hashaze)
    {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if(hasrefractmask)
        {
            glActiveTexture_(GL_TEXTURE7);
            if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msrefracttex);
            else glBindTexture(GL_TEXTURE_RECTANGLE, refracttex);
        }
        if(textured)
        {
            glActiveTexture_(GL_TEXTURE8);
            glBindTexture(GL_TEXTURE_RECTANGLE, hazertex);
        }
        glActiveTexture_(GL_TEXTURE9);
        if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
        else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
        glActiveTexture_(GL_TEXTURE0);

        const bvec &color = gethazecolour();
        float colormix = gethazecolourmix();
        if(textured && color.iszero()) colormix = 0;
        else hazemix = true;
        GLOBALPARAMF(hazecolor, color.x*ldrscaleb, color.y*ldrscaleb, color.z*ldrscaleb, colormix);
        float refract = gethazerefract(), refract2 = gethazerefract2(), refract3 = gethazerefract3();
        GLOBALPARAMF(hazerefract, refract, refract2, refract3);
        float margin = gethazemargin(), mindist = gethazemindist(), maxdist = max(max(mindist, gethazemaxdist())-mindist, margin), blend = gethazeblend();
        GLOBALPARAMF(hazeparams, mindist, 1.0f/maxdist, 1.0f/margin, blend);

        if(textured)
        {
            float xscale = gethazescalex(), yscale = gethazescaley(), scroll = lastmillis/1000.0f, xscroll = gethazescrollx()*scroll, yscroll = gethazescrolly()*scroll;
            GLOBALPARAMF(hazetexgen, xscale, yscale, xscroll, yscroll);
            glBindTexture(GL_TEXTURE_2D, hazetexture->id);
            if(hasrefractmask) SETSHADER(hazetexref);
            else SETSHADER(hazetex);
        }
        else if(hasrefractmask) SETSHADER(hazeref);
        else SETSHADER(haze);

        screenquad();

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    if(hazeparticles) renderhazeparticles(hazertex, hazemix);
}

void viewhaze()
{
    if(!hazertex) return;
    int w = min(hudw, hudh)/(debughaze == 2 ? 2 : 3), h = (w*hudh)/hudw;
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, hazertex);
    debugquad(0, 0, w, h, 0, 0, hazew, hazeh);
}
