#define CPP_ENGINE_SOUND 1
#include "engine.h"

ALCdevice *snddev = NULL;
ALCcontext *sndctx = NULL;

bool nosound = true, canmusic = false;
bool al_ext_efx = false, al_soft_spatialize = false, al_ext_float32 = false;
ALCint al_sample_rate = ALC_INVALID;
const char *al_errfunc = NULL, *al_errfile = NULL;
uint al_errline = 0;
static int mapsoundmillis;

LPALGENAUXILIARYEFFECTSLOTS    alGenAuxiliaryEffectSlots    = NULL;
LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots = NULL;
LPALAUXILIARYEFFECTSLOTI       alAuxiliaryEffectSloti       = NULL;
LPALGENEFFECTS                 alGenEffects                 = NULL;
LPALDELETEEFFECTS              alDeleteEffects              = NULL;
LPALISEFFECT                   alIsEffect                   = NULL;
LPALEFFECTI                    alEffecti                    = NULL;
LPALEFFECTF                    alEffectf                    = NULL;
LPALEFFECTFV                   alEffectfv                   = NULL;
LPALGENFILTERS                 alGenFilters                 = NULL;
LPALDELETEFILTERS              alDeleteFilters              = NULL;
LPALISFILTER                   alIsFilter                   = NULL;
LPALFILTERI                    alFilteri                    = NULL;
LPALFILTERF                    alFilterf                    = NULL;

static void getextsoundprocs()
{
    if(!al_ext_efx) return;

    alGenAuxiliaryEffectSlots    = (LPALGENAUXILIARYEFFECTSLOTS)   alGetProcAddress("alGenAuxiliaryEffectSlots");
    alDeleteAuxiliaryEffectSlots = (LPALDELETEAUXILIARYEFFECTSLOTS)alGetProcAddress("alDeleteAuxiliaryEffectSlots");
    alAuxiliaryEffectSloti       = (LPALAUXILIARYEFFECTSLOTI)      alGetProcAddress("alAuxiliaryEffectSloti");
    alGenEffects                 = (LPALGENEFFECTS)                alGetProcAddress("alGenEffects");
    alDeleteEffects              = (LPALDELETEEFFECTS)             alGetProcAddress("alDeleteEffects");
    alIsEffect                   = (LPALISEFFECT)                  alGetProcAddress("alIsEffect");
    alEffecti                    = (LPALEFFECTI)                   alGetProcAddress("alEffecti");
    alEffectf                    = (LPALEFFECTF)                   alGetProcAddress("alEffectf");
    alEffectfv                   = (LPALEFFECTFV)                  alGetProcAddress("alEffectfv");
    alGenFilters                 = (LPALGENFILTERS)                alGetProcAddress("alGenFilters");
    alDeleteFilters              = (LPALDELETEFILTERS)             alGetProcAddress("alDeleteFilters");
    alIsFilter                   = (LPALISFILTER)                  alGetProcAddress("alIsFilter");
    alFilteri                    = (LPALFILTERI)                   alGetProcAddress("alFilteri");
    alFilterf                    = (LPALFILTERF)                   alGetProcAddress("alFilterf");
}

soundslot previewslot;
hashnameset<soundsample> soundsamples;
Slotmanager<soundslot> gamesounds;
vector<soundslot> mapsounds;
vector<SoundHandle> soundmap;
vector<soundsource> soundsources;
SDL_Thread *music_thread = NULL;
SDL_mutex *music_mutex = NULL;
musicstream *music = NULL;

vector<Sharedptr<soundenv>> soundenvs;
vector<soundenvzone> envzones;
vector<soundenvzone *> sortedenvzones;
soundenvzone *insideenvzone = NULL;

Sharedptr<soundenv> newsoundenv;
vector<soundefxslot> soundefxslots;

static int duckers = 0;

FVAR(IDF_READONLY, soundduckstate,   0.0f,   0.0f, 1.0f);
FVAR(IDF_PERSIST,  musicduckfactor,  0.0f,  0.33f, 1.0f);
FVAR(IDF_PERSIST,  soundduckfactor,  0.0f,  0.67f, 1.0f);
FVAR(IDF_PERSIST,  soundduckattack,  0.0f, 0.005f, 1.0f);
FVAR(IDF_PERSIST,  soundduckrelease, 0.0f, 0.001f, 1.0f);

static Sharedptr<soundenv> soundenvfroment(entity *ent)
{
    ASSERT(ent);

    int index = ent->attrs[0] - 1;
    if(soundenvs.inrange(index)) return soundenvs[index];

    return Sharedptr<soundenv>();
}

void allocsoundefxslots();
VARF(IDF_PERSIST, maxsoundefxslots, 1, 3, 10, allocsoundefxslots());
VAR(0, soundefxslotdebug, 0, 0, 1);

static void putsoundefxslots() { loopv(soundefxslots) soundefxslots[i].put(); }

static void delsoundfxslots()
{
    sortedenvzones.setsize(0);
    putsoundefxslots();
    loopv(soundefxslots) soundefxslots[i].del();
    soundefxslots.shrink(0);
}

void allocsoundefxslots()
{
    delsoundfxslots();

    loopi(maxsoundefxslots)
    {
        soundefxslot &slot = soundefxslots.add();
        slot.gen();
    }
}

static void getsoundefxslot(soundefxslot **hook, bool priority = false)
{
    if(soundefxslots.empty()) return;

    loopv(soundefxslots) if(!soundefxslots[i].hook)
    {
        soundefxslots[i].hook = hook;
        *hook = &soundefxslots[i];

        if(soundefxslotdebug) conoutf(colourwhite, "getsoundefxslot: free slot found: %u", soundefxslots[i].id);

        return;
    }

    // Free slot not found, find one that hasn't been used recently
    soundefxslot *oldest = &soundefxslots[0];
    for(int i = 1; i < soundefxslots.length(); ++i)
    {
        if(soundefxslots[i].lastused < oldest->lastused)
            oldest = &soundefxslots[i];
    }
    oldest->put();
    oldest->hook = hook;
    *hook = oldest;

    if(soundefxslotdebug) conoutf(colourwhite, "getsoundefxslot: oldest slot found: %u, last used %u ms ago",
        oldest->id, lastmillis - oldest->lastused);
}

FVAR(IDF_PERSIST, soundefxgain, 0.0f, 1.0f, 10.0f);

void soundenv::setparams(ALuint effect)
{
    float gain = props[SOUNDENV_PROP_GAIN].get<float>() * soundefxgain;
    gain = clamp(gain, AL_EAXREVERB_MIN_GAIN, AL_EAXREVERB_MAX_GAIN);

    alEffecti (effect, AL_EFFECT_TYPE,                     AL_EFFECT_EAXREVERB);
    alEffectf (effect, AL_EAXREVERB_DENSITY,               props[SOUNDENV_PROP_DENSITY]);
    alEffectf (effect, AL_EAXREVERB_DIFFUSION,             props[SOUNDENV_PROP_DIFFUSION]);
    alEffectf (effect, AL_EAXREVERB_GAIN,                  gain);
    alEffectf (effect, AL_EAXREVERB_GAINHF,                props[SOUNDENV_PROP_GAINHF]);
    alEffectf (effect, AL_EAXREVERB_GAINLF,                props[SOUNDENV_PROP_GAINLF]);
    alEffectf (effect, AL_EAXREVERB_DECAY_TIME,            props[SOUNDENV_PROP_DECAY_TIME]);
    alEffectf (effect, AL_EAXREVERB_DECAY_HFRATIO,         props[SOUNDENV_PROP_DECAY_HFRATIO]);
    alEffectf (effect, AL_EAXREVERB_DECAY_LFRATIO,         props[SOUNDENV_PROP_DECAY_LFRATIO]);
    alEffectf (effect, AL_EAXREVERB_REFLECTIONS_GAIN,      props[SOUNDENV_PROP_REFL_GAIN]);
    alEffectf (effect, AL_EAXREVERB_REFLECTIONS_DELAY,     props[SOUNDENV_PROP_REFL_DELAY]);
    alEffectf (effect, AL_EAXREVERB_LATE_REVERB_GAIN,      props[SOUNDENV_PROP_LATE_GAIN]);
    alEffectf (effect, AL_EAXREVERB_LATE_REVERB_DELAY,     props[SOUNDENV_PROP_LATE_DELAY]);
    alEffectf (effect, AL_EAXREVERB_ECHO_TIME,             props[SOUNDENV_PROP_ECHO_TIME]);
    alEffectf (effect, AL_EAXREVERB_ECHO_DEPTH,            props[SOUNDENV_PROP_ECHO_DELAY]);
    alEffectf (effect, AL_EAXREVERB_MODULATION_TIME,       props[SOUNDENV_PROP_MOD_TIME]);
    alEffectf (effect, AL_EAXREVERB_MODULATION_DEPTH,      props[SOUNDENV_PROP_MOD_DELAY]);
    alEffectf (effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, props[SOUNDENV_PROP_ABSORBTION_GAINHF]);
    alEffectf (effect, AL_EAXREVERB_HFREFERENCE,           props[SOUNDENV_PROP_HFREFERENCE]);
    alEffectf (effect, AL_EAXREVERB_LFREFERENCE,           props[SOUNDENV_PROP_LFREFERENCE]);
    alEffectf (effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR,   props[SOUNDENV_PROP_ROOM_ROLLOFF]);
    alEffecti (effect, AL_EAXREVERB_DECAY_HFLIMIT,         props[SOUNDENV_PROP_DECAY_HFLIMIT]);

    ASSERT(alGetError() == AL_NO_ERROR);
}

void soundenv::updatezoneparams(int envindex)
{
    loopv(envzones)
    {
        soundenvzone &zone = envzones[i];
        if(zone.ent && (zone.ent->attrs[0] - 1) == envindex) zone.refreshfroment();
        if(zone.env.get() == this && zone.efxslot) zone.attachparams();
    }
}

void soundenvzone::attachparams()
{
    ASSERT(efxslot && env);

    if(!alIsEffect(effect))
    {
        alGenEffects(1, &effect);
        ASSERT(alGetError() == AL_NO_ERROR);
    }

    env->setparams(effect);

    alAuxiliaryEffectSloti(efxslot->id, AL_EFFECTSLOT_EFFECT, effect);
    alAuxiliaryEffectSloti(efxslot->id, AL_EFFECTSLOT_AUXILIARY_SEND_AUTO, AL_TRUE);

    ALenum err = alGetError();

    if(err != AL_NO_ERROR)
    {
        conoutf(colourred, "ERROR %d", err);
        ASSERT(err == AL_NO_ERROR);
    }

    ASSERT(alGetError() == AL_NO_ERROR);
}

ALuint soundenvzone::getefxslot()
{
    if(!efxslot)
    {
        getsoundefxslot(&efxslot);

        if(!efxslot) return AL_INVALID;

        attachparams();
    }

    efxslot->lastused = lastmillis;

    return efxslot->id;
}

void soundenvzone::froment(entity *newent)
{
    ent = newent;
    env = soundenvfroment(ent);
    bbmin = vec(ent->o).sub(vec(ent->attrs[1], ent->attrs[2], ent->attrs[3]));
    bbmax = vec(ent->o).add(vec(ent->attrs[1], ent->attrs[2], ent->attrs[3]));
    loopi(16)
    {
        uint val = ent->attrs[4 + (i / 4)];
        val = (val >> ((i % 4) * 8)) & 0xFF;
        fadevals[i] = clamp(val, 0, 100);
    }
    if(efxslot) efxslot->put();
}

void soundenvzone::refreshfroment()
{
    ASSERT(ent);
    froment(ent);
}

float soundenvzone::getvolume()
{
    if(!ent) return 0.0f;
    return (float)ent->attrs[1] * (float)ent->attrs[2] * (float)ent->attrs[3];
}

void soundenvzone::updatepan()
{
    if(!efxslot) return;

    if(this == insideenvzone)
    {
        alEffectfv(effect, AL_EAXREVERB_REFLECTIONS_PAN, vec(0, 0, 0).v);
        alEffectfv(effect, AL_EAXREVERB_LATE_REVERB_PAN, vec(0, 0, 0).v);
    }
    else
    {
        ASSERT(ent);

        static constexpr float PANFACTOR_DIST = 1.0f/128.0f;

        float dist = camera1->o.dist_to_bb(bbmin, bbmax);
        float panfactor = clamp(dist * PANFACTOR_DIST, 0.0f, 1.0f);

        vec pan = vec(ent->o).sub(camera1->o);
        pan.rotate_around_z(-camera1->yaw*RAD).normalize().mul(panfactor);
        pan.x *= -1.0f;

        alEffectfv(effect, AL_EAXREVERB_REFLECTIONS_PAN, pan.v);
        alEffectfv(effect, AL_EAXREVERB_LATE_REVERB_PAN, pan.v);
    }

    alAuxiliaryEffectSloti(efxslot->id, AL_EFFECTSLOT_EFFECT, effect);
}

bool soundenvzone::isvalid() { return ent && env; }

static void sortenvzones()
{
    sortedenvzones.setsize(0);
    insideenvzone = NULL;

    if(envzones.empty()) return;

    sortedenvzones.reserve(envzones.length());
    sortedenvzones.add(&envzones[0]);

    for(int i = 0; i < envzones.length(); ++i)
    {
        soundenvzone &zone = envzones[i];
        float dist = camera1->o.dist_to_bb(zone.bbmin, zone.bbmax);

        if(camera1->o.insidebb(zone.bbmin, zone.bbmax))
        {
            // smaller zones take priority over larger ones in case of overlap
            float curvolume = !insideenvzone ? 0.0f : insideenvzone->getvolume();
            float newvolume = zone.getvolume();

            if(!insideenvzone || newvolume < curvolume) insideenvzone = &zone;
        }

        for(int j = 0; i && j <= sortedenvzones.length(); ++j)
        {
            if(zone.isvalid() && (j >= sortedenvzones.length() ||
                dist <= camera1->o.dist_to_bb(sortedenvzones[j]->bbmin, sortedenvzones[j]->bbmax)))
            {
                sortedenvzones.insert(j, &zone);
                break;
            }
        }
    }
}

static inline int maxactivezones() { return min(sortedenvzones.length(), maxsoundefxslots); }

static void panenvzones()
{
    loopi(maxactivezones()) sortedenvzones[i]->updatepan();
    if(insideenvzone) insideenvzone->updatepan();
}

static soundenvzone *getactiveenvzone(const vec &pos)
{
    soundenvzone *bestzone = NULL;

    loopi(maxactivezones())
    {
        soundenvzone *zone = sortedenvzones[i];

        if(!pos.insidebb(zone->bbmin, zone->bbmax)) continue;

        float curvolume = !bestzone ? 0.0f : bestzone->getvolume();
        float newvolume = zone->getvolume();

        if(!bestzone || newvolume < curvolume) bestzone = zone;
    }

    return bestzone;
}

static soundenvzone *buildenvzone(entity *ent)
{
    soundenvzone &newzone = envzones.add();
    newzone.froment(ent);

    return &newzone;
}

static void buildenvzones()
{
    envzones.shrink(0);
    vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity *ent = ents[i];
        if(ent->flags&EF_VIRTUAL) continue; // skip virtual entities
        if(ent->type == ET_SOUNDENV) buildenvzone(ent);
    }
}

void updateenvzone(entity *ent)
{
    int index = -1;

    loopv(envzones) if(envzones[i].ent == ent)
    {
        index = i;
        break;
    }

    if(ent->type != ET_SOUNDENV && index >= 0) envzones.remove(index);
    else if(ent->type == ET_SOUNDENV)
    {
        if(index >= 0) envzones[index].froment(ent);
        else buildenvzone(ent); // zone hasn't been created for this ent
    }
}

static void getcamerasoundzone()
{
    soundenvzone *zone = getactiveenvzone(camera1->o);
    if(!zone) return;

    vector<extentity *> &ents = entities::getents();
    int entindex = -1;

    // Find matching entity
    loopv(ents) if(zone->ent && zone->ent == ents[i])
    {
        entindex = i;
        break;
    }

    intret(entindex);
}
COMMAND(0, getcamerasoundzone, "");

void initmapsound()
{
    buildenvzones();
    mapsoundmillis = lastmillis;
}

SVARF(IDF_INIT, sounddevice, "", initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(IDF_INIT, soundmaxsources, 1, 200, 1000, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));

#define SOUNDVOL(oldname, newname, def) \
    FVAR(IDF_PERSIST, sound##newname##vol, 0, def, 100); \
    ICOMMAND(0, oldname##vol, "iN$", (int *n, int *numargs, ident *id), \
        if(*numargs > 0) sound##newname##vol = clamp(*n, 0, 255) / 255.f; \
        else if(*numargs < 0) intret(int(sound##newname##vol * 255)); \
        else printvar(id, int(sound##newname##vol * 255)); \
    );

SOUNDVOL(master, master, 1.f);
SOUNDVOL(sound, effect, 1.f);
SOUNDVOL(music, music, 0.3f);

int musicfademillis = 0, muiscfadetime = 0;
bool musicstopping = false, musicfadeout = false;
VAR(IDF_PERSIST, soundmusicfadein, 0, 1000, VAR_MAX);
VAR(IDF_PERSIST, soundmusicfadeinfast, 0, 400, VAR_MAX);
VAR(IDF_PERSIST, soundmusicfadeout, 0, 3000, VAR_MAX);
VAR(IDF_PERSIST, soundmusicfadeoutfast, 0, 600, VAR_MAX);

FVAR(IDF_PERSIST, soundeffectevent, 0, 1, 100);
FVAR(IDF_PERSIST, soundeffectenv, 0, 1, 100);
FVAR(IDF_PERSIST, sounddistfilter, 0.0f, 0.5f, 1.0f);

FVAR(IDF_PERSIST, soundoccluded, 0.0f, 0.6f, 1.0f);
FVAR(IDF_PERSIST, soundoccludedhf, 0.0f, 0.1f, 1.0f);

FVAR(IDF_MAP, mapsoundfadespeed, 0.0001f, 0.001f, 1.0f);

SF_VIRTUAL_IO soundvfio;

static sf_count_t soundvfsize(void *data)
{
    stream *f = (stream *)data;
    if(f) return (sf_count_t)f->size();
    return -1;
}

static sf_count_t soundvfseek(sf_count_t pos, int whence, void *data)
{
    stream *f = (stream *)data;
    if(f && ((!pos && whence==SEEK_CUR) || f->seek(pos, whence))) return (sf_count_t)f->tell();
    return -1;
}

static sf_count_t soundvfread(void *buf, sf_count_t len, void *data)
{
    stream *f = (stream *)data;
    if(f) return (sf_count_t)f->read(buf, len);
    return 0;
}

static sf_count_t soundvfwrite(const void *buf, sf_count_t len, void *data)
{
    stream *f = (stream *)data;
    if(f) return (sf_count_t)f->write(buf, len);
    return 0;
}

static sf_count_t soundvftell(void *data)
{
    stream *f = (stream *)data;
    if(f) return (sf_count_t)f->tell();
    return -1;
}

static void soundvfsetup()
{
    soundvfio.get_filelen = soundvfsize;
    soundvfio.seek = soundvfseek;
    soundvfio.read = soundvfread;
    soundvfio.write = soundvfwrite;
    soundvfio.tell = soundvftell;
}

const char *sounderror(bool msg)
{
    ALenum err = alGetError();
    if(!msg && err == AL_NO_ERROR) return NULL;
    return alGetString(err);
}

void soundsetdoppler(float v)
{
    if(nosound) return;
    alGetError();
    alDopplerFactor(v);
    const char *err = sounderror(false);
    if(err) conoutf(colourred, "Failed to set doppler factor: %s", err);
}
FVARF(IDF_PERSIST, sounddoppler, 0, 0.1f, FVAR_MAX, soundsetdoppler(sounddoppler));

void soundsetspeed(float v)
{
    if(nosound) return;
    alGetError();
    alSpeedOfSound(v * 8.f);
    const char *err = sounderror(false);
    if(err) conoutf(colourred, "Failed to set speed of sound: %s", err);
}
FVARF(IDF_PERSIST, soundspeed, FVAR_NONZERO, 343.3f, FVAR_MAX, soundsetspeed(sounddoppler));

FVARF(IDF_INIT, soundrefdist, FVAR_NONZERO, 16, FVAR_MAX, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
FVARF(IDF_INIT, soundrolloff, FVAR_NONZERO, 256, FVAR_MAX, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));

void mapsoundslot(int index, const char *name)
{
    while(index >= soundmap.length()) soundmap.add();
    soundmap[index] = gamesounds[name];
}

int getsoundslot(int index)
{
    if(!soundmap.inrange(index) || !soundmap[index].isvalid()) return -1;
    return soundmap[index].getindex();
}

void mapsoundslots()
{
    static const char *names[S_NUM_GENERIC] = { "S_PRESS", "S_BACK", "S_ACTION" };
    loopi(S_NUM_GENERIC) mapsoundslot(i, names[i]);

    game::mapgamesounds();
}

int musicloop(void *data)
{
    SDL_LockMutex(music_mutex);
    if(!music_thread || SDL_GetThreadID(music_thread) != SDL_ThreadID())
    {
        SDL_UnlockMutex(music_mutex);
        return 0;

    }
    SDL_UnlockMutex(music_mutex);

    while(true)
    {
        SDL_LockMutex(music_mutex);

        if(!music_thread || SDL_GetThreadID(music_thread) != SDL_ThreadID() || !music)
        {
            SDL_UnlockMutex(music_mutex);
            break;
        }

        if(music && music->update() != AL_NO_ERROR) musicstopping = true;

        SDL_UnlockMutex(music_mutex);
        SDL_Delay(10);
    }

    return 0;
}

void musicloopinit()
{
    SDL_LockMutex(music_mutex);
    music_thread = SDL_CreateThread(musicloop, "music", NULL);
    SDL_UnlockMutex(music_mutex);
}

void musicloopstop()
{
    SDL_LockMutex(music_mutex);

    if(music_thread)
    {
        SDL_DetachThread(music_thread);
        music_thread = NULL;
    }

    SDL_UnlockMutex(music_mutex);
}

void initsound()
{
    if(nosound)
    {
        soundvfsetup(); // create virtual io interface

        if(*sounddevice)
        {
            alGetError();
            snddev = alcOpenDevice(sounddevice);
            if(!snddev) conoutf(colourred, "Sound device initialisation failed: %s (with '%s', retrying default device)", sounderror(), sounddevice);
        }

        if(!snddev)
        {
            alGetError();
            snddev = alcOpenDevice(NULL); // fallback to default device
        }

        if(!snddev)
        {
            conoutf(colourred, "Sound device initialisation failed: %s", sounderror());
            return;
        }

        alGetError();
        sndctx = alcCreateContext(snddev, NULL);
        if(!sndctx)
        {
            conoutf(colourred, "Sound context initialisation failed: %s", sounderror());
            alcCloseDevice(snddev);
            snddev = NULL;
            return;
        }

        alcMakeContextCurrent(sndctx);
        conoutf(colourwhite, "Sound: %s (%s) %s", alGetString(AL_RENDERER), alGetString(AL_VENDOR), alGetString(AL_VERSION));
        al_ext_efx = alcIsExtensionPresent(snddev, "ALC_EXT_EFX") ? true : false;
        al_ext_float32 = alIsExtensionPresent("AL_EXT_FLOAT32") ? true : false;
        al_soft_spatialize = alIsExtensionPresent("AL_SOFT_source_spatialize") ? true : false;
        alcGetIntegerv(snddev, ALC_FREQUENCY, 1, &al_sample_rate);

        if(alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT") != AL_FALSE)
        {
            const ALCchar *d = alcGetString(NULL, ALC_DEFAULT_ALL_DEVICES_SPECIFIER),
                          *s = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
            if(s)
            {
                conoutf(colourwhite, "Audio device list:");
                for(const ALchar *c = s; *c; c += strlen(c)+1)
                    conoutf(colourwhite, "- %s%s", c, !strcmp(c, d) ? " [default]" : "");
            }
            conoutf(colourwhite, "Audio device: %s [%d]", alcGetString(snddev, ALC_ALL_DEVICES_SPECIFIER), al_sample_rate);
        }
        else conoutf(colourwhite, "Audio device: %s [%d]", alcGetString(snddev, ALC_DEVICE_SPECIFIER), al_sample_rate);

        getextsoundprocs();
        allocsoundefxslots();

        nosound = false;

        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
        soundsetdoppler(sounddoppler);
        soundsetspeed(soundspeed);

        music_mutex = SDL_CreateMutex();
    }
    initmumble();
}

void stopmusic()
{
    SDL_LockMutex(music_mutex);

    musicstopping = false;
    if(musicfademillis && musicfadeout) musicfademillis = muiscfadetime = 0;

    if(!music)
    {
        SDL_UnlockMutex(music_mutex);
        return;
    }

    delete music;
    music = NULL;
    SDL_UnlockMutex(music_mutex);

    musicloopstop();
}

void stopsound()
{
    if(nosound) return;
    delsoundfxslots();
    stopmusic();
    clearsound();
    enumerate(soundsamples, soundsample, s, s.clear());
    soundsamples.clear();
    gamesounds.clear();
    closemumble();
    alcMakeContextCurrent(NULL);
    alcDestroyContext(sndctx);
    alcCloseDevice(snddev);
    nosound = true;
}

void clearsound()
{
    loopv(soundsources) soundsources[i].clear();
    mapsounds.shrink(0);
    soundenvs.shrink(0);
    envzones.shrink(0);
}

soundfile *loadsoundfile(const char *name, int mixtype)
{
    soundfile *w = new soundfile;
    if(!w->setup(name, al_ext_float32 ? soundfile::FLOAT : soundfile::SHORT, mixtype))
    {
        delete w;
        return NULL;
    }
    return w;
}

int fademusic(int dir, bool fast)
{
    switch(dir)
    {
        case -1: // fadeout with check
            if(musicfademillis && musicfadeout) break;
        case -2: // fadeout
            musicfademillis = getclockticks();
            musicfadeout = true;
            break;
        case 1: // fadein with check
            if(musicfademillis && !musicfadeout) break;
        case 2: // fadein
            musicfademillis = getclockticks();
            musicfadeout = false;
            break;
        case 0: default: // cancel
            musicfademillis = 0;
            musicfadeout = false;
            break;
    }

    if(musicfademillis)
    {
        if(musicfadeout) muiscfadetime = fast ? soundmusicfadeoutfast : soundmusicfadeout;
        else muiscfadetime = fast ? soundmusicfadeinfast : soundmusicfadein;
    }
    else muiscfadetime = 0;

    if(!muiscfadetime)
    {
        if(musicfademillis && musicfadeout) stopmusic();
        musicfademillis = 0;
        musicfadeout = false;
    }
    else if(musicfademillis && musicfadeout)
    {
        SDL_LockMutex(music_mutex);
        if(music && music->playing())
        {
            music->fademillis = musicfademillis;
            music->fadetime = muiscfadetime;
            music->fadeout = musicfadeout;
            SDL_UnlockMutex(music_mutex);
        }
        else
        {
            SDL_UnlockMutex(music_mutex);
            stopmusic();
            musicfademillis = muiscfadetime = 0;
            musicfadeout = false;
        }
    }

    return musicfademillis;
}

ICOMMAND(0, fademusic, "ii", (int *n, int *f), fademusic(*n, *f != 0));

bool playmusic(const char *name, bool looping)
{
    if(nosound) return false;
    stopmusic();
    if(!name || !*name) return false;

    SDL_LockMutex(music_mutex);
    string buf;
    loopi(SOUND_MDRS) loopk(SOUND_EXTS)
    {
        formatstring(buf, "%s%s%s", musicdirs[i], name, soundexts[k]);
        soundfile *w = loadsoundfile(buf, soundfile::MUSIC);
        if(!w) continue;

        music = new musicstream;
        SOUNDCHECK(music->setup(name, w, looping, musicfademillis, muiscfadetime),
        {
            SDL_UnlockMutex(music_mutex);
            musicloopinit();
            return true;
        },{
            delete music;
            music = NULL;
            conoutf(colourred, "Error loading %s: %s [%s@%s:%d]", buf, alGetString(err), al_errfunc, al_errfile, al_errline);
        });
    }

    conoutf(colourred, "Could not play music: %s", name);
    SDL_UnlockMutex(music_mutex);

    return false;
}

ICOMMAND(0, music, "sb", (char *s, int *n), playmusic(s, *n != 0));

bool playingmusic(bool active)
{
    bool result = false;

    SDL_LockMutex(music_mutex);

    if(music && (active ? music->active() : music->playing())) result = true;

    SDL_UnlockMutex(music_mutex);

    return result;
}

bool musicinfo(char *title, char *artist, char *album, size_t len)
{
    bool result = false;

    SDL_LockMutex(music_mutex);

    if(music)
    {
        if(music->title) copystring(title, music->title, len);
        else *title = 0;

        if(music->artist) copystring(artist, music->artist, len);
        else *artist = 0;

        if(music->album) copystring(album, music->album, len);
        else *album = 0;

        result = true;
    }
    else *title = *artist = *album = 0;

    SDL_UnlockMutex(music_mutex);

    return result;
}

SVAR(0, musictheme, "sounds/music/theme");
SVAR(0, musicprogress, "sounds/music/progress");
SVAR(0, musicintermission, "sounds/egmusic/antumbra");

bool canplaymusic()
{
    return !nosound && soundmastervol && soundmusicvol && canmusic;
}

void smartmusic(int type, bool init)
{
    if(init) canmusic = true;
    else if(!canplaymusic()) return;

    bool delstr = false;
    char *name = NULL;

    switch(type)
    {
        case 0: default: name = musictheme; break;
        case 1: name = musicprogress; break;
        case 2: name = musicintermission; break;
    }

    if(!name || !*name) name = musictheme;

    if(*name == '[')
    {
        vector<char *> list;
        explodelist(name, list);
        while(!list.empty())
        {
            int r = rnd(list.length());
            char *v = list[r];
            if(!v || !*v)
            {
                list.remove(r);
                continue;
            }

            name = newstring(v);
            delstr = true;

            break;
        }

        list.deletearrays();
    }

    if(!name || !*name) return;

    playmusic(name, true);

    if(delstr && name) delete[] name;
}
ICOMMAND(0, smartmusic, "i", (int *a), smartmusic(*a));

static soundsample *loadsoundsample(const char *name)
{
    soundsample *sample = NULL;
    if(!(sample = soundsamples.access(name)))
    {
        char *n = newstring(name);
        sample = &soundsamples[n];
        sample->name = n;
        sample->reset();
    }

    if(sample->valid()) return sample;

    string buf;
    loopi(SOUND_FDRS) loopk(SOUND_EXTS)
    {
        formatstring(buf, "%s%s%s", sounddirs[i], sample->name, soundexts[k]);
        soundfile *w = loadsoundfile(buf, al_soft_spatialize ? soundfile::SPATIAL : soundfile::MONO);
        if(!w) continue;
        SOUNDCHECK(sample->setup(w), {
            delete w;
            return sample;
        }, {
            delete w;
            conoutf(colourred, "Error loading %s: %s [%s@%s:%d]", buf, alGetString(err), al_errfunc, al_errfile, al_errline);
        });
    }
    return NULL;
}

static void loadsamples(soundslot &slot)
{
    soundsample *sample;
    string sam;

    loopi(slot.variants)
    {
        copystring(sam, slot.name);
        if(i) concatstring(sam, intstr(i + 1));

        sample = loadsoundsample(sam);
        slot.samples.add(sample);

        if(!sample || !sample->valid()) conoutf(colourred, "Failed to load sample: %s (%s)", sam, sample ? "Invalid sample" : "Not found");
    }
}

soundslot &addsoundslot(const char *id, int &index, Slotmanager<soundslot> &soundset)
{
    SoundHandle handle = soundset[id];
    index = handle.getindex();
    return handle.get();
}

soundslot &addsoundslot(const char *id, int &index, vector<soundslot> &soundset)
{
    index = soundset.length();
    return soundset.add();
}

template<class SetType>
int addsound(const char *id, const char *name, float gain, float pitch, float rolloff, float refdist, float maxdist, int variants, SetType &soundset)
{
    if(nosound || !strcmp(name, "<none>")) return -1;

    int index = -1;
    soundslot &slot = addsoundslot(id, index, soundset);

    slot.reset();
    slot.name = newstring(name);
    slot.gain = gain > 0 ? clamp(gain, 0.f, 100.f) : 1.f;
    slot.pitch = pitch >= 0 ? clamp(pitch, FVAR_NONZERO, 100.f) : 1.f;
    slot.rolloff = rolloff >= 0 ? rolloff : 1.f;
    slot.refdist = refdist >= 0 ? refdist : -1.f;
    slot.maxdist = maxdist >= 0 ? maxdist : -1.f;
    slot.variants = clamp(variants, 1, 32); // keep things sane please

    loadsamples(slot);

    return index;
}

ICOMMAND(0, loadsound, "ssgggggi", (char *id, char *name, float *gain, float *pitch, float *rolloff, float *refdist, float *maxdist, int *variants),
    intret(addsound(id, name, *gain, *pitch, *rolloff, *refdist, *maxdist, *variants, gamesounds));
);

template<class SetType>
int addsoundcompat(char *id, char *name, int vol, int maxrad, int minrad, int variants, SetType &soundset)
{
    float gain = vol > 0 ? vol / 255.f : 1.f, rolloff = maxrad > soundrolloff ? soundrolloff / float(maxrad) : 1.f, refdist = minrad > soundrefdist ? float(minrad) : -1.f;
    return addsound(id, name, gain, 1.f, rolloff, refdist, -1.f, variants, soundset);
}

ICOMMAND(0, registersound, "ssissi", (char *id, char *name, int *vol, char *maxrad, char *minrad, int *variants),
    intret(addsoundcompat(id, name, *vol, *maxrad ? parseint(maxrad) : -1, *minrad ? parseint(minrad) : -1, *variants, gamesounds))
);

ICOMMAND(0, mapsound, "sgggggi", (char *name, float *gain, float *pitch, float *rolloff, float *refdist, float *maxdist, int *variants),
    if(maploading && hdr.version <= 51)
    {
        intret(addsoundcompat(NULL, name, *gain, *pitch, *rolloff, int(*refdist), mapsounds));
        return;
    }
    intret(addsound(NULL, name, *gain, *pitch, *rolloff, *refdist, *maxdist, *variants, mapsounds));
);

ICOMMAND(0, mapsound, "sissi", (char *name, int *vol, char *maxrad, char *minrad, int *variants),
    intret(addsoundcompat(NULL, name, *vol, *maxrad ? parseint(maxrad) : -1, *minrad ? parseint(minrad) : -1, *variants, mapsounds))
);

void clearmapsounds()
{
    loopv(soundsources) if(soundsources[i].flags&SND_MAP) soundsources[i].clear();
    mapsounds.shrink(0);
}
COMMAND(0, clearmapsounds, "");

void remmapsound(int *index)
{
    if(!mapsounds.inrange(*index)) return;

    loopv(soundsources) if(soundsources[i].flags&SND_MAP) soundsources[i].clear();
    mapsounds.remove(*index);

    // Correct entities
    vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.flags&EF_VIRTUAL) continue; // skip virtual entities
        if(e.type == ET_SOUND && e.attrs[0] && e.attrs[0] >= *index) e.attrs[0]--;
    }
}
COMMAND(0, remmapsound, "i");

static inline bool sourcecmp(const soundsource *x, const soundsource *y) // sort with most likely to be culled first!
{
    bool xprio = x->flags&SND_PRIORITY, yprio = y->flags&SND_PRIORITY;
    if(!xprio && yprio) return true;
    if(xprio && !yprio) return false;

    bool xmap = x->flags&SND_MAP, ymap = y->flags&SND_MAP;
    if(!xmap && ymap) return true;
    if(xmap && !ymap) return false;

    bool xnoatt = (x->flags&(SND_NOATTEN|SND_NODIST)) != 0, ynoatt = (y->flags&(SND_NOATTEN|SND_NODIST)) != 0;
    if(!xnoatt && ynoatt) return true;
    if(xnoatt && !ynoatt) return false;

    float xdist = x->pos.dist(camera1->o), ydist = y->pos.dist(camera1->o);
    if(xdist > ydist) return true;
    if(xdist < ydist) return false;

    return true;
}

int soundindex(soundslot *slot, int slotnum, const vec &pos, int flags, int *hook)
{
    if(hook && soundsources.inrange(*hook))
    { // hooks can just take over their original index
        soundsources[*hook].clear(false); // don't clear a hook we're about to re-use
        return *hook;
    }

    static vector<soundsource *> sources;
    sources.setsize(0);

    static soundsource indexsource;
    indexsource.clear();
    indexsource.index = -1;
    indexsource.pos = pos;
    indexsource.flags = flags;
    sources.add(&indexsource);

    loopv(soundsources)
    {
        soundsource &s = soundsources[i];
        if(s.valid()) sources.add(&s);
        else return s.index; // unused index
    }

    if(soundsources.length() < soundmaxsources)
    { // if we get here, can't re-use an old index, try a new one first
        soundsource &s = soundsources.add();
        s.index = soundsources.length() - 1;
        return s.index;
    }

    sources.sort(sourcecmp); // sort as a last resort
    if(!sources.empty() && sources[0]->index >= 0)
    { // as long as the top isn't our dummy we can clear it
        sources[0]->clear();
        return sources[0]->index;
    }

    return -1;
}

static void updateducking()
{
    if(duckers > 0) soundduckstate = lerpstep(soundduckstate, 1.0f, soundduckattack * curtime);
    else soundduckstate = lerpstep(soundduckstate, 0.0f, soundduckrelease * curtime);
}

void updatesounds()
{
    updateducking();
    updatemumble();

    if(nosound) return;
    alcSuspendContext(sndctx);

    sortenvzones();
    panenvzones();

    loopv(soundsources) if(soundsources[i].valid())
    {
        soundsource &s = soundsources[i];

        if(s.index < 0)
        {
            s.clear();
            s.index = i;
            continue;
        }

        if((!s.ends || lastmillis < s.ends) && s.active())
        {
            s.update();
            continue;
        }
        s.cleanup();

        vector<soundslot> &soundset = s.flags&SND_MAP ? mapsounds : gamesounds.vec();
        while(!s.buffer.empty())
        {
            int n = s.buffer[0];
            s.buffer.remove(0);

            if((!soundset.inrange(n) || soundset[n].samples.empty() || !soundset[n].gain)) continue;

            soundslot *slot = &soundset[n];
            int variant = slot->variants > 1 ? rnd(slot->variants) : 0;
            soundsample *sample = slot->samples.inrange(variant) ? slot->samples[variant] : slot->samples[0];
            if(!sample || !sample->valid()) continue;

            s.slot = slot;
            s.slotnum = n;
            SOUNDCHECK(s.push(sample), break, conoutf(colourred, "Error playing buffered sample [%d] %s (%d): %s [%s@%s:%d]", n, sample->name, s.index, alGetString(err), al_errfunc, al_errfile, al_errline));
        }
        //conoutf(colourgrey, "Clearing sound source %d [%d] %d [%d] %s", s.source, s.index, s.ends, lastmillis, soundsources[i].playing() ? "playing" : "not playing");
        if(!s.active()) s.clear();
    }

    SDL_LockMutex(music_mutex);
    bool hasmusic = music != NULL;
    bool musicvalid = music && music->valid();
    SDL_UnlockMutex(music_mutex);

    if(!musicvalid || (hasmusic && (musicstopping || nosound || !soundmastervol || !soundmusicvol || !playingmusic())))
        stopmusic();

    game::updatemusic();

    dynent *d = game::focusedent();
    ALfloat orient[6] = { camdir.x, camdir.y, camdir.z, -camup.x, -camup.y, -camup.z },
            position[3] = { camera1->o.x, camera1->o.y, camera1->o.z }, velocity[3] = { d->vel.x, d->vel.y, d->vel.z };
    alListenerf(AL_GAIN, soundmastervol);
    alListenerfv(AL_ORIENTATION, (ALfloat *) &orient);
    alListenerfv(AL_POSITION, (ALfloat *) &position);
    alListenerfv(AL_VELOCITY, (ALfloat *) &velocity);
    if(al_ext_efx) alListenerf(AL_METERS_PER_UNIT, 0.125f); // 8 units = 1 meter

    alcProcessContext(sndctx);
}

bool soundready(int n, float gain, int flags)
{
    if(!engineready || nosound || gain <= 0 || !soundmastervol || !soundeffectvol || (flags&SND_MAP ? !soundeffectenv : !soundeffectevent)) return false;
    if((flags&SND_MAP || (!(flags&SND_UNMAPPED) && n >= S_GAMESPECIFIC)) && client::waiting(true)) return false;
    return true;
}

int emitsound(int n, vec *pos, physent *d, int *hook, int flags, float gain, float pitch, float rolloff, float refdist, float maxdist, int ends, float offset, int groupid)
{
    if(!soundready(n, gain, flags) || !pos) return -1;

    soundslot *slot = NULL;

    if(n < 0) slot = &previewslot;
    else
    {
        vector<soundslot> &soundset = flags&SND_MAP ? mapsounds : gamesounds.vec();
        if(!(flags&SND_UNMAPPED) && !(flags&SND_MAP)) n = getsoundslot(n);

        if(!soundset.inrange(n) && n > 0) conoutf(colourred, "Unregistered sound: %d", n);
        else if(soundset.inrange(n)) slot = &soundset[n];
    }

    if(slot)
    {
        if(slot->samples.empty() || !slot->gain) return -1;
        if(hook && issound(*hook) && flags&SND_BUFFER)
        {
            soundsources[*hook].buffer.add(n);
            return *hook;
        }

        int variant = slot->variants > 1 ? rnd(slot->variants) : 0;
        soundsample *sample = slot->samples.inrange(variant) ? slot->samples[variant] : slot->samples[0];
        if(!sample || !sample->valid()) return -1;

        int index = soundindex(slot, n, *pos, flags, hook);
        if(index < 0) return -1;

        soundsource &s = soundsources[index];
        if(s.hook && s.hook != hook) *s.hook = -1;

        s.slot = slot;
        s.slotnum = n;
        s.millis = lastmillis;
        s.flags = flags;
        if(d || s.flags&SND_TRACKED)
        {
            s.vpos = pos;
            if(d) s.vel = d->vel;
            else s.vel = vec(0, 0, 0);
            s.flags |= SND_TRACKED;
        }
        else s.vpos = NULL;
        s.pos = *pos;

        s.gain = gain > 0 ? clamp(gain, 0.f, 100.f) : 1.f;
        s.pitch = pitch >= 0 ? clamp(pitch, FVAR_NONZERO, 100.f) : 1.f;
        s.rolloff = rolloff >= 0 ? rolloff : -1.f;
        s.refdist = refdist >= 0 ? refdist : -1.f;
        s.maxdist = maxdist >= 0 ? maxdist : -1.f;
        s.owner = d;
        s.ends = ends;
        s.offset = clamp(offset, 0.0f, FLT_MAX);
        s.groupid = clamp(groupid, 0, 16);

        if(hook) *hook = s.index;
        s.hook = hook;

        // conoutf(colouryellow, "SND: %d / %.6g %.6g %.6g [%d] (%d)", index, s.pos.x, s.pos.y, s.pos.z, s.owner ? s.owner->type : -1, s.hook ? *s.hook : -1);

        SOUNDCHECK(s.push(sample), return index, conoutf(colourred, "Error playing sample [%d] %s (%d): %s [%s@%s:%d]", n, sample->name, index, alGetString(err), al_errfunc, al_errfile, al_errline));
    }

    return -1;
}

// Call CubeScript alias on sound end
void soundcshook(int *index, ident *id)
{
    if(!soundsources.inrange(*index) || !soundsources[*index].valid()) return;
    if(!id || id->type != ID_ALIAS) return;

    soundsources[*index].setcshook(id);
}

COMMAND(0, soundcshook, "ir");

int emitsoundpos(int n, const vec &pos, int *hook, int flags, float gain, float pitch, float rolloff, float refdist, float maxdist, int ends, float offset, int groupid)
{
    if(!soundready(n, gain, flags)) return -1;

    vec curpos = pos;
    flags &= ~SND_TRACKED; // can't do that here

    return emitsound(n, &curpos, NULL, hook, flags, gain, pitch, rolloff, refdist, maxdist, ends, offset);
}

VAR(IDF_PERSIST, soundpreviewlength, 1000, 4000, 10000);

void previewsound(const char *s, float *gain)
{
    loopv(soundsources) if(soundsources[i].slot == &previewslot) soundsources[i].clear();

    previewslot.reset();
    previewslot.name     = newstring(s);
    previewslot.gain     = *gain > 0 ? clamp(*gain, 0.f, 100.f) : 1.f;
    previewslot.pitch    = 1.0;
    previewslot.rolloff  = 1.0f;
    previewslot.refdist  = -1.0f;
    previewslot.maxdist  = -1.0f;
    previewslot.variants = 1;

    loadsamples(previewslot);

    emitsound(-1, &camera1->o, camera1, NULL, SND_PREVIEW);
}
COMMAND(0, previewsound, "sf");

int playsound(int n, const vec &pos, physent *d, int flags, int vol, int maxrad, int minrad, int *hook, int ends, float offset, int groupid)
{
    float gain = vol > 0 ? vol / 255.f : 1.f;
    if(!soundready(n, gain, flags)) return -1;

    float rolloff = maxrad > soundrolloff ? soundrolloff / float(maxrad) : 1.f,
          refdist = minrad > soundrefdist ? float(minrad) : -1.f;
    vec o = d ? d->o : pos;

    return emitsound(n, &o, d, hook, flags, gain, 1.f, rolloff, refdist, -1.f, ends, offset);
}

ICOMMAND(0, sound, "iib", (int *n, int *vol, int *flags),
{
    intret(playsound(*n, camera1->o, camera1, (*flags >= 0 ? *flags : SND_PRIORITY|SND_NOENV|SND_NOATTEN)|SND_UNMAPPED, *vol ? *vol : -1));
});

ICOMMAND(0, soundbyname, "sibf", (char *i, int *vol, int *flags, float *pitch),
{
    int chan = playsound(gamesounds[i].getindex(), camera1->o, camera1, (*flags >= 0 ? *flags : SND_PRIORITY|SND_NOENV|SND_NOATTEN)|SND_UNMAPPED, *vol ? *vol : -1);

    if (chan >= 0 && *pitch > 0)
        soundsources[chan].pitch = clamp(*pitch, FVAR_NONZERO, 100.f);

    intret(chan);
});

ICOMMAND(0, soundslot, "s", (char *i), intret(gamesounds[i].getindex()));

void removemapsounds()
{
    loopv(soundsources) if(soundsources[i].index >= 0 && soundsources[i].flags&SND_MAP) soundsources[i].clear();
}

void removetrackedsounds(physent *d)
{
    if(!d) return;

    vec *pos = game::getplayersoundpos(d);
    loopv(soundsources) if(soundsources[i].owner == d || (soundsources[i].flags&SND_TRACKED && soundsources[i].vpos && soundsources[i].vpos == pos))
        soundsources[i].clear();
}

void resetsound()
{
    clearchanges(CHANGE_SOUND);
    delsoundfxslots();
    loopv(soundsources) soundsources[i].clear();
    enumerate(soundsamples, soundsample, s, s.clear());
    stopmusic();
    nosound = true;
    initsound();
    if(nosound)
    {
        gamesounds.clear();
        mapsounds.shrink(0);
        soundenvs.shrink(0);
        soundsamples.clear();
        return;
    }
    loopv(gamesounds) loadsamples(gamesounds[i]);
    loopv(mapsounds) loadsamples(mapsounds[i]);
    //rehash(true);
}

COMMAND(0, resetsound, "");

bool soundfile::setup(const char *name, int t, int m)
{
    viofile = openfile(name, "rb");
    if(!viofile)
    {
        clear(); // don't print as we scan directories
        //conoutf(colourred, "Failed to open sound file: %s", name);
        return false;
    }

    sndfile = sf_open_virtual(&soundvfio, SFM_READ, &info, viofile);
    if(!sndfile)
    {
        conoutf(colourred, "Failed to create libsndfile context: %s (%s)", name, sf_strerror(NULL));
        clear();
        return false;
    }
    if(info.frames <= 0 || info.frames >= SF_COUNT_MAX)
    {
        conoutf(colourred, "Invalid sound file frame count: %s (%li)", name, info.frames);
        clear();
        return false;
    }

    switch(t)
    {
        case FLOAT:
        {
            type = FLOAT;
            size = sizeof(float);
            data_f = NULL;
            break;
        }
        case SHORT:
        {
            type = SHORT;
            size = sizeof(short);
            data_s = NULL;
            break;
        }
        default:
        {
            clear();
            return false;
        }
    }
    mixtype = clamp(m, 0, int(MAXMIX-1));

    if(info.channels == 1) format = type == soundfile::FLOAT ? AL_FORMAT_MONO_FLOAT32 : AL_FORMAT_MONO16;
    else if(info.channels == 2) format = type == soundfile::FLOAT ? AL_FORMAT_STEREO_FLOAT32 : AL_FORMAT_STEREO16;
#if 0
    else if(info.channels == 3)
    {
        if(sf_command(sndfile, SFC_WAVEX_GET_AMBISONIC, NULL, 0) == SF_AMBISONIC_B_FORMAT)
            format = AL_FORMAT_BFORMAT2D_16;
    }
    else if(info.channels == 4)
    {
        if(sf_command(sndfile, SFC_WAVEX_GET_AMBISONIC, NULL, 0) == SF_AMBISONIC_B_FORMAT)
            format = AL_FORMAT_BFORMAT3D_16;
    }
#endif

    if(!format)
    {
        conoutf(colourred, "Unsupported channel count in: %s (%d)", name, info.channels);
        clear();
        return false;
    }

    return mixtype == soundfile::MUSIC ? setupmus() : setupsnd();
}

bool soundfile::setupmus()
{
    switch(type)
    {
        case soundfile::FLOAT:
            data_f = new float[size_t((MUSICSAMP * info.channels) * sizeof(size))];
            break;
        case soundfile::SHORT:
            data_s = new short[size_t((MUSICSAMP * info.channels) * sizeof(size))];
            break;
        default: return false;
    }
    return true;
}

bool soundfile::fillmus(bool retry)
{
    len = 0;
    switch(type)
    {
        case soundfile::FLOAT:
            frames = sf_readf_float(sndfile, data_f, MUSICSAMP);
            break;
        case soundfile::SHORT:
            frames = sf_readf_short(sndfile, data_s, MUSICSAMP);
            break;
        default: return false;
    }

    if(frames <= 0)
    {
        if(!retry)
        {
            sf_seek(sndfile, 0, SF_SEEK_SET);
            return fillmus(true);
        }
        return false;
    }
    len = (ALsizei)(frames * info.channels) * (ALsizei)size;
    return true;
}

bool soundfile::setupsnd()
{
    switch(type)
    {
        case soundfile::FLOAT:
            data_f = new float[size_t((info.frames * info.channels) * size)];
            frames = sf_readf_float(sndfile, data_f, info.frames);
            break;
        case soundfile::SHORT:
            data_s = new short[size_t((info.frames * info.channels) * size)];
            frames = sf_readf_short(sndfile, data_s, info.frames);
            break;
        default: return false;
    }

    if(frames <= 0) return false;

    if(mixtype == soundfile::MONO && info.channels > 1)
    { // sources need to be downmixed to mono if soft spatialize extension isn't found
        switch(type)
        {
            case soundfile::SHORT:
            {
                short *data = data_s;
                data_s = new short[(size_t)frames * size];
                loopi(frames)
                {
                    int avg = 0;
                    loopj(info.channels) avg += data[i*info.channels + j];
                    data_s[i] = short(avg/info.channels);
                }
                format = AL_FORMAT_MONO16;
                info.channels = 1;
                delete[] data;
                break;
            }
            case soundfile::FLOAT:
            {
                float *data = data_f;
                data_f = new float[(size_t)frames * size];
                loopi(frames)
                {
                    float avg = 0;
                    loopj(info.channels) avg += data[i*info.channels + j];
                    data_f[i] = avg/info.channels;
                }
                format = AL_FORMAT_MONO_FLOAT32;
                info.channels = 1;
                delete[] data;
                break;
            }
            default: break;
        }
    }
    len = (ALsizei)(frames * info.channels) * (ALsizei)size;
    return true;
}

void soundfile::reset()
{
    type = INVALID;
    mixtype = MAXMIX;
    len = 0;
    frames = 0;
    size = 0;
    format = AL_NONE;
    data_s = NULL;
    sndfile = NULL;
    viofile = NULL;
    memset(&info, 0, sizeof (info));
}

void soundfile::clear()
{
    switch(type)
    {
        case SHORT: if(data_s) delete[] data_s; break;
        case FLOAT: if(data_f) delete[] data_f; break;
        default: break;
    }
    if(sndfile) sf_close(sndfile);
    if(viofile) viofile->close();
    reset();
}

ALenum soundsample::setup(soundfile *s)
{
    SOUNDERROR();
    if(alIsBuffer(buffer))
    {
        alDeleteBuffers(1, &buffer);
        SOUNDERRORTRACK(clear(); return err);
    }
    alGenBuffers(1, &buffer);
    SOUNDERRORTRACK(clear(); return err);

    time = s->frames / (float)s->info.samplerate;

    switch(s->type)
    {
        case soundfile::SHORT: alBufferData(buffer, s->format, s->data_s, s->len, s->info.samplerate); break;
        case soundfile::FLOAT: alBufferData(buffer, s->format, s->data_f, s->len, s->info.samplerate); break;
        default: return AL_INVALID_OPERATION;
    }
    SOUNDERRORTRACK(clear(); return err);
    return AL_NO_ERROR;
}

void soundsample::reset()
{
    buffer = AL_INVALID;
}

void soundsample::cleanup()
{
    if(valid()) alDeleteBuffers(1, &buffer);
    buffer = AL_INVALID;
}

void soundsample::clear()
{
    DELETEA(name);
    cleanup();
    reset();
}

bool soundsample::valid()
{
    if(nosound || !alIsBuffer(buffer)) return false;
    return true;
}

void soundslot::reset()
{
    DELETEA(name);
    samples.shrink(0);
}

ALenum soundsource::setup(soundsample *s)
{
    if(!s->valid()) return AL_NO_ERROR;

    SOUNDERROR();
    if(alIsSource(source))
    {
        alDeleteSources(1, &source);
        SOUNDERRORTRACK(clear(); return err);
    }
    alGenSources(1, &source);
    SOUNDERRORTRACK(clear(); return err);

    finalrolloff = rolloff >= 0 ? rolloff : (slot->rolloff >= 0 ? slot->rolloff : 1.f);
    finalrefdist = refdist >= 0 ? refdist : (slot->refdist >= 0 ? slot->refdist : soundrefdist);

    alSourcei(source, AL_BUFFER, s->buffer);
    SOUNDERRORTRACK(clear(); return err);

    if(flags&SND_NOPAN && flags&SND_NODIST) flags |= SND_NOATTEN; // superfluous
    if(flags&SND_NOATTEN) flags &= ~(SND_NOPAN|SND_NODIST); // unnecessary

    if(al_soft_spatialize)
    {
        alSourcei(source, AL_SOURCE_SPATIALIZE_SOFT, AL_TRUE);
        SOUNDERRORTRACK(clear(); return err);
    }

    alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
    SOUNDERRORTRACK(clear(); return err);

    if(al_ext_efx)
    {
        if(!(flags&SND_NOFILTER) && !(flags&SND_NOATTEN))
        {
            if(alIsFilter(dirfilter))
            {
                alDeleteFilters(1, &dirfilter);
                SOUNDERRORTRACK(clear(); return err);
            }
            alGenFilters(1, &dirfilter);
            SOUNDERRORTRACK(clear(); return err);

            alFilteri(dirfilter, AL_FILTER_TYPE, AL_FILTER_BANDPASS);
            SOUNDERRORTRACK(clear(); return err);

            alSourcei(source, AL_DIRECT_FILTER, dirfilter);
            SOUNDERRORTRACK(clear(); return err);
        }

        if(!(flags&SND_NOENV))
        {
            if(alIsFilter(efxfilter))
            {
                alDeleteFilters(1, &efxfilter);
                SOUNDERRORTRACK(clear(); return err);
            }
            alGenFilters(1, &efxfilter);
            SOUNDERRORTRACK(clear(); return err);

            // Reduce effect gain for sounds with high attenuation
            float effectgain = 1.0f - clamp(finalrolloff / finalrefdist, 0.0f, 1.0f);
            effectgain = powf(effectgain, 4.0f);

            alFilteri(efxfilter, AL_FILTER_TYPE, AL_FILTER_HIGHPASS);
            alFilterf(efxfilter, AL_HIGHPASS_GAIN, effectgain);
            alFilterf(efxfilter, AL_HIGHPASS_GAINLF, effectgain * effectgain);
            SOUNDERRORTRACK(clear(); return err);
        }
    }

    alSourcei(source, AL_LOOPING, flags&SND_LOOP ? AL_TRUE : AL_FALSE);
    SOUNDERRORTRACK(clear(); return err);

    alSourcef(source, AL_ROLLOFF_FACTOR, finalrolloff);
    SOUNDERRORTRACK(clear(); return err);

    alSourcef(source, AL_REFERENCE_DISTANCE, finalrefdist);
    SOUNDERRORTRACK(clear(); return err);

    float dist = maxdist >= 0 ? maxdist : slot->maxdist;
    if(dist >= 0)
    {
        alSourcef(source, AL_MAX_DISTANCE, dist);
        SOUNDERRORTRACK(clear(); return err);
    }

    if(flags&SND_MAP && flags&SND_LOOP)
    {
        int millis = lastmillis - mapsoundmillis;
        float finaloffset = fmodf((millis / 1000.0f) + offset, s->time);
        alSourcef(source, AL_SEC_OFFSET, finaloffset);
        SOUNDERRORTRACK(clear(); return err);
    }

    if(flags&SND_DUCKING)
    {
        ducking = true;
        duckers++;
    }

    return AL_NO_ERROR;
}

void soundsource::cleanup()
{
    if(ducking)
    {
        ducking = false;
        duckers--;
    }

    if(cshook)
    {
        execute(cshook, NULL, 0);
        cshook = NULL;
    }

    if(valid())
    {
        if(al_ext_efx)
        {
            if(alIsFilter(dirfilter)) alDeleteFilters(1, &dirfilter);
            dirfilter = AL_INVALID;
            if(alIsFilter(efxfilter)) alDeleteFilters(1, &efxfilter);
            efxfilter = AL_INVALID;
        }

        alSourceStop(source);
        alDeleteSources(1, &source);
    }

    source = AL_INVALID;
}

void soundsource::reset(bool dohook)
{
    source = dirfilter = efxfilter = AL_INVALID;
    pos = curpos = vel = vec(0, 0, 0);
    vpos = NULL;
    slot = NULL;
    owner = NULL;
    gain = curgain = pitch = curpitch = 1;
    material = MAT_AIR;
    flags = millis = ends = 0;
    rolloff = refdist = maxdist = -1;
    slotnum = lastupdate = -1;
    offset = 0;
    if(dohook)
    {
        if(hook) *hook = -1;
        hook = NULL;
    }
    buffer.shrink(0);
    mute = false;
    ducking = occluded = false;
    fade = 1.0f;
    cshook = NULL;
}

void soundsource::clear(bool dohook)
{
    cleanup();
    reset(dohook);
}

void soundsource::unhook()
{
    if(hook) *hook = -1;
    hook = NULL;
    vpos = NULL;
    vel = vec(0, 0, 0);
    owner = NULL;
    flags &= ~(SND_TRACKED|SND_LOOP);
}

ALenum soundsource::updatefilter()
{
    if(!al_ext_efx || !alIsFilter(dirfilter)) return AL_NO_ERROR;

    float gainhf = 1.0f, gainlf = 1.0f;
    
    if(sounddistfilter > 0.0f)
        gainhf = gainlf = clamp(1.0f - (((camera1->o.dist(pos) * log10f(finalrolloff + 1.0f)) / finalrefdist) * sounddistfilter), 1.0f - sounddistfilter, 1.0f);

    if(occluded && soundoccludedhf < 1.0f)
        gainhf *= soundoccludedhf;


    alFilteri(dirfilter, AL_FILTER_TYPE, AL_FILTER_BANDPASS);
    alFilterf(dirfilter, AL_BANDPASS_GAINHF, gainhf);
    alFilterf(dirfilter, AL_BANDPASS_GAINLF, gainlf);
    alSourcei(source, AL_DIRECT_FILTER, dirfilter);

    return alGetError();
}

ALenum soundsource::update()
{
    if(flags&SND_TRACKED)
    {
        if(owner) vpos = game::getplayersoundpos(owner);
        if(vpos) pos = *vpos;
    }
    material = lookupmaterial(pos);

    if(mute) curgain = 0.0f;
    else curgain = clamp(soundeffectvol * slot->gain * (flags&SND_MAP ? soundeffectenv : soundeffectevent), 0.f, 100.f);


    if(!ducking) curgain *= lerp(1.0f, soundduckfactor, soundduckstate);

    if(flags&SND_MAP)
    {
        float fadeval = groupid && insideenvzone ?
            (1.0f - (insideenvzone->fadevals[groupid - 1] / 100.0f)) :
            1.0f;

        fade = lerpstep(fade, fadeval, mapsoundfadespeed*curtime);

        curgain *= fade;
    }

    SOUNDERROR();
    if(flags&SND_CLAMPED)
    {
        alSourcef(source, AL_MIN_GAIN, gain);
        SOUNDERRORTRACK(clear(); return err);
    }
    else curgain *= gain;

    if(!(flags&SND_NOFILTER) && !(flags&SND_NOATTEN) && (soundoccluded < 1.0f || soundoccludedhf < 1.0f))
    {
        occluded = rayoccluded(camera1->o, curpos);
        if(occluded && soundoccluded < 1.0f) curgain *= soundoccluded;
    }
    else occluded = false;
    
    alSourcef(source, AL_GAIN, curgain);
    SOUNDERRORTRACK(clear(); return err);

    curpitch = clamp(pitch * slot->pitch, FVAR_NONZERO, 100.f);

    alSourcef(source, AL_PITCH, curpitch);
    SOUNDERRORTRACK(clear(); return err);

    alSourcei(source, AL_LOOPING, flags&SND_LOOP ? AL_TRUE : AL_FALSE);
    SOUNDERRORTRACK(clear(); return err);

    vec rpos = pos;
    if(flags&SND_NOATTEN) rpos = camera1->o;
    else if(flags&SND_NOPAN)
        rpos = vec(camera1->yaw * RAD, camera1->pitch * RAD).mul(vec(rpos).sub(camera1->o).magnitude()).add(camera1->o);
    else if(flags&SND_NODIST) rpos.sub(camera1->o).safenormalize().add(camera1->o);

    alSourcefv(source, AL_POSITION, (ALfloat *) &rpos);
    SOUNDERRORTRACK(clear(); return err);

    if(totalmillis != lastupdate)
    {
        if(flags&(SND_NOATTEN|SND_NODIST|SND_NOPAN)) vel = vec(0, 0, 0);
        else if(owner) vel = owner->vel;
        else if(flags&SND_VELEST && (lastupdate < 0 || totalmillis - lastupdate > physics::physframetime))
        {
            int n = physics::physframetime;
            while(lastupdate + n < totalmillis) n += physics::physframetime;
            vel = vec(pos).sub(curpos).mul(1000).div(n);
        }

        lastupdate = totalmillis;
        curpos = pos;
    }

    alSourcefv(source, AL_VELOCITY, (ALfloat *) &vel);
    SOUNDERRORTRACK(clear(); return err);

    updatefilter();

    if(!(flags&SND_NOENV))
    {
        soundenvzone *zone = getactiveenvzone(pos);

        ALuint zoneslot = zone && zone->isvalid() ?
            zone->getefxslot() : AL_EFFECTSLOT_NULL;

        ALuint insideslot = insideenvzone && insideenvzone->isvalid() && zone != insideenvzone ?
            insideenvzone->getefxslot() : AL_EFFECTSLOT_NULL;

        ALuint filter = alIsFilter(efxfilter) ? efxfilter : AL_FILTER_NULL;

        alSourcei(source, AL_AUXILIARY_SEND_FILTER_GAIN_AUTO, AL_TRUE);
        alSource3i(source, AL_AUXILIARY_SEND_FILTER, zoneslot, 0, filter);
        alSource3i(source, AL_AUXILIARY_SEND_FILTER, insideslot, 1, filter);

        SOUNDERRORTRACK(clear(); return err);
    }

    if(flags&SND_PREVIEW && lastmillis - millis > soundpreviewlength) clear();

    return AL_NO_ERROR;
}

bool soundsource::valid()
{
    if(nosound || !alIsSource(source)) return false;
    return true;
}

bool soundsource::active()
{
    if(!valid()) return false;
    ALint value;
    alGetSourcei(source, AL_SOURCE_STATE, &value);
    if(value != AL_PLAYING && value != AL_PAUSED) return false;
    return true;
}

bool soundsource::playing()
{
    if(!valid()) return false;
    ALint value;
    alGetSourcei(source, AL_SOURCE_STATE, &value);
    if(value != AL_PLAYING) return false;
    return true;
}

ALenum soundsource::play()
{
    if(playing()) return AL_NO_ERROR;
    SOUNDERROR();
    alSourcePlay(source);
    SOUNDERRORTRACK(clear(); return err);
    return AL_NO_ERROR;
}

ALenum soundsource::pause()
{
    if(playing()) return AL_NO_ERROR;
    SOUNDERROR();
    alSourcePause(source);
    SOUNDERRORTRACK(clear(); return err);
    return AL_NO_ERROR;
}

ALenum soundsource::stop()
{
    if(playing()) return AL_NO_ERROR;
    SOUNDERROR();
    alSourceStop(source);
    SOUNDERRORTRACK(clear(); return err);
    return AL_NO_ERROR;
}

ALenum soundsource::push(soundsample *s)
{
    SOUNDCHECK(setup(s), , return err);
    SOUNDCHECK(update(), , return err);
    SOUNDCHECK(play(), , return err);
    return AL_NO_ERROR;
}

void soundsource::setcshook(ident *id)
{
    if(!id || id->type != ID_ALIAS) return;
    cshook = id;
}

#define MUSICTAG(x,y) \
    do { \
        const char *tag##x = sf_get_string(s->sndfile, y); \
        if(tag##x != NULL) x = newstring(tag##x); \
    } while(false);

ALenum musicstream::setup(const char *n, soundfile *s, bool looped, int fadems, int fadet)
{
    if(!s) return AL_NO_ERROR;

    name = newstring(n);
    MUSICTAG(artist, SF_STR_ARTIST);
    MUSICTAG(title, SF_STR_TITLE);
    MUSICTAG(album, SF_STR_ALBUM);

    SOUNDERROR();
    loopi(MUSICBUFS) if(alIsBuffer(buffer[i]))
    {
        alDeleteBuffers(1, &buffer[i]);
        SOUNDERRORTRACK(clear(); return err);
    }
    alGenBuffers(MUSICBUFS, buffer);
    SOUNDERRORTRACK(clear(); return err);

    if(alIsSource(source))
    {
        alDeleteSources(1, &source);
        SOUNDERRORTRACK(clear(); return err);
    }
    alGenSources(1, &source);
    SOUNDERRORTRACK(clear(); return err);

    alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
    SOUNDERRORTRACK(clear(); return err);

    alSourcef(source, AL_ROLLOFF_FACTOR, 0.f);
    SOUNDERRORTRACK(clear(); return err);

    alSource3f(source, AL_POSITION, 0.f, 0.f, 0.f);
    SOUNDERRORTRACK(clear(); return err);

    data = s;

    int r = 0;
    for(; r < MUSICBUFS; r++)
    {
        if(!data->fillmus()) break;
        SOUNDCHECKTRACK(fill(buffer[r]), , return err);
    }

    alSourceQueueBuffers(source, r, buffer);
    SOUNDERRORTRACK(clear(); return err);

    looping = looped;
    fadeout = false;
    fademillis = fadems;
    fadetime = fadet;

    updategain();

    return AL_NO_ERROR;
}

ALenum musicstream::fill(ALint bufid)
{
    SOUNDERROR();
    switch(data->type)
    {
        case soundfile::SHORT: alBufferData(bufid, data->format, data->data_s, (ALsizei)data->len, data->info.samplerate); break;
        case soundfile::FLOAT: alBufferData(bufid, data->format, data->data_f, (ALsizei)data->len, data->info.samplerate); break;
        default: return AL_INVALID_OPERATION;
    }
    SOUNDERRORTRACK(clear(); return err);
    return AL_NO_ERROR;
}

void musicstream::cleanup()
{
    if(valid())
    {
        alSourceStop(source);
        alDeleteSources(1, &source);
        alDeleteBuffers(MUSICBUFS, buffer);
    }

    if(name) delete[] name;
    if(artist) delete[] artist;
    if(title) delete[] title;
    if(album) delete[] album;
    if(data) delete data;

    source = AL_INVALID;
}

void musicstream::reset()
{
    name = artist = title = album = NULL;
    source = AL_INVALID;
    loopi(MUSICBUFS) buffer[i] = 0;
    data = NULL;
    gain = 1;
    looping = true;
    fadeout = false;
    fademillis = fadetime = 0;
}

void musicstream::clear()
{
    cleanup();
    reset();
}

bool musicstream::updategain()
{
    if(musicstopping)
    {
        gain = 0;
        return false;
    }

    gain = soundmusicvol * lerp(1.0f, musicduckfactor, soundduckstate);

    if(fademillis)
    {
        if(fadetime)
        {
            int ms = getclockticks() - fademillis;
            if(ms >= 0 && ms <= fadetime)
            {
                float amt = ms / float(fadetime);
                if(fadeout) amt = 1.0f - amt;
                gain *= amt;
            }
            else fademillis = 0;
        }
        else fademillis = 0;

        if(!fademillis && fadeout)
        {
            gain = 0;
            return false;
        }
    }

    return true;
}

ALenum musicstream::update()
{
    if(!valid()) return AL_INVALID_OPERATION;

    if(!updategain()) return -1; // not an error code, but an error

    SOUNDERROR();
    ALint processed;
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
    SOUNDERRORTRACK(clear(); return err);

    while(processed > 0)
    {
        ALuint bufid;
        alSourceUnqueueBuffers(source, 1, &bufid);
        SOUNDERRORTRACK(clear(); return err);
        processed--;

        if(!data->fillmus(!looping)) continue;
        SOUNDCHECKTRACK(fill(bufid), , return err);

        alSourceQueueBuffers(source, 1, &bufid);
        SOUNDERRORTRACK(clear(); return err);
    }
    alSourcef(source, AL_GAIN, gain);
    SOUNDERRORTRACK(clear(); return err);

    if(!playing())
    {
        ALint queued;

        alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);
        SOUNDERRORTRACK(clear(); return err);
        if(queued)
        {
            alSourcePlay(source);
            SOUNDERRORTRACK(clear(); return err);
        }
        else clear();
    }

    return AL_NO_ERROR;
}

bool musicstream::valid()
{
    if(nosound || !alIsSource(source)) return false;
    loopi(MUSICBUFS) if(!alIsBuffer(buffer[i])) return false;
    return true;
}

bool musicstream::active()
{
    if(!valid()) return false;
    ALint value;
    alGetSourcei(source, AL_SOURCE_STATE, &value);
    if(value != AL_PLAYING && value != AL_PAUSED) return false;
    return true;
}

bool musicstream::playing()
{
    if(!valid()) return false;
    ALint value;
    alGetSourcei(source, AL_SOURCE_STATE, &value);
    if(value != AL_PLAYING) return false;
    return true;
}

ALenum musicstream::play()
{
    if(playing()) return AL_NO_ERROR;
    SOUNDERROR();
    alSourcePlay(source);
    SOUNDERRORTRACK(clear(); return err);
    return AL_NO_ERROR;
}

ALenum musicstream::pause()
{
    if(playing()) return AL_NO_ERROR;
    SOUNDERROR();
    alSourcePause(source);
    SOUNDERRORTRACK(clear(); return err);
    return AL_NO_ERROR;
}

ALenum musicstream::stop()
{
    if(playing()) return AL_NO_ERROR;
    SOUNDERROR();
    alSourceStop(source);
    SOUNDERRORTRACK(clear(); return err);
    return AL_NO_ERROR;
}

ALenum musicstream::push(const char *n, soundfile *s)
{
    SOUNDCHECK(setup(n, s), , return err);
    SOUNDCHECK(update(), , return err);
    SOUNDCHECK(play(), , return err);
    return AL_NO_ERROR;
}

ICOMMAND(0, mapsoundinfo, "i", (int *index), {
    if(*index < 0) intret(mapsounds.length());
    else if(*index < mapsounds.length()) result(mapsounds[*index].name);
});

void getsounds(bool mapsnd, int idx, int prop)
{
    vector<soundslot> &soundset = mapsnd ? mapsounds : gamesounds.vec();
    if(idx < 0) intret(soundset.length());
    else if(soundset.inrange(idx))
    {
        if(prop < 0) intret(7);
        else switch(prop)
        {
            case 0: result(soundset[idx].name); break;
            case 1: floatret(soundset[idx].gain); break;
            case 2: floatret(soundset[idx].pitch); break;
            case 3: floatret(soundset[idx].rolloff); break;
            case 4: floatret(soundset[idx].refdist); break;
            case 5: floatret(soundset[idx].maxdist); break;
            case 6: intret(soundset[idx].variants); break;
            default: break;
        }
    }
}
ICOMMAND(0, getsound, "ibb", (int *n, int *v, int *p), getsounds(*n!=0, *v, *p));

void getcursounds(int idx, int prop, int buf)
{
    if(idx < 0) intret(soundsources.length());
    else if(soundsources.inrange(idx))
    {
        soundsource &s = soundsources[idx];
        if(prop < 0) intret(21);
        else switch(prop)
        {
            case 0: intret(s.index); break;
            case 1: intret(s.flags); break;
            case 2: intret(s.material); break;
            case 3: intret(s.millis); break;
            case 4: intret(s.ends); break;
            case 5: intret(s.slotnum); break;
            case 6: floatret(s.gain); break;
            case 7: floatret(s.curgain); break;
            case 8: floatret(s.pitch); break;
            case 9: floatret(s.curpitch); break;
            case 10: floatret(s.rolloff); break;
            case 11: floatret(s.refdist); break;
            case 12: floatret(s.maxdist); break;
            case 13: defformatstring(pos, "%.f %.f %.f", s.pos.x, s.pos.y, s.pos.z); result(pos); break;
            case 14: defformatstring(vel, "%.f %.f %.f", s.vel.x, s.vel.y, s.vel.z); result(vel); break;
            case 15:
            {
                if(!s.buffer.inrange(buf))
                {
                    intret(s.buffer.length());
                    break;
                }
                intret(s.buffer[buf]);
                break;
            }
            case 16: intret(s.valid() ? 1 : 0); break;
            case 17: intret(s.playing() ? 1 : 0); break;
            case 18: intret(s.active() ? 1 : 0); break;
            case 19: intret(client::getcn(s.owner)); break;
            case 20: intret(s.owner == camera1 ? 1 : 0); break;
            default: break;
        }
    }
}
ICOMMAND(0, getcursound, "bbb", (int *n, int *p, int *b), getcursounds(*n, *p, *b));

void getmusics(int prop)
{
    if(prop < 0)
    {
        intret(9);
        return;
    }
    SDL_LockMutex(music_mutex);
    if(music) switch(prop)
    {
        case 0: result(music->name); break;
        case 1: result(music->artist); break;
        case 2: result(music->title); break;
        case 3: result(music->album); break;
        case 4: intret(music->looping ? 1 : 0); break;
        case 5: floatret(music->gain); break;
        case 6: intret(music->valid() ? 1 : 0); break;
        case 7: intret(music->active() ? 1 : 0); break;
        case 8: intret(music->playing() ? 1 : 0); break;
        default: break;
    }
    SDL_UnlockMutex(music_mutex);
}
ICOMMAND(0, getmusic, "b", (int *p), getmusics(*p));

static void initsoundenv() { initprops(newsoundenv->props, soundenvprops, SOUNDENV_PROPS); }

static int findsoundenv(const char *name)
{
    loopv(soundenvs) if(!strcmp(soundenvs[i]->name, name))
        return i;

    return -1;
}

static int makesoundenv(const char *name, bool noinit)
{
    int index = findsoundenv(name);

    if(index < 0)
    {
        newsoundenv = Sharedptr<soundenv>::make();
        newsoundenv->name = newstring(name);

        index = soundenvs.length();
        soundenvs.add(newsoundenv);
    }
    else newsoundenv = soundenvs[index];

    if(!noinit) initsoundenv();

    return index;
}

static void defsoundenv(const char *name, uint *code, bool noinit)
{
    int index = makesoundenv(name, noinit);

    execute(code);

    newsoundenv->updatezoneparams(index);
}
ICOMMAND(0, defsoundenv, "seiN", (char *name, uint *code, int *noinit, int *numargs),
    defsoundenv(name, code, *numargs > 2 && *noinit));

static void dupsoundenv(const char *name, const char *srcname)
{
    int srcindex = findsoundenv(srcname);
    if(srcindex < 0) return;

    int newindex = makesoundenv(name, false);
    Sharedptr<soundenv> srcenv = soundenvs[srcindex],
                        newenv = soundenvs[newindex];

    vector<uchar> props;
    packprops(props, srcenv.get()->props, SOUNDENV_PROPS);
    unpackprops(props, newenv.get()->props, SOUNDENV_PROPS);
}
ICOMMAND(0, dupsoundenv, "ss", (char *name, char *srcname), dupsoundenv(name, srcname));

ICOMMAND(0, remsoundenv, "s", (char *name),
{
    int index = findsoundenv(name);
    if(index >= 0) soundenvs.remove(index);
});

ICOMMAND(0, renamesoundenv, "ss", (char *oldname, char *newname),
{
    bool result = false;
    int index = findsoundenv(oldname);

    if(index >=0 && findsoundenv(newname) < 0)
    {
        Sharedptr<soundenv> env = soundenvs[index];
        DELETEA(env->name);
        env->name = newstring(newname);
        result = true;
    }

    intret(result);
});

template<class T>
static void setsoundenvprop(const char *name, const T &val)
{
    if(!newsoundenv)
    {
        conoutf(colourred, "Error: cannot assign property %s, not loading soundenvs", name);
        return;
    }

    property *p = findprop(name, newsoundenv->props, SOUNDENV_PROPS);

    if(!p)
    {
        conoutf(colourred, "Error: %s, soundenv property %s not found", newsoundenv->getname(), name);
        return;
    }

    p->set(val);
}

ICOMMAND(0, soundenvpropi, "si", (char *name, int *ival),
    setsoundenvprop(name, *ival));
ICOMMAND(0, soundenvpropf, "sf", (char *name, float *fval),
    setsoundenvprop(name, *fval));

ICOMMAND(0, getsoundenvprop, "ss", (char *envname, char *propname),
{
    int index = findsoundenv(envname);
    if(index >= 0)
    {
        property *p = findprop(propname, soundenvs[index]->props, SOUNDENV_PROPS);
        if(p) p->commandret();
    }
});

ICOMMAND(0, getsoundenvpropmin, "ss", (char *envname, char *propname),
{
    int index = findsoundenv(envname);
    if(index >= 0)
    {
        property *p = findprop(propname, soundenvs[index]->props, SOUNDENV_PROPS);
        if(p) p->commandretmin();
    }
});

ICOMMAND(0, getsoundenvpropmax, "ss", (char *envname, char *propname),
{
    int index = findsoundenv(envname);
    if(index >= 0)
    {
        property *p = findprop(propname, soundenvs[index]->props, SOUNDENV_PROPS);
        if(p) p->commandretmax();
    }
});

ICOMMAND(0, getsoundenvpropdef, "ss", (char *envname, char *propname),
{
    int index = findsoundenv(envname);
    if(index >= 0)
    {
        property *p = findprop(propname, soundenvs[index]->props, SOUNDENV_PROPS);
        if(p) p->commandretdefault();
    }
});

static void dumpsoundenv(soundenv &env, stream *s)
{
    s->printf("defsoundenv %s [\n", escapestring(env.name));

    loopi(SOUNDENV_PROPS)
    {
        property &prop = env.props[i];
        switch(prop.type)
        {
            case PROP_INT:
                s->printf("    soundenvpropi %s %d\n", prop.def->name, prop.get<int>());
                break;

            case PROP_FLOAT:
                s->printf("    soundenvpropf %s %f\n", prop.def->name, prop.get<float>());
                break;
        }
    }

    s->printf("]\n");
}

void dumpsoundenvs(stream *s) { loopv(soundenvs) dumpsoundenv(*soundenvs[i], s); }

ICOMMAND(0, soundenvinfo, "i", (int *index), {
    if(*index < 0) intret(soundenvs.length());
    else if(*index < soundenvs.length()) result(soundenvs[*index]->name);
});

#ifdef WIN32

#include <wchar.h>

#else

#include <unistd.h>

#if _POSIX_SHARED_MEMORY_OBJECTS > 0
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <wchar.h>
#endif

#endif

#if defined(WIN32) || _POSIX_SHARED_MEMORY_OBJECTS > 0
struct MumbleInfo
{
    int version, timestamp;
    vec pos, front, top;
    wchar_t name[256];
};
#endif

#ifdef WIN32
static HANDLE mumblelink = NULL;
static MumbleInfo *mumbleinfo = NULL;
#define VALID_MUMBLELINK (mumblelink && mumbleinfo)
#elif _POSIX_SHARED_MEMORY_OBJECTS > 0
static int mumblelink = -1;
static MumbleInfo *mumbleinfo = (MumbleInfo *)-1;
#define VALID_MUMBLELINK (mumblelink >= 0 && mumbleinfo != (MumbleInfo *)-1)
#endif

#ifdef VALID_MUMBLELINK
VARF(IDF_PERSIST, mumble, 0, 1, 1, { if(mumble) initmumble(); else closemumble(); });
#else
VARF(IDF_PERSIST, mumble, 0, 0, 1, { if(mumble) initmumble(); else closemumble(); });
#endif

void initmumble()
{
    if(!mumble) return;
#ifdef VALID_MUMBLELINK
    if(VALID_MUMBLELINK) return;

    #ifdef WIN32
        mumblelink = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "MumbleLink");
        if(mumblelink)
        {
            mumbleinfo = (MumbleInfo *)MapViewOfFile(mumblelink, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MumbleInfo));
            if(mumbleinfo) wcsncpy(mumbleinfo->name, (const wchar_t *)versionuname, 256);
        }
    #elif _POSIX_SHARED_MEMORY_OBJECTS > 0
        defformatstring(shmname, "/MumbleLink.%d", getuid());
        mumblelink = shm_open(shmname, O_RDWR, 0);
        if(mumblelink >= 0)
        {
            mumbleinfo = (MumbleInfo *)mmap(NULL, sizeof(MumbleInfo), PROT_READ|PROT_WRITE, MAP_SHARED, mumblelink, 0);
            if(mumbleinfo != (MumbleInfo *)-1) wcsncpy(mumbleinfo->name, (const wchar_t *)versionuname, 256);
        }
    #endif
    if(!VALID_MUMBLELINK) closemumble();
#else
    conoutf(colourred, "Mumble positional audio is not available on this platform.");
#endif
}

void closemumble()
{
#ifdef WIN32
    if(mumbleinfo) { UnmapViewOfFile(mumbleinfo); mumbleinfo = NULL; }
    if(mumblelink) { CloseHandle(mumblelink); mumblelink = NULL; }
#elif _POSIX_SHARED_MEMORY_OBJECTS > 0
    if(mumbleinfo != (MumbleInfo *)-1) { munmap(mumbleinfo, sizeof(MumbleInfo)); mumbleinfo = (MumbleInfo *)-1; }
    if(mumblelink >= 0) { close(mumblelink); mumblelink = -1; }
#endif
}

static inline vec mumblevec(const vec &v, bool pos = false)
{
    // change from X left, Z up, Y forward to X right, Y up, Z forward
    // 8 cube units = 1 meter
    vec m(-v.x, v.z, v.y);
    if(pos) m.div(8);
    return m;
}

void updatemumble()
{
#ifdef VALID_MUMBLELINK
    if(!VALID_MUMBLELINK) return;

    static int timestamp = 0;

    mumbleinfo->version = 1;
    mumbleinfo->timestamp = ++timestamp;

    mumbleinfo->pos = mumblevec(camera1->o, true);
    mumbleinfo->front = mumblevec(vec(RAD*camera1->yaw, RAD*camera1->pitch));
    mumbleinfo->top = mumblevec(vec(RAD*camera1->yaw, RAD*(camera1->pitch+90)));
#endif
}
