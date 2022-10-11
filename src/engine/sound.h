#include "soundenvprop.h"

enum
{
    S_PRESS = 0, S_BACK, S_ACTION, S_NUM_GENERIC,
    S_GAMESPECIFIC = S_NUM_GENERIC
};

enum
{
    SND_NONE     = 0,
    SND_NOATTEN  = 1<<0,    // disable attenuation
    SND_NODELAY  = 1<<1,    // disable delay
    SND_PRIORITY = 1<<2,    // high priority
    SND_NOPAN    = 1<<3,    // disable panning (distance only attenuation)
    SND_NODIST   = 1<<4,    // disable distance (panning only)
    SND_NOENV    = 1<<5,    // disable environmental effects
    SND_CLAMPED  = 1<<6,    // makes volume the minimum volume to clamp to
    SND_LOOP     = 1<<7,    // loops when it reaches the end
    SND_BUFFER   = 1<<8,    // source becomes/adds to a buffer for sounds
    SND_MAP      = 1<<9,    // sound created by map
    SND_UNMAPPED = 1<<10,   // skip slot index mapping
    SND_TRACKED  = 1<<11,   // sound vpos is tracked
    SND_VELEST   = 1<<12,   // sound vpos is estimated
    SND_NOFILTER = 1<<13,   // disable filtering
    SND_HAPTICS  = 1<<14,   // send to haptics device
    SND_MASKF    = SND_LOOP|SND_MAP,
    SND_LAST     = 8        // top N are used for entities
};

#ifndef STANDALONE
#define SOUNDMINDIST        16.0f
#define SOUNDMAXDIST        10000.f

extern bool nosound, al_ext_efx, al_soft_spatialize, al_ext_float32;
extern float soundmastervol, soundeffectvol, soundmusicvol, soundrefdist, soundrolloff;

#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"
#include "sndfile.h"

extern LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
extern LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
extern LPALISAUXILIARYEFFECTSLOT alIsAuxiliaryEffectSlot;
extern LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
extern LPALGENEFFECTS alGenEffects;
extern LPALDELETEEFFECTS alDeleteEffects;
extern LPALISEFFECT alIsEffect;
extern LPALEFFECTI alEffecti;
extern LPALEFFECTF alEffectf;
extern LPALEFFECTFV alEffectfv;
extern LPALGENFILTERS alGenFilters;
extern LPALDELETEFILTERS alDeleteFilters;
extern LPALISFILTER alIsFilter;
extern LPALFILTERI alFilteri;
extern LPALFILTERF alFilterf;

#define SOUNDERROR(body) \
    do { \
        ALenum err = alGetError(); \
        if(err != AL_NO_ERROR) { body; } \
    } while(false);

#define SOUNDCHECK(expr, tbody, fbody) \
    do { \
        ALenum err = expr; \
        if(err == AL_NO_ERROR) { tbody; } \
        else { fbody; } \
    } while(false);

struct sounddevice;

struct soundfile
{
	enum { SHORT = 0, FLOAT, INVALID };
    enum { MONO = 0, SPATIAL, MUSIC, MAXMIX };
    int type, mixtype;
    union
    {
        short *data_s;
        float *data_f;
    };
	ALsizei len;
    ALenum format;
    SF_INFO info;
    sf_count_t frames;
    size_t size;
    SNDFILE *sndfile;

    soundfile() { reset(); }
    ~soundfile() { clear(); }

    bool setup(const char *name, int t, int m);
    bool setupmus();
    bool fillmus(bool retry = false);
    bool setupsnd();
    void reset();
    void clear();
};

struct soundsample
{
    char *name;
    ALuint buffer;

    soundsample() : name(NULL) { reset(); }
    ~soundsample() { DELETEA(name); }

    ALenum setup(soundfile *s);
    void reset();
    void cleanup();
    bool valid();
};

struct soundslot
{
    vector<soundsample *> samples;
    char *name;
    int variants;
    float gain, pitch, rolloff, refdist, maxdist, fardist;

    soundslot() : name(NULL), variants(1), gain(1), pitch(1), rolloff(1), refdist(-1), maxdist(-1), fardist(-1) {}
    ~soundslot() { DELETEA(name); }

    void reset();
};

struct soundefxslot
{
    ALuint id;
    soundefxslot **hook;
    int lastused;

    soundefxslot() : id(-1), hook(NULL), lastused(0) {}

    bool gen()
    {
        alGenAuxiliaryEffectSlots(1, &id);
        return alIsAuxiliaryEffectSlot(id);
    }

    void del()
    {
        if(alIsAuxiliaryEffectSlot(id)) alDeleteAuxiliaryEffectSlots(1, &id);
        id = AL_INVALID;
    }

    void put() { if(hook) *hook = NULL; hook = NULL; }
};

struct soundenv
{
    const char *name;
    property props[SOUNDENV_PROPS];

    soundenv() : name(NULL) {}

    const char *getname() const { return name ? name : ""; }
    void setparams(ALuint effect);
    void updatezoneparams();
};
extern slotmanager<soundenv> soundenvs;

struct soundenvzone
{
    entity *ent;
    soundenv *env;
    ALuint effect;
    soundefxslot *efxslot;
    vec bbmin, bbmax;

    soundenvzone() : ent(NULL), env(NULL), effect(AL_INVALID), efxslot(NULL) {}
    ~soundenvzone()
    {
        if(!al_ext_efx) return;

        if(efxslot) efxslot->put();
        if(alIsEffect(effect)) alDeleteEffects(1, &effect);
        effect = AL_INVALID;
    }

    void attachparams();
    ALuint getefxslot();
    void froment(entity *newent);
    int getvolume();
    void updatepan();
    bool isvalid();
};

struct soundoutput
{
    ALuint source, filter;
    soundslot *slot;
    float curgain, curpitch, finalrolloff, finalrefdist;
    sounddevice *device;

    soundoutput() { reset(); }
    ~soundoutput() { cleanup(); }

    ALenum setup(sounddevice *dev, soundsample *s, soundslot *t, int flags = 0, float rolloff = -1, float refdist = -1, float maxdist = -1);
    void reset();
    void cleanup();
    void clear();
    ALenum update(const vec &pos, const vec &vel = vec(0, 0, 0), int flags = 0, float gain = 1, float pitch = 1);
    bool valid();
    bool active();
    bool playing();
    ALenum play();
    ALenum updatefilter(const vec &pos);
};

struct soundsource
{
    vec pos, curpos, vel, *vpos;
    physent *owner;
    int index, flags, slotnum, material, lastupdate, millis, ends, *hook;
    float gain, pitch, rolloff, refdist, maxdist;
    vector<soundoutput *> outputs;
    vector<int> buffer;

    soundsource() : vpos(NULL), hook(NULL) { reset(); }
    ~soundsource() { clear(); }

    soundoutput *findoutput(sounddevice *dev);
    ALenum setup(sounddevice *dev, soundsample *s, soundslot *t);
    void cleanup(sounddevice *dev = NULL);
    void reset();
    void clear();
    void unhook();
    ALenum update();
    bool valid(sounddevice *dev = NULL);
    bool active(sounddevice *dev = NULL);
    bool playing(sounddevice *dev = NULL);
    ALenum play(sounddevice *dev = NULL);
    void start();
};
extern vector<soundsource> soundsources;

#define MUSICBUFS 4
#define MUSICSAMP 8192
struct musicstream
{
    char *name, *artist, *title, *album;
    ALuint source;
    ALuint buffer[MUSICBUFS];
    soundfile *data;
    float gain;
    bool looping;

    musicstream() { reset(); }
    ~musicstream() { clear(); }

    ALenum setup(const char *n, soundfile *s);
    ALenum fill(ALint bufid);
    void cleanup();
    void reset();
    void clear();
    ALenum update();
    bool valid();
    bool active();
    bool playing();
    ALenum play();
    ALenum push(const char *n, soundfile *s);
};

struct sounddevice
{
    enum { AUDIO, HAPTICS, MAX };

    char *name;
    ALCdevice *device;
    ALCcontext *context;

    int type;
    bool has_soft_spatialize, has_ext_float32, has_ext_efx, efx_inited;

    hashnameset<soundsample> soundsamples;
    slotmanager<soundslot> gamesounds, mapsounds;
    vector<soundefxslot> soundefxslots;

    musicstream *music;
    SDL_Thread *music_thread;
    SDL_mutex *music_mutex;

    sounddevice() : efx_inited(false) { reset(); }
    ~sounddevice() { destroy(); }

    const char *gettype()
    {
        const char *typenames[MAX] = { "Sound", "Haptic" };
        return typenames[type];
    }

    void allocsoundefxslots();
    void putsoundefxslots();
    void delsoundfxslots();
    void getsoundefxslot(soundefxslot **hook, bool priority = false);
    bool cansoundefx() const { return efx_inited; }

    bool setup(const char *s, int t, bool fallback = false);
    void destroy();
    void reset();
    void current();
    void suspend();
    void process();
    void push();

    void updatemusic();
    void musicloopinit();
    void musicloopstop();
    void stopmusic();
    bool playmusic(const char *name, bool looping = true);
    bool playingmusic();
    void lockmusic();
    void unlockmusic();

    void setdoppler(float v);
    void setspeed(float v);
};
extern vector<sounddevice *> sounddevices;

#define issound(c) (soundsources.inrange(c) && soundsources[c].active())

extern void buildenvzones();
extern void updateenvzone(entity *ent);
extern const char *sounderror(bool msg = true);
extern void mapsoundslots();
extern void mapsoundslot(int index, const char *name);
extern int getsoundslot(int index);
extern void initsound();
extern void stopsound();
extern int playmusic(const char *name, bool looping = true);
extern bool playingmusic();
extern void smartmusic(bool cond, bool init = false);
extern void stopmusic();
extern void updatemusic();
extern void updatesounds();
extern int addsound(const char *id, const char *name, float gain, float pitch, float rolloff, float refdist, float maxdist, int variants, float fardist, bool game);
extern void clearsound();
extern int emitsound(int n, vec *pos, physent *d = NULL, int *hook = NULL, int flags = 0, float gain = 1, float pitch = 1, float rolloff = -1, float refdist = -1, float maxdist = -1, int ends = 0);
extern int emitsoundpos(int n, const vec &pos, int *hook = NULL, int flags = 0, float gain = 1, float pitch = 1, float rolloff = -1, float refdist = -1, float maxdist = -1, int ends = 0);
extern int playsound(int n, const vec &pos, physent *d = NULL, int flags = 0, int vol = -1, int maxrad = -1, int minrad = -1, int *hook = NULL, int ends = 0);
extern void removetrackedsounds(physent *d);
extern void removemapsounds();
extern void dumpsoundenvs(stream *s);

extern void initmumble();
extern void closemumble();
extern void updatemumble();
#endif
