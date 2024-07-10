#ifdef CPP_GAME_SERVER
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
        int ent, kinship, yaw, pitch, owner, enemy;
        string name;
#ifndef CPP_GAME_SERVER
        bool hasflag;
        int lasthad;
        vec render;
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
#ifndef CPP_GAME_SERVER
            hasflag = false;
            lasthad = 0;
#endif
            ent = -1;
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
#ifndef CPP_GAME_SERVER
        void setposition(const vec &pos)
        {
            o = render = pos;
            render.z += 4;
            physics::droptofloor(render);
            render.z -= 1.5f;
            float offset = o.z-render.z;
            if(offset < 0)
            {
                o = render;
                offset = 0;
            }
            if(offset < 4) o.z += 4-offset;
        }
#endif
    };

    vector<flag> flags;

    defendstate() { reset(); }

    void reset()
    {
        flags.shrink(0);
    }

    void addaffinity(int n, const vec &o, int team, int yaw, int pitch, const char *name)
    {
        flag &b = flags.add();
        b.kinship = team;
        b.reset();
        b.ent = n;
        b.yaw = yaw;
        b.pitch = pitch;
#ifdef CPP_GAME_SERVER
        b.o = o;
#else
        b.setposition(o);
#endif
        copystring(b.name, name);
    }

    void initaffinity(int i, int n, int kin, int yaw, int pitch, vec &o, int owner, int enemy, int converted, const char *name)
    {
        if(!flags.inrange(i)) return;
        flag &b = flags[i];
        b.kinship = kin;
        b.reset();
        b.ent = n;
        b.yaw = yaw;
        b.pitch = pitch;
#ifdef CPP_GAME_SERVER
        b.o = o;
#else
        b.setposition(o);
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

#ifndef CPP_GAME_SERVER
namespace defend
{
    extern defendstate st;
    extern bool haloallow(const vec &o, int id, int render = 0, bool justtest = false);
    extern int hasaffinity(gameent *d);
    extern void sendaffinity(packetbuf &p);
    extern void parseaffinity(ucharbuf &p);
    extern void updateaffinity(int i, int owner, int owners, int enemy, int enemies, int converted, int points);
    extern void setscore(int team, int total);
    extern void reset();
    extern void setup();
    extern void preload();
    extern void render();
    extern void adddynlights();
    extern void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests);
    extern bool aicheck(gameent *d, ai::aistate &b);
    extern bool aidefense(gameent *d, ai::aistate &b);
    extern bool aipursue(gameent *d, ai::aistate &b);
    extern bool aicheckpos(gameent *d, ai::aistate &b);
    extern void removeplayer(gameent *d);
    extern void checkcams(vector<cament *> &cameras);
    extern void updatecam(cament *c);
    extern void checkui();
}
#endif
