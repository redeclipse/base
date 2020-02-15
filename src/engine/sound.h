enum
{
    S_PRESS = 0, S_BACK, S_ACTION, S_NUM_GENERIC,
    S_GAMESPECIFIC = S_NUM_GENERIC
};

enum
{
    SND_NONE     = 0,
    SND_NOATTEN  = 1<<0, // disable attenuation
    SND_NODELAY  = 1<<1, // disable delay
    SND_NOCULL   = 1<<2, // disable culling
    SND_NOPAN    = 1<<3, // disable panning (distance only attenuation)
    SND_NODIST   = 1<<4, // disable distance (panning only)
    SND_NOQUIET  = 1<<5, // disable water effects (panning only)
    SND_CLAMPED  = 1<<6, // makes volume the minimum volume to clamp to
    SND_LOOP     = 1<<7, // loops when it reaches the end
    SND_BUFFER   = 1<<8, // channel becomes/adds to a buffer for sounds
    SND_MAP      = 1<<9,
    SND_UNMAPPED = 1<<10, // skipping slot index mapping
    SND_IMPORT   = SND_NODELAY|SND_NOCULL|SND_NOQUIET,
    SND_FORCED   = SND_IMPORT|SND_NOATTEN|SND_NODIST,
    SND_DIRECT   = SND_IMPORT|SND_CLAMPED,
    SND_MASKF    = SND_LOOP|SND_MAP,
    SND_LAST     = 8
};

#ifndef STANDALONE
#define SOUNDMINDIST        16.0f
#define SOUNDMAXDIST        10000.f

struct soundsample;

struct soundslot
{
    vector<soundsample *> samples;
    int vol, maxrad, minrad;
    char *name;

    soundslot();
    ~soundslot();
    void reset();
};

struct sound
{
    soundslot *slot;
    vec pos, oldpos;
    physent *owner;
    int vol, curvol, curpan;
    int flags, maxrad, minrad, material;
    int millis, ends, slotnum, chan, *hook;
    vector<int> buffer;

    sound();
    ~sound();

    void reset();
    bool playing();
    bool valid() { return chan >= 0 && playing(); }
};

extern bool nosound;
extern int mastervol, soundvol, musicvol;
extern slotmanager<soundslot> gamesounds, mapsounds;
extern vector<sound> sounds;

#define issound(c) (sounds.inrange(c) && sounds[c].valid())

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
extern void removesound(int c);
extern void clearsound();
extern int playsound(int n, const vec &pos, physent *d = NULL, int flags = 0, int vol = -1, int maxrad = -1, int minrad = -1, int *hook = NULL, int ends = 0, int *oldhook = NULL);
extern void removetrackedsounds(physent *d);
extern void removemapsounds();

extern void initmumble();
extern void closemumble();
extern void updatemumble();
#endif
