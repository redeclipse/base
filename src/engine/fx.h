// Workaround for header mess:
#ifndef soundslot
    struct soundslot;
    typedef Slotmanager<soundslot>::Handle SoundHandle;
#endif

namespace fx
{
    enum
    {
        FX_TYPE_PARTICLE = 0,
        FX_TYPE_LIGHT,
        FX_TYPE_SOUND,
        FX_TYPE_WIND,
        FX_TYPE_STAIN,

        FX_TYPES
    };

    enum
    {
        FX_POSCALC_NONE = 0,
        FX_POSCALC_TIP,
        FX_POSCALC_ENTTAG,
        FX_POSCALC_ENTPOS
    };

    enum
    {
        FX_COLORIZE_DISABLED = 0,
        FX_COLORIZE_PARAM,
        FX_COLORIZE_PALETTE
    };

    enum
    {
        FX_PROP_ACTIVE_LENGTH = 0,
        FX_PROP_EMIT_LENGTH,
        FX_PROP_EMIT_INTERVAL,
        FX_PROP_EMIT_DELAY,
        FX_PROP_EMIT_PARENT,
        FX_PROP_EMIT_TIMELINESS,
        FX_PROP_EMIT_SINGLE,
        FX_PROP_EMIT_RESTART,
        FX_PROP_EMIT_MOVE,
        FX_PROP_EMIT_PARAM,
        FX_PROP_EMIT_DIST,
        FX_PROP_EMIT_CULL,
        FX_PROP_FADEIN,
        FX_PROP_FADEOUT,
        FX_PROP_BLEND,
        FX_PROP_SCALE,
        FX_PROP_COLORIZED,
        FX_PROP_PALETTE,
        FX_RPOP_REL_OFFSET,
        FX_PROP_POS_OFFSET,
        FX_PROP_END_OFFSET,
        FX_PROP_END_FROM_POS,
        FX_PROP_POS_FROM_END,
        FX_PROP_POS_FLIP,
        FX_PROP_END_FROM_PREV,
        FX_RPOP_POS_FROM_ENTTAG,
        FX_PROP_END_FROM_ENTTAG,
        FX_PROP_POS_FROM_ENTPOS,
        FX_PROP_END_FROM_ENTPOS,
        FX_PROP_ITER,
        FX_PROP_ITER_OFFSET,
        FX_PROP_WEAPON,

        FX_STD_PROPS
    };

    enum
    {
        FX_PART_TYPE = 0,
        FX_PART_PARTICLE,
        FX_PART_NUM,
        FX_PART_SHAPE,
        FX_PART_COLOR,
        FX_PART_ENVCOLOR,
        FX_PART_ENVBLEND,
        FX_PART_FADE,
        FX_PART_COLLIDE,
        FX_PART_REGDELAY,
        FX_PART_SHAPESIZE,
        FX_PART_PARTSIZE,
        FX_PART_PARTSIZECHANGE,
        FX_PART_MAXPARTSIZE,
        FX_PART_VEL,
        FX_PART_GRAVITY,
        FX_PART_TEXT,
        FX_PART_TRACK,
        FX_PART_HINTCOLOR,
        FX_PART_HINTBLEND,

        FX_PART_PROPS
    };

    enum
    {
        FX_PART_TYPE_SINGLE = 0,
        FX_PART_TYPE_SPLASH,
        FX_PART_TYPE_SHAPE,
        FX_PART_TYPE_FLARE,
        FX_PART_TYPE_TRAIL,
        FX_PART_TYPE_EXPLODE,
        FX_PART_TYPE_TEXT,

        FX_PART_TYPES
    };

    enum
    {
        FX_LIGHT_RADIUS = 0,
        FX_LIGHT_COLOR,
        FX_LIGHT_FLAGS,
        FX_LIGHT_SPOT,
        FX_LIGHT_GAIN,

        FX_LIGHT_PROPS
    };

    enum
    {
        FX_SOUND_SOUND = 0,
        FX_SOUND_WEAPONSOUND,
        FX_SOUND_GAIN,
        FX_SOUND_PITCH,
        FX_SOUND_ROLLOFF,
        FX_SOUND_REFDIST,
        FX_SOUND_MAXDIST,
        FX_SOUND_FLAGS,
        FX_SOUND_ONPLAYER,

        FX_SOUND_PROPS
    };

    enum
    {
        FX_WIND_MODE = 0,
        FX_WIND_SPEED,
        FX_WIND_INTERVAL,
        FX_WIND_CYCLELEN,
        FX_WIND_RADIUS,
        FX_WIND_ATTEN,
        FX_WIND_YAW,

        FX_WIND_PROPS
    };

    enum
    {
        FX_STAIN_TYPE = 0,
        FX_STAIN_RADIUS,
        FX_STAIN_COLOR,
        FX_STAIN_ENVCOLOR,
        FX_STAIN_ENVBLEND,

        FX_STAIN_PROPS
    };

    enum
    {
        FX_MOD_RAND = 0,
        FX_MOD_LERP,
        FX_MOD_PARAM,

        FX_MODS,

        FX_MOD_FLAG_LERP360 = FX_MODS
    };

    enum
    {
        FX_MOD_LERP_PROP_TIME = 0,
        FX_MOD_LERP_PROP_MODE,
        FX_MOD_LERP_PROP_PARAM,
        FX_MOD_LERP_PROP_SHAPE,
        FX_MOD_LERP_PROP_SCALEMIN,
        FX_MOD_LERP_PROP_SCALEMAX,

        FX_MOD_LERP_PROPS
    };

    enum
    {
        FX_MOD_LERP_ACTIVE = 0, // lerp since emitter activation
        FX_MOD_LERP_EMIT,       // lerp since emit begin
        FX_MOD_LERP_PARAM,      // param based lerp
        FX_MOD_LERP_ITER,       // iteration based lerp
        FX_MOD_LERP_CAMFACING,  // lerp based on inverse dir dot product with camera
        FX_MOD_LERP_SPEED,      // lerp based on speed

        FX_MOD_LERP_MODES
    };

    enum
    {
        FX_MOD_LERP_SHAPE_LINEAR = 0,
        FX_MOD_LERP_SHAPE_SQUARE_IN,
        FX_MOD_LERP_SHAPE_SQUARE_OUT,
        FX_MOD_LERP_SHAPE_SMOOTH,

        FX_MOD_LERP_SHAPES
    };

    enum
    {
        FX_MOD_PARAM_PROP_MODE = 0,
        FX_MOD_PARAM_PROP_SCALE,

        FX_MOD_PARAM_PROPS
    };

    enum
    {
        FX_MOD_PARAM_ADD = 0,
        FX_MOD_PARAM_MUL,

        FX_MOD_PARAM_MODES
    };

    #define FX_PARAMS 4

    #define FX_EXT_PROPS 20
    #define FX_EXT_PROP(idx) (idx + FX_STD_PROPS)

    #define FX_TOTAL_PROPS (FX_STD_PROPS + FX_EXT_PROPS)

    #define FX_ITER_MAX 8

    struct propmodlerp;
    struct propmodparam;
    struct fxdef;
    struct instance;
    struct emitter;

    struct fxpropertydef : propertydef
    {
        int modflags;

        template<class T> fxpropertydef(const char *_name, int _type, T _minval, T _defval,
                                        T _maxval, int _modflags) :
                                        propertydef(_name, _type, _minval, _defval, _maxval),
                                        modflags(_modflags) {}
    };

    struct fxproperty : property
    {
        fxproperty *rand;
        propmodlerp *lerp;
        propmodparam *param;

        void (*calcmodifiers)(instance &inst, fxproperty &prop, void *value);

        fxproperty() : rand(NULL), lerp(NULL), param(NULL), calcmodifiers(NULL) {}
        virtual ~fxproperty();

        const fxpropertydef *getdef() const { return (fxpropertydef *)def; }

        virtual void pack(vector<uchar> &buf) const;
        virtual int unpack(uchar *buf, size_t bufsize);
    };

    struct propmodlerp : fxproperty
    {
        property props[FX_MOD_LERP_PROPS];

        virtual void pack(vector<uchar> &buf) const;
        virtual int unpack(uchar *buf, size_t bufsize);
    };

    struct propmodparam : property
    {
        property props[FX_MOD_PARAM_PROPS];

        virtual void pack(vector<uchar> &buf) const;
        virtual int unpack(uchar *buf, size_t bufsize);
    };

    struct fxdef;
    typedef Slotmanager<fxdef>::Handle FxHandle;

    extern FxHandle getfxhandle(const char *name);
    extern bool hasfx(const char *name);

    struct fxdef
    {
        const char *name;
        int type;
        fxproperty props[FX_TOTAL_PROPS];
        vector<FxHandle> children;
        SoundHandle sound;
        FxHandle endfx;

        const char *getname() const { return name ? name : ""; }
        fxproperty *getextprops() { return &props[FX_STD_PROPS]; }
    };

    struct instance
    {
        instance *prev, *next; // linked list for chained FX and tracking unused instances
        instance *parent;
        emitter *e;
        FxHandle fxhandle;
        int beginmillis, endmillis, activeendmillis, curiter, iters;
        bool sync, emitted, canparttrack;
        vec from, to, prevfrom[FX_ITER_MAX];

        union
        {
            int soundhook;
            windemitter *windhook;
        };

        void reset(bool initialize = false);
        void setflags();
        void init(emitter *em, FxHandle newhandle, instance *prnt);
        void calcactiveend();
        void calcend(int from);
        void prolong();
        void schedule(bool resync);
        void calcpos();
        bool checkconditions();
        void update();
        void emitfx();
        void stop();

        float getscale();

        template<class T> T getprop(int propindex)
        {
            fxdef &def = fxhandle.get();
            fxproperty &prop = def.props[propindex];
            T ret = prop;
            if(prop.calcmodifiers) prop.calcmodifiers(*this, prop, &ret);
            return ret;
        }

        template<class T> T getextprop(int propindex) { return getprop<T>(FX_EXT_PROP(propindex)); }
    };

    struct emitter
    {
        enum
        {
            CALC_CAMDIST = 1 << 0,
            CALC_MOVED   = 1 << 1,
            CALC_CULL    = 1 << 2,
        };

        emitter *prev, *next; // linked list for tracking unused emitters
        instance *firstfx, *lastfx;
        int beginmillis, endmillis;
        vec rand;

        emitter **hook;
        vec from, to, prevfrom;
        bvec color;
        physent *pl;
        float blend, scale, moved, camdist, cullradius;
        float params[FX_PARAMS];
        int flags;
        bool cull;

        emitter() : hook(NULL) {}

        void calcrandom();
        void unhook();
        void updateend(int end);
        void init(emitter **newhook);
        bool instantiate(FxHandle handle, instance *parent = NULL);
        void prolong();
        bool done();
        void update();
        void stop();
        bool isvalid();
        void setflag(int flag);
        void updatecullradius(float radius);

        // Setters
        emitter &setfrom(const vec &newfrom);
        emitter &setto(const vec &newto);
        emitter &setcolor(const bvec &newcolor);
        emitter &setblend(float newblend);
        emitter &setscale(float newscale);
        emitter &setentity(physent *newpl);
        emitter &setparam(int index, float param);
    };

    extern void startframe();
    extern void update();
    extern void stopfx(emitter *e);
    extern emitter &createfx(FxHandle fxhandle, emitter **hook = NULL);
    extern void clear();
    extern void cleanup();
    extern void setup();
    extern void removetracked(physent *pl);

    extern int fxdebug;
}
