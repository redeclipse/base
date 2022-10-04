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
    SND_NOQUIET  = 1<<5,    // disable water effects (panning only)
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
extern int mastervol, soundvol, musicvol, soundmono, soundmaxrad, soundminrad;
extern float soundevtvol, soundevtscale, soundenvvol, soundenvscale;

extern bool al_ext_efx, al_soft_spatialize;

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
	short *data;
	ALsizei len;
    ALenum format;
    SF_INFO info;
    sf_count_t frames;

    soundfile() : data(NULL), len(0), format(AL_NONE) {}
    ~soundfile() { if(data) delete[] data; }
};

struct soundsample
{
    char *name;
    ALuint buffer;

    soundsample() : name(NULL) { reset(); }
    ~soundsample() { DELETEA(name); }

    ALenum setup(soundfile *s)
    {
        SOUNDERROR();
        alGenBuffers(1, &buffer);
        SOUNDERROR(cleanup(); return err);
        alBufferData(buffer, s->format, s->data, s->len, s->info.samplerate);
        SOUNDERROR(cleanup(); return err);
        return AL_NO_ERROR;
    }

    void reset()
    {
        buffer = 0;
    }

    void cleanup()
    {
        if(valid()) alDeleteBuffers(1, &buffer);
        buffer = 0;
    }

    bool valid()
    {
        if(!buffer || !alIsBuffer(buffer)) return false;
        return true;
    }
};
extern hashnameset<soundsample> soundsamples;

struct soundslot
{
    vector<soundsample *> samples;
    int vol, maxrad, minrad, variants, fardistance;
    char *name;

    soundslot() : vol(255), maxrad(-1), minrad(-1), variants(1), fardistance(0), name(NULL) {}
    ~soundslot() { DELETEA(name); }

    void reset()
    {
        DELETEA(name);
        samples.shrink(0);
    }
};
extern slotmanager<soundslot> gamesounds, mapsounds;

struct sound
{
    ALuint source;
    soundslot *slot;
    vec pos, oldpos;
    physent *owner;
    int index, vol;
    int flags, material;
    int millis, ends, slotnum, *hook;
    float curvol, maxrad, minrad;
    vector<int> buffer;

    sound() : hook(NULL) { reset(); }
    ~sound() {}

    ALenum setup(soundsample *s)
    {
        if(!s->valid()) return AL_NO_ERROR;

        SOUNDERROR();
        alGenSources(1, &source);
        SOUNDERROR(clear(); return err);

        alSourcei(source, AL_BUFFER, s->buffer);
        SOUNDERROR(clear(); return err);

        if(al_soft_spatialize)
        {
            alSourcei(source, AL_SOURCE_SPATIALIZE_SOFT, AL_TRUE);
            SOUNDERROR(clear(); return err);
        }
        /*
        alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
        SOUNDERROR(clear(); return err);
        */
        alSourcei(source, AL_LOOPING, flags&SND_LOOP ? AL_TRUE : AL_FALSE);
        SOUNDERROR(clear(); return err);

        minrad = clamp(minrad, 1.f, maxrad); // avoid divide by zero in distance model
        alSourcef(source, AL_REFERENCE_DISTANCE, minrad);
        SOUNDERROR(clear(); return err);

        maxrad = max(maxrad, minrad);
        alSourcef(source, AL_MAX_DISTANCE, maxrad);
        SOUNDERROR(clear(); return err);

        alSourcef(source, AL_ROLLOFF_FACTOR, 1.f);
        SOUNDERROR(clear(); return err);

        /*
        alSourcef(source, AL_PITCH, 1.f);
        SOUNDERROR(clear(); return err);

        alSourcef(source, AL_MIN_GAIN, 0.f);
        SOUNDERROR(clear(); return err);

        alSourcef(source, AL_MAX_GAIN, 1.f);
        SOUNDERROR(clear(); return err);

        alSourcef(source, AL_GAIN, 1.f);
        SOUNDERROR(clear(); return err);
        */

        return AL_NO_ERROR;
    }

    void cleanup()
    {
        if(!valid()) return;
        alSourceStop(source);
        alDeleteSources(1, &source);
    }

    void reset()
    {
        source = 0;
        pos = oldpos = vec(-1, -1, -1);
        slot = NULL;
        owner = NULL;
        vol = 255;
        curvol = 1;
        material = MAT_AIR;
        flags = millis = ends = 0;
        maxrad = soundmaxrad;
        minrad = soundminrad;
        index = slotnum = -1;
        if(hook) *hook = -1;
        hook = NULL;
        buffer.shrink(0);
    }

    void clear()
    {
        cleanup();
        reset();
    }

    ALenum update()
    {
        //if(!valid()) return AL_NO_ERROR;
        /*
        float ovol = clamp(vol, 0, 255)/255.f, svol = flags&SND_CLAMPED ? 1.f : ovol;
        if(!(flags&SND_NOQUIET) && svol > 0 && (isliquid(lookupmaterial(pos)&MATF_VOLUME) || isliquid(lookupmaterial(camera1->o)&MATF_VOLUME))) svol *= 0.65f;
        if(flags&SND_CLAMPED) svol = max(svol, ovol);
        curvol = (soundvol/255.f)*(vol/255.f)*svol;

        SOUNDERROR();
        alSourcef(source, AL_GAIN, curvol);
        SOUNDERROR(clear(); return err);
        */

        SOUNDERROR();
        alSourcefv(source, AL_POSITION, (ALfloat *) &pos);
        SOUNDERROR(clear(); return err);

        alSource3f(source, AL_VELOCITY, 0.f, 0.f, 0.f);
        SOUNDERROR(clear(); return err);

        return AL_NO_ERROR;
    }

    bool valid()
    {
        if(!source || !alIsSource(source)) return false;
        return true;
    }

    bool active()
    {
        if(!valid()) return false;
        ALint value;
        alGetSourcei(source, AL_SOURCE_STATE, &value);
        if(value != AL_PLAYING && value != AL_PAUSED) return false;
        return true;
    }

    bool playing()
    {
        if(!valid()) return false;
        ALint value;
        alGetSourcei(source, AL_SOURCE_STATE, &value);
        if(value != AL_PLAYING) return false;
        return true;
    }

    ALenum play()
    {
        if(playing()) return AL_NO_ERROR;
        SOUNDERROR();
        alSourcePlay(source);
        SOUNDERROR(clear(); return err);
        return AL_NO_ERROR;
    }

    ALenum push(soundsample *s)
    {
        SOUNDCHECK(setup(s), , return err);
        SOUNDCHECK(update(), , return err);
        SOUNDCHECK(play(), , return err);
        return AL_NO_ERROR;
    }
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
