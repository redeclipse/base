enum
{
    // 0 is a constant localized wind source
    WIND_EMIT_IMPULSE  = 1,
    WIND_EMIT_VECTORED = 1 << 1
};

struct windattrs
{
    vec o;
    int mode;
    int yaw;
    float speed;
    int radius;
    float atten;
    int interval;
    int length;
};

struct windemitter
{
    extentity *ent; // Wind entity
    int entindex;
    windattrs attrs;
    windemitter **hook; // Hook pointing to this instance
    float curspeed;
    int lastimpulse;
    bool unused;

    windemitter(extentity *e = NULL);
    virtual ~windemitter();

    const vec &getwindpos();
    int getwindmode();
    int getwindyaw();
    float getwindspeed();
    int getwindradius();
    int getwindatten();
    int getwindinterval();
    int getwindlen();
    void updateimpulse();
    void updatewind();
    void update();
};

// windprobes used for large-scale wind sampling (e.g. particles)
struct windprobe
{
    int nextprobe;
    vec lastwind;

    void reset();
    vec probe(const vec &o, const dynent *d = NULL);
};

extern int windanimfalloff;

extern int getwindanimdist();
extern void cleanupwind();
extern void setupwind();
extern void clearwindemitters();
extern void updatewind();
extern vec getwind(const vec &o, const dynent *d = NULL);
extern void addwind(const vec &o, int mode, float speed, windemitter **hook = NULL, int yaw = 0, int interval = 0, int length = 4000, int radius = 0, float atten = 0.1f);
extern void remwind(windemitter **we);
extern void addwind(extentity *e);
extern void remwind(extentity *e);
