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
        bool active;

        scoregroup() { reset(); }
        ~scoregroup() { reset(); }

        void reset()
        {
            players.shrink(0);
            active = false;
        }
    };
    vector<scoregroup *> groups;
    scoregroup spectators;

    VAR(IDF_PERSIST, autoscores, 0, 1, 3); // 1 = when dead, 2 = also in spectv, 3 = and in waittv too
    VAR(IDF_PERSIST, scoresdelay, 0, 0, VAR_MAX); // otherwise use respawn delay
    VAR(IDF_PERSIST, scoreconnecting, 0, 0, 1);
    VAR(IDF_PERSIST, scoreracestyle, 0, 1, 4);

    bool scoreson = false, scoresoff = false, shownscores = false;
    int scorespress = 0;

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
        int numgroups = 0, numdyns = game::numdynents();
        loopv(groups) groups[i]->reset();
        spectators.reset();
        loopi(numdyns)
        {
            gameent *o = (gameent *)game::iterdynents(i);
            if(!o || o->actortype >= A_ENEMY || (!scoreconnecting && !o->name[0])) continue;
            if(o->state == CS_SPECTATOR)
            {
                if(o != game::player1 || !client::demoplayback) spectators.players.add(o);
                continue;
            }
            int team = m_team(game::gamemode, game::mutators) ? o->team : T_NEUTRAL;
            bool found = false;
            loopj(numgroups)
            {
                scoregroup &g = *groups[j];
                if(team != g.team) continue;
                if(team) g.total = teamscore(team).total;
                g.players.add(o);
                g.active = found = true;
                break;
            }
            if(found) continue;
            if(numgroups >= groups.length()) groups.add(new scoregroup);
            scoregroup &g = *groups[numgroups++];
            g.team = team;
            if(!team) g.total = 0;
            else if(m_team(game::gamemode, game::mutators)) g.total = teamscore(o->team).total;
            else g.total = o->points;
            g.players.add(o);
            g.active = true;
        }
        loopi(numgroups) groups[i]->players.sort(playersort);
        loopvrev(groups) if(!groups[i]->active)
        {
            scoregroup *g = groups[i];
            groups.removeobj(g);
            delete g;
        }
        spectators.players.sort(playersort);
        groups.sort(scoregroupcmp, 0, numgroups);
        return numgroups;
    }

    void showscores(bool on, bool interm, bool onauto, bool ispress)
    {
        if(client::waiting())
        {
            scoresoff = scoreson = false;
            return;
        }
        if(ispress)
        {
            bool within = scorespress && totalmillis-scorespress < PHYSMILLIS;
            if(on)
            {
                if(within) onauto = true;
                scorespress = totalmillis ? totalmillis : 1;
            }
            else if(within && !scoresoff) { scorespress = 0; return; }
        }
        scoresoff = !onauto;
        scoreson = on;
        if(interm)
        {
            int numgroups = groupplayers();
            if(!numgroups) return;
            scoregroup &sg = *groups[0];
            if(m_team(game::gamemode, game::mutators))
            {
                int anc = sg.players.find(game::player1) >= 0 ? S_V_YOUWIN : (game::player1->state != CS_SPECTATOR ? S_V_YOULOSE : -1);
                if(m_defend(game::gamemode) && sg.total == INT_MAX)
                    game::announcef(anc, CON_EVENT, NULL, true, "\fwTeam %s secured all points", game::colourteam(sg.team));
                else
                {
                    if(numgroups > 1 && sg.total == groups[1]->total)
                    {
                        stringz(winner);
                        loopi(numgroups) if(i)
                        {
                            if(sg.total == groups[i]->total)
                            {
                                defformatstring(tw, "%s, ", game::colourteam(groups[i]->team));
                                concatstring(winner, tw);
                            }
                            else break;
                        }
                        game::announcef(S_V_DRAW, CON_EVENT, NULL, true, "\fw%s tied %swith a total score of \fs\fc%s\fS", game::colourteam(sg.team), winner, m_laptime(game::gamemode, game::mutators) ? timestr(sg.total, scoreracestyle) : intstr(sg.total));
                    }
                    else game::announcef(anc, CON_EVENT, NULL, true, "\fwTeam %s won the match with a total score of \fs\fc%s\fS", game::colourteam(sg.team), m_laptime(game::gamemode, game::mutators) ? timestr(sg.total, scoreracestyle) : intstr(sg.total));
                }
            }
            else
            {
                int anc = sg.players[0] == game::player1 ? S_V_YOUWIN : (game::player1->state != CS_SPECTATOR ? S_V_YOULOSE : -1);
                if(m_laptime(game::gamemode, game::mutators))
                {
                    if(sg.players.length() > 1 && sg.players[0]->cptime == sg.players[1]->cptime)
                    {
                        stringz(winner);
                        loopv(sg.players) if(i)
                        {
                            if(sg.players[0]->cptime == sg.players[i]->cptime)
                            {
                                concatstring(winner, game::colourname(sg.players[i]));
                                concatstring(winner, ", ");
                            }
                            else break;
                        }
                        game::announcef(S_V_DRAW, CON_EVENT, NULL, true, "\fw%s tied %swith the fastest lap \fs\fc%s\fS", game::colourname(sg.players[0]), winner, sg.players[0]->cptime ? timestr(sg.players[0]->cptime, scoreracestyle) : "dnf");
                    }
                    else game::announcef(anc, CON_EVENT, NULL, true, "\fw%s won the match with the fastest lap \fs\fc%s\fS", game::colourname(sg.players[0]), sg.players[0]->cptime ? timestr(sg.players[0]->cptime, scoreracestyle) : "dnf");
                }
                else
                {
                    if(sg.players.length() > 1 && sg.players[0]->points == sg.players[1]->points)
                    {
                        stringz(winner);
                        loopv(sg.players) if(i)
                        {
                            if(sg.players[0]->points == sg.players[i]->points)
                            {
                                concatstring(winner, game::colourname(sg.players[i]));
                                concatstring(winner, ", ");
                            }
                            else break;
                        }
                        game::announcef(S_V_DRAW, CON_EVENT, NULL, true, "\fw%s tied %swith a total score of \fs\fc%d\fS", game::colourname(sg.players[0]), winner, sg.players[0]->points);
                    }
                    else game::announcef(anc, CON_EVENT, NULL, true, "\fw%s won the match with a total score of \fs\fc%d\fS", game::colourname(sg.players[0]), sg.players[0]->points);
                }
            }
        }
    }

    ICOMMAND(0, getscoreteam, "i", (int *group), intret(groups.inrange(*group) ? groups[*group]->team : -1));
    ICOMMAND(0, getscoretotal, "i", (int *group), intret(groups.inrange(*group) ? groups[*group]->total : 0));

    ICOMMAND(0, refreshscoreboard, "", (), groupplayers());
    ICOMMAND(0, numscoregroups, "", (), intret(groups.length()));
    ICOMMAND(0, numscoreboard, "i", (int *group), intret(*group >= 0 ? (groups.inrange(*group) ? groups[*group]->players.length() : 0) : spectators.players.length()));
    ICOMMAND(0, numspectators, "i", (int *group), intret(spectators.players.length()));
    ICOMMAND(0, loopscoreboard, "rie", (ident *id, int *group, uint *body),
    {
        if(*group >= groups.length()) return;
        loopstart(id, stack);
        scoregroup &g = *group >= 0 ? *groups[*group] : spectators;
        loopv(g.players)
        {
            loopiter(id, stack, g.players[i]->clientnum);
            execute(body);
        }
        loopend(id, stack);
    });
    ICOMMAND(0, loopscoregroups, "reee", (ident *id, uint *group, uint *spec, uint *body),
    {
        loopstart(id, stack);
        loopv(groups) if(groups[i]->players.length())
        {
            loopiter(id, stack, i);
            execute(group);
            execute(body);
        }
        if(spectators.players.length())
        {
            loopiter(id, stack, -1);
            execute(spec);
            execute(body);
        }
        loopend(id, stack);
    });

    ICOMMAND(0, showscores, "D", (int *down), showscores(*down!=0, false, false, true));
    ICOMMAND(0, togglescores, "b", (int *on), showscores(*on >= 0 ? *on!=0 : !scoreson, false, false, false));
}
