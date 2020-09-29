#include "engine.h"

static GLuint halofbo[2] = { 0, 0 }, halotex[2] = { 0, 0 };
static int halow = -1, haloh = -1;

VAR(0, debughalo, 0, 0, 1);
VAR(IDF_PERSIST, haloblur, 1, 2, 7);
FVAR(IDF_PERSIST, halosize, 0.25f, 1.0f, 2.0f);
VAR(IDF_PERSIST, halodist, 32, 1024, VAR_MAX);
FVAR(IDF_PERSIST, haloblend, 0, 1, 1);
CVAR(IDF_PERSIST, halocolour, 0xFFFFFF);

void setuphalo(int w, int h)
{
    if(w != halow || h != haloh)
    {
        cleanuphalo();
        halow = w;
        haloh = h;
    }

    GLERROR;
    if(!halotex[0])
    {
        glGenTextures(2, halotex);
        loopi(2) createtexture(halotex[i], halow, haloh, NULL, 3, 1, GL_RGBA8, GL_TEXTURE_RECTANGLE);
    }

    if(!halofbo[0])
    {
        glGenFramebuffers_(2, halofbo);
        loopi(2)
        {
            glBindFramebuffer_(GL_FRAMEBUFFER, halofbo[i]);
            glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, halotex[i], 0);
        }
    }
    else glBindFramebuffer_(GL_FRAMEBUFFER, halofbo[0]);

    glViewport(0, 0, halow, haloh);
}

void endhalo()
{
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
}

void cleanuphalo()
{
    if(halofbo[0])
    {
        glDeleteFramebuffers_(2, halofbo);
        loopi(2) halofbo[i] = 0;
    }

    if(halotex[0])
    {
        glDeleteTextures(2, halotex);
        loopi(2) halotex[i] = 0;
    }
}

void renderhalo()
{
    setuphalo(vieww*halosize, viewh*halosize);
    resetmodelbatches();

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_CULL_FACE);

    game::render();
    rendertransparentmodelbatches();
    rendermodelbatches();
    renderavatar();

    glDisable(GL_CULL_FACE);

    endhalo();
}

// debug view
void viewhalo()
{
    if(!halotex[0]) return;
    int w = min(hudw, hudh)/3, h = (w*hudh)/hudw;
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, halotex[0]);
    debugquad(0, 0, w, h, 0, 0, halow, haloh);
}

static void haloblurkernel(int radius, float *weights, float *offsets)
{
    loopi(MAXBLURRADIUS)
    {
        weights[i] = 1.0f / radius;
        offsets[i] = i ? (1 + ((i-1)*2)) : 0;
    }
    weights[MAXBLURRADIUS] = 0;
    offsets[MAXBLURRADIUS] = 0;
}

void drawhalo()
{
    float blurweights[MAXBLURRADIUS+1], bluroffsets[MAXBLURRADIUS+1];
    haloblurkernel(haloblur, blurweights, bluroffsets);

    glViewport(0, 0, halow, haloh);

    loopi(2)
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, i ? 0 : halofbo[1]);
        setblurshader(i, 1, haloblur, blurweights, bluroffsets, GL_TEXTURE_RECTANGLE,
            i ? halotex[0] : 0);
        glBindTexture(GL_TEXTURE_RECTANGLE, halotex[i]);

        if(i)
        {
            // blend with the existing frame
            glViewport(0, 0, hudw, hudh);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            gle::color(bvec4(halocolour, uchar(haloblend*255.0f)));
        }

        screenquad(halow, haloh);
    }

    glDisable(GL_BLEND);
}
