#include "engine.h"

int gw = -1, gh = -1, bloomw = -1, bloomh = -1, lasthdraccum = 0;
GLuint gfbo = 0, gdepthtex = 0, gcolortex = 0, gnormaltex = 0, gglowtex = 0, gdepthrb = 0, gstencilrb = 0;
bool gdepthinit = false;
int scalew = -1, scaleh = -1;
GLuint scalefbo[2] = { 0, 0 }, scaletex[2] = { 0, 0 };
GLuint hdrfbo = 0, hdrtex = 0, bloompbo = 0, bloomfbo[6] = { 0, 0, 0, 0, 0, 0 }, bloomtex[6] = { 0, 0, 0, 0, 0, 0 };
int hdrclear = 0;
GLuint refractfbo = 0, refracttex = 0;
GLenum bloomformat = 0, hdrformat = 0, stencilformat = 0;
bool hdrfloat = false;
GLuint msfbo = 0, msdepthtex = 0, mscolortex = 0, msnormaltex = 0, msglowtex = 0, msdepthrb = 0, msstencilrb = 0, mshdrfbo = 0, mshdrtex = 0, msrefractfbo = 0, msrefracttex = 0;
vector<vec2> msaapositions;
int aow = -1, aoh = -1;
GLuint aofbo[4] = { 0, 0, 0, 0 }, aotex[4] = { 0, 0, 0, 0 }, aonoisetex = 0;
matrix4 eyematrix, worldmatrix, linearworldmatrix, screenmatrix;

extern int amd_pf_bug;

int gethdrformat(int prec, int fallback = GL_RGB)
{
    if(prec >= 3 && hasTF) return GL_RGB16F;
    if(prec >= 2 && hasPF && !amd_pf_bug) return GL_R11F_G11F_B10F;
    if(prec >= 1) return GL_RGB10;
    return fallback;
}

extern int bloomsize, bloomprec;

void setupbloom(int w, int h)
{
    int maxsize = ((1<<bloomsize)*5)/4;
    while(w >= maxsize || h >= maxsize)
    {
        w /= 2;
        h /= 2;
    }
    w = max(w, 1);
    h = max(h, 1);
    if(w == bloomw && h == bloomh) return;
    bloomw = w;
    bloomh = h;

    loopi(5) if(!bloomtex[i]) glGenTextures(1, &bloomtex[i]);

    loopi(5) if(!bloomfbo[i]) glGenFramebuffers_(1, &bloomfbo[i]);

    bloomformat = gethdrformat(bloomprec);
    createtexture(bloomtex[0], max(gw/2, bloomw), max(gh/2, bloomh), NULL, 3, 1, bloomformat, GL_TEXTURE_RECTANGLE);
    createtexture(bloomtex[1], max(gw/4, bloomw), max(gh/4, bloomh), NULL, 3, 1, bloomformat, GL_TEXTURE_RECTANGLE);
    createtexture(bloomtex[2], bloomw, bloomh, NULL, 3, 1, GL_RGB, GL_TEXTURE_RECTANGLE);
    createtexture(bloomtex[3], bloomw, bloomh, NULL, 3, 1, GL_RGB, GL_TEXTURE_RECTANGLE);
    if(bloomformat != GL_RGB)
    {
        if(!bloomtex[5]) glGenTextures(1, &bloomtex[5]);
        if(!bloomfbo[5]) glGenFramebuffers_(1, &bloomfbo[5]);
        createtexture(bloomtex[5], bloomw, bloomh, NULL, 3, 1, bloomformat, GL_TEXTURE_RECTANGLE);
    }

    if(hwvtexunits < 4)
    {
        glGenBuffers_(1, &bloompbo);
        glBindBuffer_(GL_PIXEL_PACK_BUFFER, bloompbo);
        glBufferData_(GL_PIXEL_PACK_BUFFER, 4*(hasTF ? sizeof(GLfloat) : sizeof(GLushort))*(hasTRG ? 1 : 3), NULL, GL_DYNAMIC_COPY);
        glBindBuffer_(GL_PIXEL_PACK_BUFFER, 0);
    }

    static const uchar gray[12] = { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 };
    static const float grayf[12] = { 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f };
    createtexture(bloomtex[4], bloompbo ? 4 : 1, 1, hasTF ? (const void *)grayf : (const void *)gray, 3, 1, hasTF ? (hasTRG ? GL_R16F : GL_RGB16F) : (hasTRG ? GL_R16 : GL_RGB16));

    loopi(5 + (bloomformat != GL_RGB ? 1 : 0))
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, bloomfbo[i]);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, i==4 ? GL_TEXTURE_2D : GL_TEXTURE_RECTANGLE, bloomtex[i], 0);

        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fatal("Failed allocating bloom buffer!");
    }

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
}

void cleanupbloom()
{
    if(bloompbo) { glDeleteBuffers_(1, &bloompbo); bloompbo = 0; }
    loopi(6) if(bloomfbo[i]) { glDeleteFramebuffers_(1, &bloomfbo[i]); bloomfbo[i] = 0; }
    loopi(6) if(bloomtex[i]) { glDeleteTextures(1, &bloomtex[i]); bloomtex[i] = 0; }
    bloomw = bloomh = -1;
    lasthdraccum = 0;
}

extern int ao, aotaps, aoreduce, aoreducedepth, aonoise, aobilateral, aobilateralupscale, aopackdepth, aodepthformat, aoprec, aoderivnormal;

static Shader *bilateralshader[2] = { NULL, NULL };

Shader *loadbilateralshader(int pass)
{
    if(!aobilateral) return nullshader;

    string opts;
    int optslen = 0;

    bool linear = aoreducedepth && (aoreduce || aoreducedepth > 1),
         upscale = aoreduce && aobilateralupscale,
         reduce = aoreduce && (upscale || (!linear && !aopackdepth));
    if(reduce)
    {
        opts[optslen++] = 'r';
        opts[optslen++] = '0' + aoreduce;
    }
    if(upscale) opts[optslen++] = 'u';
    else if(linear) opts[optslen++] = 'l';
    if(aopackdepth) opts[optslen++] = 'p';
    opts[optslen] = '\0';

    defformatstring(name, "bilateral%c%s%d", 'x' + pass, opts, aobilateral);
    return generateshader(name, "bilateralshader \"%s\" %d %d", opts, aobilateral, reduce ? aoreduce : 0);
}

void loadbilateralshaders()
{
    loopk(2) bilateralshader[k] = loadbilateralshader(k);
}

void clearbilateralshaders()
{
    loopk(2) bilateralshader[k] = NULL;
}

void setbilateralparams(int radius, float depth)
{
    float sigma = blursigma*2*radius;
    LOCALPARAMF(bilateralparams, 1.0f/(M_LN2*2*sigma*sigma), 1.0f/(M_LN2*depth*depth));
}

void setbilateralshader(int radius, int pass, float depth)
{
    bilateralshader[pass]->set();
    setbilateralparams(radius, depth);
}

static Shader *ambientobscuranceshader = NULL;

Shader *loadambientobscuranceshader()
{
    string opts;
    int optslen = 0;

    bool linear = aoreducedepth && (aoreduce || aoreducedepth > 1);
    if(linear) opts[optslen++] = 'l';
    if(aoderivnormal) opts[optslen++] = 'd';
    if(aobilateral && aopackdepth) opts[optslen++] = 'p';
    opts[optslen] = '\0';

    defformatstring(name, "ambientobscurance%s%d", opts, aotaps);
    return generateshader(name, "ambientobscuranceshader \"%s\" %d", opts, aotaps);
}

void loadaoshaders()
{
    ambientobscuranceshader = loadambientobscuranceshader();
}

void clearaoshaders()
{
    ambientobscuranceshader = NULL;
}

void setupao(int w, int h)
{
    int sw = w>>aoreduce, sh = h>>aoreduce;

    if(sw == aow && sh == aoh) return;

    aow = sw;
    aoh = sh;

    if(!aonoisetex) glGenTextures(1, &aonoisetex);
    bvec *noise = new bvec[(1<<aonoise)*(1<<aonoise)];
    loopk((1<<aonoise)*(1<<aonoise)) noise[k] = bvec(vec(rndscale(2)-1, rndscale(2)-1, 0).normalize());
    createtexture(aonoisetex, 1<<aonoise, 1<<aonoise, noise, 0, 0, GL_RGB, GL_TEXTURE_2D);
    delete[] noise;

    bool upscale = aoreduce && aobilateral && aobilateralupscale;
    GLenum format = aoprec && hasTRG ? GL_R8 : GL_RGBA8,
           packformat = aobilateral && aopackdepth ? (aodepthformat ? GL_RG16F : GL_RGBA8) : format;
    int packfilter = upscale && aopackdepth && !aodepthformat ? 0 : 1;
    loopi(upscale ? 3 : 2)
    {
        if(!aotex[i]) glGenTextures(1, &aotex[i]);
        if(!aofbo[i]) glGenFramebuffers_(1, &aofbo[i]);
        createtexture(aotex[i], upscale && i ? w : aow, upscale && i >= 2 ? h : aoh, NULL, 3, i < 2 ? packfilter : 1, i < 2 ? packformat : format, GL_TEXTURE_RECTANGLE);
        glBindFramebuffer_(GL_FRAMEBUFFER, aofbo[i]);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, aotex[i], 0);
        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fatal("Failed allocating AO buffer!");
        if(!upscale && packformat == GL_RG16F)
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }

    if(aoreducedepth && (aoreduce || aoreducedepth > 1))
    {
        if(!aotex[3]) glGenTextures(1, &aotex[3]);
        if(!aofbo[3]) glGenFramebuffers_(1, &aofbo[3]);
        createtexture(aotex[3], aow, aoh, NULL, 3, 0, aodepthformat > 1 ? GL_R32F : (aodepthformat ? GL_R16F : GL_RGBA8), GL_TEXTURE_RECTANGLE);
        glBindFramebuffer_(GL_FRAMEBUFFER, aofbo[3]);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, aotex[3], 0);
        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fatal("Failed allocating AO buffer!");
    }

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);

    loadaoshaders();
    loadbilateralshaders();
}

void cleanupao()
{
    loopi(4) if(aofbo[i]) { glDeleteFramebuffers_(1, &aofbo[i]); aofbo[i] = 0; }
    loopi(4) if(aotex[i]) { glDeleteTextures(1, &aotex[i]); aotex[i] = 0; }
    if(aonoisetex) { glDeleteTextures(1, &aonoisetex); aonoisetex = 0; }
    aow = aoh = -1;

    clearaoshaders();
    clearbilateralshaders();
}

#define GETVARMPV(name, var, type) \
    type get##name##var() \
    { \
        if(checkmapvariant(MPV_ALT)) return name##var##alt; \
        return name##var; \
    }

#define AOVARS(name) \
    FVAR(IDF_WORLD, aoradius##name, 0, 5, 256); \
    FVAR(IDF_WORLD, aodark##name, 1e-3f, 11.0f, 1e3f); \
    FVAR(IDF_WORLD, aomin##name, 0, 0.25f, 1); \
    VARF(IDF_WORLD, aosun##name, 0, 1, 1, cleardeferredlightshaders()); \
    FVAR(IDF_WORLD, aosunmin##name, 0, 0.5f, 1); \
    FVAR(IDF_WORLD, aosharp##name, 1e-3f, 1, 1e3f);

AOVARS();
AOVARS(alt);

GETVARMPV(ao, radius, float);
GETVARMPV(ao, dark, float);
GETVARMPV(ao, min, float);
GETVARMPV(ao, sun, float);
GETVARMPV(ao, sunmin, float);
GETVARMPV(ao, sharp, float);

VARF(IDF_PERSIST, ao, 0, 1, 1, { cleanupao(); cleardeferredlightshaders(); });
FVAR(0, aocutoff, 0, 2.0f, 1e3f);
FVAR(0, aoprefilterdepth, 0, 1, 1e3f);
VAR(IDF_PERSIST, aoblur, 0, 4, 7);
VAR(IDF_PERSIST, aoiter, 0, 0, 4);
VARF(IDF_PERSIST, aoreduce, 0, 1, 2, cleanupao());
VARF(0, aoreducedepth, 0, 1, 2, cleanupao());
VARF(IDF_PERSIST, aofloatdepth, 0, 1, 2, initwarning("AO setup", INIT_LOAD, CHANGE_SHADERS));
VARF(IDF_PERSIST, aoprec, 0, 1, 1, cleanupao());
VAR(0, aodepthformat, 1, 0, 0);
VARF(0, aonoise, 0, 5, 8, cleanupao());
VARF(IDF_PERSIST, aobilateral, 0, 3, 10, cleanupao());
FVAR(IDF_PERSIST, aobilateraldepth, 0, 4, 1e3f);
VARF(IDF_PERSIST, aobilateralupscale, 0, 0, 1, cleanupao());
VARF(0, aopackdepth, 0, 1, 1, cleanupao());
VARF(IDF_PERSIST, aotaps, 1, 5, 12, cleanupao());
VARF(0, aoderivnormal, 0, 0, 1, cleanupao());
VAR(0, debugao, 0, 0, 1);

void initao()
{
    aodepthformat = aofloatdepth && hasTRG && hasTF ? aofloatdepth : 0;
}

void viewao()
{
    if(!ao) return;
    int w = min(hudw, hudh)/2, h = (w*hudh)/hudw;
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, aotex[2] ? aotex[2] : aotex[0]);
    int tw = aotex[2] ? gw : aow, th = aotex[2] ? gh : aoh;
    debugquad(0, 0, w, h, 0, 0, tw, th);
}

void renderao()
{
    if(!ao) return;

    timer *aotimer = begintimer("Ambient Obscurance");

    if(msaasamples) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);

    bool linear = aoreducedepth && (aoreduce || aoreducedepth > 1);
    float xscale = eyematrix.a.x, yscale = eyematrix.b.y;
    if(linear)
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, aofbo[3]);
        glViewport(0, 0, aow, aoh);
        SETSHADER(linearizedepth);
        screenquad(vieww, viewh);

        xscale *= float(vieww)/aow;
        yscale *= float(viewh)/aoh;

        glBindTexture(GL_TEXTURE_RECTANGLE, aotex[3]);
    }

    ambientobscuranceshader->set();

    glBindFramebuffer_(GL_FRAMEBUFFER, aofbo[0]);
    glViewport(0, 0, aow, aoh);
    glActiveTexture_(GL_TEXTURE1);
    if(aoderivnormal)
    {
        if(msaasamples) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
        else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    }
    else
    {
        if(msaasamples) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msnormaltex);
        else glBindTexture(GL_TEXTURE_RECTANGLE, gnormaltex);
        LOCALPARAM(normalmatrix, matrix3(cammatrix));
    }
    glActiveTexture_(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, aonoisetex);
    glActiveTexture_(GL_TEXTURE0);

    LOCALPARAMF(tapparams, getaoradius()*eyematrix.d.z/xscale, getaoradius()*eyematrix.d.z/yscale, getaoradius()*getaoradius()*aocutoff*aocutoff);
    LOCALPARAMF(contrastparams, (2.0f*getaodark())/aotaps, getaosharp());
    LOCALPARAMF(offsetscale, xscale/eyematrix.d.z, yscale/eyematrix.d.z, eyematrix.d.x/eyematrix.d.z, eyematrix.d.y/eyematrix.d.z);
    LOCALPARAMF(prefilterdepth, aoprefilterdepth);
    screenquad(vieww, viewh, aow/float(1<<aonoise), aoh/float(1<<aonoise));

    if(aobilateral)
    {
        if(aoreduce && aobilateralupscale) loopi(2)
        {
            setbilateralshader(aobilateral, i, aobilateraldepth);
            glBindFramebuffer_(GL_FRAMEBUFFER, aofbo[i+1]);
            glViewport(0, 0, vieww, i ? viewh : aoh);
            glBindTexture(GL_TEXTURE_RECTANGLE, aotex[i]);
            glActiveTexture_(GL_TEXTURE1);
            if(msaasamples) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
            else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
            glActiveTexture_(GL_TEXTURE0);
            screenquad(vieww, viewh, i ? vieww : aow, aoh);
        }
        else loopi(2 + 2*aoiter)
        {
            setbilateralshader(aobilateral, i%2, aobilateraldepth);
            glBindFramebuffer_(GL_FRAMEBUFFER, aofbo[(i+1)%2]);
            glViewport(0, 0, aow, aoh);
            glBindTexture(GL_TEXTURE_RECTANGLE, aotex[i%2]);
            glActiveTexture_(GL_TEXTURE1);
            if(linear) glBindTexture(GL_TEXTURE_RECTANGLE, aotex[3]);
            else if(msaasamples) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
            else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
            glActiveTexture_(GL_TEXTURE0);
            screenquad(vieww, viewh);
        }
    }
    else if(aoblur)
    {
        float blurweights[MAXBLURRADIUS+1], bluroffsets[MAXBLURRADIUS+1];
        setupblurkernel(aoblur, blurweights, bluroffsets);
        loopi(2 + 2*aoiter)
        {
            glBindFramebuffer_(GL_FRAMEBUFFER, aofbo[(i+1)%2]);
            glViewport(0, 0, aow, aoh);
            setblurshader(i%2, 1, aoblur, blurweights, bluroffsets, GL_TEXTURE_RECTANGLE);
            glBindTexture(GL_TEXTURE_RECTANGLE, aotex[i%2]);
            screenquad(aow, aoh);
        }
    }

    glBindFramebuffer_(GL_FRAMEBUFFER, msaasamples ? msfbo : gfbo);
    glViewport(0, 0, vieww, viewh);

    endtimer(aotimer);
}

void cleanupscale()
{
    loopi(2) if(scalefbo[i]) { glDeleteFramebuffers_(1, &scalefbo[i]); scalefbo[i] = 0; }
    loopi(2) if(scaletex[i]) { glDeleteTextures(1, &scaletex[i]); scaletex[i] = 0; }
    scalew = scaleh = -1;
}

extern int gscale, gscalecubic, gscalenearest;

void setupscale(int sw, int sh, int w, int h)
{
    scalew = w;
    scaleh = h;

    loopi(gscalecubic ? 2 : 1)
    {
        if(!scaletex[i]) glGenTextures(1, &scaletex[i]);
        if(!scalefbo[i]) glGenFramebuffers_(1, &scalefbo[i]);

        glBindFramebuffer_(GL_FRAMEBUFFER, scalefbo[i]);

        // When `gscalenearest` is -1 (the default), filtering is only enabled for non-integer scale factors.
        // This makes visuals crisper when using `gscale 25` or `gscale 50`.
        // See <http://tanalin.com/en/articles/integer-scaling/> for rationale.
        const bool filter = gscalecubic || gscalenearest == 0 || (gscalenearest == -1 && gscale != 25 && gscale != 50);
        createtexture(scaletex[i], sw, i ? h : sh, NULL, 3, filter, GL_RGB, GL_TEXTURE_RECTANGLE);

        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, scaletex[i], 0);
        if(!i) bindgdepth();

        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fatal("Failed allocating scale buffer!");
    }

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);

    if(gscalecubic)
    {
        useshaderbyname("scalecubicx");
        useshaderbyname("scalecubicy");
    }
}

GLuint shouldscale()
{
    return scalefbo[0];
}

void doscale(GLuint outfbo)
{
    if(!scaletex[0]) return;

    timer *scaletimer = begintimer("Scaling");

    if(gscalecubic)
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, scalefbo[1]);
        glViewport(0, 0, gw, hudh);
        glBindTexture(GL_TEXTURE_RECTANGLE, scaletex[0]);
        SETSHADER(scalecubicy);
        screenquad(gw, gh);
        glBindFramebuffer_(GL_FRAMEBUFFER, outfbo);
        glViewport(0, 0, hudw, hudh);
        glBindTexture(GL_TEXTURE_RECTANGLE, scaletex[1]);
        SETSHADER(scalecubicx);
        screenquad(gw, hudh);
    }
    else
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, outfbo);
        glViewport(0, 0, hudw, hudh);
        glBindTexture(GL_TEXTURE_RECTANGLE, scaletex[0]);
        SETSHADER(scalelinear);
        screenquad(gw, gh);
    }

    endtimer(scaletimer);
}

VARF(IDF_PERSIST, glineardepth, 0, 0, 3, initwarning("g-buffer setup", INIT_LOAD, CHANGE_SHADERS));
VAR(0, gdepthformat, 1, 0, 0);
VARF(0, gstencil, 0, 0, 1, initwarning("g-buffer setup", INIT_LOAD, CHANGE_SHADERS));
VARF(0, gdepthstencil, 0, 2, 2, initwarning("g-buffer setup", INIT_LOAD, CHANGE_SHADERS));
VAR(0, ghasstencil, 1, 0, 0);
VARF(IDF_PERSIST, msaa, 0, 0, 16, initwarning("MSAA setup", INIT_LOAD, CHANGE_SHADERS));
VARF(0, msaadepthstencil, 0, 2, 2, initwarning("MSAA setup", INIT_LOAD, CHANGE_SHADERS));
VARF(0, msaastencil, 0, 0, 1, initwarning("MSAA setup", INIT_LOAD, CHANGE_SHADERS));
VARF(0, msaaedgedetect, 0, 1, 1, cleanupgbuffer());
VARF(IDF_PERSIST, msaalineardepth, -1, -1, 3, initwarning("MSAA setup", INIT_LOAD, CHANGE_SHADERS));
VARF(IDF_PERSIST, msaatonemap, 0, 0, 1, cleanupgbuffer());
VARF(0, msaatonemapblit, 0, 0, 1, cleanupgbuffer());
VAR(0, msaamaxsamples, 1, 0, 0);
VAR(0, msaamaxdepthtexsamples, 1, 0, 0);
VAR(0, msaamaxcolortexsamples, 1, 0, 0);
VAR(0, msaaminsamples, 1, 0, 0);
VAR(0, msaasamples, 1, 0, 0);
VAR(0, msaalight, 1, 0, 0);
VARF(0, msaapreserve, -1, 0, 1, initwarning("MSAA setup", INIT_LOAD, CHANGE_SHADERS));

void checkmsaasamples()
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex);

    GLint samples;
    glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaaminsamples, GL_RGBA8, 1, 1, GL_TRUE);
    glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_SAMPLES, &samples);
    msaasamples = samples;

    glDeleteTextures(1, &tex);
}

void initgbuffer()
{
    msaamaxsamples = msaamaxdepthtexsamples = msaamaxcolortexsamples = msaaminsamples = msaasamples = msaalight = 0;
    msaapositions.setsize(0);

    if(hasFBMS && hasFBB && hasTMS)
    {
        GLint val;
        glGetIntegerv(GL_MAX_SAMPLES, &val);
        msaamaxsamples = val;
        glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &val);
        msaamaxdepthtexsamples = val;
        glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &val);
        msaamaxcolortexsamples = val;
    }

    int maxsamples = min(msaamaxsamples, msaamaxcolortexsamples), reqsamples = min(msaa, maxsamples);
    if(reqsamples >= 2)
    {
        msaaminsamples = 2;
        while(msaaminsamples*2 <= reqsamples) msaaminsamples *= 2;
    }

    int lineardepth = glineardepth;
    if(msaaminsamples)
    {
        if(msaamaxdepthtexsamples < msaaminsamples)
        {
            if(msaalineardepth > 0) lineardepth = msaalineardepth;
            else if(!lineardepth) lineardepth = 1;
        }
        else if(msaalineardepth >= 0) lineardepth = msaalineardepth;
    }

    if(lineardepth > 1 && (!hasAFBO || !hasTF || !hasTRG)) gdepthformat = 1;
    else gdepthformat = lineardepth;

    if(msaaminsamples)
    {
        ghasstencil = (msaadepthstencil > 1 || (msaadepthstencil && gdepthformat)) && hasDS ? 2 : (msaastencil ? 1 : 0);

        checkmsaasamples();

        if(msaapreserve >= 0) msaalight = hasMSS ? 3 : (msaasamples==2 ? 2 : msaapreserve);
    }
    else ghasstencil = (gdepthstencil > 1 || (gdepthstencil && gdepthformat)) && hasDS ? 2 : (gstencil ? 1 : 0);

    initao();
}

VARF(0, forcepacknorm, 0, 0, 1, initwarning("g-buffer setup", INIT_LOAD, CHANGE_SHADERS));

bool usepacknorm() { return forcepacknorm || msaasamples || !useavatarmask(); }
ICOMMAND(0, usepacknorm, "", (), intret(usepacknorm() ? 1 : 0));

void maskgbuffer(const char *mask)
{
    GLenum drawbufs[4];
    int numbufs = 0;
    while(*mask) switch(*mask++)
    {
        case 'c': drawbufs[numbufs++] = GL_COLOR_ATTACHMENT0; break;
        case 'n': drawbufs[numbufs++] = GL_COLOR_ATTACHMENT1; break;
        case 'd': if(gdepthformat) drawbufs[numbufs++] = GL_COLOR_ATTACHMENT3; break;
        case 'g': drawbufs[numbufs++] = GL_COLOR_ATTACHMENT2; break;
    }
    glDrawBuffers_(numbufs, drawbufs);
}

extern int hdrprec, gscale;

void cleanupmsbuffer()
{
    if(msfbo) { glDeleteFramebuffers_(1, &msfbo); msfbo = 0; }
    if(msdepthtex) { glDeleteTextures(1, &msdepthtex); msdepthtex = 0; }
    if(mscolortex) { glDeleteTextures(1, &mscolortex); mscolortex = 0; }
    if(msnormaltex) { glDeleteTextures(1, &msnormaltex); msnormaltex = 0; }
    if(msglowtex) { glDeleteTextures(1, &msglowtex); msglowtex = 0; }
    if(msstencilrb) { glDeleteRenderbuffers_(1, &msstencilrb); msstencilrb = 0; }
    if(msdepthrb) { glDeleteRenderbuffers_(1, &msdepthrb); msdepthrb = 0; }
    if(mshdrfbo) { glDeleteFramebuffers_(1, &mshdrfbo); mshdrfbo = 0; }
    if(mshdrtex) { glDeleteTextures(1, &mshdrtex); mshdrtex = 0; }
    if(msrefractfbo) { glDeleteFramebuffers_(1, &msrefractfbo); msrefractfbo = 0; }
    if(msrefracttex) { glDeleteTextures(1, &msrefracttex); msrefracttex = 0; }
}

void bindmsdepth()
{
    if(gdepthformat)
    {
        glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msdepthrb);
        if(ghasstencil > 1) glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, msdepthrb);
        else if(msaalight && ghasstencil) glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, msstencilrb);
    }
    else
    {
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, msdepthtex, 0);
        if(ghasstencil > 1) glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, msdepthtex, 0);
        else if(msaalight && ghasstencil) glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, msstencilrb);
    }
}

void setupmsbuffer(int w, int h)
{
    if(!msfbo) glGenFramebuffers_(1, &msfbo);

    glBindFramebuffer_(GL_FRAMEBUFFER, msfbo);

    stencilformat = ghasstencil > 1 ? GL_DEPTH24_STENCIL8 : (ghasstencil ? GL_STENCIL_INDEX8 : 0);

    if(gdepthformat)
    {
        if(!msdepthrb) glGenRenderbuffers_(1, &msdepthrb);
        glBindRenderbuffer_(GL_RENDERBUFFER, msdepthrb);
        glRenderbufferStorageMultisample_(GL_RENDERBUFFER, msaasamples, ghasstencil > 1 ? stencilformat : GL_DEPTH_COMPONENT24, w, h);
        glBindRenderbuffer_(GL_RENDERBUFFER, 0);
    }
    if(msaalight && ghasstencil == 1)
    {
        if(!msstencilrb) glGenRenderbuffers_(1, &msstencilrb);
        glBindRenderbuffer_(GL_RENDERBUFFER, msstencilrb);
        glRenderbufferStorageMultisample_(GL_RENDERBUFFER, msaasamples, GL_STENCIL_INDEX8, w, h);
        glBindRenderbuffer_(GL_RENDERBUFFER, 0);
    }

    if(!msdepthtex) glGenTextures(1, &msdepthtex);
    if(!mscolortex) glGenTextures(1, &mscolortex);
    if(!msnormaltex) glGenTextures(1, &msnormaltex);

    maskgbuffer(msaalight ? "cndg" : "cnd");

    static const GLenum depthformats[] = { GL_RGBA8, GL_R16F, GL_R32F };
    GLenum depthformat = gdepthformat ? depthformats[gdepthformat-1] : (ghasstencil > 1 ? stencilformat : GL_DEPTH_COMPONENT24);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, depthformat, w, h, GL_TRUE);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mscolortex);
    glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, GL_RGBA8, w, h, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msnormaltex);
    glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, GL_RGBA8, w, h, GL_TRUE);
    if(msaalight)
    {
        if(!msglowtex) glGenTextures(1, &msglowtex);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msglowtex);
        glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, hasAFBO ? hdrformat : GL_RGBA8, w, h, GL_TRUE);
    }

    bindmsdepth();
    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, mscolortex, 0);
    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D_MULTISAMPLE, msnormaltex, 0);
    if(msaalight) glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D_MULTISAMPLE, msglowtex, 0);
    if(gdepthformat) glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D_MULTISAMPLE, msdepthtex, 0);

    if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        if(msaalight && hasAFBO)
        {
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msglowtex);
            glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, GL_RGBA8, w, h, GL_TRUE);
            glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D_MULTISAMPLE, msglowtex, 0);
            if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                fatal("Failed allocating MSAA g-buffer!");
        }
        else fatal("Failed allocating MSAA g-buffer!");
    }

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (ghasstencil ? GL_STENCIL_BUFFER_BIT : 0));

    msaapositions.setsize(0);
    loopi(msaasamples)
    {
        GLfloat vals[2];
        glGetMultisamplefv_(GL_SAMPLE_POSITION, i, vals);
        msaapositions.add(vec2(vals[0], vals[1]));
    }

    if(msaalight)
    {
        if(!mshdrtex) glGenTextures(1, &mshdrtex);
        if(!mshdrfbo) glGenFramebuffers_(1, &mshdrfbo);

        glBindFramebuffer_(GL_FRAMEBUFFER, mshdrfbo);

        bindmsdepth();

        hdrformat = 0;
        for(int prec = hdrprec; prec >= 0; prec--)
        {
            GLenum format = gethdrformat(prec);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mshdrtex);
            glGetError();
            glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, format, w, h, GL_TRUE);
            if(glGetError() == GL_NO_ERROR)
            {
                glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, mshdrtex, 0);
                if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
                {
                    hdrformat = format;
                    break;
                }
            }
        }

        if(!hdrformat || glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fatal("Failed allocating MSAA HDR buffer!");

        if(!msrefracttex) glGenTextures(1, &msrefracttex);
        if(!msrefractfbo) glGenFramebuffers_(1, &msrefractfbo);

        glBindFramebuffer_(GL_FRAMEBUFFER, msrefractfbo);

        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msrefracttex);
        glTexImage2DMultisample_(GL_TEXTURE_2D_MULTISAMPLE, msaasamples, GL_RGB, w, h, GL_TRUE);

        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, msrefracttex, 0);
        bindmsdepth();

        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fatal("Failed allocating MSAA refraction buffer!");
    }

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);

    useshaderbyname("msaaedgedetect");
    useshaderbyname("msaaresolve");
    useshaderbyname("msaareducew");
    useshaderbyname("msaareduce");
    if(!msaalight) useshaderbyname("msaaresolvedepth");
    if(msaalight > 1 && msaatonemap)
    {
        useshaderbyname("msaatonemap");
        if(msaalight > 2) useshaderbyname("msaatonemapsample");
    }
}

void bindgdepth()
{
    if(gdepthformat || msaalight)
    {
        glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gdepthrb);
        if(ghasstencil > 1) glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gdepthrb);
        else if(!msaalight || ghasstencil) glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gstencilrb);
    }
    else
    {
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_RECTANGLE, gdepthtex, 0);
        if(ghasstencil > 1) glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_RECTANGLE, gdepthtex, 0);
        else if(ghasstencil) glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gstencilrb);
    }
}

void setupgbuffer()
{
    int sw = renderw, sh = renderh;
    if(gscale != 100)
    {
        sw = max((renderw*gscale + 99)/100, 1);
        sh = max((renderh*gscale + 99)/100, 1);
    }

    if(gw == sw && gh == sh && ((sw >= hudw && sh >= hudh && !scalefbo[0]) || (scalew == hudw && scaleh == hudh))) return;

    cleanupscale();
    cleanupbloom();
    cleanupao();
    cleanupvolumetric();
    cleanupaa();
    cleanuppostfx();

    gw = sw;
    gh = sh;

    hdrformat = gethdrformat(hdrprec);
    stencilformat = ghasstencil > 1 ? GL_DEPTH24_STENCIL8 : (ghasstencil ? GL_STENCIL_INDEX8 : 0);

    if(msaasamples) setupmsbuffer(gw, gh);

    hdrfloat = floatformat(hdrformat);
    hdrclear = 3;
    gdepthinit = false;

    if(gdepthformat || msaalight)
    {
        if(!gdepthrb) glGenRenderbuffers_(1, &gdepthrb);
        glBindRenderbuffer_(GL_RENDERBUFFER, gdepthrb);
        glRenderbufferStorage_(GL_RENDERBUFFER, ghasstencil > 1 ? stencilformat : GL_DEPTH_COMPONENT24, gw, gh);
        glBindRenderbuffer_(GL_RENDERBUFFER, 0);
    }
    if(!msaalight && ghasstencil == 1)
    {
        if(!gstencilrb) glGenRenderbuffers_(1, &gstencilrb);
        glBindRenderbuffer_(GL_RENDERBUFFER, gstencilrb);
        glRenderbufferStorage_(GL_RENDERBUFFER, GL_STENCIL_INDEX8, gw, gh);
        glBindRenderbuffer_(GL_RENDERBUFFER, 0);
    }

    if(!msaalight)
    {
        if(!gdepthtex) glGenTextures(1, &gdepthtex);
        if(!gcolortex) glGenTextures(1, &gcolortex);
        if(!gnormaltex) glGenTextures(1, &gnormaltex);
        if(!gglowtex) glGenTextures(1, &gglowtex);
        if(!gfbo) glGenFramebuffers_(1, &gfbo);

        glBindFramebuffer_(GL_FRAMEBUFFER, gfbo);

        maskgbuffer("cndg");

        static const GLenum depthformats[] = { GL_RGBA8, GL_R16F, GL_R32F };
        GLenum depthformat = gdepthformat ? depthformats[gdepthformat-1] : (ghasstencil > 1 ? stencilformat : GL_DEPTH_COMPONENT24);
        createtexture(gdepthtex, gw, gh, NULL, 3, 0, depthformat, GL_TEXTURE_RECTANGLE);

        createtexture(gcolortex, gw, gh, NULL, 3, 0, GL_RGBA8, GL_TEXTURE_RECTANGLE);
        createtexture(gnormaltex, gw, gh, NULL, 3, 0, GL_RGBA8, GL_TEXTURE_RECTANGLE);
        createtexture(gglowtex, gw, gh, NULL, 3, 0, hasAFBO ? hdrformat : GL_RGBA8, GL_TEXTURE_RECTANGLE);

        bindgdepth();
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, gcolortex, 0);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_RECTANGLE, gnormaltex, 0);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_RECTANGLE, gglowtex, 0);
        if(gdepthformat) glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_RECTANGLE, gdepthtex, 0);

        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            if(hasAFBO)
            {
                createtexture(gglowtex, gw, gh, NULL, 3, 0, GL_RGBA8, GL_TEXTURE_RECTANGLE);
                glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_RECTANGLE, gglowtex, 0);
                if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                    fatal("Failed allocating g-buffer!");
            }
            else fatal("Failed allocating g-buffer!");
        }

        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (ghasstencil ? GL_STENCIL_BUFFER_BIT : 0));
    }

    if(!hdrtex) glGenTextures(1, &hdrtex);
    if(!hdrfbo) glGenFramebuffers_(1, &hdrfbo);

    glBindFramebuffer_(GL_FRAMEBUFFER, hdrfbo);

    createtexture(hdrtex, gw, gh, NULL, 3, 1, hdrformat, GL_TEXTURE_RECTANGLE);

    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, hdrtex, 0);
    bindgdepth();

    if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fatal("Failed allocating HDR buffer!");

    if(!msaalight || (msaalight > 2 && msaatonemap && msaatonemapblit))
    {
        if(!refracttex) glGenTextures(1, &refracttex);
        if(!refractfbo) glGenFramebuffers_(1, &refractfbo);

        glBindFramebuffer_(GL_FRAMEBUFFER, refractfbo);

        createtexture(refracttex, gw, gh, NULL, 3, 0, GL_RGB, GL_TEXTURE_RECTANGLE);

        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, refracttex, 0);
        bindgdepth();

        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fatal("Failed allocating refraction buffer!");
    }

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);

    if(gw < hudw || gh < hudh) setupscale(gw, gh, hudw, hudh);
}

void cleanupgbuffer()
{
    if(gfbo) { glDeleteFramebuffers_(1, &gfbo); gfbo = 0; }
    if(gdepthtex) { glDeleteTextures(1, &gdepthtex); gdepthtex = 0; }
    if(gcolortex) { glDeleteTextures(1, &gcolortex); gcolortex = 0; }
    if(gnormaltex) { glDeleteTextures(1, &gnormaltex); gnormaltex = 0; }
    if(gglowtex) { glDeleteTextures(1, &gglowtex); gglowtex = 0; }
    if(gstencilrb) { glDeleteRenderbuffers_(1, &gstencilrb); gstencilrb = 0; }
    if(gdepthrb) { glDeleteRenderbuffers_(1, &gdepthrb); gdepthrb = 0; }
    if(hdrfbo) { glDeleteFramebuffers_(1, &hdrfbo); hdrfbo = 0; }
    if(hdrtex) { glDeleteTextures(1, &hdrtex); hdrtex = 0; }
    if(refractfbo) { glDeleteFramebuffers_(1, &refractfbo); refractfbo = 0; }
    if(refracttex) { glDeleteTextures(1, &refracttex); refracttex = 0; }
    gw = gh = -1;
    cleanupscale();
    cleanupmsbuffer();
    cleardeferredlightshaders();
}

VAR(0, msaadepthblit, 0, 0, 1);

void resolvemsaadepth(int w = vieww, int h = viewh)
{
    if(!msaasamples || msaalight) return;

    timer *resolvetimer = drawtex ? NULL : begintimer("MSAA Depth");

    if(msaadepthblit)
    {
        glBindFramebuffer_(GL_READ_FRAMEBUFFER, msfbo);
        glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, gfbo);
        if(ghasstencil) glClear(GL_STENCIL_BUFFER_BIT);
        glBlitFramebuffer_(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }
    if(!msaadepthblit || gdepthformat)
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, gfbo);
        glViewport(0, 0, w, h);
        maskgbuffer("d");
        if(!msaadepthblit)
        {
            if(ghasstencil)
            {
                glStencilFunc(GL_ALWAYS, 0, ~0);
                glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                glEnable(GL_STENCIL_TEST);
            }
            glDepthFunc(GL_ALWAYS);
            SETSHADER(msaaresolvedepth);
        }
        else
        {
             glDisable(GL_DEPTH_TEST);
             SETSHADER(msaaresolve);
        }
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
        screenquad();
        maskgbuffer("cnd");
        if(!msaadepthblit)
        {
            if(ghasstencil) glDisable(GL_STENCIL_TEST);
            glDepthFunc(GL_LESS);
        }
        else glEnable(GL_DEPTH_TEST);
    }

    endtimer(resolvetimer);
}

void resolvemsaacolor(int w = vieww, int h = viewh)
{
    if(!msaalight) return;

    timer *resolvetimer = drawtex ? NULL : begintimer("MSAA Resolve");

    glBindFramebuffer_(GL_READ_FRAMEBUFFER, mshdrfbo);
    glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, hdrfbo);
    glBlitFramebuffer_(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer_(GL_FRAMEBUFFER, hdrfbo);

    endtimer(resolvetimer);
}

#define HDRVARS(name) \
    FVAR(IDF_WORLD, hdrbright##name, 1e-4f, 1.0f, 1e4f);

HDRVARS();
HDRVARS(alt);

GETVARMPV(hdr, bright, float);

VAR(IDF_PERSIST, bloom, 0, 1, 1);
FVAR(0, bloomthreshold, 1e-3f, 0.9f, 1e3f);
FVAR(IDF_PERSIST, bloomscale, 0, 2.0f, 1e3f);
VAR(IDF_PERSIST, bloomblur, 0, 5, 7);
VAR(IDF_PERSIST, bloomiter, 0, 0, 4);
VARF(IDF_PERSIST, bloomsize, 6, 9, 11, cleanupbloom());
VARF(IDF_PERSIST, bloomprec, 0, 2, 3, cleanupbloom());
FVAR(0, hdraccumscale, 0, 0.98f, 1);
VAR(0, hdraccummillis, 1, 33, 1000);
VAR(0, hdrreduce, 0, 2, 2);
VARF(IDF_PERSIST, hdrprec, 0, 2, 3, cleanupgbuffer());
FVARF(IDF_PERSIST, hdrgamma, 1e-3f, 2, 1e3f, initwarning("HDR setup", INIT_LOAD, CHANGE_SHADERS));
FVAR(0, hdrsaturate, 1e-3f, 0.8f, 1e3f);
VARF(IDF_PERSIST, gscale, 25, 100, 100, cleanupgbuffer());
VARF(IDF_PERSIST, gscalecubic, 0, 0, 1, cleanupgbuffer());
VARF(IDF_PERSIST, gscalenearest, -1, -1, 1, cleanupgbuffer());
FVARF(IDF_PERSIST, gscalecubicsoft, 0, 0, 1, initwarning("scaling setup", INIT_LOAD, CHANGE_SHADERS));

float ldrscale = 1.0f, ldrscaleb = 1.0f/255;

void copyhdr(int sw, int sh, GLuint fbo, int dw, int dh, bool flipx, bool flipy, bool swapxy)
{
    if(!dw) dw = sw;
    if(!dh) dh = sh;

    if(msaalight) resolvemsaacolor(sw, sh);
    GLERROR;

    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, dw, dh);

    SETSHADER(reorient);
    vec reorientx(flipx ? -0.5f : 0.5f, 0, 0.5f), reorienty(0, flipy ? -0.5f : 0.5f, 0.5f);
    if(swapxy) swap(reorientx, reorienty);
    reorientx.mul(sw);
    reorienty.mul(sh);
    LOCALPARAM(reorientx, reorientx);
    LOCALPARAM(reorienty, reorienty);

    glBindTexture(GL_TEXTURE_RECTANGLE, hdrtex);
    screenquad();
    GLERROR;

    hdrclear = 3;
}

void loadhdrshaders(int aa)
{
    switch(aa)
    {
        case AA_LUMA:
            useshaderbyname("hdrtonemapluma");
            useshaderbyname("hdrnopluma");
            if(msaalight > 1 && msaatonemap) useshaderbyname("msaatonemapluma");
            break;
        case AA_MASKED:
            if(!msaasamples && ghasstencil) useshaderbyname("hdrtonemapstencil");
            else
            {
                useshaderbyname("hdrtonemapmasked");
                useshaderbyname("hdrnopmasked");
                if(msaalight > 1 && msaatonemap) useshaderbyname("msaatonemapmasked");
            }
            break;
        case AA_SPLIT:
            useshaderbyname("msaatonemapsplit");
            break;
        case AA_SPLIT_LUMA:
            useshaderbyname("msaatonemapsplitluma");
            break;
        case AA_SPLIT_MASKED:
            useshaderbyname("msaatonemapsplitmasked");
            break;
        default:
            break;
    }
}

void processhdr(GLuint outfbo, int aa)
{
    timer *hdrtimer = begintimer("HDR Processing");

    GLOBALPARAMF(hdrparams, gethdrbright(), hdrsaturate, bloom ? bloomthreshold : 0.f, bloom ? bloomscale : 0.f);

    GLuint b0fbo = bloomfbo[1], b0tex = bloomtex[1], b1fbo =  bloomfbo[0], b1tex = bloomtex[0], ptex = hdrtex;
    int b0w = max(vieww/4, bloomw), b0h = max(viewh/4, bloomh), b1w = max(vieww/2, bloomw), b1h = max(viewh/2, bloomh),
        pw = vieww, ph = viewh;
    if(msaalight)
    {
        if(aa < AA_SPLIT && (msaalight <= 1 || !msaatonemap))
        {
            glBindFramebuffer_(GL_READ_FRAMEBUFFER, mshdrfbo);
            glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, hdrfbo);
            glBlitFramebuffer_(0, 0, vieww, viewh, 0, 0, vieww, viewh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        else if(hasFBMSBS && (vieww > bloomw || viewh > bloomh))
        {
            int cw = max(vieww/2, bloomw), ch = max(viewh/2, bloomh);
            glBindFramebuffer_(GL_READ_FRAMEBUFFER, mshdrfbo);
            glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, hdrfbo);
            glBlitFramebuffer_(0, 0, vieww, viewh, 0, 0, cw, ch, GL_COLOR_BUFFER_BIT, GL_SCALED_RESOLVE_FASTEST_EXT);
            pw = cw;
            ph = ch;
        }
        else
        {
            glBindFramebuffer_(GL_FRAMEBUFFER, hdrfbo);
            if(vieww/2 >= bloomw)
            {
                pw = vieww/2;
                if(viewh/2 >= bloomh)
                {
                    ph = viewh/2;
                    glViewport(0, 0, pw, ph);
                    SETSHADER(msaareduce);
                }
                else
                {
                    glViewport(0, 0, pw, viewh);
                    SETSHADER(msaareducew);
                }
            }
            else
            {
                glViewport(0, 0, vieww, viewh);
                SETSHADER(msaaresolve);
            }
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mshdrtex);
            screenquad(vieww, viewh);
        }
    }
    if(hdrreduce) while(pw > bloomw || ph > bloomh)
    {
        GLuint cfbo = b1fbo, ctex = b1tex;
        int cw = max(pw/2, bloomw), ch = max(ph/2, bloomh);

        if(hdrreduce > 1 && cw/2 >= bloomw)
        {
            cw /= 2;
            if(ch/2 >= bloomh)
            {
                ch /= 2;
                SETSHADER(hdrreduce2);
            }
            else SETSHADER(hdrreduce2w);
        }
        else SETSHADER(hdrreduce);
        if(cw == bloomw && ch == bloomh) { if(bloomfbo[5]) { cfbo = bloomfbo[5]; ctex = bloomtex[5]; } else { cfbo = bloomfbo[2]; ctex = bloomtex[2]; } }
        glBindFramebuffer_(GL_FRAMEBUFFER, cfbo);
        glViewport(0, 0, cw, ch);
        glBindTexture(GL_TEXTURE_RECTANGLE, ptex);
        screenquad(pw, ph);

        ptex = ctex;
        pw = cw;
        ph = ch;
        swap(b0fbo, b1fbo);
        swap(b0tex, b1tex);
        swap(b0w, b1w);
        swap(b0h, b1h);
    }

    if(!lasthdraccum || lastmillis - lasthdraccum >= hdraccummillis)
    {
        GLuint ltex = ptex;
        int lw = pw, lh = ph;
        for(int i = 0; lw > 2 || lh > 2; i++)
        {
            int cw = max(lw/2, 2), ch = max(lh/2, 2);

            if(hdrreduce > 1 && cw/2 >= 2)
            {
                cw /= 2;
                if(ch/2 >= 2)
                {
                    ch /= 2;
                    if(i) SETSHADER(hdrreduce2); else SETSHADER(hdrluminance2);
                }
                else if(i) SETSHADER(hdrreduce2w); else SETSHADER(hdrluminance2w);
            }
            else if(i) SETSHADER(hdrreduce); else SETSHADER(hdrluminance);
            glBindFramebuffer_(GL_FRAMEBUFFER, b1fbo);
            glViewport(0, 0, cw, ch);
            glBindTexture(GL_TEXTURE_RECTANGLE, ltex);
            screenquad(lw, lh);

            ltex = b1tex;
            lw = cw;
            lh = ch;
            swap(b0fbo, b1fbo);
            swap(b0tex, b1tex);
            swap(b0w, b1w);
            swap(b0h, b1h);
        }

        glBindFramebuffer_(GL_FRAMEBUFFER, bloomfbo[4]);
        glViewport(0, 0, bloompbo ? 4 : 1, 1);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
        SETSHADER(hdraccum);
        glBindTexture(GL_TEXTURE_RECTANGLE, b0tex);
        LOCALPARAMF(accumscale, lasthdraccum ? pow(hdraccumscale, float(lastmillis - lasthdraccum)/hdraccummillis) : 0);
        screenquad(2, 2);
        glDisable(GL_BLEND);

        if(bloompbo)
        {
            glBindBuffer_(GL_PIXEL_PACK_BUFFER, bloompbo);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, 4, 1, hasTRG ? GL_RED : GL_RGB, hasTF ? GL_FLOAT : GL_UNSIGNED_SHORT, NULL);
            glBindBuffer_(GL_PIXEL_PACK_BUFFER, 0);
        }

        lasthdraccum = lastmillis;
    }

    if(bloompbo)
    {
        gle::bindvbo(bloompbo);
        gle::enablecolor();
        gle::colorpointer(hasTF ? sizeof(GLfloat) : sizeof(GLushort), (const void *)0, hasTF ? GL_FLOAT : GL_UNSIGNED_SHORT, 1);
        gle::clearvbo();
    }

    b0fbo = bloomfbo[3];
    b0tex = bloomtex[3];
    b1fbo = bloomfbo[2];
    b1tex = bloomtex[2];
    b0w = b1w = bloomw;
    b0h = b1h = bloomh;

    glActiveTexture_(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, bloomtex[4]);
    glActiveTexture_(GL_TEXTURE0);

    glBindFramebuffer_(GL_FRAMEBUFFER, b0fbo);
    glViewport(0, 0, b0w, b0h);
    SETSHADER(hdrbloom);
    glBindTexture(GL_TEXTURE_RECTANGLE, ptex);
    screenquad(pw, ph);

    if(bloom && bloomblur)
    {
        float blurweights[MAXBLURRADIUS+1], bluroffsets[MAXBLURRADIUS+1];
        setupblurkernel(bloomblur, blurweights, bluroffsets);
        loopi(2 + 2*bloomiter)
        {
            glBindFramebuffer_(GL_FRAMEBUFFER, b1fbo);
            glViewport(0, 0, b1w, b1h);
            setblurshader(i%2, 1, bloomblur, blurweights, bluroffsets, GL_TEXTURE_RECTANGLE);
            glBindTexture(GL_TEXTURE_RECTANGLE, b0tex);
            screenquad(b0w, b0h);
            swap(b0w, b1w);
            swap(b0h, b1h);
            swap(b0tex, b1tex);
            swap(b0fbo, b1fbo);
        }
    }

    if(aa >= AA_SPLIT)
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, outfbo);
        glViewport(0, 0, vieww, viewh);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mshdrtex);
        glActiveTexture_(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_RECTANGLE, b0tex);
        glActiveTexture_(GL_TEXTURE0);
        switch(aa)
        {
            case AA_SPLIT_LUMA: SETSHADER(msaatonemapsplitluma); break;
            case AA_SPLIT_MASKED:
                SETSHADER(msaatonemapsplitmasked);
                setaavelocityparams(GL_TEXTURE3);
                break;
            default: SETSHADER(msaatonemapsplit); break;
        }
        screenquad(vieww, viewh, b0w, b0h);
    }
    else if(msaalight <= 1 || !msaatonemap)
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, outfbo);
        glViewport(0, 0, vieww, viewh);
        glBindTexture(GL_TEXTURE_RECTANGLE, hdrtex);
        glActiveTexture_(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_RECTANGLE, b0tex);
        glActiveTexture_(GL_TEXTURE0);
        switch(aa)
        {
            case AA_LUMA: SETSHADER(hdrtonemapluma); break;
            case AA_MASKED:
                if(!msaasamples && ghasstencil)
                {
                    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                    glStencilFunc(GL_EQUAL, 0, 0x80);
                    glEnable(GL_STENCIL_TEST);
                    SETSHADER(hdrtonemap);
                    screenquad(vieww, viewh, b0w, b0h);

                    glStencilFunc(GL_EQUAL, 0x80, 0x80);
                    SETSHADER(hdrtonemapstencil);
                    screenquad(vieww, viewh, b0w, b0h);
                    glDisable(GL_STENCIL_TEST);
                    goto done;
                }
                SETSHADER(hdrtonemapmasked);
                setaavelocityparams(GL_TEXTURE3);
                break;
            default: SETSHADER(hdrtonemap); break;
        }
        screenquad(vieww, viewh, b0w, b0h);
    }
    else
    {
        bool blit = msaalight > 2 && msaatonemapblit && (!aa || !outfbo);

        glBindFramebuffer_(GL_FRAMEBUFFER, blit ? msrefractfbo : outfbo);
        glViewport(0, 0, vieww, viewh);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mshdrtex);
        glActiveTexture_(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_RECTANGLE, b0tex);
        glActiveTexture_(GL_TEXTURE0);

        if(blit) SETSHADER(msaatonemapsample);
        else switch(aa)
        {
            case AA_LUMA: SETSHADER(msaatonemapluma); break;
            case AA_MASKED:
                SETSHADER(msaatonemapmasked);
                setaavelocityparams(GL_TEXTURE3);
                break;
            default: SETSHADER(msaatonemap); break;
        }
        screenquad(vieww, viewh, b0w, b0h);

        if(blit)
        {
            glBindFramebuffer_(GL_READ_FRAMEBUFFER, msrefractfbo);
            glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, aa || !outfbo ? refractfbo : outfbo);
            glBlitFramebuffer_(0, 0, vieww, viewh, 0, 0, vieww, viewh, GL_COLOR_BUFFER_BIT, GL_NEAREST);

            if(!outfbo)
            {
                glBindFramebuffer_(GL_FRAMEBUFFER, outfbo);
                glViewport(0, 0, vieww, viewh);
                if(!blit) SETSHADER(hdrnop);
                else switch(aa)
                {
                    case AA_LUMA: SETSHADER(hdrnopluma); break;
                    case AA_MASKED:
                        SETSHADER(hdrnopmasked);
                        setaavelocityparams(GL_TEXTURE3);
                        break;
                    default: SETSHADER(hdrnop); break;
                }
                glBindTexture(GL_TEXTURE_RECTANGLE, refracttex);
                screenquad(vieww, viewh);
            }
        }
    }

done:
    if(bloompbo) gle::disablecolor();

    endtimer(hdrtimer);
}

VAR(0, debugdepth, 0, 0, 1);

void viewdepth()
{
    int w = min(hudw, hudh)/2, h = (w*hudh)/hudw;
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    debugquad(0, 0, w, h, 0, 0, gw, gh);
}

VAR(0, debugstencil, 0, 0, 0xFF);

void viewstencil()
{
    if(!ghasstencil || !hdrfbo) return;
    glBindFramebuffer_(GL_FRAMEBUFFER, hdrfbo);
    glViewport(0, 0, gw, gh);

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glStencilFunc(GL_NOTEQUAL, 0, debugstencil);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glEnable(GL_STENCIL_TEST);
    SETSHADER(hudnotexture);
    gle::colorf(1, 1, 1);
    debugquad(0, 0, hudw, hudh, 0, 0, gw, gh);
    glDisable(GL_STENCIL_TEST);

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, hudw, hudh);

    int w = min(hudw, hudh)/2, h = (w*hudh)/hudw;
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, hdrtex);
    debugquad(0, 0, w, h, 0, 0, gw, gh);
}

VAR(0, debugrefract, 0, 0, 1);

void viewrefract()
{
    int w = min(hudw, hudh)/2, h = (w*hudh)/hudw;
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, refracttex);
    debugquad(0, 0, w, h, 0, 0, gw, gh);
}

#define RH_MAXSPLITS 4
#define RH_MAXGRID 64

GLuint rhtex[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }, rhrb[4] = { 0, 0, 0, 0 }, rhfbo = 0;
uint rhclearmasks[2][RH_MAXSPLITS][(RH_MAXGRID+2+31)/32];
GLuint rsmdepthtex = 0, rsmcolortex = 0, rsmnormaltex = 0, rsmfbo = 0;

extern int rhrect, rhgrid, rhsplits, rhborder, rhprec, rhtaps, rhcache, rhforce, rsmprec, rsmdepthprec, rsmsize;

static Shader *radiancehintsshader = NULL;
Shader *rsmworldshader = NULL;

Shader *loadradiancehintsshader()
{
    defformatstring(name, "radiancehints%d", rhtaps);
    return generateshader(name, "radiancehintsshader %d", rhtaps);
}

void loadrhshaders()
{
    if(rhborder) useshaderbyname("radiancehintsborder");
    if(rhcache) useshaderbyname("radiancehintscached");
    useshaderbyname("radiancehintsdisable");
    radiancehintsshader = loadradiancehintsshader();
    rsmworldshader = useshaderbyname("rsmworld");
    useshaderbyname("rsmsky");
}

void clearrhshaders()
{
    radiancehintsshader = NULL;
    rsmworldshader = NULL;
}

void setupradiancehints()
{
    GLenum rhformat = hasTF && rhprec >= 1 ? GL_RGBA16F : GL_RGBA8;

    loopi(!rhrect && rhcache ? 8 : 4)
    {
        if(!rhtex[i]) glGenTextures(1, &rhtex[i]);
        create3dtexture(rhtex[i], rhgrid+2*rhborder, rhgrid+2*rhborder, (rhgrid+2*rhborder)*rhsplits, NULL, 7, 1, rhformat);
        if(rhborder)
        {
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
            GLfloat border[4] = { 0.5f, 0.5f, 0.5f, 0 };
            glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, border);
        }
    }

    if(!rhfbo) glGenFramebuffers_(1, &rhfbo);
    glBindFramebuffer_(GL_FRAMEBUFFER, rhfbo);

    if(rhrect) loopi(4)
    {
        if(!rhrb[i]) glGenRenderbuffers_(1, &rhrb[i]);
        glBindRenderbuffer_(GL_RENDERBUFFER, rhrb[i]);
        glRenderbufferStorage_(GL_RENDERBUFFER, rhformat, (rhgrid + 2*rhborder)*(rhgrid + 2*rhborder), (rhgrid + 2*rhborder)*rhsplits);
        glBindRenderbuffer_(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, rhrb[i]);
    }
    else loopi(4) glFramebufferTexture3D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_3D, rhtex[i], 0, 0);

    static const GLenum drawbufs[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
    glDrawBuffers_(4, drawbufs);

    if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fatal("Failed allocating radiance hints buffer!");

    if(!rsmdepthtex) glGenTextures(1, &rsmdepthtex);
    if(!rsmcolortex) glGenTextures(1, &rsmcolortex);
    if(!rsmnormaltex) glGenTextures(1, &rsmnormaltex);

    if(!rsmfbo) glGenFramebuffers_(1, &rsmfbo);

    glBindFramebuffer_(GL_FRAMEBUFFER, rsmfbo);

    GLenum rsmformat = gethdrformat(rsmprec, GL_RGBA8);

    createtexture(rsmdepthtex, rsmsize, rsmsize, NULL, 3, 0, rsmdepthprec > 1 ? GL_DEPTH_COMPONENT32 : (rsmdepthprec ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16), GL_TEXTURE_RECTANGLE);
    createtexture(rsmcolortex, rsmsize, rsmsize, NULL, 3, 0, rsmformat, GL_TEXTURE_RECTANGLE);
    createtexture(rsmnormaltex, rsmsize, rsmsize, NULL, 3, 0, rsmformat, GL_TEXTURE_RECTANGLE);

    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_RECTANGLE, rsmdepthtex, 0);
    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, rsmcolortex, 0);
    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_RECTANGLE, rsmnormaltex, 0);

    glDrawBuffers_(2, drawbufs);

    if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fatal("Failed allocating RSM buffer!");

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);

    loadrhshaders();

    clearradiancehintscache();
}

void cleanupradiancehints()
{
    clearradiancehintscache();

    loopi(8) if(rhtex[i]) { glDeleteTextures(1, &rhtex[i]); rhtex[i] = 0; }
    loopi(4) if(rhrb[i]) { glDeleteRenderbuffers_(1, &rhrb[i]); rhrb[i] = 0; }
    if(rhfbo) { glDeleteFramebuffers_(1, &rhfbo); rhfbo = 0; }
    if(rsmdepthtex) { glDeleteTextures(1, &rsmdepthtex); rsmdepthtex = 0; }
    if(rsmcolortex) { glDeleteTextures(1, &rsmcolortex); rsmcolortex = 0; }
    if(rsmnormaltex) { glDeleteTextures(1, &rsmnormaltex); rsmnormaltex = 0; }
    if(rsmfbo) { glDeleteFramebuffers_(1, &rsmfbo); rsmfbo = 0; }

    clearrhshaders();
}

VARF(0, rhrect, 0, 0, 1, cleanupradiancehints());
VARF(0, rhsplits, 1, 2, RH_MAXSPLITS, { cleardeferredlightshaders(); cleanupradiancehints(); });
VARF(0, rhborder, 0, 1, 1, cleanupradiancehints());
VARF(0, rsmsize, 64, 384, 2048, cleanupradiancehints());
VARF(IDF_WORLD, rhnearplane, 1, 1, 16, clearradiancehintscache());
VARF(IDF_WORLD, rhfarplane, 64, 1024, 16384, clearradiancehintscache());
FVARF(0, rsmpradiustweak, 1e-3f, 1, 1e3f, clearradiancehintscache());
FVARF(0, rhpradiustweak, 1e-3f, 1, 1e3f, clearradiancehintscache());
FVARF(0, rsmdepthrange, 0, 1024, 1e6f, clearradiancehintscache());
FVARF(0, rsmdepthmargin, 0, 0.1f, 1e3f, clearradiancehintscache());
VARF(IDF_PERSIST, rhprec, 0, 0, 1, cleanupradiancehints());
VARF(IDF_PERSIST, rsmprec, 0, 0, 3, cleanupradiancehints());
VARF(IDF_PERSIST, rsmdepthprec, 0, 0, 2, cleanupradiancehints());
FVAR(0, rhnudge, 0, 0.5f, 4);
FVARF(0, rhworldbias, 0, 0.5f, 10, clearradiancehintscache());
FVARF(0, rhsplitweight, 0.20f, 0.6f, 0.95f, clearradiancehintscache());
VARF(0, rhgrid, 3, 27, RH_MAXGRID, cleanupradiancehints());
FVARF(0, rsmspread, 0, 0.15f, 1, clearradiancehintscache());
VAR(0, rhclipgrid, 0, 1, 1);
VARF(0, rhcache, 0, 1, 1, cleanupradiancehints());
VARF(0, rhforce, 0, 0, 1, cleanupradiancehints());
VAR(0, rsmcull, 0, 1, 1);
VARF(IDF_PERSIST, rhtaps, 0, 20, 32, cleanupradiancehints());
VAR(0, rhdyntex, 0, 0, 1);
VAR(0, rhdynmm, 0, 0, 1);

#define GIVARS(name) \
    VARF(IDF_WORLD, gidist##name, 0, 384, 1024, { clearradiancehintscache(); cleardeferredlightshaders(); if(!gidist##name) cleanupradiancehints(); }); \
    FVARF(IDF_WORLD, giscale##name, 0, 1.5f, 1e3f, { cleardeferredlightshaders(); if(!giscale##name) cleanupradiancehints(); }); \
    FVAR(IDF_WORLD, giaoscale##name, 0, 3, 1e3f);

GIVARS();
GIVARS(alt);

GETVARMPV(gi, dist, float);
GETVARMPV(gi, scale, float);
GETVARMPV(gi, aoscale, float);

VARF(IDF_PERSIST, gi, 0, 1, 1, { cleardeferredlightshaders(); cleanupradiancehints(); });

VAR(0, debugrsm, 0, 0, 2);
void viewrsm()
{
    int w = min(hudw, hudh)/2, h = (w*hudh)/hudw, x = hudw-w, y = hudh-h;
    SETSHADER(hudrect);
    gle::colorf(1, 1, 1);
    glBindTexture(GL_TEXTURE_RECTANGLE, debugrsm == 2 ? rsmnormaltex : rsmcolortex);
    debugquad(x, y, w, h, 0, 0, rsmsize, rsmsize);
}

VAR(0, debugrh, -1, 0, RH_MAXSPLITS*(RH_MAXGRID + 2));
void viewrh()
{
    int w = min(hudw, hudh)/2, h = (w*hudh)/hudw, x = hudw-w, y = hudh-h;
    gle::colorf(1, 1, 1);
    if(debugrh < 0 && rhrect)
    {
        SETSHADER(hudrect);
        glBindTexture(GL_TEXTURE_RECTANGLE, rhtex[5]);
        float tw = (rhgrid+2*rhborder)*(rhgrid+2*rhborder), th = (rhgrid+2*rhborder)*rhsplits;
        gle::defvertex(2);
        gle::deftexcoord0(2);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x,   y);   gle::attribf(0,  0);
        gle::attribf(x+w, y);   gle::attribf(tw, 0);
        gle::attribf(x,   y+h); gle::attribf(0,  th);
        gle::attribf(x+w, y+h); gle::attribf(tw, th);
        gle::end();
    }
    else
    {
        SETSHADER(hud3d);
        glBindTexture(GL_TEXTURE_3D, rhtex[1]);
        float z = (max(debugrh, 1)-1+0.5f)/float((rhgrid+2*rhborder)*rhsplits);
        gle::defvertex(2);
        gle::deftexcoord0(3);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x,   y);   gle::attribf(0, 0, z);
        gle::attribf(x+w, y);   gle::attribf(1, 0, z);
        gle::attribf(x,   y+h); gle::attribf(0, 1, z);
        gle::attribf(x+w, y+h); gle::attribf(1, 1, z);
        gle::end();
    }
}

#define SHADOWATLAS_SIZE 4096

PackNode shadowatlaspacker(0, 0, SHADOWATLAS_SIZE, SHADOWATLAS_SIZE);

extern int smminradius;

struct lightinfo
{
    int ent, shadowmap, flags;
    vec o, color;
    float radius, dist;
    vec dir, spotx, spoty;
    int spot;
    float sx1, sy1, sx2, sy2, sz1, sz2;
    occludequery *query;

    lightinfo() {}
    lightinfo(const vec &o, const vec &color, float radius, int flags = 0, const vec &dir = vec(0, 0, 0), int spot = 0)
      : ent(-1), shadowmap(-1), flags(flags),
        o(o), color(color), radius(radius), dist(camera1->o.dist(o)),
        dir(dir), spot(spot), query(NULL)
    {
        if(spot > 0) calcspot();
        calcscissor();
    }
    lightinfo(int i, const vec &o, const vec &color, float radius, int flags = 0, const vec &dir = vec(0, 0, 0), int spot = 0)
      : ent(i), shadowmap(-1), flags(flags),
        o(o), color(color), radius(radius), dist(camera1->o.dist(o)),
        dir(dir), spot(spot), query(NULL)
    {
        if(spot > 0) calcspot();
        calcscissor();
    }

    void calcspot()
    {
        quat orient(dir, vec(0, 0, dir.z < 0 ? -1 : 1));
        spotx = orient.invertedrotate(vec(1, 0, 0));
        spoty = orient.invertedrotate(vec(0, 1, 0));
    }

    bool noshadow() const { return flags&L_NOSHADOW || radius <= smminradius; }
    bool nospec() const { return (flags&L_NOSPEC) != 0; }
    bool volumetric() const { return (flags&L_VOLUMETRIC) != 0; }

    void addscissor(float &dx1, float &dy1, float &dx2, float &dy2) const
    {
        dx1 = min(dx1, sx1);
        dy1 = min(dy1, sy1);
        dx2 = max(dx2, sx2);
        dy2 = max(dy2, sy2);
    }

    void addscissor(float &dx1, float &dy1, float &dx2, float &dy2, float &dz1, float &dz2) const
    {
        addscissor(dx1, dy1, dx2, dy2);
        dz1 = min(dz1, sz1);
        dz2 = max(dz2, sz2);
    }

    bool validscissor() const { return sx1 < sx2 && sy1 < sy2 && sz1 < sz2; }

    void calcscissor()
    {
        sx1 = sy1 = sz1 = -1;
        sx2 = sy2 = sz2 = 1;
        if(spot > 0) calcspotscissor(o, radius, dir, spot, spotx, spoty, sx1, sy1, sx2, sy2, sz1, sz2);
        else calcspherescissor(o, radius, sx1, sy1, sx2, sy2, sz1, sz2);
    }

    bool checkquery() const { return query && query->owner == this && ::checkquery(query); }

    void calcbb(vec &bbmin, vec &bbmax)
    {
        if(spot > 0)
        {
            float spotscale = radius * tan360(spot);
            vec up = vec(spotx).mul(spotscale).abs(), right = vec(spoty).mul(spotscale).abs(), center = vec(dir).mul(radius).add(o);
            bbmin = bbmax = center;
            bbmin.sub(up).sub(right);
            bbmax.add(up).add(right);
            bbmin.min(o);
            bbmax.max(o);
        }
        else
        {
            bbmin = vec(o).sub(radius);
            bbmax = vec(o).add(radius);
        }
    }
};

struct shadowcachekey
{
    vec o;
    float radius;
    vec dir;
    int spot;

    shadowcachekey() {}
    shadowcachekey(const lightinfo &l) : o(l.o), radius(l.radius), dir(l.dir), spot(l.spot) {}
};

static inline uint hthash(const shadowcachekey &k)
{
    return hthash(k.o);
}

static inline bool htcmp(const shadowcachekey &x, const shadowcachekey &y)
{
    return x.o == y.o && x.radius == y.radius && x.dir == y.dir && x.spot == y.spot;
}

struct shadowcacheval;

struct shadowmapinfo
{
    ushort x, y, size, sidemask;
    int light;
    shadowcacheval *cached;
};

struct shadowcacheval
{
    ushort x, y, size, sidemask;

    shadowcacheval() {}
    shadowcacheval(const shadowmapinfo &sm) : x(sm.x), y(sm.y), size(sm.size), sidemask(sm.sidemask) {}
};

struct shadowcache : hashtable<shadowcachekey, shadowcacheval>
{
    shadowcache() : hashtable<shadowcachekey, shadowcacheval>(256) {}

    void reset()
    {
        clear();
    }
};

extern int smcache, smfilter, smgather;

#define SHADOWCACHE_EVICT 2

GLuint shadowatlastex = 0, shadowatlasfbo = 0;
GLenum shadowatlastarget = GL_NONE;
shadowcache shadowcache;
bool shadowcachefull = false;
int evictshadowcache = 0;

static inline void setsmnoncomparemode() // use texture gather
{
    glTexParameteri(shadowatlastarget, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glTexParameteri(shadowatlastarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(shadowatlastarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

static inline void setsmcomparemode() // use embedded shadow cmp
{
    glTexParameteri(shadowatlastarget, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(shadowatlastarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(shadowatlastarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

extern int usetexgather;
static inline bool usegatherforsm() { return smfilter > 1 && smgather && hasTG && usetexgather; }
static inline bool usesmcomparemode() { return !usegatherforsm() || (hasTG && hasGPU5 && usetexgather > 1); }

void viewshadowatlas()
{
    int w = min(hudw, hudh)/2, h = (w*hudh)/hudw, x = hudw-w, y = hudh-h;
    float tw = 1, th = 1;
    if(shadowatlastarget == GL_TEXTURE_RECTANGLE)
    {
        tw = shadowatlaspacker.w;
        th = shadowatlaspacker.h;
        SETSHADER(hudrect);
    }
    else hudshader->set();
    gle::colorf(1, 1, 1);
    glBindTexture(shadowatlastarget, shadowatlastex);
    if(usesmcomparemode()) setsmnoncomparemode();
    debugquad(x, y, w, h, 0, 0, tw, th);
    if(usesmcomparemode()) setsmcomparemode();
}
VAR(0, debugshadowatlas, 0, 0, 1);

extern int smdepthprec, smsize;

void setupshadowatlas()
{
    int size = min((1<<smsize), hwtexsize);
    shadowatlaspacker.resize(size, size);

    if(!shadowatlastex) glGenTextures(1, &shadowatlastex);

    shadowatlastarget = usegatherforsm() ? GL_TEXTURE_2D : GL_TEXTURE_RECTANGLE;
    createtexture(shadowatlastex, shadowatlaspacker.w, shadowatlaspacker.h, NULL, 3, 1, smdepthprec > 1 ? GL_DEPTH_COMPONENT32 : (smdepthprec ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16), shadowatlastarget);
    glTexParameteri(shadowatlastarget, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(shadowatlastarget, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    if(!shadowatlasfbo) glGenFramebuffers_(1, &shadowatlasfbo);

    glBindFramebuffer_(GL_FRAMEBUFFER, shadowatlasfbo);

    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowatlastarget, shadowatlastex, 0);

    if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fatal("Failed allocating shadow atlas!");

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
}

void cleanupshadowatlas()
{
    if(shadowatlastex) { glDeleteTextures(1, &shadowatlastex); shadowatlastex = 0; }
    if(shadowatlasfbo) { glDeleteFramebuffers_(1, &shadowatlasfbo); shadowatlasfbo = 0; }
    clearshadowcache();
}

const matrix4 cubeshadowviewmatrix[6] =
{
    // sign-preserving cubemap projections
    matrix4(vec(0, 0, 1), vec(0, 1, 0), vec(-1, 0, 0)), // +X
    matrix4(vec(0, 0, 1), vec(0, 1, 0), vec( 1, 0, 0)), // -X
    matrix4(vec(1, 0, 0), vec(0, 0, 1), vec(0, -1, 0)), // +Y
    matrix4(vec(1, 0, 0), vec(0, 0, 1), vec(0,  1, 0)), // -Y
    matrix4(vec(1, 0, 0), vec(0, 1, 0), vec(0, 0, -1)), // +Z
    matrix4(vec(1, 0, 0), vec(0, 1, 0), vec(0, 0,  1))  // -Z
};

FVAR(0, smpolyfactor, -1e3f, 1, 1e3f);
FVAR(0, smpolyoffset, -1e3f, 0, 1e3f);
FVAR(0, smbias, -1e6f, 0.01f, 1e6f);
FVAR(0, smpolyfactor2, -1e3f, 1.5f, 1e3f);
FVAR(0, smpolyoffset2, -1e3f, 0, 1e3f);
FVAR(0, smbias2, -1e6f, 0.02f, 1e6f);
FVAR(0, smprec, 1e-3f, 1, 1e3f);
FVAR(0, smcubeprec, 1e-3f, 1, 1e3f);
FVAR(0, smspotprec, 1e-3f, 1, 1e3f);

VARF(IDF_PERSIST, smsize, 10, 12, 14, cleanupshadowatlas());
VARF(IDF_PERSIST, smdepthprec, 0, 0, 2, cleanupshadowatlas());
VAR(0, smsidecull, 0, 1, 1);
VAR(0, smviscull, 0, 1, 1);
VAR(0, smborder, 0, 3, 16);
VAR(0, smborder2, 0, 4, 16);
VAR(0, smminradius, 0, 16, 10000);
VAR(0, smminsize, 1, 96, 1024);
VAR(0, smmaxsize, 1, 384, 1024);
//VAR(0, smmaxsize, 1, 4096, 4096);
VAR(0, smused, 1, 0, 0);
VAR(0, smquery, 0, 1, 1);
VARF(0, smcullside, 0, 1, 1, cleanupshadowatlas());
VARF(0, smcache, 0, 1, 2, cleanupshadowatlas());
VARF(IDF_PERSIST, smfilter, 0, 2, 3, { cleardeferredlightshaders(); cleanupshadowatlas(); cleanupvolumetric(); });
VARF(IDF_PERSIST, smgather, 0, 0, 1, { cleardeferredlightshaders(); cleanupshadowatlas(); cleanupvolumetric(); });
VAR(0, smnoshadow, 0, 0, 1);
VAR(0, smdynshadow, 0, 1, 1);
VAR(0, lightpassesused, 1, 0, 0);
VAR(0, lightsvisible, 1, 0, 0);
VAR(0, lightsoccluded, 1, 0, 0);
VARN(0, lightbatches, lightbatchesused, 1, 0, 0);
VARN(0, lightbatchrects, lightbatchrectsused, 1, 0, 0);
VARN(0, lightbatchstacks, lightbatchstacksused, 1, 0, 0);

enum
{
    MAXLIGHTTILEBATCH = 8
};

VARF(0, lighttilebatch, 0, MAXLIGHTTILEBATCH, MAXLIGHTTILEBATCH, cleardeferredlightshaders());
VARF(0, batchsunlight, 0, 2, 2, cleardeferredlightshaders());

int shadowmapping = 0;

struct lightrect
{
    uchar x1, y1, x2, y2;

    lightrect() {}
    lightrect(uchar x1, uchar y1, uchar x2, uchar y2) : x1(x1), y1(y1), x2(x2), y2(y2) {}
    lightrect(const lightinfo &l)
    {
        calctilebounds(l.sx1, l.sy1, l.sx2, l.sy2, x1, y1, x2, y2);
    }

    bool outside(const lightrect &o) const
    {
        return x1 >= o.x2 || x2 <= o.x1 || y1 >= o.y2 || y2 <= o.y1;
    }

    bool inside(const lightrect &o) const
    {
        return x1 >= o.x1 && x2 <= o.x2 && y1 >= o.y1 && y2 <= o.y2;
    }

    void intersect(const lightrect &o)
    {
        x1 = max(x1, o.x1);
        y1 = max(y1, o.y1);
        x2 = min(x2, o.x2);
        y2 = min(y2, o.y2);
    }

    bool overlaps(int tx1, int ty1, int tx2, int ty2, const uint *tilemask) const
    {
        if(int(x2) <= tx1 || int(x1) >= tx2 || int(y2) <= ty1 || int(y1) >= ty2) return false;
        if(!tilemask) return true;
        uint xmask = (1<<x2) - (1<<x1);
        for(int y = max(int(y1), ty1), end = min(int(y2), ty2); y < end; y++) if(tilemask[y] & xmask) return true;
        return false;
    }
};

enum
{
    BF_SPOTLIGHT = 1<<0,
    BF_NOSHADOW  = 1<<1,
    BF_NOSUN     = 1<<2
};

struct lightbatchkey
{
    uchar flags, numlights;
    ushort lights[MAXLIGHTTILEBATCH];
};

struct lightbatch : lightbatchkey
{
    vector<lightrect> rects;

    void reset()
    {
        rects.setsize(0);
    }

    bool overlaps(int tx1, int ty1, int tx2, int ty2, const uint *tilemask) const
    {
        if(!tx1 && !ty1 && tx2 >= lighttilew && ty2 >= lighttileh && !tilemask) return true;
        loopv(rects) if(rects[i].overlaps(tx1, ty1, tx2, ty2, tilemask)) return true;
        return false;
    }
};

static inline void htrecycle(lightbatch &l)
{
    l.reset();
}

static inline uint hthash(const lightbatchkey &l)
{
    uint h = 0;
    loopi(l.numlights) h = ((h<<8)+h)^l.lights[i];
    return h;
}

static inline bool htcmp(const lightbatchkey &x, const lightbatchkey &y)
{
    return x.flags == y.flags &&
           x.numlights == y.numlights &&
           (!x.numlights || !memcmp(x.lights, y.lights, x.numlights*sizeof(x.lights[0])));
}

vector<lightinfo> lights;
vector<int> lightorder;
hashset<lightbatch> lightbatcher(128);
vector<lightbatch *> lightbatches;
vector<shadowmapinfo> shadowmaps;

void clearshadowcache()
{
    shadowmaps.setsize(0);

    clearradiancehintscache();
    clearshadowmeshes();
}

static shadowmapinfo *addshadowmap(ushort x, ushort y, int size, int &idx, int light = -1, shadowcacheval *cached = NULL)
{
    idx = shadowmaps.length();
    shadowmapinfo *sm = &shadowmaps.add();
    sm->x = x;
    sm->y = y;
    sm->size = size;
    sm->light = light;
    sm->sidemask = 0;
    sm->cached = cached;
    return sm;
}

#define CSM_MAXSPLITS 8

VARF(0, csmmaxsize, 256, 768, 2048, clearshadowcache());
VARF(0, csmsplits, 1, 3, CSM_MAXSPLITS, { cleardeferredlightshaders(); clearshadowcache(); });
FVAR(0, csmsplitweight, 0.20f, 0.75f, 0.95f);
VARF(0, csmshadowmap, 0, 1, 1, { cleardeferredlightshaders(); clearshadowcache(); });

// cascaded shadow maps
struct cascadedshadowmap
{
    struct splitinfo
    {
        float nearplane;     // split distance to near plane
        float farplane;      // split distance to farplane
        matrix4 proj;      // one projection per split
        vec scale, offset;   // scale and offset of the projection
        int idx;             // shadowmapinfo indices
        vec center, bounds;  // max extents of shadowmap in sunlight model space
        plane cull[4];       // world space culling planes of the split's projected sides
    };
    matrix4 model;                // model view is shared by all splits
    splitinfo splits[CSM_MAXSPLITS]; // per-split parameters
    vec lightview;                  // view vector for light
    void setup();                   // insert shadowmaps for each split frustum if there is sunlight
    void updatesplitdist();         // compute split frustum distances
    void getmodelmatrix();          // compute the shared model matrix
    void getprojmatrix();           // compute each cropped projection matrix
    void gencullplanes();           // generate culling planes for the mvp matrix
    void bindparams();              // bind any shader params necessary for lighting
};

void cascadedshadowmap::setup()
{
    int size = (csmmaxsize * shadowatlaspacker.w) / SHADOWATLAS_SIZE;
    loopi(csmsplits)
    {
        ushort smx = USHRT_MAX, smy = USHRT_MAX;
        splits[i].idx = -1;
        if(shadowatlaspacker.insert(smx, smy, size, size))
            addshadowmap(smx, smy, size, splits[i].idx);
    }
    getmodelmatrix();
    getprojmatrix();
    gencullplanes();
}

VAR(IDF_WORLD, csmnearplane, 1, 1, 16);
VAR(IDF_WORLD, csmfarplane, 64, 1024, 16384);
FVAR(0, csmpradiustweak, 1e-3f, 1, 1e3f);
FVAR(0, csmdepthrange, 0, 1024, 1e6f);
FVAR(0, csmdepthmargin, 0, 0.1f, 1e3f);
FVAR(0, csmpolyfactor, -1e3f, 2, 1e3f);
FVAR(0, csmpolyoffset, -1e4f, 0, 1e4f);
FVAR(0, csmbias, -1e6f, 1e-4f, 1e6f);
FVAR(0, csmpolyfactor2, -1e3f, 3, 1e3f);
FVAR(0, csmpolyoffset2, -1e4f, 0, 1e4f);
FVAR(0, csmbias2, -1e16f, 2e-4f, 1e6f);
VAR(0, csmcull, 0, 1, 1);

void cascadedshadowmap::updatesplitdist()
{
    float lambda = csmsplitweight, nd = csmnearplane, fd = csmfarplane, ratio = fd/nd;
    splits[0].nearplane = nd;
    for(int i = 1; i < csmsplits; ++i)
    {
        float si = i / float(csmsplits);
        splits[i].nearplane = lambda*(nd*pow(ratio, si)) + (1-lambda)*(nd + (fd - nd)*si);
        splits[i-1].farplane = splits[i].nearplane * 1.005f;
    }
    splits[csmsplits-1].farplane = fd;
}

void cascadedshadowmap::getmodelmatrix()
{
    model = viewmatrix;
    model.rotate_around_x(getpielightpitch()*RAD);
    model.rotate_around_z((180-getpielightyaw())*RAD);
}

void cascadedshadowmap::getprojmatrix()
{
    lightview = vec(getpielightdir()).neg();

    // compute the split frustums
    updatesplitdist();

    // find z extent
    float minz = lightview.project_bb(worldmin, worldmax), maxz = lightview.project_bb(worldmax, worldmin),
          zmargin = max((maxz - minz)*csmdepthmargin, 0.5f*(csmdepthrange - (maxz - minz)));
    minz -= zmargin;
    maxz += zmargin;

    // compute each split projection matrix
    loopi(csmsplits)
    {
        splitinfo &split = splits[i];
        if(split.idx < 0) continue;
        const shadowmapinfo &sm = shadowmaps[split.idx];

        vec c;
        float radius = calcfrustumboundsphere(split.nearplane, split.farplane, camera1->o, camdir, c);

        // compute the projected bounding box of the sphere
        vec tc;
        model.transform(c, tc);
        int border = smfilter > 2 ? smborder2 : smborder;
        const float pradius = ceil(radius * csmpradiustweak), step = (2*pradius) / (sm.size - 2*border);
        vec2 offset = vec2(tc).sub(pradius).div(step);
        offset.x = floor(offset.x);
        offset.y = floor(offset.y);
        split.center = vec(vec2(offset).mul(step).add(pradius), -0.5f*(minz + maxz));
        split.bounds = vec(pradius, pradius, 0.5f*(maxz - minz));

        // modify mvp with a scale and offset
        // now compute the update model view matrix for this split
        split.scale = vec(1/step, 1/step, -1/(maxz - minz));
        split.offset = vec(border - offset.x, border - offset.y, -minz/(maxz - minz));

        split.proj.identity();
        split.proj.settranslation(2*split.offset.x/sm.size - 1, 2*split.offset.y/sm.size - 1, 2*split.offset.z - 1);
        split.proj.setscale(2*split.scale.x/sm.size, 2*split.scale.y/sm.size, 2*split.scale.z);
    }
}

void cascadedshadowmap::gencullplanes()
{
    loopi(csmsplits)
    {
        splitinfo &split = splits[i];
        matrix4 mvp;
        mvp.mul(split.proj, model);
        vec4 px = mvp.rowx(), py = mvp.rowy(), pw = mvp.roww();
        split.cull[0] = plane(vec4(pw).add(px)).normalize(); // left plane
        split.cull[1] = plane(vec4(pw).sub(px)).normalize(); // right plane
        split.cull[2] = plane(vec4(pw).add(py)).normalize(); // bottom plane
        split.cull[3] = plane(vec4(pw).sub(py)).normalize(); // top plane
    }
}

void cascadedshadowmap::bindparams()
{
    GLOBALPARAM(csmmatrix, matrix3(model));

    static GlobalShaderParam csmtc("csmtc"), csmoffset("csmoffset");
    vec4 *csmtcv = csmtc.reserve<vec4>(csmsplits);
    vec *csmoffsetv = csmoffset.reserve<vec>(csmsplits);
    loopi(csmsplits)
    {
        cascadedshadowmap::splitinfo &split = splits[i];
        if(split.idx < 0) continue;
        const shadowmapinfo &sm = shadowmaps[split.idx];

        csmtcv[i] = vec4(vec2(split.center).mul(-split.scale.x), split.scale.x, split.bounds.x*split.scale.x);

        const float bias = (smfilter > 2 ? csmbias2 : csmbias) * (-512.0f / sm.size) * (split.farplane - split.nearplane) / (splits[0].farplane - splits[0].nearplane);
        csmoffsetv[i] = vec(sm.x, sm.y, 0.5f + bias).add2(0.5f*sm.size);
    }
    GLOBALPARAMF(csmz, splits[0].center.z*-splits[0].scale.z, splits[0].scale.z);
}

cascadedshadowmap csm;

int calcbbcsmsplits(const ivec &bbmin, const ivec &bbmax)
{
    int mask = (1<<csmsplits)-1;
    if(!csmcull) return mask;
    loopi(csmsplits)
    {
        const cascadedshadowmap::splitinfo &split = csm.splits[i];
        int k;
        for(k = 0; k < 4; k++)
        {
            const plane &p = split.cull[k];
            ivec omin, omax;
            if(p.x > 0) { omin.x = bbmin.x; omax.x = bbmax.x; } else { omin.x = bbmax.x; omax.x = bbmin.x; }
            if(p.y > 0) { omin.y = bbmin.y; omax.y = bbmax.y; } else { omin.y = bbmax.y; omax.y = bbmin.y; }
            if(p.z > 0) { omin.z = bbmin.z; omax.z = bbmax.z; } else { omin.z = bbmax.z; omax.z = bbmin.z; }
            if(omax.dist(p) < 0) { mask &= ~(1<<i); goto nextsplit; }
            if(omin.dist(p) < 0) goto notinside;
        }
        mask &= (2<<i)-1;
        break;
    notinside:
        while(++k < 4)
        {
            const plane &p = split.cull[k];
            ivec omax(p.x > 0 ? bbmax.x : bbmin.x, p.y > 0 ? bbmax.y : bbmin.y, p.z > 0 ? bbmax.z : bbmin.z);
            if(omax.dist(p) < 0) { mask &= ~(1<<i); break; }
        }
    nextsplit:;
    }
    return mask;
}

int calcspherecsmsplits(const vec &center, float radius)
{
    int mask = (1<<csmsplits)-1;
    if(!csmcull) return mask;
    loopi(csmsplits)
    {
        const cascadedshadowmap::splitinfo &split = csm.splits[i];
        int k;
        for(k = 0; k < 4; k++)
        {
            const plane &p = split.cull[k];
            float dist = p.dist(center);
            if(dist < -radius) { mask &= ~(1<<i); goto nextsplit; }
            if(dist < radius) goto notinside;
        }
        mask &= (2<<i)-1;
        break;
    notinside:
        while(++k < 4)
        {
            const plane &p = split.cull[k];
            if(p.dist(center) < -radius) { mask &= ~(1<<i); break; }
        }
    nextsplit:;
    }
    return mask;
}

struct reflectiveshadowmap
{
    matrix4 model, proj;
    vec lightview;
    plane cull[4];
    vec scale, offset;
    vec center, bounds;
    void setup();
    void getmodelmatrix();
    void getprojmatrix();
    void gencullplanes();
} rsm;

void reflectiveshadowmap::setup()
{
    getmodelmatrix();
    getprojmatrix();
    gencullplanes();
}

void reflectiveshadowmap::getmodelmatrix()
{
    model = viewmatrix;
    model.rotate_around_x(getpielightpitch()*RAD);
    model.rotate_around_z((180-getpielightyaw())*RAD);
}

void reflectiveshadowmap::getprojmatrix()
{
    lightview = vec(getpielightdir()).neg();

    // find z extent
    float minz = lightview.project_bb(worldmin, worldmax), maxz = lightview.project_bb(worldmax, worldmin),
          zmargin = max((maxz - minz)*rsmdepthmargin, 0.5f*(rsmdepthrange - (maxz - minz)));
    minz -= zmargin;
    maxz += zmargin;

    vec c;
    float radius = calcfrustumboundsphere(rhnearplane, rhfarplane, camera1->o, camdir, c);

    // compute the projected bounding box of the sphere
    vec tc;
    model.transform(c, tc);
    const float pradius = ceil((radius + getgidist()) * rsmpradiustweak), step = (2*pradius) / rsmsize;
    vec2 tcoff = vec2(tc).sub(pradius).div(step);
    tcoff.x = floor(tcoff.x);
    tcoff.y = floor(tcoff.y);
    center = vec(vec2(tcoff).mul(step).add(pradius), -0.5f*(minz + maxz));
    bounds = vec(pradius, pradius, 0.5f*(maxz - minz));

    scale = vec(1/step, 1/step, -1/(maxz - minz));
    offset = vec(-tcoff.x, -tcoff.y, -minz/(maxz - minz));

    proj.identity();
    proj.settranslation(2*offset.x/rsmsize - 1, 2*offset.y/rsmsize - 1, 2*offset.z - 1);
    proj.setscale(2*scale.x/rsmsize, 2*scale.y/rsmsize, 2*scale.z);
}

void reflectiveshadowmap::gencullplanes()
{
    matrix4 mvp;
    mvp.mul(proj, model);
    vec4 px = mvp.rowx(), py = mvp.rowy(), pw = mvp.roww();
    cull[0] = plane(vec4(pw).add(px)).normalize(); // left plane
    cull[1] = plane(vec4(pw).sub(px)).normalize(); // right plane
    cull[2] = plane(vec4(pw).add(py)).normalize(); // bottom plane
    cull[3] = plane(vec4(pw).sub(py)).normalize(); // top plane
}

int calcbbrsmsplits(const ivec &bbmin, const ivec &bbmax)
{
    if(!rsmcull) return 1;
    loopk(4)
    {
        const plane &p = rsm.cull[k];
        ivec omin, omax;
        if(p.x > 0) { omin.x = bbmin.x; omax.x = bbmax.x; } else { omin.x = bbmax.x; omax.x = bbmin.x; }
        if(p.y > 0) { omin.y = bbmin.y; omax.y = bbmax.y; } else { omin.y = bbmax.y; omax.y = bbmin.y; }
        if(p.z > 0) { omin.z = bbmin.z; omax.z = bbmax.z; } else { omin.z = bbmax.z; omax.z = bbmin.z; }
        if(omax.dist(p) < 0) return 0;
        if(omin.dist(p) < 0) while(++k < 4)
        {
            const plane &p = rsm.cull[k];
            ivec omax(p.x > 0 ? bbmax.x : bbmin.x, p.y > 0 ? bbmax.y : bbmin.y, p.z > 0 ? bbmax.z : bbmin.z);
            if(omax.dist(p) < 0) return 0;
        }
    }
    return 1;
}

int calcspherersmsplits(const vec &center, float radius)
{
    if(!rsmcull) return 1;
    loopk(4)
    {
        const plane &p = rsm.cull[k];
        float dist = p.dist(center);
        if(dist < -radius) return 0;
        if(dist < radius) while(++k < 4)
        {
            const plane &p = rsm.cull[k];
            if(p.dist(center) < -radius) return 0;
        }
    }
    return 1;
}

struct radiancehints
{
    struct splitinfo
    {
        float nearplane, farplane;
        vec offset, scale;
        vec center; float bounds;
        vec cached; bool copied;

        splitinfo() : center(-1e16f, -1e16f, -1e16f), bounds(-1e16f), cached(-1e16f, -1e16f, -1e16f), copied(false) {}

        void clearcache() { bounds = -1e16f; }
    } splits[RH_MAXSPLITS];

    vec dynmin, dynmax, prevdynmin, prevdynmax;

    radiancehints() : dynmin(1e16f, 1e16f, 1e16f), dynmax(-1e16f, -1e16f, -1e16f), prevdynmin(1e16f, 1e16f, 1e16f), prevdynmax(-1e16f, -1e16f, -1e16f) {}

    void setup();
    void updatesplitdist();
    void bindparams();
    void renderslices();

    void clearcache() { loopi(RH_MAXSPLITS) splits[i].clearcache(); }
    bool allcached() const { loopi(rhsplits) if(splits[i].cached != splits[i].center) return false; return true; }
} rh;

void clearradiancehintscache()
{
    rh.clearcache();
    memset(rhclearmasks, 0, sizeof(rhclearmasks));
}

void radiancehints::updatesplitdist()
{
    float lambda = rhsplitweight, nd = rhnearplane, fd = rhfarplane, ratio = fd/nd;
    splits[0].nearplane = nd;
    for(int i = 1; i < rhsplits; ++i)
    {
        float si = i / float(rhsplits);
        splits[i].nearplane = lambda*(nd*pow(ratio, si)) + (1-lambda)*(nd + (fd - nd)*si);
        splits[i-1].farplane = splits[i].nearplane * 1.005f;
    }
    splits[rhsplits-1].farplane = fd;
}

void radiancehints::setup()
{
    updatesplitdist();

    loopi(rhsplits)
    {
        splitinfo &split = splits[i];

        vec c;
        float radius = calcfrustumboundsphere(split.nearplane, split.farplane, camera1->o, camdir, c);

        // compute the projected bounding box of the sphere
        const float pradius = ceil(radius * rhpradiustweak), step = (2*pradius) / rhgrid;
        vec offset = vec(c).sub(pradius).div(step);
        offset.x = floor(offset.x);
        offset.y = floor(offset.y);
        offset.z = floor(offset.z);
        split.cached = split.bounds == pradius ? split.center : vec(-1e16f, -1e16f, -1e16f);
        split.center = vec(offset).mul(step).add(pradius);
        split.bounds = pradius;

        // modify mvp with a scale and offset
        // now compute the update model view matrix for this split
        split.scale = vec(1/(step*(rhgrid+2*rhborder)), 1/(step*(rhgrid+2*rhborder)), 1/(step*(rhgrid+2*rhborder)*rhsplits));
        split.offset = vec(-(offset.x-rhborder)/(rhgrid+2*rhborder), -(offset.y-rhborder)/(rhgrid+2*rhborder), (i - (offset.z-rhborder)/(rhgrid+2*rhborder))/float(rhsplits));
    }
}

void radiancehints::bindparams()
{
    float step = 2*splits[0].bounds/rhgrid;
    GLOBALPARAMF(rhnudge, rhnudge*step);

    static GlobalShaderParam rhtc("rhtc");
    vec4 *rhtcv = rhtc.reserve<vec4>(rhsplits);
    loopi(rhsplits)
    {
        splitinfo &split = splits[i];
        rhtcv[i] = vec4(vec(split.center).mul(-split.scale.x), split.scale.x);//split.bounds*(1 + rhborder*2*0.5f/rhgrid));
    }
    GLOBALPARAMF(rhbounds, 0.5f*(rhgrid + rhborder)/float(rhgrid + 2*rhborder));
}

bool useradiancehints()
{
    return !getpielight().iszero() && csmshadowmap && gi && getgiscale() && getgidist();
}

FVAR(0, avatarshadowdist, 0, 12, 100);
FVAR(0, avatarshadowbias, 0, 8, 100);
VARF(0, avatarshadowstencil, 0, 1, 2, initwarning("g-buffer setup", INIT_LOAD, CHANGE_SHADERS));

int avatarmask = 0;

bool useavatarmask() { return avatarshadowstencil && ghasstencil && (!msaasamples || (msaalight && avatarshadowstencil > 1)); }

void enableavatarmask()
{
    if(useavatarmask())
    {
        avatarmask = 0x40;
        glStencilFunc(GL_ALWAYS, avatarmask, ~0);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glEnable(GL_STENCIL_TEST);
    }
}

void disableavatarmask()
{
    if(avatarmask)
    {
        avatarmask = 0;
        glDisable(GL_STENCIL_TEST);
    }
}

VAR(0, forcespotlights, 1, 0, 0);

extern int spotlights;

static Shader *volumetricshader = NULL, *volumetricbilateralshader[2] = { NULL, NULL };

void clearvolumetricshaders()
{
    volumetricshader = NULL;

    loopi(2) volumetricbilateralshader[i] = NULL;
}

extern int volsteps, volbilateral, volblur, volreduce;

Shader *loadvolumetricshader()
{
    string common, shadow;
    int commonlen = 0, shadowlen = 0;

    if(usegatherforsm()) common[commonlen++] = smfilter > 2 ? 'G' : 'g';
    else if(smfilter) common[commonlen++] = smfilter > 2 ? 'E' : (smfilter > 1 ? 'F' : 'f');
    if(spotlights || forcespotlights) common[commonlen++] = 's';
    common[commonlen] = '\0';

    shadow[shadowlen++] = 'p';
    shadow[shadowlen] = '\0';

    defformatstring(name, "volumetric%s%s%d", common, shadow, volsteps);
    return generateshader(name, "volumetricshader \"%s\" \"%s\" %d", common, shadow, volsteps);
}

void loadvolumetricshaders()
{
    volumetricshader = loadvolumetricshader();

    if(volbilateral) loopi(2)
    {
        defformatstring(name, "volumetricbilateral%c%d%d", 'x' + i, volbilateral, volreduce);
        volumetricbilateralshader[i] = generateshader(name, "volumetricbilateralshader %d %d", volbilateral, volreduce);
    }
}

static int volw = -1, volh = -1;
static GLuint volfbo[2] = { 0, 0 }, voltex[2] = { 0, 0 };

void setupvolumetric(int w, int h)
{
    volw = w>>volreduce;
    volh = h>>volreduce;

    loopi(volbilateral || volblur ? 2 : 1)
    {
        if(!voltex[i]) glGenTextures(1, &voltex[i]);
        if(!volfbo[i]) glGenFramebuffers_(1, &volfbo[i]);

        glBindFramebuffer_(GL_FRAMEBUFFER, volfbo[i]);

        createtexture(voltex[i], volw, volh, NULL, 3, 1, hdrformat, GL_TEXTURE_RECTANGLE);

        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, voltex[i], 0);

        if(glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fatal("Failed allocating volumetric buffer!");
    }

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);

    loadvolumetricshaders();
}

void cleanupvolumetric()
{
    loopi(2) if(volfbo[i]) { glDeleteFramebuffers_(1, &volfbo[i]); volfbo[i] = 0; }
    loopi(2) if(voltex[i]) { glDeleteTextures(1, &voltex[i]); voltex[i] = 0; }
    volw = volh = -1;

    clearvolumetricshaders();
}

VARF(IDF_PERSIST, volumetric, 0, 1, 1, cleanupvolumetric());
VARF(IDF_PERSIST, volreduce, 0, 1, 2, cleanupvolumetric());
VARF(IDF_PERSIST, volbilateral, 0, 2, 3, cleanupvolumetric());
FVAR(0, volbilateraldepth, 0, 4, 1e3f);
VARF(IDF_PERSIST, volblur, 0, 1, 3, cleanupvolumetric());
VARF(IDF_PERSIST, volsteps, 1, 16, 64, cleanupvolumetric());
FVAR(0, volminstep, 0, 0.0625f, 1e3f);
FVAR(IDF_PERSIST, volprefilter, 0, 4, 1e3f);
FVAR(0, voldistclamp, 0, 0.99f, 2);

#define VOLVARS(name) \
    CVAR1(IDF_WORLD, volcolour##name, 0x808080); \
    FVAR(IDF_WORLD, volscale##name, 0, 1, 16);

VOLVARS();
VOLVARS(alt);

GETVARMPV(vol, colour, const bvec &);
GETVARMPV(vol, scale, float);

static Shader *deferredlightshader = NULL, *deferredminimapshader = NULL, *deferredmsaapixelshader = NULL, *deferredmsaasampleshader = NULL;

void cleardeferredlightshaders()
{
    deferredlightshader = NULL;
    deferredminimapshader = NULL;
    deferredmsaapixelshader = NULL;
    deferredmsaasampleshader = NULL;
}

extern int nospeclights;

Shader *loaddeferredlightshader(const char *type = NULL)
{
    string common, shadow, sun;
    int commonlen = 0, shadowlen = 0, sunlen = 0;

    bool minimap = false, multisample = false, avatar = true;
    if(type)
    {
        if(strchr(type, 'm')) minimap = true;
        if(strchr(type, 'M')) multisample = true;
        if(strchr(type, 'D')) avatar = false;
        copystring(common, type);
        commonlen = strlen(common);
    }
    if(!minimap)
    {
        if(!multisample || msaalight) common[commonlen++] = 't';
        if(avatar && useavatarmask()) common[commonlen++] = 'd';
        if(lighttilebatch)
        {
            common[commonlen++] = 'n';
            common[commonlen++] = '0' + lighttilebatch;
        }
    }
    if(usegatherforsm()) common[commonlen++] = smfilter > 2 ? 'G' : 'g';
    else if(smfilter) common[commonlen++] = smfilter > 2 ? 'E' : (smfilter > 1 ? 'F' : 'f');
    if(spotlights || forcespotlights) common[commonlen++] = 's';
    if(nospeclights) common[commonlen++] = 'z';
    common[commonlen] = '\0';

    shadow[shadowlen++] = 'p';
    shadow[shadowlen] = '\0';

    int usecsm = 0, userh = 0;
    if(!getpielight().iszero() && csmshadowmap)
    {
        usecsm = csmsplits;
        sun[sunlen++] = 'c';
        sun[sunlen++] = '0' + csmsplits;
        if(!minimap)
        {
            if(avatar && ao && getaosun()) sun[sunlen++] = 'A';
            if(gi && getgiscale() && getgidist())
            {
                userh = rhsplits;
                sun[sunlen++] = 'r';
                sun[sunlen++] = '0' + rhsplits;
            }
        }
    }
    if(!minimap)
    {
        if(avatar && ao) sun[sunlen++] = 'a';
        if(lighttilebatch && (!usecsm || batchsunlight > (userh ? 1 : 0))) sun[sunlen++] = 'b';
    }
    sun[sunlen] = '\0';

    defformatstring(name, "deferredlight%s%s%s", common, shadow, sun);
    return generateshader(name, "deferredlightshader \"%s\" \"%s\" \"%s\" %d %d %d", common, shadow, sun, usecsm, userh, !minimap ? lighttilebatch : 0);
}

void loaddeferredlightshaders()
{
    if(msaasamples)
    {
        string opts;
        if(msaalight > 2) copystring(opts, "MS");
        else if(msaalight==2) copystring(opts, ghasstencil || !msaaedgedetect ? "MO" : "MOT");
        else formatstring(opts, ghasstencil || !msaaedgedetect ? "MR%d" : "MRT%d", msaasamples);
        deferredmsaasampleshader = loaddeferredlightshader(opts);
        deferredmsaapixelshader = loaddeferredlightshader("M");
        deferredlightshader = msaalight ? deferredmsaapixelshader : loaddeferredlightshader("D");
    }
    else deferredlightshader = loaddeferredlightshader();
}

static inline bool sortlights(int x, int y)
{
    const lightinfo &xl = lights[x], &yl = lights[y];
    if(!xl.spot) { if(yl.spot) return true; }
    else if(!yl.spot) return false;
    if(!xl.noshadow()) { if(yl.noshadow()) return true; }
    else if(!yl.noshadow()) return false;
    if(xl.sz1 < yl.sz1) return true;
    else if(xl.sz1 > yl.sz1) return false;
    return xl.dist - xl.radius < yl.dist - yl.radius;
}

VAR(0, lighttilealignw, 1, 16, 256);
VAR(0, lighttilealignh, 1, 16, 256);
VARN(0, lighttilew, lighttilemaxw, 1, 10, LIGHTTILE_MAXW);
VARN(0, lighttileh, lighttilemaxh, 1, 10, LIGHTTILE_MAXH);

int lighttilew = 0, lighttileh = 0, lighttilevieww = 0, lighttileviewh = 0;

void calctilesize()
{
    lighttilevieww = (vieww + lighttilealignw - 1)/lighttilealignw;
    lighttileviewh = (viewh + lighttilealignh - 1)/lighttilealignh;
    lighttilew = min(lighttilevieww, lighttilemaxw);
    lighttileh = min(lighttileviewh, lighttilemaxh);
}

void resetlights()
{
    shadowcache.reset();
    if(smcache)
    {
        int evictx = ((evictshadowcache%SHADOWCACHE_EVICT)*shadowatlaspacker.w)/SHADOWCACHE_EVICT,
            evicty = ((evictshadowcache/SHADOWCACHE_EVICT)*shadowatlaspacker.h)/SHADOWCACHE_EVICT,
            evictx2 = (((evictshadowcache%SHADOWCACHE_EVICT)+1)*shadowatlaspacker.w)/SHADOWCACHE_EVICT,
            evicty2 = (((evictshadowcache/SHADOWCACHE_EVICT)+1)*shadowatlaspacker.h)/SHADOWCACHE_EVICT;
        loopv(shadowmaps)
        {
            shadowmapinfo &sm = shadowmaps[i];
            if(sm.light < 0) continue;
            lightinfo &l = lights[sm.light];
            if(sm.cached && shadowcachefull)
            {
                int w = l.spot ? sm.size : sm.size*3, h = l.spot ? sm.size : sm.size*2;
                if(sm.x < evictx2 && sm.x + w > evictx && sm.y < evicty2 && sm.y + h > evicty) continue;
            }
            shadowcache[l] = sm;
        }
        if(shadowcachefull)
        {
            evictshadowcache = (evictshadowcache + 1)%(SHADOWCACHE_EVICT*SHADOWCACHE_EVICT);
            shadowcachefull = false;
        }
    }

    lights.setsize(0);
    lightorder.setsize(0);

    shadowmaps.setsize(0);
    shadowatlaspacker.reset();

    calctilesize();
}

namespace lightsphere
{
    vec *verts = NULL;
    GLushort *indices = NULL;
    int numverts = 0, numindices = 0;
    GLuint vbuf = 0, ebuf = 0;

    void init(int slices, int stacks)
    {
        numverts = (stacks+1)*(slices+1);
        verts = new vec[numverts];
        float ds = 1.0f/slices, dt = 1.0f/stacks, t = 1.0f;
        loopi(stacks+1)
        {
            float rho = M_PI*(1-t), s = 0.0f, sinrho = i && i < stacks ? sin(rho) : 0, cosrho = !i ? 1 : (i < stacks ? cos(rho) : -1);
            loopj(slices+1)
            {
                float theta = j==slices ? 0 : 2*M_PI*s;
                verts[i*(slices+1) + j] = vec(-sin(theta)*sinrho, -cos(theta)*sinrho, cosrho);
                s += ds;
            }
            t -= dt;
        }

        numindices = (stacks-1)*slices*3*2;
        indices = new ushort[numindices];
        GLushort *curindex = indices;
        loopi(stacks)
        {
            loopk(slices)
            {
                int j = i%2 ? slices-k-1 : k;
                if(i)
                {
                    *curindex++ = i*(slices+1)+j;
                    *curindex++ = i*(slices+1)+j+1;
                    *curindex++ = (i+1)*(slices+1)+j;
                }
                if(i+1 < stacks)
                {
                    *curindex++ = i*(slices+1)+j+1;
                    *curindex++ = (i+1)*(slices+1)+j+1;
                    *curindex++ = (i+1)*(slices+1)+j;
                }
            }
        }

        if(!vbuf) glGenBuffers_(1, &vbuf);
        gle::bindvbo(vbuf);
        glBufferData_(GL_ARRAY_BUFFER, numverts*sizeof(vec), verts, GL_STATIC_DRAW);
        DELETEA(verts);

        if(!ebuf) glGenBuffers_(1, &ebuf);
        gle::bindebo(ebuf);
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER, numindices*sizeof(GLushort), indices, GL_STATIC_DRAW);
        DELETEA(indices);
    }

    void cleanup()
    {
        if(vbuf) { glDeleteBuffers_(1, &vbuf); vbuf = 0; }
        if(ebuf) { glDeleteBuffers_(1, &ebuf); ebuf = 0; }
    }

    void enable()
    {
        if(!vbuf) init(8, 4);
        gle::bindvbo(vbuf);
        gle::bindebo(ebuf);
        gle::vertexpointer(sizeof(vec), verts);
        gle::enablevertex();
    }

    void draw()
    {
        glDrawRangeElements_(GL_TRIANGLES, 0, numverts-1, numindices, GL_UNSIGNED_SHORT, indices);
        xtraverts += numindices;
        glde++;
    }

    void disable()
    {
        gle::disablevertex();
        gle::clearvbo();
        gle::clearebo();
    }
}

VAR(0, depthtestlights, 0, 2, 2);
FVAR(0, depthtestlightsclamp, 0, 0.999995f, 1);
VAR(0, depthfaillights, 0, 1, 1);
FVAR(0, lightradiustweak, 1, 1.11f, 2);

static inline void lightquad(float z = -1, float sx1 = -1, float sy1 = -1, float sx2 = 1, float sy2 = 1)
{
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(sx2, sy1, z);
    gle::attribf(sx1, sy1, z);
    gle::attribf(sx2, sy2, z);
    gle::attribf(sx1, sy2, z);
    gle::end();
}

static inline void lightquads(float z, float sx1, float sy1, float sx2, float sy2)
{
    gle::attribf(sx1, sy2, z);
    gle::attribf(sx2, sy2, z);
    gle::attribf(sx2, sy1, z);
    gle::attribf(sx1, sy1, z);
}

static inline void lightquads(float z, float sx1, float sy1, float sx2, float sy2, int tx1, int ty1, int tx2, int ty2)
{
    int vx1 = max(int(floor((sx1*0.5f+0.5f)*vieww)), ((tx1*lighttilevieww)/lighttilew)*lighttilealignw),
        vy1 = max(int(floor((sy1*0.5f+0.5f)*viewh)), ((ty1*lighttileviewh)/lighttileh)*lighttilealignh),
        vx2 = min(int(ceil((sx2*0.5f+0.5f)*vieww)), min(((tx2*lighttilevieww)/lighttilew)*lighttilealignw, vieww)),
        vy2 = min(int(ceil((sy2*0.5f+0.5f)*viewh)), min(((ty2*lighttileviewh)/lighttileh)*lighttilealignh, viewh));
    lightquads(z, (vx1*2.0f)/vieww-1.0f, (vy1*2.0f)/viewh-1.0f, (vx2*2.0f)/vieww-1.0f, (vy2*2.0f)/viewh-1.0f);
}

static inline void lightquads(float z, float sx1, float sy1, float sx2, float sy2, int x1, int y1, int x2, int y2, const uint *tilemask)
{
    if(!tilemask) lightquads(z, sx1, sy1, sx2, sy2, x1, y1, x2, y2);
    else for(int y = y1; y < y2;)
    {
        int starty = y;
        uint xmask = (1<<x2) - (1<<x1), startmask = tilemask[y] & xmask;
        do ++y; while(y < y2 && (tilemask[y]&xmask) == startmask);
        for(int x = x1; x < x2;)
        {
            while(x < x2 && !(startmask&(1<<x))) ++x;
            if(x >= x2) break;
            int startx = x;
            do ++x; while(x < x2 && startmask&(1<<x));
            lightquads(z, sx1, sy1, sx2, sy2, startx, starty, x, y);
        }
    }
}

static void lightquad(float sz1, float bsx1, float bsy1, float bsx2, float bsy2, const uint *tilemask)
{
    int btx1, bty1, btx2, bty2;
    calctilebounds(bsx1, bsy1, bsx2, bsy2, btx1, bty1, btx2, bty2);

    gle::begin(GL_QUADS);
    lightquads(sz1, bsx1, bsy1, bsx2, bsy2, btx1, bty1, btx2, bty2, tilemask);
    gle::end();
}

static void bindlighttexs(int msaapass = 0, bool transparent = false)
{
    if(msaapass) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mscolortex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, gcolortex);
    glActiveTexture_(GL_TEXTURE1);
    if(msaapass) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msnormaltex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, gnormaltex);
    if(transparent)
    {
        glActiveTexture_(GL_TEXTURE2);
        if(msaapass) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msglowtex);
        else glBindTexture(GL_TEXTURE_RECTANGLE, gglowtex);
    }
    glActiveTexture_(GL_TEXTURE3);
    if(msaapass) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    glActiveTexture_(GL_TEXTURE4);
    glBindTexture(shadowatlastarget, shadowatlastex);
    if(usesmcomparemode()) setsmcomparemode(); else setsmnoncomparemode();
    if(ao)
    {
        glActiveTexture_(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_RECTANGLE, aotex[2] ? aotex[2] : aotex[0]);
    }
    if(useradiancehints()) loopi(4)
    {
        glActiveTexture_(GL_TEXTURE6 + i);
        glBindTexture(GL_TEXTURE_3D, rhtex[i]);
    }
    glActiveTexture_(GL_TEXTURE0);
}

static inline void setlightglobals(bool transparent = false)
{
    GLOBALPARAMF(shadowatlasscale, 1.0f/shadowatlaspacker.w, 1.0f/shadowatlaspacker.h);
    if(ao)
    {
        if(transparent || drawtex || (editmode && fullbright))
        {
            GLOBALPARAMF(aoscale, 0.0f, 0.0f);
            GLOBALPARAMF(aoparams, 1.0f, 0.0f, 1.0f, 0.0f);
        }
        else
        {
            GLOBALPARAM(aoscale, aotex[2] ? vec2(1, 1) : vec2(float(aow)/vieww, float(aoh)/viewh));
            GLOBALPARAMF(aoparams, getaomin(), 1.0f-getaomin(), getaosunmin(), 1.0f-getaosunmin());
        }
    }
    float lightscale = 2.0f*ldrscaleb;
    if(!drawtex && editmode && fullbright)
        GLOBALPARAMF(lightscale, fullbrightlevel*lightscale, fullbrightlevel*lightscale, fullbrightlevel*lightscale, 255*lightscale);
    else
    {
        bvec curambient = getambient();
        float curambientscale = getambientscale();
        GLOBALPARAMF(lightscale, curambient.x*lightscale*curambientscale, curambient.y*lightscale*curambientscale, curambient.z*lightscale*curambientscale, 255*lightscale);
    }

    bvec pie = getpielight();
    if(!pie.iszero() && csmshadowmap)
    {
        csm.bindparams();
        rh.bindparams();
        if(!drawtex && editmode && fullbright)
        {
            GLOBALPARAMF(sunlightdir, 0, 0, 0);
            GLOBALPARAMF(sunlightcolor, 0, 0, 0);
            GLOBALPARAMF(giscale, 0);
            GLOBALPARAMF(skylightcolor, 0, 0, 0);
        }
        else
        {
            bvec piesky = getskylight();
            vec piedir = getpielightdir();
            float piescale = getpielightscale(), pieskyscale = getskylightscale();
            GLOBALPARAM(sunlightdir, piedir);
            GLOBALPARAMF(sunlightcolor, pie.x*lightscale*piescale, pie.y*lightscale*piescale, pie.z*lightscale*piescale);
            GLOBALPARAMF(giscale, 2*getgiscale());
            GLOBALPARAMF(skylightcolor, 2*getgiaoscale()*piesky.x*lightscale*pieskyscale, 2*getgiaoscale()*piesky.y*lightscale*pieskyscale, 2*getgiaoscale()*piesky.z*lightscale*pieskyscale);
        }
    }

    matrix4 lightmatrix;
    lightmatrix.identity();
    GLOBALPARAM(lightmatrix, lightmatrix);
}

static LocalShaderParam lightpos("lightpos"), lightcolor("lightcolor"), spotparams("spotparams"), shadowparams("shadowparams"), shadowoffset("shadowoffset");
static vec4 lightposv[8], lightcolorv[8], spotparamsv[8], shadowparamsv[8];
static vec2 shadowoffsetv[8];

static inline void setlightparams(int i, const lightinfo &l)
{
    lightposv[i] = vec4(l.o, 1).div(l.radius);
    lightcolorv[i] = vec4(vec(l.color).mul(2*ldrscaleb), l.nospec() ? 0 : 1);
    if(l.spot > 0) spotparamsv[i] = vec4(vec(l.dir).neg(), 1/(1 - cos360(l.spot)));
    if(l.shadowmap >= 0)
    {
        shadowmapinfo &sm = shadowmaps[l.shadowmap];
        float smnearclip = SQRT3 / l.radius, smfarclip = SQRT3,
              bias = (smfilter > 2 || shadowatlaspacker.w > SHADOWATLAS_SIZE ? smbias2 : smbias) * (smcullside ? 1 : -1) * smnearclip * (1024.0f / sm.size);
        int border = smfilter > 2 ? smborder2 : smborder;
        if(l.spot > 0)
        {
            shadowparamsv[i] = vec4(
                -0.5f * sm.size * cotan360(l.spot),
                (-smnearclip * smfarclip / (smfarclip - smnearclip) - 0.5f*bias),
                1 / (1 + fabs(l.dir.z)),
                0.5f + 0.5f * (smfarclip + smnearclip) / (smfarclip - smnearclip));
        }
        else
        {
            shadowparamsv[i] = vec4(
                -0.5f * (sm.size - border),
                -smnearclip * smfarclip / (smfarclip - smnearclip) - 0.5f*bias,
                sm.size,
                0.5f + 0.5f * (smfarclip + smnearclip) / (smfarclip - smnearclip));
        }
        shadowoffsetv[i] = vec2(sm.x + 0.5f*sm.size, sm.y + 0.5f*sm.size);
    }
}

static inline void setlightshader(Shader *s, int n, bool baselight, bool shadowmap, bool spotlight, bool transparent = false, bool avatar = false)
{
    s->setvariant(n-1, (shadowmap ? 1 : 0) + (baselight ? 0 : 2) + (spotlight ? 4 : 0) + (transparent ? 8 : 0) + (avatar ? 24 : 0));
    lightpos.setv(lightposv, n);
    lightcolor.setv(lightcolorv, n);
    if(spotlight) spotparams.setv(spotparamsv, n);
    if(shadowmap)
    {
        shadowparams.setv(shadowparamsv, n);
        shadowoffset.setv(shadowoffsetv, n);
    }
}

static inline void setavatarstencil(int stencilref, bool on)
{
    glStencilFunc(GL_EQUAL, (on ? 0x40 : 0) | stencilref, !(stencilref&0x08) && msaalight==2 ? 0x47 : 0x4F);
}

static void rendersunpass(Shader *s, int stencilref, bool transparent, float bsx1, float bsy1, float bsx2, float bsy2, const uint *tilemask)
{
    if(hasDBT && depthtestlights > 1) glDepthBounds_(0, depthtestlightsclamp);

    int tx1 = max(int(floor((bsx1*0.5f+0.5f)*vieww)), 0), ty1 = max(int(floor((bsy1*0.5f+0.5f)*viewh)), 0),
        tx2 = min(int(ceil((bsx2*0.5f+0.5f)*vieww)), vieww), ty2 = min(int(ceil((bsy2*0.5f+0.5f)*viewh)), viewh);
    s->setvariant(transparent ? 0 : -1, 16);
    lightquad(-1, (tx1*2.0f)/vieww-1.0f, (ty1*2.0f)/viewh-1.0f, (tx2*2.0f)/vieww-1.0f, (ty2*2.0f)/viewh-1.0f, tilemask);
    lightpassesused++;

    if(stencilref >= 0)
    {
        setavatarstencil(stencilref, true);

        s->setvariant(0, 17);
        lightquad(-1, (tx1*2.0f)/vieww-1.0f, (ty1*2.0f)/viewh-1.0f, (tx2*2.0f)/vieww-1.0f, (ty2*2.0f)/viewh-1.0f, tilemask);
        lightpassesused++;

        setavatarstencil(stencilref, false);
    }
}

static void renderlightsnobatch(Shader *s, int stencilref, bool transparent, float bsx1, float bsy1, float bsx2, float bsy2)
{
    lightsphere::enable();

    glEnable(GL_SCISSOR_TEST);

    bool outside = true;
    loop(avatarpass, stencilref >= 0 ? 2 : 1)
    {
        if(avatarpass) setavatarstencil(stencilref, true);

        loopv(lightorder)
        {
            const lightinfo &l = lights[lightorder[i]];
            float sx1 = max(bsx1, l.sx1), sy1 = max(bsy1, l.sy1),
                  sx2 = min(bsx2, l.sx2), sy2 = min(bsy2, l.sy2);
            if(sx1 >= sx2 || sy1 >= sy2 || l.sz1 >= l.sz2 || (avatarpass && l.dist - l.radius > avatarshadowdist)) continue;

            matrix4 lightmatrix = camprojmatrix;
            lightmatrix.translate(l.o);
            lightmatrix.scale(l.radius*lightradiustweak);
            GLOBALPARAM(lightmatrix, lightmatrix);

            setlightparams(0, l);
            setlightshader(s, 1, false, l.shadowmap >= 0, l.spot > 0, transparent, avatarpass > 0);

            int tx1 = int(floor((sx1*0.5f+0.5f)*vieww)), ty1 = int(floor((sy1*0.5f+0.5f)*viewh)),
                tx2 = int(ceil((sx2*0.5f+0.5f)*vieww)), ty2 = int(ceil((sy2*0.5f+0.5f)*viewh));
            glScissor(tx1, ty1, tx2-tx1, ty2-ty1);

            if(hasDBT && depthtestlights > 1) glDepthBounds_(l.sz1*0.5f + 0.5f, min(l.sz2*0.5f + 0.5f, depthtestlightsclamp));

            if(camera1->o.dist(l.o) <= l.radius*lightradiustweak + nearplane + 1 && depthfaillights)
            {
                if(outside)
                {
                    outside = false;
                    glDepthFunc(GL_GEQUAL);
                    glCullFace(GL_FRONT);
                }
            }
            else if(!outside)
            {
                outside = true;
                glDepthFunc(GL_LESS);
                glCullFace(GL_BACK);
            }

            lightsphere::draw();

            lightpassesused++;
        }

        if(avatarpass) setavatarstencil(stencilref, false);
    }

    if(!outside)
    {
        outside = true;
        glDepthFunc(GL_LESS);
        glCullFace(GL_BACK);
    }

    glDisable(GL_SCISSOR_TEST);

    lightsphere::disable();
}

static void renderlightbatches(Shader *s, int stencilref, bool transparent, float bsx1, float bsy1, float bsx2, float bsy2, const uint *tilemask)
{
    bool sunpass = !getpielight().iszero() && csmshadowmap && batchsunlight <= (gi && getgiscale() && getgidist() ? 1 : 0);
    int btx1, bty1, btx2, bty2;
    calctilebounds(bsx1, bsy1, bsx2, bsy2, btx1, bty1, btx2, bty2);
    loopv(lightbatches)
    {
        lightbatch &batch = *lightbatches[i];
        if(!batch.overlaps(btx1, bty1, btx2, bty2, tilemask)) continue;

        int n = batch.numlights;
        float sx1 = 1, sy1 = 1, sx2 = -1, sy2 = -1, sz1 = 1, sz2 = -1;
        loopj(n)
        {
            const lightinfo &l = lights[batch.lights[j]];
            setlightparams(j, l);
            l.addscissor(sx1, sy1, sx2, sy2, sz1, sz2);
        }

        bool baselight = !(batch.flags & BF_NOSUN) && !sunpass;
        if(baselight) { sx1 = bsx1; sy1 = bsy1; sx2 = bsx2; sy2 = bsy2; sz1 = -1; sz2 = 1; }
        else
        {
            sx1 = max(sx1, bsx1); sy1 = max(sy1, bsy1); sx2 = min(sx2, bsx2); sy2 = min(sy2, bsy2);
            if(sx1 >= sx2 || sy1 >= sy2 || sz1 >= sz2) continue;
        }

        if(n)
        {
            bool shadowmap = !(batch.flags & BF_NOSHADOW), spotlight = (batch.flags & BF_SPOTLIGHT) != 0;
            setlightshader(s, n, baselight, shadowmap, spotlight, transparent);
        }
        else s->setvariant(transparent ? 0 : -1, 16);

        lightpassesused++;

        if(hasDBT && depthtestlights > 1) glDepthBounds_(sz1*0.5f + 0.5f, min(sz2*0.5f + 0.5f, depthtestlightsclamp));
        gle::begin(GL_QUADS);
        loopvj(batch.rects)
        {
            const lightrect &r = batch.rects[j];
            int x1 = max(int(r.x1), btx1), y1 = max(int(r.y1), bty1),
                x2 = min(int(r.x2), btx2), y2 = min(int(r.y2), bty2);
            if(x1 < x2 && y1 < y2) lightquads(sz1, sx1, sy1, sx2, sy2, x1, y1, x2, y2, tilemask);
        }
        gle::end();
    }

    if(stencilref >= 0)
    {
        setavatarstencil(stencilref, true);

        bool baselight = !sunpass;
        for(int offset = 0; baselight || offset < lightorder.length(); baselight = false)
        {
            int n = 0;
            bool shadowmap = false, spotlight = false;
            float sx1 = 1, sy1 = 1, sx2 = -1, sy2 = -1, sz1 = 1, sz2 = -1;
            for(; offset < lightorder.length(); offset++)
            {
                const lightinfo &l = lights[lightorder[offset]];
                if(l.dist - l.radius > avatarshadowdist) continue;
                if(!n)
                {
                    shadowmap = l.shadowmap >= 0;
                    spotlight = l.spot > 0;
                }
                else if(n >= lighttilebatch || (l.shadowmap >= 0) != shadowmap || (l.spot > 0) != spotlight) break;
                setlightparams(n++, l);
                l.addscissor(sx1, sy1, sx2, sy2, sz1, sz2);
            }
            if(baselight) { sx1 = bsx1; sy1 = bsy1; sx2 = bsx2; sy2 = bsy2; sz1 = -1; sz2 = 1; }
            else
            {
                if(!n) break;
                sx1 = max(sx1, bsx1); sy1 = max(sy1, bsy1); sx2 = min(sx2, bsx2); sy2 = min(sy2, bsy2);
                if(sx1 >= sx2 || sy1 >= sy2 || sz1 >= sz2) continue;
            }

            if(n) setlightshader(s, n, baselight, shadowmap, spotlight, false, true);
            else s->setvariant(0, 17);

            if(hasDBT && depthtestlights > 1) glDepthBounds_(sz1*0.5f + 0.5f, min(sz2*0.5f + 0.5f, depthtestlightsclamp));
            lightquad(sz1, sx1, sy1, sx2, sy2, tilemask);
            lightpassesused++;
        }

        setavatarstencil(stencilref, false);
    }
}

void renderlights(float bsx1 = -1, float bsy1 = -1, float bsx2 = 1, float bsy2 = 1, const uint *tilemask = NULL, int stencilmask = 0, int msaapass = 0, bool transparent = false)
{
    Shader *s = drawtex == DRAWTEX_MINIMAP ? deferredminimapshader : (msaapass <= 0 ? deferredlightshader : (msaapass > 1 ? deferredmsaasampleshader : deferredmsaapixelshader));
    if(!s || s == nullshader) return;

    bool depth = true;
    if(!depthtestlights) { glDisable(GL_DEPTH_TEST); depth = false; }
    else glDepthMask(GL_FALSE);

    bindlighttexs(msaapass, transparent);
    setlightglobals(transparent);

    gle::defvertex(3);

    bool avatar = useavatarmask() && !transparent && !drawtex;
    int stencilref = -1;
    if(msaapass == 1 && ghasstencil)
    {
        int tx1 = max(int(floor((bsx1*0.5f+0.5f)*vieww)), 0), ty1 = max(int(floor((bsy1*0.5f+0.5f)*viewh)), 0),
            tx2 = min(int(ceil((bsx2*0.5f+0.5f)*vieww)), vieww), ty2 = min(int(ceil((bsy2*0.5f+0.5f)*viewh)), viewh);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        if(stencilmask) glStencilFunc(GL_EQUAL, stencilmask|0x08, 0x07);
        else
        {
            glStencilFunc(GL_ALWAYS, 0x08, ~0);
            glEnable(GL_STENCIL_TEST);
        }
        if(avatar) glStencilMask(~0x40);
        if(depthtestlights && depth) { glDisable(GL_DEPTH_TEST); depth = false; }
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        SETSHADER(msaaedgedetect);
        lightquad(-1, (tx1*2.0f)/vieww-1.0f, (ty1*2.0f)/viewh-1.0f, (tx2*2.0f)/vieww-1.0f, (ty2*2.0f)/viewh-1.0f, tilemask);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glStencilFunc(GL_EQUAL, stencilref = stencilmask, (avatar ? 0x40 : 0) | (msaalight==2 ? 0x07 : 0x0F));
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        if(avatar) glStencilMask(~0);
        else if(msaalight==2 && !stencilmask) glDisable(GL_STENCIL_TEST);
    }
    else if(msaapass == 2)
    {
        if(ghasstencil) glStencilFunc(GL_EQUAL, stencilref = stencilmask|0x08, avatar ? 0x4F : 0x0F);
        if(msaalight==2) { glSampleMaski_(0, 2); glEnable(GL_SAMPLE_MASK); }
    }
    else if(ghasstencil && (stencilmask || avatar))
    {
        if(!stencilmask) glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, stencilref = stencilmask, avatar ? 0x4F : 0x0F);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }

    if(!avatar) stencilref = -1;

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    if(hasDBT && depthtestlights > 1) glEnable(GL_DEPTH_BOUNDS_TEST_EXT);

    bool sunpass = !lighttilebatch || drawtex == DRAWTEX_MINIMAP || (!getpielight().iszero() && csmshadowmap && batchsunlight <= (gi && getgiscale() && getgidist() ? 1 : 0));
    if(sunpass)
    {
        if(depthtestlights && depth) { glDisable(GL_DEPTH_TEST); depth = false; }
        rendersunpass(s, stencilref, transparent, bsx1, bsy1, bsx2, bsy2, tilemask);
    }

    if(depthtestlights && !depth) { glEnable(GL_DEPTH_TEST); depth = true; }

    if(!lighttilebatch || drawtex == DRAWTEX_MINIMAP)
    {
        renderlightsnobatch(s, stencilref, transparent, bsx1, bsy1, bsx2, bsy2);
    }
    else
    {
        renderlightbatches(s, stencilref, transparent, bsx1, bsy1, bsx2, bsy2, tilemask);
    }

    if(msaapass == 1 && ghasstencil)
    {
        if(msaalight==2 && !stencilmask && !avatar) glEnable(GL_STENCIL_TEST);
    }
    else if(msaapass == 2)
    {
        if(ghasstencil && !stencilmask) glDisable(GL_STENCIL_TEST);
        if(msaalight==2) glDisable(GL_SAMPLE_MASK);
    }
    else if(avatar && !stencilmask) glDisable(GL_STENCIL_TEST);

    glDisable(GL_BLEND);

    if(!depthtestlights) glEnable(GL_DEPTH_TEST);
    else
    {
        glDepthMask(GL_TRUE);
        if(hasDBT && depthtestlights > 1) glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
    }
}

extern int volumetriclights;

void rendervolumetric()
{
    if(!volumetric || !volumetriclights || !getvolscale()) return;

    float bsx1 = 1, bsy1 = 1, bsx2 = -1, bsy2 = -1;
    loopv(lightorder)
    {
        const lightinfo &l = lights[lightorder[i]];
        if(!l.volumetric() || l.checkquery()) continue;

        l.addscissor(bsx1, bsy1, bsx2, bsy2);
    }
    if(bsx1 >= bsx2 || bsy1 >= bsy2) return;

    timer *voltimer = begintimer("Volumetric Lights");

    glBindFramebuffer_(GL_FRAMEBUFFER, volfbo[0]);
    glViewport(0, 0, volw, volh);

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture_(GL_TEXTURE3);
    if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    glActiveTexture_(GL_TEXTURE4);
    glBindTexture(shadowatlastarget, shadowatlastex);
    if(usesmcomparemode()) setsmcomparemode(); else setsmnoncomparemode();
    glActiveTexture_(GL_TEXTURE0);

    GLOBALPARAMF(shadowatlasscale, 1.0f/shadowatlaspacker.w, 1.0f/shadowatlaspacker.h);
    GLOBALPARAMF(volscale, float(vieww)/volw, float(viewh)/volh, float(volw)/vieww, float(volh)/viewh);
    GLOBALPARAMF(volminstep, volminstep);
    GLOBALPARAMF(volprefilter, volprefilter);
    GLOBALPARAMF(voldistclamp, farplane*voldistclamp);

    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_BLEND);

    if(!depthtestlights) glDisable(GL_DEPTH_TEST);
    else glDepthMask(GL_FALSE);

    lightsphere::enable();

    glEnable(GL_SCISSOR_TEST);

    bool outside = true;
    loopv(lightorder)
    {
        const lightinfo &l = lights[lightorder[i]];
        if(!l.volumetric() || l.checkquery()) continue;

        matrix4 lightmatrix = camprojmatrix;
        lightmatrix.translate(l.o);
        lightmatrix.scale(l.radius*lightradiustweak);
        GLOBALPARAM(lightmatrix, lightmatrix);

        if(l.spot > 0)
        {
            volumetricshader->setvariant(0, l.shadowmap >= 0 ? 2 : 1);
            LOCALPARAM(spotparams, vec4(l.dir, 1/(1 - cos360(l.spot))));
        }
        else if(l.shadowmap >= 0) volumetricshader->setvariant(0, 0);
        else volumetricshader->set();

        LOCALPARAM(lightpos, vec4(l.o, 1).div(l.radius));
        vec color = vec(l.color).mul(ldrscaleb).mul(getvolcolour().tocolor().mul(getvolscale()));
        LOCALPARAM(lightcolor, color);

        if(l.shadowmap >= 0)
        {
            shadowmapinfo &sm = shadowmaps[l.shadowmap];
            float smnearclip = SQRT3 / l.radius, smfarclip = SQRT3,
                  bias = (smfilter > 2 ? smbias2 : smbias) * (smcullside ? 1 : -1) * smnearclip * (1024.0f / sm.size);
            int border = smfilter > 2 ? smborder2 : smborder;
            if(l.spot > 0)
            {
                LOCALPARAMF(shadowparams,
                    0.5f * sm.size * cotan360(l.spot),
                    (-smnearclip * smfarclip / (smfarclip - smnearclip) - 0.5f*bias),
                    1 / (1 + fabs(l.dir.z)),
                    0.5f + 0.5f * (smfarclip + smnearclip) / (smfarclip - smnearclip));
            }
            else
            {
                LOCALPARAMF(shadowparams,
                    0.5f * (sm.size - border),
                    -smnearclip * smfarclip / (smfarclip - smnearclip) - 0.5f*bias,
                    sm.size,
                    0.5f + 0.5f * (smfarclip + smnearclip) / (smfarclip - smnearclip));
            }
            LOCALPARAMF(shadowoffset, sm.x + 0.5f*sm.size, sm.y + 0.5f*sm.size);
        }

        int tx1 = int(floor((l.sx1*0.5f+0.5f)*volw)), ty1 = int(floor((l.sy1*0.5f+0.5f)*volh)),
            tx2 = int(ceil((l.sx2*0.5f+0.5f)*volw)), ty2 = int(ceil((l.sy2*0.5f+0.5f)*volh));
        glScissor(tx1, ty1, tx2-tx1, ty2-ty1);

        if(camera1->o.dist(l.o) <= l.radius*lightradiustweak + nearplane + 1 && depthfaillights)
        {
            if(outside)
            {
                outside = false;
                if(depthtestlights) glDisable(GL_DEPTH_TEST);
                glCullFace(GL_FRONT);
            }
        }
        else if(!outside)
        {
            outside = true;
            if(depthtestlights) glEnable(GL_DEPTH_TEST);
            glCullFace(GL_BACK);
        }

        lightsphere::draw();
    }

    if(!outside)
    {
        outside = true;
        glCullFace(GL_BACK);
    }

    lightsphere::disable();

    if(depthtestlights)
    {
        glDepthMask(GL_TRUE);

        glDisable(GL_DEPTH_TEST);
    }

    int cx1 = int(floor((bsx1*0.5f+0.5f)*volw))&~1,
        cy1 = int(floor((bsy1*0.5f+0.5f)*volh))&~1,
        cx2 = (int(ceil((bsx2*0.5f+0.5f)*volw))&~1) + 2,
        cy2 = (int(ceil((bsy2*0.5f+0.5f)*volh))&~1) + 2;
    if(volbilateral || volblur)
    {
        int radius = (volbilateral ? volbilateral : volblur)*2;
        cx1 = max(cx1 - radius, 0);
        cy1 = max(cy1 - radius, 0);
        cx2 = min(cx2 + radius, volw);
        cy2 = min(cy2 + radius, volh);
        glScissor(cx1, cy1, cx2-cx1, cy2-cy1);

        glDisable(GL_BLEND);

        if(volbilateral) loopi(2)
        {
            glBindFramebuffer_(GL_FRAMEBUFFER, volfbo[(i+1)%2]);
            glViewport(0, 0, volw, volh);
            volumetricbilateralshader[i]->set();
            setbilateralparams(volbilateral, volbilateraldepth);
            glBindTexture(GL_TEXTURE_RECTANGLE, voltex[i%2]);
            screenquadoffset(0.25f, 0.25f, vieww, viewh);
        }
        else
        {
            float blurweights[MAXBLURRADIUS+1], bluroffsets[MAXBLURRADIUS+1];
            setupblurkernel(volblur, blurweights, bluroffsets);
            loopi(2)
            {
                glBindFramebuffer_(GL_FRAMEBUFFER, volfbo[(i+1)%2]);
                glViewport(0, 0, volw, volh);
                setblurshader(i%2, 1, volblur, blurweights, bluroffsets, GL_TEXTURE_RECTANGLE);
                glBindTexture(GL_TEXTURE_RECTANGLE, voltex[i%2]);
                screenquad(volw, volh);
            }
        }

        glEnable(GL_BLEND);
    }

    glBindFramebuffer_(GL_FRAMEBUFFER, msaalight ? mshdrfbo : hdrfbo);
    glViewport(0, 0, vieww, viewh);

    int margin = (1<<volreduce) - 1;
    cx1 = max((cx1 * vieww) / volw - margin, 0);
    cy1 = max((cy1 * viewh) / volh - margin, 0);
    cx2 = min((cx2 * vieww + margin + volw - 1) / volw, vieww);
    cy2 = min((cy2 * viewh + margin + volh - 1) / volh, viewh);
    glScissor(cx1, cy1, cx2-cx1, cy2-cy1);

    bool avatar = useavatarmask();
    if(avatar)
    {
        glStencilFunc(GL_EQUAL, 0, 0x40);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glEnable(GL_STENCIL_TEST);
    }

    SETSHADER(scalelinear);
    glBindTexture(GL_TEXTURE_RECTANGLE, voltex[0]);
    screenquad(volw, volh);

    if(volbilateral || volblur)
    {
        swap(volfbo[0], volfbo[1]);
        swap(voltex[0], voltex[1]);
    }

    if(avatar) glDisable(GL_STENCIL_TEST);

    glDisable(GL_SCISSOR_TEST);

    glEnable(GL_DEPTH_TEST);

    glDisable(GL_BLEND);

    endtimer(voltimer);
}

VAR(0, oqvol, 0, 1, 1);
VAR(0, oqlights, 0, 1, 1);
VAR(0, debuglightscissor, 0, 0, 1);

void viewlightscissor()
{
    vector<extentity *> &ents = entities::getents();
    gle::defvertex(2);
    loopv(entgroup)
    {
        int idx = entgroup[i];
        if(ents.inrange(idx) && ents[idx]->type == ET_LIGHT)
        {
            extentity &e = *ents[idx];
            loopvj(lights) if(lights[j].o == e.o)
            {
                lightinfo &l = lights[j];
                if(!l.validscissor()) break;
                gle::colorf(l.color.x/255, l.color.y/255, l.color.z/255);
                float x1 = (l.sx1+1)/2*hudw, x2 = (l.sx2+1)/2*hudw,
                      y1 = (1-l.sy1)/2*hudh, y2 = (1-l.sy2)/2*hudh;
                gle::begin(GL_TRIANGLE_STRIP);
                gle::attribf(x1, y1);
                gle::attribf(x2, y1);
                gle::attribf(x1, y2);
                gle::attribf(x2, y2);
                gle::end();
            }
        }
    }
}

void collectlights()
{
    if(lights.length()) return;

    // point lights processed here
    const vector<extentity *> &ents = entities::getents();
    if(!editmode || !fullbright) loopv(ents)
    {
        const extentity &e = *ents[i];
        if(e.type != ET_LIGHT) continue;

        int radius = e.attrs[0], spotlight = -1;
        vec color(255, 255, 255);
        if(!getlightfx(e, &radius, &spotlight, &color, false)) continue;
        vec dir(0, 0, 0);
        int spot = 0;
        if(ents.inrange(spotlight))
        {
            const extentity &f = *ents[spotlight];
            dir = vec(f.o).sub(e.o).normalize();
            spot = clamp(int(f.attrs[1]), 1, 89);
        }

        if(smviscull)
        {
            if(isfoggedsphere(radius, e.o)) continue;
            if(pvsoccludedsphere(e.o, radius)) continue;
        }

        lightinfo &l = lights.add(lightinfo(i, e.o, color, float(radius), e.attrs[6], dir, spot));
        if(l.validscissor()) lightorder.add(lights.length()-1);
    }

    int numdynlights = 0;
    if(!drawtex || drawtex == DRAWTEX_MAPSHOT)
    {
        updatedynlights();
        numdynlights = finddynlights();
    }
    loopi(numdynlights)
    {
        vec o, color, dir;
        float radius;
        int spot, flags;
        if(!getdynlight(i, o, radius, color, dir, spot, flags)) continue;

        lightinfo &l = lights.add(lightinfo(o, vec(color).mul(255).max(0), radius, flags, dir, spot));
        if(l.validscissor()) lightorder.add(lights.length()-1);
    }

    lightorder.sort(sortlights);

    bool queried = false;
    if((!drawtex || drawtex == DRAWTEX_MAPSHOT) && smquery && oqfrags && oqlights) loopv(lightorder)
    {
        int idx = lightorder[i];
        lightinfo &l = lights[idx];
        if((l.noshadow() && (!oqvol || !l.volumetric())) || l.radius >= worldsize) continue;
        vec bbmin, bbmax;
        l.calcbb(bbmin, bbmax);
        if(!camera1->o.insidebb(bbmin, bbmax, 2))
        {
            l.query = newquery(&l);
            if(l.query)
            {
                if(!queried)
                {
                    startbb(false);
                    queried = true;
                }
                startquery(l.query);
                ivec bo(bbmin), br = ivec(bbmax).sub(bo).add(1);
                drawbb(bo, br);
                endquery(l.query);
            }
        }
    }
    if(queried)
    {
        endbb(false);
        glFlush();
    }

    smused = 0;

    if(smcache && !smnoshadow && shadowcache.numelems) loop(mismatched, 2) loopv(lightorder)
    {
        int idx = lightorder[i];
        lightinfo &l = lights[idx];
        if(l.noshadow()) continue;

        shadowcacheval *cached = shadowcache.access(l);
        if(!cached) continue;

        float prec = smprec, lod;
        int w, h;
        if(l.spot) { w = 1; h = 1; prec *= tan360(l.spot); lod = smspotprec; }
        else { w = 3; h = 2; lod = smcubeprec; }
        lod *= clamp(l.radius * prec / sqrtf(max(1.0f, l.dist/l.radius)), float(smminsize), float(smmaxsize));
        int size = clamp(int(ceil((lod * shadowatlaspacker.w) / SHADOWATLAS_SIZE)), 1, shadowatlaspacker.w / w);
        w *= size;
        h *= size;

        if(mismatched)
        {
            if(cached->size == size) continue;

            ushort x = USHRT_MAX, y = USHRT_MAX;
            if(!shadowatlaspacker.insert(x, y, w, h)) continue;
            addshadowmap(x, y, size, l.shadowmap, idx);
        }
        else
        {
            if(cached->size != size) continue;

            ushort x = cached->x, y = cached->y;
            shadowatlaspacker.reserve(x, y, w, h);
            addshadowmap(x, y, size, l.shadowmap, idx, cached);
        }

        smused += w*h;
    }
}

static bool inoq = false;
VAR(0, csminoq, 0, 1, 1);
VAR(0, sminoq, 0, 1, 1);
VAR(0, rhinoq, 0, 1, 1);

static inline bool shouldworkinoq()
{
    return !drawtex && oqfrags && (!wireframe || !editmode);
}

struct batchrect : lightrect
{
    uchar group;
    ushort idx;

    batchrect() {}
    batchrect(const lightinfo &l, ushort idx)
      : lightrect(l),
        group((l.shadowmap < 0 ? BF_NOSHADOW : 0) | (l.spot > 0 ? BF_SPOTLIGHT : 0)),
        idx(idx)
    {}
};

struct batchstack : lightrect
{
    ushort offset, numrects;
    uchar flags;

    batchstack() {}
    batchstack(uchar x1, uchar y1, uchar x2, uchar y2, ushort offset, ushort numrects, uchar flags = 0) : lightrect(x1, y1, x2, y2), offset(offset), numrects(numrects), flags(flags) {}
};

static vector<batchrect> batchrects;

static void batchlights(const batchstack &initstack)
{
    batchstack stack[32];
    size_t numstack = 1;
    stack[0] = initstack;

    while(numstack > 0)
    {
        batchstack s = stack[--numstack];
        if(numstack + 5 > sizeof(stack)/sizeof(stack[0])) { batchlights(s); continue; }

        ++lightbatchstacksused;
        int groups[BF_NOSUN] = { 0 };
        lightrect split(s);
        ushort splitidx = USHRT_MAX;
        int outside = s.offset, inside = s.offset + s.numrects;
        for(int i = outside; i < inside; ++i)
        {
            const batchrect &r = batchrects[i];
            if(r.outside(s))
            {
                if(i != outside) swap(batchrects[i], batchrects[outside]);
                ++outside;
            }
            else if(s.inside(r))
            {
                ++groups[r.group];
                swap(batchrects[i--], batchrects[--inside]);
            }
            else if(r.idx < splitidx) { split = r; splitidx = r.idx; }
        }

        uchar flags = s.flags;
        int batched = s.offset + s.numrects;
        loop(g, BF_NOSUN) while(groups[g] >= lighttilebatch || (inside == outside && (groups[g] || !(flags & BF_NOSUN))))
        {
            lightbatchkey key;
            key.flags = flags | g;
            flags |= BF_NOSUN;

            int n = min(groups[g], lighttilebatch);
            groups[g] -= n;
            key.numlights = n;
            loopi(n)
            {
                int best = -1;
                ushort bestidx = USHRT_MAX;
                for(int j = inside; j < batched; ++j) { const batchrect &r = batchrects[j]; if(r.group == g && r.idx < bestidx) { best = j; bestidx = r.idx; } }
                key.lights[i] = lightorder[bestidx];
                swap(batchrects[best], batchrects[--batched]);
            }

            lightbatch &batch = lightbatcher[key];
            if(batch.rects.empty())
            {
                (lightbatchkey &)batch = key;
                lightbatches.add(&batch);
            }
            batch.rects.add(s);
            ++lightbatchrectsused;
        }

        if(splitidx != USHRT_MAX)
        {
            int numoverlap = batched - outside;
            split.intersect(s);

            if(split.y1 > s.y1) stack[numstack++] = batchstack(s.x1, s.y1, s.x2, split.y1, outside, numoverlap, flags);

            if(split.x1 > s.x1) stack[numstack++] = batchstack(s.x1, split.y1, split.x1, split.y2, outside, numoverlap, flags);
            stack[numstack++] = batchstack(split.x1, split.y1, split.x2, split.y2, outside, numoverlap, flags);
            if(split.x2 < s.x2) stack[numstack++] = batchstack(split.x2, split.y1, s.x2, split.y2, outside, numoverlap, flags);

            if(split.y2 < s.y2) stack[numstack++] = batchstack(s.x1, split.y2, s.x2, s.y2, outside, numoverlap, flags);
        }
    }
}

static inline bool sortlightbatches(const lightbatch *x, const lightbatch *y)
{
    if(x->flags < y->flags) return true;
    if(x->flags > y->flags) return false;
    return x->numlights > y->numlights;
}

static void batchlights()
{
    lightbatches.setsize(0);
    lightbatchstacksused = 0;
    lightbatchrectsused = 0;

    if(lighttilebatch && drawtex != DRAWTEX_MINIMAP)
    {
        lightbatcher.recycle();
        batchlights(batchstack(0, 0, lighttilew, lighttileh, 0, batchrects.length()));
        lightbatches.sort(sortlightbatches);
    }

    lightbatchesused = lightbatches.length();
}

void packlights()
{
    lightsvisible = lightsoccluded = 0;
    lightpassesused = 0;
    batchrects.setsize(0);

    loopv(lightorder)
    {
        int idx = lightorder[i];
        lightinfo &l = lights[idx];
        if(l.checkquery())
        {
            if(l.shadowmap >= 0)
            {
                shadowmaps[l.shadowmap].light = -1;
                l.shadowmap = -1;
            }
            lightsoccluded++;
            continue;
        }

        if(!l.noshadow() && !smnoshadow && l.shadowmap < 0)
        {
            float prec = smprec, lod;
            int w, h;
            if(l.spot) { w = 1; h = 1; prec *= tan360(l.spot); lod = smspotprec; }
            else { w = 3; h = 2; lod = smcubeprec; }
            lod *= clamp(l.radius * prec / sqrtf(max(1.0f, l.dist/l.radius)), float(smminsize), float(smmaxsize));
            int size = clamp(int(ceil((lod * shadowatlaspacker.w) / SHADOWATLAS_SIZE)), 1, shadowatlaspacker.w / w);
            w *= size;
            h *= size;
            ushort x = USHRT_MAX, y = USHRT_MAX;
            if(shadowatlaspacker.insert(x, y, w, h))
            {
                addshadowmap(x, y, size, l.shadowmap, idx);
                smused += w*h;
            }
            else if(smcache) shadowcachefull = true;
        }

        batchrects.add(batchrect(l, i));
    }

    lightsvisible = lightorder.length() - lightsoccluded;

    batchlights();
}

static inline void nogiquad(int x, int y, int w, int h)
{
    gle::attribf(x, y+h);
    gle::attribf(x+w, y+h);
    gle::attribf(x+w, y);
    gle::attribf(x, y);
}

static inline bool rendernogi(cube *c, const ivec &o, int size, const ivec &bbmin, const ivec &bbmax, int minsize)
{
    ivec mid = ivec(o).add(size);
    uchar overlap = 0;
    if(bbmin.y < mid.y)
    {
        if(bbmin.x < mid.x)
        {
            if((bbmin.z < mid.z && (c[0].children ? rendernogi(c[0].children, ivec(o.x, o.y, o.z), size>>1, bbmin, bbmax, minsize) : c[0].material&MAT_NOGI)) ||
               (bbmax.z > mid.z && (c[4].children ? rendernogi(c[4].children, ivec(o.x, o.y, mid.z), size>>1, bbmin, bbmax, minsize) : c[4].material&MAT_NOGI)))
                overlap |= 1;
        }
        if(bbmax.x > mid.x)
        {
            if((bbmin.z < mid.z && (c[1].children ? rendernogi(c[1].children, ivec(mid.x, o.y, o.z), size>>1, bbmin, bbmax, minsize) : c[1].material&MAT_NOGI)) ||
               (bbmax.z > mid.z && (c[5].children ? rendernogi(c[5].children, ivec(mid.x, o.y, mid.z), size>>1, bbmin, bbmax, minsize) : c[5].material&MAT_NOGI)))
                overlap |= 2;
        }
    }
    if(bbmax.y > mid.y)
    {
        if(bbmin.x < mid.x)
        {
            if((bbmin.z < mid.z && (c[2].children ? rendernogi(c[2].children, ivec(o.x, mid.y, o.z), size>>1, bbmin, bbmax, minsize) : c[2].material&MAT_NOGI)) ||
               (bbmax.z > mid.z && (c[6].children ? rendernogi(c[6].children, ivec(o.x, mid.y, mid.z), size>>1, bbmin, bbmax, minsize) : c[6].material&MAT_NOGI)))
                overlap |= 4;
        }
        if(bbmax.x > mid.x)
        {
            if((bbmin.z < mid.z && (c[3].children ? rendernogi(c[3].children, ivec(mid.x, mid.y, o.z), size>>1, bbmin, bbmax, minsize) : c[3].material&MAT_NOGI)) ||
               (bbmax.z > mid.z && (c[7].children ? rendernogi(c[7].children, ivec(mid.x, mid.y, mid.z), size>>1, bbmin, bbmax, minsize) : c[7].material&MAT_NOGI)))
                overlap |= 8;
        }
    }
    if(!overlap) return false;
    if(overlap == 0xF || size <= minsize) return true;
    if(overlap&1)
    {
        if(overlap&2) nogiquad(o.x, o.y, 2*size, size);
        else nogiquad(o.x, o.y, size, size);
    }
    else if(overlap&2) nogiquad(o.x+size, o.y, size, size);
    if(overlap&4)
    {
        if(overlap&8) nogiquad(o.x, o.y+size, 2*size, size);
        else nogiquad(o.x, o.y+size, size, size);
    }
    else if(overlap&8) nogiquad(o.x+size, o.y+size, size, size);
    return false;
}

static inline void rendernogi(const ivec &bbmin, const ivec &bbmax, int minsize)
{
    if(rendernogi(worldroot, ivec(0, 0, 0), worldsize>>1, ivec(bbmin).max(nogimin), ivec(bbmax).min(nogimax), minsize))
        nogiquad(0, 0, worldsize, worldsize);
}

static inline void rhquad(float x1, float y1, float x2, float y2, float tx1, float ty1, float tx2, float ty2, float tz)
{
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(x2, y1); gle::attribf(tx2, ty1, tz);
    gle::attribf(x1, y1); gle::attribf(tx1, ty1, tz);
    gle::attribf(x2, y2); gle::attribf(tx2, ty2, tz);
    gle::attribf(x1, y2); gle::attribf(tx1, ty2, tz);
    gle::end();
}

static inline void rhquad(float dx1, float dy1, float dx2, float dy2, float dtx1, float dty1, float dtx2, float dty2, float dtz,
                          float px1, float py1, float px2, float py2, float ptx1, float pty1, float ptx2, float pty2, float ptz)
{
    gle::begin(GL_TRIANGLE_STRIP);
    gle::attribf(dx2, dy1); gle::attribf(dtx2, dty1, dtz);
        gle::attribf(px2, py1); gle::attribf(ptx2, pty1, ptz);
    gle::attribf(dx1, dy1); gle::attribf(dtx1, dty1, dtz);
        gle::attribf(px1, py1); gle::attribf(ptx1, pty1, ptz);
    gle::attribf(dx1, dy2); gle::attribf(dtx1, dty2, dtz);
        gle::attribf(px1, py2); gle::attribf(ptx1, pty2, ptz);
    gle::attribf(dx2, dy2); gle::attribf(dtx2, dty2, dtz);
        gle::attribf(px2, py2); gle::attribf(ptx2, pty2, ptz);
    gle::attribf(dx2, dy1); gle::attribf(dtx2, dty1, dtz);
        gle::attribf(px2, py1); gle::attribf(ptx2, pty1, ptz);
    gle::end();
}

void radiancehints::renderslices()
{
    int sw = rhgrid+2*rhborder, sh = rhgrid+2*rhborder;
    glBindFramebuffer_(GL_FRAMEBUFFER, rhfbo);
    if(!rhrect)
    {
        glViewport(0, 0, sw, sh);
        if(rhcache)
        {
            loopi(4) swap(rhtex[i], rhtex[i+4]);
            uint clearmasks[RH_MAXSPLITS][(RH_MAXGRID+2+31)/32];
            memcpy(clearmasks, rhclearmasks[0], sizeof(clearmasks));
            memcpy(rhclearmasks[0], rhclearmasks[1], sizeof(clearmasks));
            memcpy(rhclearmasks[1], clearmasks, sizeof(clearmasks));
        }
    }

    GLOBALPARAMF(rhatten, 1.0f/(getgidist()*getgidist()));
    GLOBALPARAMF(rsmspread, getgidist()*rsmspread*rsm.scale.x, getgidist()*rsmspread*rsm.scale.y);
    GLOBALPARAMF(rhaothreshold, splits[0].bounds/rhgrid);
    GLOBALPARAMF(rhaoatten, 1.0f/(getgidist()*rsmspread));
    GLOBALPARAMF(rhaoheight, getgidist()*rsmspread);

    matrix4 rsmtcmatrix;
    rsmtcmatrix.identity();
    rsmtcmatrix.settranslation(rsm.offset);
    rsmtcmatrix.setscale(rsm.scale);
    rsmtcmatrix.mul(rsm.model);
    GLOBALPARAM(rsmtcmatrix, rsmtcmatrix);

    matrix4 rsmworldmatrix;
    rsmworldmatrix.invert(rsmtcmatrix);
    GLOBALPARAM(rsmworldmatrix, rsmworldmatrix);

    glBindTexture(GL_TEXTURE_RECTANGLE, rsmdepthtex);
    glActiveTexture_(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, rsmcolortex);
    glActiveTexture_(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_RECTANGLE, rsmnormaltex);
    if(rhborder) loopi(4)
    {
        glActiveTexture_(GL_TEXTURE3 + i);
        glBindTexture(GL_TEXTURE_3D, rhtex[i]);
    }
    if(rhcache) loopi(4)
    {
        glActiveTexture_(GL_TEXTURE7 + i);
        glBindTexture(GL_TEXTURE_3D, rhtex[rhrect ? i : 4+i]);
    }
    glActiveTexture_(GL_TEXTURE0);

    glClearColor(0.5f, 0.5f, 0.5f, 0);
    if(rhrect) glEnable(GL_SCISSOR_TEST);

    gle::defvertex(2);
    gle::deftexcoord0(3);

    bool prevcached = true;
    int cx = -1, cy = -1;
    loopirev(rhsplits)
    {
        splitinfo &split = splits[i];
        float cellradius = split.bounds/rhgrid, step = 2*cellradius, nudge = rhnudge*2*splits[0].bounds/rhgrid + rhworldbias*step;
        vec cmin, cmax, dmin(1e16f, 1e16f, 1e16f), dmax(-1e16f, -1e16f, -1e16f), bmin(1e16f, 1e16f, 1e16f), bmax(-1e16f, -1e16f, -1e16f);
        loopk(3)
        {
            cmin[k] = floor((worldmin[k] - nudge - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds;
            cmax[k] = ceil((worldmax[k] + nudge - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds;
        }
        if(prevdynmin.z < prevdynmax.z) loopk(3)
        {
            dmin[k] = min(dmin[k], (float)floor((prevdynmin[k] - getgidist() - cellradius - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds);
            dmax[k] = max(dmax[k], (float)ceil((prevdynmax[k] + getgidist() + cellradius - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds);
        }
        if(dynmin.z < dynmax.z) loopk(3)
        {
            dmin[k] = min(dmin[k], (float)floor((dynmin[k] - getgidist() - cellradius - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds);
            dmax[k] = max(dmax[k], (float)ceil((dynmax[k] + getgidist() + cellradius - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds);
        }

        if((rhrect || !rhcache || hasCI) && split.cached == split.center && (!rhborder || prevcached) && !rhforce &&
           (dmin.x > split.center.x + split.bounds || dmax.x < split.center.x - split.bounds ||
            dmin.y > split.center.y + split.bounds || dmax.y < split.center.y - split.bounds ||
            dmin.z > split.center.z + split.bounds || dmax.z < split.center.z - split.bounds))
        {
            if(rhrect || !rhcache || split.copied) continue;
            split.copied = true;
            loopk(4) glCopyImageSubData_(rhtex[4+k], GL_TEXTURE_3D, 0, 0, 0, i*sh, rhtex[k], GL_TEXTURE_3D, 0, 0, 0, i*sh, sw, sh, sh);
            continue;
        }

        prevcached = false;
        split.copied = false;

        GLOBALPARAM(rhcenter, split.center);
        GLOBALPARAMF(rhbounds, split.bounds);
        GLOBALPARAMF(rhspread, cellradius);

        if(rhborder && i + 1 < rhsplits)
        {
            GLOBALPARAMF(bordercenter, 0.5f, 0.5f, float(i+1 + 0.5f)/rhsplits);
            GLOBALPARAMF(borderrange, 0.5f - 0.5f/(rhgrid+2), 0.5f - 0.5f/(rhgrid+2), (0.5f - 0.5f/(rhgrid+2))/rhsplits);
            GLOBALPARAMF(borderscale, rhgrid+2, rhgrid+2, (rhgrid+2)*rhsplits);

            splitinfo &next = splits[i+1];
            loopk(3)
            {
                bmin[k] = floor((max(float(worldmin[k] - nudge), next.center[k] - next.bounds) - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds;
                bmax[k] = ceil((min(float(worldmax[k] + nudge), next.center[k] + next.bounds) - (split.center[k] - split.bounds))/step)*step + split.center[k] - split.bounds;
            }
        }

        uint clearmasks[(RH_MAXGRID+2+31)/32];
        memset(clearmasks, 0xFF, sizeof(clearmasks));

        int sy = rhrect ? i*sh : 0;
        loopjrev(sh)
        {
            int sx = rhrect ? j*sw : 0;

            #define BIND_SLICE do { \
                if(rhrect) \
                { \
                    glViewport(sx, sy, sw, sh); \
                    glScissor(sx, sy, sw, sh); \
                } \
                else \
                { \
                    glFramebufferTexture3D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, rhtex[0], 0, i*sh + j); \
                    glFramebufferTexture3D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_3D, rhtex[1], 0, i*sh + j); \
                    glFramebufferTexture3D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_3D, rhtex[2], 0, i*sh + j); \
                    glFramebufferTexture3D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_3D, rhtex[3], 0, i*sh + j); \
                } \
            } while(0)

            float x1 = split.center.x - split.bounds, x2 = split.center.x + split.bounds,
                  y1 = split.center.y - split.bounds, y2 = split.center.y + split.bounds,
                  z = split.center.z - split.bounds + (j-rhborder+0.5f)*step,
                  vx1 = -1 + rhborder*2.0f/(rhgrid+2), vx2 = 1 - rhborder*2.0f/(rhgrid+2), vy1 = -1 + rhborder*2.0f/(rhgrid+2), vy2 = 1 - rhborder*2.0f/(rhgrid+2),
                  tx1 = x1, tx2 = x2, ty1 = y1, ty2 = y2;
            bool clipped = false;

            if(rhborder && i + 1 < rhsplits)
            {
                splitinfo &next = splits[i+1];
                float bx1 = x1-step, bx2 = x2+step, by1 = y1-step, by2 = y2+step, bz = z,
                      bvx1 = -1, bvx2 = 1, bvy1 = -1, bvy2 = 1,
                      btx1 = bx1, btx2 = bx2, bty1 = by1, bty2 = by2;

                if(rhclipgrid)
                {
                    if(bz < bmin.z || bz > bmax.z) goto noborder;
                    if(bx1 < bmin.x || bx2 > bmax.x || by1 < bmin.y || by2 > bmax.y)
                    {
                        btx1 = max(bx1, bmin.x);
                        btx2 = min(bx2, bmax.x);
                        bty1 = max(by1, bmin.y);
                        bty2 = min(by2, bmax.y);
                        if(btx1 > tx2 || bty1 > bty2) goto noborder;
                        bvx1 += 2*(btx1 - bx1)/(bx2 - bx1);
                        bvx2 += 2*(btx2 - bx2)/(bx2 - bx1);
                        bvy1 += 2*(bty1 - by1)/(by2 - by1);
                        bvy2 += 2*(bty2 - by2)/(by2 - by1);
                        clipped = true;
                    }
                }

                btx1 = btx1*next.scale.x + next.offset.x;
                btx2 = btx2*next.scale.x + next.offset.x;
                bty1 = bty1*next.scale.y + next.offset.y;
                bty2 = bty2*next.scale.y + next.offset.y;
                bz = bz*next.scale.z + next.offset.z;

                BIND_SLICE;
                if(clipped) glClear(GL_COLOR_BUFFER_BIT);

                SETSHADER(radiancehintsborder);
                rhquad(bvx1, bvy1, bvx2, bvy2, btx1, bty1, btx2, bty2, bz);

                clearmasks[j/32] &= ~(1 << (j%32));
            }

        noborder:
            if(j < rhborder || j >= rhgrid + rhborder)
            {
            skipped:
                if(clearmasks[j/32] & (1 << (j%32)) && (!rhrect || cx < 0) && !(rhclearmasks[0][i][j/32] & (1 << (j%32))))
                {
                    BIND_SLICE;
                    glClear(GL_COLOR_BUFFER_BIT);
                    cx = sx;
                    cy = sy;
                }
                continue;
            }

            if(rhclipgrid)
            {
                if(z < cmin.z || z > cmax.z) goto skipped;
                if(x1 < cmin.x || x2 > cmax.x || y1 < cmin.y || y2 > cmax.y)
                {
                    tx1 = max(x1, cmin.x);
                    tx2 = min(x2, cmax.x);
                    ty1 = max(y1, cmin.y);
                    ty2 = min(y2, cmax.y);
                    if(tx1 > tx2 || ty1 > ty2) goto skipped;
                    vx1 += 2*rhgrid/float(sw)*(tx1 - x1)/(x2 - x1);
                    vx2 += 2*rhgrid/float(sw)*(tx2 - x2)/(x2 - x1);
                    vy1 += 2*rhgrid/float(sh)*(ty1 - y1)/(y2 - y1);
                    vy2 += 2*rhgrid/float(sh)*(ty2 - y2)/(y2 - y1);
                    clipped = true;
                }
            }

            if(clearmasks[j/32] & (1 << (j%32)))
            {
                BIND_SLICE;
                if(clipped || (rhborder && i + 1 >= rhsplits)) glClear(GL_COLOR_BUFFER_BIT);
                clearmasks[j/32] &= ~(1 << (j%32));
            }

            if(rhcache && z > split.cached.z - split.bounds && z < split.cached.z + split.bounds)
            {
                float px1 = max(tx1, split.cached.x - split.bounds), px2 = min(tx2, split.cached.x + split.bounds),
                      py1 = max(ty1, split.cached.y - split.bounds), py2 = min(ty2, split.cached.y + split.bounds);
                if(px1 < px2 && py1 < py2)
                {
                    float pvx1 = -1 + rhborder*2.0f/(rhgrid+2) + 2*rhgrid/float(sw)*(px1 - x1)/(x2 - x1),
                          pvx2 = 1 - rhborder*2.0f/(rhgrid+2) + 2*rhgrid/float(sw)*(px2 - x2)/(x2 - x1),
                          pvy1 = -1 + rhborder*2.0f/(rhgrid+2) + 2*rhgrid/float(sh)*(py1 - y1)/(y2 - y1),
                          pvy2 = 1 - rhborder*2.0f/(rhgrid+2) + 2*rhgrid/float(sh)*(py2 - y2)/(y2 - y1),
                          ptx1 = (px1 + split.center.x - split.cached.x)*split.scale.x + split.offset.x,
                          ptx2 = (px2 + split.center.x - split.cached.x)*split.scale.x + split.offset.x,
                          pty1 = (py1 + split.center.y - split.cached.y)*split.scale.y + split.offset.y,
                          pty2 = (py2 + split.center.y - split.cached.y)*split.scale.y + split.offset.y,
                          pz = (z + split.center.z - split.cached.z)*split.scale.z + split.offset.z;

                    if(px1 != tx1 || px2 != tx2 || py1 != ty1 || py2 != ty2)
                    {
                        radiancehintsshader->set();
                        rhquad(pvx1, pvy1, pvx2, pvy2, px1, py1, px2, py2, z,
                                vx1,  vy1,  vx2,  vy2, tx1, ty1, tx2, ty2, z);
                    }

                    if(z > dmin.z && z < dmax.z)
                    {
                        float dx1 = max(px1, dmin.x), dx2 = min(px2, dmax.x),
                              dy1 = max(py1, dmin.y), dy2 = min(py2, dmax.y);
                        if(dx1 < dx2 && dy1 < dy2)
                        {
                            float dvx1 = -1 + rhborder*2.0f/(rhgrid+2) + 2*rhgrid/float(sw)*(dx1 - x1)/(x2 - x1),
                                  dvx2 = 1 - rhborder*2.0f/(rhgrid+2) + 2*rhgrid/float(sw)*(dx2 - x2)/(x2 - x1),
                                  dvy1 = -1 + rhborder*2.0f/(rhgrid+2) + 2*rhgrid/float(sh)*(dy1 - y1)/(y2 - y1),
                                  dvy2 = 1 - rhborder*2.0f/(rhgrid+2) + 2*rhgrid/float(sh)*(dy2 - y2)/(y2 - y1),
                                  dtx1 = (dx1 + split.center.x - split.cached.x)*split.scale.x + split.offset.x,
                                  dtx2 = (dx2 + split.center.x - split.cached.x)*split.scale.x + split.offset.x,
                                  dty1 = (dy1 + split.center.y - split.cached.y)*split.scale.y + split.offset.y,
                                  dty2 = (dy2 + split.center.y - split.cached.y)*split.scale.y + split.offset.y,
                                  dz = (z + split.center.z - split.cached.z)*split.scale.z + split.offset.z;

                            if(dx1 != px1 || dx2 != px2 || dy1 != py1 || dy2 != py2)
                            {
                                SETSHADER(radiancehintscached);
                                rhquad(dvx1, dvy1, dvx2, dvy2, dtx1, dty1, dtx2, dty2, dz,
                                       pvx1, pvy1, pvx2, pvy2, ptx1, pty1, ptx2, pty2, pz);
                            }

                            radiancehintsshader->set();
                            rhquad(dvx1, dvy1, dvx2, dvy2, dx1, dy1, dx2, dy2, z);
                            goto maskslice;
                        }
                    }

                    SETSHADER(radiancehintscached);
                    rhquad(pvx1, pvy1, pvx2, pvy2, ptx1, pty1, ptx2, pty2, pz);
                    goto maskslice;
                }
            }

            radiancehintsshader->set();
            rhquad(vx1, vy1, vx2, vy2, tx1, ty1, tx2, ty2, z);

        maskslice:
            if(i) continue;
            rendernogi(ivec::floor(vec(x1, y1, z - 0.5f*step)), ivec::ceil(vec(x2, y2, z + 0.5f*step)), int(step));
            if(gle::attribbuf.empty()) continue;
            SETSHADER(radiancehintsdisable);
            if(rhborder)
            {
                glScissor(sx + rhborder, sy + rhborder, sw - 2*rhborder, sh - 2*rhborder);
                if(!rhrect) glEnable(GL_SCISSOR_TEST);
            }
            gle::defvertex(2);
            gle::begin(GL_QUADS);
            gle::end();
            if(rhborder && !rhrect) glDisable(GL_SCISSOR_TEST);
            gle::defvertex(2);
            gle::deftexcoord0(3);
        }
        if(rhrect) loopk(4)
        {
            glReadBuffer(GL_COLOR_ATTACHMENT0+k);
            glBindTexture(GL_TEXTURE_3D, rhtex[k]);
            loopj(sh)
            {
                if(clearmasks[j/32] & (1 << (j%32)))
                {
                    if(!(rhclearmasks[0][i][j/32] & (1 << (j%32)))) glCopyTexSubImage3D_(GL_TEXTURE_3D, 0, 0, 0, sy+j, cx, cy, sw, sh);
                    continue;
                }
                glCopyTexSubImage3D_(GL_TEXTURE_3D, 0, 0, 0, sy+j, j*sw, sy, sw, sh);
            }
        }
        memcpy(rhclearmasks[0][i], clearmasks, sizeof(clearmasks));
    }

    if(rhrect) glDisable(GL_SCISSOR_TEST);
}

void renderradiancehints()
{
    if(rhinoq && !inoq && shouldworkinoq()) return;
    if(!useradiancehints()) return;

    timer *rhcputimer = begintimer("Radiance Hints", false);
    timer *rhtimer = begintimer("Radiance Hints");

    rh.setup();
    rsm.setup();

    shadowmapping = SM_REFLECT;
    shadowside = 0;
    shadoworigin = vec(0, 0, 0);
    shadowdir = rsm.lightview;
    shadowbias = rsm.lightview.project_bb(worldmin, worldmax);
    shadowradius = fabs(rsm.lightview.project_bb(worldmax, worldmin));

    findshadowvas();
    findshadowmms();

    shadowmaskbatchedmodels(false);
    batchshadowmapmodels();

    rh.prevdynmin = rh.dynmin;
    rh.prevdynmax = rh.dynmax;
    rh.dynmin = vec(1e16f, 1e16f, 1e16f);
    rh.dynmax = vec(-1e16f, -1e16f, -1e16f);
    if(rhdyntex) dynamicshadowvabounds(1<<shadowside, rh.dynmin, rh.dynmax);
    if(rhdynmm) batcheddynamicmodelbounds(1<<shadowside, rh.dynmin, rh.dynmax);

    if(rhforce || rh.prevdynmin.z < rh.prevdynmax.z || rh.dynmin.z < rh.dynmax.z || !rh.allcached())
    {
        if(inoq)
        {
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDepthMask(GL_TRUE);
        }

        glBindFramebuffer_(GL_FRAMEBUFFER, rsmfbo);

        shadowmatrix.mul(rsm.proj, rsm.model);
        GLOBALPARAM(rsmmatrix, shadowmatrix);
        GLOBALPARAMF(rsmdir, -rsm.lightview.x, -rsm.lightview.y, -rsm.lightview.z);

        glViewport(0, 0, rsmsize, rsmsize);
        glClearColor(0, 0, 0, 0);
        glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);

        renderrsmgeom(rhdyntex!=0);
        rendershadowmodelbatches(rhdynmm!=0);

        rh.renderslices();

        if(inoq)
        {
            glBindFramebuffer_(GL_FRAMEBUFFER, msaasamples ? msfbo : gfbo);
            glViewport(0, 0, vieww, viewh);

            glFlush();
        }
    }

    clearbatchedmapmodels();

    shadowmapping = 0;

    endtimer(rhtimer);
    endtimer(rhcputimer);
}

void rendercsmshadowmaps()
{
    if(csminoq && !debugshadowatlas && !inoq && shouldworkinoq()) return;
    if(getpielight().iszero() || !csmshadowmap) return;

    if(inoq)
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, shadowatlasfbo);
        glDepthMask(GL_TRUE);
    }

    csm.setup();

    shadowmapping = SM_CASCADE;
    shadoworigin = vec(0, 0, 0);
    shadowdir = csm.lightview;
    shadowbias = csm.lightview.project_bb(worldmin, worldmax);
    shadowradius = fabs(csm.lightview.project_bb(worldmax, worldmin));

    float polyfactor = csmpolyfactor, polyoffset = csmpolyoffset;
    if(smfilter > 2) { polyfactor = csmpolyfactor2; polyoffset = csmpolyoffset2; }
    if(polyfactor || polyoffset)
    {
        glPolygonOffset(polyfactor, polyoffset);
        glEnable(GL_POLYGON_OFFSET_FILL);
    }

    glEnable(GL_SCISSOR_TEST);

    findshadowvas();
    findshadowmms();

    shadowmaskbatchedmodels(smdynshadow!=0);
    batchshadowmapmodels();

    loopi(csmsplits) if(csm.splits[i].idx >= 0)
    {
        const shadowmapinfo &sm = shadowmaps[csm.splits[i].idx];

        shadowmatrix.mul(csm.splits[i].proj, csm.model);
        GLOBALPARAM(shadowmatrix, shadowmatrix);

        glViewport(sm.x, sm.y, sm.size, sm.size);
        glScissor(sm.x, sm.y, sm.size, sm.size);
        glClear(GL_DEPTH_BUFFER_BIT);

        shadowside = i;

        rendershadowmapworld();
        rendershadowmodelbatches();
    }

    clearbatchedmapmodels();

    glDisable(GL_SCISSOR_TEST);

    if(polyfactor || polyoffset) glDisable(GL_POLYGON_OFFSET_FILL);

    shadowmapping = 0;

    if(inoq)
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, msaasamples ? msfbo : gfbo);
        glViewport(0, 0, vieww, viewh);

        glFlush();
    }
}

int calcshadowinfo(const extentity &e, vec &origin, float &radius, vec &spotloc, int &spotangle, float &bias)
{
    if(e.attrs[6]&L_NOSHADOW) return SM_NONE;
    int rad = e.attrs[0], slight = -1;
    if(!getlightfx(e, &rad, &slight) || rad <= smminradius) return SM_NONE;
    origin = e.o;
    radius = float(rad);
    int type, w, border;
    float lod;
    const vector<extentity *> &ents = entities::getents();
    if(ents.inrange(slight))
    {
        type = SM_SPOT;
        w = 1;
        border = 0;
        lod = smspotprec;
        spotloc = ents[slight]->o;
        spotangle = clamp(int(ents[slight]->attrs[1]), 1, 89);
    }
    else
    {
        type = SM_CUBEMAP;
        w = 3;
        lod = smcubeprec;
        border = smfilter > 2 ? smborder2 : smborder;
        spotloc = e.o;
        spotangle = 0;
    }

    lod *= smminsize;
    int size = clamp(int(ceil((lod * shadowatlaspacker.w) / SHADOWATLAS_SIZE)), 1, shadowatlaspacker.w / w);
    bias = border / float(size - border);

    return type;
}

matrix4 shadowmatrix;

void rendershadowmaps(int offset = 0)
{
    if(!(sminoq && !debugshadowatlas && !inoq && shouldworkinoq())) offset = 0;

    for(; offset < shadowmaps.length(); offset++) if(shadowmaps[offset].light >= 0) break;
    if(offset >= shadowmaps.length()) return;

    if(inoq)
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, shadowatlasfbo);
        glDepthMask(GL_TRUE);
    }

    float polyfactor = smpolyfactor, polyoffset = smpolyoffset;
    if(smfilter > 2) { polyfactor = smpolyfactor2; polyoffset = smpolyoffset2; }
    if(polyfactor || polyoffset)
    {
        glPolygonOffset(polyfactor, polyoffset);
        glEnable(GL_POLYGON_OFFSET_FILL);
    }

    glEnable(GL_SCISSOR_TEST);

    const vector<extentity *> &ents = entities::getents();
    for(int i = offset; i < shadowmaps.length(); i++)
    {
        shadowmapinfo &sm = shadowmaps[i];
        if(sm.light < 0) continue;

        lightinfo &l = lights[sm.light];
        extentity *e = l.ent >= 0 ? ents[l.ent] : NULL;

        int border, sidemask;
        if(l.spot)
        {
            shadowmapping = SM_SPOT;
            border = 0;
            sidemask = 1;
        }
        else
        {
            shadowmapping = SM_CUBEMAP;
            border = smfilter > 2 ? smborder2 : smborder;
            sidemask = drawtex == DRAWTEX_MINIMAP ? 0x2F : (smsidecull ? cullfrustumsides(l.o, l.radius, sm.size, border) : 0x3F);
        }

        sm.sidemask = sidemask;

        shadoworigin = l.o;
        shadowradius = l.radius;
        shadowbias = border / float(sm.size - border);
        shadowdir = l.dir;
        shadowspot = l.spot;

        shadowmesh *mesh = e ? findshadowmesh(l.ent, *e) : NULL;

        findshadowvas();
        findshadowmms();

        shadowmaskbatchedmodels(!(l.flags&L_NODYNSHADOW) && smdynshadow);
        batchshadowmapmodels(mesh != NULL);

        shadowcacheval *cached = NULL;
        int cachemask = 0;
        if(smcache)
        {
            int dynmask = smcache <= 1 ? batcheddynamicmodels() : 0;
            cached = sm.cached;
            if(cached)
            {
                if(!debugshadowatlas) cachemask = cached->sidemask & ~dynmask;
                sm.sidemask |= cachemask;
            }
            sm.sidemask &= ~dynmask;

            sidemask &= ~cachemask;
            if(!sidemask) { clearbatchedmapmodels(); continue; }
        }

        float smnearclip = SQRT3 / l.radius, smfarclip = SQRT3;
        matrix4 smprojmatrix(vec4(float(sm.size - border) / sm.size, 0, 0, 0),
                              vec4(0, float(sm.size - border) / sm.size, 0, 0),
                              vec4(0, 0, -(smfarclip + smnearclip) / (smfarclip - smnearclip), -1),
                              vec4(0, 0, -2*smnearclip*smfarclip / (smfarclip - smnearclip), 0));

        if(shadowmapping == SM_SPOT)
        {
            glViewport(sm.x, sm.y, sm.size, sm.size);
            glScissor(sm.x, sm.y, sm.size, sm.size);
            glClear(GL_DEPTH_BUFFER_BIT);

            float invradius = 1.0f / l.radius, spotscale = invradius * cotan360(l.spot);
            matrix4 spotmatrix(vec(l.spotx).mul(spotscale), vec(l.spoty).mul(spotscale), vec(l.dir).mul(-invradius));
            spotmatrix.translate(vec(l.o).neg());
            shadowmatrix.mul(smprojmatrix, spotmatrix);
            GLOBALPARAM(shadowmatrix, shadowmatrix);

            glCullFace((l.dir.z >= 0) == (smcullside != 0) ? GL_BACK : GL_FRONT);

            shadowside = 0;

            if(mesh) rendershadowmesh(mesh); else rendershadowmapworld();
            rendershadowmodelbatches();
        }
        else
        {
            if(!cachemask)
            {
                int cx1 = sidemask & 0x03 ? 0 : (sidemask & 0xC ? sm.size : 2 * sm.size),
                    cx2 = sidemask & 0x30 ? 3 * sm.size : (sidemask & 0xC ? 2 * sm.size : sm.size),
                    cy1 = sidemask & 0x15 ? 0 : sm.size,
                    cy2 = sidemask & 0x2A ? 2 * sm.size : sm.size;
                glScissor(sm.x + cx1, sm.y + cy1, cx2 - cx1, cy2 - cy1);
                glClear(GL_DEPTH_BUFFER_BIT);
            }
            loop(side, 6) if(sidemask&(1<<side))
            {
                int sidex = (side>>1)*sm.size, sidey = (side&1)*sm.size;
                glViewport(sm.x + sidex, sm.y + sidey, sm.size, sm.size);
                glScissor(sm.x + sidex, sm.y + sidey, sm.size, sm.size);
                if(cachemask) glClear(GL_DEPTH_BUFFER_BIT);

                matrix4 cubematrix(cubeshadowviewmatrix[side]);
                cubematrix.scale(1.0f/l.radius);
                cubematrix.translate(vec(l.o).neg());
                shadowmatrix.mul(smprojmatrix, cubematrix);
                GLOBALPARAM(shadowmatrix, shadowmatrix);

                glCullFace((side & 1) ^ (side >> 2) ^ smcullside ? GL_FRONT : GL_BACK);

                shadowside = side;

                if(mesh) rendershadowmesh(mesh); else rendershadowmapworld();
                rendershadowmodelbatches();
            }
        }

        clearbatchedmapmodels();
    }

    glCullFace(GL_BACK);
    glDisable(GL_SCISSOR_TEST);

    if(polyfactor || polyoffset) glDisable(GL_POLYGON_OFFSET_FILL);

    shadowmapping = 0;

    if(inoq)
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, msaasamples ? msfbo : gfbo);
        glViewport(0, 0, vieww, viewh);

        glFlush();
    }
}

void rendershadowatlas()
{
    timer *smcputimer = begintimer("Shadow Map", false);
    timer *smtimer = begintimer("Shadow Map");

    glBindFramebuffer_(GL_FRAMEBUFFER, shadowatlasfbo);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    if(debugshadowatlas)
    {
        glClearDepth(0);
        glClear(GL_DEPTH_BUFFER_BIT);
        glClearDepth(1);
    }

    // sun light
    rendercsmshadowmaps();

    int smoffset = shadowmaps.length();

    packlights();

    // point lights
    rendershadowmaps(smoffset);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    endtimer(smtimer);
    endtimer(smcputimer);
}

void workinoq()
{
    collectlights();

    if(drawtex && drawtex != DRAWTEX_MAPSHOT) return;

    rendertransparentmapmodels();
    game::render();

    if(shouldworkinoq())
    {
        inoq = true;

        if(csminoq && !debugshadowatlas) rendercsmshadowmaps();
        if(sminoq && !debugshadowatlas) rendershadowmaps();
        if(rhinoq) renderradiancehints();

        inoq = false;
    }
}

FVAR(0, refractmargin, 0, 0.1f, 1);
FVAR(0, refractdepth, 1e-3f, 16, 1e3f);

int transparentlayer = 0;

void rendertransparent()
{
    int hasalphavas = findalphavas();
    int hasmats = findmaterials();
    bool hasmodels = transmdlsx1 < transmdlsx2 && transmdlsy1 < transmdlsy2;
    if(!hasalphavas && !hasmats && !hasmodels)
    {
        if(!editmode) renderparticles();
        return;
    }

    if(!editmode && particlelayers && ghasstencil) renderparticles(PL_UNDER);

    timer *transtimer = begintimer("Transparent");

    if(hasalphavas&4 || hasmats&4)
    {
        glBindFramebuffer_(GL_FRAMEBUFFER, msaalight ? msrefractfbo : refractfbo);
        glDepthMask(GL_FALSE);
        if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
        else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
        float sx1 = min(alpharefractsx1, matrefractsx1), sy1 = min(alpharefractsy1, matrefractsy1),
              sx2 = max(alpharefractsx2, matrefractsx2), sy2 = max(alpharefractsy2, matrefractsy2);
        bool scissor = sx1 > -1 || sy1 > -1 || sx2 < 1 || sy2 < 1;
        if(scissor)
        {
            int x1 = int(floor(max(sx1*0.5f+0.5f-refractmargin*viewh/vieww, 0.0f)*vieww)),
                y1 = int(floor(max(sy1*0.5f+0.5f-refractmargin, 0.0f)*viewh)),
                x2 = int(ceil(min(sx2*0.5f+0.5f+refractmargin*viewh/vieww, 1.0f)*vieww)),
                y2 = int(ceil(min(sy2*0.5f+0.5f+refractmargin, 1.0f)*viewh));
            glEnable(GL_SCISSOR_TEST);
            glScissor(x1, y1, x2 - x1, y2 - y1);
        }
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        if(scissor) glDisable(GL_SCISSOR_TEST);
        GLOBALPARAMF(refractdepth, 1.0f/refractdepth);
        SETSHADER(refractmask);
        if(hasalphavas&4) renderrefractmask();
        if(hasmats&4) rendermaterialmask();

        glDepthMask(GL_TRUE);
    }

    glActiveTexture_(GL_TEXTURE7);
    if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msrefracttex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, refracttex);
    glActiveTexture_(GL_TEXTURE8);
    if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mshdrtex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, hdrtex);
    glActiveTexture_(GL_TEXTURE9);
    if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    glActiveTexture_(GL_TEXTURE0);

    if(ghasstencil) glEnable(GL_STENCIL_TEST);

    matrix4 raymatrix(vec(-0.5f*vieww*projmatrix.a.x, 0, 0.5f*vieww - 0.5f*vieww*projmatrix.c.x),
                      vec(0, -0.5f*viewh*projmatrix.b.y, 0.5f*viewh - 0.5f*viewh*projmatrix.c.y));
    raymatrix.muld(cammatrix);
    GLOBALPARAM(raymatrix, raymatrix);
    GLOBALPARAM(linearworldmatrix, linearworldmatrix);

    uint tiles[LIGHTTILE_MAXH];
    float allsx1 = 1, allsy1 = 1, allsx2 = -1, allsy2 = -1, sx1, sy1, sx2, sy2;

    loop(layer, 4)
    {
        switch(layer)
        {
        case 0:
            if(!(hasmats&1)) continue;
            sx1 = matliquidsx1; sy1 = matliquidsy1; sx2 = matliquidsx2; sy2 = matliquidsy2;
            memcpy(tiles, matliquidtiles, sizeof(tiles));
            break;
        case 1:
            if(!(hasalphavas&1)) continue;
            sx1 = alphabacksx1; sy1 = alphabacksy1; sx2 = alphabacksx2; sy2 = alphabacksy2;
            memcpy(tiles, alphatiles, sizeof(tiles));
            break;
        case 2:
            if(!(hasalphavas&2) && !(hasmats&2)) continue;
            sx1 = alphafrontsx1; sy1 = alphafrontsy1; sx2 = alphafrontsx2; sy2 = alphafrontsy2;
            memcpy(tiles, alphatiles, sizeof(tiles));
            if(hasmats&2)
            {
                sx1 = min(sx1, matsolidsx1);
                sy1 = min(sy1, matsolidsy1);
                sx2 = max(sx2, matsolidsx2);
                sy2 = max(sy2, matsolidsy2);
                loopj(LIGHTTILE_MAXH) tiles[j] |= matsolidtiles[j];
            }
            break;
        case 3:
            if(!hasmodels) continue;
            sx1 = transmdlsx1; sy1 = transmdlsy1; sx2 = transmdlsx2; sy2 = transmdlsy2;
            memcpy(tiles, transmdltiles, sizeof(tiles));
            break;

        default:
            continue;
        }

        transparentlayer = layer+1;

        allsx1 = min(allsx1, sx1);
        allsy1 = min(allsy1, sy1);
        allsx2 = max(allsx2, sx2);
        allsy2 = max(allsy2, sy2);

        glBindFramebuffer_(GL_FRAMEBUFFER, msaalight ? msfbo : gfbo);
        if(ghasstencil)
        {
            glStencilFunc(GL_ALWAYS, layer+1, ~0);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        }
        else
        {
            bool scissor = sx1 > -1 || sy1 > -1 || sx2 < 1 || sy2 < 1;
            if(scissor)
            {
                int x1 = int(floor((sx1*0.5f+0.5f)*vieww)), y1 = int(floor((sy1*0.5f+0.5f)*viewh)),
                    x2 = int(ceil((sx2*0.5f+0.5f)*vieww)), y2 = int(ceil((sy2*0.5f+0.5f)*viewh));
                glEnable(GL_SCISSOR_TEST);
                glScissor(x1, y1, x2 - x1, y2 - y1);
            }

            maskgbuffer("n");
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            if(scissor) glDisable(GL_SCISSOR_TEST);
        }
        maskgbuffer("cndg");

        if(!drawtex && wireframe && editmode) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        switch(layer)
        {
        case 0:
            renderliquidmaterials();
            break;
        case 1:
            renderalphageom(1);
            break;
        case 2:
            if(hasalphavas&2) renderalphageom(2);
            if(hasmats&2) rendersolidmaterials();
            renderstains(STAINBUF_TRANSPARENT, true, layer+1);
            break;
        case 3:
            rendertransparentmodelbatches(layer+1);
            break;
        }

        if(!drawtex && wireframe && editmode) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        if(msaalight)
        {
            glBindFramebuffer_(GL_FRAMEBUFFER, mshdrfbo);
            if((ghasstencil && msaaedgedetect) || msaalight==2) loopi(2) renderlights(sx1, sy1, sx2, sy2, tiles, layer+1, i+1, true);
            else renderlights(sx1, sy1, sx2, sy2, tiles, layer+1, 3, true);
        }
        else
        {
            glBindFramebuffer_(GL_FRAMEBUFFER, hdrfbo);
            renderlights(sx1, sy1, sx2, sy2, tiles, layer+1, 0, true);
        }

        switch(layer)
        {
        case 2:
            renderstains(STAINBUF_TRANSPARENT, false, layer+1);
            break;
        }
    }

    transparentlayer = 0;

    if(ghasstencil) glDisable(GL_STENCIL_TEST);

    endtimer(transtimer);

    if(editmode) return;

    if(particlelayers && ghasstencil)
    {
        bool scissor = allsx1 > -1 || allsy1 > -1 || allsx2 < 1 || allsy2 < 1;
        if(scissor)
        {
            int x1 = int(floor((allsx1*0.5f+0.5f)*vieww)), y1 = int(floor((allsy1*0.5f+0.5f)*viewh)),
                x2 = int(ceil((allsx2*0.5f+0.5f)*vieww)), y2 = int(ceil((allsy2*0.5f+0.5f)*viewh));
            glEnable(GL_SCISSOR_TEST);
            glScissor(x1, y1, x2 - x1, y2 - y1);
        }
        glStencilFunc(GL_NOTEQUAL, 0, 0x07);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glEnable(GL_STENCIL_TEST);
        renderparticles(PL_OVER);
        glDisable(GL_STENCIL_TEST);
        if(scissor) glDisable(GL_SCISSOR_TEST);

        renderparticles(PL_NOLAYER);
    }
    else renderparticles();
}

VAR(0, gdepthclear, 0, 1, 1);
VAR(0, gcolorclear, 0, 1, 1);

void preparegbuffer(bool depthclear)
{
    glBindFramebuffer_(GL_FRAMEBUFFER, msaasamples && (msaalight || !drawtex) ? msfbo : gfbo);
    glViewport(0, 0, vieww, viewh);

    if(drawtex && gdepthinit)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, vieww, viewh);
    }
    if(gdepthformat && gdepthclear)
    {
        maskgbuffer("d");
        if(gdepthformat == 1) glClearColor(1, 1, 1, 1);
        else glClearColor(-farplane, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        maskgbuffer("cn");
    }
    else maskgbuffer("cnd");
    if(gcolorclear) glClearColor(0, 0, 0, 0);
    glClear((depthclear ? GL_DEPTH_BUFFER_BIT : 0)|(gcolorclear ? GL_COLOR_BUFFER_BIT : 0)|(depthclear && ghasstencil && (!msaasamples || msaalight || ghasstencil > 1) ? GL_STENCIL_BUFFER_BIT : 0));
    if(gdepthformat && gdepthclear) maskgbuffer("cnd");
    if(drawtex && gdepthinit) glDisable(GL_SCISSOR_TEST);
    gdepthinit = true;

    matrix4 invscreenmatrix;
    invscreenmatrix.identity();
    invscreenmatrix.settranslation(-1.0f, -1.0f, -1.0f);
    invscreenmatrix.setscale(2.0f/vieww, 2.0f/viewh, 2.0f);
    eyematrix.muld(invprojmatrix, invscreenmatrix);
    if(drawtex == DRAWTEX_MINIMAP)
    {
        linearworldmatrix.muld(invcamprojmatrix, invscreenmatrix);
        if(!gdepthformat) worldmatrix = linearworldmatrix;
        linearworldmatrix.a.z = invcammatrix.a.z;
        linearworldmatrix.b.z = invcammatrix.b.z;
        linearworldmatrix.c.z = invcammatrix.c.z;
        linearworldmatrix.d.z = invcammatrix.d.z;
        if(gdepthformat) worldmatrix = linearworldmatrix;

        GLOBALPARAMF(radialfogscale, 0, 0, 0, 0);
    }
    else
    {
        float xscale = eyematrix.a.x, yscale = eyematrix.b.y, xoffset = eyematrix.d.x, yoffset = eyematrix.d.y, zscale = eyematrix.d.z;
        matrix4 depthmatrix(vec(xscale/zscale, 0, xoffset/zscale), vec(0, yscale/zscale, yoffset/zscale));
        linearworldmatrix.muld(invcammatrix, depthmatrix);
        if(gdepthformat) worldmatrix = linearworldmatrix;
        else worldmatrix.muld(invcamprojmatrix, invscreenmatrix);

        GLOBALPARAMF(radialfogscale, xscale/zscale, yscale/zscale, xoffset/zscale, yoffset/zscale);
    }

    screenmatrix.identity();
    screenmatrix.settranslation(0.5f*vieww, 0.5f*viewh, 0.5f);
    screenmatrix.setscale(0.5f*vieww, 0.5f*viewh, 0.5f);
    screenmatrix.muld(camprojmatrix);

    GLOBALPARAMF(viewsize, vieww, viewh, 1.0f/vieww, 1.0f/viewh);
    GLOBALPARAMF(gdepthscale, eyematrix.d.z, eyematrix.c.w, eyematrix.d.w);
    GLOBALPARAMF(gdepthinvscale, eyematrix.d.z / eyematrix.c.w, eyematrix.d.w / eyematrix.c.w);
    GLOBALPARAMF(gdepthpackparams, -1.0f/farplane, -255.0f/farplane, -(255.0f*255.0f)/farplane);
    GLOBALPARAMF(gdepthunpackparams, -farplane, -farplane/255.0f, -farplane/(255.0f*255.0f));
    GLOBALPARAM(worldmatrix, worldmatrix);

    GLOBALPARAMF(ldrscale, ldrscale);
    GLOBALPARAMF(hdrgamma, hdrgamma, 1.0f/hdrgamma);
    GLOBALPARAM(camera, camera1->o);
    GLOBALPARAMF(millis, lastmillis/1000.0f);

    GLERROR;

    if(depthclear) resetlights();

    resetmodelbatches();
}

void rendergbuffer(bool depthclear)
{
    timer *gcputimer = drawtex ? NULL : begintimer("G-Buffer", false);
    timer *gtimer = drawtex ? NULL : begintimer("G-Buffer");

    preparegbuffer(depthclear);

    if(limitsky())
    {
        renderexplicitsky();
        GLERROR;
    }
    rendergeom();
    GLERROR;
    renderdecals();
    GLERROR;
    rendermapmodels();
    GLERROR;

    if(drawtex == DRAWTEX_MINIMAP)
    {
        if(depthclear) findmaterials();
        renderminimapmaterials();
        GLERROR;
    }
    else
    {
        rendermodelbatches();
        GLERROR;
        if(!drawtex)
        {
            renderstains(STAINBUF_OPAQUE, true);
            renderstains(STAINBUF_MAPMODEL, true);
            GLERROR;
            //renderavatar();
            //GLERROR;
        }
    }

    endtimer(gtimer);
    endtimer(gcputimer);
}

void shademinimap(const vec &color)
{
    GLERROR;

    glBindFramebuffer_(GL_FRAMEBUFFER, msaalight ? mshdrfbo : hdrfbo);
    glViewport(0, 0, vieww, viewh);

    if(color.x >= 0)
    {
        glClearColor(color.x, color.y, color.z, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    renderlights(-1, -1, 1, 1, NULL, 0, msaalight ? -1 : 0);
    GLERROR;
}

void shademodelpreview(int x, int y, int w, int h, bool background, bool scissor)
{
    GLERROR;

    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, hudw, hudh);

    if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mscolortex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, gcolortex);
    glActiveTexture_(GL_TEXTURE1);
    if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msnormaltex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, gnormaltex);
    glActiveTexture_(GL_TEXTURE3);
    if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
    else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
    glActiveTexture_(GL_TEXTURE0);

    float lightscale = 2.0f*ldrscale;
    GLOBALPARAMF(lightscale, 0.1f*lightscale, 0.1f*lightscale, 0.1f*lightscale, lightscale);
    GLOBALPARAM(sunlightdir, vec(0, -1, 2).normalize());
    GLOBALPARAMF(sunlightcolor, 0.6f*lightscale, 0.6f*lightscale, 0.6f*lightscale);

    SETSHADER(modelpreview);

    LOCALPARAMF(cutout, background ? -1 : 0);

    if(scissor) glEnable(GL_SCISSOR_TEST);

    int sx = clamp(x, 0, hudw), sy = clamp(y, 0, hudh),
        sw = clamp(x + w, 0, hudw) - sx, sh = clamp(y + h, 0, hudh) - sy;
    float sxk = 2.0f/hudw, syk = 2.0f/hudh, txk = vieww/float(w), tyk = viewh/float(h);
    hudquad(sx*sxk - 1, sy*syk - 1, sw*sxk, sh*syk, (sx-x)*txk, (sy-y)*tyk, sw*txk, sh*tyk);

    if(scissor) glDisable(GL_SCISSOR_TEST);

    GLERROR;
}

void shadesky()
{
    glBindFramebuffer_(GL_FRAMEBUFFER, msaalight ? mshdrfbo : hdrfbo);
    glViewport(0, 0, vieww, viewh);

    drawskybox((hdrclear > 0 ? hdrclear-- : msaalight) > 0);
}

void shadegbuffer()
{
    if(msaasamples && !msaalight && !drawtex) resolvemsaadepth();
    GLERROR;

    timer *shcputimer = begintimer("Deferred Shading", false);
    timer *shtimer = begintimer("Deferred Shading");

    shadesky();

    if(msaasamples && (msaalight || !drawtex))
    {
        if((ghasstencil && msaaedgedetect) || msaalight==2) loopi(2) renderlights(-1, -1, 1, 1, NULL, 0, i+1);
        else renderlights(-1, -1, 1, 1, NULL, 0, drawtex ? -1 : 3);
    }
    else renderlights();
    GLERROR;

    if(!drawtex)
    {
        renderstains(STAINBUF_OPAQUE, false);
        renderstains(STAINBUF_MAPMODEL, false);
    }

    endtimer(shtimer);
    endtimer(shcputimer);
}

void setuplights()
{
    GLERROR;
    setupgbuffer();
    if(bloomw < 0 || bloomh < 0) setupbloom(gw, gh);
    if(ao && (aow < 0 || aoh < 0)) setupao(gw, gh);
    if(volumetriclights && volumetric && (volw < 0 || volh < 0)) setupvolumetric(gw, gh);
    if(!shadowatlasfbo) setupshadowatlas();
    if(useradiancehints() && !rhfbo) setupradiancehints();
    if(!deferredlightshader) loaddeferredlightshaders();
    if(drawtex == DRAWTEX_MINIMAP && !deferredminimapshader) deferredminimapshader = loaddeferredlightshader(msaalight ? "mM" : "m");
    setupaa(gw, gh);
    GLERROR;
}

bool debuglights()
{
    if(debugshadowatlas) viewshadowatlas();
    else if(debugao) viewao();
    else if(debugdepth) viewdepth();
    else if(debugstencil) viewstencil();
    else if(debugrefract) viewrefract();
    else if(debuglightscissor) viewlightscissor();
    else if(debugrsm) viewrsm();
    else if(debugrh) viewrh();
    else if(!debugaa()) return false;
    return true;
}

void cleanuplights()
{
    cleanupgbuffer();
    cleanupbloom();
    cleanupao();
    cleanupvolumetric();
    cleanupshadowatlas();
    cleanupradiancehints();
    lightsphere::cleanup();
    cleanupaa();
}
