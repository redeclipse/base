#ifdef GAMESERVER
#define capturedelay (m_ctf_defend(gamemode, mutators) ? G(capturedefenddelay) : G(captureresetdelay))
#define capturestore (G(captureresetstore)&((m_ctf_quick(gamemode, mutators) ? 1 : 0)|(m_ctf_defend(gamemode, mutators) ? 2 : 0)|(m_ctf_protect(gamemode, mutators) ? 4 : 0)|(!m_ctf_quick(gamemode, mutators) && !m_ctf_defend(gamemode, mutators) && !m_ctf_protect(gamemode, mutators) ? 8 : 0)))
#define capturestate captureservstate
#else
#define capturedelay (m_ctf_defend(game::gamemode, game::mutators) ? G(capturedefenddelay) : G(captureresetdelay))
#define capturestore (G(captureresetstore)&((m_ctf_quick(game::gamemode, game::mutators) ? 1 : 0)|(m_ctf_defend(game::gamemode, game::mutators) ? 2 : 0)|(m_ctf_protect(game::gamemode, game::mutators) ? 4 : 0)|(!m_ctf_quick(game::gamemode, game::mutators) && !m_ctf_defend(game::gamemode, game::mutators) && !m_ctf_protect(game::gamemode, game::mutators) ? 8 : 0)))
#endif
struct capturestate
{
    struct flag
    {
        vec droploc, inertia, spawnloc;
        int team, yaw, pitch, droptime, taketime, dropoffset;
#ifdef GAMESERVER
        int owner, lastowner, lastownerteam, returntime;
        vector<int> votes;
        vec floorpos;
#else
        gameent *owner, *lastowner;
        projent *proj;
        int displaytime, movetime, viewtime, interptime;
        vec viewpos, interppos, render, above;
        //entitylight light, baselight;
#endif

        flag() { reset(); }

        void reset()
        {
            inertia = vec(0, 0, 0);
            droploc = spawnloc = vec(-1, -1, -1);
#ifdef GAMESERVER
            owner = lastowner = -1;
            lastownerteam = returntime = 0;
            votes.shrink(0);
            floorpos = vec(-1, -1, -1);
#else
            owner = lastowner = NULL;
            proj = NULL;
            displaytime = movetime = viewtime = interptime = 0;
#endif
            team = T_NEUTRAL;
            yaw = pitch = taketime = droptime = dropoffset = 0;
        }

#ifndef GAMESERVER
        vec &position()
        {
            if(owner) return owner->waist;
            if(droptime) return proj ? proj->o : droploc;
            return above;
        }

        vec &pos(bool view = false)
        {
            if(view)
            {
                if(interptime && lastmillis-interptime < 500)
                {
                    if(totalmillis != viewtime)
                    {
                        float amt = (lastmillis-interptime)/500.f;
                        viewpos = vec(interppos).add(vec(position()).sub(interppos).mul(amt));
                        viewtime = totalmillis;
                    }
                    return viewpos;
                }
            }
            return position();
        }

        void setposition(const vec &pos)
        {
            spawnloc = render = above = pos;
            render.z += 2;
            physics::droptofloor(render);
            if(render.z >= above.z-1) above.z += 1+render.z-above.z;
        }
#endif

        int dropleft(int t, bool b)
        {
            return (t-droptime)+(b ? dropoffset : 0);
        }
    };
    vector<flag> flags;

    void reset()
    {
        flags.shrink(0);
    }

    void addaffinity(const vec &o, int team, int yaw, int pitch)
    {
        flag &f = flags.add();
        f.reset();
        f.team = team;
        f.yaw = yaw;
        f.pitch = pitch;
#ifdef GAMESERVER
        f.spawnloc = o;
#else
        f.setposition(o);
#endif
    }

#ifndef GAMESERVER
    void interp(int i, int t)
    {
        flag &f = flags[i];
        f.displaytime = f.displaytime ? t-max(1000-(t-f.displaytime), 0) : t;
        f.interptime = t;
        f.interppos = f.position();
    }

    void destroy(int id)
    {
        flags[id].proj = NULL;
        loopv(projs::projs) if(projs::projs[i]->projtype == PRJ_AFFINITY && projs::projs[i]->id == id)
        {
            projs::projs[i]->state = CS_DEAD;
            projs::projs[i]->beenused = 2;
        }
    }

    void create(int id)
    {
        flag &f = flags[id];
        f.proj = projs::create(f.droploc, f.inertia, false, NULL, PRJ_AFFINITY, -1, HIT_NONE, capturedelay, capturedelay, 1, 1, id);
    }
#endif

#ifdef GAMESERVER
    void takeaffinity(int i, int owner, int t, int ownerteam)
#else
    void takeaffinity(int i, gameent *owner, int t)
#endif
    {
        flag &f = flags[i];
        if(f.droptime) f.dropoffset += t-f.droptime;
#ifndef GAMESERVER
        interp(i, t);
#endif
        f.owner = owner;
        f.taketime = t;
        f.droptime = 0;
#ifdef GAMESERVER
        f.votes.shrink(0);
        f.lastowner = owner;
        f.lastownerteam = ownerteam;
#else
        f.movetime = 0;
        (f.lastowner = owner)->addicon(eventicon::AFFINITY, t, game::eventiconfade, f.team);
        if(f.proj)
        {
            f.proj->beenused = 2;
            f.proj->lifetime = min(f.proj->lifetime, f.proj->fadetime);
        }
        destroy(i);
#endif
    }

    void dropaffinity(int i, const vec &o, const vec &p, int t, int offset = -1)
    {
        flag &f = flags[i];
        if(offset >= 0) f.dropoffset = offset;
#ifndef GAMESERVER
        interp(i, t);
#endif
        f.droploc = o;
        f.inertia = p;
        f.droptime = t;
        f.taketime = 0;
#ifdef GAMESERVER
        f.owner = -1;
        f.votes.shrink(0);
#else
        f.movetime = 0;
        f.owner = NULL;
        destroy(i);
        create(i);
#endif
    }

    void returnaffinity(int i, int t)
    {
        flag &f = flags[i];
#ifndef GAMESERVER
        interp(i, t);
#endif
        f.droptime = f.taketime = f.dropoffset = 0;
#ifdef GAMESERVER
        f.returntime = t;
        f.owner = -1;
        f.votes.shrink(0);
        f.floorpos = vec(-1, -1, -1);
#else
        f.movetime = 0;
        f.owner = NULL;
        destroy(i);
#endif
    }
};

#ifndef GAMESERVER
namespace capture
{
    extern capturestate st;
    extern int carryaffinity(gameent *d);
    extern bool dropaffinity(gameent *d);
    extern void sendaffinity(packetbuf &p);
    extern void parseaffinity(ucharbuf &p);
    extern void dropaffinity(gameent *d, int i, const vec &droploc, const vec &inertia, int offset = -1);
    extern void scoreaffinity(gameent *d, int relay, int goal, int score);
    extern void returnaffinity(gameent *d, int i);
    extern void takeaffinity(gameent *d, int i);
    extern void resetaffinity(int i, int value, const vec &pos);
    extern void reset();
    extern void setup();
    extern void setscore(int team, int total);
    extern void update();
    extern void drawnotices(int w, int h, int &tx, int &ty, int tr, int tg, int tb, float blend);
    extern void drawevents(int w, int h, int &tx, int &ty, int tr, int tg, int tb, float blend);
    extern void drawonscreen(int w, int h, float blend);
    extern void preload();
    extern void render();
    extern void adddynlights();
    extern bool aihomerun(gameent *d, ai::aistate &b);
    extern void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests);
    extern bool aicheck(gameent *d, ai::aistate &b);
    extern bool aidefense(gameent *d, ai::aistate &b);
    extern bool aipursue(gameent *d, ai::aistate &b);
    extern bool aicheckpos(gameent *d, ai::aistate &b);
    extern void removeplayer(gameent *d);
    extern void checkcams(vector<cament *> &cameras);
    extern void updatecam(cament *c);
}
#endif
