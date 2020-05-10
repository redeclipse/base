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
        FX_PROP_ACTIVE_LENGTH = 0,
        FX_PROP_EMIT_LENGTH,
        FX_PROP_EMIT_INTERVAL,
        FX_PROP_EMIT_DELAY,
        FX_PROP_EMIT_PARENT,
        FX_PROP_EMIT_TIMELINESS,
        FX_PROP_EMIT_SINGLE,
        FX_PROP_EMIT_RESTART,
        FX_PROP_EMIT_MOVE,
        FX_PROP_FADEIN,
        FX_PROP_FADEOUT,
        FX_PROP_BLEND,
        FX_PROP_SCALE,
        FX_PROP_COLORIZED,
        FX_RPOP_REL_OFFSET,
        FX_PROP_POS_OFFSET,
        FX_PROP_END_OFFSET,
        FX_PROP_END_FROM_POS,
        FX_PROP_POS_FROM_END,
        FX_PROP_POS_FLIP,
        FX_PROP_END_FROM_PREV,

        FX_STD_PROPS
    };

    enum
    {
        FX_PART_TYPE = 0,
        FX_PART_PARTICLE,
        FX_PART_NUM,
        FX_PART_SHAPE,
        FX_PART_COLOR,
        FX_PART_FADE,
        FX_PART_COLLIDE,
        FX_PART_REGDELAY,
        FX_PART_SHAPESIZE,
        FX_PART_PARTSIZE,
        FX_PART_MAXPARTSIZE,
        FX_PART_VEL,
        FX_PART_GRAVITY,
        FX_PART_TEXT,
        FX_PART_TRACK,

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

        FX_LIGHT_PROPS
    };

    enum
    {
        FX_SOUND_SOUND = 0,
        FX_SOUND_VOL,
        FX_SOUND_FLAGS,
        FX_SOUND_MINRAD,
        FX_SOUND_MAXRAD,

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

        FX_STAIN_PROPS
    };

    enum
    {
        FX_MOD_RAND = 0,
        FX_MOD_LERP,

        FX_MODS,

        FX_MOD_FLAG_LERP360 = FX_MODS
    };

    enum
    {
        FX_MOD_LERP_PROP_TIME = 0,
        FX_MOD_LERP_PROP_MODE,
        FX_MOD_LERP_PROP_PARAM,
        FX_MOD_LERP_PROP_SQUARE,

        FX_MOD_LERP_PROPS
    };

    enum
    {
        FX_MOD_LERP_ACTIVE = 0, // lerp since emitter activation
        FX_MOD_LERP_EMIT,       // lerp since emit begin
        FX_MOD_LERP_PARAM,      // param based lerp

        FX_MOD_LERP_MODES
    };

    #define FX_PARAMS 2

    #define FX_EXT_PROPS 15
    #define FX_EXT_PROP(idx) (idx + FX_STD_PROPS)

    #define FX_TOTAL_PROPS (FX_STD_PROPS + FX_EXT_PROPS)

    struct propmodlerp;
    struct fxdef;
    struct instance;
    struct emitter;

    bool isfx(int index);
    fxdef &getfxdef(int index);
    int getfxindex(const char *name);

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

        void (*calcmodifiers)(instance &inst, fxproperty &prop, void *value);

        fxproperty() : rand(NULL), lerp(NULL), calcmodifiers(NULL) {}
        virtual ~fxproperty();

        const fxpropertydef *getdef() const { return (fxpropertydef *)def; }
    };

    struct propmodlerp
    {
        fxproperty lerp;
        property props[FX_MOD_LERP_PROPS];
    };

    struct fxdef
    {
        const char *name;
        int type;
        fxproperty props[FX_TOTAL_PROPS];
        vector<int> children;
        slot *endfx, *sound;

        const char *getname() const { return name ? name : ""; }
        fxproperty *getextprops() { return &props[FX_STD_PROPS]; }
    };

    struct instance
    {
        instance *prev, *next; // linked list for chained FX and tracking unused instances
        instance *parent;
        emitter *e;
        int fxindex;
        int beginmillis, endmillis, activeendmillis;
        bool sync, emitted;

        union
        {
            int soundhook;
            windemitter *windhook;
        };

        void reset(bool initialize = false);
        void init(emitter *em, int index, instance *prnt);
        void calcactiveend();
        void calcend(int from);
        void prolong();
        void schedule(bool resync);
        bool checkconditions();
        void update();
        void emitfx();
        void stop();

        template<class T> T getprop(int propindex)
        {
            fxproperty &prop = getfxdef(fxindex).props[propindex];
            T ret = prop;
            if(prop.calcmodifiers) prop.calcmodifiers(*this, prop, &ret);
            return ret;
        }

        template<class T> T getextprop(int propindex) { return getprop<T>(FX_EXT_PROP(propindex)); }
    };

    struct emitter
    {
        emitter *prev, *next; // linked list for tracking unused emitters
        instance *firstfx, *lastfx;
        int beginmillis, endmillis;
        vec rand;

        emitter **hook;
        vec from, to, prevfrom;
        bvec color;
        physent *pl;
        float blend, scale;
        float params[FX_PARAMS];

        emitter() : hook(NULL) {}

        void calcrandom();
        void unhook();
        void updateend(int end);
        void init(emitter **newhook);
        void instantiate(int index, instance *parent = NULL);
        void prolong();
        bool done();
        void update();
        void stop();

        emitter &setparam(int index, float param)
        {
            ASSERT(index >= 0 && index < FX_PARAMS);
            params[index] = param;
            return *this;
        }
    };

    extern void update();
    extern void stopfx(emitter *e);
    extern slot *getfxslot(const char *name);
    extern emitter *createfx(int index, const vec &from, const vec &to, float blend = 1.0f,
        float scale = 1.0f, const bvec &color = bvec(255, 255, 255), physent *pl = NULL,
        emitter **hook = NULL);
    extern void clear();
    extern void cleanup();
    extern void setup();

    extern int fxdebug;
}
