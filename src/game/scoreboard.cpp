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
    VAR(IDF_PERSIST, scoreconnecting, 0, 0, 1);
    VAR(IDF_PERSIST, scoreracestyle, 0, 1, 4);

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
                    menulastpress = totalmillis ? totalmillis : 1;
                }
                else if(within && !scoresoff) { menulastpress = 0; return; }
            }
            if(!scoreson && on) menustart = lastmillis;
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

    ICOMMAND(0, showscores, "D", (int *down), showscores(*down!=0, false, false, true));

    void gamemenus()
    {
        UI::pressui("scoreboard", scoreson);
        if(game::player1->state == CS_DEAD) { if(scoreson) shownscores = true; }
        else shownscores = false;
    }

    static const char *posnames[10] = { "th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th" };

    int drawscoreitem(const char *name, int colour, int x, int y, int s, float skew, float fade, int pos, int score, int offset)
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
        if(name && *name) concformatstring(str, "%s\fs\f[%d]%s\fS", *str ? (inventoryscorebreak ? "\n" : " ") : "", colour, name);
        if(inventoryscoreinfo)
        {
            if(m_laptime(game::gamemode, game::mutators))
                concformatstring(str, "%s\fs\f[%d]\f(%s)\fS%s", *str ? (inventoryscorebreak ? "\n" : " ") : "", col, offset ? (offset < 0 ? arrowtex : arrowdowntex) : arrowrighttex, timestr(offset < 0 ? 0-offset : offset, inventoryracestyle));
            else concformatstring(str, "%s\fs\f[%d]\f(%s)\fS%d", *str ? (inventoryscorebreak ? "\n" : " ") : "", col, offset ? (offset > 0 ? arrowtex : arrowdowntex) : arrowrighttex, offset < 0 ? 0-offset : offset);
        }
        if(m_laptime(game::gamemode, game::mutators)) { concformatstring(str, "%s%s", *str ? (inventoryscorebreak ? "\n" : " ") : "", timestr(score, inventoryracestyle)); }
        else if(m_defend(game::gamemode) && score >= VAR_MAX) { concformatstring(str, "%sWIN", *str ? (inventoryscorebreak ? "\n" : " ") : ""); }
        else { concformatstring(str, "%s\fs\f[%d][\fS%d pts\fs\f[%d]]\fS", *str ? (inventoryscorebreak ? "\n" : " ") : "", col, score, col); }
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
                    sy += drawscoreitem(inventoryscorename >= 2 ? game::colourteam(sg.team) : NULL, TEAM(sg.team, colour), x, y+sy, s, sk*inventoryscoresize, blend*inventoryblend, pos, sg.total, offset);
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
                        sy += drawscoreitem(inventoryscorename ? game::colourname(d) : NULL, game::getcolour(d, game::playerteamtone, game::playerteamtonelevel), x, y+sy, s, sk*inventoryscoresize, blend*inventoryblend, pos, score, offset);
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
}
