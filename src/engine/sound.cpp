#include "engine.h"
#include <AL/al.h>

ALCdevice *snddev = NULL;
ALCcontext *sndctx = NULL;

bool nosound = true, changedvol = false, canmusic = false;
bool al_ext_efx = false, al_soft_spatialize = false, al_ext_float32 = false;

hashnameset<soundsample> soundsamples;
slotmanager<soundslot> gamesounds, mapsounds;
vector<slot *> soundmap;
vector<sound> sounds;

char *musicfile = NULL, *musicdonecmd = NULL;
int musictime = -1, musicdonetime = -1;

VAR(IDF_PERSIST, mastervol, 0, 255, 255);
VAR(IDF_PERSIST, soundvol, 0, 255, 255);
SVARF(IDF_INIT, sounddev, "", initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(IDF_INIT, soundmono, 0, 0, 1, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(IDF_INIT, soundsrcs, 16, 256, 1024, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(IDF_INIT, soundfreq, 0, 44100, 48000, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(IDF_INIT, soundbuflen, 128, 1024, VAR_MAX, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VAR(IDF_PERSIST, soundmaxrad, 0, 512, VAR_MAX);
VAR(IDF_PERSIST, soundminrad, 0, 0, VAR_MAX);
FVAR(IDF_PERSIST, soundevtvol, 0, 1, FVAR_MAX);
FVAR(IDF_PERSIST, soundevtscale, 0, 1, FVAR_MAX);
FVAR(IDF_PERSIST, soundenvvol, 0, 1, FVAR_MAX);
FVAR(IDF_PERSIST, soundenvscale, 0, 1, FVAR_MAX);

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
FVARF(IDF_PERSIST, sounddoppler, 0, 0.05f, FVAR_MAX, soundsetdoppler(sounddoppler));

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
    changedvol = true;
#if 0
    if(!connected() && !music && musicvol > 0 && mastervol > 0) smartmusic(true);
#endif
}
VARF(IDF_PERSIST, musicvol, 0, 25, 255, updatemusic());
VAR(IDF_PERSIST, musicfadein, 0, 1000, VAR_MAX);
VAR(IDF_PERSIST, musicfadeout, 0, 2500, VAR_MAX);
SVAR(0, titlemusic, "sounds/theme");

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

    /*
    minrad = clamp(minrad, 1.f, maxrad); // must be > 0
    alSourcef(source, AL_REFERENCE_DISTANCE, minrad);
    SOUNDERROR(clear(); return err);

    maxrad = max(maxrad, minrad);
    alSourcef(source, AL_MAX_DISTANCE, maxrad);
    SOUNDERROR(clear(); return err);

    alSourcef(source, AL_ROLLOFF_FACTOR, 1.f);
    SOUNDERROR(clear(); return err);
    */

    alSourcef(source, AL_REFERENCE_DISTANCE, soundrefdist);
    SOUNDERROR(clear(); return err);

    alSourcef(source, AL_ROLLOFF_FACTOR, soundrolloff / maxrad);
    SOUNDERROR(clear(); return err);

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
    curvol = clamp((soundvol/255.f)*(vol/255.f)*(slot->vol/255.f)*(flags&SND_MAP ? soundenvvol : soundevtvol), 0.f, 1.f);

    SOUNDERROR();
    alSourcef(source, AL_GAIN, curvol);
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
        if(prop < 0) intret(4);
        else switch(prop)
        {
            case 0: intret(soundset[idx].vol); break;
            case 1: intret(soundset[idx].maxrad); break;
            case 2: intret(soundset[idx].minrad); break;
            case 3: result(soundset[idx].name); break;
            default: break;
        }
    }
}
ICOMMAND(0, getsound, "ibb", (int *n, int *v, int *p), getsounds(*n!=0, *v, *p));

void getcursounds(int idx, int prop)
{
    if(idx < 0) intret(sounds.length());
    else if(sounds.inrange(idx))
    {
        if(prop < 0) intret(19);
        else switch(prop)
        {
            case 0: intret(sounds[idx].vol); break;
            case 1: intret(int(sounds[idx].curvol*255)); break;
            case 2: intret(0); break; // curpan
            case 3: intret(sounds[idx].flags); break;
            case 4: intret(sounds[idx].maxrad); break;
            case 5: intret(sounds[idx].minrad); break;
            case 6: intret(sounds[idx].material); break;
            case 7: intret(sounds[idx].millis); break;
            case 8: intret(sounds[idx].ends); break;
            case 9: intret(sounds[idx].slotnum); break;
            case 10: intret(sounds[idx].source); break;
            case 11: defformatstring(pos, "%.f %.f %.f", sounds[idx].pos.x, sounds[idx].pos.y, sounds[idx].pos.z); result(pos); break;
            case 12: defformatstring(vel, "%.f %.f %.f", sounds[idx].vel.x, sounds[idx].vel.y, sounds[idx].vel.z); result(vel); break;
            case 13: intret(sounds[idx].valid() ? 1 : 0); break;
            case 14: intret(sounds[idx].playing() ? 1 : 0); break;
            case 15: intret(sounds[idx].flags&SND_MAP ? 1 : 0); break;
            case 16: intret(sounds[idx].owner!=NULL ? 1 : 0); break;
            case 17: intret(sounds[idx].owner==camera1 ? 1 : 0); break;
            case 18: intret(client::getcn(sounds[idx].owner)); break;
            default: break;
        }
    }
}
ICOMMAND(0, getcursound, "bb", (int *n, int *p), getcursounds(*n, *p));

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

void initsound()
{
    if(nosound)
    {
        if(*sounddev)
        {
            alGetError();
            snddev = alcOpenDevice(sounddev);
            if(!snddev) conoutf("\frSound device initialisation failed: %s (with '%s', retrying default device)", sounderror(), sounddev);
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

        if(alIsExtensionPresent("AL_EXT_EFX")) al_ext_efx = true;
        if(alIsExtensionPresent("AL_EXT_FLOAT32")) al_ext_float32 = true;

        if(alIsExtensionPresent("AL_SOFT_source_spatialize")) al_soft_spatialize = true;
        else conoutf("\frAL_SOFT_source_spatialize not found, stereo sounds will be downmixed to mono");

        nosound = false;

        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
        soundsetdoppler(sounddoppler);
        soundsetspeed(soundspeed);
    }
    initmumble();
}

void stopmusic(bool docmd)
{
#if 0
    if(nosound) return;
    if(Mix_PlayingMusic()) Mix_HaltMusic();
    if(music)
    {
        Mix_FreeMusic(music);
        music = NULL;
    }
    if(musicrw) { SDL_FreeRW(musicrw); musicrw = NULL; }
    DELETEP(musicstream);
    DELETEA(musicfile);
    if(musicdonecmd != NULL)
    {
        char *cmd = musicdonecmd;
        musicdonecmd = NULL;
        if(docmd) execute(cmd);
        delete[] cmd;
    }
    musicdonetime = -1;
#endif
}

void musicdone(bool docmd)
{
#if 0
    if(nosound) return;
    if(musicfadeout && !docmd)
    {
        if(Mix_PlayingMusic()) Mix_FadeOutMusic(musicfadeout);
    }
    else stopmusic(docmd);
#endif
}

void stopsound()
{
    if(nosound) return;
    stopmusic(false);
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
}

#if 0
Mix_Music *loadmusic(const char *name)
{
    if(!musicstream) musicstream = openzipfile(name, "rb");
    if(musicstream)
    {
        if(!musicrw) musicrw = musicstream->rwops();
        if(!musicrw) DELETEP(musicstream);
    }
    if(musicrw) music = Mix_LoadMUSType_RW(musicrw, MUS_NONE, 0);
    else music = Mix_LoadMUS(findfile(name, "rb"));
    if(!music)
    {
        if(musicrw) { SDL_FreeRW(musicrw); musicrw = NULL; }
        DELETEP(musicstream);
    }
    return music;
    return NULL;
}
#endif

bool playmusic(const char *name, const char *cmd)
{
#if 0
    if(nosound) return false;
    stopmusic(false);
    if(!*name) return false;
    string buf;
    const char *dirs[] = { "", "sounds/" }, *exts[] = { "", ".wav", ".ogg" };
    loopi(sizeof(dirs)/sizeof(dirs[0])) loopk(sizeof(exts)/sizeof(exts[0]))
    {
        formatstring(buf, "%s%s%s", dirs[i], name, exts[k]);
        if(!loadmusic(buf)) continue;
        DELETEA(musicfile);
        DELETEA(musicdonecmd);
        musicfile = newstring(name);
        if(cmd && *cmd) musicdonecmd = newstring(cmd);
        musicdonetime = -1;
        if(musicfadein) Mix_FadeInMusic(music, cmd && *cmd ? 0 : -1, musicfadein);
        else Mix_PlayMusic(music, cmd && *cmd ? 0 : -1);
        Mix_VolumeMusic(int((mastervol/255.f)*(musicvol/255.f)*MIX_MAX_VOLUME));
        changedvol = true;
        return true;
    }
    if(!music) conoutf("\frCould not play music: %s", name);
#endif
    return false;
}

COMMANDN(0, music, playmusic, "ss");

bool playingmusic(bool check)
{
#if 0
    if(!music) return false;
    if(Mix_PlayingMusic())
    {
        if(musicdonetime >= 0) musicdonetime = -1;
        return true;
    }
    if(!check) return false;
    if(musicdonetime < 0)
    {
        musicdonetime = totalmillis;
        return true;
    }
    if(totalmillis-musicdonetime < 500) return true;
#endif
    return false;
}

void smartmusic(bool cond, bool init)
{
#if 0
    if(init) canmusic = true;
    if(!canmusic || nosound || !mastervol || !musicvol || (!cond && Mix_PlayingMusic()) || !*titlemusic) return;
    if(!playingmusic() || (cond && strcmp(musicfile, titlemusic))) playmusic(titlemusic);
    else
    {
        Mix_VolumeMusic(int((mastervol/255.f)*(musicvol/255.f)*MIX_MAX_VOLUME));
        changedvol = true;
    }
#endif
}
ICOMMAND(0, smartmusic, "i", (int *a), smartmusic(*a));

int findsound(const char *name, int vol, vector<soundslot> &soundset)
{
    loopv(soundset) if(!strcmp(soundset[i].name, name) && (!vol || soundset[i].vol == vol)) return i;
    return -1;
}

soundfile *loadwav(const char *name, bool source = true)
{
    soundfile *w = new soundfile(al_ext_float32);
    SNDFILE *sndfile = sf_open(findfile(name, "rb"), SFM_READ, &w->info);
    if(!sndfile) return NULL;

    if(w->info.frames < 1 || w->info.frames > (sf_count_t)(INT_MAX/w->size)/w->info.channels)
    {
        defformatstring(fr, SI64, w->info.frames);
        conoutf("\frInvalid numver of frames in %s: %s", name, fr);
        sf_close(sndfile);
        delete w;
        return NULL;
    }

    if(w->info.channels == 1) w->format = al_ext_float32 ? AL_FORMAT_MONO_FLOAT32 : AL_FORMAT_MONO16;
    else if(w->info.channels == 2) w->format = al_ext_float32 ? AL_FORMAT_STEREO_FLOAT32 : AL_FORMAT_STEREO16;
#if 0
    else if(w->info.channels == 3)
    {
        if(sf_command(sndfile, SFC_WAVEX_GET_AMBISONIC, NULL, 0) == SF_AMBISONIC_B_FORMAT)
            w->format = AL_FORMAT_BFORMAT2D_16;
    }
    else if(w->info.channels == 4)
    {
        if(sf_command(sndfile, SFC_WAVEX_GET_AMBISONIC, NULL, 0) == SF_AMBISONIC_B_FORMAT)
            w->format = AL_FORMAT_BFORMAT3D_16;
    }
#endif

    if(!w->format)
    {
        conoutf("\frUnsupported channel count in %s: %d", name, w->info.channels);
        sf_close(sndfile);
        delete w;
        return NULL;
    }

    if(al_ext_float32)
    {
        w->data_f = new float[(size_t)(w->info.frames * w->info.channels) * w->size];
        w->frames = sf_readf_float(sndfile, w->data_f, w->info.frames);
    }
    else
    {
        w->data_s = new short[(size_t)(w->info.frames * w->info.channels) * w->size];
        w->frames = sf_readf_short(sndfile, w->data_s, w->info.frames);
    }

    if(w->frames < 1)
    {
        w->clear();
        sf_close(sndfile);
        defformatstring(fr, SI64, w->frames);
        conoutf("\frFailed to read samples in %s: %s", name, fr);
        delete w;
        return NULL;
    }

    if(source && !al_soft_spatialize)
    { // sources need to be downmixed to mono if soft spatialize extension isn't found
        switch(w->type)
        {
            case soundfile::SHORT:
            {
                short *data = w->data_s;
                w->data_s = new short[(size_t)w->frames * w->size];
                loopi(w->frames)
                {
                    int avg = 0;
                    loopj(w->info.channels) avg += data[i*w->info.channels + j];
                    w->data_s[i] = short(avg/w->info.channels);
                }
                w->format = AL_FORMAT_MONO16;
                w->info.channels = 1;
                delete[] data;
                break;
            }
            case soundfile::FLOAT:
            {
                float *data = w->data_f;
                w->data_f = new float[(size_t)w->frames * w->size];
                loopi(w->frames)
                {
                    float avg = 0;
                    loopj(w->info.channels) avg += data[i*w->info.channels + j];
                    w->data_f[i] = avg/w->info.channels;
                }
                w->format = AL_FORMAT_MONO_FLOAT32;
                w->info.channels = 1;
                delete[] data;
                break;
            }
            default: break;
        }
        w->len = (ALsizei)(w->frames) * (ALsizei)w->size;
    }
    else w->len = (ALsizei)(w->frames * w->info.channels) * (ALsizei)w->size;

    sf_close(sndfile);

    return w;
}

static soundsample *loadsound(const char *name)
{
    soundsample *sample = NULL;
    if(!(sample = soundsamples.access(name)))
    {
        char *n = newstring(name);
        sample = &soundsamples[n];
        sample->name = n;
        sample->reset();
    }

    if(!sample->valid())
    {
        string buf;
        const char *dirs[] = { "", "sounds/" }, *exts[] = { "", ".wav", ".ogg", ".mp3" };
        bool found = false;
        loopi(sizeof(dirs)/sizeof(dirs[0]))
        {
            loopk(sizeof(exts)/sizeof(exts[0]))
            {
                formatstring(buf, "%s%s%s", dirs[i], sample->name, exts[k]);
                soundfile *w = loadwav(buf);
                if(w != NULL)
                {
                    SOUNDCHECK(sample->setup(w), found = true, conoutf("Error loading %s: %s", buf, alGetString(err)));
                    delete w; // don't need to keep around the data
                }
                if(found) break;
            }
            if(found) break;
        }
    }
    return sample;
}

static void loadsamples(soundslot &slot, const char *name, int variants, bool farvariant)
{
    soundsample *sample;
    string sam;

    loopi(variants)
    {
        copystring(sam, name);
        if(farvariant) concatstring(sam, "far");
        if(i) concatstring(sam, intstr(i + 1));

        sample = loadsound(sam);
        slot.samples.add(sample);

        if(!sample || !sample->valid()) conoutf("\frFailed to load sample: %s", sam);
    }
}

int addsound(const char *id, const char *name, int vol, int maxrad, int minrad, int variants, int fardistance, slotmanager<soundslot> &soundset)
{
    if(nosound || !strcmp(name, "<none>")) return -1;

    if(vol <= 0 || vol >= 255) vol = 255;
    if(maxrad <= 0) maxrad = -1;
    if(minrad < 0) minrad = -1;
    if(variants < 1) variants = 1;

    int newidx = soundset.add(id);
    soundslot &slot = soundset[newidx];

    slot.reset();
    slot.name = newstring(name);
    slot.vol = vol;
    slot.maxrad = maxrad; // use these values if none are supplied when playing
    slot.minrad = minrad;
    slot.variants = variants;
    slot.fardistance = fardistance;

    loadsamples(slot, name, variants, false);
    if(fardistance > 0) loadsamples(slot, name, variants, true);

    return newidx;
}

ICOMMAND(0, registersound, "ssissii", (char *i, char *n, int *v, char *w, char *x, int *u, int *f), intret(addsound(i, n, *v, *w ? parseint(w) : -1, *x ? parseint(x) : -1, *u, *f, gamesounds)));
ICOMMAND(0, mapsound, "sissi", (char *n, int *v, char *w, char *x, int *u), intret(addsound(NULL, n, *v, *w ? parseint(w) : -1, *x ? parseint(x) : -1, *u, 0, mapsounds)));

static inline bool sourcecmp(const sound *x, const sound *y)
{
    if(x->flags&SND_PRIORITY && !(y->flags&SND_PRIORITY)) return true;
    if(!(x->flags&SND_PRIORITY) && y->flags&SND_PRIORITY) return false;
    if(x->pos.dist(camera1->o) < y->pos.dist(camera1->o)) return true;
    if(x->pos.dist(camera1->o) > y->pos.dist(camera1->o)) return false;
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

    while(sources.length() >= soundsrcs)
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
        while(!s.buffer.empty() && (!soundset.inrange(s.buffer[0]) || soundset[s.buffer[0]].samples.empty() || !soundset[s.buffer[0]].vol)) s.buffer.remove(0);
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
#if 0
    if(music || Mix_PlayingMusic())
    {
        if(nosound || !mastervol || !musicvol) stopmusic(false);
        else if(!playingmusic()) musicdone(true);
        else if(changedvol)
        {
            Mix_VolumeMusic(int((mastervol/255.f)*(musicvol/255.f)*MIX_MAX_VOLUME));
            changedvol = false;
        }
    }
#endif
    vec o[2];
    o[0].x = (float)(cosf(RAD*(camera1->yaw-90)));
    o[0].y = (float)(sinf(RAD*(camera1->yaw-90)));
    o[0].z = o[1].x = o[1].y = 0.0f;
    o[1].z = 1.0f;
    alListenerf(AL_GAIN, mastervol/255.f);
    alListenerfv(AL_ORIENTATION, (ALfloat *) &o);
    alListenerfv(AL_POSITION, (ALfloat *) &camera1->o);
    alListenerfv(AL_VELOCITY, (ALfloat *) &game::focusedent()->vel);
    if(al_ext_efx) alListenerf(AL_METERS_PER_UNIT, 0.125f); // 8 units = 1 meter

    alcProcessContext(sndctx);
}

int playsound(int n, const vec &pos, physent *d, int flags, int vol, int maxrad, int minrad, int *hook, int ends)
{
    if(nosound || !vol || !mastervol || !soundvol || !vol || (flags&SND_MAP ? !soundenvvol : !soundevtvol)) return -1;
    if(((flags&SND_MAP || (!(flags&SND_UNMAPPED) && n >= S_GAMESPECIFIC)) && client::waiting(true)) || (!d && !insideworld(pos))) return -1;

    slotmanager<soundslot> &soundset = flags&SND_MAP ? mapsounds : gamesounds;
    if(!(flags&SND_UNMAPPED) && !(flags&SND_MAP)) n = getsoundslot(n);

    if(soundset.inrange(n))
    {
        soundslot *slot = &soundset[n];
        if(slot->samples.empty() || !slot->vol) return -1;
        if(hook && issound(*hook) && flags&SND_BUFFER)
        {
            sounds[*hook].buffer.add(n);
            return *hook;
        }

        vec o = d ? d->o : pos, v = d ? d->vel : vec(0, 0, 0);
        int variant = rnd(slot->variants);
        if(slot->fardistance && o.dist(camera1->o) >= slot->fardistance) variant += slot->variants;
        soundsample *sample = slot->samples[variant];
        if(!sample || !sample->valid()) return -1;

        int index = soundindex();
        while(index >= sounds.length()) sounds.add();
        sound &s = sounds[index];
        if(s.hook && s.hook != hook) *s.hook = -1;

        s.slot = slot;
        s.vol = vol > 0 ? vol : 255;
        s.maxrad = maxrad > 0 ? maxrad : (slot->maxrad > 0 ? slot->maxrad : soundmaxrad);
        s.minrad = minrad >= 0 ? minrad : (slot->minrad >= 0 ? slot->minrad : soundminrad);
        s.flags = flags;
        s.millis = lastmillis;
        s.ends = ends;
        s.slotnum = n;
        s.owner = d;
        s.pos = o;
        s.vel = v;
        s.index = index;
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
    if(!nosound)
    {
        loopv(sounds) sounds[i].clear();
        enumerate(soundsamples, soundsample, s, s.cleanup());
#if 0
        if(music)
        {
            Mix_HaltMusic();
            Mix_FreeMusic(music);
        }
        if(musicstream) musicstream->seek(0, SEEK_SET);
        Mix_CloseAudio();
#endif
        nosound = true;
    }
    initsound();
    if(nosound)
    {
#if 0
        DELETEA(musicfile);
        DELETEA(musicdonecmd);
        music = NULL;
#endif
        gamesounds.clear(false);
        mapsounds.clear(false);
        soundsamples.clear();
        return;
    }
    rehash(true);
#if 0
    if(music && loadmusic(musicfile))
    {
        if(musicfadein) Mix_FadeInMusic(music, musicdonecmd ? 0 : -1, musicfadein);
        else Mix_PlayMusic(music, musicdonecmd ? 0 : -1);
        Mix_VolumeMusic(int((mastervol/255.f)*(musicvol/255.f)*MIX_MAX_VOLUME));
        changedvol = true;
    }
    else
    {
        DELETEA(musicfile);
        DELETEA(musicdonecmd);
    }
#endif
}

COMMAND(0, resetsound, "");

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

