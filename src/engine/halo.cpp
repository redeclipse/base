#include "engine.h"

static GLuint halofbo = 0, halotex = 0;
static int halow = -1, haloh = -1;

VAR(0, debughalo, 0, 0, 2);
VAR(IDF_PERSIST, halodist, 32, 1024, VAR_MAX);
FVAR(IDF_PERSIST, haloscale, 0, 0.5f, 1);
FVAR(IDF_PERSIST, haloblend, 0, 1, 1);
CVAR(IDF_PERSIST, halocolour, 0xFFFFFF);
VAR(IDF_PERSIST, halooffset, 1, 2, 4);
FVAR(IDF_PERSIST, halofactor, FVAR_NONZERO, 4.f, FVAR_MAX);

void setuphalo(int w, int h)
{
    w = int(w*haloscale);
    h = int(h*haloscale);
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

    game::render();
    renderhalomodelbatches();
    renderavatar();

    glDisable(GL_CULL_FACE);

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
}

// debug view
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
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, hudw, hudh);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    gle::color(halocolour.tocolor(), haloblend);
    glEnable(GL_BLEND);

    glBindTexture(GL_TEXTURE_RECTANGLE, halotex);

    SETSHADER(hudhalo);
    LOCALPARAMI(offset, halooffset);
    LOCALPARAMF(factor, halofactor);

    hudquad(0, 0, hudw, hudh, 0, haloh, halow, -haloh);

    glDisable(GL_BLEND);
}
