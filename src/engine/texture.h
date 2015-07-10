enum { SHPARAM_LOOKUP = 0, SHPARAM_VERTEX, SHPARAM_PIXEL, SHPARAM_UNIFORM };

#define RESERVEDSHADERPARAMS 16
#define MAXSHADERPARAMS 8

struct ShaderParam
{
    const char *name;
    int type, index, loc, palette, palindex;
    float val[4];
};

struct LocalShaderParamState : ShaderParam
{
    float curval[4];
    GLenum format;

    LocalShaderParamState() : format(GL_FLOAT_VEC4)
    {
        memset(curval, -1, sizeof(curval));
    }
    LocalShaderParamState(const ShaderParam &p) : ShaderParam(p), format(GL_FLOAT_VEC4)
    {
        memset(curval, -1, sizeof(curval));
    }
};

struct ShaderParamState
{
    enum
    {
        CLEAN = 0,
        INVALID,
        DIRTY
    };

    const char *name;
    float val[4];
    bool local;
    int dirty;

    ShaderParamState()
        : name(NULL), local(false), dirty(INVALID)
    {
        memset(val, -1, sizeof(val));
    }
};

enum
{
    SHADER_DEFAULT    = 0,
    SHADER_NORMALSLMS = 1<<0,
    SHADER_ENVMAP     = 1<<1,
    SHADER_OPTION     = 1<<3,

    SHADER_INVALID    = 1<<8,
    SHADER_DEFERRED   = 1<<9
};

#define MAXSHADERDETAIL 3
#define MAXVARIANTROWS 5

extern int shaderdetail;

struct Slot;
struct VSlot;

struct UniformLoc
{
    const char *name, *blockname;
    int loc, version, binding, stride, offset, size;
    void *data;
    UniformLoc(const char *name = NULL, const char *blockname = NULL, int binding = -1, int stride = -1) : name(name), blockname(blockname), loc(-1), version(-1), binding(binding), stride(stride), offset(-1), size(-1), data(NULL) {}
};

struct AttribLoc
{
    const char *name;
    int loc;
    AttribLoc(const char *name = NULL, int loc = -1) : name(name), loc(loc) {}
};

struct Shader
{
    static Shader *lastshader;

    char *name, *vsstr, *psstr, *defer;
    int type;
    GLuint program, vsobj, psobj;
    vector<LocalShaderParamState> defaultparams;
    Shader *detailshader, *variantshader, *altshader, *fastshader[MAXSHADERDETAIL];
    vector<Shader *> variants[MAXVARIANTROWS];
    bool standard, forced, used;
    Shader *reusevs, *reuseps;
    int numextparams;
    LocalShaderParamState *extparams;
    uchar *extvertparams, *extpixparams;
    vector<UniformLoc> uniformlocs;
    vector<AttribLoc> attriblocs;

    Shader() : name(NULL), vsstr(NULL), psstr(NULL), defer(NULL), type(SHADER_DEFAULT), program(0), vsobj(0), psobj(0), detailshader(NULL), variantshader(NULL), altshader(NULL), standard(false), forced(false), used(false), reusevs(NULL), reuseps(NULL), numextparams(0), extparams(NULL), extvertparams(NULL), extpixparams(NULL)
    {
        loopi(MAXSHADERDETAIL) fastshader[i] = this;
    }

    ~Shader()
    {
        DELETEA(name);
        DELETEA(vsstr);
        DELETEA(psstr);
        DELETEA(defer);
        DELETEA(extparams);
        DELETEA(extvertparams);
        extpixparams = NULL;
    }

    void fixdetailshader(bool force = true, bool recurse = true);
    void allocenvparams(Slot *slot = NULL);
    void flushenvparams(Slot *slot = NULL);
    void setslotparams(Slot &slot, VSlot &vslot);
    void bindprograms();

    bool hasoption(int row)
    {
        if(!detailshader || detailshader->variants[row].empty()) return false;
        return (detailshader->variants[row][0]->type&SHADER_OPTION)!=0;
    }

    static inline bool isnull(const Shader *s) { return !s; }

    bool isnull() const { return isnull(this); }

    void setvariant_(int col, int row, Shader *fallbackshader)
    {
        Shader *s = fallbackshader;
        for(col = min(col, detailshader->variants[row].length()-1); col >= 0; col--)
            if(!(detailshader->variants[row][col]->type&SHADER_INVALID))
            {
                s = detailshader->variants[row][col];
                break;
            }
        if(lastshader!=s) s->bindprograms();
    }

    void setvariant(int col, int row, Shader *fallbackshader)
    {
        if(isnull() || !detailshader) return;
        setvariant_(col, row, fallbackshader);
        lastshader->flushenvparams();
    }

    void setvariant(int col, int row)
    {
        if(isnull() || !detailshader) return;
        setvariant_(col, row, detailshader);
        lastshader->flushenvparams();
    }

    void setvariant(int col, int row, Slot &slot, VSlot &vslot, Shader *fallbackshader)
    {
        if(isnull() || !detailshader) return;
        setvariant_(col, row, fallbackshader);
        lastshader->flushenvparams(&slot);
        lastshader->setslotparams(slot, vslot);
    }

    void setvariant(int col, int row, Slot &slot, VSlot &vslot)
    {
        if(isnull() || !detailshader) return;
        setvariant_(col, row, detailshader);
        lastshader->flushenvparams(&slot);
        lastshader->setslotparams(slot, vslot);
    }

    void set_()
    {
        if(lastshader!=detailshader) detailshader->bindprograms();
    }

    void set()
    {
        if(isnull() || !detailshader) return;
        set_();
        lastshader->flushenvparams();
    }

    void set(Slot &slot, VSlot &vslot)
    {
        if(isnull() || !detailshader) return;
        set_();
        lastshader->flushenvparams(&slot);
        lastshader->setslotparams(slot, vslot);
    }

    bool compile();
    void cleanup(bool invalid = false);

    static int uniformlocversion();
};

#define SETSHADER(name) \
    do { \
        static Shader *name##shader = NULL; \
        if(!name##shader) name##shader = lookupshaderbyname(#name); \
        name##shader->set(); \
    } while(0)

struct ImageData
{
    int w, h, bpp, levels, align, pitch;
    GLenum compressed;
    uchar *data;
    void *owner;
    void (*freefunc)(void *);

    ImageData()
        : data(NULL), owner(NULL), freefunc(NULL)
    {}


    ImageData(int nw, int nh, int nbpp, int nlevels = 1, int nalign = 0, GLenum ncompressed = GL_FALSE)
    {
        setdata(NULL, nw, nh, nbpp, nlevels, nalign, ncompressed);
    }

    ImageData(int nw, int nh, int nbpp, uchar *data)
        : owner(NULL), freefunc(NULL)
    {
        setdata(data, nw, nh, nbpp);
    }

    ImageData(SDL_Surface *s) { wrap(s); }
    ~ImageData() { cleanup(); }

    void setdata(uchar *ndata, int nw, int nh, int nbpp, int nlevels = 1, int nalign = 0, GLenum ncompressed = GL_FALSE)
    {
        w = nw;
        h = nh;
        bpp = nbpp;
        levels = nlevels;
        align = nalign;
        pitch = align ? 0 : w*bpp;
        compressed = ncompressed;
        data = ndata ? ndata : new uchar[calcsize()];
        if(!ndata) { owner = this; freefunc = NULL; }
    }

    int calclevelsize(int level) const { return ((max(w>>level, 1)+align-1)/align)*((max(h>>level, 1)+align-1)/align)*bpp; }

    int calcsize() const
    {
        if(!align) return w*h*bpp;
        int lw = w, lh = h,
            size = 0;
        loopi(levels)
        {
            if(lw<=0) lw = 1;
            if(lh<=0) lh = 1;
            size += ((lw+align-1)/align)*((lh+align-1)/align)*bpp;
            if(lw*lh==1) break;
            lw >>= 1;
            lh >>= 1;
        }
        return size;
    }

    void disown()
    {
        data = NULL;
        owner = NULL;
        freefunc = NULL;
    }

    void cleanup()
    {
        if(owner==this) delete[] data;
        else if(freefunc) (*freefunc)(owner);
        disown();
    }

    void replace(ImageData &d)
    {
        cleanup();
        *this = d;
        if(owner == &d) owner = this;
        d.disown();
    }

    void wrap(SDL_Surface *s)
    {
        setdata((uchar *)s->pixels, s->w, s->h, s->format->BytesPerPixel);
        pitch = s->pitch;
        owner = s;
        freefunc = (void (*)(void *))SDL_FreeSurface;
    }
};

// management of texture slots
// each texture slot can have multiple texture frames, of which currently only the first is used
// additional frames can be used for various shaders

struct TextureAnim
{
    int count, delay, x, y, w, h;
    bool throb;
    TextureAnim() : count(0), delay(0), throb(false) {}
};


struct Texture
{
    enum
    {
        IMAGE      = 0,
        CUBEMAP    = 1,
        TYPE       = 0xFF,

        STUB       = 1<<8,
        TRANSIENT  = 1<<9,
        COMPRESSED = 1<<10,
        ALPHA      = 1<<11,
        FLAGS      = 0xFF00
    };

    char *name;
    int type, w, h, xs, ys, bpp, clamp, frame, delay, last;
    bool mipmap, canreduce, throb;
    vector<GLuint> frames;
    GLuint id;
    uchar *alphamask;


    Texture() : frame(0), delay(0), last(0), throb(false), alphamask(NULL)
    {
        frames.shrink(0);
    }

    GLuint idframe(int id)
    {
        if(!frames.empty())
            return frames[::clamp(id, 0, frames.length()-1)];
        return id;
    }

    GLuint getframe(float amt)
    {
        if(!frames.empty())
            return frames[::clamp(int((frames.length()-1)*amt), 0, frames.length()-1)];
        return id;
    }

    GLuint retframe(int cur, int total)
    {
        if(!frames.empty())
            return frames[::clamp((frames.length()-1)*cur/min(1, total), 0, frames.length()-1)];
        return id;
    }
};

enum
{
    TEX_DIFFUSE = 0,
    TEX_UNKNOWN,
    TEX_DECAL,
    TEX_NORMAL,
    TEX_GLOW,
    TEX_SPEC,
    TEX_DEPTH,
    TEX_ENVMAP,
    TEX_MAX
};

enum
{
    VSLOT_SHPARAM = 0,
    VSLOT_SCALE,
    VSLOT_ROTATION,
    VSLOT_OFFSET,
    VSLOT_SCROLL,
    VSLOT_LAYER,
    VSLOT_ALPHA,
    VSLOT_COLOR,
    VSLOT_PALETTE,
    VSLOT_COAST,
    VSLOT_NUM
};

#define DEFAULT_ALPHA_FRONT 0.5f
#define DEFAULT_ALPHA_BACK 0.0f

struct VSlot
{
    Slot *slot;
    VSlot *next;
    int index, changed;
    vector<ShaderParam> params;
    bool linked;
    float scale;
    int rotation;
    ivec2 offset;
    vec2 scroll;
    int layer;
    int palette, palindex;
    float alphafront, alphaback;
    vec colorscale;
    float pulseglowspeed;
    ShaderParam *glowcolor, *pulseglowcolor;
    vec envscale;
    int skipped;
    float coastscale;

    VSlot(Slot *slot = NULL, int index = -1) : slot(slot), next(NULL), index(index), changed(0), skipped(0)
    {
        reset();
        if(slot) addvariant(slot);
    }

    void addvariant(Slot *slot);

    void reset()
    {
        params.shrink(0);
        linked = false;
        scale = 1;
        rotation = 0;
        offset = ivec2(0, 0);
        scroll = vec2(0, 0);
        layer = palette = palindex = 0;
        alphafront = DEFAULT_ALPHA_FRONT;
        alphaback = DEFAULT_ALPHA_BACK;
        colorscale = vec(1, 1, 1);
        pulseglowspeed = 0;
        glowcolor = pulseglowcolor = NULL;
        envscale = vec(0, 0, 0);
        coastscale = 1;
    }

    vec getcolorscale() const { return palette || palindex ? vec(colorscale).mul(game::getpalette(palette, palindex)) : colorscale; }
    vec getglowcolor() const
    {
        if(glowcolor)
        {
            vec c(glowcolor->val);
            if(glowcolor->palette || glowcolor->palindex) c.mul(game::getpalette(glowcolor->palette, glowcolor->palindex));
            else if(palette || palindex) c.mul(game::getpalette(palette, palindex));
            return c.clamp(0.0f, 1.0f);
        }
        if(palette || palindex) return game::getpalette(palette, palindex);
        return vec(1, 1, 1);
    }
    vec getpulseglowcolor() const
    {
        if(pulseglowcolor)
        {
            vec c(pulseglowcolor->val);
            if(pulseglowcolor->palette || pulseglowcolor->palindex) c.mul(game::getpalette(pulseglowcolor->palette, pulseglowcolor->palindex));
            else if(palette || palindex) c.mul(game::getpalette(palette, palindex));
            return c.clamp(0.0f, 1.0f);
        }
        //if(palette || palindex) return vec(1, 1, 1).sub(game::getpalette(palette, palindex));
        return vec(0, 0, 0);
    }

    void cleanup()
    {
        linked = false;
        glowcolor = pulseglowcolor = NULL;
    }
};

struct Slot
{
    struct Tex
    {
        int type;
        Texture *t;
        string name;
        int combined;
    };

    int index;
    vector<Tex> sts;
    Shader *shader;
    vector<ShaderParam> params;
    VSlot *variants;
    bool loaded;
    uint texmask;
    char *texgrass;
    vec grasscolor;
    float grassblend;
    int grassscale, grassheight;
    Texture *grasstex, *thumbnail;
    char *layermaskname;
    int layermaskmode;
    float layermaskscale;
    ImageData *layermask;

    Slot(int index = -1) : index(index), variants(NULL), texgrass(NULL), layermaskname(NULL), layermask(NULL) { reset(); }

    void reset()
    {
        sts.shrink(0);
        shader = NULL;
        params.shrink(0);
        loaded = false;
        texmask = 0;
        DELETEA(texgrass);
        grasscolor = vec(0, 0, 0);
        grassblend = 0;
        grassscale = grassheight = 0;
        grasstex = NULL;
        thumbnail = NULL;
        DELETEA(layermaskname);
        layermaskmode = 0;
        layermaskscale = 1;
        if(layermask) DELETEP(layermask);
    }

    void cleanup()
    {
        loaded = false;
        grasstex = NULL;
        thumbnail = NULL;
        loopv(sts)
        {
            Tex &t = sts[i];
            t.t = NULL;
            t.combined = -1;
        }
    }
};

inline void VSlot::addvariant(Slot *slot)
{
    if(!slot->variants) slot->variants = this;
    else
    {
        VSlot *prev = slot->variants;
        while(prev->next) prev = prev->next;
        prev->next = this;
    }
}

struct MSlot : Slot, VSlot
{
    MSlot() : VSlot(this) {}

    void reset()
    {
        Slot::reset();
        VSlot::reset();
    }

    void cleanup()
    {
        Slot::cleanup();
        VSlot::cleanup();
    }
};

extern void scaleimage(ImageData &s, int w, int h);

enum
{
    IFMT_NONE = 0,
    IFMT_BMP,
    IFMT_PNG,
    IFMT_TGA,
    IFMT_MAX,
};
extern const char *ifmtexts[IFMT_MAX];
extern char *notexturetex, *blanktex, *logotex, *badgetex, *emblemtex, *nothumbtex, *backgroundtex;
extern int imageformat;

extern void savepng(const char *filename, ImageData &image, int compress = 9, bool flip = false);
extern void savetga(const char *filename, ImageData &image, int compress = 1, bool flip = false);
extern void saveimage(const char *name, ImageData &image, int format = IFMT_NONE, int compress = 9, bool flip = false, bool skip = false);
extern SDL_Surface *loadsurface(const char *name, bool noload = false);
extern bool loadimage(const char *name, ImageData &image);
extern bool loaddds(const char *filename, ImageData &image, int force = 0);

extern void resetmaterials();
extern void resettextures(int n = 0);
extern void setshader(char *name);
extern void setshaderparam(const char *name, int type, int n, float x, float y, float z, float w);
extern int findtexturetype(char *name, bool tryint = false);
extern const char *findtexturetypename(int type);
extern void texture(char *type, char *name, int *rot, int *xoffet, int *yoffset, float *scale);
extern void updatetextures();
extern void preloadtextures(int flags = IDF_PRELOAD);

struct cubemapside
{
    GLenum target;
    const char *name;
    bool flipx, flipy, swapxy;
};

extern cubemapside cubemapsides[6];
extern Texture *notexture, *blanktexture;
extern Shader *defaultshader, *rectshader, *cubemapshader, *notextureshader, *nocolorshader, *foggedshader, *foggednotextureshader, *stdworldshader;
extern int reservevpparams, maxvsuniforms, maxfsuniforms;

extern Shader *lookupshaderbyname(const char *name);
extern Shader *useshaderbyname(const char *name);
extern Texture *loadthumbnail(Slot &slot);
extern void resetslotshader();
extern void setslotshader(Slot &s);
extern void linkslotshader(Slot &s, bool load = true);
extern void linkvslotshader(VSlot &s, bool load = true);
extern void linkslotshaders();
extern void setenvparamf(const char *name, int type, int index, float x = 0, float y = 0, float z = 0, float w = 0);
extern void setenvparamfv(const char *name, int type, int index, const float *v);
extern void flushenvparamf(const char *name, int type, int index, float x = 0, float y = 0, float z = 0, float w = 0);
extern void flushenvparamfv(const char *name, int type, int index, const float *v);
extern void setlocalparamf(const char *name, int type, int index, float x = 0, float y = 0, float z = 0, float w = 0);
extern void setlocalparamfv(const char *name, int type, int index, const float *v);
extern void invalidateenvparams(int type, int start, int count);
extern ShaderParam *findshaderparam(Slot &s, const char *name, int type, int index);
extern ShaderParam *findshaderparam(VSlot &s, const char *name, int type, int index);
extern const char *getshaderparamname(const char *name);

extern MSlot &lookupmaterialslot(int slot, bool load = true);
extern Slot &lookupslot(int slot, bool load = true);
extern VSlot &lookupvslot(int slot, bool load = true);
extern VSlot *findvslot(Slot &slot, const VSlot &src, const VSlot &delta);
extern VSlot *editvslot(const VSlot &src, const VSlot &delta);
extern void mergevslot(VSlot &dst, const VSlot &src, const VSlot &delta);

extern vector<Slot *> slots;
extern vector<VSlot *> vslots;
extern MSlot materialslots[(MATF_VOLUME|MATF_INDEX)+1];

#define MAXDYNLIGHTS 5
#define DYNLIGHTBITS 6
#define DYNLIGHTMASK ((1<<DYNLIGHTBITS)-1)

#define MAXBLURRADIUS 7

extern void setupblurkernel(int radius, float sigma, float *weights, float *offsets);
extern void setblurshader(int pass, int size, int radius, float *weights, float *offsets, GLenum target = GL_TEXTURE_2D);

#define _TVAR(f, n, c, m) _SVARF(n, n, c, { if(initing==NOT_INITING && n[0]) textureload(n, m, true); }, f|IDF_TEXTURE)
#define TVAR(f, n, c, m)  _TVAR(f, n, c, m)
#define _TVARN(f, n, c, t, m) _SVARF(n, n, c, { if(initing==NOT_INITING) t = n[0] ? textureload(n, m, true) : notexture; }, f|IDF_TEXTURE)
#define TVARN(f, n, c, t, m) _TVARN(f, n, c, t, m)
#define TVARC(f, n, c, t, m) Texture *##t; _TVARN(f, n, c, t, m)

