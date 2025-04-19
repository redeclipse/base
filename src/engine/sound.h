#ifndef STANDALONE
#include "soundenvprop.h"
#endif

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
    SND_PREVIEW  = 1<<14,   // sound is a preview
    SND_DUCKING  = 1<<15,   // sound is ducking other sounds
    SND_MASKF    = SND_LOOP|SND_MAP,
    SND_LAST     = 7        // top N are used for entities
};

#ifndef STANDALONE
#define SOUNDMINDIST        16.0f
#define SOUNDMAXDIST        10000.f

extern bool al_ext_efx, al_soft_spatialize, al_ext_float32;

extern bool nosound;
extern float soundmastervol, soundeffectvol, soundmusicvol, soundrefdist, soundrolloff;
extern int soundmusicfadein, soundmusicfadeout;

#define SOUND_FDRS 2
#define SOUND_MDRS 4
#define SOUND_EXTS 4

#ifdef CPP_ENGINE_SOUND
const char *sounddirs[SOUND_FDRS] = { "", "sounds/" },
           *musicdirs[SOUND_MDRS] = { "", "sounds/", "sounds/music/", "sounds/egmusic/" },
           *soundexts[SOUND_EXTS] = { "", ".ogg", ".flac", ".wav" };
#else
extern const char *sounddirs[SOUND_FDRS], *musicdirs[SOUND_MDRS], *soundexts[SOUND_EXTS];
#endif

#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"
#include "sndfile.h"

extern LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
extern LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
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

#define SOUNDERRORTRACK(body) \
    do { \
        ALenum err = alGetError(); \
        if(err != AL_NO_ERROR) { al_errfunc = __FUNCTION__; al_errfile = __FILE__; al_errline = __LINE__; body; } \
    } while(false);

#define SOUNDCHECKTRACK(expr, tbody, fbody) \
    do { \
        ALenum err = expr; \
        if(err == AL_NO_ERROR) { tbody; } \
        else { al_errfunc = __FUNCTION__; al_errfile = __FILE__; al_errline = __LINE__; fbody; } \
    } while(false);

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
    stream *viofile;

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
    float time;

    soundsample() : name(NULL) { reset(); }
    ~soundsample() { clear(); }

    ALenum setup(soundfile *s);
    void reset();
    void cleanup();
    void clear();
    bool valid();
};
extern hashnameset<soundsample> soundsamples;

struct soundslot
{
    vector<soundsample *> samples;
    char *name;
    int variants;
    float gain, pitch, rolloff, refdist, maxdist;

    soundslot() : name(NULL), variants(1), gain(1), pitch(1), rolloff(1), refdist(-1), maxdist(-1) {}
    ~soundslot() { DELETEA(name); }

    void reset();
};
extern Slotmanager<soundslot> gamesounds;
extern vector<soundslot> mapsounds;
typedef Slotmanager<soundslot>::Handle SoundHandle;

struct soundefxslot
{
    ALuint id;
    soundefxslot **hook;
    int lastused;

    soundefxslot() : id(-1), hook(NULL), lastused(0) {}

    void gen() { alGenAuxiliaryEffectSlots(1, &id); }
    void del() { alDeleteAuxiliaryEffectSlots(1, &id); id = AL_INVALID; }
    void put() { if(hook) *hook = NULL; hook = NULL; }
};

struct soundenv
{
    const char *name;
    property props[SOUNDENV_PROPS];

    soundenv() : name(NULL) {}

    const char *getname() const { return name ? name : ""; }
    void setparams(ALuint effect);
    void updatezoneparams(int envindex);
};
extern vector<Sharedptr<soundenv>> soundenvs;

struct soundenvzone
{
    entity *ent;
    Sharedptr<soundenv> env;
    ALuint effect;
    soundefxslot *efxslot;
    vec bbmin, bbmax;
    uchar fadevals[16];

    soundenvzone() : ent(NULL), effect(AL_INVALID), efxslot(NULL), fadevals{0} {}
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
    void refreshfroment();
    float getvolume();
    void updatepan();
    bool isvalid();
};

struct soundsource
{
    enum
    {
        SNDSRC_FILTER_DIRECT = 0,
        SNDSRC_FILTER_EFFECT,

        SNDSRC_NUM_FILTERS
    };

    ALuint source, dirfilter, efxfilter;
    soundslot *slot;
    vec pos, curpos, vel, *vpos;
    physent *owner;
    int index, flags, material, lastupdate, groupid;
    int millis, ends, slotnum, *hook;
    float gain, curgain, pitch, curpitch, rolloff, refdist, maxdist, offset;
    float finalrolloff, finalrefdist, fade;
    bool mute, ducking, occluded;
    vector<int> buffer;
    ident *cshook;

    soundsource() : vpos(NULL), index(-1), hook(NULL), cshook(NULL) { reset(); }
    ~soundsource() { clear(); }

    ALenum setup(soundsample *s);
    void cleanup();
    void reset(bool dohook = true);
    void clear(bool dohook = true);
    void unhook();
    ALenum updatefilter();
    ALenum update();
    bool valid();
    bool active();
    bool playing();
    ALenum play();
    ALenum pause();
    ALenum stop();
    ALenum push(soundsample *s);
    void setcshook(ident *id);
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
    bool looping, fadeout;
    int fademillis, fadetime;

    musicstream() { reset(); }
    ~musicstream() { clear(); }

    ALenum setup(const char *n, soundfile *s, bool looped = true, int fadems = 0, int fadet = 0);
    ALenum fill(ALint bufid);
    void cleanup();
    void reset();
    void clear();
    bool updategain();
    ALenum update();
    bool valid();
    bool active();
    bool playing();
    ALenum play();
    ALenum pause();
    ALenum stop();
    ALenum push(const char *n, soundfile *s);
};
extern musicstream *music;

#define issound(c) (soundsources.inrange(c) && soundsources[c].active())

extern void initmapsound();
extern void updateenvzone(entity *ent);
extern const char *sounderror(bool msg = true);
extern void mapsoundslots();
extern void mapsoundslot(int index, const char *name);
extern int getsoundslot(int index);
extern void initsound();
extern void stopsound();
extern int fademusic(int dir, bool fast = false);
extern bool playmusic(const char *name, bool looping = true);
extern bool playingmusic(bool active = true);
extern bool canplaymusic();
extern void smartmusic(int type = 0, bool init = false);
extern void stopmusic();
extern void updatesounds();
extern void clearsound();
extern int emitsound(int n, vec *pos, physent *d = NULL, int *hook = NULL, int flags = 0, float gain = 1, float pitch = 1, float rolloff = -1, float refdist = -1, float maxdist = -1, int ends = 0, float offset = 0, int groupid = 0);
extern int emitsoundpos(int n, const vec &pos, int *hook = NULL, int flags = 0, float gain = 1, float pitch = 1, float rolloff = -1, float refdist = -1, float maxdist = -1, int ends = 0, float offset = 0, int groupid = 0);
extern int playsound(int n, const vec &pos, physent *d = NULL, int flags = 0, int vol = -1, int maxrad = -1, int minrad = -1, int *hook = NULL, int ends = 0, float offset = 0, int groupid = 0);
extern void removetrackedsounds(physent *d);
extern void removemapsounds();
extern void dumpsoundenvs(stream *s);

extern bool musicinfo(char *title, char *artist, char *album, size_t len);

extern void initmumble();
extern void closemumble();
extern void updatemumble();
#endif
