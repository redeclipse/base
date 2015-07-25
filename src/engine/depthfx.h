// eye space depth texture for soft particles, done at low res then blurred to prevent ugly jaggies
VAR(IDF_PERSIST, depthfxfpscale, 1, 1<<12, 1<<16);
VAR(IDF_PERSIST, depthfxscale, 1, 1<<6, 1<<8);
VAR(IDF_PERSIST, depthfxblend, 1, 16, 64);
VAR(IDF_PERSIST, depthfxpartblend, 1, 8, 64);
VAR(0, depthfxmargin, 0, 16, 64);
VAR(0, depthfxbias, 0, 1, 64);

extern void cleanupdepthfx();
VARF(IDF_PERSIST, fpdepthfx, 0, 0, 1, cleanupdepthfx());
VAR(IDF_PERSIST, depthfxemuprecision, 0, 1, 1);
VARF(IDF_PERSIST, depthfxsize, 6, 7, 12, cleanupdepthfx());
VAR(IDF_PERSIST, depthfx, 0, 1, 1);
VAR(IDF_PERSIST, depthfxparts, 0, 1, 1);
VARF(IDF_PERSIST, depthfxfilter, 0, 1, 1, cleanupdepthfx());
VAR(IDF_PERSIST, blurdepthfx, 0, 1, 7);
VAR(IDF_PERSIST, blurdepthfxsigma, 1, 50, 200);
VAR(0, depthfxscissor, 0, 2, 2);
VAR(0, debugdepthfx, 0, 0, 1);

#define MAXDFXRANGES 4

void *depthfxowners[MAXDFXRANGES];
float depthfxranges[MAXDFXRANGES];
int numdepthfxranges = 0;
vec depthfxmin(1e16f, 1e16f, 1e16f), depthfxmax(1e16f, 1e16f, 1e16f);

static struct depthfxtexture : rendertarget
{
    const GLenum *colorformats() const
    {
        static const GLenum colorfmts[] = { GL_RG16F, GL_RGB16F, GL_RGBA, GL_RGBA8, GL_RGB, GL_RGB8, GL_FALSE };
        static const GLenum colorfmtsnv[] = { GL_FLOAT_RG16_NV, GL_RGB16F, GL_RGBA, GL_RGBA8, GL_RGB, GL_RGB8, GL_FALSE };
        if(!hasTF || !fpdepthfx) return &colorfmts[2];
        if(hasTRG) return colorfmts;
        if(hasNVFB && !filter()) return colorfmtsnv;
        return &colorfmts[1];
    }

    float eyedepth(const vec &p) const
    {
        return max(-mvmatrix.transform<vec>(p).z, 0.0f);
    }

    void addscissorvert(const vec &v, float &sx1, float &sy1, float &sx2, float &sy2)
    {
        vec p = mvpmatrix.perspectivetransform(v);
        sx1 = min(sx1, p.x);
        sy1 = min(sy1, p.y);
        sx2 = max(sx2, p.x);
        sy2 = max(sy2, p.y);
    }

    bool addscissorbox(const vec &center, float size)
    {
        float sx1, sy1, sx2, sy2;
        calcspherescissor(center, size, sx1, sy1, sx2, sy2);
        return addblurtiles(sx1, sy1, sx2, sy2);
    }

    bool addscissorbox(const vec &bbmin, const vec &bbmax)
    {
        float sx1 = 1, sy1 = 1, sx2 = -1, sy2 = -1;
        loopi(8)
        {
            vec v(i&1 ? bbmax.x : bbmin.x, i&2 ? bbmax.y : bbmin.y, i&4 ? bbmax.z : bbmin.z);
            addscissorvert(v, sx1, sy1, sx2, sy2);
        }
        return addblurtiles(sx1, sy1, sx2, sy2);
    }

    bool texrect() const { return true; }
    bool filter() const { return depthfxfilter!=0; }
    bool highprecision() const { return colorfmt==GL_RG16F || colorfmt==GL_FLOAT_RG16_NV || colorfmt==GL_RGB16F; }
    bool emulatehighprecision() const { return depthfxemuprecision && !depthfxfilter; }

    bool shouldrender()
    {
        extern void finddepthfxranges();
        finddepthfxranges();
        return (numdepthfxranges && scissorx1 < scissorx2 && scissory1 < scissory2) || debugdepthfx;
    }

    bool dorender()
    {
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        depthfxing = true;
        refracting = -1;

        extern void renderdepthobstacles(const vec &bbmin, const vec &bbmax, float scale, float *ranges, int numranges);
        float scale = depthfxscale;
        float *ranges = depthfxranges;
        int numranges = numdepthfxranges;
        if(highprecision())
        {
            scale = depthfxfpscale;
            ranges = NULL;
            numranges = 0;
        }
        else if(emulatehighprecision())
        {
            scale = depthfxfpscale;
            ranges = NULL;
            numranges = -3;
        }
        renderdepthobstacles(depthfxmin, depthfxmax, scale, ranges, numranges);

        refracting = 0;
        depthfxing = false;

        return numdepthfxranges > 0;
    }

    void dodebug(int w, int h)
    {
        if(numdepthfxranges > 0)
        {
            glColor3f(0, 1, 0);
            debugscissor(w, h, true);
            glColor3f(0, 0, 1);
            debugblurtiles(w, h, true);
            glColor3f(1, 1, 1);
        }
    }
} depthfxtex;

void cleanupdepthfx()
{
    depthfxtex.cleanup(true);
}

void viewdepthfxtex()
{
    if(!depthfx) return;
    depthfxtex.debug();
}

bool depthfxing = false;

bool binddepthfxtex()
{
    if(!reflecting && !refracting && depthfx && depthfxtex.rendertex && numdepthfxranges>0)
    {
        glActiveTexture_(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_RECTANGLE, depthfxtex.rendertex);
        glActiveTexture_(GL_TEXTURE0);

        float w = 0.5f*depthfxtex.vieww, h = 0.5f*depthfxtex.viewh;
        GLOBALPARAMF(depthfxview, w, h);
        return true;
    }
    return false;
}

void binddepthfxparams(float blend, float minblend = 0, bool allow = true, void *owner = NULL)
{
    if(!reflecting && !refracting && depthfx && depthfxtex.rendertex && numdepthfxranges>0)
    {
        float scale = 0, offset = -1, texscale = 0;
        if(!depthfxtex.highprecision())
        {
            float select[4] = { 0, 0, 0, 0 };
            if(!depthfxtex.emulatehighprecision())
            {
                loopi(numdepthfxranges) if(depthfxowners[i]==owner)
                {
                    select[i] = float(depthfxscale)/blend;
                    scale = 1.0f/blend;
                    offset = -float(depthfxranges[i] - depthfxbias)/blend;
                    break;
                }
            }
            else if(allow)
            {
                select[0] = float(depthfxfpscale)/blend;
                select[1] = select[0]/256;
                select[2] = select[1]/256;
                scale = 1.0f/blend;
                offset = 0;
            }
            LOCALPARAMF(depthfxselect, select[0], select[1], select[2], select[3]);
        }
        else if(allow)
        {
            scale = 1.0f/blend;
            offset = 0;
            texscale = float(depthfxfpscale)/blend;
        }
        LOCALPARAMF(depthfxparams, scale, offset, texscale, minblend);
    }
}

void drawdepthfxtex()
{
    if(!depthfx) return;

    // Apple/ATI bug - fixed-function fog state can force software fallback even when fragment program is enabled
    glDisable(GL_FOG);
    depthfxtex.render(1<<depthfxsize, 1<<depthfxsize, blurdepthfx, blurdepthfxsigma/100.0f);
    glEnable(GL_FOG);
}

