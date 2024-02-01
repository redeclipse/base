#include "engine.h"

static GLuint halofbo[HALO_MAX] = { 0, 0 }, halotex[HALO_MAX] = { 0, 0 };
static int halow = -1, haloh = -1, halotype = -1;

VAR(0, debughalo, 0, 0, 2);
VAR(IDF_PERSIST, halos, 0, 1, 1);
FVAR(IDF_PERSIST, halowireframe, 0, 0, FVAR_MAX);
VAR(IDF_PERSIST, halodist, 32, 2048, VAR_MAX);
FVARF(IDF_PERSIST, haloscale, FVAR_NONZERO, 0.5f, 1, cleanuphalo());
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
    loopi(HALO_MAX)
    {
        if(!halotex[i])
        {
            glGenTextures(1, &halotex[i]);
            createtexture(halotex[i], halow, haloh, NULL, 3, 1, GL_RGBA, GL_TEXTURE_RECTANGLE);
        }

        if(!halofbo[i])
        {
            glGenFramebuffers_(1, &halofbo[i]);
            glBindFramebuffer_(GL_FRAMEBUFFER, halofbo[i]);
            glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, halotex[i], 0);
        }
        GLERROR;
    }

    useshaderbyname("hudhalodepth");
    useshaderbyname("hudhalotop");
    useshaderbyname("hudhalodepthref");
    useshaderbyname("hudhalotopref");

    halotype = -1;
}

bool wanthalos(bool check, bool val)
{
    if(check && drawtex != DRAWTEX_HALO) return true;
    return halos && val;
}

void cleanuphalo()
{
    loopi(HALO_MAX)
    {
        if(halofbo[i])
        {
            glDeleteFramebuffers_(1, &halofbo[i]);
            halofbo[i] = 0;
        }

        if(halotex[i])
        {
            glDeleteTextures(1, &halotex[i]);
            halotex[i] = 0;
        }
    }

    halotype = -1;
}

void swaphalo(int type)
{
    if(type < 0 || type >= HALO_MAX || halotype == type) return;

    glBindFramebuffer_(GL_FRAMEBUFFER, halofbo[type]);
    glViewport(0, 0, halow, haloh);
}

void renderhalo()
{
    setuphalo(vieww, viewh);

    loopirev(HALO_MAX)
    {
        swaphalo(i);

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

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
}

void viewhalo()
{
    int s = min(hudw, hudh)/(debughalo == 2 ? 2 : 3), x = 0, w = s, h = (w*hudh)/hudw;
    loopi(HALO_MAX)
    {
        if(halotex[i])
        {
            SETSHADER(hudrect);
            gle::colorf(1, 1, 1);
            glBindTexture(GL_TEXTURE_RECTANGLE, halotex[i]);
            debugquad(x, 0, w, h, 0, 0, halow, haloh);
        }
        else
        {
            SETSHADER(hud);
            gle::colorf(1, 1, 1);
            glBindTexture(GL_TEXTURE_2D, notexture->id);
            debugquad(x, 0, w, h, 0, 0, halow, haloh);
        }
        x += s;
    }
}

void blendhalos()
{
    if(!halos) return;

    //glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, hudw, hudh);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gle::color(halocolour.tocolor().mul(game::darkness(DARK_HALO)), haloblend);
    glEnable(GL_BLEND);

    float maxdist = hud::radarlimit(halodist), scanlines = haloscanlines >= 0 ? haloscanlines : visorscanlines * fabs(haloscanlines);
    loopirev(HALO_MAX)
    {
        switch(i)
        {
            case HALO_DEPTH:
                if(hasrefractmask) SETSHADER(hudhalodepthref);
                else SETSHADER(hudhalodepth);
                break;
            case HALO_ONTOP:
                if(hasrefractmask) SETSHADER(hudhalotopref);
                else SETSHADER(hudhalotop);
                break;
            default: continue;
        }
        glBindTexture(GL_TEXTURE_RECTANGLE, halotex[i]);

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
        LOCALPARAMF(halosize, hudw, hudh, 1.f/hudw, 1.f/hudh);
        LOCALPARAMF(haloparams, maxdist, 1 / maxdist);
        LOCALPARAMF(halofx, scanlines, haloscanlineblend, halonoiseblend, haloflickerblend);
        vec4 color = vec4::fromcolor(haloscanlinemixcolour, haloscanlinemixblend);
        LOCALPARAMF(halofxcol, color.r, color.g, color.b, color.a);

        hudquad(0, 0, hudw, hudh, 0, haloh, halow, -haloh);
    }

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
            glActiveTexture_(GL_TEXTURE0 + TEX_REFRACT_MASK);
            if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msrefracttex);
            else glBindTexture(GL_TEXTURE_RECTANGLE, refracttex);
        }

        if(textured)
        {
            glActiveTexture_(GL_TEXTURE0 + TEX_REFRACT_LIGHT);
            glBindTexture(GL_TEXTURE_RECTANGLE, hazertex);
        }

        glActiveTexture_(GL_TEXTURE0 + TEX_REFRACT_DEPTH);
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
            settexture(hazetexture);
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
