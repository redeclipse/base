#ifdef GAMESERVER
    #define defendstate stfservstate
    #define defendcount (m_dac_king(gamemode, mutators) ? G(defendking) : G(defendoccupy))
#else
    #define defendcount (m_dac_king(game::gamemode, game::mutators) ? G(defendking) : G(defendoccupy))
#endif

struct defendstate
{
    struct flag
    {
        vec o;
        int kinship, yaw, pitch, owner, enemy;
        string name;
#ifndef GAMESERVER
        string info;
        bool hasflag;
        int lasthad;
        vec render, above;
        entitylight baselight;
#endif
        int owners, enemies, converted, points;

        flag()
        {
            kinship = T_NEUTRAL;
            reset();
        }

        void noenemy()
        {
            enemy = T_NEUTRAL;
            enemies = 0;
            converted = 0;
        }

        void reset()
        {
            noenemy();
            owner = kinship;
            yaw = pitch = owners = points = 0;
#ifndef GAMESERVER
            hasflag = false;
            lasthad = 0;
#endif
        }

        bool enter(int team)
        {
            if(owner == team)
            {
                owners++;
                return false;
            }
            if(!enemies)
            {
                if(enemy != team)
                {
                    converted = 0;
                    enemy = team;
                }
                enemies++;
                return true;
            }
            else if(enemy != team) return false;
            else enemies++;
            return false;
        }

        bool steal(int team)
        {
            return !enemies && owner != team;
        }

        bool leave(int team)
        {
            if(owner == team)
            {
                owners--;
                return false;
            }
            if(enemy != team) return false;
            enemies--;
            return !enemies;
        }

        int occupy(int team, int units, int occupy, bool instant)
        {
            if(enemy != team) return -1;
            converted += units;

            if(units < 0)
            {
                if(converted <= 0) noenemy();
                return -1;
            }
            else if(converted < occupy) return -1;

            if(!instant && owner)
            {
                owner = T_NEUTRAL;
                converted = points = 0;
                enemy = team;
                return 0;
            }
            else
            {
                owner = team;
                points = 0;
                owners = enemies;
                noenemy();
                return 1;
            }
        }

        float occupied(bool instant, float amt)
        {
            return (enemy ? enemy : owner) ? (!owner || enemy ? clamp(converted/amt, 0.f, 1.f) : 1.f) : 0.f;
        }
    };

    vector<flag> flags;

    defendstate() { reset(); }

    void reset()
    {
        flags.shrink(0);
    }

    void addaffinity(const vec &o, int team, int yaw, int pitch, const char *name)
    {
        flag &b = flags.add();
        b.kinship = team;
        b.reset();
        b.o = o;
#ifndef GAMESERVER
        b.render = b.above = o;
        b.render.z += 2;
        physics::droptofloor(b.render);
        if(b.render.z >= b.above.z-1) b.above.z += (b.render.z-(b.above.z-1))+2;
#endif
        b.yaw = yaw;
        b.pitch = pitch;
        copystring(b.name, name);
    }

    void initaffinity(int i, int kin, int yaw, int pitch, vec &o, int owner, int enemy, int converted, const char *name)
    {
        if(!flags.inrange(i)) return;
        flag &b = flags[i];
        b.kinship = kin;
        b.reset();
        b.yaw = yaw;
        b.pitch = pitch;
        b.o = o;
#ifndef GAMESERVER
        b.render = b.above = o;
        b.render.z += 2;
        physics::droptofloor(b.render);
        if(b.render.z >= b.above.z-1) b.above.z += (b.render.z-(b.above.z-1))+2;
#endif
        b.owner = owner;
        b.enemy = enemy;
        b.converted = converted;
        copystring(b.name, name);
    }

    bool hasaffinity(int team)
    {
        loopv(flags)
        {
            flag &b = flags[i];
            if(b.owner && b.owner == team) return true;
        }
        return false;
    }

    float disttoenemy(flag &b)
    {
        float dist = 1e10f;
        loopv(flags)
        {
            flag &e = flags[i];
            if(e.owner && b.owner != e.owner)
                dist = min(dist, b.o.dist(e.o));
        }
        return dist;
    }

    bool insideaffinity(const flag &b, const vec &o, float radius = 0)
    {
        if(radius <= 0) radius = enttype[AFFINITY].radius;
        float dx = (b.o.x-o.x), dy = (b.o.y-o.y), dz = (b.o.z-o.z);
        return dx*dx + dy*dy <= radius*radius && fabs(dz) <= radius;
    }
};

#ifndef GAMESERVER
namespace defend
{
    extern defendstate st;
    extern void sendaffinity(packetbuf &p);
    extern void parseaffinity(ucharbuf &p);
    extern void updateaffinity(int i, int owner, int enemy, int converted);
    extern void setscore(int team, int total);
    extern void reset();
    extern void setup();
    extern void drawnotices(int w, int h, int &tx, int &ty, int tr, int tg, int tb, float blend);
    extern void drawevents(int w, int h, int &tx, int &ty, int tr, int tg, int tb, float blend);
    extern void drawblips(int w, int h, float blend);
    extern void preload();
    extern void render();
    extern void adddynlights();
    extern void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests);
    extern bool aicheck(gameent *d, ai::aistate &b);
    extern bool aidefense(gameent *d, ai::aistate &b);
    extern bool aipursue(gameent *d, ai::aistate &b);
    extern void removeplayer(gameent *d);
    extern void checkcams(vector<cament *> &cameras);
    extern void updatecam(cament *c);
}
#endif
