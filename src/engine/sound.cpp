#include "engine.h"

ALCdevice *snddev = NULL;
ALCcontext *sndctx = NULL;

bool nosound = true, canmusic = false;
bool al_ext_efx = false, al_soft_spatialize = false, al_ext_float32 = false;

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

hashnameset<soundsample> soundsamples;
slotmanager<soundslot> gamesounds, mapsounds;
vector<slot *> soundmap;
vector<soundsource> soundsources;
SDL_Thread *music_thread;
SDL_mutex *music_mutex;
musicstream *music = NULL;

slotmanager<soundenv> soundenvs;
vector<soundenvzone> envzones;
vector<soundenvzone *> sortedenvzones;
soundenvzone *insideenvzone = NULL;

soundenv *newsoundenv = NULL;
vector<soundefxslot> soundefxslots;

static soundenv *soundenvfroment(entity *ent)
{
    ASSERT(ent);

    int index = ent->attrs[0];
    if(soundenvs.inrange(index)) return &soundenvs[index];

    return NULL;
}

void allocsoundefxslots();
VARF(IDF_PERSIST, maxsoundefxslots, 1, 2, 10, allocsoundefxslots());
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

        if(soundefxslotdebug) conoutf("getsoundefxslot: free slot found: %u", soundefxslots[i].id);

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

    if(soundefxslotdebug) conoutf("getsoundefxslot: oldest slot found: %u, last used %u ms ago",
        oldest->id, lastmillis - oldest->lastused);
}

void soundenv::setparams(ALuint effect)
{
    alEffecti (effect, AL_EFFECT_TYPE,                     AL_EFFECT_EAXREVERB);
    alEffectf (effect, AL_EAXREVERB_DENSITY,               props[SOUNDENV_PROP_DENSITY]);
    alEffectf (effect, AL_EAXREVERB_DIFFUSION,             props[SOUNDENV_PROP_DIFFUSION]);
    alEffectf (effect, AL_EAXREVERB_GAIN,                  props[SOUNDENV_PROP_GAIN]);
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

void soundenv::updatezoneparams()
{
    loopv(envzones)
    {
        soundenvzone &zone = envzones[i];
        if(zone.env == this && zone.efxslot) zone.attachparams();
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
    if(efxslot) efxslot->put();
}

int soundenvzone::getvolume()
{
    if(!ent) return 0;
    return ent->attrs[1] * ent->attrs[2] * ent->attrs[3];
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
            int curvolume = !insideenvzone ? 0 : insideenvzone->getvolume();
            int newvolume = zone.getvolume();

            if(!insideenvzone || newvolume < curvolume) insideenvzone = &zone;
        }

        for(int j = 0; i && j <= sortedenvzones.length(); ++j)
        {
            if(j >= sortedenvzones.length() ||
                dist <= camera1->o.dist_to_bb(sortedenvzones[j]->bbmin, sortedenvzones[j]->bbmax))
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

        int curvolume = !bestzone ? 0 : bestzone->getvolume();
        int newvolume = zone->getvolume();

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

void buildenvzones()
{
    envzones.shrink(0);
    vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity *ent = ents[i];
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

SVARF(IDF_INIT, sounddevice, "", initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(IDF_INIT, soundmaxsources, 16, 256, 1024, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(IDF_INIT, sounddownmix, 0, 0, 1, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));

#define SOUNDVOL(oldname, newname, def, body) \
    FVARF(IDF_PERSIST, sound##newname##vol, 0, def, 100, body); \
    ICOMMAND(0, oldname##vol, "iN$", (int *n, int *numargs, ident *id), \
        if(*numargs > 0) \
        { \
            sound##newname##vol = clamp(*n, 0, 255) / 255.f; \
            body; \
        } \
        else if(*numargs < 0) intret(int(sound##newname##vol * 255)); \
        else printvar(id, int(sound##newname##vol * 255)); \
    );

SOUNDVOL(master, master, 1.f, );
SOUNDVOL(sound, effect, 1.f, );
SOUNDVOL(music, music, 0.25f, updatemusic());
FVARF(IDF_PERSIST, soundeffectevent, 0, 1, 100, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
FVARF(IDF_PERSIST, soundeffectenv, 0, 1, 100, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
FVAR(IDF_PERSIST, sounddistfilter, 0.0f, 0.3f, 1.0f);

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
    if(err) conoutf("\frFailed to set doppler factor: %s", err);
}
FVARF(IDF_PERSIST, sounddoppler, 0, 0.1f, FVAR_MAX, soundsetdoppler(sounddoppler));

void soundsetspeed(float v)
{
    if(nosound) return;
    alGetError();
    alSpeedOfSound(v * 8.f);
    const char *err = sounderror(false);
    if(err) conoutf("\frFailed to set speed of sound: %s", err);
}
FVARF(IDF_PERSIST, soundspeed, FVAR_NONZERO, 343.3f, FVAR_MAX, soundsetspeed(sounddoppler));

FVARF(IDF_PERSIST, soundrefdist, FVAR_NONZERO, 16, FVAR_MAX, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
FVARF(IDF_PERSIST, soundrolloff, FVAR_NONZERO, 256, FVAR_MAX, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));

void updatemusic()
{
    SDL_LockMutex(music_mutex);
    bool hasmusic = music != NULL;
    SDL_UnlockMutex(music_mutex);

    if(!connected() && !hasmusic && soundmusicvol && soundmastervol) smartmusic(true);

    SDL_LockMutex(music_mutex);
    if(music) music->gain = soundmusicvol;
    SDL_UnlockMutex(music_mutex);
}
SVAR(0, titlemusic, "sounds/theme");

void mapsoundslot(int index, const char *name)
{
    while(index >= soundmap.length()) soundmap.add();
    soundmap[index] = gamesounds.getslot(name);
}

int getsoundslot(int index)
{
    if(!soundmap.inrange(index) || !soundmap[index]) return -1;
    return soundmap[index]->index;
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

        if(music) music->update();

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
        if(*sounddevice)
        {
            alGetError();
            snddev = alcOpenDevice(sounddevice);
            if(!snddev) conoutf("\frSound device initialisation failed: %s (with '%s', retrying default device)", sounderror(), sounddevice);
        }

        if(!snddev)
        {
            alGetError();
            snddev = alcOpenDevice(NULL); // fallback to default device
        }

        if(!snddev)
        {
            conoutf("\frSound device initialisation failed: %s", sounderror());
            return;
        }

        alGetError();
        sndctx = alcCreateContext(snddev, NULL);
        if(!sndctx)
        {
            conoutf("\frSound context initialisation failed: %s", sounderror());
            alcCloseDevice(snddev);
            snddev = NULL;
            return;
        }

        alcMakeContextCurrent(sndctx);
        conoutf("Sound: %s (%s) %s", alGetString(AL_RENDERER), alGetString(AL_VENDOR), alGetString(AL_VERSION));

        if(alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT") != AL_FALSE)
        {
            const ALCchar *d = alcGetString(NULL, ALC_DEFAULT_ALL_DEVICES_SPECIFIER),
                          *s = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
            if(s)
            {
                conoutf("Audio device list:");
                for(const ALchar *c = s; *c; c += strlen(c)+1)
                    conoutf("- %s%s", c, !strcmp(c, d) ? " [default]" : "");
            }
            conoutf("Audio device: %s", alcGetString(snddev, ALC_DEFAULT_ALL_DEVICES_SPECIFIER));
        }
        else conoutf("Audio device: %s", alcGetString(snddev, ALC_DEVICE_SPECIFIER));

        al_ext_efx = alcIsExtensionPresent(snddev, "ALC_EXT_EFX") ? true : false;
        al_ext_float32 = alIsExtensionPresent("AL_EXT_FLOAT32") ? true : false;
        al_soft_spatialize = !sounddownmix && alIsExtensionPresent("AL_SOFT_source_spatialize") ? true : false;

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
    enumerate(soundsamples, soundsample, s, s.cleanup());
    soundsamples.clear();
    gamesounds.clear(false);
    closemumble();
    alcMakeContextCurrent(NULL);
    alcDestroyContext(sndctx);
    alcCloseDevice(snddev);
    nosound = true;
}

void clearsound()
{
    loopv(soundsources) soundsources[i].clear();
    mapsounds.clear(false);
    soundenvs.clear();
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

const char *sounddirs[] = { "", "sounds/" }, *soundexts[] = { "", ".ogg", ".flac", ".wav" };

bool playmusic(const char *name, bool looping)
{
    if(nosound) return false;
    stopmusic();
    if(!name || !*name) return false;

    SDL_LockMutex(music_mutex);

    string buf;
    music = new musicstream;

    loopi(sizeof(sounddirs)/sizeof(sounddirs[0])) loopk(sizeof(soundexts)/sizeof(soundexts[0]))
    {
        formatstring(buf, "%s%s%s", sounddirs[i], name, soundexts[k]);
        soundfile *w = loadsoundfile(buf, soundfile::MUSIC);
        if(!w) continue;
        SOUNDCHECK(music->setup(name, w),
        {
            music->gain = soundmusicvol;
            music->looping = looping;
            SDL_UnlockMutex(music_mutex);
            musicloopinit();
            return true;
        }, conoutf("Error loading %s: %s", buf, alGetString(err)));
    }

    conoutf("\frCould not play music: %s", name);
    delete music;
    music = NULL;

    SDL_UnlockMutex(music_mutex);

    return false;
}

ICOMMAND(0, music, "sb", (char *s, int *n), playmusic(s, *n != 0));

bool playingmusic()
{
    bool result = false;

    SDL_LockMutex(music_mutex);

    if(music && music->playing()) result = true;

    SDL_UnlockMutex(music_mutex);

    return result;
}

void smartmusic(bool cond, bool init)
{
    SDL_LockMutex(music_mutex);

    bool isplayingmusic = music && music->playing();
    bool isplayingtitlemusic = music && !strcmp(music->name, titlemusic);

    SDL_UnlockMutex(music_mutex);

    if(init) canmusic = true;
    if(!canmusic || nosound || !soundmastervol || !soundmusicvol || (!cond && isplayingmusic) || !*titlemusic)
        return;

    if(!playingmusic() || (cond && !isplayingtitlemusic)) playmusic(titlemusic);
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
    loopi(sizeof(sounddirs)/sizeof(sounddirs[0])) loopk(sizeof(soundexts)/sizeof(soundexts[0]))
    {
        formatstring(buf, "%s%s%s", sounddirs[i], sample->name, soundexts[k]);
        soundfile *w = loadsoundfile(buf, al_soft_spatialize ? soundfile::SPATIAL : soundfile::MONO);
        if(!w) continue;
        SOUNDCHECK(sample->setup(w), {
            delete w;
            return sample;
        }, {
            delete w;
            conoutf("Error loading %s: %s", buf, alGetString(err));
        });
    }
    return NULL;
}

static void loadsamples(soundslot &slot, bool farvariant = false)
{
    soundsample *sample;
    string sam;

    loopi(slot.variants)
    {
        copystring(sam, slot.name);
        if(farvariant) concatstring(sam, "far");
        if(i) concatstring(sam, intstr(i + 1));

        sample = loadsoundsample(sam);
        slot.samples.add(sample);

        if(!sample || !sample->valid()) conoutf("\frFailed to load sample: %s", sam);
    }

    if(!farvariant && slot.fardist > 0) loadsamples(slot, true);
}

int addsound(const char *id, const char *name, float gain, float pitch, float rolloff, float refdist, float maxdist, int variants, float fardist, slotmanager<soundslot> &soundset)
{
    if(nosound || !strcmp(name, "<none>")) return -1;

    int newidx = soundset.add(id);
    soundslot &slot = soundset[newidx];

    slot.reset();
    slot.name = newstring(name);
    slot.gain = gain > 0 ? clamp(gain, 0.f, 100.f) : 1.f;
    slot.pitch = pitch >= 0 ? clamp(pitch, 1e-6f, 100.f) : 1.f;
    slot.rolloff = rolloff >= 0 ? rolloff : 1.f;
    slot.refdist = refdist >= 0 ? refdist : -1.f;
    slot.maxdist = maxdist >= 0 ? maxdist : -1.f;
    slot.fardist = fardist >= 0 ? fardist : -1.f;
    slot.variants = clamp(variants, 1, 32); // keep things sane please

    loadsamples(slot);

    return newidx;
}

ICOMMAND(0, loadsound, "ssgggggig", (char *id, char *name, float *gain, float *pitch, float *rolloff, float *refdist, float *maxdist, int *variants, float *fardist),
    intret(addsound(id, name, *gain, *pitch, *rolloff, *refdist, *maxdist, *variants, *fardist, gamesounds));
);

int addsoundcompat(char *id, char *name, int vol, int maxrad, int minrad, int variants, int fardist, slotmanager<soundslot> &soundset)
{
    float gain = vol > 0 ? vol / 255.f : 1.f, rolloff = maxrad > soundrolloff ? soundrolloff / float(maxrad) : 1.f, refdist = minrad > soundrefdist ? float(minrad) : -1.f;
    return addsound(id, name, gain, 1.f, rolloff, refdist, -1.f, variants, fardist, soundset);
}

ICOMMAND(0, registersound, "ssissii", (char *id, char *name, int *vol, char *maxrad, char *minrad, int *variants, int *fardist),
    intret(addsoundcompat(id, name, *vol, *maxrad ? parseint(maxrad) : -1, *minrad ? parseint(minrad) : -1, *variants, *fardist, gamesounds))
);

ICOMMAND(0, mapsound, "sgggggig", (char *name, float *gain, float *pitch, float *rolloff, float *refdist, float *maxdist, int *variants, float *fardist),
    if(maploading && hdr.version <= 51)
    {
        intret(addsoundcompat(NULL, name, *gain, *pitch, *rolloff, int(*refdist), *maxdist, mapsounds));
        return;
    }
    intret(addsound(NULL, name, *gain, *pitch, *rolloff, *refdist, *maxdist, *variants, *fardist, mapsounds));
);

ICOMMAND(0, mapsound, "sissi", (char *name, int *vol, char *maxrad, char *minrad, int *variants),
    intret(addsoundcompat(NULL, name, *vol, *maxrad ? parseint(maxrad) : -1, *minrad ? parseint(minrad) : -1, *variants, 0, mapsounds))
);

static inline bool sourcecmp(const soundsource *x, const soundsource *y) // sort with most likely to be culled first!
{
    bool xprio = x->flags&SND_PRIORITY, yprio = y->flags&SND_PRIORITY;
    if(!xprio && yprio) return true;
    if(xprio && !yprio) return false;

    bool xmap = x->flags&SND_MAP, ymap = y->flags&SND_MAP;
    if(!xmap && ymap) return true;
    if(xmap && !ymap) return false;

    bool xnoatt = x->flags&SND_NOATTEN || x->flags&SND_NODIST, ynoatt = y->flags&SND_NOATTEN || y->flags&SND_NODIST;
    if(!xnoatt && ynoatt) return true;
    if(xnoatt && !ynoatt) return false;

    if(!xnoatt && !ynoatt)
    {
        float xrefdist = x->refdist >= 0 ? x->refdist : (x->slot->refdist >= 0 ? x->slot->refdist : soundrefdist),
              xmaxdist = x->maxdist >= 0 ? x->maxdist : (x->slot->maxdist >= 0 ? x->slot->maxdist : FLT_MAX),
              xrolloff = x->rolloff >= 0 ? x->rolloff : x->slot->rolloff,
              xdist = xrefdist / (xrefdist + xrolloff * (clamp(x->vpos->dist(camera1->o), xrefdist, xmaxdist) - xrefdist)) * x->curgain,
              yrefdist = y->refdist >= 0 ? y->refdist : (y->slot->refdist >= 0 ? y->slot->refdist : soundrefdist),
              ymaxdist = y->maxdist >= 0 ? y->maxdist : (y->slot->maxdist >= 0 ? y->slot->maxdist : FLT_MAX),
              yrolloff = y->rolloff >= 0 ? y->rolloff : y->slot->rolloff,
              ydist = yrefdist / (yrefdist + yrolloff * (clamp(y->vpos->dist(camera1->o), yrefdist, ymaxdist) - yrefdist)) * x->curgain;
        if(xdist > ydist) return true;
        if(xdist < ydist) return false;
    }
    else
    {
        if(x->curgain < y->curgain) return true;
        if(x->curgain > y->curgain) return false;
    }

    return true;
}

int soundindex(soundslot *slot, int slotnum, const vec &pos, int flags, float gain, float pitch, float rolloff, float refdist, float maxdist)
{
    static vector<soundsource *> sources;
    sources.setsize(0);

    static soundsource indexsource;
    indexsource.clear();
    indexsource.index = -1;
    indexsource.slot = slot;
    indexsource.slotnum = slotnum;
    indexsource.pos = pos;
    indexsource.flags = flags;
    indexsource.gain = indexsource.curgain = gain;
    indexsource.pitch = pitch;
    indexsource.rolloff = rolloff;
    indexsource.refdist = refdist;
    indexsource.maxdist = maxdist;
    sources.add(&indexsource);

    loopv(soundsources)
    {
        if(soundsources[i].valid()) sources.add(&soundsources[i]);
        else return i;
    }
    sources.sort(sourcecmp);

    if(sources[0]->index >= 0 && sources.length() > soundmaxsources)
    {
        int index = sources[0]->index;
        sources[0]->clear();
        return index;
    }
    if(sources.length() < soundmaxsources)
    {
        int index = soundsources.length();
        soundsources.add();
        return index;
    }
    return -1;
}

void updatesounds()
{
    updatemumble();

    if(nosound) return;
    alcSuspendContext(sndctx);

    sortenvzones();
    panenvzones();

    loopv(soundsources) if(soundsources[i].valid())
    {
        soundsource &s = soundsources[i];

        if((!s.ends || lastmillis < s.ends) && s.playing())
        {
            s.update();
            continue;
        }
        s.cleanup();

        slotmanager<soundslot> &soundset = s.flags&SND_MAP ? mapsounds : gamesounds;
        while(!s.buffer.empty())
        {
            int n = s.buffer[0];
            s.buffer.remove(0);

            if((!soundset.inrange(n) || soundset[n].samples.empty() || !soundset[n].gain)) continue;

            soundslot *slot = &soundset[n];
            int variant = slot->variants > 1 ? rnd(slot->variants) : 0;
            // if(slot->fardist > 0 && s.vpos->dist(camera1->o) >= slot->fardist) variant += slot->variants;
            soundsample *sample = slot->samples.inrange(variant) ? slot->samples[variant] : NULL;
            if(!sample || !sample->valid()) continue;

            s.slot = slot;
            s.slotnum = n;
            SOUNDCHECK(s.push(sample), break, conoutf("Error playing sample %s [%d]: %s", sample->name, s.index, alGetString(err)));
        }
        //conoutf("Clearing sound source %d [%d] %d [%d] %s", s.source, s.index, s.ends, lastmillis, soundsources[i].playing() ? "playing" : "not playing");
        if(!s.playing()) s.clear();
    }

    SDL_LockMutex(music_mutex);
    bool hasmusic = music != NULL;
    bool musicvalid = music && music->valid();
    SDL_UnlockMutex(music_mutex);

    if(!musicvalid || (hasmusic && (nosound || !soundmastervol || !soundmusicvol || !playingmusic())))
        stopmusic();

    vec o[2];
    o[0].x = (float)(cosf(RAD*(camera1->yaw-90)));
    o[0].y = (float)(sinf(RAD*(camera1->yaw-90)));
    o[0].z = o[1].x = o[1].y = 0.0f;
    o[1].z = 1.0f;
    alListenerf(AL_GAIN, soundmastervol);
    alListenerfv(AL_ORIENTATION, (ALfloat *) &o);
    alListenerfv(AL_POSITION, (ALfloat *) &camera1->o);
    alListenerfv(AL_VELOCITY, (ALfloat *) &game::focusedent()->vel);
    if(al_ext_efx) alListenerf(AL_METERS_PER_UNIT, 0.125f); // 8 units = 1 meter

    alcProcessContext(sndctx);
}

int emitsound(int n, vec *pos, physent *d, int *hook, int flags, float gain, float pitch, float rolloff, float refdist, float maxdist, int ends)
{
    if(nosound || !pos || gain <= 0 || !soundmastervol || !soundeffectvol || (flags&SND_MAP ? !soundeffectenv : !soundeffectevent)) return -1;
    if((flags&SND_MAP || (!(flags&SND_UNMAPPED) && n >= S_GAMESPECIFIC)) && client::waiting(true)) return -1;

    slotmanager<soundslot> &soundset = flags&SND_MAP ? mapsounds : gamesounds;
    if(!(flags&SND_UNMAPPED) && !(flags&SND_MAP)) n = getsoundslot(n);

    if(soundset.inrange(n))
    {
        soundslot *slot = &soundset[n];
        if(slot->samples.empty() || !slot->gain) return -1;
        if(hook && issound(*hook) && flags&SND_BUFFER)
        {
            soundsources[*hook].buffer.add(n);
            return *hook;
        }

        int variant = slot->variants > 1 ? rnd(slot->variants) : 0;
        if(slot->fardist > 0 && pos->dist(camera1->o) >= slot->fardist) variant += slot->variants;
        soundsample *sample = slot->samples.inrange(variant) ? slot->samples[variant] : NULL;
        if(!sample || !sample->valid()) return -1;

        float sgain = gain > 0 ? clamp(gain, 0.f, 100.f) : 1.f, spitch = pitch >= 0 ? clamp(pitch, 1e-6f, 100.f) : 1.f,
              srolloff = rolloff >= 0 ? rolloff : -1.f, srefdist = refdist >= 0 ? refdist : -1.f, smaxdist = maxdist >= 0 ? maxdist : -1.f;

        int index = soundindex(slot, n, *pos, flags, sgain, spitch, srolloff, srefdist, smaxdist);
        if(index < 0) return -1;

        soundsource &s = soundsources[index];
        if(s.hook && s.hook != hook) *s.hook = -1;

        s.slot = slot;
        s.slotnum = n;
        s.millis = lastmillis;
        s.index = index;

        s.flags = flags;
        if(d || s.flags&SND_TRACKED)
        {
            s.vpos = pos;
            if(d) s.vel = d->vel;
            else s.vel = vec(0, 0, 0);
            s.flags |= SND_TRACKED;
        }
        else
        {
            s.vpos = &s.pos;
            *s.vpos = *pos;
        }

        s.gain = sgain;
        s.pitch = spitch;
        s.rolloff = srolloff;
        s.refdist = srefdist;
        s.maxdist = smaxdist;
        s.owner = d;
        s.ends = ends;

        if(hook && *hook != s.index)
        {
            if(issound(*hook)) soundsources[*hook].clear();
            *hook = s.index;
        }
        s.hook = hook;

        SOUNDCHECK(s.push(sample), return index, conoutf("Error playing sample %s [%d]: %s", sample->name, index, alGetString(err)));
    }
    else if(n > 0) conoutf("\frUnregistered sound: %d", n);
    return -1;
}

int emitsoundpos(int n, const vec &pos, int *hook, int flags, float gain, float pitch, float rolloff, float refdist, float maxdist, int ends)
{
    vec curpos = pos;
    flags &= ~SND_TRACKED; // can't do that here
    return emitsound(n, &curpos, NULL, hook, flags, gain, pitch, rolloff, refdist, maxdist, ends);
}

int playsound(int n, const vec &pos, physent *d, int flags, int vol, int maxrad, int minrad, int *hook, int ends)
{
    vec o = d ? d->o : pos;
    float gain = vol > 0 ? vol / 255.f : 1.f, rolloff = maxrad > soundrolloff ? soundrolloff / float(maxrad) : 1.f, refdist = minrad > soundrefdist ? float(minrad) : -1.f;
    return emitsound(n, &o, d, hook, flags, gain, 1.f, rolloff, refdist, -1.f, ends);
}

ICOMMAND(0, sound, "iib", (int *n, int *vol, int *flags),
{
    intret(playsound(*n, camera1->o, camera1, (*flags >= 0 ? *flags : SND_PRIORITY|SND_NOENV|SND_NOATTEN)|SND_UNMAPPED, *vol ? *vol : -1));
});

ICOMMAND(0, soundbyname, "sib", (char *i, int *vol, int *flags),
{
    intret(playsound(gamesounds.getindex(i), camera1->o, camera1, (*flags >= 0 ? *flags : SND_PRIORITY|SND_NOENV|SND_NOATTEN)|SND_UNMAPPED, *vol ? *vol : -1));
});

ICOMMAND(0, soundslot, "s", (char *i), intret(gamesounds.getindex(i)));

void removemapsounds()
{
    loopv(soundsources) if(soundsources[i].index >= 0 && soundsources[i].flags&SND_MAP) soundsources[i].clear();
}

void removetrackedsounds(physent *d)
{
    loopv(soundsources) if(soundsources[i].owner == d) soundsources[i].clear();
}

void resetsound()
{
    clearchanges(CHANGE_SOUND);
    delsoundfxslots();
    loopv(soundsources) soundsources[i].clear();
    enumerate(soundsamples, soundsample, s, s.cleanup());
    stopmusic();
    nosound = true;
    initsound();
    if(nosound)
    {
        gamesounds.clear(false);
        mapsounds.clear(false);
        soundenvs.clear();
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
        }
        default: return false;
    }
    mixtype = clamp(m, 0, int(MAXMIX-1));

    sndfile = sf_open(findfile(name, "rb"), SFM_READ, &info);
    if(!sndfile || info.frames <= 0) return false;

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
        conoutf("\frUnsupported channel count in %s: %d", name, info.channels);
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
    reset();
}

ALenum soundsample::setup(soundfile *s)
{
    SOUNDERROR();
    alGenBuffers(1, &buffer);
    SOUNDERROR(cleanup(); return err);
    switch(s->type)
    {
        case soundfile::SHORT: alBufferData(buffer, s->format, s->data_s, s->len, s->info.samplerate); break;
        case soundfile::FLOAT: alBufferData(buffer, s->format, s->data_f, s->len, s->info.samplerate); break;
        default: return AL_INVALID_OPERATION;
    }
    SOUNDERROR(cleanup(); return err);
    return AL_NO_ERROR;
}

void soundsample::reset()
{
    buffer = 0;
}

void soundsample::cleanup()
{
    if(valid()) alDeleteBuffers(1, &buffer);
    buffer = 0;
}

bool soundsample::valid()
{
    if(!buffer || !alIsBuffer(buffer)) return false;
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
    alGenSources(1, &source);
    SOUNDERROR(clear(); return err);

    if(al_ext_efx && !(flags&SND_NOFILTER) && sounddistfilter > 0.0f)
    {
        alGenFilters(1, &filter);
        alFilteri(filter, AL_FILTER_TYPE, AL_FILTER_BANDPASS);
        alSourcei(source, AL_DIRECT_FILTER, filter);

        SOUNDERROR(clear(); return err);
    }

    alSourcei(source, AL_BUFFER, s->buffer);
    SOUNDERROR(clear(); return err);

    if(flags&SND_NOPAN && flags&SND_NODIST) flags |= SND_NOATTEN; // superfluous
    if(flags&SND_NOATTEN)
    {
        flags &= ~(SND_NOPAN|SND_NODIST); // unnecessary
        alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
        SOUNDERROR(clear(); return err);
    }
    else
    {
        if(al_soft_spatialize)
        {
            alSourcei(source, AL_SOURCE_SPATIALIZE_SOFT, AL_TRUE);
            SOUNDERROR(clear(); return err);
        }
        alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
        SOUNDERROR(clear(); return err);
    }

    alSourcei(source, AL_LOOPING, flags&SND_LOOP ? AL_TRUE : AL_FALSE);
    SOUNDERROR(clear(); return err);

    finalrolloff = rolloff >= 0 ? rolloff : (slot->rolloff >= 0 ? slot->rolloff : 1.f);
    alSourcef(source, AL_ROLLOFF_FACTOR, finalrolloff);
    SOUNDERROR(clear(); return err);

    alSourcef(source, AL_REFERENCE_DISTANCE, refdist >= 0 ? refdist : (slot->refdist >= 0 ? slot->refdist : soundrefdist));
    SOUNDERROR(clear(); return err);

    float dist = maxdist >= 0 ? maxdist : slot->maxdist;
    if(dist >= 0)
    {
        alSourcef(source, AL_MAX_DISTANCE, dist);
        SOUNDERROR(clear(); return err);
    }

    return AL_NO_ERROR;
}

void soundsource::cleanup()
{
    if(!valid()) return;
    if(al_ext_efx && alIsFilter(filter)) alDeleteFilters(1, &filter);
    alSourceStop(source);
    alDeleteSources(1, &source);
}

void soundsource::reset()
{
    source = 0;
    pos = curpos = vel = vec(0, 0, 0);
    vpos = &pos;
    slot = NULL;
    owner = NULL;
    gain = curgain = pitch = curpitch = 1;
    material = MAT_AIR;
    flags = millis = ends = 0;
    rolloff = refdist = maxdist = -1;
    index = slotnum = lastupdate = -1;
    if(hook) *hook = -1;
    hook = NULL;
    buffer.shrink(0);
}

void soundsource::clear()
{
    cleanup();
    reset();
}

void soundsource::unhook()
{
    if(hook) *hook = -1;
    hook = NULL;
    if(owner || flags&SND_TRACKED)
    {
        pos = *vpos;
        vpos = &pos;
        vel = vec(0, 0, 0);
        owner = NULL;
        flags &= ~(SND_TRACKED|SND_LOOP);
    }
}

ALenum soundsource::updatefilter()
{
    if(!al_ext_efx || !alIsFilter(filter)) return AL_NO_ERROR;

    float dist = camera1->o.dist(*vpos);
    float gain = 1.0f - (((dist * log10f(finalrolloff + 1.0f)) / finalrefdist) * sounddistfilter);
    gain = clamp(gain, 1.0f - sounddistfilter, 1.0f);

    alFilteri(filter, AL_FILTER_TYPE, AL_FILTER_BANDPASS);
    alFilterf(filter, AL_BANDPASS_GAINHF, gain);
    alFilterf(filter, AL_BANDPASS_GAINLF, gain);
    alSourcei(source, AL_DIRECT_FILTER, filter);

    return alGetError();
}

ALenum soundsource::update()
{
    material = lookupmaterial(*vpos);
    curgain = clamp(soundeffectvol*slot->gain*(flags&SND_MAP ? soundeffectenv : soundeffectevent), 0.f, 100.f);

    if(flags&SND_CLAMPED)
    {
        SOUNDERROR();
        alSourcef(source, AL_MIN_GAIN, gain);
        SOUNDERROR(clear(); return err);
    }
    else curgain *= gain;

    SOUNDERROR();
    alSourcef(source, AL_GAIN, curgain);
    SOUNDERROR(clear(); return err);

    curpitch = clamp(pitch * slot->pitch, 1e-6f, 100.f);
    SOUNDERROR();
    alSourcef(source, AL_PITCH, pitch);
    SOUNDERROR(clear(); return err);

    vec rpos = *vpos;
    if(flags&SND_NOATTEN) rpos = vel = vec(0, 0, 0);
    else if(flags&SND_NOPAN)
    {
        float mag = vec(rpos).sub(camera1->o).magnitude();
        rpos = vec(camera1->yaw*RAD, camera1->pitch*RAD).mul(mag).add(camera1->o);
    }
    else if(flags&SND_NODIST) rpos.sub(camera1->o).safenormalize().mul(2).add(camera1->o);

    SOUNDERROR();
    alSourcefv(source, AL_POSITION, (ALfloat *) &rpos);
    SOUNDERROR(clear(); return err);

    if(owner) vel = owner->vel;
    if(totalmillis != lastupdate && (lastupdate < 0 || totalmillis-lastupdate > physics::physframetime))
    {
        if(!owner && (flags&SND_TRACKED || flags&SND_VELEST) && lastupdate >= 0)
        {
            int n = physics::physframetime;
            while(lastupdate+n < totalmillis) n += physics::physframetime;
            vel = vec(*vpos).sub(curpos).mul(1000).div(n);
        }
        lastupdate = totalmillis;
        curpos = *vpos;
    }

    alSourcefv(source, AL_VELOCITY, (ALfloat *) &vel);
    SOUNDERROR(clear(); return err);

    updatefilter();

    if(!(flags&SND_NOENV))
    {
        soundenvzone *zone = getactiveenvzone(*vpos);

        ALuint zoneslot = zone && zone->isvalid() ?
            zone->getefxslot() : AL_EFFECTSLOT_NULL;

        ALuint insideslot = insideenvzone && insideenvzone->isvalid() && zone != insideenvzone ?
            insideenvzone->getefxslot() : AL_EFFECTSLOT_NULL;

        alSourcei(source, AL_AUXILIARY_SEND_FILTER_GAIN_AUTO, AL_TRUE);
        alSource3i(source, AL_AUXILIARY_SEND_FILTER, zoneslot, 0, AL_FILTER_NULL);
        alSource3i(source, AL_AUXILIARY_SEND_FILTER, insideslot, 1, AL_FILTER_NULL);

        SOUNDERROR(clear(); return err);
    }

    return AL_NO_ERROR;
}

bool soundsource::valid()
{
    if(!source || !alIsSource(source)) return false;
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
    SOUNDERROR(clear(); return err);
    return AL_NO_ERROR;
}

ALenum soundsource::push(soundsample *s)
{
    SOUNDCHECK(setup(s), , return err);
    SOUNDCHECK(update(), , return err);
    SOUNDCHECK(play(), , return err);
    return AL_NO_ERROR;
}

#define MUSICTAG(x,y) \
    do { \
        const char *tag##x = sf_get_string(s->sndfile, y); \
        if(tag##x != NULL) x = newstring(tag##x); \
    } while(false);

ALenum musicstream::setup(const char *n, soundfile *s)
{
    if(!s) return AL_NO_ERROR;

    name = newstring(n);
    MUSICTAG(artist, SF_STR_ARTIST);
    MUSICTAG(title, SF_STR_TITLE);
    MUSICTAG(album, SF_STR_ALBUM);

    SOUNDERROR();
    alGenBuffers(MUSICBUFS, buffer);
    SOUNDERROR(clear(); return err);

    alGenSources(1, &source);
    SOUNDERROR(clear(); return err);

    alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
    SOUNDERROR(clear(); return err);

    alSourcef(source, AL_ROLLOFF_FACTOR, 0.f);
    SOUNDERROR(clear(); return err);

    alSource3f(source, AL_POSITION, 0.f, 0.f, 0.f);
    SOUNDERROR(clear(); return err);

    data = s;

    int r = 0;
    for(; r < MUSICBUFS; r++)
    {
        if(!data->fillmus()) break;
        SOUNDCHECK(fill(buffer[r]), , return err);
    }

    alSourceQueueBuffers(source, r, buffer);
    SOUNDERROR(clear(); return err);

    return AL_NO_ERROR;
}

ALenum musicstream::fill(ALint bufid)
{
    switch(data->type)
    {
        case soundfile::SHORT: alBufferData(bufid, data->format, data->data_s, (ALsizei)data->len, data->info.samplerate); break;
        case soundfile::FLOAT: alBufferData(bufid, data->format, data->data_f, (ALsizei)data->len, data->info.samplerate); break;
        default: return AL_INVALID_OPERATION;
    }
    SOUNDERROR(clear(); return err);
    return AL_NO_ERROR;
}

void musicstream::cleanup()
{
    if(!valid()) return;
    alSourceStop(source);
    alDeleteSources(1, &source);
    alDeleteBuffers(MUSICBUFS, buffer);
    if(name) delete[] name;
    if(artist) delete[] artist;
    if(title) delete[] title;
    if(album) delete[] album;
    if(data) delete data;
}

void musicstream::reset()
{
    name = artist = title = album = NULL;
    source = 0;
    loopi(MUSICBUFS) buffer[i] = 0;
    data = NULL;
    gain = 1;
    looping = true;
}

void musicstream::clear()
{
    cleanup();
    reset();
}

ALenum musicstream::update()
{
    if(!valid()) return AL_INVALID_OPERATION;

    SOUNDERROR();
    ALint processed;
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
    SOUNDERROR(clear(); return err);

    while(processed > 0)
    {
        ALuint bufid;
        alSourceUnqueueBuffers(source, 1, &bufid);
        SOUNDERROR(clear(); return err);
        processed--;

        if(!data->fillmus(!looping)) continue;
        SOUNDCHECK(fill(bufid), , return err);

        alSourceQueueBuffers(source, 1, &bufid);
        SOUNDERROR(clear(); return err);
    }

    SOUNDERROR();
    alSourcef(source, AL_GAIN, gain);
    SOUNDERROR(clear(); return err);

    if(!playing())
    {
        ALint queued;

        alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);
        SOUNDERROR(clear(); return err);
        if(queued)
        {
            alSourcePlay(source);
            SOUNDERROR(clear(); return err);
        }
        else clear();
    }

    return AL_NO_ERROR;
}

bool musicstream::valid()
{
    if(!source || !alIsSource(source)) return false;
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
    SOUNDERROR(clear(); return err);
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
    slotmanager<soundslot> &soundset = mapsnd ? mapsounds : gamesounds;
    if(idx < 0) intret(soundset.length());
    else if(soundset.inrange(idx))
    {
        if(prop < 0) intret(8);
        else switch(prop)
        {
            case 0: result(soundset[idx].name); break;
            case 1: floatret(soundset[idx].gain); break;
            case 2: floatret(soundset[idx].pitch); break;
            case 3: floatret(soundset[idx].rolloff); break;
            case 4: floatret(soundset[idx].refdist); break;
            case 5: floatret(soundset[idx].maxdist); break;
            case 6: floatret(soundset[idx].fardist); break;
            case 7: intret(soundset[idx].variants); break;
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
        if(prop < 0) intret(20);
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
            case 13: defformatstring(pos, "%.f %.f %.f", s.vpos->x, s.vpos->y, s.vpos->z); result(pos); break;
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
            case 18: intret(client::getcn(s.owner)); break;
            case 19: intret(s.owner == camera1 ? 1 : 0); break;
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

static void defsoundenv(const char *name, uint *code)
{
    int newsoundenvidx = -1;

    if((newsoundenvidx = soundenvs.getindex(name)) < 0) newsoundenvidx = soundenvs.add(name);
    newsoundenv = &soundenvs[newsoundenvidx];
    newsoundenv->name = soundenvs.getname(newsoundenvidx);

    initsoundenv();
    execute(code);

    newsoundenv = NULL;
}
ICOMMAND(0, defsoundenv, "se", (char *name, uint *code), defsoundenv(name, code));

template<class T>
static void setsoundenvprop(const char *name, const T &val)
{
    if(!newsoundenv)
    {
        conoutf("\frError: cannot assign property %s, not loading soundenvs", name);
        return;
    }

    property *p = findprop(name, newsoundenv->props, SOUNDENV_PROPS);

    if(!p)
    {
        conoutf("\frError: %s, soundenv property %s not found", newsoundenv->getname(), name);
        return;
    }

    p->set(val);
    newsoundenv->updatezoneparams();
}

ICOMMAND(0, soundenvpropi, "si", (char *name, int *ival),
    setsoundenvprop(name, *ival));
ICOMMAND(0, soundenvpropf, "sf", (char *name, float *fval),
    setsoundenvprop(name, *fval));

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

void dumpsoundenvs(stream *s) { loopv(soundenvs) dumpsoundenv(soundenvs[i], s); }

ICOMMAND(0, soundenvinfo, "i", (int *index), {
    if(*index < 0) intret(soundenvs.length());
    else if(*index < soundenvs.length()) result(soundenvs[*index].name);
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
    conoutft(CON_DEBUG, "Mumble positional audio is not available on this platform.");
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
