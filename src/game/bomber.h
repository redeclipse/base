#define isbomberaffinity(a) (a.enabled && a.team == T_NEUTRAL)
#define isbomberhome(a,b)   (a.enabled && !isbomberaffinity(a) && a.team != T_NEUTRAL && a.team == b)
#define isbombertarg(a,b)   (a.enabled && !isbomberaffinity(a) && a.team != T_NEUTRAL && a.team != b)

#ifdef GAMESERVER
#define carrytime (m_gsp1(gamemode, mutators) ? G(bomberholdtime) : G(bombercarrytime))
#define bomberstate bomberservstate
#else
#define carrytime (m_gsp1(game::gamemode, game::mutators) ? G(bomberholdtime) : G(bombercarrytime))
#endif
struct bomberstate
{
    struct flag
    {
        vec droploc, droppos, inertia, spawnloc;
        int team, yaw, pitch, droptime, taketime;
        bool enabled;
        float distance;
#ifdef GAMESERVER
        int owner, lastowner;
        vector<int> votes;
#else
        gameent *owner, *lastowner;
        projent *proj;
        int displaytime, pickuptime, movetime, inittime, viewtime, rendertime, interptime;
        vec viewpos, renderpos, interppos, render, above;
        entitylight light, baselight;
#endif

        flag() { reset(); }

        void reset()
        {
            inertia = vec(0, 0, 0);
            droploc = droppos = spawnloc = vec(-1, -1, -1);
#ifdef GAMESERVER
            owner = lastowner = -1;
            votes.shrink(0);
#else
            owner = lastowner = NULL;
            proj = NULL;
            displaytime = pickuptime = movetime = inittime = viewtime = rendertime = interptime = 0;
            viewpos = renderpos = vec(-1, -1, -1);
#endif
            team = T_NEUTRAL;
            yaw = pitch = taketime = droptime = 0;
            enabled = false;
            distance = 0;
        }

#ifndef GAMESERVER
        vec &position(bool render = false)
        {
            if(team == T_NEUTRAL)
            {
                if(owner)
                {
                    if(render)
                    {
                        if(totalmillis != rendertime)
                        {
                            float yaw = 360-((lastmillis/2)%360), off = (lastmillis%1000)/500.f;
                            renderpos = vec(yaw*RAD, 0.f).mul(owner->radius+4).add(owner->center());
                            renderpos.z += owner->height*(off > 1 ?  2-off : off);
                            rendertime = totalmillis;
                        }
                        return renderpos;
                    }
                    else return owner->waist;
                }
                if(droptime) return proj ? proj->o : droploc;
            }
            return above;
        }

        vec &pos(bool view = false, bool render = false)
        {
            if(team == T_NEUTRAL && view)
            {
                if(interptime && lastmillis-interptime < 500)
                {
                    if(totalmillis != viewtime)
                    {
                        float amt = (lastmillis-interptime)/500.f;
                        viewpos = vec(interppos).add(vec(position(render)).sub(interppos).mul(amt));
                        viewtime = totalmillis;
                    }
                    return viewpos;
                }
            }
            return position(render);
        }
#endif
        bool travel(const vec &o, float dist)
        {
            if(!droptime) return false;
            if(distance > 0 && distance >= dist) return true;
            return droppos.dist(o) >= dist;
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
        f.spawnloc = o;
        f.yaw = yaw;
        f.pitch = pitch;
#ifndef GAMESERVER
        f.render = f.above = o;
        f.render.z += 2;
        physics::droptofloor(f.render);
        if(f.render.z >= f.above.z-1) f.above.z += f.render.z-(f.above.z-1);
#endif
    }

#ifndef GAMESERVER
    void interp(int i, int t)
    {
        flag &f = flags[i];
        f.displaytime = f.displaytime ? t-max(1000-(t-f.displaytime), 0) : t;
        f.interptime = t;
        f.interppos = f.position(true);
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

    void create(int id, int target)
    {
        flag &f = flags[id];
        f.proj = projs::create(f.droploc, f.inertia, false, NULL, PRJ_AFFINITY, bomberresetdelay, bomberresetdelay, 1, 1, id, target);
    }
#endif

#ifdef GAMESERVER
    void takeaffinity(int i, int owner, int t)
#else
    void takeaffinity(int i, gameent *owner, int t)
#endif
    {
        flag &f = flags[i];
#ifndef GAMESERVER
        interp(i, t);
#endif
        f.owner = owner;
        f.taketime = t;
        f.droptime = 0;
#ifdef GAMESERVER
        f.votes.shrink(0);
        f.lastowner = owner;
#else
        f.pickuptime = f.movetime = 0;
        if(!f.inittime) f.inittime = t;
        owner->addicon(eventicon::AFFINITY, t, game::eventiconfade, f.team);
        f.lastowner = owner;
        destroy(i);
#endif
    }

    void dropaffinity(int i, const vec &o, const vec &p, int t, int target = -1)
    {
        flag &f = flags[i];
#ifndef GAMESERVER
        interp(i, t);
#endif
        f.droploc = f.droppos = o;
        f.inertia = p;
        f.droptime = t;
        f.taketime = 0;
        f.distance = 0;
#ifdef GAMESERVER
        f.owner = -1;
        f.votes.shrink(0);
#else
        f.pickuptime = f.movetime = 0;
        if(!f.inittime) f.inittime = t;
        f.owner = NULL;
        destroy(i);
        create(i, target);
#endif
    }

    void returnaffinity(int i, int t, bool enabled)
    {
        flag &f = flags[i];
#ifndef GAMESERVER
        interp(i, t);
#endif
        f.droptime = f.taketime = 0;
        f.enabled = enabled;
#ifdef GAMESERVER
        f.owner = -1;
        f.votes.shrink(0);
#else
        f.pickuptime = f.inittime = f.movetime = 0;
        f.owner = NULL;
        destroy(i);
#endif
    }
};

#ifndef GAMESERVER
namespace bomber
{
    extern bomberstate st;
    extern bool carryaffinity(gameent *d);
    extern bool dropaffinity(gameent *d);
    extern void sendaffinity(packetbuf &p);
    extern void parseaffinity(ucharbuf &p);
    extern void dropaffinity(gameent *d, int i, const vec &droploc, const vec &inertia, int target = -1);
    extern void scoreaffinity(gameent *d, int relay, int goal, int score);
    extern void takeaffinity(gameent *d, int i);
    extern void resetaffinity(int i, int enabled);
    extern void reset();
    extern void setup();
    extern void setscore(int team, int total);
    extern void update();
    extern void killed(gameent *d, gameent *v);
    extern void drawnotices(int w, int h, int &tx, int &ty, float blend);
    extern void drawblips(int w, int h, float blend);
    extern int drawinventory(int x, int y, int s, int m, float blend);
    extern void preload();
    extern void render();
    extern void adddynlights();
    extern int aiowner(gameent *d);
    extern bool aihomerun(gameent *d, ai::aistate &b);
    extern void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests);
    extern bool aicheck(gameent *d, ai::aistate &b);
    extern bool aidefense(gameent *d, ai::aistate &b);
    extern bool aipursue(gameent *d, ai::aistate &b);
    extern void removeplayer(gameent *d);
    extern vec pulsecolour();
    extern void checkcams(vector<cament *> &cameras);
    extern void updatecam(cament *c);
}
#endif
