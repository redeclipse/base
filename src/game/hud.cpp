#include "game.h"
namespace hud
{
    int damageresidue = 0, hudwidth = 0, hudheight = 0, lastteam = 0, laststats = 0;

    #include "compass.h"
    vector<int> teamkills;

    struct dhloc
    {
        int clientnum, outtime, damage, colour; vec dir;
        dhloc(int a, int t, int d, const vec &p, int c) : clientnum(a), outtime(t), damage(d), colour(c), dir(p) {}
    };
    vector<dhloc> damagelocs, hitlocs;
    VAR(IDF_PERSIST, damageresiduefade, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, damageresiduemax, 1, 2000, VAR_MAX);
    VAR(IDF_PERSIST, damageresiduemul, 1, 3, VAR_MAX);
    VAR(IDF_PERSIST, damageresiduemulresidual, 1, 5, VAR_MAX);

    ICOMMAND(0, conout, "is", (int *n, char *s), conoutft(clamp(*n, 0, CON_MAX-1), "%s", s));

    struct event
    {
        struct client
        {
            int cn, type, team, colour, model, priv, weap, health;
            char *name;

            client() : cn(-1), type(-1), team(-1), colour(-1), model(0), priv(0), weap(0), health(0), name(NULL) {}
            ~client() { DELETEA(name); }
        };

        int type, subtype, sndidx, sndflags, millis;
        vector<client> clients;
        vector<int> infos;
        char *str;

        event() : type(-1), subtype(-1), sndidx(-1), sndflags(0), millis(-1), str(NULL) {}
        ~event()
        {
            clients.shrink(0);
            infos.shrink(0);
            DELETEA(str);
        }
    };

    vector<event *> events;
    VAR(IDF_PERSIST, eventmaxlines, 1, 50, VAR_MAX);

    void eventlogv(int type, int subtype, int sndidx, int sndflags, const vector<int> &clients, const vector<int> &infos, const char *str)
    {
        if(type < 0 || type >= EV_MAX) return;
        if(events.length() >= eventmaxlines)
        {
            event *e = events[0];
            events.remove(0);
            delete e;
        }
        event *e = new event;
        e->type = type;
        e->subtype = subtype;
        e->sndidx = sndidx;
        e->sndflags = sndflags;
        e->millis = totalmillis;
        loopv(clients)
        {
            gameent *d = game::getclient(clients[i]);
            event::client &c = e->clients.add();
            if(!d) continue;
            c.cn = d->clientnum;
            c.type = d->actortype;
            c.team = d->team;
            c.colour = d->colour;
            c.model = d->model;
            c.priv = d->privilege;
            c.weap = d->weapselect;
            c.health = d->health;
            if(*d->name) c.name = newstring(d->name);
            if(e->sndflags&(i == 0 ? EV_S_CLIENT1 : (i == 1 ? EV_S_CLIENT2 : EV_S_CLIENTN)))
                entities::announce(e->sndidx, d);
        }
        loopv(infos) e->infos.add(infos[i]);
        if(str && *str)
        {
            e->str = newstring(str);
            conoutft(CON_EVENT, "%s", str);
        }
        if(e->sndidx >= 0 && e->sndflags&EV_S_BROADCAST) entities::announce(e->sndidx);
        events.add(e);
    }

    void eventlogvf(int type, int subtype, int sndidx, int sndflags, const vector<int> &clients, const vector<int> &infos, const char *str, ...)
    {
        if(type < 0 || type >= EV_MAX) return;
        defvformatbigstring(sf, str, str);
        eventlogv(type, subtype, sndidx, sndflags, clients, infos, sf);
    }

    static vector<int> eventclients, eventinfos;
    void eventlogvi(int type, int subtype, int sndidx, int sndflags, const vector<int> &clients, int *infos, int ilen, const char *str)
    {
        if(type < 0 || type >= EV_MAX) return;
        eventinfos.setsize(0);
        if(infos) loopi(ilen) eventinfos.add(infos[i]);
        eventlogv(type, subtype, sndidx, sndflags, clients, eventinfos, str);
    }

    void eventlogiv(int type, int subtype, int sndidx, int sndflags, int *clients, int clen, const vector<int> &infos, const char *str)
    {
        if(type < 0 || type >= EV_MAX) return;
        eventclients.setsize(0);
        if(clients) loopi(clen) eventclients.add(clients[i]);
        eventlogv(type, subtype, sndidx, sndflags, eventclients, infos, str);
    }

    void eventlogvif(int type, int subtype, int sndidx, int sndflags, const vector<int> &clients, int *infos, int ilen, const char *str, ...)
    {
        if(type < 0 || type >= EV_MAX) return;
        defvformatbigstring(sf, str, str);
        eventlogvi(type, subtype, sndidx, sndflags, clients, infos, ilen, sf);
    }

    void eventlogi(int type, int subtype, int sndidx, int sndflags, int *clients, int clen, int *infos, int ilen, const char *str)
    {
        if(type < 0 || type >= EV_MAX) return;
        eventclients.setsize(0);
        if(clients) loopi(clen) eventclients.add(clients[i]);
        eventinfos.setsize(0);
        if(infos) loopi(ilen) eventinfos.add(infos[i]);
        eventlogv(type, subtype, sndidx, sndflags, eventclients, eventinfos, str);
    }

    void eventlogif(int type, int subtype, int sndidx, int sndflags, int *clients, int clen, int *infos, int ilen, const char *str, ...)
    {
        if(type < 0 || type >= EV_MAX) return;
        defvformatbigstring(sf, str, str);
        eventlogi(type, subtype, sndidx, sndflags, clients, clen, infos, ilen, sf);
    }

    #define LOOPEVENTS(name,op) \
        ICOMMAND(0, loopevents##name, "iire", (int *count, int *skip, ident *id, uint *body), \
        { \
            loopstart(id, stack); \
            op(events, *count, *skip, \
            { \
                loopiter(id, stack, i); \
                execute(body); \
            }); \
            loopend(id, stack); \
        });

    LOOPEVENTS(,loopcsv);
    LOOPEVENTS(rev,loopcsvrev);

    ICOMMANDV(0, eventcount, events.length());
    ICOMMAND(0, geteventtype, "i", (int *n), intret(events.inrange(*n) ? events[*n]->type : -1));
    ICOMMAND(0, geteventsubtype, "i", (int *n), intret(events.inrange(*n) ? events[*n]->subtype : -1));
    ICOMMAND(0, geteventsndidx, "i", (int *n), intret(events.inrange(*n) ? events[*n]->sndidx : -1));
    ICOMMAND(0, geteventsndflags, "i", (int *n), intret(events.inrange(*n) ? events[*n]->sndflags : -1));
    ICOMMAND(0, geteventmillis, "i", (int *n), intret(events.inrange(*n) ? events[*n]->millis : -1));
    ICOMMAND(0, geteventstr, "i", (int *n), result(events.inrange(*n) ? events[*n]->str : ""));

    #define EV_P_ENUM(pr, en) \
        en(pr, CN) en(pr, TYPE) en(pr, TEAM) en(pr, COLOUR) en(pr, MODEL) \
        en(pr, PRIV) en(pr, WEAP) en(pr, HEALTH) en(pr, NAME) en(pr, MAX)
    ENUMLV(EV_P, EV_P_ENUM);

    ICOMMAND(0, geteventclient, "ibb", (int *n, int *pos, int *prop),
    {
        if(*n < 0) intret(events.length());
        else if(events.inrange(*n))
        {
            if(*pos < 0) intret(events[*n]->clients.length());
            else if(events[*n]->clients.inrange(*pos))
            {
                if(*prop < 0) intret(EV_P_MAX);
                else switch(*prop)
                {
                    case EV_P_CN: intret(events[*n]->clients[*pos].cn); break;
                    case EV_P_TYPE: intret(events[*n]->clients[*pos].type); break;
                    case EV_P_TEAM: intret(events[*n]->clients[*pos].team); break;
                    case EV_P_COLOUR: intret(events[*n]->clients[*pos].colour); break;
                    case EV_P_MODEL: intret(events[*n]->clients[*pos].model); break;
                    case EV_P_PRIV: intret(events[*n]->clients[*pos].priv); break;
                    case EV_P_WEAP: intret(events[*n]->clients[*pos].weap); break;
                    case EV_P_HEALTH: intret(events[*n]->clients[*pos].health); break;
                    case EV_P_NAME: result(events[*n]->clients[*pos].name ? events[*n]->clients[*pos].name : ""); break;
                    default: break;
                }
            }
        }
    });
    ICOMMAND(0, geteventname, "iibbb", (int *n, int *pos, int *colour, int *icon, int *dupname),
    {
        if(events.inrange(*n) && events[*n]->clients.inrange(*pos) && events[*n]->clients[*pos].name)
        {
            event::client &c = events[*n]->clients[*pos];
            result(game::colourname(c.name, c.cn, c.team, c.type, c.colour, c.priv, c.weap, *icon != 0, *dupname != 0, *colour >= 0 ? *colour : 3));
        }
    });

    ICOMMAND(0, geteventinfo, "ib", (int *n, int *pos),
    {
        if(*n < 0) intret(events.length());
        else if(events.inrange(*n))
        {
            if(*pos < 0) intret(events[*n]->infos.length());
            else if(events[*n]->infos.inrange(*pos)) intret(events[*n]->infos[*pos]);
        }
    });

    VAR(IDF_PERSIST, showhud, 0, 1, 1);
    VAR(IDF_PERSIST, hudsize, 0, 2048, VAR_MAX);
    FVAR(IDF_PERSIST, hudblend, 0, 1, 1);

    VAR(IDF_PERSIST, showdemoplayback, 0, 1, 1);
    FVAR(IDF_PERSIST, edgesize, 0, 0.005f, 1000);

    VAR(IDF_PERSIST, showeventicons, 0, 1, 7);
    VAR(IDF_PERSIST, showloadingaspect, 0, 1, 1);
    VAR(IDF_PERSIST, showloadingmapbg, 0, 1, 1);
    VAR(IDF_PERSIST, showloadinglogos, 0, 1, 1);

    const int NUMSTATS = 42;
    int prevstats[NUMSTATS] = {0}, curstats[NUMSTATS] = {0};
    VAR(IDF_PERSIST, statrate, 1, 100, 1000);

    void enginestatrefresh()
    {
        if(totalmillis-laststats >= statrate)
        {
            memcpy(prevstats, curstats, sizeof(prevstats));
            laststats = totalmillis-(totalmillis%statrate);
        }
        int nextstats[NUMSTATS] = {
            wtris/1024, vtris*100/max(wtris, 1), wverts/1024, vverts*100/max(wverts, 1), xtraverts/1024, xtravertsva/1024,
            allocnodes*8, allocva, glde, gbatches, getnumqueries(),
            curfps, bestfpsdiff, worstfpsdiff, entities::ents.length(), entgroup.length(), ai::waypoints.length(), getnumviewcells(),
            int(vec(game::focus->vel).add(game::focus->falling).magnitude()),
            int(vec(game::focus->vel).add(game::focus->falling).magnitude()/8.f),
            int(vec(game::focus->vel).add(game::focus->falling).magnitude()*0.45f),
            int(camera1->o.x), int(camera1->o.y), int(camera1->o.z), int(camera1->yaw), int(camera1->pitch), int(camera1->roll),
            sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.cx, sel.cxs, sel.cy, sel.cys,
            selchildcount, selchildmat[0], sel.corner, sel.orient, sel.grid
        };
        loopi(NUMSTATS) if(prevstats[i] == curstats[i]) curstats[i] = nextstats[i];
    }
    ICOMMAND(0, refreshenginestats, "", (), enginestatrefresh());
    ICOMMAND(0, getenginestat, "ii", (int *n, int *prev), intret(*n >= 0 && *n < NUMSTATS ? (*prev!=0 ? prevstats[*n] : curstats[*n]) : -1));
    static const char *enginestats[NUMSTATS] = {
        "wtr", "wtr%", "wvt", "wvt%", "evt", "eva", "ond", "va", "gl" "gb", "oq",
        "fps", "best", "worst", "ents", "entsel", "wp", "pvs", "vel", "mps", "kmh",
        "posx", "posy", "posz", "yaw", "pitch", "roll",
        "selox", "seloy", "seloz", "selsx", "selsy", "selsz", "selcx", "selcxs", "selcy", "selcys",
        "cube", "mat", "corner", "orient", "grid"
    };
    ICOMMAND(0, getenginestatname, "i", (int *n), result(*n >= 0 && *n < NUMSTATS ? enginestats[*n]: ""));
    #define LOOPENGSTATS(name,op) \
        ICOMMAND(0, loopenginestat##name, "iire", (int *count, int *skip, ident *id, uint *body), \
        { \
            loopstart(id, stack); \
            op(NUMSTATS, *count, *skip, \
            { \
                loopiter(id, stack, i); \
                execute(body); \
            }); \
            loopend(id, stack); \
        });
    LOOPENGSTATS(,loopcsi)
    LOOPENGSTATS(rev,loopcsirev)

    VAR(IDF_PERSIST, titlefade, 0, 1000, 10000);
    VAR(IDF_PERSIST, tvmodefade, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, spawnfade, 0, 250, VAR_MAX);

    FVAR(IDF_PERSIST, eventiconoffset, -1, 0.58f, 1);
    FVAR(IDF_PERSIST, eventiconblend, 0, 1, 1);
    FVAR(IDF_PERSIST, eventiconscale, 1e-4f, 2.5f, 1000);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamneutraltex, "<grey>textures/icons/teamneutral", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamalphatex, "<grey>textures/icons/teamalpha", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamomegatex, "<grey>textures/icons/teamomega", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamenemytex, "<grey>textures/icons/teamenemy", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, playertex, "<grey>textures/icons/player", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, playermaletex, "<grey>textures/icons/playermale", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, playerfemaletex, "<grey>textures/icons/playerfemale", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, outlinemaletex, "<grey>textures/icons/outlinemale", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, outlinefemaletex, "<grey>textures/icons/outlinefemale", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, deadtex, "<grey>textures/icons/dead", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, dominatingtex, "<grey>textures/icons/dominating", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, dominatedtex, "<grey>textures/icons/dominated", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, inputtex, "<grey><rotate:1>textures/icons/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, menutex, "<grey>textures/icons/menu", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, waitingtex, "<grey>textures/icons/waiting", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, spectatortex, "<grey>textures/icons/spectator", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, cameratex, "<grey>textures/icons/camera", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, chattex, "<grey>textures/icons/chat", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, editingtex, "<grey>textures/icons/editing", 3);

    TVAR(IDF_PERSIST|IDF_PRELOAD, progresstex, "<grey>textures/hud/progress", 3);
    TVAR(IDF_PERSIST|IDF_PRELOAD, progringtex, "<grey>textures/hud/progring", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, warningtex, "<grey>textures/icons/warning", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, questiontex, "<grey>textures/icons/question", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flagtex, "<grey>textures/icons/flag", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, pointtex, "<grey>textures/icons/point", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bombtex, "<grey>textures/icons/bomb", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, arrowtex, "<grey>textures/icons/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, arrowrighttex, "<grey><rotate:1>textures/icons/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, arrowdowntex, "<grey><rotate:2>textures/icons/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, arrowlefttex, "<grey><rotate:3>textures/icons/arrow", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, alerttex, "<grey>textures/icons/alert", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flagdroptex, "<grey>textures/icons/flagdrop", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flagtakentex, "<grey>textures/icons/flagtaken", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bombdroptex, "<grey>textures/icons/bombdrop", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, bombtakentex, "<grey>textures/icons/bombtaken", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, attacktex, "<grey>textures/icons/attack", 3);

    VAR(IDF_PERSIST|IDF_HEX, crosshairtone, -CTONE_MAX, 0, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, hitcrosshairtone, -CTONE_MAX, 0, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, clipstone, -CTONE_MAX, 0, 0xFFFFFF);

    VAR(IDF_PERSIST, teamhurthud, 0, 1, 3); // 0 = off, 1 = full body particle, 2 = fixed position and size
    VAR(IDF_PERSIST, teamhurttime, 0, 2500, VAR_MAX);
    VAR(IDF_PERSIST, teamhurtdist, 0, 0, VAR_MAX);
    FVAR(IDF_PERSIST, teamhurtsize, 0, 0.0175f, 1000);

    VAR(IDF_PERSIST, showindicator, 0, 4, 4);
    FVAR(IDF_PERSIST, indicatorsize, 0, 0.03f, 1000);
    FVAR(IDF_PERSIST, indicatorblend, 0, 1, 1);
    VAR(IDF_PERSIST, indicatorminattack, 0, 1000, VAR_MAX);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, indicatortex, "<grey>textures/hud/indicator", 3);

    VAR(0, hidecrosshair, 0, 0, 1); // temporarily hides crosshair, needs to be set each frame you want it hidden
    VAR(IDF_PERSIST, showcrosshair, 0, 2, 2); // 0 = off, 1 = on, 2 = blend depending on current accuracy level
    VAR(IDF_PERSIST, crosshairdistance, 0, 0, 1); // 0 = off, 1 = shows distance to crosshair target
    VAR(IDF_PERSIST, crosshairdistancex, VAR_MIN, 160, VAR_MAX); // offset from the crosshair
    VAR(IDF_PERSIST, crosshairdistancey, VAR_MIN, 80, VAR_MAX); // offset from the crosshair
    VAR(IDF_PERSIST, crosshairweapons, 0, 1, 3); // 0 = off, &1 = crosshair-specific weapons, &2 = also appy colour
    FVAR(IDF_PERSIST, crosshairsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, crosshairscale, 0, 1.0f, 4.0f);
    VAR(IDF_PERSIST, crosshairhitspeed, 0, 500, VAR_MAX);
    FVAR(IDF_PERSIST, crosshairblend, 0, 1, 1);
    FVAR(IDF_PERSIST, crosshairaccamt, 0, 0, 1);
    VAR(IDF_PERSIST, crosshairflash, 0, 1, 1);
    FVAR(IDF_PERSIST, crosshairthrob, 0, 0, 1000);
    TVAR(IDF_PERSIST|IDF_PRELOAD, cursortex, "textures/hud/cursor", 3);
    TVAR(IDF_PERSIST|IDF_PRELOAD, cursorhovertex, "textures/hud/cursorhover", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, crosshairtex, "crosshairs/cross-01", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, hithairtex, "crosshairs/cross-01-hit", 3);
    FVAR(IDF_PERSIST, clawcrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, clawcrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, clawcrosshairtex, "crosshairs/triangle-02", 3);
    TVAR(IDF_PERSIST, clawhithairtex, "crosshairs/triangle-02-hit", 3);
    FVAR(IDF_PERSIST, pistolcrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, pistolcrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, pistolcrosshairtex, "crosshairs/simple-04", 3);
    TVAR(IDF_PERSIST, pistolhithairtex, "crosshairs/simple-04-hit", 3);
    FVAR(IDF_PERSIST, swordcrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, swordcrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, swordcrosshairtex, "crosshairs/simple-02", 3);
    TVAR(IDF_PERSIST, swordhithairtex, "crosshairs/simple-02-hit", 3);
    FVAR(IDF_PERSIST, shotguncrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, shotguncrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, shotguncrosshairtex, "crosshairs/cross-02", 3);
    TVAR(IDF_PERSIST, shotgunhithairtex, "crosshairs/cross-02-hit", 3);
    FVAR(IDF_PERSIST, smgcrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, smgcrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, smgcrosshairtex, "crosshairs/simple-03", 3);
    TVAR(IDF_PERSIST, smghithairtex, "crosshairs/simple-03-hit", 3);
    FVAR(IDF_PERSIST, plasmacrosshairsize, 0, 0.06f, 1000);
    FVAR(IDF_PERSIST, plasmacrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, plasmacrosshairtex, "crosshairs/circle-05", 3);
    TVAR(IDF_PERSIST, plasmahithairtex, "crosshairs/circle-05-hit", 3);
    FVAR(IDF_PERSIST, zappercrosshairsize, 0, 0.06f, 1000);
    FVAR(IDF_PERSIST, zappercrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, zappercrosshairtex, "crosshairs/circle-03", 3);
    TVAR(IDF_PERSIST, zapperhithairtex, "crosshairs/circle-03-hit", 3);
    FVAR(IDF_PERSIST, flamercrosshairsize, 0, 0.06f, 1000);
    FVAR(IDF_PERSIST, flamercrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, flamercrosshairtex, "crosshairs/circle-06", 3);
    TVAR(IDF_PERSIST, flamerhithairtex, "crosshairs/circle-06-hit", 3);
    FVAR(IDF_PERSIST, riflecrosshairsize, 0, 0.075f, 1000);
    FVAR(IDF_PERSIST, riflecrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, riflecrosshairtex, "crosshairs/simple-01", 3);
    TVAR(IDF_PERSIST, riflehithairtex, "crosshairs/simple-01-hit", 3);
    FVAR(IDF_PERSIST, grenadecrosshairsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, grenadecrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, grenadecrosshairtex, "crosshairs/circle-02", 3);
    TVAR(IDF_PERSIST, grenadehithairtex, "crosshairs/circle-02-hit", 3);
    FVAR(IDF_PERSIST, minecrosshairsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, minecrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, minecrosshairtex, "crosshairs/circle-02", 3);
    TVAR(IDF_PERSIST, minehithairtex, "crosshairs/circle-02-hit", 3);
    FVAR(IDF_PERSIST, rocketcrosshairsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, rocketcrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, rocketcrosshairtex, "crosshairs/circle-01", 3);
    TVAR(IDF_PERSIST, rockethithairtex, "crosshairs/circle-01-hit", 3);
    FVAR(IDF_PERSIST, meleecrosshairsize, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, meleecrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST, meleecrosshairtex, "crosshairs/triangle-02", 3);
    TVAR(IDF_PERSIST, meleehithairtex, "crosshairs/triangle-02-hit", 3);

    FVAR(IDF_PERSIST, editcursorsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, editcursorblend, 0, 1, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, editcursortex, "crosshairs/cross-01", 3);
    FVAR(IDF_PERSIST, speccursorsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, speccursorblend, 0, 1, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, speccursortex, "crosshairs/cross-01", 3);
    FVAR(IDF_PERSIST, tvcursorsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, tvcursorblend, 0, 1, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, tvcursortex, "", 3);
    FVAR(IDF_PERSIST, teamcrosshairsize, 0, 0.05f, 1000);
    FVAR(IDF_PERSIST, teamcrosshairblend, 0, 1, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, teamcrosshairtex, "textures/icons/warning", 3);
    VAR(IDF_PERSIST, teamcrosshaircolour, 0, 0xFF0000, 0xFFFFFF);

    VAR(IDF_PERSIST, cursorstyle, 0, 0, 1); // 0 = top left tracking, 1 = center
    FVAR(IDF_PERSIST, cursorsize, 0, 0.03f, 1000);
    FVAR(IDF_PERSIST, cursorblend, 0, 1, 1);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zoomtex, "textures/hud/zoom", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zoomcrosshairtex, "crosshairs/simple-01", 3);
    FVAR(IDF_PERSIST, zoomcrosshairsize, 0, 0.04f, 1000);
    FVAR(IDF_PERSIST, zoomcrosshairblend, 0, 1, 1);

    VAR(IDF_PERSIST, showcirclebar, 0, 0, 1);
    VAR(IDF_PERSIST, circlebartype, 0, 7, 7); // 0 = off, &1 = health, &2 = impulse, &4 = ammo
    FVAR(IDF_PERSIST, circlebarsize, 0, 0.04f, 1000);
    FVAR(IDF_PERSIST, circlebarblend, 0, 1, 1);
    VAR(IDF_PERSIST|IDF_HEX, circlebarhealthtone, -CTONE_MAX, 0x88FF88, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, circlebarimpulsetone, -CTONE_MAX, 0xFF88FF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, circlebarammocolour, 0, 1, 1);
    VAR(IDF_PERSIST|IDF_HEX, circlebarammotone, -CTONE_MAX-1, 0xFFAA66, 0xFFFFFF);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, circlebartex, "textures/hud/circlebar", 3);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, clawtex, "<grey>textures/weapons/claw", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, pistoltex, "<grey>textures/weapons/pistol", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, swordtex, "<grey>textures/weapons/sword", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, shotguntex, "<grey>textures/weapons/shotgun", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, smgtex, "<grey>textures/weapons/smg", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flamertex, "<grey>textures/weapons/flamer", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, plasmatex, "<grey>textures/weapons/plasma", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zappertex, "<grey>textures/weapons/zapper", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, rifletex, "<grey>textures/weapons/rifle", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, grenadetex, "<grey>textures/weapons/grenade", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, minetex, "<grey>textures/weapons/mine", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, rockettex, "<grey>textures/weapons/rocket", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, meleetex, "<grey>textures/weapons/melee", 3);

    VAR(IDF_PERSIST, showclips, 0, 1, 1);
    VAR(IDF_PERSIST, clipanims, 0, 2, 2);
    FVAR(IDF_PERSIST, clipsize, 0, 0.03f, 1000);
    FVAR(IDF_PERSIST, clipoffset, 0, 0.045f, 1000);
    FVAR(IDF_PERSIST, clipminscale, 0, 0.3f, 1000);
    FVAR(IDF_PERSIST, clipmaxscale, 0, 1, 1000);
    FVAR(IDF_PERSIST, clipblend, 0, 1, 1);
    FVAR(IDF_PERSIST, clipcolour, 0, 1, 1);
    VAR(IDF_PERSIST, cliplength, 0, 0, VAR_MAX);
    VAR(IDF_PERSIST, clipstore, 0, 1, 1);
    FVAR(IDF_PERSIST, clipstoreblend, 0, 0.2f, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, clawcliptex, "<grey>textures/weapons/clips/claw", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, pistolcliptex, "<grey>textures/weapons/clips/pistol", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, swordcliptex, "<grey>textures/weapons/clips/sword", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, shotguncliptex, "<grey>textures/weapons/clips/shotgun", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, smgcliptex, "<grey>textures/weapons/clips/smg", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, flamercliptex, "<grey>textures/weapons/clips/flamer", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, plasmacliptex, "<grey>textures/weapons/clips/plasma", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, zappercliptex, "<grey>textures/weapons/clips/zapper", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, riflecliptex, "<grey>textures/weapons/clips/rifle", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, grenadecliptex, "<grey>textures/weapons/clips/grenade", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, minecliptex, "<grey>textures/weapons/clips/mine", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, rocketcliptex, "<grey>textures/weapons/clips/rocket", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, meleecliptex, "<grey>textures/weapons/clips/melee", 3);
    FVAR(IDF_PERSIST, clawclipoffset, 0, 0.25f, 0.5f);
    FVAR(IDF_PERSIST, pistolclipoffset, 0, 0.1f, 0.5f);
    FVAR(IDF_PERSIST, swordclipoffset, 0, 0.25f, 0.5f);
    FVAR(IDF_PERSIST, shotgunclipoffset, 0, 0.125f, 0.5f);
    FVAR(IDF_PERSIST, smgclipoffset, 0, 0.35f, 0.5f);
    FVAR(IDF_PERSIST, flamerclipoffset, 0, 0.5f, 0.5f);
    FVAR(IDF_PERSIST, plasmaclipoffset, 0, 0.1f, 0.5f);
    FVAR(IDF_PERSIST, zapperclipoffset, 0, 0.3f, 0.5f);
    FVAR(IDF_PERSIST, rifleclipoffset, 0, 0.25f, 0.5f);
    FVAR(IDF_PERSIST, grenadeclipoffset, 0, 0, 0.5f);
    FVAR(IDF_PERSIST, mineclipoffset, 0, 0, 0.5f);
    FVAR(IDF_PERSIST, rocketclipoffset, 0, 0, 0.5f);
    FVAR(IDF_PERSIST, meleeclipoffset, 0, 0.25f, 0.5f);
    FVAR(IDF_PERSIST, clawclipskew, 0, 0.75f, 10);
    FVAR(IDF_PERSIST, pistolclipskew, 0, 0.65f, 10);
    FVAR(IDF_PERSIST, swordclipskew, 0, 1, 10);
    FVAR(IDF_PERSIST, shotgunclipskew, 0, 0.7f, 10);
    FVAR(IDF_PERSIST, smgclipskew, 0, 0.55f, 10);
    FVAR(IDF_PERSIST, flamerclipskew, 0, 0.4f, 10);
    FVAR(IDF_PERSIST, plasmaclipskew, 0, 0.5f, 10);
    FVAR(IDF_PERSIST, zapperclipskew, 0, 0.5f, 10);
    FVAR(IDF_PERSIST, rifleclipskew, 0, 0.9f, 10);
    FVAR(IDF_PERSIST, grenadeclipskew, 0, 1.f, 10);
    FVAR(IDF_PERSIST, mineclipskew, 0, 1.f, 10);
    FVAR(IDF_PERSIST, rocketclipskew, 0, 1.25f, 10);
    FVAR(IDF_PERSIST, meleeclipskew, 0, 0.75f, 10);
    VAR(IDF_PERSIST, clawcliprotate, 0, 12, 7); // "round-the-clock" rotation of texture, 0 = off, &1 = flip x, &2 = flip y, &4 = angle, &8 = spin
    VAR(IDF_PERSIST, pistolcliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, swordcliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, shotguncliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, smgcliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, flamercliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, plasmacliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, zappercliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, riflecliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, grenadecliprotate, 0, 11, 15);
    VAR(IDF_PERSIST, minecliprotate, 0, 11, 15);
    VAR(IDF_PERSIST, rocketcliprotate, 0, 12, 15);
    VAR(IDF_PERSIST, meleecliprotate, 0, 12, 7);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, hurttex, "<grey>textures/hud/hurt", 3);
    VAR(IDF_PERSIST, onscreendamage, 0, 1, 2); // 0 = off, 1 = basic damage, 2 = verbose
    FVAR(IDF_PERSIST, onscreendamagescale, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, onscreendamageblipsize, 0, 0.1f, 1);
    FVAR(IDF_PERSIST, onscreendamageoffset, 0, 0.4f, 1);
    VAR(IDF_PERSIST, onscreendamageself, 0, 1, 1);
    VAR(IDF_PERSIST, onscreendamagemerge, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, onscreendamagetime, 1, 250, VAR_MAX);
    VAR(IDF_PERSIST, onscreendamagefade, 1, 3500, VAR_MAX);
    FVAR(IDF_PERSIST, onscreendamagesize, 0, 20, 1000);
    FVAR(IDF_PERSIST, onscreendamageblend, 0, 0.75f, 1);
    VAR(IDF_PERSIST, onscreendamagemin, 1, 10, VAR_MAX);
    VAR(IDF_PERSIST, onscreendamagemax, 1, 1000, VAR_MAX);
    VAR(IDF_PERSIST|IDF_HEX, onscreendamagecolour, PC(LAST), 0xFF4444, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, onscreendamageburncolour, PC(LAST), PC(BURN), 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, onscreendamagebleedcolour, PC(LAST), PC(BLEED), 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, onscreendamageshockcolour, PC(LAST), PC(SHOCK), 0xFFFFFF);

    VAR(IDF_PERSIST, onscreenhits, 0, 1, 2);
    VAR(IDF_PERSIST, onscreenhitsheal, 0, 1, 1);
    VAR(IDF_PERSIST, onscreenhitsself, 0, 0, 1);
    VAR(IDF_PERSIST, onscreenhitsfollow, 0, 0, 1);
    VAR(IDF_PERSIST, onscreenhitsmerge, 0, 250, VAR_MAX);
    VAR(IDF_PERSIST, onscreenhitstime, 1, 250, VAR_MAX);
    VAR(IDF_PERSIST, onscreenhitsfade, 1, 3000, VAR_MAX);
    FVAR(IDF_PERSIST, onscreenhitsswipe, 0, 6, 1000);
    FVAR(IDF_PERSIST, onscreenhitsscale, 0, 0.75f, 1000);
    FVAR(IDF_PERSIST, onscreenhitsblend, 0, 1, 1);
    FVAR(IDF_PERSIST, onscreenhitsheight, -1000, 0.25f, 1000);
    FVAR(IDF_PERSIST, onscreenhitsoffset, -1000, 3, 1000);
    VAR(IDF_PERSIST, onscreenhitsglow, 0, 1, 1);
    FVAR(IDF_PERSIST, onscreenhitsglowblend, 0, 1, 1);
    FVAR(IDF_PERSIST, onscreenhitsglowscale, 0, 1.5f, 1000);
    FVAR(IDF_PERSIST, onscreenhitsglowcolour, 0, 0.75f, 5);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, onscreenhitsglowtex, "<grey>textures/hud/glow", 3);
    VAR(IDF_PERSIST|IDF_HEX, onscreenhitscolour, PC(LAST), 0xFF4444, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, onscreenhitsburncolour, PC(LAST), PC(BURN), 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, onscreenhitsbleedcolour, PC(LAST), PC(BLEED), 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, onscreenhitsshockcolour, PC(LAST), PC(SHOCK), 0xFFFFFF);

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, spree1tex, "textures/rewards/carnage", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, spree2tex, "textures/rewards/slaughter", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, spree3tex, "textures/rewards/massacre", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, spree4tex, "textures/rewards/bloodbath", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, multi1tex, "textures/rewards/double", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, multi2tex, "textures/rewards/triple", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, multi3tex, "textures/rewards/multi", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, headshottex, "textures/rewards/headshot", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, dominatetex, "textures/rewards/dominate", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, revengetex, "textures/rewards/revenge", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, firstbloodtex, "textures/rewards/firstblood", 3);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, breakertex, "textures/rewards/breaker", 3);

    TVAR(IDF_PERSIST, privnonetex, "<grey>textures/privs/none", 3);
    TVAR(IDF_PERSIST, privbottex, "<grey>textures/privs/bot", 3);
    TVAR(IDF_PERSIST, privplayertex, "<grey>textures/privs/player", 3);
    TVAR(IDF_PERSIST, privsupportertex, "<grey>textures/privs/supporter", 3);
    TVAR(IDF_PERSIST, privmoderatortex, "<grey>textures/privs/moderator", 3);
    TVAR(IDF_PERSIST, privadministratortex, "<grey>textures/privs/administrator", 3);
    TVAR(IDF_PERSIST, privdevelopertex, "<grey>textures/privs/developer", 3);
    TVAR(IDF_PERSIST, privfoundertex, "<grey>textures/privs/founder", 3);
    TVAR(IDF_PERSIST, privlocalsupportertex, "<grey>textures/privs/localsupporter", 3);
    TVAR(IDF_PERSIST, privlocalmoderatortex, "<grey>textures/privs/localmoderator", 3);
    TVAR(IDF_PERSIST, privlocaladministratortex, "<grey>textures/privs/localadministrator", 3);

    TVAR(IDF_PERSIST, modedemotex, "<grey>textures/modes/demo", 3);
    TVAR(IDF_PERSIST, modeeditingtex, "<grey>textures/modes/editing", 3);
    TVAR(IDF_PERSIST, modedeathmatchtex, "<grey>textures/modes/deathmatch", 3);
    TVAR(IDF_PERSIST, modegladiatortex, "<grey>textures/modes/gladiator", 3);
    TVAR(IDF_PERSIST, modeoldschooltex, "<grey>textures/modes/oldschool", 3);

    TVAR(IDF_PERSIST, modecapturetex, "<grey>textures/modes/capture", 3);
    TVAR(IDF_PERSIST, modecapturequicktex, "<grey>textures/modes/capturequick", 3);
    TVAR(IDF_PERSIST, modecapturedefendtex, "<grey>textures/modes/capturedefend", 3);
    TVAR(IDF_PERSIST, modecaptureprotecttex, "<grey>textures/modes/captureprotect", 3);

    TVAR(IDF_PERSIST, modedefendtex, "<grey>textures/modes/defend", 3);
    TVAR(IDF_PERSIST, modedefendquicktex, "<grey>textures/modes/defendquick", 3);
    TVAR(IDF_PERSIST, modedefendkingtex, "<grey>textures/modes/defendking", 3);

    TVAR(IDF_PERSIST, modebombertex, "<grey>textures/modes/bomber", 3);
    TVAR(IDF_PERSIST, modebomberholdtex, "<grey>textures/modes/bomberhold", 3);
    TVAR(IDF_PERSIST, modebomberbaskettex, "<grey>textures/modes/bomberbasket", 3);
    TVAR(IDF_PERSIST, modebomberassaulttex, "<grey>textures/modes/bomberassault", 3);

    TVAR(IDF_PERSIST, moderacetex, "<grey>textures/modes/race", 3);
    TVAR(IDF_PERSIST, moderacelappedtex, "<grey>textures/modes/racelapped", 3);
    TVAR(IDF_PERSIST, moderaceendurancetex, "<grey>textures/modes/raceendurance", 3);
    TVAR(IDF_PERSIST, moderacegauntlettex, "<grey>textures/modes/racegauntlet", 3);

    TVAR(IDF_PERSIST, modeffatex, "<grey>textures/modes/ffa", 3);
    TVAR(IDF_PERSIST, modecooptex, "<grey>textures/modes/coop", 3);
    TVAR(IDF_PERSIST, modeinstatex, "<grey>textures/modes/instagib", 3);
    TVAR(IDF_PERSIST, modemedievaltex, "<grey>textures/modes/medieval", 3);
    TVAR(IDF_PERSIST, modekaboomtex, "<grey>textures/modes/kaboom", 3);
    TVAR(IDF_PERSIST, modedueltex, "<grey>textures/modes/duel", 3);
    TVAR(IDF_PERSIST, modesurvivortex, "<grey>textures/modes/survivor", 3);
    TVAR(IDF_PERSIST, modeclassictex, "<grey>textures/modes/classic", 3);
    TVAR(IDF_PERSIST, modeonslaughttex, "<grey>textures/modes/onslaught", 3);
    TVAR(IDF_PERSIST, modevampiretex, "<grey>textures/modes/vampire", 3);
    TVAR(IDF_PERSIST, moderesizetex, "<grey>textures/modes/resize", 3);
    TVAR(IDF_PERSIST, modehardtex, "<grey>textures/modes/hard", 3);
    TVAR(IDF_PERSIST, modearenatex, "<grey>textures/modes/arena", 3);

    #define ADDMODEICON(g,m) \
    { \
        if(m_demo(g)) ADDMODE(demo) \
        else if(m_edit(g)) ADDMODE(editing) \
        else if(m_capture(g)) \
        { \
            if(m_ctf_quick(g, m)) ADDMODE(capturequick) \
            else if(m_ctf_defend(g, m)) ADDMODE(capturedefend) \
            else if(m_ctf_protect(g, m)) ADDMODE(captureprotect) \
            else ADDMODE(capture) \
        } \
        else if(m_defend(g)) \
        { \
            if(m_dac_king(g, m)) \
            { \
                ADDMODE(defendking) \
                if(m_dac_quick(g, m)) ADDMODE(defendquick) \
            } \
            else if(m_dac_quick(g, m)) ADDMODE(defendquick) \
            else ADDMODE(defend) \
        } \
        else if(m_bomber(g)) \
        { \
            if(m_bb_hold(g, m)) ADDMODE(bomberhold) \
            else if(m_bb_assault(g, m)) \
            { \
                ADDMODE(bomberassault) \
                if(m_bb_basket(g, m)) ADDMODE(bomberbasket) \
            } \
            else if(m_bb_basket(g, m)) ADDMODE(bomberbasket) \
            else ADDMODE(bomber) \
        } \
        else if(m_race(g)) \
        { \
            if(m_ra_gauntlet(g, m)) \
            { \
                ADDMODE(racegauntlet) \
                if(m_ra_lapped(g, m)) ADDMODE(racelapped) \
                if(m_ra_endurance(g, m)) ADDMODE(raceendurance) \
            } \
            else if(m_ra_lapped(g, m)) \
            { \
                ADDMODE(racelapped) \
                if(m_ra_endurance(g, m)) ADDMODE(raceendurance) \
            } \
            else if(m_ra_endurance(g, m)) ADDMODE(raceendurance) \
            else ADDMODE(race) \
        } \
        else \
        { \
            if(m_dm_gladiator(g, m)) ADDMODE(gladiator) \
            else if(m_dm_oldschool(g, m)) ADDMODE(oldschool) \
            else ADDMODE(deathmatch) \
        } \
    }

    #define ADDMODE(s) { if(list.length()) list.add(' '); list.put(mode##s##tex, strlen(mode##s##tex)); }
    void modetex(int g, int m, vector<char> &list)
    {
        modecheck(g, m);
        ADDMODEICON(g, m);
    }

    void modetexs(int g, int m, bool before, bool implied, vector<char> &list)
    {
        modecheck(g, m);
        if(before) modetex(g, m, list);
        if(m_ffa(g, m) && (implied || !(gametype[g].implied&(1<<G_M_FFA)))) ADDMODE(ffa)
        if(m_coop(g, m) && (implied || !(gametype[g].implied&(1<<G_M_COOP)))) ADDMODE(coop)
        if(m_insta(g, m) && (implied || !(gametype[g].implied&(1<<G_M_INSTAGIB)))) ADDMODE(insta)
        if(m_medieval(g, m) && (implied || !(gametype[g].implied&(1<<G_M_MEDIEVAL)))) ADDMODE(medieval)
        if(m_kaboom(g, m) && (implied || !(gametype[g].implied&(1<<G_M_KABOOM)))) ADDMODE(kaboom)
        if(m_duel(g, m) && (implied || !(gametype[g].implied&(1<<G_M_DUEL)))) ADDMODE(duel)
        if(m_survivor(g, m) && (implied || !(gametype[g].implied&(1<<G_M_SURVIVOR)))) ADDMODE(survivor)
        if(m_classic(g, m) && (implied || !(gametype[g].implied&(1<<G_M_CLASSIC)))) ADDMODE(classic)
        if(m_onslaught(g, m) && (implied || !(gametype[g].implied&(1<<G_M_ONSLAUGHT)))) ADDMODE(onslaught)
        if(m_vampire(g, m) && (implied || !(gametype[g].implied&(1<<G_M_VAMPIRE)))) ADDMODE(vampire)
        if(m_resize(g, m) && (implied || !(gametype[g].implied&(1<<G_M_RESIZE)))) ADDMODE(resize)
        if(m_hard(g, m) && (implied || !(gametype[g].implied&(1<<G_M_HARD)))) ADDMODE(hard)
        if(m_arena(g, m) && (implied || !(gametype[g].implied&(1<<G_M_ARENA)))) ADDMODE(arena)
        if(!before) modetex(g, m, list);
    }
    #undef ADDMODE

    ICOMMAND(0, modetexlist, "iibi", (int *g, int *m, int *b, int *p),
    {
        vector<char> list;
        if(*b >= 0) modetexs(*g, *m, *b!=0, *p!=0, list);
        else modetex(*g, *m, list);
        list.add('\0');
        result(list.getbuf());
    });

    const char *modeimage()
    {
        if(!connected()) return "menu";
        #define ADDMODE(s) return #s;
        ADDMODEICON(game::gamemode, game::mutators);
        #undef ADDMODE
    }

    bool needminimap() { return true; }

    int hasinput(bool pass, bool cursor)
    {
        if(cdpi::getoverlay() > 0 || commandmillis > 0 || curcompass) return true;
        int cur = UI::hasinput(cursor);
        if(!cur && UI::hasmenu(pass)) cur = 1;
        return cur;
    }
    ICOMMANDV(0, hasinput, hasinput())

    bool hastkwarn()
    {
        if(!m_play(game::gamemode)) return false;
        return teamkillwarn && m_team(game::gamemode, game::mutators) && numteamkills() >= teamkillwarn;
    }
    ICOMMANDV(0, hastkwarn, hastkwarn() ? 1 : 0)

    bool textinput(const char *str, int len)
    {
        return UI::textinput(str, len);
    }

    bool keypress(int code, bool isdown)
    {
        if(curcompass) return keycmenu(code, isdown);
        return UI::keypress(code, isdown); // ignore UI if compass is open
    }

    VAR(IDF_PERSIST, aboveheadui, 0, 1, 1);
    FVAR(IDF_PERSIST, aboveheaduiyaw, -1, -1, 360);
    FVAR(IDF_PERSIST, aboveheaduipitch, -181, -181, 181);
    FVAR(IDF_PERSIST, aboveheaduiscale, FVAR_NONZERO, 1, FVAR_MAX);
    FVAR(IDF_PERSIST, aboveheaduidetentyaw, 0, 0, 180);
    FVAR(IDF_PERSIST, aboveheaduidetentpitch, 0, 0, 90);
    void checkui()
    {
        hidecrosshair = 0;

        UI::showui("hud");
        if(!UI::hasmenu(true))
        {
            if(connected())
            {
                UI::pressui("scoreboard", scoreson);
                if(game::player1->state == CS_DEAD) { if(scoreson) shownscores = true; }
                else shownscores = false;
            }
            else UI::openui("main");
        }

        if(aboveheadui)
        {
            gameent *d = NULL;
            int numdyns = game::numdynents();
            loopi(numdyns) if((d = (gameent *)game::iterdynents(i)))
            {
                if(d->actortype >= A_ENEMY || (d == game::focus && (!game::aboveheaddead || (d->state != CS_DEAD && d->state != CS_WAITING))))
                {
                    if(UI::uivisible("abovehead", UI::SURFACE_MAIN, d->clientnum)) UI::hideui("abovehead", UI::SURFACE_MAIN, d->clientnum);
                    continue;
                }
                if(UI::uivisible("abovehead", UI::SURFACE_MAIN, d->clientnum))
                    UI::setui("abovehead", UI::SURFACE_MAIN, d->clientnum, d->abovehead(), aboveheaduiyaw, aboveheaduipitch, aboveheaduiscale, aboveheaduidetentyaw, aboveheaduidetentpitch);
                else UI::showui("abovehead", UI::SURFACE_MAIN, d->clientnum, d->abovehead(), aboveheaduiyaw, aboveheaduipitch, aboveheaduiscale, aboveheaduidetentyaw, aboveheaduidetentpitch);
            }
        }
        else UI::closedynui("abovehead");

        UI::update();
    }

    void damage(int n, const vec &loc, gameent *v, int weap, int flags)
    {
        if(!n || !v) return;
        int m = flags&HIT(BURN) || flags&HIT(BLEED) || flags&HIT(SHOCK) ? damageresiduemulresidual : damageresiduemul;
        damageresidue = clamp(damageresidue+(n*m), 0, damageresiduemax);
        int colour = onscreendamagecolour;
        if(game::nogore || game::bloodscale <= 0) colour = 0xFF00FF;
        else if(wr_burns(weap, flags)) colour = onscreendamageburncolour;
        else if(wr_bleeds(weap, flags)) colour = onscreendamagebleedcolour;
        else if(wr_shocks(weap, flags)) colour = onscreendamageshockcolour;
        vec dir = vec(loc).sub(camera1->o).normalize();
        loopv(damagelocs)
        {
            dhloc &l = damagelocs[i];
            if(v->clientnum != l.clientnum) continue;
            if(totalmillis-l.outtime > onscreendamagemerge) continue;
            if(l.colour != colour) continue;
            l.damage += n;
            l.dir = dir;
            return; // accumulate
        }
        damagelocs.add(dhloc(v->clientnum, totalmillis, n, loc, colour));
    }

    void hit(int n, const vec &loc, gameent *v, int weap, int flags)
    {
        if(!n || !v) return;
        int colour = onscreenhitscolour;
        if(game::nogore || game::bloodscale <= 0) colour = 0xFF00FF;
        else if(wr_burns(weap, flags)) colour = onscreenhitsburncolour;
        else if(wr_bleeds(weap, flags)) colour = onscreenhitsbleedcolour;
        else if(wr_shocks(weap, flags)) colour = onscreenhitsshockcolour;
        loopv(hitlocs)
        {
            dhloc &l = hitlocs[i];
            if(v->clientnum != l.clientnum) continue;
            if(totalmillis-l.outtime > onscreenhitsmerge) continue;
            if(l.colour != colour) continue;
            l.damage += n;
            l.dir = v->center();
            return; // accumulate
        }
        hitlocs.add(dhloc(v->clientnum, totalmillis, n, v->center(), colour));
    }

    void removeplayer(gameent *d)
    {
        loopvrev(damagelocs)
        {
            dhloc &l = damagelocs[i];
            gameent *e = game::getclient(l.clientnum);
            if(!e || e == d) damagelocs.remove(i);
        }
        loopvrev(hitlocs)
        {
            dhloc &l = hitlocs[i];
            gameent *e = game::getclient(l.clientnum);
            if(!e || e == d) hitlocs.remove(i);
        }
    }

    void drawquad(float x, float y, float w, float h, float tx1, float ty1, float tx2, float ty2, bool flipx, bool flipy)
    {
        if(flipx) swap(tx1, tx2);
        if(flipy) swap(ty1, ty2);
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x, y); gle::attribf(tx1, ty1);
        gle::attribf(x+w, y); gle::attribf(tx2, ty1);
        gle::attribf(x, y+h); gle::attribf(tx1, ty2);
        gle::attribf(x+w, y+h); gle::attribf(tx2, ty2);
        gle::end();
    }
    void drawcoord(float x, float y, float w, float h, float tx, float ty, float tw, float th, bool flipx, bool flipy) { drawquad(x, y, w, h, tx, ty, tx+tw, ty+th, flipx, flipy); }
    void drawtexture(float x, float y, float w, float h, bool flipx, bool flipy) { drawquad(x, y, w, h, 0, 0, 1, 1, flipx, flipy); }
    void drawsized(float x, float y, float s, bool flipx, bool flipy) { drawquad(x, y, s, s, 0, 0, 1, 1, flipx, flipy); }

    void drawblend(int x, int y, int w, int h, float r, float g, float b, bool blend)
    {
        if(!blend) glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        gle::colorf(r, g, b);
        gle::defvertex(2);
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x, y);
        gle::attribf(x+w, y);
        gle::attribf(x, y+h);
        gle::attribf(x+w, y+h);
        gle::end();
        if(!blend) glDisable(GL_BLEND);
        else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void colourskew(float &r, float &g, float &b, float skew)
    {
        if(skew >= 2.f)
        { // fully overcharged to green
            r = b = 0;
        }
        else if(skew >= 1.f)
        { // overcharge to yellow
            b *= 1.f-(skew-1.f);
        }
        else if(skew >= 0.5f)
        { // fade to orange
            float off = skew-0.5f;
            g *= 0.5f+off;
            b *= off*2.f;
        }
        else
        { // fade to red
            g *= skew;
            b = 0;
        }
    }

    template<class T>
    void skewcolour(T &r, T &g, T &b, int colour = 0, bool faded = false)
    {
        if(colour < 0) colour = game::getcolour(game::focus, INVPULSE(colour));
        vec c = vec::fromcolor(colour);
        r = T(r*c.r);
        g = T(g*c.g);
        b = T(b*c.b);
        if(!game::focus->team && faded)
        {
            float f = game::focus->state == CS_SPECTATOR || game::focus->state == CS_EDITING ? 0.25f : 0.5f;
            r = T(r*f);
            g = T(g*f);
            b = T(b*f);
        }
    }

    enum
    {
        POINTER_NONE = 0, POINTER_UI, POINTER_EDIT, POINTER_SPEC,
        POINTER_HAIR, POINTER_TEAM, POINTER_ZOOM, POINTER_HIT, POINTER_MAX
    };

    const char *getpointer(int index, int weap = -1)
    {
        switch(index)
        {
            case POINTER_UI:
            {
                if(UI::uihidden) return NULL;
                switch(UI::cursortype())
                {
                    case CURSOR_HIDDEN: return NULL; break;
                    case CURSOR_HOVER: return cursorhovertex; break;
                    case CURSOR_DEFAULT: default: break;
                }
                return cursortex;
            }
            case POINTER_EDIT: return editcursortex;
            case POINTER_SPEC: return game::tvmode() ? tvcursortex : speccursortex;
            case POINTER_HAIR:
            {
                if(crosshairweapons&1 && isweap(weap))
                {
                    const char *crosshairtexs[W_MAX] = {
                        clawcrosshairtex, pistolcrosshairtex, swordcrosshairtex, shotguncrosshairtex, smgcrosshairtex,
                        flamercrosshairtex, plasmacrosshairtex, zappercrosshairtex, riflecrosshairtex, grenadecrosshairtex, minecrosshairtex,
                        rocketcrosshairtex, meleecrosshairtex // end of regular weapons
                    };
                    if(*crosshairtexs[weap]) return crosshairtexs[weap];
                }
                return crosshairtex;
            }
            case POINTER_TEAM: return teamcrosshairtex;
            case POINTER_ZOOM: return zoomcrosshairtex;
            case POINTER_HIT:
            {
                if(crosshairweapons&1 && isweap(weap))
                {
                    const char *hithairtexs[W_MAX] = {
                        clawhithairtex, pistolhithairtex, swordhithairtex, shotgunhithairtex, smghithairtex,
                        flamerhithairtex, plasmahithairtex, zapperhithairtex, riflehithairtex, grenadehithairtex, minehithairtex,
                        rockethithairtex, meleehithairtex // end of regular weapons
                    };
                    if(*hithairtexs[weap]) return hithairtexs[weap];
                }
                return hithairtex;
            }
            default: break;
        }
        return NULL;
    }
    ICOMMAND(0, getpointer, "ii", (int *i, int *j), result(getpointer(*i, *j)));

    void drawindicator(int weap, int x, int y, float s, bool secondary)
    {
        int millis = lastmillis-game::focus->weaptime[weap];
        if(!game::focus->weapwait[weap] || millis > game::focus->weapwait[weap]) return;
        float r = 1, g = 1, b = 1, amt = 0;
        switch(game::focus->weapstate[weap])
        {
            case W_S_POWER: case W_S_ZOOM:
            {
                amt = clamp(float(millis)/float(game::focus->weapwait[weap]), 0.f, 1.f);
                colourskew(r, g, b, amt);
                break;
            }
            case W_S_RELOAD:
            {
                if(showindicator < (W(weap, ammoadd) < W(weap, ammoclip) ? 3 : 2)) return;
                amt = 1.f-clamp(float(millis)/float(game::focus->weapwait[weap]), 0.f, 1.f);
                colourskew(r, g, b, 1.f-amt);
                break;
            }
            case W_S_PRIMARY: case W_S_SECONDARY:
            {
                if(showindicator < 4 || game::focus->weapwait[weap] < indicatorminattack) return;
                amt = 1.f-clamp(float(millis)/float(game::focus->weapwait[weap]), 0.f, 1.f);
                colourskew(r, g, b, 1.f-amt);
                break;
            }
            default: return;
        }
        Texture *t = textureload(indicatortex, 3);
        if(t->type&Texture::ALPHA) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        else glBlendFunc(GL_ONE, GL_ONE);
        settexture(t);
        float val = amt < 0.25f ? amt : (amt > 0.75f ? 1.f-amt : 0.25f);
        gle::colorf(val*4.f, val*4.f, val*4.f, indicatorblend*hudblend*val);
        drawsized(x-s, y-s, s*2);
        gle::colorf(r, g, b, indicatorblend*hudblend);
        drawslice(0, clamp(amt, 0.f, 1.f), x, y, s);
    }

    void drawclipitem(const char *tex, float x, float y, float offset, float size, float blend, float angle, float spin, int rotate, const vec &colour)
    {
        Texture *t = textureload(tex, 3);
        if(!t || t == notexture) return;
        while(angle < 0.0f) angle += 360.0f;
        while(angle >= 360.0f) angle -= 360.0f;
        bool flipx = false, flipy = false;
        float rot = 0;
        if(rotate&8) rot += spin;
        if(rotate&4) rot += angle;
        if(rotate&2) flipy = angle > 90.f && angle <= 180.f;
        if(rotate&1) flipx = angle >= 180.f;
        while(rot < 0.0f) rot += 360.0f;
        while(rot >= 360.0f) rot -= 360.0f;
        vec2 loc(x+offset*sinf(RAD*angle), y+offset*-cosf(RAD*angle));
        gle::color(colour, blend);
        settexture(t);
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        loopk(4)
        {
            vec2 norm, tc;
            switch(k)
            {
                case 0: vecfromyaw(rot, 1, -1, norm);   tc = vec2(0, 1); break;
                case 1: vecfromyaw(rot, 1, 1, norm);    tc = vec2(1, 1); break;
                case 2: vecfromyaw(rot, -1, -1, norm);  tc = vec2(0, 0); break;
                case 3: vecfromyaw(rot, -1, 1, norm);   tc = vec2(1, 0); break;
            }
            norm.mul(size*0.5f).add(loc);
            gle::attrib(norm);
            if(flipx) tc.x = 1 - tc.x;
            if(flipy) tc.y = 1 - tc.y;
            gle::attrib(tc);
        }
        gle::end();
    }

    void drawclip(int weap, int x, int y, float s, bool preview)
    {
        if(!isweap(weap) || weap >= W_MAX || (!W2(weap, ammosub, false) && !W2(weap, ammosub, true))) return;
        const char *cliptexs[W_MAX] = {
            clawcliptex, pistolcliptex, swordcliptex, shotguncliptex, smgcliptex,
            flamercliptex, plasmacliptex, zappercliptex, riflecliptex, grenadecliptex, minecliptex, rocketcliptex, meleecliptex
        };
        const float clipoffs[W_MAX] = {
            clawclipoffset, pistolclipoffset, swordclipoffset, shotgunclipoffset, smgclipoffset,
            flamerclipoffset, plasmaclipoffset, zapperclipoffset, rifleclipoffset, grenadeclipoffset, mineclipoffset, rocketclipoffset, meleeclipoffset
        };
        const float clipskew[W_MAX] = {
            clawclipskew, pistolclipskew, swordclipskew, shotgunclipskew, smgclipskew,
            flamerclipskew, plasmaclipskew, zapperclipskew, rifleclipskew, grenadeclipskew, mineclipskew, rocketclipskew, meleeclipskew
        };
        const int cliprots[W_MAX] = {
            clawcliprotate, pistolcliprotate, swordcliprotate, shotguncliprotate, smgcliprotate,
            flamercliprotate, plasmacliprotate, zappercliprotate, riflecliprotate, grenadecliprotate, minecliprotate, rocketcliprotate, meleecliprotate
        };
        int maxammo = W(weap, ammoclip), ammo = preview ? maxammo : game::focus->weapammo[weap][W_A_CLIP],
            store = game::focus->actortype >= A_ENEMY || W(weap, ammostore) < 0 ? maxammo : game::focus->weapammo[weap][W_A_STORE], interval = lastmillis-game::focus->weaptime[weap];
        float fade = clipblend*hudblend, skew = clipskew[weap]*clipsize, size = s*skew, offset = s*clipoffset,
              slice = 360/float(maxammo), angle = (maxammo > (cliprots[weap]&4 ? 4 : 3) || maxammo%2 ? 360.f : 360.f-slice*0.5f)-((maxammo-ammo)*slice),
              area = 1-clamp(clipoffs[weap]*2, 1e-3f, 1.f), need = s*skew*area*maxammo, have = 2*M_PI*s*clipoffset,
              scale = clamp(have/need, clipminscale, clipmaxscale), start = angle, amt = 0, spin = 0;
        vec c(1, 1, 1);
        if(clipstone) skewcolour(c.r, c.g, c.b, clipstone);
        if(clipcolour) skewcolour(c.r, c.g, c.b, W(weap, colour));
        if(!preview && interval <= game::focus->weapwait[weap]) switch(game::focus->weapstate[weap])
        {
            case W_S_PRIMARY: case W_S_SECONDARY:
            {
                if(!W2(weap, ammosub, game::focus->weapstate[weap] == W_S_SECONDARY)) break;
                amt = 1.f-clamp(interval/float(game::focus->weapwait[weap]), 0.f, 1.f);
                fade *= amt;
                if(clipanims)
                {
                    size *= amt;
                    offset *= amt;
                    if(clipanims >= 2) spin = 360*amt;
                }
                int shot = game::focus->weapshot[weap] ? game::focus->weapshot[weap] : 1;
                float rewind = angle;
                loopi(shot) drawclipitem(cliptexs[weap], x, y, offset, size*scale, fade, rewind += slice, spin, cliprots[weap], c);
                fade = clipblend*hudblend;
                size = s*skew;
                offset = s*clipoffset;
                spin = 0;
                break;
            }
            case W_S_RELOAD: case W_S_USE:
            {
                if(game::focus->weapload[weap][W_A_CLIP] > 0)
                {
                    int check = game::focus->weapwait[weap]/2;
                    float ss = slice*game::focus->weapload[weap][W_A_CLIP];
                    if(interval >= check)
                    {
                        amt = clamp((interval-check)/float(check), 0.f, 1.f);
                        fade *= amt;
                        if(clipanims)
                        {
                            size *= amt*3/4;
                            offset *= amt*3/4;
                            if(clipanims >= 2) spin = 360*amt;
                        }
                        loopi(game::focus->weapload[weap][W_A_CLIP])
                        {
                            drawclipitem(cliptexs[weap], x, y, offset, size*scale, fade, angle, spin, cliprots[weap], c);
                            angle -= slice;
                        }
                    }
                    else angle -= ss;
                    start -= ss;
                    store += game::focus->weapload[weap][W_A_CLIP];
                    ammo -= game::focus->weapload[weap][W_A_CLIP];
                    fade = clipblend*hudblend;
                    size = s*skew;
                    offset = s*clipoffset;
                    spin = 0;
                    break;
                }
                if(game::focus->weapstate[weap] == W_S_USE && game::focus->getlastweap(m_weapon(game::focus->actortype, game::gamemode, game::mutators)) == weap)
                {
                    amt = clamp(interval/float(game::focus->weapwait[weap]), 0.f, 1.f);
                    if(clipanims)
                    {
                        angle -= 360*amt;
                        start -= 360*amt;
                        if(clipanims >= 2) spin = 360*amt;
                    }
                    break;
                }
                // falls through
            }
            case W_S_SWITCH:
            {
                amt = clamp(interval/float(game::focus->weapwait[weap]), 0.f, 1.f);
                fade *= amt;
                if(clipanims && game::focus->weapstate[weap] != W_S_RELOAD)
                {
                    size *= amt;
                    offset *= amt;
                    if(clipanims >= 2) spin = 360*amt;
                }
                else spin = 0;
                break;
            }
            default: break;
        }
        loopi(ammo)
        {
            drawclipitem(cliptexs[weap], x, y, offset, size*scale, fade, angle, spin, cliprots[weap], c);
            angle -= slice;
        }
        if(clipstore && ammo < maxammo && store > 0)
        {
            int total = clamp(store, 0, maxammo-ammo);
            loopi(total) drawclipitem(cliptexs[weap], x, y, offset, size*scale, fade*clipstoreblend, start += slice, spin, cliprots[weap], c);
        }
    }

    void drawcirclebar(int x, int y, float s)
    {
        if(game::focus->state != CS_ALIVE) return;
        int num = 0;
        loopi(3) if(circlebartype&(1<<i))
        {
            if(i == 1 && !impulsemeter) continue;
            num++;
        }
        if(!num) return;
        Texture *t = circlebartex && *circlebartex ? textureload(circlebartex, 3) : NULL;
        if(!t || t == notexture) return;
        float slice = 1.f/num, pos = num%2 ? slice*0.5f : 0.f;
        if(t->type&Texture::ALPHA) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        else glBlendFunc(GL_ONE, GL_ONE);
        settexture(t);
        loopi(3) if(circlebartype&(1<<i))
        {
            float val = 0, fade = hudblend*circlebarblend;
            vec c(1, 1, 1);
            switch(i)
            {
                case 0:
                    val = min(1.f, game::focus->health/float(max(game::focus->gethealth(game::gamemode, game::mutators), 1)));
                    if(circlebarhealthtone) skewcolour(c.r, c.g, c.b, circlebarhealthtone);
                    break;
                case 1:
                    if(!impulsemeter) continue;
                    val = 1-clamp(float(game::focus->impulse[IM_METER])/float(impulsemeter), 0.f, 1.f);
                    if(circlebarimpulsetone) skewcolour(c.r, c.g, c.b, circlebarimpulsetone);
                    break;
                case 2:
                {
                    if(!isweap(game::focus->weapselect)) continue;
                    int weap = game::focus->weapselect, interval = lastmillis-game::focus->weaptime[weap];
                    val = game::focus->weapammo[weap][W_A_CLIP]/float(W(weap, ammoclip));
                    if(circlebarammotone || circlebarammocolour) skewcolour(c.r, c.g, c.b, circlebarammocolour ? W(weap, colour) : circlebarammotone);
                    if(interval <= game::focus->weapwait[weap]) switch(game::focus->weapstate[weap])
                    {
                        case W_S_RELOAD: case W_S_USE:
                            if(game::focus->weapload[weap][W_A_CLIP] > 0)
                            {
                                val -= game::focus->weapload[weap][W_A_CLIP]/float(W(weap, ammoclip));
                                break;
                            }
                            else if(game::focus->weapstate[weap] == W_S_RELOAD) break;
                        case W_S_SWITCH:
                        {
                            float amt = clamp(float(interval)/float(game::focus->weapwait[weap]), 0.f, 1.f);
                            fade *= amt;
                            val *= amt;
                            break;
                        }
                        default: break;
                    }
                    break;
                }
            }
            gle::color(vec(c).mul(0.25f), hudblend*circlebarblend*0.65f);
            drawslice(pos, slice, x, y, s*circlebarsize);
            if(val > 0)
            {
                gle::color(c, fade);
                drawslice(pos, val*slice, x, y, s*circlebarsize);
            }
            float nps = pos+val*slice;
            val = 0;
            fade = hudblend*circlebarblend;
            switch(i)
            {
                case 2:
                {
                    int weap = game::focus->weapselect, interval = lastmillis-game::focus->weaptime[weap];
                    if(interval <= game::focus->weapwait[weap]) switch(game::focus->weapstate[weap])
                    {
                        case W_S_PRIMARY: case W_S_SECONDARY:
                        {
                            float amt = 1.f-clamp(float(interval)/float(game::focus->weapwait[weap]), 0.f, 1.f);
                            fade *= amt;
                            val = (game::focus->weapshot[weap] ? game::focus->weapshot[weap] : 1)/float(W(weap, ammoclip))*amt;
                            break;
                        }
                        case W_S_RELOAD: case W_S_USE:
                        {
                            if(game::focus->weapload[weap][W_A_CLIP] > 0)
                            {
                                int check = game::focus->weapwait[weap]/2;
                                if(interval >= check)
                                {
                                    float amt = clamp(float(interval-check)/float(check), 0.f, 1.f);
                                    fade *= amt;
                                    val = game::focus->weapload[weap][W_A_CLIP]/float(W(weap, ammoclip))*amt;
                                }
                            }
                            break;
                        }
                        default: break;
                    }
                    break;
                }
                case 0:
                {
                    float total = 0;
                    loopv(damagelocs)
                    {
                        dhloc &l = damagelocs[i];
                        gameent *e = game::getclient(l.clientnum);
                        if(!e || l.dir.iszero()) { damagelocs.remove(i--); continue; }
                        int millis = totalmillis-l.outtime, delay = min(20, l.damage)*50;
                        if(millis >= delay) { if(millis >= onscreendamagetime+onscreendamagefade) damagelocs.remove(i--); continue; }
                        if(!onscreendamageself && e == game::focus) continue;
                        float dam = l.damage/float(max(game::focus->gethealth(game::gamemode, game::mutators), 1)),
                              amt = millis/float(delay);
                        total += dam;
                        val += dam*(1-amt);
                    }
                    if(total > 0)
                    {
                        float amt = val/total;
                        fade *= amt;
                        flashcolour(c.r, c.g, c.b, 0.3f, 0.6f, 0.1f, amt);
                    }
                    else val = 0;
                }
                default: case 1: break;
            }
            if(val > 0)
            {
                gle::color(c, fade);
                drawslice(nps, val*slice, x, y, s*circlebarsize);
            }
            pos += slice;
        }
    }

    void drawpointertex(const char *tex, int x, int y, int s, float r, float g, float b, float fade)
    {
        if(!tex || !*tex) return;
        Texture *t = textureload(tex, 3);
        if(!t || t == notexture) return;
        if(t->type&Texture::ALPHA) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        else glBlendFunc(GL_ONE, GL_ONE);
        gle::colorf(r, g, b, fade);
        settexture(t);
        drawsized(x, y, s);
    }

    void drawpointer(int w, int h, int index)
    {
        float csize = crosshairsize * crosshairscale, fade = crosshairblend;
        switch(index)
        {
            case POINTER_EDIT: csize = editcursorsize; fade = editcursorblend; break;
            case POINTER_SPEC: csize = speccursorsize; fade = speccursorblend; break;
            case POINTER_TEAM: csize = teamcrosshairsize; fade = teamcrosshairblend; break;
            case POINTER_ZOOM:
                if(game::inzoom())
                {
                    csize = zoomcrosshairsize * crosshairscale;
                    fade = zoomcrosshairblend;
                    break;
                } // fall through
            case POINTER_HIT: case POINTER_HAIR:
            {
                if(crosshairweapons && isweap(game::focus->weapselect))
                {
                    const float crosshairsizes[W_MAX] = {
                        clawcrosshairsize, pistolcrosshairsize, swordcrosshairsize, shotguncrosshairsize, smgcrosshairsize,
                        flamercrosshairsize, plasmacrosshairsize, zappercrosshairsize, riflecrosshairsize, grenadecrosshairsize, minecrosshairsize, rocketcrosshairsize, meleecrosshairsize
                    }, crosshairblends[W_MAX] = {
                        clawcrosshairblend, pistolcrosshairblend, swordcrosshairblend, shotguncrosshairblend, smgcrosshairblend,
                        flamercrosshairblend, plasmacrosshairblend, zappercrosshairblend, riflecrosshairblend, grenadecrosshairblend, minecrosshairblend, rocketcrosshairblend, meleecrosshairblend
                    };
                    csize = crosshairsizes[game::focus->weapselect] * crosshairscale;
                    fade = crosshairblends[game::focus->weapselect];
                }
                break;
            }
            default: csize = cursorsize; fade = cursorblend; break;
        }
        vec c(1, 1, 1);
        int cs = int(csize*hudsize);
        if(game::focus->state == CS_ALIVE && index >= POINTER_HAIR)
        {
            if(index == POINTER_TEAM) c = vec::fromcolor(teamcrosshaircolour);
            else if(crosshairweapons&2) c = vec::fromcolor(W(game::focus->weapselect, colour));
            else if(crosshairtone) skewcolour(c.r, c.g, c.b, crosshairtone);
            int heal = game::focus->gethealth(game::gamemode, game::mutators);
            if(crosshairflash && game::focus->state == CS_ALIVE && game::focus->health < heal)
            {
                int millis = lastmillis%1000;
                float amt = (millis <= 500 ? millis/500.f : 1.f-((millis-500)/500.f))*clamp(float(heal-game::focus->health)/float(heal), 0.f, 1.f);
                flashcolour(c.r, c.g, c.b, 1.f, 0.f, 0.f, amt);
            }
            if(crosshairthrob > 0 && regentime && game::focus->lastregen && lastmillis-game::focus->lastregen <= regentime)
            {
                float skew = clamp((lastmillis-game::focus->lastregen)/float(regentime/2), 0.f, 2.f);
                cs += int(cs*(skew > 1.f ? 1.f-skew : skew)*(crosshairthrob*(game::focus->lastregenamt >= 0 ? 1 : -1)));
            }
            if(showcrosshair >= 2)
            {
                bool secondary = physics::secondaryweap(game::focus);
                float accskew = weapons::accmodspread(game::focus, game::focus->weapselect, secondary,  W2(game::focus->weapselect, cooked, true)&W_C_ZOOM && secondary)*crosshairaccamt;
                if(accskew > 0) fade /= accskew;
            }
        }
        int cx = int(hudwidth*cursorx), cy = int(hudheight*cursory);
        if(index != POINTER_UI)
        {
            drawpointertex(getpointer(index, game::focus->weapselect), cx-cs/2, cy-cs/2, cs, c.r, c.g, c.b, fade*hudblend);
            if(index > POINTER_UI)
            {
                if(showcirclebar) drawcirclebar(cx, cy, hudsize);
                if(game::focus->state == CS_ALIVE && game::focus->hasweap(game::focus->weapselect, m_weapon(game::focus->actortype, game::gamemode, game::mutators)))
                {
                    if(showclips) drawclip(game::focus->weapselect, cx, cy, hudsize);
                    if(showindicator) drawindicator(game::focus->weapselect, cx, cy, int(indicatorsize*hudsize), physics::secondaryweap(game::focus));
                }
                if(crosshairhitspeed && totalmillis-game::focus->lasthit <= crosshairhitspeed)
                {
                    vec c2(1, 1, 1);
                    if(hitcrosshairtone) skewcolour(c2.r, c2.g, c2.b, hitcrosshairtone);
                    else c2 = c;
                    drawpointertex(getpointer(POINTER_HIT, game::focus->weapselect), cx-cs/2, cy-cs/2, cs, c2.r, c2.g, c2.b, crosshairblend*hudblend);
                }
                if(crosshairdistance && game::focus->state == CS_EDITING)
                {
                    draw_textf("\fa%.1f\fwm", cx+crosshairdistancex, cy+crosshairdistancey, 0, 0, -1, -1, -1, int(hudblend*255), TEXT_RIGHT_JUSTIFY, -1, -1, 1, game::focus->o.dist(worldpos)/8.f);
                    resethudshader();
                }
            }
        }
        else drawpointertex(getpointer(index, game::focus->weapselect), cx, cy, cs, c.r, c.g, c.b, fade*hudblend);
    }

    void drawpointers(int w, int h)
    {
        int index = POINTER_NONE;
        if(hasinput(false, true)) index = hasinput(true, true) ? POINTER_UI : POINTER_NONE;
        else if(hidecrosshair || !showhud || !showcrosshair || game::focus->state == CS_DEAD || !gs_playing(game::gamestate) || client::waiting() || (game::thirdpersonview(true) && game::focus != game::player1))
            index = POINTER_NONE;
        else if(game::focus->state == CS_EDITING) index = POINTER_EDIT;
        else if(game::focus->state >= CS_SPECTATOR) index = POINTER_SPEC;
        else if(game::inzoom()) index = POINTER_ZOOM;
        else if(m_team(game::gamemode, game::mutators))
        {
            vec pos = game::focus->headpos();
            gameent *d = game::intersectclosest(pos, worldpos, game::focus);
            if(d && d->actortype < A_ENEMY && d->team == game::focus->team) index = POINTER_TEAM;
            else index = POINTER_HAIR;
        }
        else index = POINTER_HAIR;
        if(index > POINTER_NONE)
        {
            resethudshader();
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            drawpointer(w, h, index);
            glDisable(GL_BLEND);
        }
    }

    int numteamkills()
    {
        int numkilled = 0;
        loopvrev(teamkills)
        {
            if(totalmillis-teamkills[i] <= teamkilltime*60000) numkilled++;
            else teamkills.remove(i);
        }
        return numkilled;
    }
    ICOMMANDV(0, numteamkills, numteamkills())

    bool showname()
    {
        if(game::focus != game::player1)
        {
            if(game::thirdpersonview(true) && game::aboveheadnames >= 2) return false;
            return true;
        }
        return false;
    }
    ICOMMANDV(0, specshowname, showname() ? 1 : 0)

    const char *specviewname()
    {
        if(showname()) return game::colourname(game::focus);
        if(game::tvmode())
        {
            if(game::spectvfollow >= 0)
            {
                gameent *d = game::getclient(game::spectvfollow);
                if(d) return game::colourname(d);
            }
            return "SpecTV";
        }
        return "Spectating";
    }
    ICOMMANDVS(0, specviewname, specviewname());

    void drawevents(float blend)
    {
        if(!showeventicons || game::focus->state == CS_EDITING || game::focus->state == CS_SPECTATOR) return;

        int ty = int(((hudheight/2)-(hudheight/2*eventiconoffset))/eventiconscale), tx = int((hudwidth/2)/eventiconscale);
        pushhudscale(eventiconscale);
        resethudshader();

        loopv(game::focus->icons)
        {
            if(game::focus->icons[i].type == eventicon::AFFINITY && !(showeventicons&2)) continue;
            if(game::focus->icons[i].type == eventicon::WEAPON && !(showeventicons&4)) continue;

            int millis = totalmillis-game::focus->icons[i].millis;
            if(millis <= game::focus->icons[i].fade)
            {
                Texture *t = textureload(geticon(game::focus->icons[i].type, game::focus->icons[i].value));
                if(t && t != notexture)
                {
                    int olen = min(game::focus->icons[i].length/5, 1000), ilen = olen/2, colour = colourwhite;
                    float skew = millis < ilen ? millis/float(ilen) : (millis > game::focus->icons[i].fade-olen ? (game::focus->icons[i].fade-millis)/float(olen) : 1.f),
                          fade = blend*eventiconblend*skew;
                    int size = int(FONTH*skew), width = int((t->w/float(t->h))*size), rsize = game::focus->icons[i].type < eventicon::SORTED ? int(size*2/3) : int(size);
                    switch(game::focus->icons[i].type)
                    {
                        case eventicon::WEAPON: colour = W(game::focus->icons[i].value, colour); break;
                        case eventicon::AFFINITY: colour = m_bomber(game::gamemode) ? game::pulsehexcol(game::focus, PULSE_DISCO) : TEAM(game::focus->icons[i].value, colour); break;
                        default: break;
                    }
                    settexture(t);
                    gle::color(vec::fromcolor(colour), fade);
                    drawtexture(tx-width/2, ty-rsize/2, width, size);
                    ty -= rsize;
                }
            }
        }

        pophudmatrix();
    }

    float radarlimit(float dist) { return dist >= 0 && radardistlimit > 0 ? clamp(dist, 0.f, radardistlimit) : max(dist, 0.f); }
    ICOMMAND(0, getradarlimit, "f", (float *n), floatret(radarlimit(*n)));

    bool radarlimited(float dist) { return radardistlimit > 0 && dist > radardistlimit; }
    ICOMMAND(0, getradarlimited, "f", (float *n), intret(radarlimited(*n) ? 1 : 0));

    const char *teamtexname(int team)
    {
        const char *teamtexs[T_MAX] = { teamneutraltex, teamalphatex, teamomegatex, teamenemytex };
        return teamtexs[clamp(team, 0, T_MAX-1)];
    }

    const char *privtex(int priv, int actortype)
    {
        if(actortype != A_PLAYER) return privbottex;
        const char *privtexs[2][PRIV_MAX] = {
            { privnonetex, privplayertex, privsupportertex, privmoderatortex, privadministratortex, privdevelopertex, privfoundertex },
            { privnonetex, privplayertex, privlocalsupportertex, privlocalmoderatortex, privlocaladministratortex, privnonetex, privnonetex }
        };
        return privtexs[priv&PRIV_LOCAL ? 1 : 0][clamp(priv&PRIV_TYPE, 0, int(priv&PRIV_LOCAL ? PRIV_ADMINISTRATOR : PRIV_LAST))];
    }

    const char *itemtex(int type, int stype)
    {
        switch(type)
        {
            case PLAYERSTART: return playertex; break;
            case AFFINITY: return flagtex; break;
            case WEAPON:
            {
                const char *weaptexs[W_MAX] = {
                    clawtex, pistoltex, swordtex, shotguntex, smgtex, flamertex, plasmatex, zappertex, rifletex, grenadetex, minetex, rockettex, meleetex
                };
                return isweap(stype) && *weaptexs[stype] ? weaptexs[stype] : questiontex;
                break;
            }
            default: break;
        }
        return "";
    }

    const char *geticon(int type, int value)
    {
        switch(type)
        {
            case eventicon::SPREE:
            {
                switch(value)
                {
                    case 0: return spree1tex; break;
                    case 1: return spree2tex; break;
                    case 2: return spree3tex; break;
                    case 3: default: return spree4tex; break;
                }
                break;
            }
            case eventicon::MULTIKILL:
            {
                switch(value)
                {
                    case 0: return multi1tex; break;
                    case 1: return multi2tex; break;
                    case 2: default: return multi3tex; break;
                }
                break;
            }
            case eventicon::HEADSHOT: return headshottex; break;
            case eventicon::DOMINATE: return dominatetex; break;
            case eventicon::REVENGE: return revengetex; break;
            case eventicon::FIRSTBLOOD: return firstbloodtex; break;
            case eventicon::BREAKER: return breakertex; break;
            case eventicon::WEAPON: return itemtex(WEAPON, value);
            case eventicon::AFFINITY:
            {
                if(m_bomber(game::gamemode)) return bombtex;
                if(m_defend(game::gamemode)) return pointtex;
                return flagtex;
            }
        }
        return "";
    }

    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, damagemasktex, "<grey>textures/damage/mask", 0);

    VAR(IDF_PERSIST, showdamage, 0, 1, 1);
    CVAR(IDF_PERSIST, damagecolour, 0x600000);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, damagetex, "<grey>textures/damage/hurt", 0x300);
    FVAR(IDF_PERSIST, damagedistort, 0, 1.85f, 16);
    FVAR(IDF_PERSIST, damageblend, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, damageblenddead, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, damagespeed1, FVAR_MIN, -0.05f, FVAR_MAX);
    FVAR(IDF_PERSIST, damagespeed2, FVAR_MIN, 0.1f, FVAR_MAX);

    VAR(IDF_PERSIST, showdamageburn, 0, 1, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, damageburntex, "<grey>textures/damage/burn", 0x300);
    FVAR(IDF_PERSIST, damageburnbright, 0, 0.9f, 10);
    FVAR(IDF_PERSIST, damageburnblend, 0, 0.6f, 1);
    FVAR(IDF_PERSIST, damageburnspeed1, FVAR_MIN, -0.3f, FVAR_MAX);
    FVAR(IDF_PERSIST, damageburnspeed2, FVAR_MIN, 0.4f, FVAR_MAX);

    VAR(IDF_PERSIST, showdamagebleed, 0, 1, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, damagebleedtex, "<grey>textures/damage/bleed", 0x300);
    FVAR(IDF_PERSIST, damagebleedbright, 0, 0.6f, 10);
    FVAR(IDF_PERSIST, damagebleedblend, 0, 0.6f, 1);
    FVAR(IDF_PERSIST, damagebleedspeed1, FVAR_MIN, -0.025f, FVAR_MAX);
    FVAR(IDF_PERSIST, damagebleedspeed2, FVAR_MIN, 0.05f, FVAR_MAX);

    VAR(IDF_PERSIST, showdamageshock, 0, 1, 1);
    TVAR(IDF_PERSIST|IDF_GAMEPRELOAD, damageshocktex, "<grey>textures/damage/shock", 0x300);
    FVAR(IDF_PERSIST, damageshockbright, 0, 0.9f, 10);
    FVAR(IDF_PERSIST, damageshockblend, 0, 0.5f, 1);
    FVAR(IDF_PERSIST, damageshockspeed1, FVAR_MIN, -0.4f, FVAR_MAX);
    FVAR(IDF_PERSIST, damageshockspeed2, FVAR_MIN, 0.3f, FVAR_MAX);

    void drawdamage(const char *tex, const vec &color, float fade, float speed1, float speed2, float distort = 0.f, float bright = 1.f)
    {
        if(!*damagemasktex || !*tex || !fade) return;
        LOCALPARAMF(time, lastmillis/1000.f);
        LOCALPARAM(speed, vec(speed1, speed2, distort));
        LOCALPARAM(colour, vec(color).mul(bright));
        glActiveTexture_(GL_TEXTURE0);
        settexture(damagemasktex, 0);
        glActiveTexture_(GL_TEXTURE1);
        settexture(tex, 0x300);
        glActiveTexture_(GL_TEXTURE0);
        gle::colorf(1, 1, 1, fade);
        drawquad(0, 0, 1, 1);
    }

    void drawdamages(float blend)
    {
        pushhudmatrix();
        hudmatrix.ortho(0, 1, 1, 0, -1, 1);
        flushhudmatrix();
        SETSHADER(huddamage);
        if(showdamage)
        {
            int hp = max(1, game::focus->gethealth(game::gamemode, game::mutators));
            float pc = game::focus->state == CS_DEAD ? damageblenddead : (game::focus->state == CS_ALIVE ? min(damageresidue, hp)/float(hp)*damageblend : 0.f);
            if(pc > 0) drawdamage(damagetex, damagecolour.tocolor(), pc*blend, damagespeed1, damagespeed2, damagedistort);
        }
        #define RESIDUAL(name, type, pulse) \
            if(showdamage##name && game::focus->name##ing(lastmillis, game::focus->name##time)) \
            { \
                int interval = lastmillis-game::focus->lastres[W_R_##type], delay = max(game::focus->name##delay, 1); \
                float pc = interval >= game::focus->name##time-500 ? 1.f+(interval-(game::focus->name##time-500))/500.f : (interval%delay)/float(delay/2); \
                if(pc > 1.f) pc = 2.f-pc; \
                if(interval < game::focus->name##time-(delay/2)) pc = min(pc+0.5f, 1.f); \
                if(pc > 0) drawdamage(damage##name##tex, game::pulsecolour(game::focus, PULSE_##pulse), pc*blend*damage##name##blend, damage##name##speed1, damage##name##speed2, 0.f, damage##name##bright); \
            }
        RESIDUALSF
        #undef RESIDUAL
        pophudmatrix();
        resethudshader();
    }

    void drawzoom(int w, int h)
    {
        if(!gs_playing(game::gamestate) || game::focus->state != CS_ALIVE || !game::inzoom()) return;
        float pc = game::zoomscale();
        int x = 0, y = 0, c = 0;
        if(w > h)
        {
            float rc = 1.f-pc;
            c = h;
            x += (w-h)/2;
            usetexturing(false);
            drawblend(0, 0, x, c, rc, rc, rc, true);
            drawblend(x+c, 0, x+1, c, rc, rc, rc, true);
            usetexturing(true);
        }
        else if(h > w)
        {
            float rc = 1.f-pc;
            c = w;
            y += (h-w)/2;
            usetexturing(false);
            drawblend(0, 0, c, y, rc, rc, rc, true);
            drawblend(0, y+c, c, y, rc, rc, rc, true);
            usetexturing(true);
        }
        else c = h;
        Texture *t = textureload(zoomtex, 3);
        if(!t || t == notexture) return;
        settexture(t);
        gle::colorf(0, 0, 0, pc);
        drawtexture(x, y, c, c);
    }

    CVAR(IDF_PERSIST, backgroundcolour, 0x000000);
    TVAR(IDF_PERSIST|IDF_PRELOAD, backgroundwatertex, "<grey><noswizzle>textures/water", 0x300);
    TVAR(IDF_PERSIST|IDF_PRELOAD, backgroundcausttex, "<comp>caustic", 0x300);
    TVAR(IDF_PERSIST|IDF_PRELOAD, backgroundtex, "<nocompress>textures/menubg", 3);
    TVAR(IDF_PERSIST|IDF_PRELOAD, backgroundmasktex, "<nocompress>textures/menubg_mask", 3);

    void drawbackground(int w, int h)
    {
        gle::colorf(1, 1, 1, 1);

        Texture *t = NULL;
        if(engineready && showloadingmapbg && *mapname && strcmp(mapname, "maps/untitled"))
        {
            defformatstring(tex, "<blur:2>%s", mapname);
            t = textureload(tex, 3, true, false);
        }
        if(!engineready || !t || t == notexture)
        {
            pushhudmatrix();
            hudmatrix.ortho(0, 1, 1, 0, -1, 1);
            flushhudmatrix();

            t = textureload(backgroundtex, 3);

            if(t)
            {
                if(engineready && hudbackgroundshader)
                {
                    hudbackgroundshader->set();
                    LOCALPARAMF(time, lastmillis/1000.0f);
                    LOCALPARAMF(aspect, hudh/(float)hudw);

                    glActiveTexture_(GL_TEXTURE0);
                    settexture(t);
                    glActiveTexture_(GL_TEXTURE1);
                    settexture(backgroundwatertex, 0x300);
                    glActiveTexture_(GL_TEXTURE2);
                    settexture(backgroundcausttex, 0x300);
                    glActiveTexture_(GL_TEXTURE3);
                    settexture(backgroundmasktex, 3);

                    glActiveTexture_(GL_TEXTURE0);
                }
                else
                {
                    hudshader->set();
                    glActiveTexture_(GL_TEXTURE0);
                    settexture(t);
                }
            }
            else if(hudnotextureshader)
            {
                hudnotextureshader->set();
                gle::color(backgroundcolour.tocolor(), 1.f);
            }
            else nullshader->set();

            float offsetx = 0, offsety = 0;
            if(t)
            {
                // Calculate cropping of the background
                float hudratio = hudh / (float)hudw, bgratio = t->h / (float)t->w;

                if(hudratio < bgratio)
                {
                    float scalex = hudw / (float)t->w;
                    float scaledh = t->h * scalex;
                    float ratioy = hudh / scaledh;
                    offsety = (1.0f - ratioy) * 0.5f;
                }
                else
                {
                    float scaley = hudh / (float)t->h;
                    float scaledw = t->w * scaley;
                    float ratiox = hudw / scaledw;
                    offsetx = (1.0f - ratiox) * 0.5f;
                }
            }

            drawquad(0, 0, 1, 1, offsetx, offsety, 1-offsetx, 1-offsety);
            pophudmatrix();
            resethudshader();
        }
        else
        {
            settexture(t);
            float offsetx = 0, offsety = 0;
            if(showloadingaspect)
            {
                if(w > h) offsety = ((w-h)/float(w))*0.5f;
                else if(h > w) offsetx = ((h-w)/float(h))*0.5f;
            }
            drawquad(0, 0, w, h, offsetx, offsety, 1-offsetx, 1-offsety);
        }

        if(progressing && !engineready)
        {
            if(showloadinglogos)
            {
                gle::colorf(1, 1, 1, 1);

                t = textureload(logotex, 3);
                settexture(t);
                drawtexture(w-w/4-w/3, h/2-w/6, w/2, w/4);
            }

            float oldtextscale = curtextscale;
            pushfont(textfontlogo);
            curtextscale = 0.8f;
            draw_textf("%s", FONTH/2, h-FONTH*5/4, 0, 0, 255, 255, 255, 255, TEXT_LEFT_JUSTIFY, -1, -1, 1, *progresstitle ? progresstitle : "Loading, please wait..");
            if(progressamt > 0) draw_textf("[ %.1f%% ]", w-FONTH/2, h-FONTH*5/4, 0, 0, 255, 255, 255, 255, TEXT_RIGHT_JUSTIFY, -1, -1, 1, progressamt*100);
            curtextscale = oldtextscale;
            popfont();
        }
    }

    ICOMMAND(0, getprogresstitle, "", (),
    {
        if(progressing) result(progresstitle);
        else
        {
            int wait = client::waiting();
            switch(wait)
            {
                case 0: break;
                case 1:
                    if(curpeer || haslocalclients())
                    {
                        if(!client::isready) result("Negotiating with server..");
                        else if(!client::loadedmap) result("Getting game information..");
                        else result("Loading game state..");
                    }
                    else if(connpeer != NULL) result("Connecting to server..");
                    else result("Loading game state..");
                    break;
                case 2:
                    result("Requesting map..");
                    break;
                case 3:
                    result("Downloading map..");
                    break;
                default: break;
            }
        }
    });

    void drawonscreenhits(int w, int h, float blend)
    {
        pushhudscale(onscreenhitsscale);
        float maxy = -1.f;
        loopv(hitlocs)
        {
            dhloc &l = hitlocs[i];
            int millis = totalmillis-l.outtime;
            gameent *a = game::getclient(l.clientnum);
            if(!a || millis >= onscreenhitstime+onscreenhitsfade || l.dir.iszero()) { hitlocs.remove(i--); continue; }
            if(game::focus->state == CS_SPECTATOR || game::focus->state == CS_EDITING) continue;
            if((!onscreenhitsheal && l.damage < 0) || (!onscreenhitsself && a == game::focus)) continue;
            vec o = onscreenhitsfollow ? a->center() : l.dir;
            o.z += actors[a->actortype].height*onscreenhitsheight;
            float cx = 0, cy = 0, cz = 0;
            if(!vectocursor(o, cx, cy, cz)) continue;
            float hx = cx*w/onscreenhitsscale, hy = cy*h/onscreenhitsscale, fade = blend*onscreenhitsblend;
            if(onscreenhitsoffset != 0) hx += FONTW*onscreenhitsoffset;
            if(millis <= onscreenhitstime)
            {
                float amt = millis/float(onscreenhitstime), total = FONTW*onscreenhitsswipe*(1-amt);
                if(onscreenhitsoffset < 0) hx -= total;
                else hx += total;
                fade *= amt;
            }
            else
            {
                int offset = millis-onscreenhitstime;
                hy -= FONTH*offset/float(onscreenhitstime);
                fade *= 1-(offset/float(onscreenhitsfade));
            }
            string text;
            if(game::damageinteger)
                formatstring(text, "%c%d", l.damage > 0 ? '-' : (l.damage < 0 ? '+' : '~'), int(ceilf((l.damage < 0 ? 0-l.damage : l.damage)/game::damagedivisor)));
            else formatstring(text, "%c%.1f", l.damage > 0 ? '-' : (l.damage < 0 ? '+' : '~'), (l.damage < 0 ? 0-l.damage : l.damage)/game::damagedivisor);
            vec colour = l.colour < 0 ? game::pulsecolour(a, INVPULSE(l.colour)) : vec::fromcolor(l.colour);
            if(maxy >= 0 && hy < maxy) hy = maxy;
            if(onscreenhitsglow && settexture(onscreenhitsglowtex))
            {
                float width = 0, height = 0;
                text_boundsf(text, width, height, 0, 0, -1, TEXT_CENTERED, 1);
                gle::colorf(colour.r*onscreenhitsglowcolour, colour.g*onscreenhitsglowcolour, colour.b*onscreenhitsglowcolour, fade*onscreenhitsglowblend);
                drawtexture(hx-(width*onscreenhitsglowscale*0.5f), hy-(height*onscreenhitsglowscale*0.25f), width*onscreenhitsglowscale, height*onscreenhitsglowscale);
            }
            hy += draw_textf("%s", hx, hy, 0, 0, int(colour.r*255), int(colour.g*255), int(colour.b*255), int(fade*255), TEXT_CENTERED, -1, -1, 1, text)/onscreenhitsscale;
            resethudshader();
            if(maxy < 0 || hy > maxy) maxy = hy;
        }
        pophudmatrix();
    }

    void drawonscreendamage(int w, int h, float blend)
    {
        loopv(damagelocs)
        {
            dhloc &l = damagelocs[i];
            gameent *e = game::getclient(l.clientnum);
            if(!e || l.dir.iszero()) { damagelocs.remove(i--); continue; }
            int millis = totalmillis-l.outtime;
            if(millis >= onscreendamagetime+onscreendamagefade) { if(millis >= min(20, l.damage)*50) damagelocs.remove(i--); continue; }
            if(game::focus->state == CS_SPECTATOR || game::focus->state == CS_EDITING) continue;
            if(!onscreendamageself && e == game::focus) continue;
            float amt = millis >= onscreendamagetime ? 1.f-(float(millis-onscreendamagetime)/float(onscreendamagefade)) : float(millis)/float(onscreendamagetime),
                range = clamp(max(l.damage, onscreendamagemin)/float(max(onscreendamagemax-onscreendamagemin, 1)), onscreendamagemin/100.f, 1.f),
                fade = clamp(onscreendamageblend*blend, min(onscreendamageblend*onscreendamagemin/100.f, 1.f), onscreendamageblend)*amt,
                size = clamp(range*onscreendamagesize, min(onscreendamagesize*onscreendamagemin/100.f, 1.f), onscreendamagesize)*amt;
            vec dir = l.dir, colour = l.colour < 0 ? game::pulsecolour(game::focus, INVPULSE(l.colour)) : vec::fromcolor(l.colour);
            if(e == game::focus) l.dir = vec(e->yaw*RAD, 0.f).neg();
            dir.rotate_around_z(-camera1->yaw*RAD).normalize();
            float yaw = -atan2(dir.x, dir.y)/RAD, x = sinf(RAD*yaw), y = -cosf(RAD*yaw), sz = max(w, h)/2,
                  ts = sz*onscreendamagescale, tp = ts*size, tq = tp*onscreendamageblipsize, tr = ts*onscreendamageoffset, lx = (tr*x)+w/2, ly = (tr*y)+h/2;
            gle::color(colour, fade);
            Texture *t = textureload(hurttex, 3);
            if(t != notexture)
            {
                settexture(t);
                gle::defvertex(2);
                gle::deftexcoord0();
                gle::begin(GL_TRIANGLE_STRIP);
                vec2 o(lx, ly);
                loopk(4)
                {
                    vec2 norm, tc;
                    switch(k)
                    {
                        case 0: vecfromyaw(yaw, 1, -1, norm);   tc = vec2(0, 1); break;
                        case 1: vecfromyaw(yaw, 1, 1, norm);    tc = vec2(1, 1); break;
                        case 2: vecfromyaw(yaw, -1, -1, norm);  tc = vec2(0, 0); break;
                        case 3: vecfromyaw(yaw, -1, 1, norm);   tc = vec2(1, 0); break;
                    }
                    norm.mul(tq).add(o);
                    gle::attrib(norm);
                    gle::attrib(tc);
                }
                gle::end();
            }
        }
    }

    void render(bool noview)
    {
        int wait = client::waiting();
        float fade = hudblend;
        hudmatrix.ortho(0, hudwidth, hudheight, 0, -1, 1);
        flushhudmatrix();
        if(!progressing && !wait && engineready)
        {
            vec colour = vec(1, 1, 1);
            if(compassfade && (compassmillis > 0 || totalmillis-abs(compassmillis) <= compassfade))
            {
                float a = min(float(totalmillis-abs(compassmillis))/float(compassfade), 1.f)*compassfadeamt;
                if(compassmillis > 0) a = 1.f-a;
                else a += (1.f-compassfadeamt);
                loopi(3) if(a < colour[i]) colour[i] *= a;
            }
            if(!noview)
            {
                if(titlefade && (!game::mapstart || totalmillis-game::mapstart <= titlefade))
                {
                    float a = game::mapstart ? float(totalmillis-game::mapstart)/float(titlefade) : 0.f;
                    loopi(3) if(a < colour[i]) colour[i] *= a;
                }
                if(tvmodefade && game::tvmode())
                {
                    float a = game::lasttvchg ? (totalmillis-game::lasttvchg <= tvmodefade ? float(totalmillis-game::lasttvchg)/float(tvmodefade) : 1.f) : 0.f;
                    loopi(3) if(a < colour[i]) colour[i] *= a;
                }
                if((game::focus == game::player1 || !game::thirdpersonview(true)) && (spawnfade && game::focus->state == CS_ALIVE && game::focus->lastspawn && lastmillis-game::focus->lastspawn <= spawnfade))
                {
                    float a = (lastmillis-game::focus->lastspawn)/float(spawnfade/3);
                    if(a < 3.f)
                    {
                        vec col = vec(1, 1, 1);
                        skewcolour(col.x, col.y, col.z, game::getcolour(game::focus, game::playereffecttone, game::playereffecttonelevel));
                        if(a < 1.f) { loopi(3) col[i] *= a; }
                        else { a = (a-1.f)*0.5f; loopi(3) col[i] += (1.f-col[i])*a; }
                        loopi(3) if(col[i] < colour[i]) colour[i] *= col[i];
                    }
                }
            }
            if(colour.x < 1 || colour.y < 1 || colour.z < 1)
            {
                usetexturing(false);
                drawblend(0, 0, hudwidth, hudheight, colour.x, colour.y, colour.z);
                usetexturing(true);
                fade *= (colour.x+colour.y+colour.z)/3.f;
            }
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        resethudshader();
        if(noview || wait) drawbackground(hudwidth, hudheight);
        else if(engineready)
        {
            drawzoom(hudwidth, hudheight);
            if(showhud)
            {
                if(gs_playing(game::gamestate))
                {
                    drawdamages(fade);
                    if(teamhurthud&2 && teamhurttime && m_team(game::gamemode, game::mutators) && game::focus == game::player1 && game::player1->lastteamhit >= 0 && totalmillis-game::player1->lastteamhit <= teamhurttime)
                    {
                        vec targ;
                        bool hasbound = false;
                        int dist = teamhurtdist ? teamhurtdist : worldsize;
                        loopv(game::players) if(game::players[i] && game::players[i]->team == game::player1->team)
                        {
                            if(game::players[i]->lastteamhit < 0 || lastmillis-game::players[i]->lastteamhit > teamhurttime) continue;
                            if(!getsight(camera1->o, camera1->yaw, camera1->pitch, game::players[i]->o, targ, dist, curfov, fovy)) continue;
                            if(!hasbound)
                            {
                                Texture *t = textureload(warningtex, 3);
                                settexture(t);
                                float amt = float(totalmillis%250)/250.f, value = (amt > 0.5f ? 1.f-amt : amt)*2.f;
                                gle::colorf(value, value*0.125f, value*0.125f, value);
                                hasbound = true;
                            }
                            float cx = 0.5f, cy = 0.5f, cz = 1;
                            if(vectocursor(game::players[i]->o, cx, cy, cz))
                            {
                                int s = int(teamhurtsize*hudwidth), sx = int(cx*hudwidth-s), sy = int(cy*hudheight-s);
                                drawsized(sx, sy, s*2);
                            }
                        }
                    }
                    if(!hasinput(true))
                    {
                        if(onscreenhits) drawonscreenhits(hudwidth, hudheight, fade);
                        if(onscreendamage) drawonscreendamage(hudwidth, hudheight, fade);
                        if(m_capture(game::gamemode)) capture::drawonscreen(hudwidth, hudheight, fade);
                        else if(m_defend(game::gamemode)) defend::drawonscreen(hudwidth, hudheight, fade);
                        else if(m_bomber(game::gamemode)) bomber::drawonscreen(hudwidth, hudheight, fade);
                    }
                }
                if(!game::tvmode() && !client::waiting() && !hasinput(false)) drawevents(fade);
            }
        }
        if(engineready)
        {
            if(!progressing)
            {
                if(showhud && commandmillis <= 0 && curcompass) rendercmenu();
                else UI::render();
                hudmatrix.ortho(0, hudwidth, hudheight, 0, -1, 1);
                flushhudmatrix();
                resethudshader();
                drawpointers(hudwidth, hudheight);
            }
            else UI::render(true);
        }
        glDisable(GL_BLEND);
    }

    void update(int w, int h)
    {
        aspect = forceaspect ? forceaspect : w/float(h);
        fovy = 2*atan2(tan(curfov/2*RAD), aspect)/RAD;
        if(aspect > 1)
        {
            hudheight = hudsize;
            hudwidth = int(ceil(hudsize*aspect));
        }
        else if(aspect < 1)
        {
            hudwidth = hudsize;
            hudheight = int(ceil(hudsize/aspect));
        }
        else hudwidth = hudheight = hudsize;
    }

    void cleanup()
    {
        teamkills.shrink(0);
        damagelocs.shrink(0);
        hitlocs.shrink(0);
        events.shrink(0);
        damageresidue = lastteam = 0;
    }
}
