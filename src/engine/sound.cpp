#include "engine.h"
#include "SDL_mixer.h"

struct soundsample
{
    Mix_Chunk *sound;
    char *name;

    soundsample() : name(NULL) {}
    ~soundsample() { DELETEA(name); }

    void cleanup()
    {
        Mix_FreeChunk(sound);
        sound = NULL;
    }
};

soundslot::soundslot() : vol(255), maxrad(-1), minrad(-1), name(NULL) {}
soundslot::~soundslot() { DELETEA(name); }

sound::sound() : hook(NULL) { reset(); }
sound::~sound() {}
bool sound::playing() { return chan >= 0 && (Mix_Playing(chan) || Mix_Paused(chan)); }
void sound::reset()
{
    pos = oldpos = vec(-1, -1, -1);
    slot = NULL;
    owner = NULL;
    vol = curvol = 255;
    curpan = 127;
    material = MAT_AIR;
    flags = maxrad = minrad = millis = ends = 0;
    slotnum = chan = -1;
    if(hook) *hook = -1;
    hook = NULL;
    buffer.shrink(0);
}

hashnameset<soundsample> soundsamples;
vector<soundslot> gamesounds, mapsounds;
vector<sound> sounds;

bool nosound = true, changedvol = false, canmusic = false;
Mix_Music *music = NULL;
SDL_RWops *musicrw = NULL;
stream *musicstream = NULL;
char *musicfile = NULL, *musicdonecmd = NULL;
int musictime = -1, musicdonetime = -1;

void updatemusic()
{
    changedvol = true;
    if(!music && musicvol > 0 && mastervol > 0) smartmusic(true);
}

VARF(IDF_PERSIST, mastervol, 0, 255, 255, updatemusic());
VAR(IDF_PERSIST, soundvol, 0, 255, 255);
VARF(IDF_INIT, soundmono, 0, 0, 1, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(IDF_INIT, soundmixchans, 16, 256, 1024, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(IDF_INIT, soundfreq, 0, 44100, 48000, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(IDF_INIT, soundbuflen, 128, 1024, VAR_MAX, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VAR(IDF_PERSIST, soundmaxrad, 0, 512, VAR_MAX);
VAR(IDF_PERSIST, soundminrad, 0, 0, VAR_MAX);
FVAR(IDF_PERSIST, soundevtvol, 0, 1, FVAR_MAX);
FVAR(IDF_PERSIST, soundevtscale, 0, 1, FVAR_MAX);
FVAR(IDF_PERSIST, soundenvvol, 0, 1, FVAR_MAX);
FVAR(IDF_PERSIST, soundenvscale, 0, 1, FVAR_MAX);
VAR(IDF_PERSIST, soundcull, 0, 1, 1);

VARF(IDF_PERSIST, musicvol, 0, 25, 255, updatemusic());
VAR(IDF_PERSIST, musicfadein, 0, 1000, VAR_MAX);
VAR(IDF_PERSIST, musicfadeout, 0, 2500, VAR_MAX);
SVAR(0, titlemusic, "sounds/theme");

void initsound()
{
    if(nosound)
    {
        SDL_version version;
        SDL_GetVersion(&version);
        if(version.major == 2 && version.minor == 0 && version.patch == 6)
        {
            conoutf("\frSound is broken in SDL 2.0.6");
            return;
        }
        if(Mix_OpenAudio(soundfreq, MIX_DEFAULT_FORMAT, soundmono ? 1 : 2, soundbuflen) == -1)
        {
            conoutf("\frSound initialisation failed: %s", Mix_GetError());
            return;
        }
        int chans = Mix_AllocateChannels(soundmixchans);
        conoutf("Allocated %d of %d sound channels", chans, soundmixchans);
        nosound = false;
    }
    initmumble();
}

void stopmusic(bool docmd)
{
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
}

void musicdone(bool docmd)
{
    if(nosound) return;
    if(musicfadeout && !docmd)
    {
        if(Mix_PlayingMusic()) Mix_FadeOutMusic(musicfadeout);
    }
    else stopmusic(docmd);
}

void stopsound()
{
    if(nosound) return;
    Mix_HaltChannel(-1);
    stopmusic(false);
    clearsound();
    enumerate(soundsamples, soundsample, s, s.cleanup());
    soundsamples.clear();
    gamesounds.setsize(0);
    closemumble();
    Mix_CloseAudio();
    nosound = true;
}

void removesound(int c)
{
    if(!nosound) Mix_HaltChannel(c);
    sounds[c].reset();
}

void clearsound()
{
    loopv(sounds) removesound(i);
    mapsounds.setsize(0);
}

void getsounds(bool mapsnd, int idx, int prop)
{
    vector<soundslot> &soundset = mapsnd ? mapsounds : gamesounds;
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
            case 1: intret(sounds[idx].curvol); break;
            case 2: intret(sounds[idx].curpan); break;
            case 3: intret(sounds[idx].flags); break;
            case 4: intret(sounds[idx].maxrad); break;
            case 5: intret(sounds[idx].minrad); break;
            case 6: intret(sounds[idx].material); break;
            case 7: intret(sounds[idx].millis); break;
            case 8: intret(sounds[idx].ends); break;
            case 9: intret(sounds[idx].slotnum); break;
            case 10: intret(sounds[idx].chan); break;
            case 11: defformatstring(pos, "%.f %.f %.f", sounds[idx].pos.x, sounds[idx].pos.y, sounds[idx].pos.z); result(pos); break;
            case 12: defformatstring(oldpos, "%.f %.f %.f", sounds[idx].oldpos.x, sounds[idx].oldpos.y, sounds[idx].oldpos.z); result(oldpos); break;
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
}

bool playmusic(const char *name, const char *cmd)
{
    if(nosound) return false;
    stopmusic(false);
    if(!*name) return false;
    string buf;
    const char *dirs[] = { "", "sounds/" }, *exts[] = { "", ".wav", ".ogg" };
    bool found = false;
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
        changedvol = found = true;
        return true;
    }
    if(!music) conoutf("\frCould not play music: %s", name);
    return false;
}

COMMANDN(0, music, playmusic, "ss");

bool playingmusic(bool check)
{
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
    return false;
}

void smartmusic(bool cond, bool init)
{
    if(init) canmusic = true;
    if(!canmusic || nosound || !mastervol || !musicvol || (!cond && Mix_PlayingMusic()) || !*titlemusic) return;
    if(!playingmusic() || (cond && strcmp(musicfile, titlemusic))) playmusic(titlemusic);
    else
    {
        Mix_VolumeMusic(int((mastervol/255.f)*(musicvol/255.f)*MIX_MAX_VOLUME));
        changedvol = true;
    }
}
ICOMMAND(0, smartmusic, "i", (int *a), smartmusic(*a));

int findsound(const char *name, int vol, vector<soundslot> &soundset)
{
    loopv(soundset) if(!strcmp(soundset[i].name, name) && (!vol || soundset[i].vol == vol)) return i;
    return -1;
}

static Mix_Chunk *loadwav(const char *name)
{
    Mix_Chunk *c = NULL;
    stream *z = openzipfile(name, "rb");
    if(z)
    {
        SDL_RWops *rw = z->rwops();
        if(rw)
        {
            c = Mix_LoadWAV_RW(rw, 0);
            SDL_FreeRW(rw);
        }
        delete z;
    }
    if(!c) c = Mix_LoadWAV(findfile(name, "rb"));
    return c;
}

int addsound(const char *name, int vol, int maxrad, int minrad, int value, vector<soundslot> &soundset)
{
    if(vol <= 0 || vol >= 255) vol = 255;
    if(maxrad <= 0) maxrad = -1;
    if(minrad < 0) minrad = -1;
    if(value == 1)
    {
        loopv(soundset)
        {
            soundslot &slot = soundset[i];
            if(slot.vol == vol && slot.maxrad == maxrad && slot.minrad == minrad && !strcmp(slot.name, name))
                return i;
        }
    }
    if(!strcmp(name, "<none>"))
    {
        soundslot &slot = soundset.add();
        slot.name = newstring(name);
        slot.vol = 0;
        slot.maxrad = slot.minrad = -1;
        return soundset.length()-1;
    }
    soundsample *sample = NULL;
    #define loadsound(req) \
    { \
        if(!(sample = soundsamples.access(req))) \
        { \
            char *n = newstring(req); \
            sample = &soundsamples[n]; \
            sample->name = n; \
            sample->sound = NULL; \
        } \
        if(!sample->sound) \
        { \
            string buf; \
            const char *dirs[] = { "", "sounds/" }, *exts[] = { "", ".wav", ".ogg", ".mp3" }; \
            bool found = false; \
            loopi(sizeof(dirs)/sizeof(dirs[0])) \
            { \
                loopk(sizeof(exts)/sizeof(exts[0])) \
                { \
                    formatstring(buf, "%s%s%s", dirs[i], sample->name, exts[k]); \
                    if((sample->sound = loadwav(buf)) != NULL) found = true; \
                    if(found) break; \
                } \
                if(found) break; \
            } \
        } \
    }
    string sam;
    if(!nosound) loopi(value > 1 ? 2 : 1)
    {
        if(value > 1 && !i) formatstring(sam, "%s1", name);
        else copystring(sam, name);
        loadsound(sam);
        if(sample->sound) break;
        if(value < 2 || i) conoutf("\frFailed to load sample: %s", name);
    }
    soundslot &slot = soundset.add();
    slot.name = newstring(name);
    slot.vol = vol;
    slot.maxrad = maxrad; // use these values if none are supplied when playing
    slot.minrad = minrad;
    slot.samples.add(sample);
    if(!nosound && value > 1) loopi(value-1)
    {
        formatstring(sam, "%s%d", name, i+2);
        loadsound(sam);
        if(sample->sound) slot.samples.add(sample);
        else conoutf("\frFailed to load sample: %s", sam);
    }
    return soundset.length()-1;
}

ICOMMAND(0, registersound, "sissi", (char *n, int *v, char *w, char *x, int *u), intret(addsound(n, *v, *w ? parseint(w) : -1, *x ? parseint(x) : -1, *u, gamesounds)));
ICOMMAND(0, mapsound, "sissi", (char *n, int *v, char *w, char *x, int *u), intret(addsound(n, *v, *w ? parseint(w) : -1, *x ? parseint(x) : -1, *u, mapsounds)));

void calcvol(int flags, int vol, int slotvol, int maxrad, int minrad, const vec &pos, int *curvol, int *curpan, bool liquid)
{
    vec v;
    float dist = pos.dist(camera1->o, v);
    int svol = flags&SND_CLAMPED ? 255 : clamp(vol, 0, 255), span = 127;
    if(!(flags&SND_NOATTEN) && dist > 0)
    {
        if(!(flags&SND_NOPAN) && !soundmono && (v.x != 0 || v.y != 0))
        {
            v.rotate_around_z(-camera1->yaw*RAD);
            span = int(255.9f*(0.5f - 0.5f*v.x/v.magnitude2())); // range is from 0 (left) to 255 (right)
        }
        if(!(flags&SND_NODIST))
        {
            float mrad = int((maxrad > 0 ? maxrad : soundmaxrad)*(flags&SND_MAP ? soundenvscale : soundevtscale)),
                  nrad = minrad > 0 ? (minrad <= mrad ? minrad : mrad) : soundminrad;
            if(dist > nrad)
            {
                if(dist <= mrad) svol = int(svol*(1.f-((dist-nrad)/max(mrad-nrad,1e-16f))));
                else svol = 0;
            }
        }
    }
    if(!(flags&SND_NOQUIET) && svol > 0 && liquid) svol = int(svol*0.65f);
    if(flags&SND_CLAMPED) svol = max(svol, clamp(vol, 0, 255));
    *curvol = clamp(int((mastervol/255.f)*(soundvol/255.f)*(slotvol/255.f)*(svol/255.f)*MIX_MAX_VOLUME*(flags&SND_MAP ? soundenvvol : soundevtvol)), 0, MIX_MAX_VOLUME);
    *curpan = span;
}

void updatesound(int chan)
{
    sound &s = sounds[chan];
    bool waiting = (!(s.flags&SND_NODELAY) && Mix_Paused(chan));
    if((s.flags&SND_NOCULL) || (s.flags&SND_BUFFER) || !soundcull || s.curvol > 0 || s.pos.dist(camera1->o) <= 0)
    {
        if(waiting)
        { // delay the sound based on average physical constants
            bool liquid = isliquid(lookupmaterial(s.pos)&MATF_VOLUME) || isliquid(lookupmaterial(camera1->o)&MATF_VOLUME);
            float dist = camera1->o.dist(s.pos);
            int delay = int((dist/8.f)*(liquid ? 1.5f : 0.35f));
            if(lastmillis >= s.millis+delay)
            {
                Mix_Resume(chan);
                waiting = false;
            }
        }
        if(!waiting)
        {
            Mix_Volume(chan, s.curvol);
            if(!soundmono) Mix_SetPanning(chan, 255-s.curpan, s.curpan);
        }
    }
    else
    {
        removesound(chan);
        if(verbose >= 4) conoutf("Culled sound %d (%d)", chan, s.curvol);
    }
}

static bool updatesoundchan(int chan, int srcchan, soundslot *slot)
{
    while(chan >= sounds.length()) sounds.add();

    sound &s = sounds[srcchan];
    sound &t = sounds[chan];

    if(chan == s.chan) return false;

    t.slot = slot;
    t.vol = s.vol;
    t.maxrad = s.maxrad;
    t.minrad = s.minrad;
    t.material = s.material;
    t.flags = s.flags;
    t.millis = s.millis;
    t.ends = s.ends;
    t.slotnum = s.slotnum;
    t.owner = s.owner;
    t.pos = t.oldpos = s.pos;
    t.curvol = s.curvol;
    t.curpan = s.curpan;
    t.chan = chan;
    t.hook = s.hook;
    loopv(s.buffer) t.buffer.add(s.buffer[i]);
    updatesound(chan);

    return true;
}

void updatesounds()
{
    updatemumble();
    if(nosound) return;
    bool liquid = isliquid(lookupmaterial(camera1->o)&MATF_VOLUME);
    loopv(sounds) if(sounds[i].chan >= 0)
    {
        sound &s = sounds[i];
        if((!s.ends || lastmillis < s.ends) && Mix_Playing(sounds[i].chan))
        {
            if(s.owner) s.pos = game::camerapos(s.owner);
            if(s.pos != s.oldpos) s.material = lookupmaterial(s.pos);
            calcvol(s.flags, s.vol, s.slot->vol, s.maxrad, s.minrad, s.pos, &s.curvol, &s.curpan, liquid || isliquid(s.material&MATF_VOLUME));
            s.oldpos = s.pos;
            updatesound(i);
            continue;
        }
        vector<soundslot> &soundset = s.flags&SND_MAP ? mapsounds : gamesounds;
        while(!s.buffer.empty() && (!soundset.inrange(s.buffer[0]) || soundset[s.buffer[0]].samples.empty() || !soundset[s.buffer[0]].vol)) s.buffer.remove(0);
        if(!s.buffer.empty())
        {
            int n = s.buffer[0], chan = -1;
            Mix_HaltChannel(i);
            s.buffer.remove(0);
            bool nocull = s.flags&SND_NOCULL || s.pos.dist(camera1->o) <= 0;
            soundslot *slot = &soundset[n];
            soundsample *sample = slot->samples[rnd(slot->samples.length())];
            if((chan = Mix_PlayChannel(i, sample->sound, s.flags&SND_LOOP ? -1 : 0)) < 0)
            {
                int lowest = -1;
                loopv(sounds) if(sounds[i].chan >= 0 && (!(sounds[i].flags&SND_MAP) || s.flags&SND_MAP) && sounds[i].curvol < s.curvol && (lowest < 0 || sounds[i].curvol < sounds[lowest].curvol) && (nocull || (!(sounds[i].flags&SND_NOCULL) && sounds[i].pos.dist(camera1->o) > 0)))
                    lowest = i;
                if(sounds.inrange(lowest))
                {
                    if(verbose >= 4) conoutf("Culled channel %d (%d)", lowest, sounds[lowest].curvol);
                    removesound(lowest);
                    chan = Mix_PlayChannel(-1, sample->sound, s.flags&SND_LOOP ? -1 : 0);
                }
            }
            if(chan >= 0 && !updatesoundchan(chan, i, slot))
            {
                i--;
                continue;
            }
        }
        removesound(i);
    }
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
}

int playsound(int n, const vec &pos, physent *d, int flags, int vol, int maxrad, int minrad, int *hook, int ends, int *oldhook)
{
    if(nosound || !mastervol || !soundvol || ((flags&SND_MAP || n >= S_GAMESPECIFIC) && client::waiting(true)) || (!d && !insideworld(pos))) return -1;
    vector<soundslot> &soundset = flags&SND_MAP ? mapsounds : gamesounds;

    if(soundset.inrange(n))
    {
        if(hook && issound(*hook) && flags&SND_BUFFER)
        {
            sounds[*hook].buffer.add(n);
            return *hook;
        }
        if(soundset[n].samples.empty() || !soundset[n].vol)
        {
            if(oldhook && issound(*oldhook)) removesound(*oldhook);
            return -1;
        }
        soundslot *slot = &soundset[n];
        if(!oldhook || !issound(*oldhook) || (n != sounds[*oldhook].slotnum && strcmp(slot->name, gamesounds[sounds[*oldhook].slotnum].name)))
            oldhook = NULL;

        vec o = d ? game::camerapos(d) : pos;
        int cvol = 0, cpan = 0, v = clamp(vol >= 0 ? vol : 255, flags&SND_CLAMPED ? 64 : 0, 255),
            x = maxrad > 0 ? maxrad : (flags&SND_CLAMPED ? worldsize : (slot->maxrad > 0 ? slot->maxrad : soundmaxrad)),
            y = minrad >= 0 ? minrad : (flags&SND_CLAMPED ? 32 : (slot->minrad >= 0 ? slot->minrad : soundminrad)),
            mat = lookupmaterial(o);

        bool liquid = isliquid(lookupmaterial(camera1->o)&MATF_VOLUME);
        calcvol(flags, v, slot->vol, x, y, o, &cvol, &cpan, liquid || isliquid(mat&MATF_VOLUME));
        bool nocull = flags&SND_NOCULL || o.dist(camera1->o) <= 0;

        if(nocull || !soundcull || cvol > 0)
        {
            int chan = -1;
            if(oldhook) chan = *oldhook;
            else
            {
                oldhook = NULL;
                soundsample *sample = slot->samples[rnd(slot->samples.length())];
                if((chan = Mix_PlayChannel(-1, sample->sound, flags&SND_LOOP ? -1 : 0)) < 0)
                {
                    int lowest = -1;
                    loopv(sounds) if(sounds[i].chan >= 0 && (!(sounds[i].flags&SND_MAP) || flags&SND_MAP) && sounds[i].curvol < cvol && (lowest < 0 || sounds[i].curvol < sounds[lowest].curvol) && (nocull || (!(sounds[i].flags&SND_NOCULL) && sounds[i].pos.dist(camera1->o) > 0)))
                        lowest = i;
                    if(sounds.inrange(lowest))
                    {
                        if(verbose >= 4) conoutf("Culled channel %d (%d)", lowest, sounds[lowest].curvol);
                        removesound(lowest);
                        chan = Mix_PlayChannel(-1, sample->sound, flags&SND_LOOP ? -1 : 0);
                    }
                }
            }
            if(chan >= 0)
            {
                if(!oldhook && !(flags&SND_NODELAY)) Mix_Pause(chan);

                while(chan >= sounds.length()) sounds.add();

                sound &s = sounds[chan];

                // invalidate old hook
                if(s.hook && s.hook != hook) *s.hook = -1;

                s.slot = slot;
                s.vol = v;
                s.maxrad = x;
                s.minrad = y;
                s.material = mat;
                s.flags = flags;
                s.millis = oldhook && sounds.inrange(*oldhook) ? sounds[*oldhook].millis : lastmillis;
                s.ends = ends;
                s.slotnum = n;
                s.owner = d;
                s.pos = s.oldpos = o;
                s.curvol = cvol;
                s.curpan = cpan;
                s.chan = chan;
                if(hook)
                {
                    if(issound(*hook) && (!oldhook || *hook != *oldhook)) removesound(*hook);
                    *hook = s.chan;
                    s.hook = hook;
                }
                else s.hook = NULL;
                if(oldhook) *oldhook = -1;
                updatesound(chan);
                return chan;
            }
            else if(verbose >= 2)
                conoutf("\frCannot play sound %d (%s): %s", n, slot->name, Mix_GetError());
        }
        else if(verbose >= 4) conoutf("Culled sound %d (%d)", n, cvol);
    }
    else if(n > 0) conoutf("\frUnregistered sound: %d", n);
    if(oldhook && issound(*oldhook)) removesound(*oldhook);
    return -1;
}

void sound(int *n, int *vol, int *flags)
{
    intret(playsound(*n, camera1->o, camera1, *flags >= 0 ? *flags : SND_FORCED, *vol ? *vol : -1));
}
COMMAND(0, sound, "iib");

void removemapsounds()
{
    loopv(sounds) if(sounds[i].chan >= 0 && sounds[i].flags&SND_MAP) removesound(i);
}

void removetrackedsounds(physent *d)
{
    loopv(sounds)
        if(sounds[i].chan >= 0 && sounds[i].owner == d)
            removesound(i);
}

void resetsound()
{
    clearchanges(CHANGE_SOUND);
    if(!nosound)
    {
        loopv(sounds) removesound(i);
        enumerate(soundsamples, soundsample, s, s.cleanup());
        if(music)
        {
            Mix_HaltMusic();
            Mix_FreeMusic(music);
        }
        if(musicstream) musicstream->seek(0, SEEK_SET);
        Mix_CloseAudio();
        nosound = true;
    }
    initsound();
    if(nosound)
    {
        DELETEA(musicfile);
        DELETEA(musicdonecmd);
        music = NULL;
        gamesounds.setsize(0);
        mapsounds.setsize(0);
        soundsamples.clear();
        return;
    }
    rehash(true);
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

