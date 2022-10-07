#include "engine.h"

ALCdevice *snddev = NULL;
ALCcontext *sndctx = NULL;

bool nosound = true, canmusic = false;
bool al_ext_efx = false, al_soft_spatialize = false, al_ext_float32 = false;

hashnameset<soundsample> soundsamples;
slotmanager<soundslot> gamesounds, mapsounds;
vector<slot *> soundmap;
vector<sound> sounds;
SDL_Thread *mstream_thread;
SDL_mutex *mstream_mutex;
music *mstream = NULL;

slotmanager<soundenv> soundenvs, mapsoundenvs;
int newsoundenvidx = -1;
soundenv *newsoundenv = NULL;

SVARF(IDF_INIT, sounddevice, "", initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(IDF_INIT, soundsources, 16, 256, 1024, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(IDF_INIT, sounddownmix, 0, 0, 1, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));

#define SOUNDVOL(oldname, newname, def, body) \
    FVARF(IDF_PERSIST, sound##newname##vol, 0, def, 1, body); \
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
FVAR(IDF_PERSIST, soundeffectevent, 0, 1, 2);
FVAR(IDF_PERSIST, soundeffectenv, 0, 1, 2);

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

FVAR(IDF_PERSIST, soundrefdist, FVAR_NONZERO, 8, FVAR_MAX);
FVAR(IDF_PERSIST, soundrolloff, FVAR_NONZERO, 128, FVAR_MAX);

void updatemusic()
{
    SDL_LockMutex(mstream_mutex);
    bool hasmstream = mstream != NULL;
    SDL_UnlockMutex(mstream_mutex);

    if(!connected() && !hasmstream && soundmusicvol && soundmastervol) smartmusic(true);

    SDL_LockMutex(mstream_mutex);
    if(mstream) mstream->gain = soundmusicvol;
    SDL_UnlockMutex(mstream_mutex);
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

int mstreamloop(void *data)
{
    SDL_LockMutex(mstream_mutex);
    if(!mstream_thread || SDL_GetThreadID(mstream_thread) != SDL_ThreadID())
    {
        SDL_UnlockMutex(mstream_mutex);
        return 0;

    }
    SDL_UnlockMutex(mstream_mutex);

    while(true)
    {
        SDL_LockMutex(mstream_mutex);

        if(!mstream_thread || SDL_GetThreadID(mstream_thread) != SDL_ThreadID() || !mstream)
        {
            SDL_UnlockMutex(mstream_mutex);
            break;
        }

        if(mstream) mstream->update();

        SDL_UnlockMutex(mstream_mutex);
        SDL_Delay(10);
    }

    return 0;
}

void mstreamloopinit()
{
    SDL_LockMutex(mstream_mutex);
    mstream_thread = SDL_CreateThread(mstreamloop, "mstream", NULL);
    SDL_UnlockMutex(mstream_mutex);
}

void mstreamloopstop()
{
    SDL_LockMutex(mstream_mutex);

    if(mstream_thread)
    {
        SDL_DetachThread(mstream_thread);
        mstream_thread = NULL;
    }

    SDL_UnlockMutex(mstream_mutex);
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

        al_ext_efx = alIsExtensionPresent("AL_EXT_EFX") ? true : false;
        al_ext_float32 = alIsExtensionPresent("AL_EXT_FLOAT32") ? true : false;
        al_soft_spatialize = !sounddownmix && alIsExtensionPresent("AL_SOFT_source_spatialize") ? true : false;

        nosound = false;

        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
        soundsetdoppler(sounddoppler);
        soundsetspeed(soundspeed);

        mstream_mutex = SDL_CreateMutex();
    }
    initmumble();
}

void stopmusic()
{
    SDL_LockMutex(mstream_mutex);

    if(!mstream)
    {
        SDL_UnlockMutex(mstream_mutex);
        return;
    }

    delete mstream;
    mstream = NULL;
    SDL_UnlockMutex(mstream_mutex);

    mstreamloopstop();
}

void stopsound()
{
    if(nosound) return;
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
    loopv(sounds) sounds[i].clear();
    mapsounds.clear(false);
    mapsoundenvs.clear();
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

    SDL_LockMutex(mstream_mutex);

    string buf;
    mstream = new music;

    loopi(sizeof(sounddirs)/sizeof(sounddirs[0])) loopk(sizeof(soundexts)/sizeof(soundexts[0]))
    {
        formatstring(buf, "%s%s%s", sounddirs[i], name, soundexts[k]);
        soundfile *w = loadsoundfile(buf, soundfile::MUSIC);
        if(!w) continue;
        SOUNDCHECK(mstream->setup(name, w),
        {
            mstream->gain = soundmusicvol;
            mstream->looping = looping;
            SDL_UnlockMutex(mstream_mutex);
            mstreamloopinit();
            return true;
        }, conoutf("Error loading %s: %s", buf, alGetString(err)));
    }

    conoutf("\frCould not play music: %s", name);
    delete mstream;
    mstream = NULL;

    SDL_UnlockMutex(mstream_mutex);

    return false;
}

ICOMMAND(0, music, "sb", (char *s, int *n), playmusic(s, *n != 0));

bool playingmusic()
{
    bool result = false;

    SDL_LockMutex(mstream_mutex);

    if(mstream && mstream->playing()) result = true;

    SDL_UnlockMutex(mstream_mutex);

    return result;
}

void smartmusic(bool cond, bool init)
{
    SDL_LockMutex(mstream_mutex);

    bool isplayingmusic = mstream && mstream->playing();
    bool isplayingtitlemusic = mstream && !strcmp(mstream->name, titlemusic);

    SDL_UnlockMutex(mstream_mutex);

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

        if(!sample || !sample->valid())
        {
            conoutf("\frFailed to load sample: %s", sam);
            if(i) break; // stop if missing a variant
        }
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
    slot.gain = clamp(gain, 0.f, 1.f);
    slot.pitch = pitch > 0 ? clamp(pitch, 1e-6f, 100.f) : 1.f;
    slot.rolloff = rolloff > 0 ? rolloff : 1.f;
    slot.refdist = refdist >= 0 ? refdist : -1.f;
    slot.maxdist = maxdist > 0 ? maxdist : 0.f;
    slot.fardist = fardist >= 0 ? fardist : -1.f;
    slot.variants = clamp(variants, 1, 32); // keep things sane please
    slot.fardist = fardist;

    loadsamples(slot);

    return newidx;
}

ICOMMAND(0, loadsound, "ssggggfif", (const char *id, const char *name, float *gain, float *pitch, float *rolloff, float *refdist, float *maxdist, int *variants, float *fardist),
    intret(addsound(id, name, *gain, *pitch, *rolloff, *refdist, *maxdist, *variants, *fardist, gamesounds));
);

#define addsoundcompat(i, n, v, x, r, t, f, s) \
    addsound(i, n, v > 0 ? v / 255.f : 1.f, 1.f, x > 0 ? soundrolloff / x : 1.f, r > 0 ? float(r) : -1.f, 0.f, t, f, s)

ICOMMAND(0, registersound, "ssissii", (char *id, char *name, int *vol, char *maxrad, char *minrad, int *variants, float *fardist),
    intret(addsoundcompat(id, name, *vol, *maxrad ? parseint(maxrad) : -1, *minrad ? parseint(minrad) : -1, *variants, *fardist, gamesounds))
);

ICOMMAND(0, mapsound, "sggggfif", (const char *name, float *gain, float *pitch, float *rolloff, float *refdist, float *maxdist, int *variants, float *fardist),
    if(maploading && hdr.version < 52)
    {
        intret(addsoundcompat(NULL, name, *gain, *pitch, *rolloff, int(*refdist), -1.f, mapsounds));
        return;
    }
    intret(addsound(NULL, name, *gain, *pitch, *rolloff, *refdist, *maxdist, *variants, *fardist, gamesounds));
);

ICOMMAND(0, mapsound, "sissi", (char *name, int *vol, char *maxrad, char *minrad, int *variants),
    intret(addsoundcompat(NULL, name, *vol, *maxrad ? parseint(maxrad) : -1, *minrad ? parseint(minrad) : -1, *variants, 0, mapsounds))
);

static inline bool sourcecmp(const sound *x, const sound *y) // sort with most likely to be culled first!
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
        float xdist = x->pos.dist(camera1->o), ydist = y->pos.dist(camera1->o);
        if(xdist > ydist) return true;
        if(xdist < ydist) return false;
    }

    return rnd(2);
}

int soundindex()
{
    vector<sound *> sources;
    loopv(sounds)
    {
        if(sounds[i].valid()) sources.add(&sounds[i]);
        else return i;
    }
    sources.sort(sourcecmp);

    while(sources.length() >= soundsources)
    {
        sources[0]->clear();
        sources.remove(0);
    }

    return sources.length();
}

void updatesounds()
{
    updatemumble();

    if(nosound) return;
    alcSuspendContext(sndctx);

    loopv(sounds) if(sounds[i].valid())
    {
        sound &s = sounds[i];

        if((!s.ends || lastmillis < s.ends) && sounds[i].playing())
        {
            s.update();
            continue;
        }

        slotmanager<soundslot> &soundset = s.flags&SND_MAP ? mapsounds : gamesounds;
        while(!s.buffer.empty() && (!soundset.inrange(s.buffer[0]) || soundset[s.buffer[0]].samples.empty() || !soundset[s.buffer[0]].gain)) s.buffer.remove(0);
        if(!s.buffer.empty())
        {
            int n = s.buffer[0];
            s.cleanup();
            s.buffer.remove(0);
            soundslot *slot = &soundset[n];
            soundsample *sample = slot->samples[rnd(slot->samples.length())];
            s.push(sample);
            continue;
        }
        //conoutf("Clearing sound source %d [%d] %d [%d] %s", s.source, s.index, s.ends, lastmillis, sounds[i].playing() ? "playing" : "not playing");
        s.clear();
    }

    SDL_LockMutex(mstream_mutex);
    bool hasmstream = mstream != NULL;
    bool mstreamvalid = mstream && mstream->valid();
    SDL_UnlockMutex(mstream_mutex);

    if(!mstreamvalid || (hasmstream && (nosound || !soundmastervol || !soundmusicvol || !playingmusic())))
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

int emitsound(int n, const vec &pos, int flags, float gain, float pitch, float rolloff, float refdist, float maxdist, physent *d, int *hook, int ends)
{
    if(nosound || !gain || !soundmastervol || !soundeffectvol || (flags&SND_MAP ? !soundeffectenv : !soundeffectevent)) return -1;
    if(((flags&SND_MAP || (!(flags&SND_UNMAPPED) && n >= S_GAMESPECIFIC)) && client::waiting(true)) || (!d && !insideworld(pos))) return -1;

    slotmanager<soundslot> &soundset = flags&SND_MAP ? mapsounds : gamesounds;
    if(!(flags&SND_UNMAPPED) && !(flags&SND_MAP)) n = getsoundslot(n);

    if(soundset.inrange(n))
    {
        soundslot *slot = &soundset[n];
        if(slot->samples.empty() || !slot->gain) return -1;
        if(hook && issound(*hook) && flags&SND_BUFFER)
        {
            sounds[*hook].buffer.add(n);
            return *hook;
        }

        vec o = d ? d->o : pos, v = d ? d->vel : vec(0, 0, 0);
        int variant = slot->variants > 1 ? rnd(slot->variants) : 0;
        if(slot->fardist && o.dist(camera1->o) >= slot->fardist) variant += slot->variants;
        soundsample *sample = slot->samples.inrange(variant) ? slot->samples[variant] : NULL;
        if(!sample || !sample->valid()) return -1;

        int index = soundindex();
        while(index >= sounds.length()) sounds.add();
        sound &s = sounds[index];
        if(s.hook && s.hook != hook) *s.hook = -1;

        s.slot = slot;
        s.millis = lastmillis;
        s.index = index;

        s.slotnum = n;
        s.flags = flags;
        s.pos = o;
        s.gain = clamp(gain, 0.f, 1.f);
        s.pitch = pitch > 0 ? clamp(pitch, 1e-6f, 100.f) : 1.f;
        s.rolloff = rolloff >= 0 ? rolloff : slot->rolloff;
        s.refdist = refdist >= 0 ? refdist : slot->refdist;
        s.maxdist = maxdist > 0 ? maxdist : slot->maxdist;
        s.owner = d;
        s.ends = ends;
        s.vel = v;

        if(hook)
        {
            if(issound(*hook)) sounds[*hook].clear();
            *hook = s.index;
            s.hook = hook;
        }
        else s.hook = NULL;
        SOUNDCHECK(s.push(sample), return index, {
            conoutf("Error playing sample %s [%d]: %s", sample->name, index, alGetString(err));
            s.clear();
        });
    }
    else if(n > 0) conoutf("\frUnregistered sound: %d", n);
    return -1;
}

int playsound(int n, const vec &pos, physent *d, int flags, int vol, int maxrad, int minrad, int *hook, int ends)
{
    return emitsound(n, pos, flags, vol > 0 ? vol / 255.f : 1.f, 1.f, maxrad > 0 ? soundrolloff / maxrad : 1.f, minrad > 0 ? float(minrad) : -1.f, -1.f, d, hook, ends);
}

ICOMMAND(0, sound, "iib", (int *n, int *vol, int *flags),
{
    intret(playsound(*n, camera1->o, camera1, (*flags >= 0 ? *flags : SND_FORCED) | SND_UNMAPPED, *vol ? *vol : -1));
});

ICOMMAND(0, soundbyname, "sib", (char *i, int *vol, int *flags),
{
    intret(playsound(gamesounds.getindex(i), camera1->o, camera1, (*flags >= 0 ? *flags : SND_FORCED) | SND_UNMAPPED, *vol ? *vol : -1));
});

ICOMMAND(0, soundslot, "s", (char *i), intret(gamesounds.getindex(i)));

void removemapsounds()
{
    loopv(sounds) if(sounds[i].index >= 0 && sounds[i].flags&SND_MAP) sounds[i].clear();
}

void removetrackedsounds(physent *d)
{
    loopv(sounds)
        if(sounds[i].index >= 0 && sounds[i].owner == d)
            sounds[i].clear();
}

void resetsound()
{
    clearchanges(CHANGE_SOUND);
    loopv(sounds) sounds[i].clear();
    enumerate(soundsamples, soundsample, s, s.cleanup());
    stopmusic();
    nosound = true;
    initsound();
    if(nosound)
    {
        gamesounds.clear(false);
        mapsounds.clear(false);
        mapsoundenvs.clear();
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

ALenum sound::setup(soundsample *s)
{
    if(!s->valid()) return AL_NO_ERROR;

    SOUNDERROR();
    alGenSources(1, &source);
    SOUNDERROR(clear(); return err);

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

    alSourcef(source, AL_ROLLOFF_FACTOR, rolloff);
    SOUNDERROR(clear(); return err);

    alSourcef(source, AL_REFERENCE_DISTANCE, refdist >= 0 ? refdist : soundrefdist);
    SOUNDERROR(clear(); return err);

    if(maxdist > 0)
    {
        alSourcef(source, AL_MAX_DISTANCE, maxdist);
        SOUNDERROR(clear(); return err);
    }

    return AL_NO_ERROR;
}

void sound::cleanup()
{
    if(!valid()) return;
    alSourceStop(source);
    alDeleteSources(1, &source);
}

void sound::reset()
{
    source = 0;
    pos = vel = vec(0, 0, 0);
    slot = NULL;
    owner = NULL;
    gain = curgain = pitch = curpitch = 1;
    material = MAT_AIR;
    flags = millis = ends = 0;
    rolloff = 1;
    refdist = -1;
    maxdist = 0;
    index = slotnum = -1;
    if(hook) *hook = -1;
    hook = NULL;
    buffer.shrink(0);
}

void sound::clear()
{
    cleanup();
    reset();
}

ALenum sound::update()
{
    if(owner && !(flags&SND_NOATTEN))
    {
        pos = owner->o;
        vel = owner->vel;
    }

    material = lookupmaterial(pos);
    curgain = clamp(soundeffectvol*slot->gain*(flags&SND_MAP ? soundeffectenv : soundeffectevent), 0.f, 1.f);

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

    vec o = pos, v = vel;
    if(flags&SND_NOATTEN) o = v = vec(0, 0, 0);
    else if(flags&SND_NOPAN)
    {
        float mag = vec(o).sub(camera1->o).magnitude();
        o = vec(camera1->yaw*RAD, camera1->pitch*RAD).mul(mag).add(camera1->o);
    }
    else if(flags&SND_NODIST) o.sub(camera1->o).safenormalize().mul(2).add(camera1->o);

    SOUNDERROR();
    alSourcefv(source, AL_POSITION, (ALfloat *) &o);
    SOUNDERROR(clear(); return err);

    alSourcefv(source, AL_VELOCITY, (ALfloat *) &v);
    SOUNDERROR(clear(); return err);

    return AL_NO_ERROR;
}

bool sound::valid()
{
    if(!source || !alIsSource(source)) return false;
    return true;
}

bool sound::active()
{
    if(!valid()) return false;
    ALint value;
    alGetSourcei(source, AL_SOURCE_STATE, &value);
    if(value != AL_PLAYING && value != AL_PAUSED) return false;
    return true;
}

bool sound::playing()
{
    if(!valid()) return false;
    ALint value;
    alGetSourcei(source, AL_SOURCE_STATE, &value);
    if(value != AL_PLAYING) return false;
    return true;
}

ALenum sound::play()
{
    if(playing()) return AL_NO_ERROR;
    SOUNDERROR();
    alSourcePlay(source);
    SOUNDERROR(clear(); return err);
    return AL_NO_ERROR;
}

ALenum sound::push(soundsample *s)
{
    SOUNDCHECK(setup(s), , return err);
    SOUNDCHECK(update(), , return err);
    SOUNDCHECK(play(), , return err);
    return AL_NO_ERROR;
}

ALenum music::setup(const char *n, soundfile *s)
{
    if(!s) return AL_NO_ERROR;

    name = newstring(n);

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

ALenum music::fill(ALint bufid)
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

void music::cleanup()
{
    if(!valid()) return;
    alSourceStop(source);
    alDeleteSources(1, &source);
    alDeleteBuffers(MUSICBUFS, buffer);
    if(name) delete[] name;
    if(data) delete data;
}

void music::reset()
{
    name = NULL;
    source = 0;
    loopi(MUSICBUFS) buffer[i] = 0;
    data = NULL;
    gain = 1;
    looping = true;
}

void music::clear()
{
    cleanup();
    reset();
}

ALenum music::update()
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

bool music::valid()
{
    if(!source || !alIsSource(source)) return false;
    loopi(MUSICBUFS) if(!alIsBuffer(buffer[i])) return false;
    return true;
}

bool music::active()
{
    if(!valid()) return false;
    ALint value;
    alGetSourcei(source, AL_SOURCE_STATE, &value);
    if(value != AL_PLAYING && value != AL_PAUSED) return false;
    return true;
}

bool music::playing()
{
    if(!valid()) return false;
    ALint value;
    alGetSourcei(source, AL_SOURCE_STATE, &value);
    if(value != AL_PLAYING) return false;
    return true;
}

ALenum music::play()
{
    if(playing()) return AL_NO_ERROR;
    SOUNDERROR();
    alSourcePlay(source);
    SOUNDERROR(clear(); return err);
    return AL_NO_ERROR;
}

ALenum music::push(const char *n, soundfile *s)
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
    if(idx < 0) intret(sounds.length());
    else if(sounds.inrange(idx))
    {
        if(prop < 0) intret(20);
        else switch(prop)
        {
            case 0: intret(sounds[idx].index); break;
            case 1: intret(sounds[idx].flags); break;
            case 2: intret(sounds[idx].material); break;
            case 3: intret(sounds[idx].millis); break;
            case 4: intret(sounds[idx].ends); break;
            case 5: intret(sounds[idx].slotnum); break;
            case 6: floatret(sounds[idx].gain); break;
            case 7: floatret(sounds[idx].curgain); break;
            case 8: floatret(sounds[idx].pitch); break;
            case 9: floatret(sounds[idx].curpitch); break;
            case 10: floatret(sounds[idx].rolloff); break;
            case 11: floatret(sounds[idx].refdist); break;
            case 12: floatret(sounds[idx].maxdist); break;
            case 13: defformatstring(pos, "%.f %.f %.f", sounds[idx].pos.x, sounds[idx].pos.y, sounds[idx].pos.z); result(pos); break;
            case 14: defformatstring(vel, "%.f %.f %.f", sounds[idx].vel.x, sounds[idx].vel.y, sounds[idx].vel.z); result(vel); break;
            case 15:
            {
                if(!sounds[idx].buffer.inrange(buf))
                {
                    intret(sounds[idx].buffer.length());
                    break;
                }
                intret(sounds[idx].buffer[buf]);
                break;
            }
            case 16: intret(sounds[idx].valid() ? 1 : 0); break;
            case 17: intret(sounds[idx].playing() ? 1 : 0); break;
            case 18: intret(client::getcn(sounds[idx].owner)); break;
            case 19: intret(sounds[idx].owner == camera1 ? 1 : 0); break;
            default: break;
        }
    }
}
ICOMMAND(0, getcursound, "bbb", (int *n, int *p, int *b), getcursounds(*n, *p, *b));

void getmusics(int prop)
{
    if(prop < 0)
    {
        intret(6);
        return;
    }
    SDL_LockMutex(mstream_mutex);
    if(mstream) switch(prop)
    {
        case 0: result(mstream->name);
        case 1: intret(mstream->looping ? 1 : 0); break;
        case 2: floatret(mstream->gain); break;
        case 3: intret(mstream->valid() ? 1 : 0); break;
        case 4: intret(mstream->active() ? 1 : 0); break;
        case 5: intret(mstream->playing() ? 1 : 0); break;
        default: break;
    }
    SDL_UnlockMutex(mstream_mutex);
}
ICOMMAND(0, getmusic, "b", (int *p), getmusics(*p));

static void initsoundenv() { initprops(newsoundenv->props, soundenvprops, SOUNDENV_PROPS); }

static void defsoundenv(const char *name, uint *code, slotmanager<soundenv> &envs)
{
    newsoundenvidx = envs.add(name);
    newsoundenv = &envs[newsoundenvidx];
    newsoundenv->name = envs.getname(newsoundenvidx);

    initsoundenv();
    execute(code);

    newsoundenvidx = -1;
    newsoundenv = NULL;
}
ICOMMAND(0, defsoundenv, "se", (char *name, uint *code), defsoundenv(name, code, soundenvs));
ICOMMAND(0, defmapsoundenv, "se", (char *name, uint *code), defsoundenv(name, code, mapsoundenvs));

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
}

ICOMMAND(0, soundenvpropi, "siie", (char *name, int *ival),
    setsoundenvprop(name, *ival));
ICOMMAND(0, soundenvpropf, "sfie", (char *name, float *fval),
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

void dumpsoundenvs(stream *s) { loopv(mapsoundenvs) dumpsoundenv(mapsoundenvs[i], s); }

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
