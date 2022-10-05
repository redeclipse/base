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
    SND_NOQUIET  = 1<<5,    // disable water effects
    SND_CLAMPED  = 1<<6,    // makes volume the minimum volume to clamp to
    SND_LOOP     = 1<<7,    // loops when it reaches the end
    SND_BUFFER   = 1<<8,    // source becomes/adds to a buffer for sounds
    SND_MAP      = 1<<9,    // sound created by map
    SND_UNMAPPED = 1<<10,   // skip slot index mapping
    SND_IMPORT   = SND_NODELAY|SND_PRIORITY|SND_NOQUIET,
    SND_FORCED   = SND_IMPORT|SND_NOATTEN|SND_NODIST,
    SND_DIRECT   = SND_IMPORT|SND_CLAMPED,
    SND_MASKF    = SND_LOOP|SND_MAP,
    SND_LAST     = 8        // top N are used for entities
};

#ifndef STANDALONE
#define SOUNDMINDIST        16.0f
#define SOUNDMAXDIST        10000.f

extern bool nosound;
extern int mastervol, soundvol, musicvol;

#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"
#include "sndfile.h"

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

struct soundfile
{
	enum { SHORT = 0, FLOAT, MAX };
    int type;
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

    soundfile(bool f) : len(0), format(AL_NONE)
    {
        if(f)
        {
            type = FLOAT;
            size = sizeof(float);
            data_f = NULL;
        }
        else
        {
            type = SHORT;
            size = sizeof(short);
            data_s = NULL;
        }

    }
    ~soundfile() { clear(); }

    void clear()
    {
        switch(type)
        {
            case SHORT: if(data_s) delete[] data_s; break;
            case FLOAT: if(data_f) delete[] data_f; break;
            default: break;
        }
    }
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
extern hashnameset<soundsample> soundsamples;

struct soundslot
{
    vector<soundsample *> samples;
    int vol, maxrad, minrad, variants, fardistance;
    char *name;

    soundslot() : vol(255), maxrad(-1), minrad(-1), variants(1), fardistance(0), name(NULL) {}
    ~soundslot() { DELETEA(name); }

    void reset();
};
extern slotmanager<soundslot> gamesounds, mapsounds;

struct sound
{
    ALuint source;
    soundslot *slot;
    vec pos, vel;
    physent *owner;
    int index, vol;
    int flags, material;
    int millis, ends, slotnum, *hook;
    float curvol, maxrad, minrad;
    vector<int> buffer;

    sound() : hook(NULL) { reset(); }
    ~sound() {}

    ALenum setup(soundsample *s);
    void cleanup();
    void reset();
    void clear();
    ALenum update();
    bool valid();
    bool active();
    bool playing();
    ALenum play();
    ALenum push(soundsample *s);
};
extern vector<sound> sounds;

#define issound(c) (sounds.inrange(c) && sounds[c].valid())

extern const char *sounderror(bool msg = true);
extern void mapsoundslots();
extern void mapsoundslot(int index, const char *name);
extern int getsoundslot(int index);
extern void initsound();
extern void stopsound();
extern bool playmusic(const char *name, const char *cmd = NULL);
extern bool playingmusic(bool check = true);
extern void smartmusic(bool cond, bool init = false);
extern void musicdone(bool docmd);
extern void updatesounds();
extern int addsound(const char *id, const char *name, int vol, int maxrad, int minrad, int value, slotmanager<soundslot> &soundset);
extern void clearsound();
extern int playsound(int n, const vec &pos, physent *d = NULL, int flags = 0, int vol = -1, int maxrad = -1, int minrad = -1, int *hook = NULL, int ends = 0);
extern void removetrackedsounds(physent *d);
extern void removemapsounds();

extern void initmumble();
extern void closemumble();
extern void updatemumble();
#endif
