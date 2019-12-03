#include "game.h"
namespace bomber
{
    bomberstate st;

    ICOMMAND(0, getbombernum, "", (), intret(st.flags.length()));
    ICOMMAND(0, getbomberenabled, "i", (int *n), intret(st.flags.inrange(*n) && st.flags[*n].enabled ? 1 : 0));
    ICOMMAND(0, getbomberteam, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].team : -1));
    ICOMMAND(0, getbomberdroptime, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].droptime : -1));
    ICOMMAND(0, getbombertaketime, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].taketime : -1));
    ICOMMAND(0, getbomberdisptime, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].displaytime : -1));
    ICOMMAND(0, getbombertarget, "i", (int *n), intret(st.flags.inrange(*n) ? st.flags[*n].target : -1));
    ICOMMAND(0, getbomberowner, "i", (int *n), intret(st.flags.inrange(*n) && st.flags[*n].owner ? st.flags[*n].owner->clientnum : -1));
    ICOMMAND(0, getbomberlastowner, "i", (int *n), intret(st.flags.inrange(*n) && st.flags[*n].lastowner ? st.flags[*n].lastowner->clientnum : -1));

    bool radarallow(int id, int render, vec &dir, float &dist, bool justtest = false)
    {
        if(!st.flags.inrange(id) || (m_hard(game::gamemode, game::mutators) && !G(radarhardaffinity)) || !st.flags[id].enabled) return false;
        if(justtest) return true;
        dir = vec(render > 0 ? st.flags[id].spawnloc : st.flags[id].pos(render < 0)).sub(camera1->o);
        dist = dir.magnitude();
        if(st.flags[id].owner != game::focus && hud::radarlimited(dist)) return false;
        return true;
    }

    ICOMMAND(0, getbomberradarallow, "ibi", (int *n, int *v, int *q),
    {
        vec dir(0, 0, 0);
        float dist = -1;
        intret(radarallow(*n, *v, dir, dist, *q != 0) ? 1 : 0);
    });
    ICOMMAND(0, getbomberradardist, "ib", (int *n, int *v),
    {
        vec dir(0, 0, 0);
        float dist = -1;
        if(!radarallow(*n, *v, dir, dist)) return;
        floatret(dist);
    });
    ICOMMAND(0, getbomberradardir, "ib", (int *n, int *v),
    {
        vec dir(0, 0, 0);
        float dist = -1;
        if(!radarallow(*n, *v, dir, dist)) return;
        dir.rotate_around_z(-camera1->yaw*RAD).normalize();
        floatret(-atan2(dir.x, dir.y)/RAD);
    });

    #define LOOPBOMBER(name,op) \
        ICOMMAND(0, loopbomber##name, "iire", (int *count, int *skip, ident *id, uint *body), \
        { \
            if(!m_bomber(game::gamemode)) return; \
            loopstart(id, stack); \
            op(st.flags, *count, *skip) \
            { \
                loopiter(id, stack, i); \
                execute(body); \
            } \
            loopend(id, stack); \
        });
    LOOPBOMBER(,loopcsv);
    LOOPBOMBER(rev,loopcsvrev);

    #define LOOPBOMBERIF(name,op) \
        ICOMMAND(0, loopbomber##name##if, "iiree", (int *count, int *skip, ident *id, uint *cond, uint *body), \
        { \
            if(!m_bomber(game::gamemode)) return; \
            loopstart(id, stack); \
            op(st.flags, *count, *skip) \
            { \
                loopiter(id, stack, i); \
                if(executebool(cond)) execute(body); \
            } \
            loopend(id, stack); \
        });
    LOOPBOMBERIF(,loopcsv);
    LOOPBOMBERIF(rev,loopcsvrev);

    void killed(gameent *d, gameent *v)
    {
        if(v && m_bb_hold(game::gamemode, game::mutators) && (!m_team(game::gamemode, game::mutators) || d->team != v->team))
        {
            loopv(st.flags) if(isbomberaffinity(st.flags[i]) && st.flags[i].owner == v)
                st.flags[i].taketime = lastmillis;
        }
    }

    int carryaffinity(gameent *d)
    {
        int n = 0;
        loopv(st.flags) if(st.flags[i].owner == d) n++;
        return n;
    }

    float fixrot(float c)
    {
        float angle = c;
        while(angle < 0.0f) angle += 360.0f;
        while(angle >= 360.0f) angle -= 360.0f;
        return angle;
    }

    float offrot(float a, float b)
    {
        float angle = fixrot(a)-fixrot(b);
        while(angle < -180.0f) angle += 360.0f;
        while(angle >= 180.0f) angle -= 360.0f;
        return fabs(angle);
    }

    VAR(IDF_PERSIST, bombertargetintersect, 0, 1, 1);
    VAR(IDF_PERSIST, bombertargetangle, 0, 1, 1);
    int findtarget(gameent *d)
    {
        vec dest;
        gameent *e = NULL;
        float bestangle = 1e16f, bestdist = 1e16f;
        int best = -1;
        int numdyns = game::numdynents();
        loopk(d->actortype != A_PLAYER ? 4 : 2)
        {
            if(bombertargetintersect)
            {
                findorientation(d->o, d->yaw, d->pitch, dest);
                if((e = game::intersectclosest(d->o, dest, d)) && e->team == d->team && e->state == CS_ALIVE && (k%2 || e->actortype != A_BOT))
                    return e->clientnum;
            }
            float fx = k >= 2 ? 360 : (d->ai ? d->ai->views[0] : curfov), fy = k >= 2 ? 360 : (d->ai ? d->ai->views[1] : fovy);
            loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && e->team == d->team && e->state == CS_ALIVE && (k%2 || e->actortype != A_BOT))
            {
                if(getsight(d->o, d->yaw, d->pitch, e->o, dest, 1e16f, fx, fy))
                {
                    vec dir = vec(e->o).sub(d->o);
                    float dist = dir.magnitude();
                    if(dist > 1e-3f) dir.div(dist);
                    float yaw = 0, pitch = 0;
                    vectoyawpitch(dir, yaw, pitch);
                    float offyaw = offrot(d->yaw, yaw), offpitch = offrot(d->pitch, pitch), offangle = offpitch+offyaw;
                    if(best < 0 || (bombertargetangle ? (offangle < bestangle || (offangle <= bestangle && dist < bestdist)) : (dist < bestdist || (dist <= bestdist && offangle < bestangle))))
                    {
                        best = e->clientnum;
                        bestdist = dist;
                        bestangle = offangle;
                    }
                }
            }
            if(best >= 0) break;
        }
        return best;
    }

    bool dropaffinity(gameent *d)
    {
        if(!carryaffinity(d) || (!d->action[AC_AFFINITY] && d->actiontime[AC_AFFINITY] <= 0)) return false;
        if(d->action[AC_AFFINITY]) return true;
        vec o = d->headpos(), inertia = vec(d->yaw*RAD, d->pitch*RAD).mul(bomberspeed).add(vec(d->vel).add(d->falling).mul(bomberrelativity));
        bool guided = m_team(game::gamemode, game::mutators) && bomberlockondelay && lastmillis-d->actiontime[AC_AFFINITY] >= bomberlockondelay;
        client::addmsg(N_DROPAFFIN, "ri8", d->clientnum, guided ? findtarget(d) : -1, int(o.x*DMF), int(o.y*DMF), int(o.z*DMF), int(inertia.x*DMF), int(inertia.y*DMF), int(inertia.z*DMF));
        d->action[AC_AFFINITY] = false;
        d->actiontime[AC_AFFINITY] = 0;
        return true;
    }

    void preload()
    {
        preloadmodel("props/ball");
    }

    vec pulsecolour()
    {
        uint n = lastmillis/100;
        return vec::fromcolor(pulsecols[PULSE_DISCO][n%PULSECOLOURS]).lerp(vec::fromcolor(pulsecols[PULSE_DISCO][(n+1)%PULSECOLOURS]), (lastmillis%100)/100.0f);
    }

    FVAR(IDF_PERSIST, bomberreticlesize, 0, 0.1f, 1.f);
    void drawonscreen(int w, int h, float blend)
    {
        if(!gs_playing(game::gamestate) || !m_team(game::gamemode, game::mutators) || !bomberlockondelay || game::focus->state != CS_ALIVE || !game::focus->action[AC_AFFINITY] || lastmillis-game::focus->actiontime[AC_AFFINITY] < bomberlockondelay)
            return;
        loopv(st.flags)
        {
            bomberstate::flag &f = st.flags[i];
            if(!f.enabled || !isbomberaffinity(f) || f.owner != game::focus) continue;
            gameent *e = game::getclient(findtarget(f.owner));
            float cx = 0.5f, cy = 0.5f, cz = 1;
            if(e && vectocursor(e->headpos(), cx, cy, cz))
            {
                int interval = lastmillis%500;
                float rp = 1, gp = 1, bp = 1,
                      sp = interval >= 250 ? (500-interval)/250.f : interval/250.f,
                      sq = max(sp, 0.5f), size = bomberreticlesize*min(w, h);
                hud::colourskew(rp, gp, bp, sp);
                int sx = int(cx*w-size*sq), sy = int(cy*h-size*sq), ss = int(size*2*sq);
                Texture *t = textureload(hud::indicatortex, 3);
                if(t && t != notexture)
                {
                    glBindTexture(GL_TEXTURE_2D, t->id);
                    gle::colorf(rp, gp, bp, sq);
                    hud::drawsized(sx, sy, ss);
                }
                t = textureload(hud::crosshairtex, 3);
                if(t && t != notexture)
                {
                    glBindTexture(GL_TEXTURE_2D, t->id);
                    gle::colorf(rp, gp, bp, sq*0.5f);
                    hud::drawsized(sx+ss/4, sy+ss/4, ss/2);
                }
            }
        }
    }

    void drawnotices(int w, int h, int &tx, int &ty, int tr, int tg, int tb, float blend)
    {
        if(game::focus->state != CS_ALIVE || hud::shownotices < 2) return;
        if(game::focus->lastbuff && hud::shownotices >= 3)
        {
            pushfont("reduced");
            if(m_regen(game::gamemode, game::mutators) && bomberregenbuff && bomberregenextra)
            {
                if(game::damageinteger)
                    ty += draw_textf("Buffing: \fs\fo%d%%\fS damage, \fs\fg%d%%\fS shield, +\fs\fy%d\fS regen", tx, ty, int(FONTW*hud::noticepadx), int(FONTH*hud::noticepady), tr, tg, tb, int(255*blend), TEXT_CENTERED, -1, -1, 1, int(bomberbuffdamage*100), int(bomberbuffshield*100), int(ceilf(bomberregenextra/game::damagedivisor)));
                else ty += draw_textf("Buffing: \fs\fo%d%%\fS damage, \fs\fg%d%%\fS shield, +\fs\fy%1.f\fS regen", tx, ty, int(FONTW*hud::noticepadx), int(FONTH*hud::noticepady), tr, tg, tb, int(255*blend), TEXT_CENTERED, -1, -1, 1, int(bomberbuffdamage*100), int(bomberbuffshield*100), bomberregenextra/game::damagedivisor);
            }
            else ty += draw_textf("Buffing: \fs\fo%d%%\fS damage, \fs\fg%d%%\fS shield", tx, ty, int(FONTW*hud::noticepadx), int(FONTH*hud::noticepady), tr, tg, tb, int(255*blend), TEXT_CENTERED, -1, -1, 1, int(bomberbuffdamage*100), int(bomberbuffshield*100));
            popfont();
        }
        loopv(st.flags)
        {
            bomberstate::flag &f = st.flags[i];
            if(f.owner == game::focus)
            {
                bool important = false;
                if(carrytime)
                {
                    int delay = carrytime-(lastmillis-f.taketime);
                    pushfont("default");
                    ty += draw_textf("Bomb explodes in \fs\fzgy%s\fS", tx, ty, int(FONTW*hud::noticepadx), int(FONTH*hud::noticepady), tr, tg, tb, int(255*blend), TEXT_CENTERED, -1, -1, 1, timestr(delay));
                    popfont();
                    if(m_bb_hold(game::gamemode, game::mutators))
                    {
                        pushfont("reduced");
                        ty += draw_textf("Killing enemies resets fuse timer", tx, ty, int(FONTW*hud::noticepadx), int(FONTH*hud::noticepady), tr, tg, tb, int(255*blend), TEXT_CENTERED);
                        popfont();
                    }
                    if(delay <= carrytime/4) important = true;
                }
                if(game::focus == game::player1)
                {
                    pushfont(important ? "emphasis" : "reduced");
                    ty += draw_textf(important ? "\fs\fzuyPress \fs\fw\f{=affinity}\fS to throw the bomb\fS" : "Press \fs\fw\f{=affinity}\fS to throw the bomb", tx, ty, int(FONTW*hud::noticepadx), int(FONTH*hud::noticepady), tr, tg, tb, int(255*blend), TEXT_CENTERED);
                    popfont();
                }
                break;
            }
        }
    }

    void drawevents(int w, int h, int &tx, int &ty, int tr, int tg, int tb, float blend)
    {
        if(game::focus->state != CS_ALIVE || hud::showevents < 2) return;
        loopv(st.flags)
        {
            bomberstate::flag &f = st.flags[i];
            if(f.owner == game::focus)
            {
                ty -= draw_textf("You are holding the \fs\f[%d]\f(%s)bomb\fS", tx, ty, int(FONTW*hud::eventpadx), int(FONTH*hud::eventpady), tr, tg, tb, int(255*blend), TEXT_CENTERED, -1, -1, 1, game::pulsehexcol(game::focus, PULSE_DISCO), hud::bombtex);
                ty -= FONTH/4;
                break;
            }
        }
    }

    void checkcams(vector<cament *> &cameras)
    {
        loopv(st.flags) // flags/bases
        {
            bomberstate::flag &f = st.flags[i];
            cament *c = cameras.add(new cament(cameras.length(), cament::AFFINITY, i));
            c->o = f.pos(true);
            c->o.z += enttype[AFFINITY].radius/2;
            c->player = f.owner;
        }
    }

    void updatecam(cament *c)
    {
        switch(c->type)
        {
            case cament::AFFINITY:
            {
                if(!st.flags.inrange(c->id)) break;
                bomberstate::flag &f = st.flags[c->id];
                c->o = f.pos(true);
                c->o.z += enttype[AFFINITY].radius/2;
                c->player = f.owner;
                break;
            }
            default: break;
        }
    }

    void render()
    {
        loopv(st.flags) // flags/bases
        {
            bomberstate::flag &f = st.flags[i];
            modelstate mdl, basemdl;
            float trans = 1;
            int millis = lastmillis-f.displaytime;
            if(millis <= 1000) trans *= float(millis)/1000.f;
            if(!f.enabled) basemdl.material[0] = mdl.material[0] = bvec(0, 0, 0);
            else if(isbomberaffinity(f))
            {
                vec above(f.pos(true, true));
                if(!f.owner && !f.droptime) above.z += enttype[AFFINITY].radius/4*trans;
                mdl.anim = ANIM_MAPMODEL|ANIM_LOOP;
                mdl.flags = MDL_CULL_VFC|MDL_CULL_OCCLUDED;
                mdl.o = above;
                mdl.size = trans;
                mdl.yaw = !f.owner && f.proj ? f.proj->yaw : (lastmillis/4)%360;
                mdl.pitch = !f.owner && f.proj ? f.proj->pitch : 0;
                mdl.roll = !f.owner && f.proj ? f.proj->roll : 0;
                float wait = f.droptime ? clamp((lastmillis-f.droptime)/float(bomberresetdelay), 0.f, 1.f) : ((f.owner && carrytime) ? clamp((lastmillis-f.taketime)/float(carrytime), 0.f, 1.f) : 0.f);
                int interval = lastmillis%1000;
                vec effect = pulsecolour();
                if(wait > 0.5f)
                {
                    int delay = wait > 0.7f ? (wait > 0.85f ? 150 : 300) : 600, millis = lastmillis%(delay*2);
                    float amt = (millis <= delay ? millis/float(delay) : 1.f-((millis-delay)/float(delay)));
                    flashcolour(effect.r, effect.g, effect.b, 1.f, 0.f, 0.f, amt);
                }
                basemdl.material[0] = mdl.material[0] = bvec::fromcolor(effect);
                if(f.owner != game::focus || game::thirdpersonview(true))
                {
                    if(f.owner == game::focus)
                        trans *= game::focus != game::player1 ? game::affinityfollowblend : game::affinitythirdblend;
                    mdl.color.a *= trans;
                    rendermodel("props/ball", mdl);
                    float fluc = interval >= 500 ? (1500-interval)/1000.f : (500+interval)/1000.f;
                    int pcolour = effect.tohexcolor();
                    if(game::affinityhint)
                        part_create(PART_HINT_SOFT, 1, above, pcolour, enttype[AFFINITY].radius/4*game::affinityhintsize+(2*fluc), fluc*trans*game::affinityhintblend*camera1->o.distrange(above, game::affinityhintfadeat, game::affinityhintfadecut));
                    if(gs_playing(game::gamestate) && f.droptime)
                    {
                        above.z += enttype[AFFINITY].radius/4*trans+1.5f;
                        part_icon(above, textureload(hud::progringtex, 3), 5*trans, 1, 0, 0, 1, pcolour, (lastmillis%1000)/1000.f, 0.1f);
                        part_icon(above, textureload(hud::progresstex, 3), 5*trans, 0.25f, 0, 0, 1, pcolour);
                        part_icon(above, textureload(hud::progresstex, 3), 5*trans, 1, 0, 0, 1, pcolour, 0, wait);
                    }
                }
            }
            else if(!m_bb_hold(game::gamemode, game::mutators))
            {
                vec above = f.spawnloc, effect = vec::fromcolor(TEAM(f.team, colour)).mul(trans);
                float blend = camera1->o.distrange(above, game::affinityhintfadeat, game::affinityhintfadecut);
                basemdl.material[0] = mdl.material[0] = bvec::fromcolor(effect);
                int pcolour = effect.tohexcolor();
                part_explosion(above, 3, PART_GLIMMERY, 1, pcolour, 1, trans*blend);
                part_create(PART_HINT_SOFT, 1, above, pcolour, 6, trans*blend);
                if(m_bb_basket(game::gamemode, game::mutators) && carryaffinity(game::focus) && bomberbasketmindist > 0 && game::focus->o.dist(above) < bomberbasketmindist)
                {
                    int millis = lastmillis%500;
                    float amt = millis <= 250 ? 1.f-(millis/250.f) : (millis-250)/250.f, height = enttype[AFFINITY].radius*0.75f;
                    vec c = game::pulsecolour(game::focus, PULSE_WARN),
                        offset = vec(above).sub(camera1->o).rescale(-enttype[AFFINITY].radius*0.5f);
                    height += height*amt*0.1f;
                    offset.z = max(offset.z, -1.0f);
                    offset.add(above);
                    part_icon(offset, textureload(hud::warningtex, 3, true, false), height, amt*blend, 0, 0, 1, c.tohexcolor());
                }
                above.z += 6*trans;
                defformatstring(info, "<super>%s Base", TEAM(f.team, name));
                part_textcopy(above, info, PART_TEXT, 1, TEAM(f.team, colour), 2, trans*blend);
                above.z += 4;
                part_icon(above, textureload(hud::teamtexname(f.team), 3), 4, trans*blend, 0, 0, 1, TEAM(f.team, colour));
            }
            if(!m_bb_hold(game::gamemode, game::mutators))
            {
                basemdl.anim = ANIM_MAPMODEL|ANIM_LOOP;
                basemdl.flags = MDL_CULL_VFC|MDL_CULL_OCCLUDED;
                basemdl.o = f.render;
                basemdl.yaw = f.yaw;
                rendermodel("props/point", basemdl);
            }
        }
    }

    void adddynlights()
    {
        loopv(st.flags)
        {
            bomberstate::flag &f = st.flags[i];
            if(!f.enabled) continue;
            float trans = 1.f;
            int millis = lastmillis-f.displaytime;
            if(millis <= 1000) trans = float(millis)/1000.f;
            vec colour = isbomberaffinity(f) ? pulsecolour() : vec::fromcolor(TEAM(f.team, colour));
            adddynlight(f.pos(true, true), enttype[AFFINITY].radius*trans, colour, 0, 0);
        }
    }

    void reset()
    {
        st.reset();
    }

    void setup()
    {
        loopv(entities::ents) if(entities::ents[i]->type == AFFINITY)
        {
            gameentity &e = *(gameentity *)entities::ents[i];
            if(!checkmapvariant(e.attrs[enttype[e.type].mvattr])) continue;
            if(!m_check(e.attrs[3], e.attrs[4], game::gamemode, game::mutators)) continue;
            if(!isteam(game::gamemode, game::mutators, e.attrs[0], T_NEUTRAL)) continue;
            int team = e.attrs[0];
            if(m_bb_assault(game::gamemode, game::mutators)) switch(team)
            { // attack
                case T_ALPHA: break; // goal
                case T_OMEGA: team = T_NEUTRAL; break; // ball
                default: continue; // remove
            }
            st.addaffinity(e.o, team, e.attrs[1], e.attrs[2]);
            if(st.flags.length() >= MAXPARAMS) break;
        }
    }

    void sendaffinity(packetbuf &p)
    {
        putint(p, N_INITAFFIN);
        putint(p, st.flags.length());
        loopv(st.flags)
        {
            bomberstate::flag &f = st.flags[i];
            putint(p, f.team);
            putint(p, f.yaw);
            putint(p, f.pitch);
            loopj(3) putint(p, int(f.spawnloc[j]*DMF));
        }
    }

    void setscore(int team, int total)
    {
        hud::teamscore(team).total = total;
    }

    void parseaffinity(ucharbuf &p)
    {
        int numflags = getint(p);
        if(numflags < 0) return;
        while(st.flags.length() > numflags) st.flags.pop();
        loopi(numflags)
        {
            int team = getint(p), yaw = getint(p), pitch = getint(p), enabled = getint(p), owner = getint(p), dropped = 0, target = -1;
            vec spawnloc(0, 0, 0), droploc(0, 0, 0), inertia(0, 0, 0);
            loopj(3) spawnloc[j] = getint(p)/DMF;
            if(owner < 0)
            {
                dropped = getint(p);
                if(dropped)
                {
                    target = getint(p);
                    loopj(3) droploc[j] = getint(p)/DMF;
                    loopj(3) inertia[j] = getint(p)/DMF;
                }
            }
            if(p.overread()) break;
            if(i >= MAXPARAMS) continue;
            while(!st.flags.inrange(i)) st.flags.add();
            bomberstate::flag &f = st.flags[i];
            f.reset();
            f.team = team;
            f.yaw = yaw;
            f.pitch = pitch;
            f.enabled = enabled != 0;
            f.setposition(spawnloc);
            if(owner >= 0) st.takeaffinity(i, game::newclient(owner), lastmillis);
            else if(dropped) st.dropaffinity(i, droploc, inertia, lastmillis, target);
        }
    }

    void dropaffinity(gameent *d, int i, const vec &droploc, const vec &inertia, int target)
    {
        if(!st.flags.inrange(i)) return;
        st.dropaffinity(i, droploc, inertia, lastmillis, target);
    }

    void removeplayer(gameent *d)
    {
        loopv(st.flags) if(st.flags[i].owner == d)
        {
            bomberstate::flag &f = st.flags[i];
            st.dropaffinity(i, f.owner->feetpos(bomberdropheight), f.owner->vel, lastmillis);
        }
    }

    void affinityeffect(int i, int team, const vec &from, const vec &to, int effect, const char *str)
    {
        if(from.x >= 0)
        {
            if(effect&1 && game::aboveheadaffinity)
            {
                defformatstring(text, "<huge>\fzuw%s", str);
                part_textcopy(vec(from).add(vec(0, 0, enttype[AFFINITY].radius)), text, PART_TEXT, game::eventiconfade, TEAM(team, colour), 3, 1, -10);
            }
            if(game::dynlighteffects) adddynlight(vec(from).add(vec(0, 0, enttype[AFFINITY].radius)), enttype[AFFINITY].radius*2, vec::fromcolor(TEAM(team, colour)).mul(2.f), 500, 250);
        }
        if(to.x >= 0)
        {
            if(effect&2 && game::aboveheadaffinity)
            {
                defformatstring(text, "<huge>\fzuw%s", str);
                part_textcopy(vec(to).add(vec(0, 0, enttype[AFFINITY].radius)), text, PART_TEXT, game::eventiconfade, TEAM(team, colour), 3, 1, -10);
            }
            if(game::dynlighteffects) adddynlight(vec(to).add(vec(0, 0, enttype[AFFINITY].radius)), enttype[AFFINITY].radius*2, vec::fromcolor(TEAM(team, colour)).mul(2.f), 500, 250);
        }
        if(from.x >= 0 && to.x >= 0 && from != to) part_trail(PART_SPARK, 500, from, to, TEAM(team, colour), 1, 1, -10);
    }

    void destroyaffinity(const vec &o)
    {
        part_create(PART_PLASMA_SOFT, 250, o, 0xAA4400, enttype[AFFINITY].radius*0.5f, 0.5f);
        part_explosion(o, enttype[AFFINITY].radius, PART_EXPLOSION, 500, 0xAA4400, 1.f, 0.5f);
        part_explosion(o, enttype[AFFINITY].radius*2, PART_SHOCKWAVE, 250, 0xAA4400, 1.f, 0.1f);
        part_create(PART_SMOKE_LERP_SOFT, 500, o, 0x333333, enttype[AFFINITY].radius*0.75f, 0.5f, -15);
        if(!m_kaboom(game::gamemode, game::mutators) && game::nogore != 2 && game::debrisscale > 0)
        {
            int debris = rnd(5)+5, amt = int((rnd(debris)+debris+1)*game::debrisscale);
            loopi(amt) projs::create(o, o, true, NULL, PRJ_DEBRIS, -1, 0, rnd(game::debrisfade)+game::debrisfade, 0, rnd(501), rnd(101)+50);
        }
        playsound(WSND2(W_GRENADE, false, S_W_EXPLODE), o, NULL, 0, 255);
    }

    void resetaffinity(int i, int value, const vec &pos)
    {
        if(!st.flags.inrange(i)) return;
        bomberstate::flag &f = st.flags[i];
        if(f.enabled && !value)
        {
            destroyaffinity(f.pos(true, true));
            if(isbomberaffinity(f))
            {
                affinityeffect(i, T_NEUTRAL, f.pos(true, true), f.spawnloc, 3, "RESET");
                game::announcef(S_V_BOMBRESET, CON_EVENT, NULL, true, "\faThe \fs\fzwvbomb\fS has been reset");
            }
        }
        if(value == 2) st.dropaffinity(i, pos, vec(0, 0, 1), lastmillis);
        else st.returnaffinity(i, lastmillis, value!=0);
    }

    VAR(IDF_PERSIST, showbomberdists, 0, 2, 2); // 0 = off, 1 = self only, 2 = all
    void scoreaffinity(gameent *d, int relay, int goal, int score)
    {
        if(!st.flags.inrange(relay) || !st.flags.inrange(goal)) return;
        bomberstate::flag &f = st.flags[relay], &g = st.flags[goal];
        stringz(extra);
        if(m_bb_basket(game::gamemode, game::mutators) && showbomberdists >= (d != game::player1 ? 2 : 1))
        {
            if(f.droptime) formatstring(extra, " from \fs\fy%.2f\fom\fS", f.droppos.dist(g.spawnloc)/8.f);
            else copystring(extra, " with a \fs\fytouchdown\fS");
        }
        affinityeffect(goal, d->team, g.spawnloc, f.spawnloc, 3, "DESTROYED");
        destroyaffinity(g.spawnloc);
        hud::teamscore(d->team).total = score;
        defformatstring(gteam, "%s", game::colourteam(g.team, "pointtex"));
        game::announcef(S_V_BOMBSCORE, CON_EVENT, d, true, "\fa%s destroyed the %s base for team %s%s (score: \fs\fc%d\fS, time taken: \fs\fc%s\fS)", game::colourname(d), gteam, game::colourteam(d->team), extra, score, timestr(lastmillis-f.inittime, 1));
        st.returnaffinity(relay, lastmillis, false);
    }

    void takeaffinity(gameent *d, int i)
    {
        if(!st.flags.inrange(i)) return;
        bomberstate::flag &f = st.flags[i];
        playsound(S_CATCH, d->o, d);
        if(!f.droptime)
        {
            affinityeffect(i, d->team, d->feetpos(), f.pos(true, true), 1, "TAKEN");
            game::announcef(S_V_BOMBPICKUP, CON_EVENT, d, true, "\fa%s picked up the \fs\fzwv\f($bombtex)bomb\fS", game::colourname(d));
        }
        st.takeaffinity(i, d, lastmillis);
        if(d->ai) aihomerun(d, d->ai->state.last());
    }

    static inline bool pointinsidecylinder(vec pt, const vec center, float height, float radius)
    {
        if(pt.z < center.z-height/2 || pt.z > center.z+height/2) return false;
        pt.sub(center);
        return pt.x*pt.x + pt.y*pt.y <= radius*radius;
    }

    void checkaffinity(gameent *d, int i)
    {
        if(!(AA(d->actortype, abilities)&(1<<A_A_AFFINITY))) return;
        bomberstate::flag &f = st.flags[i];
        if(f.owner)
        {
            if(!d->ai || f.owner != d) return;
            int hp = max(d->gethealth(game::gamemode, game::mutators)/3, 1);
            bool forever = m_ffa(game::gamemode, game::mutators) || d->health >= hp || findtarget(d) < 0;
            if(!carrytime && forever) return;
            int takemillis = lastmillis-f.taketime, length = forever ? carrytime-550-bomberlockondelay : min(carrytime, 1000);
            if(takemillis >= length)
            {
                if(d->action[AC_AFFINITY])
                {
                    if((carrytime && takemillis >= carrytime-500) || lastmillis-d->actiontime[AC_AFFINITY] >= bomberlockondelay)
                        d->action[AC_AFFINITY] = false;
                }
                else
                {
                    d->action[AC_AFFINITY] = true;
                    d->actiontime[AC_AFFINITY] = lastmillis;
                }
            }
            return;
        }
        if(!f.droptime && m_bb_assault(game::gamemode, game::mutators) && d->team == T_ALPHA && bomberassaultreset) return;
        if(f.lastowner == d && f.droptime && lastmillis-f.droptime <= bomberpickupdelay) return;
        float radius = enttype[AFFINITY].radius;
        if(pointinsidecylinder(f.pos(), d->center(), d->height+radius/2, radius/2))
            client::addmsg(N_TAKEAFFIN, "ri2", d->clientnum, i);
    }

    void update()
    {
        gameent *d = NULL;
        int numdyn = game::numdynents();
        loopj(numdyn) if(((d = (gameent *)game::iterdynents(j))) && d->state == CS_ALIVE && (d == game::player1 || d->ai)) dropaffinity(d);
        loopv(st.flags)
        {
            bomberstate::flag &f = st.flags[i];
            if(!f.enabled || !isbomberaffinity(f)) continue;
            if(f.droptime)
            {
                vec pos = f.pos();
                f.distance += f.droploc.dist(pos);
                f.droploc = pos;
                if(f.lastowner && (f.lastowner == game::player1 || f.lastowner->ai) && f.proj && (!f.movetime || totalmillis-f.movetime >= 40))
                {
                    f.inertia = f.proj->vel;
                    f.movetime = totalmillis-(totalmillis%40);
                    client::addmsg(N_MOVEAFFIN, "ri8", f.lastowner->clientnum, i, int(f.droploc.x*DMF), int(f.droploc.y*DMF), int(f.droploc.z*DMF), int(f.inertia.x*DMF), int(f.inertia.y*DMF), int(f.inertia.z*DMF));
                }
            }
            loopj(numdyn) if(((d = (gameent *)game::iterdynents(j))) && d->state == CS_ALIVE && (d == game::player1 || d->ai)) checkaffinity(d, i);
        }
    }

    bool aihomerun(gameent *d, ai::aistate &b)
    {
        if(d->actortype < A_ENEMY && m_team(game::gamemode, game::mutators) && !m_bb_hold(game::gamemode, game::mutators) && (!m_bb_assault(game::gamemode, game::mutators) || d->team != T_ALPHA))
        {
            int goal = -1;
            vec pos = d->feetpos();
            loopv(st.flags)
            {
                bomberstate::flag &g = st.flags[i];
                if(isbombertarg(g, d->team) && (goal < 0 || g.pos().squaredist(pos) < st.flags[goal].pos().squaredist(pos)))
                    goal = i;
            }
            if(st.flags.inrange(goal) && ai::makeroute(d, b, st.flags[goal].pos()))
            {
                d->ai->switchstate(b, ai::AI_S_PURSUE, ai::AI_T_AFFINITY, goal, ai::AI_A_HASTE);
                return true;
            }
        }
        if(b.type == ai::AI_S_PURSUE && b.targtype == ai::AI_T_NODE) return true; // we already did this..
        if(ai::randomnode(d, b, ai::ALERTMIN, 1e16f))
        {
            d->ai->switchstate(b, ai::AI_S_PURSUE, ai::AI_T_NODE, d->ai->route[0], ai::AI_A_HASTE);
            return true;
        }
        return false;
    }

    bool aicheck(gameent *d, ai::aistate &b)
    {
        if(d->actortype != A_BOT) return false;
        static vector<int> taken; taken.setsize(0);
        loopv(st.flags)
        {
            bomberstate::flag &g = st.flags[i];
            if(g.owner == d) return aihomerun(d, b);
            else if((g.owner && g.owner->team != d->team) || g.droptime) taken.add(i);
        }
        if(!ai::badhealth(d)) while(!taken.empty())
        {
            int flag = taken.length() > 2 ? rnd(taken.length()) : 0;
            if(ai::makeroute(d, b, st.flags[taken[flag]].pos()))
            {
                d->ai->switchstate(b, ai::AI_S_PURSUE, ai::AI_T_AFFINITY, taken[flag], ai::AI_A_HASTE);
                return true;
            }
            else taken.remove(flag);
        }
        return false;
    }

    void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests)
    {
        vec pos = d->feetpos();
        loopvj(st.flags)
        {
            bomberstate::flag &f = st.flags[j];
            if(!f.enabled) continue;
            bool home = isbomberhome(f, d->team) || isbombertarg(f, d->team);
            if(d->actortype == A_BOT && m_duke(game::gamemode, game::mutators) && home) continue;
            static vector<int> targets; // build a list of others who are interested in this
            targets.setsize(0);
            bool regen = d->actortype != A_BOT || f.team != d->team || !m_regen(game::gamemode, game::mutators) || d->health >= d->gethealth(game::gamemode, game::mutators);
            ai::checkothers(targets, d, home || d->actortype != A_BOT ? ai::AI_S_DEFEND : ai::AI_S_PURSUE, ai::AI_T_AFFINITY, j, true);
            if(d->actortype == A_BOT)
            {
                gameent *e = NULL;
                int numdyns = game::numdynents();
                float mindist = enttype[AFFINITY].radius*4; mindist *= mindist;
                loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && !e->ai && e->state == CS_ALIVE && d->team == e->team)
                {
                    if(targets.find(e->clientnum) < 0 && (f.owner == e || e->feetpos().squaredist(f.pos()) <= mindist))
                        targets.add(e->clientnum);
                }
            }
            if(home)
            {
                bool guard = false;
                if(f.owner || f.droptime || targets.empty()) guard = true;
                else if(d->hasweap(ai::weappref(d), m_weapon(d->actortype, game::gamemode, game::mutators)))
                { // see if we can relieve someone who only has a piece of crap
                    gameent *t;
                    loopvk(targets) if((t = game::getclient(targets[k])))
                    {
                        if((t->ai && !t->hasweap(ai::weappref(t), m_weapon(t->actortype, game::gamemode, game::mutators))) || (!t->ai && t->weapselect < W_OFFSET))
                        {
                            guard = true;
                            break;
                        }
                    }
                }
                if(guard)
                { // defend the flag
                    ai::interest &n = interests.add();
                    n.state = ai::AI_S_DEFEND;
                    n.node = ai::closestwaypoint(f.pos(), ai::CLOSEDIST, true);
                    n.target = j;
                    n.targtype = ai::AI_T_AFFINITY;
                    n.score = pos.squaredist(f.pos())/(!regen ? 100.f : 1.f);
                    n.tolerance = f.team != d->team ? 0.5f : 0.25f;
                    n.team = true;
                    n.acttype = ai::AI_A_PROTECT;
                }
            }
            else if(isbomberaffinity(f))
            {
                if(targets.empty())
                { // attack the flag
                    ai::interest &n = interests.add();
                    n.state = d->actortype == A_BOT ? ai::AI_S_PURSUE : ai::AI_S_DEFEND;
                    n.node = ai::closestwaypoint(f.pos(), ai::CLOSEDIST, true);
                    n.target = j;
                    n.targtype = ai::AI_T_AFFINITY;
                    n.score = pos.squaredist(f.pos());
                    n.tolerance = 0.5f;
                    n.team = true;
                }
                else
                { // help by defending the attacker
                    gameent *t;
                    loopvk(targets) if((t = game::getclient(targets[k])))
                    {
                        ai::interest &n = interests.add();
                        bool team = d->team == t->team;
                        if(d->actortype == A_BOT && m_duke(game::gamemode, game::mutators) && team) continue;
                        n.state = team ? ai::AI_S_DEFEND : ai::AI_S_PURSUE;
                        n.node = t->lastnode;
                        n.target = t->clientnum;
                        n.targtype = ai::AI_T_ACTOR;
                        n.score = d->o.squaredist(t->o);
                        n.tolerance = 0.5f;
                        n.team = team;
                        if(team) n.acttype = ai::AI_A_PROTECT;
                    }
                }
            }
        }
    }

    bool aidefense(gameent *d, ai::aistate &b)
    {
        if(d->actortype == A_BOT)
        {
            loopv(st.flags) if(st.flags[i].owner == d) return aihomerun(d, b);
            if(m_duke(game::gamemode, game::mutators) && b.owner < 0) return false;
        }
        if(st.flags.inrange(b.target))
        {
            bomberstate::flag &f = st.flags[b.target];
            if(isbomberaffinity(f) && f.owner && d->team != f.owner->team && ai::violence(d, b, f.owner, 4)) return true;
            int walk = f.owner && f.owner->team != d->team ? 1 : 0;
            if(d->actortype == A_BOT)
            {
                if((!m_regen(game::gamemode, game::mutators) || d->health >= d->gethealth(game::gamemode, game::mutators)) && lastmillis-b.millis >= (201-d->skill)*33)
                {
                    if(b.owner < 0)
                    {
                        static vector<int> targets; // build a list of others who are interested in this
                        targets.setsize(0);
                        ai::checkothers(targets, d, ai::AI_S_DEFEND, ai::AI_T_AFFINITY, b.target, true);
                        gameent *e = NULL;
                        int numdyns = game::numdynents();
                        float mindist = enttype[AFFINITY].radius*4; mindist *= mindist;
                        loopi(numdyns) if((e = (gameent *)game::iterdynents(i)) && !e->ai && e->state == CS_ALIVE && d->team == e->team)
                        {
                            if(targets.find(e->clientnum) < 0 && (f.owner == e || e->feetpos().squaredist(f.pos()) <= mindist))
                                targets.add(e->clientnum);
                        }
                        if(!targets.empty())
                        {
                            d->ai->tryreset = true; // re-evaluate so as not to herd
                            return true;
                        }
                        else
                        {
                            walk = 2;
                            b.millis = lastmillis;
                        }
                    }
                    else
                    {
                        walk = 2;
                        b.millis = lastmillis;
                    }
                }
            }
            return ai::defense(d, b, f.pos(), enttype[AFFINITY].radius, enttype[AFFINITY].radius*walk*8, walk);
        }
        return false;
    }

    bool aicheckpos(gameent *d, ai::aistate &b)
    {
        if(!st.flags.inrange(b.target)) return false;
        bomberstate::flag &f = st.flags[b.target];
        if(!f.enabled || !isbomberaffinity(f)) return false;
        if(f.pos().dist(d->feetpos()) > ai::WAYPOINTRADIUS*2) return false;
        d->ai->spot = f.pos();
        d->ai->targnode = -1;
        return true;
    }

    bool aipursue(gameent *d, ai::aistate &b)
    {
        if(!st.flags.inrange(b.target) || d->actortype != A_BOT) return false;
        bomberstate::flag &f = st.flags[b.target];
        if(!f.enabled) return false;
        if(isbomberaffinity(f))
        {
            if(f.owner)
            {
                if(d == f.owner) return aihomerun(d, b);
                else if(d->team != f.owner->team) return ai::violence(d, b, f.owner, 4);
                else return ai::defense(d, b, f.pos());
            }
            return ai::makeroute(d, b, f.pos());
        }
        if(isbombertarg(f, d->team))
        {
            loopv(st.flags) if(st.flags[i].owner == d && ai::makeroute(d, b, f.pos()))
            {
                b.acttype = ai::AI_A_HASTE;
                return true;
            }
        }
        if(b.owner >= 0) return ai::makeroute(d, b, f.pos());
        return false;
    }
}
