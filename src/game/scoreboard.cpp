#include "game.h"

namespace hud
{
    static vector<score> scores;

    score &teamscore(int team)
    {
        loopv(scores)
        {
            score &cs = scores[i];
            if(cs.team == team) return cs;
        }
        score &cs = scores.add();
        cs.team = team;
        cs.total = 0;
        return cs;
    }

    void resetscores()
    {
        scores.shrink(0);
    }

    struct scoregroup : score
    {
        vector<gameent *> players;
    };
    vector<scoregroup *> groups;
    scoregroup spectators;

    VAR(IDF_PERSIST, autoscores, 0, 1, 3); // 1 = when dead, 2 = also in spectv, 3 = and in waittv too
    VAR(IDF_PERSIST, scoresdelay, 0, 0, VAR_MAX); // otherwise use respawn delay
    VAR(IDF_PERSIST, scoresinfo, 0, 1, 1);
    VAR(IDF_PERSIST, scorehandles, 0, 1, 1);

    VAR(IDF_PERSIST, scorepj, 0, 0, 1);
    VAR(IDF_PERSIST, scoreping, 0, 1, 1);
    VAR(IDF_PERSIST, scorepoints, 0, 1, 2);
    VAR(IDF_PERSIST, scoretimer, 0, 1, 2);
    VAR(IDF_PERSIST, scorefrags, 0, 2, 2);
    VAR(IDF_PERSIST, scoredeaths, 0, 2, 2);
    VAR(IDF_PERSIST, scoreratios, 0, 0, 2);
    VAR(IDF_PERSIST, scoreclientnum, 0, 1, 1);
    VAR(IDF_PERSIST, scoretimestyle, 0, 3, 4);
    VAR(IDF_PERSIST, scoreracestyle, 0, 1, 4);
    VAR(IDF_PERSIST, scorebotinfo, 0, 0, 1);
    VAR(IDF_PERSIST, scorespectators, 0, 1, 1);
    VAR(IDF_PERSIST, scoreconnecting, 0, 0, 1);
    VAR(IDF_PERSIST, scorehostinfo, 0, 0, 1);
    VAR(IDF_PERSIST, scoreipinfo, 0, 0, 1);
    VAR(IDF_PERSIST, scoreverinfo, 0, 0, 1);
    VAR(IDF_PERSIST, scoreicons, 0, 1, 1);
    VAR(IDF_PERSIST|IDF_HEX, scorehilight, 0, 0xFFFFFF, 0xFFFFFF);
    VAR(IDF_PERSIST, scoredarken, 0, 1, 1);
    VAR(IDF_PERSIST, scoreimage, 0, 1, 1);
    FVAR(IDF_PERSIST, scoreimagesize, FVAR_NONZERO, 6, 10);
    VAR(IDF_PERSIST, scoresideinfo, 0, 1, 1);
    VAR(IDF_PERSIST, scorebgfx, 0, 1, 1);
    VAR(IDF_PERSIST, scorebgrows, 0, 2, 2);
    VAR(IDF_PERSIST, scorebgborder, 0, 1, 1);
    FVAR(IDF_PERSIST, scorebgblend, 0, 0.5f, 1);

    static bool scoreson = false, scoresoff = false, shownscores = false;
    static int menustart = 0, menulastpress = 0;

    bool canshowscores()
    {
        if(!scoresoff && !scoreson && !shownscores && autoscores)
        {
            if(game::player1->state == CS_DEAD)
            {
                int delay = scoresdelay ? scoresdelay : m_delay(game::player1->actortype, game::gamemode, game::mutators, game::player1->team);
                if(!delay || lastmillis-game::player1->lastdeath > delay) return true;
            }
            else return game::tvmode() && autoscores >= (game::player1->state == CS_SPECTATOR ? 2 : 3);
        }
        return false;
    }

    static inline bool playersort(const gameent *a, const gameent *b)
    {
        if(a->state == CS_SPECTATOR || a->state == CS_EDITING)
        {
            if(b->state == CS_SPECTATOR || b->state == CS_EDITING) return strcmp(a->name, b->name) < 0;
            else return false;
        }
        else if(b->state == CS_SPECTATOR || b->state == CS_EDITING) return true;
        if(m_laptime(game::gamemode, game::mutators))
        {
            if((a->cptime && !b->cptime) || (a->cptime && b->cptime && a->cptime < b->cptime)) return true;
            if((b->cptime && !a->cptime) || (a->cptime && b->cptime && b->cptime < a->cptime)) return false;
        }
        if(a->points > b->points) return true;
        if(a->points < b->points) return false;
        if(!m_race(game::gamemode))
        {
            if(a->frags > b->frags) return true;
            if(a->frags < b->frags) return false;
        }
        return strcmp(a->name, b->name) < 0;
    }

    static inline bool scoregroupcmp(const scoregroup *x, const scoregroup *y)
    {
        if(!x->team)
        {
            if(y->team) return false;
        }
        else if(!y->team) return true;
        if(m_laptime(game::gamemode, game::mutators))
        {
            if((x->total && !y->total) || (x->total && y->total && x->total < y->total)) return true;
            if((y->total && !x->total) || (x->total && y->total && x->total > y->total)) return false;
        }
        else
        {
            if(x->total > y->total) return true;
            if(x->total < y->total) return false;
        }
        if(x->players.length() > y->players.length()) return true;
        if(x->players.length() < y->players.length()) return false;
        return x->team && y->team && x->team < y->team;
    }

    int groupplayers()
    {
        int numgroups = 0;
        spectators.players.shrink(0);
        int numdyns = game::numdynents();
        loopi(numdyns)
        {
            gameent *o = (gameent *)game::iterdynents(i);
            if(!o || o->actortype >= A_ENEMY || (!scoreconnecting && !o->name[0])) continue;
            if(o->state == CS_SPECTATOR)
            {
                if(o != game::player1 || !client::demoplayback) spectators.players.add(o);
                continue;
            }
            int team = m_play(game::gamemode) && m_team(game::gamemode, game::mutators) ? o->team : T_NEUTRAL;
            bool found = false;
            loopj(numgroups)
            {
                scoregroup &g = *groups[j];
                if(team != g.team) continue;
                if(team) g.total = teamscore(team).total;
                g.players.add(o);
                found = true;
                break;
            }
            if(found) continue;
            if(numgroups >= groups.length()) groups.add(new scoregroup);
            scoregroup &g = *groups[numgroups++];
            g.team = team;
            if(!team) g.total = 0;
            else if(m_team(game::gamemode, game::mutators)) g.total = teamscore(o->team).total;
            else g.total = o->points;

            g.players.shrink(0);
            g.players.add(o);
        }
        loopi(numgroups) groups[i]->players.sort(playersort);
        spectators.players.sort(playersort);
        groups.sort(scoregroupcmp, 0, numgroups);
        return numgroups;
    }

    void showscores(bool on, bool interm, bool onauto, bool ispress)
    {
        if(!client::waiting())
        {
            if(ispress)
            {
                bool within = menulastpress && totalmillis-menulastpress < PHYSMILLIS;
                if(on)
                {
                    if(within) onauto = true;
                    menulastpress = totalmillis;
                }
                else if(within && !scoresoff) { menulastpress = 0; return; }
            }
            if(!scoreson && on) menustart = guicb::starttime();
            scoresoff = !onauto;
            scoreson = on;
            if(m_play(game::gamemode) && m_play(game::gamemode) && interm)
            {
                int numgroups = groupplayers();
                if(!numgroups) return;
                scoregroup &sg = *groups[0];
                if(m_team(game::gamemode, game::mutators))
                {
                    int anc = sg.players.find(game::player1) >= 0 ? S_V_YOUWIN : (game::player1->state != CS_SPECTATOR ? S_V_YOULOSE : -1);
                    if(m_defend(game::gamemode) && sg.total == INT_MAX)
                        game::announcef(anc, CON_MESG, NULL, true, "\fwteam %s secured all flags", game::colourteam(sg.team));
                    else
                    {
                        if(numgroups > 1 && sg.total == groups[1]->total)
                        {
                            string winner = "";
                            loopi(numgroups) if(i)
                            {
                                if(sg.total == groups[i]->total)
                                {
                                    defformatstring(tw, "%s, ", game::colourteam(groups[i]->team));
                                    concatstring(winner, tw);
                                }
                                else break;
                            }
                            game::announcef(S_V_DRAW, CON_MESG, NULL, true, "\fw%s tied %swith a total score of \fs\fc%s\fS", game::colourteam(sg.team), winner, m_laptime(game::gamemode, game::mutators) ? timestr(sg.total, scoreracestyle) : intstr(sg.total));
                        }
                        else game::announcef(anc, CON_MESG, NULL, true, "\fwteam %s won the match with a total score of \fs\fc%s\fS", game::colourteam(sg.team), m_laptime(game::gamemode, game::mutators) ? timestr(sg.total, scoreracestyle) : intstr(sg.total));
                    }
                }
                else
                {
                    int anc = sg.players[0] == game::player1 ? S_V_YOUWIN : (game::player1->state != CS_SPECTATOR ? S_V_YOULOSE : -1);
                    if(m_laptime(game::gamemode, game::mutators))
                    {
                        if(sg.players.length() > 1 && sg.players[0]->cptime == sg.players[1]->cptime)
                        {
                            string winner = "";
                            loopv(sg.players) if(i)
                            {
                                if(sg.players[0]->cptime == sg.players[i]->cptime)
                                {
                                    concatstring(winner, game::colourname(sg.players[i]));
                                    concatstring(winner, ", ");
                                }
                                else break;
                            }
                            game::announcef(S_V_DRAW, CON_MESG, NULL, true, "\fw%s tied %swith the fastest lap \fs\fc%s\fS", game::colourname(sg.players[0]), winner, sg.players[0]->cptime ? timestr(sg.players[0]->cptime, scoreracestyle) : "dnf");
                        }
                        else game::announcef(anc, CON_MESG, NULL, true, "\fw%s won the match with the fastest lap \fs\fc%s\fS", game::colourname(sg.players[0]), sg.players[0]->cptime ? timestr(sg.players[0]->cptime, scoreracestyle) : "dnf");
                    }
                    else
                    {
                        if(sg.players.length() > 1 && sg.players[0]->points == sg.players[1]->points)
                        {
                            string winner = "";
                            loopv(sg.players) if(i)
                            {
                                if(sg.players[0]->points == sg.players[i]->points)
                                {
                                    concatstring(winner, game::colourname(sg.players[i]));
                                    concatstring(winner, ", ");
                                }
                                else break;
                            }
                            game::announcef(S_V_DRAW, CON_MESG, NULL, true, "\fw%s tied %swith a total score of \fs\fc%d\fS", game::colourname(sg.players[0]), winner, sg.players[0]->points);
                        }
                        else game::announcef(anc, CON_MESG, NULL, true, "\fw%s won the match with a total score of \fs\fc%d\fS", game::colourname(sg.players[0]), sg.players[0]->points);
                    }
                }
            }
        }
        else scoresoff = scoreson = false;
    }

    const char *scorehost(gameent *d, bool hostname = true)
    {
        if(hostname && d->actortype > A_PLAYER)
        {
            static string hoststr;
            hoststr[0] = '\0';
            gameent *e = game::getclient(d->ownernum);
            if(e)
            {
                concatstring(hoststr, game::colourname(e, NULL, false, false));
                concatstring(hoststr, " ");
            }
            defformatstring(owner, "[%d]", d->ownernum);
            concatstring(hoststr, owner);
            return hoststr;
        }
        return hostname ? d->hostname : d->hostip;
    }

    const char *scoreversion(gameent *d)
    {
        static string verstr;
        formatstring(verstr, "%d.%d.%d-%s%d-%s", d->version.major, d->version.minor, d->version.patch, plat_name(d->version.platform), d->version.arch, d->version.branch);
        return verstr;
    }

    void renderscoreboard(guient &g, bool firstpass)
    {
        g.start(menustart, NULL, false, false, scorebgfx!=0);
        int numgroups = groupplayers();
        uilist(g, {
            if(scoresideinfo)
            {
                uicenterlist(g, {
                    g.pushlist();
                    g.space(0.5f);
                    g.pushlist();
                    g.space(0.25f);
                    uicenter(g, {
                        uicenterlist(g, uicenterlist(g, {
                            uicenterlist(g, uifont(g, "emphasis", g.textf("%s", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, *maptitle ? maptitle : mapname)));
                            if(*mapauthor)
                            {
                                int len = strlen(mapauthor);
                                uicenterlist(g, uifont(g, (len >= 24 ? (len >= 40 ? "tiny" : "little") : "reduced"), g.textf("by %s", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, mapauthor)));
                            }
                            uicenterlist(g, uifont(g, "little", {
                                g.textf("\fy%s", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, server::gamename(game::gamemode, game::mutators, 0, 32));
                                if(paused) g.text(", \fs\fopaused\fS", 0xFFFFFF);
                                else if(m_play(game::gamemode) || client::demoplayback)
                                {
                                    int timecorrected = max(game::timeremaining*1000-((gs_playing(game::gamestate) ? lastmillis : totalmillis)-game::lasttimeremain), 0);
                                    if(game::gamestate != G_S_PLAYING)
                                        g.textf(", \fs\fc%s\fS, \fs%s%s\fS remain", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, gamestates[0][game::gamestate], gs_waiting(game::gamestate) ? "\fr" : (game::gamestate == G_S_OVERTIME ? "\fzoy" : "\fg"), timestr(timecorrected, scoretimestyle));
                                    else if(timelimit) g.textf(", \fs\fc%s\fS, \fs%s%s\fS remain", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, gamestates[0][game::gamestate], timecorrected > 60000 ? "\fg" : "\fzgy", timestr(timecorrected, scoretimestyle));
                                }
                            }));
                            if(*connectname)
                            {
                                uicenterlist(g, uifont(g, "little", {
                                    g.textf("\faon: \fw%s:[%d]", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, connectname, connectport);
                                }));
                                if(*serverdesc)
                                {
                                    uicenterlist(g, uifont(g, (strlen(serverdesc) >= 32 ? "tiny" : "little"), {
                                        g.textf("\"\fs%s\fS\"", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, serverdesc);
                                    }));
                                }
                            }
                        }));
                        if(scoreimage)
                        {
                            g.space(0.25f);
                            uicenterlist(g, {
                                g.image(NULL, scoreimagesize, true);
                            });
                        }
                        g.space(0.25f);
                        uicenterlist(g, uicenterlist(g, {
                            if(game::player1->quarantine)
                            {
                                uicenterlist(g, uifont(g, "default", g.text("You are \fzoyQUARANTINED", 0xFFFFFF)));
                                uicenterlist(g, uifont(g, "reduced", g.text("Please await instructions from a moderator", 0xFFFFFF)));
                            }
                            if(client::demoplayback)
                            {
                                uicenterlist(g, {
                                    uifont(g, "default", g.text("Demo Playback in Progress", 0xFFFFFF));
                                });
                            }
                            if(gs_playing(game::gamestate) && !client::demoplayback)
                            {
                                if(game::player1->state == CS_DEAD || game::player1->state == CS_WAITING)
                                {
                                    int sdelay = m_delay(game::player1->actortype, game::gamemode, game::mutators, game::player1->team);
                                    int delay = game::player1->respawnwait(lastmillis, sdelay);
                                    if(delay || m_duke(game::gamemode, game::mutators) || (m_play(game::gamemode) && maxalive > 0))
                                    {
                                        uicenterlist(g, uifont(g, "default", {
                                            if(gs_waiting(game::gamestate) || m_duke(game::gamemode, game::mutators)) g.text("Queued for new round", 0xFFFFFF);
                                            else if(delay) g.textf("%s: Down for \fs\fy%s\fS", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, game::player1->state == CS_WAITING ? "Please Wait" : "Fragged", timestr(delay));
                                            else if(game::player1->state == CS_WAITING && m_play(game::gamemode) && maxalive > 0 && maxalivequeue)
                                            {
                                                if(game::player1->queuepos) g.textf("Waiting for \fs\fy%d\fS %s", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, game::player1->queuepos, game::player1->queuepos != 1 ? "players" : "player");
                                                else g.text("You are \fs\fgnext\fS in the queue", 0xFFFFFF);
                                            }
                                        }));
                                        if(game::player1->state != CS_WAITING && lastmillis-game::player1->lastdeath >= 500)
                                            uicenterlist(g, uifont(g, "little", g.textf("Press \f{=primary} to enter respawn queue", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF)));
                                    }
                                    else
                                    {
                                        uicenterlist(g, uifont(g, "default", g.text("Ready to respawn", 0xFFFFFF)));
                                        if(game::player1->state != CS_WAITING) uicenterlist(g, uifont(g, "little", g.textf("Press \f{=primary} to respawn now", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF)));
                                    }
                                    if(shownotices >= 2)
                                    {
                                        uifont(g, "little", {
                                            if(game::player1->state == CS_WAITING && lastmillis-game::player1->lastdeath >= 500)
                                                uicenterlist(g, g.textf("Press \f{=3:waitmodeswitch} to %s", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, game::tvmode() ? "interact" : "switch to TV"));
                                            if(m_loadout(game::gamemode, game::mutators))
                                                uicenterlist(g, g.textf("Press \f{=showgui profile 2} to \fs%s\fS loadout", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, game::player1->loadweap.empty() ? "\fzoyselect" : "change"));
                                            if(m_play(game::gamemode) && m_team(game::gamemode, game::mutators))
                                                uicenterlist(g, g.textf("Press \f{=showgui team} to change teams", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF));
                                        });
                                    }
                                }
                                else if(game::player1->state == CS_ALIVE)
                                {
                                    uifont(g, "default", uicenterlist(g, {
                                        if(m_team(game::gamemode, game::mutators))
                                            g.textf("Playing for team %s", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, game::colourteam(game::player1->team));
                                        else g.text("Playing free-for-all", 0xFFFFFF);
                                    }));
                                }
                                else if(game::player1->state == CS_SPECTATOR)
                                {
                                    uifont(g, "default", uicenterlist(g, {
                                        g.textf("You are %s", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, game::tvmode() ? "watching SpecTV" : "a spectator");
                                    }));
                                    uifont(g, "little", {
                                        uicenterlist(g, g.textf("Press \f{=1:spectator 0} to join the game", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF));
                                        if(!m_edit(game::gamemode) && shownotices >= 2)
                                            uicenterlist(g, g.textf("Press \f{=1:specmodeswitch} to %s", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, game::tvmode() ? "interact" : "switch to TV"));
                                    });
                                }

                                if(m_edit(game::gamemode) && (game::player1->state != CS_EDITING || shownotices >= 4))
                                    uicenterlist(g, uifont(g, "reduced", g.textf("Press \f{=1:edittoggle} to %s editmode", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, game::player1->state != CS_EDITING ? "enter" : "exit")));
                            }

                            uicenterlist(g, uifont(g, "little", g.textf("%s \f{=1:showscores} to close this window", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, scoresoff ? "Release" : "Press")));
                            uicenterlist(g, uifont(g, "tiny", g.text("Double-tap to keep the window open", 0xFFFFFF)));

                            if(m_play(game::gamemode) && game::player1->state != CS_SPECTATOR && (!gs_playing(game::gamestate) || scoresinfo))
                            {
                                float ratio = game::player1->frags >= game::player1->deaths ? (game::player1->frags/float(max(game::player1->deaths, 1))) : -(game::player1->deaths/float(max(game::player1->frags, 1)));
                                uicenterlist(g, uifont(g, "little", {
                                    g.textf("\fs\fg%d\fS %s, \fs\fg%d\fS %s (\fs\fy%.1f\fS:\fs\fy%.1f\fS) \fs\fg%d\fS damage", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF,
                                        game::player1->frags, game::player1->frags != 1 ? "frags" : "frag",
                                        game::player1->deaths, game::player1->deaths != 1 ? "deaths" : "death", ratio >= 0 ? ratio : 1.f, ratio >= 0 ? 1.f : -ratio,
                                        game::player1->totaldamage);
                                }));
                            }
                        }));
                    });
                    g.space(0.25f);
                    g.poplist();
                    g.space(0.5f);
                    g.poplist();
                });
                g.space(1);
            }
            uicenterlist(g, {
                uicenter(g, {
                    int ngroup = numgroups;
                    if(scorespectators && !spectators.players.empty()) ngroup++;
                    #define loopscorelist(b) \
                    { \
                        int _n = sg.players.length(); \
                        loopi(_n) if(sg.players[i]) \
                        { \
                            b; \
                        } \
                    }
                    #define loopscoregroup(b) \
                    { \
                        loopv(sg.players) if(sg.players[i]) \
                        { \
                            gameent *o = sg.players[i]; \
                            b; \
                        } \
                    }
                    uifont(g, "reduced", {
                        float namepad = 0;
                        float handlepad = 0;
                        float ippad = 0;
                        float hostpad = 0;
                        float verpad = 0;
                        bool hashandle = false;
                        bool hasip = false;
                        bool hashost = false;
                        bool hasver = false;
                        bool hasbots = false;
                        loopk(ngroup)
                        {
                            scoregroup &sg = k == numgroups ? spectators : *groups[k];
                            loopscoregroup({
                                if(scorebotinfo && o->actortype > A_PLAYER) hasbots = true;
                                namepad = max(namepad, (float)text_width(game::colourname(o, NULL, false, true))/FONTW*0.51f);
                                if(scorehandles && o->handle[0])
                                {
                                    handlepad = max(handlepad, (float)text_width(o->handle)/FONTW*0.51f);
                                    hashandle = true;
                                }
                                if(scoreipinfo)
                                {
                                    const char *host = scorehost(o, false);
                                    if(host && *host)
                                    {
                                        ippad = max(ippad, (float)text_width(host)/FONTW*0.51f);
                                        if(o->ownernum != game::player1->clientnum) hasip = true;
                                    }
                                }
                                if(scorehostinfo)
                                {
                                    const char *host = scorehost(o, true);
                                    if(host && *host)
                                    {
                                        hostpad = max(hostpad, (float)text_width(host)/FONTW*0.51f);
                                        if(o->ownernum != game::player1->clientnum) hashost = true;
                                    }
                                }
                                if(scoreverinfo)
                                {
                                    const char *ver = scoreversion(o);
                                    if(ver && *ver)
                                    {
                                        verpad = max(verpad, (float)text_width(ver)/FONTW*0.51f);
                                        if(o->ownernum != game::player1->clientnum) hasver = true;
                                    }
                                }
                            });
                        }
                        loopk(ngroup)
                        {
                            scoregroup &sg = k == numgroups ? spectators : *groups[k];
                            if(k) g.space(0.5f);
                            int colour = k == numgroups ? 0x404040 : (sg.team >= 0 && m_team(game::gamemode, game::mutators) ? TEAM(sg.team, colour) : TEAM(T_NEUTRAL, colour));
                            vec c = vec::hexcolor(colour);
                            int bgcolor = vec(c).mul(0.65f).tohexcolor();
                            int bgc1 = vec(c).mul(0.25f).tohexcolor();
                            int bgc2 = vec(c).mul(0.125f).tohexcolor();
                            #define ownerfgc (scoredarken && (o->state != CS_ALIVE && o->state != CS_EDITING) ? 0x707070 : 0xFFFFFF)
                            #define ownerbgc (i%2 ? bgc2 : bgc1)
                            #define ownerbgch (scorehilight && o == game::player1 ? scorehilight : (ownerbgc))
                            #define ownerbg if((scorehilight && o == game::player1) || scorebgrows >= 2) g.background(ownerbgc, scorebgblend, ownerbgch, scorebgblend, scorehilight && o == game::player1);
                            uicenterlist(g, {
                                if(scorebgrows) g.background(bgcolor, scorebgblend, bgcolor, scorebgblend);
                                g.space(0.5f);
                                g.pushlist();
                                g.space(0.25f);
                                uilist(g, uifont(g, "default", {
                                    if(scorebgrows) g.background(bgc2, scorebgblend, bgc2, scorebgblend);
                                    g.space(0.15f);
                                    if(k == numgroups)
                                    {
                                        g.text("spectators", 0xFFFFFF, spectatortex, colour);
                                        g.spring();
                                    }
                                    else if(sg.team > 0 && m_team(game::gamemode, game::mutators))
                                    {
                                        g.textf("team %s", 0xFFFFFF, teamtexname(sg.team), colour, -1, false, NULL, 0xFFFFFF, TEAM(sg.team, name));
                                        g.spring();
                                        if(m_defend(game::gamemode) && ((defendlimit && sg.total >= defendlimit) || sg.total == INT_MAX)) g.text("WINNER", 0xFFFFFF);
                                        else if(m_laptime(game::gamemode, game::mutators)) g.textf("%s", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, sg.total ? timestr(sg.total, scoreracestyle) : "\fadnf");
                                        else if(m_race(game::gamemode)) g.textf("%d %s", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, sg.total, sg.total != 1 ? "laps" : "lap");
                                        else g.textf("%d %s", 0xFFFFFF, NULL, 0, -1, false, NULL, 0xFFFFFF, sg.total, sg.total != 1 ? "points" : "point");
                                    }
                                    else
                                    {
                                        g.text("free-for-all", 0xFFFFFF, playertex, colour);
                                        g.spring();
                                    }
                                    g.space(0.25f);
                                }));
                                g.pushlist();
                                uilist(g, {
                                    uilist(g, {
                                        uicenter(g, uipad(g, 0.25f, g.space(1); g.strut(1)));
                                    });
                                    loopscoregroup(uilist(g, {
                                        uicenter(g, uipad(g, 0.25f, uicenterlist(g, g.textf("\f($priv%stex)", game::findcolour(o), NULL, 0, -1, false, NULL, 0xFFFFFF, server::privnamex(o->privilege, o->actortype, true)))));
                                    }));
                                });

                                uilist(g, {
                                    uilist(g, {
                                        uicenter(g, uipad(g, namepad, uicenterlist(g, g.text("name", 0xFFFFFF))));
                                    });
                                    loopscoregroup(uilist(g, {
                                        ownerbg;
                                        uicenter(g, uipad(g, 0.25f, uicenterlist(g, g.textf("%s", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, game::colourname(o, NULL, false, true, scorebgrows >= 2 || (scorehilight && o == game::player1) ? 0 : 3)))));
                                    }));
                                });

                                if(scorepoints >= (m_laptime(game::gamemode, game::mutators) ? 2 : 1))
                                {
                                    uilist(g, {
                                        uilist(g, {
                                            uicenter(g, uipad(g, 1, g.text("points", 0xFFFFFF)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            ownerbg;
                                            uicenter(g, uipad(g, 0.5f, g.textf("%d", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, o->points)));
                                        }));
                                    });
                                }

                                if(m_race(game::gamemode))
                                {
                                    if(scoretimer && (scoretimer >= 2 || m_laptime(game::gamemode, game::mutators)))
                                    {
                                        uilist(g, {
                                            uilist(g, {
                                                uicenter(g, uipad(g, 4, g.text("best", 0xFFFFFF)));
                                            });
                                            loopscoregroup(uilist(g, {
                                                ownerbg;
                                                uicenter(g, uipad(g, 0.5f, g.textf("%s", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, o->cptime ? timestr(o->cptime, scoreracestyle) : "\fadnf")));
                                            }));
                                        });
                                    }
                                }

                                if(scorefrags >= (!m_dm(game::gamemode) ? 2 : 1))
                                {
                                    uilist(g, {
                                        uilist(g, {
                                            uicenter(g, uipad(g, 1, g.text("frags", 0xFFFFFF)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            ownerbg;
                                            uicenter(g, uipad(g, 0.5f, g.textf("%d", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, o->frags)));
                                        }));
                                    });
                                }

                                if(scoredeaths >= (!m_dm(game::gamemode) ? 2 : 1))
                                {
                                    uilist(g, {
                                        uilist(g, {
                                            uicenter(g, uipad(g, 1, g.text("deaths", 0xFFFFFF)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            ownerbg;
                                            uicenter(g, uipad(g, 0.5f, g.textf("%d", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, o->deaths)));
                                        }));
                                    });
                                }

                                if(scoreratios >= (!m_dm(game::gamemode) ? 2 : 1))
                                {
                                    uilist(g, {
                                        uilist(g, {
                                            uicenter(g, uipad(g, 1, g.text("ratio", 0xFFFFFF)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            ownerbg;
                                            float ratio = o->frags >= o->deaths ? (o->frags/float(max(o->deaths, 1))) : -(o->deaths/float(max(o->frags, 1)));
                                            uicenter(g, uipad(g, 0.5f, g.textf("%.1f\fs\fa:\fS%.1f", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, ratio >= 0 ? ratio : 1.f, ratio >= 0 ? 1.f : -ratio)));
                                        }));
                                    });
                                }

                                if(scorepj)
                                {
                                    uilist(g, {
                                        uilist(g, {
                                            uicenter(g, uipad(g, 2, g.text("pj", 0xFFFFFF)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            ownerbg;
                                            uicenter(g, uipad(g, 0.5f, g.textf("%d", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, o->plag)));
                                        }));
                                    });
                                }

                                if(scoreping)
                                {
                                    uilist(g, {
                                        uilist(g, {
                                            uicenter(g, uipad(g, 2, g.text("ping", 0xFFFFFF)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            ownerbg;
                                            uicenter(g, uipad(g, 0.5f, g.textf("%d", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, o->ping)));
                                        }));
                                    });
                                }

                                if(scoreclientnum || (game::player1->privilege&PRIV_TYPE) >= PRIV_ELEVATED)
                                {
                                    uilist(g, {
                                        uilist(g, {
                                            uicenter(g, uipad(g, 1, g.text("cn", 0xFFFFFF)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            ownerbg;
                                            uicenter(g, uipad(g, 0.5f, g.textf("%d", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, o->clientnum)));
                                        }));
                                    });
                                }

                                if(scorebotinfo && hasbots)
                                {
                                    uilist(g, {
                                        uilist(g, {
                                            uicenter(g, uipad(g, 1, g.text("sk", 0xFFFFFF)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            ownerbg;
                                            uicenter(g, uipad(g, 0.5f, {
                                                if(o->actortype > A_PLAYER) g.textf("%d", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, o->skill);
                                                else { g.space(1); g.strut(1); }
                                            }));
                                        }));
                                    });
                                }
                                if(scorehandles && hashandle)
                                {
                                    uilist(g, {
                                        uilist(g, {
                                            uicenter(g, uipad(g, handlepad, g.space(1); g.strut(1)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            ownerbg;
                                            uicenter(g, uipad(g, 0.5f, g.textf("%s", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, o->handle[0] ? o->handle : "-")));
                                        }));
                                    });
                                }
                                if(scoreipinfo && hasip)
                                {
                                    uilist(g, {
                                        uilist(g, {
                                           uicenter(g, uipad(g, ippad, g.space(1); g.strut(1)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            ownerbg;
                                            uicenter(g, uipad(g, 0.5f, g.textf("%s", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, scorehost(o, false))));
                                        }));
                                    });
                                }
                                if(scorehostinfo && hashost)
                                {
                                    uilist(g, {
                                        uilist(g, {
                                           uicenter(g, uipad(g, hostpad, g.space(1); g.strut(1)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            ownerbg;
                                            uicenter(g, uipad(g, 0.5f, g.textf("%s", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, scorehost(o, true))));
                                        }));
                                    });
                                }
                                if(scoreverinfo && hasver)
                                {
                                    uilist(g, {
                                        uilist(g, {
                                           uicenter(g, uipad(g, verpad, g.space(1); g.strut(1)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            ownerbg;
                                            uicenter(g, uipad(g, 0.5f, g.textf("%s", ownerfgc, NULL, 0, -1, false, NULL, 0xFFFFFF, scoreversion(o))));
                                        }));
                                    });
                                }
                                if(scoreicons)
                                {
                                    uilist(g, {
                                        uilist(g, {
                                            uicenter(g, uipad(g, 0.125f, g.space(1); g.strut(1)));
                                        });
                                        loopscoregroup(uilist(g, {
                                            const char *status = questiontex;
                                            if(k == numgroups) status = spectatortex;
                                            else if(game::player1->dominating.find(o) >= 0) status = dominatedtex;
                                            else if(game::player1->dominated.find(o) >= 0) status = dominatingtex;
                                            else switch(o->state)
                                            {
                                                case CS_ALIVE: status = playertex; break;
                                                case CS_DEAD: status = deadtex; break;
                                                case CS_WAITING: status = waitingtex; break;
                                                case CS_EDITING: status = editingtex; break;
                                                default: break; // spectators shouldn't be here
                                            }
                                            uicenter(g, uipad(g, 0.125f, g.textf("\f(%s)", colour, NULL, 0, -1, false, NULL, 0xFFFFFF, status)));
                                        }));
                                    });
                                }
                                #if 0
                                uilist(g, {
                                    uilist(g, {
                                        uicenter(g, uipad(g, 0.25f, g.space(1); g.strut(1)));
                                    });
                                    loopscorelist(uilist(g, {
                                        uicenter(g, uipad(g, 0.25f, g.space(1); g.strut(1)));
                                    }));
                                });
                                #endif
                                g.poplist(); // horizontal
                                g.space(0.25f);
                                g.poplist(); // vertical
                                g.space(0.5f);
                            });
                        }
                    });
                });
            });
        });
        g.end();
    }

    static const char *posnames[10] = { "th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th" };

    int drawscoreitem(const char *icon, int colour, int x, int y, int s, float skew, float fade, int pos, int score, int offset, const char *name)
    {
        int col = 0xFF0000;
        switch(pos)
        {
            case 1: col = 0x00FF00; break;
            case 2: col = 0x00FFFF; break;
            case 3: col = 0xFFFF00; break;
            case 4: col = 0xFF8800; break;
            default: break;
        }
        string str = "";
        if(inventoryscorepos) concformatstring(str, "\fs\f[%d]%d%s\fS", col, pos, posnames[pos < 10 || pos > 13 ? pos%10 : 0]);
        if(inventoryscoreinfo)
        {
            if(*str) concatstring(str, inventoryscorebreak ? "\n" : " ");
            if(m_laptime(game::gamemode, game::mutators))
                concformatstring(str, "\fs\f[%d]\f(%s)\fS%s", col, offset ? (offset < 0 ? arrowtex : arrowdowntex) : arrowrighttex, timestr(offset < 0 ? 0-offset : offset, inventoryracestyle));
            else concformatstring(str, "\fs\f[%d]\f(%s)\fS%d", col, offset ? (offset > 0 ? arrowtex : arrowdowntex) : arrowrighttex, offset < 0 ? 0-offset : offset);
        }
        if(*str) concatstring(str, inventoryscorebreak ? "\n" : " ");
        if(name) concformatstring(str, "\fs\f[%d]%s\fS", colour, name);
        else concformatstring(str, "\fs\f[%d]\f(%s)\fS", colour, icon);
        if(m_laptime(game::gamemode, game::mutators)) { concformatstring(str, "%s%s", inventoryscorebreak ? "\n" : " ", timestr(score, inventoryracestyle)); }
        else if(m_defend(game::gamemode) && score >= VAR_MAX) { concformatstring(str, "%sWIN", inventoryscorebreak ? "\n" : " "); }
        else { concformatstring(str, "%s\fs\f[%d][\fS%d\fs\f[%d]]\fS", inventoryscorebreak ? "\n" : " ", col, score, col); }
        return drawitemtextx(x, y, 0, (inventoryscorebg ? TEXT_SKIN : 0)|(inventoryscorebreak == 2 ? TEXT_CENTERED : TEXT_RIGHT_JUSTIFY), skew, m_laptime(game::gamemode, game::mutators) ? "reduced" : "default", fade, "%s", str)+FONTH/8;
    }

    int drawscore(int x, int y, int s, int m, float blend, int count)
    {
        int sy = 0, numgroups = groupplayers(), numout = 0;
        loopi(2)
        {
            if(!i && game::focus->state == CS_SPECTATOR) continue;
            int pos = 0, realpos = 0, lastpos = -1;
            loopk(numgroups)
            {
                if(sy > m) break;
                scoregroup &sg = *groups[k];
                if(m_play(game::gamemode) && m_team(game::gamemode, game::mutators))
                {
                    realpos++;
                    if(!pos || (m_laptime(game::gamemode, game::mutators) ? ((!sg.total && groups[lastpos]->total) || sg.total > groups[lastpos]->total) : sg.total < groups[lastpos]->total))
                    {
                        pos = realpos;
                        lastpos = k;
                    }
                    if(!sg.team || ((sg.team != game::focus->team) == !i)) continue;
                    float sk = numout && inventoryscoreshrink > 0 ? 1.f-min(numout*inventoryscoreshrink, inventoryscoreshrinkmax) : 1;
                    int offset = numgroups > 1 ? sg.total-groups[k ? 0 : 1]->total : 0;
                    sy += drawscoreitem(teamtexname(sg.team), TEAM(sg.team, colour), x, y+sy, s, sk*inventoryscoresize, blend*inventoryblend, pos, sg.total, offset, inventoryscorename >= 2 ? TEAM(sg.team, name) : NULL);
                    if(++numout >= count) return sy;
                }
                else
                {
                    if(sg.team) continue;
                    loopvj(sg.players)
                    {
                        gameent *d = sg.players[j];
                        realpos++;
                        if(!pos || (m_laptime(game::gamemode, game::mutators) ? ((!d->cptime && sg.players[lastpos]->cptime) || d->cptime > sg.players[lastpos]->cptime) : d->points < sg.players[lastpos]->points))
                        {
                            pos = realpos;
                            lastpos = j;
                        }
                        if((d != game::focus) == !i) continue;
                        float sk = numout && inventoryscoreshrink > 0 ? 1.f-min(numout*inventoryscoreshrink, inventoryscoreshrinkmax) : 1;
                        int score = m_laptime(game::gamemode, game::mutators) ? d->cptime : d->points,
                            offset = (sg.players.length() > 1 && (!m_laptime(game::gamemode, game::mutators) || score)) ? score-(m_laptime(game::gamemode, game::mutators) ? sg.players[j ? 0 : 1]->cptime : sg.players[j ? 0 : 1]->points) : 0;
                        sy += drawscoreitem(playertex, game::getcolour(d, game::playerteamtone), x, y+sy, s, sk*inventoryscoresize, blend*inventoryblend, pos, score, offset, inventoryscorename ? game::colourname(d) : NULL);
                        if(++numout >= count) return sy;
                    }
                }
            }
        }
        return sy;
    }

    int raceinventory(int x, int y, int s, float blend)
    {
        int sy = 0;
        if(groupplayers())
        {
            scoregroup &sg = *groups[0];
            if(m_laptime(game::gamemode, game::mutators))
            {
                if(m_team(game::gamemode, game::mutators))
                {
                    if(sg.total)
                    {
                        pushfont("little");
                        sy += draw_textf("by %s", x+FONTW*2, y, 0, 0, 255, 255, 255, int(blend*255), TEXT_LEFT_UP, -1, -1, 1, game::colourteam(sg.team));
                        popfont();
                        sy += draw_textf("\fg%s", x, y-sy, 0, 0, 255, 255, 255, int(blend*255), TEXT_LEFT_UP, -1, -1, 1, timestr(sg.total, inventoryracestyle));
                    }
                }
                else if(!sg.players.empty())
                {
                    if(sg.players[0]->cptime)
                    {
                        pushfont("little");
                        sy += draw_textf("by %s", x+FONTW*2, y, 0, 0, 255, 255, 255, int(blend*255), TEXT_LEFT_UP, -1, -1, 1, game::colourname(sg.players[0]));
                        popfont();
                        sy += draw_textf("\fg%s", x, y-sy, 0, 0, 255, 255, 255, int(blend*255), TEXT_LEFT_UP, -1, -1, 1, timestr(sg.players[0]->cptime, inventoryracestyle));
                    }
                }
            }
            else if(m_team(game::gamemode, game::mutators))
            {
                if(sg.total)
                {
                    pushfont("little");
                    sy += draw_textf("by %s", x+FONTW*2, y, 0, 0, 255, 255, 255, int(blend*255), TEXT_LEFT_UP, -1, -1, 1, game::colourteam(sg.team));
                    popfont();
                    sy += draw_textf("\fs\fg%d\fS %s", x, y-sy, 0, 0, 255, 255, 255, int(blend*255), TEXT_LEFT_UP, -1, -1, 1, sg.total, sg.total != 1 ? "laps" : "lap");
                }
            }
            else if(!sg.players.empty() && sg.players[0]->points)
            {
                pushfont("little");
                sy += draw_textf("by %s", x+FONTW*2, y, 0, 0, 255, 255, 255, int(blend*255), TEXT_LEFT_UP, -1, -1, 1, game::colourname(sg.players[0]));
                popfont();
                sy += draw_textf("\fs\fg%d\fS %s", x, y-sy, 0, 0, 255, 255, 255, int(blend*255), TEXT_LEFT_UP, -1, -1, 1, sg.players[0]->points, sg.players[0]->points != 1 ? "laps" : "lap");
            }
        }
        return sy;
    }

    struct scoreboard : guicb
    {
        void gui(guient &g, bool firstpass)
        {
            renderscoreboard(g, firstpass);
        }
    } sb;

    ICOMMAND(0, showscores, "D", (int *down), showscores(*down!=0, false, false, true));

    void gamemenus()
    {
        if(scoreson) UI::addcb(&sb);
        if(game::player1->state == CS_DEAD) { if(scoreson) shownscores = true; }
        else shownscores = false;
    }
}
