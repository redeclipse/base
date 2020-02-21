#include "game.h"

namespace entities
{
    int firstenttype[MAXENTTYPES], firstusetype[EU_MAX], lastenttype[MAXENTTYPES], lastusetype[EU_MAX], numactors = 0, lastroutenode = -1, lastroutefloor = -1, lastroutetime = 0;

    vector<extentity *> ents;
    vector<int> airnodes;
    vector<inanimate *> inanimates;

    VAR(IDF_PERSIST, showentmodels, 0, 1, 2);
    VAR(IDF_PERSIST, showentdescs, 0, 2, 3);
    VAR(IDF_PERSIST, showentinfo, 0, 21, 127);
    VAR(IDF_PERSIST, showentattrinfo, 0, 7, 7);
    VAR(IDF_PERSIST, showentweapons, 0, 0, 2);

    VAR(IDF_PERSIST, showentdir, 0, 1, 3); // 0 = off, 1 = only selected, 2 = always when editing, 3 = always in editmode
    VAR(IDF_PERSIST, showentradius, 0, 1, 3);
    VAR(IDF_PERSIST, showentlinks, 0, 1, 3);
    VAR(IDF_PERSIST, showentinterval, 0, 32, VAR_MAX);
    VAR(IDF_PERSIST, showentdist, 0, 512, VAR_MAX);
    VAR(IDF_PERSIST, showentfull, 0, 0, 1);
    FVAR(IDF_PERSIST, showentsize, 0, 3, 10);
    FVAR(IDF_PERSIST, showentavailable, 0, 1, 1);
    FVAR(IDF_PERSIST, showentunavailable, 0, 0.15f, 1);

    FVAR(IDF_PERSIST, entselsize, 0, 1.5f, FVAR_MAX);
    FVAR(IDF_PERSIST, entselsizetop, 0, 3, FVAR_MAX);
    FVAR(IDF_PERSIST, entdirsize, 0, 10, FVAR_MAX);

    VAR(IDF_PERSIST|IDF_HEX, entselcolour, 0, 0xFF00FF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, entselcolourtop, 0, 0xFF88FF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, entlinkcolour, 0, 0xFF00FF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, entlinkcolourboth, 0, 0xFF88FF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, entdircolour, 0, 0x88FF88, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, entradiuscolour, 0, 0x88FF88, 0xFFFFFF);

    VAR(IDF_PERSIST, simpleitems, 0, 0, 2); // 0 = items are models, 1 = items are icons, 2 = items are off and only halos appear
    FVAR(IDF_PERSIST, simpleitemsize, 0, 2, 8);
    FVAR(IDF_PERSIST, simpleitemblend, 0, 1, 1);
    FVAR(IDF_PERSIST, simpleitemhalo, 0, 0.25f, 1);

    FVAR(IDF_PERSIST, haloitemsize, 0, 1, 8);
    FVAR(IDF_PERSIST, haloitemblend, 0, 0.7f, 1);

    VARF(0, routeid, -1, -1, VAR_MAX, lastroutenode = -1; lastroutetime = 0; airnodes.setsize(0)); // selected route in race
    VARF(0, droproute, 0, 0, 1, lastroutenode = -1; lastroutetime = 0; airnodes.setsize(0); if(routeid < 0) routeid = 0);
    VAR(IDF_HEX, routecolour, 0, 0xFF22FF, 0xFFFFFF);
    VAR(0, droproutedist, 1, 16, VAR_MAX);
    VAR(0, routemaxdist, 0, 64, VAR_MAX);

    struct rail
    {
        int ent, length, rotstart, rotend, rotlen, flags, animtype;
        float yaw, pitch;
        vec pos, dir;

        rail() : ent(-1), length(0), rotstart(0), rotend(0), rotlen(0), flags(0), animtype(0), yaw(0), pitch(0), pos(0, 0, 0), dir(0, 0, 0) {}
        rail(int n, const vec &o, int d = 0, int f = 0, int a = 0) : ent(n), length(d), rotstart(0), rotend(0), rotlen(0), flags(f), animtype(a), yaw(0), pitch(0), pos(o), dir(0, 0, 0) {}
        ~rail() {}
    };

    struct railway
    {
        int ent, ret, flags, length[2], lastsecs, millis, coltype, animtype, animtime;
        float yaw, pitch, lastyaw, lastpitch;
        vec pos, dir, offset, lastoffset;

        vector<rail> rails;
        vector<int> parents;

        railway() : ent(-1), ret(0), flags(0), lastsecs(0), millis(0), coltype(0), animtype(0), animtime(0), yaw(0), pitch(0), lastyaw(0), lastpitch(0), pos(0, 0, 0), dir(0, 0, 0), offset(0, 0, 0), lastoffset(0, 0, 0) { reset(); }
        railway(int n, int f = 0, int c = 0) : ent(n), ret(0), flags(f), lastsecs(0), millis(0), coltype(c), animtype(0), animtime(0), yaw(0), pitch(0), lastyaw(0), lastpitch(0), pos(0, 0, 0), dir(0, 0, 0), offset(0, 0, 0), lastoffset(0, 0, 0) { reset(); }

        ~railway()
        {
            loopv(inanimates)
            {
                inanimate *m = inanimates[i];
                if(m->control != INANIMATE_RAIL || parents.find(m->ent) < 0) continue;
                DELETEP(m);
                inanimates.remove(i--);
            }
        }

        void reset()
        {
            rails.setsize(0);
            parents.setsize(0);
            loopi(2) length[i] = 0;
        }

        void clear()
        {
            ret = 0;
            rails.setsize(0);
            loopi(2) length[i] = 0;
        }

        int findchild(int n)
        {
            loopv(rails) if(rails[i].ent == n) return i;
            return -1;
        }

        int findparent(int n)
        {
            loopv(parents) if(parents[i] == n) return i;
            return -1;
        }

        void addparent(int n)
        {
            if(parents.find(n) < 0) parents.add(n);
        }

        rail &getrail(int cur, int offset, int iter)
        {
            int index = cur;

            if(offset == -1 && ((cur == ret && iter) || (!iter && !cur)))
                index = rails.length() - 1;
            else
            {
                index += offset;
                if(index >= rails.length()) index %= rails.length() - ret;
            }

            return rails[index];
        }

        bool build()
        {
            clear();
            if(!ents.inrange(ent)) return false;

            for(int i = ent; ents.inrange(i); )
            { // build the rails for this line
                gameentity &e = *(gameentity *)ents[i];
                rails.add(rail(i, e.o, max(e.attrs[0], 0), e.attrs[1], clamp(e.attrs[7], 0, int(ANIM_MAX-1))));
                i = -1;

                loopvj(e.links)
                {
                    int link = e.links[j];
                    if(!ents.inrange(link) || ents[link]->type != RAIL) continue;
                    int cur = findchild(link);
                    if(cur >= 0)
                    {
                        ret = cur;
                        break;
                    }
                    i = link;
                    break;
                }
            }

            if(!rails.empty())
            { // calculate the telemetry of the line
                if(ret < 0) ret = 0;
                loopv(rails)
                {
                    rail &r = rails[i], &s = rails.inrange(i+1) ? rails[i+1] : rails[ret];
                    if(flags&(1<<RAIL_YAW) || flags&(1<<RAIL_PITCH))
                    {
                        gameentity &e = *(gameentity *)ents[r.ent];
                        r.rotstart = clamp(e.attrs[5], 0, r.length);
                        r.rotend = clamp(r.rotstart+(e.attrs[4] > 0 ? e.attrs[4] : r.length), r.rotstart, r.length);
                        r.rotlen = clamp(r.rotend-r.rotstart, 0, r.length);

                        if(flags&(1<<RAIL_SEEK))
                        {
                            r.dir = vec(s.pos).sub(r.pos).safenormalize();
                            vectoyawpitch(r.dir, r.yaw, r.pitch);
                        }
                        else
                        {
                            if(flags&(1<<RAIL_YAW)) r.yaw = e.attrs[2];
                            if(flags&(1<<RAIL_PITCH)) r.pitch = e.attrs[3];
                            r.dir = vec(r.yaw*RAD, r.pitch*RAD);
                        }
                    }
                    length[0] += r.length;
                    if(i >= ret) length[1] += r.length;
                }
                if(length[0] > 0) return true;
            }

            return false;
        }

        bool run(int secs)
        {
            if(rails.empty()) return false;
            if(lastsecs >= secs)
            { // rail has already run this timestep
                loopv(parents)
                {
                    int parent = parents[i];
                    if(!ents.inrange(parent))
                    {
                        parents.remove(i--);
                        continue;
                    }
                    loopvj(inanimates) if(inanimates[j]->control == INANIMATE_RAIL && inanimates[j]->ent == parent)
                    {
                        inanimate *m = inanimates[j];
                        m->yawed = m->pitched = 0;
                        m->moved = m->resized = vec(0, 0, 0);
                        break;
                    }
                }
                return true;
            }

            int elapsed = secs, start = 0, iter = 0;
            if(elapsed >= length[0])
            { // allow the loop point to be different from the start
                elapsed -= length[0];
                start = ret;
                iter++;
            }

            lastoffset = offset;
            lastyaw = yaw;
            lastpitch = pitch;

            millis = elapsed%length[iter];
            offset = rails[start].pos;
            if(flags&(1<<RAIL_YAW) || flags&(1<<RAIL_PITCH)) dir = rails[start].dir;

            int span = 0, anim = 0;
            bool moved = false;
            for(int i = start; i < rails.length(); i++)
            { // look for the station on the timetable
                rail &rcur = rails[i], &rnext = getrail(i, 1, iter);

                if(rcur.length > 0 && millis <= span+rcur.length)
                { // interpolate toward the next station
                    rail &rprev = getrail(i, -1, iter), &rnext2 = getrail(i,  2, iter);

                    int step = millis-span;
                    float amt = step/float(rcur.length);

                    moved = true;
                    anim = rcur.animtype;
                    if(flags&(1<<RAIL_SPLINE))
                    {
                        vec spline[4] = { rprev.pos, rcur.pos, rnext.pos, rnext2.pos };
                        offset = catmullrom(spline, amt);
                    }
                    else
                    {
                        vec dest = vec(rnext.pos).sub(rcur.pos);
                        offset = vec(rcur.pos).add(dest.mul(amt));
                    }

                    if(flags&(1<<RAIL_YAW) || flags&(1<<RAIL_PITCH))
                    {
                        if(step >= rcur.rotend) dir = rnext.dir;
                        else if(step >= rcur.rotstart)
                        {
                            float part = (step-rcur.rotstart)/float(max(rcur.rotlen, 1));
                            dir = vec(rcur.dir).mul(1-part).add(vec(rnext.dir).mul(part)).safenormalize();
                        }
                    }
                    break;
                }

                offset = rnext.pos;
                anim = rnext.animtype;
                if(flags&(1<<RAIL_YAW) || flags&(1<<RAIL_PITCH)) dir = rnext.dir;
                span += rcur.length;
            }

            offset.sub(rails[0].pos); // all coordinates translate based on first rail
            if(flags&(1<<RAIL_YAW) || flags&(1<<RAIL_PITCH)) vectoyawpitch(dir, yaw, pitch);
            if(anim != animtype)
            {
                animtype = anim;
                animtime = lastmillis;
            }

            loopv(parents)
            {
                int parent = parents[i];
                if(!ents.inrange(parent))
                {
                    parents.remove(i--);
                    continue;
                }

                gameentity &e = *(gameentity *)ents[parent];
                e.offset = offset;
                if(flags&(1<<RAIL_YAW)) e.yaw = yaw;
                if(flags&(1<<RAIL_PITCH)) e.pitch = pitch;

                if(e.type == MAPMODEL && !(e.flags&EF_NOCOLLIDE))
                {
                    mapmodelinfo *mmi = getmminfo(e.attrs[0]);
                    if(!mmi || !mmi->m) continue;

                    inanimate *m = NULL;
                    loopvj(inanimates) if(inanimates[j]->control == INANIMATE_RAIL && inanimates[j]->ent == parent)
                    {
                        m = inanimates[j];
                        break;
                    }
                    if(!m)
                    {
                        m = new inanimate;
                        m->ent = parent;
                        m->control = INANIMATE_RAIL;
                        m->coltype = coltype;
                        if(mmi->m->collide != COLLIDE_ELLIPSE) m->collidetype = COLLIDE_OBB;
                        inanimates.add(m);
                    }

                    m->o = e.pos();
                    m->yaw = e.attrs[1]+e.yaw;
                    m->pitch = e.attrs[2]+e.pitch;
                    game::fixrange(m->yaw, m->pitch);
                    vec center, radius;
                    mmi->m->collisionbox(center, radius);
                    if(e.attrs[5])
                    {
                        float scale = e.attrs[5]/100.f;
                        center.mul(scale);
                        radius.mul(scale);
                    }
                    rotatebb(center, radius, int(m->yaw), int(m->pitch));

                    vec oldsize(m->xradius, m->yradius, m->height);
                    m->xradius = radius.x + fabs(center.x);
                    m->yradius = radius.y + fabs(center.y);
                    m->radius = m->collidetype == COLLIDE_OBB ? sqrtf(m->xradius*m->xradius + m->yradius*m->yradius) : max(m->xradius, m->yradius);
                    float offz = center.z-radius.z;
                    m->height = m->zradius = max(offz, 0.f) + radius.z*2*mmi->m->height;
                    m->aboveeye = radius.z*2*(1.0f-mmi->m->height);
                    if(m->aboveeye+m->height <= 0.5f)
                    {
                        float zrad = (0.5f-(m->aboveeye+m->height))/2;
                        m->aboveeye += zrad;
                        m->height += zrad;
                    }
                    m->o.z += m->height;
                    if(offz < 0) m->o.z += offz;

                    if(lastsecs && moved)
                    {
                        m->yawed = yaw-lastyaw;
                        m->pitched = pitch-lastpitch;
                        m->moved = vec(offset).sub(lastoffset);
                    }
                    else
                    {
                        m->yawed = m->pitched = 0;
                        m->moved = vec(0, 0, 0);
                    }
                    m->resized = vec(m->xradius, m->yradius, m->height).sub(oldsize);
                }
            }

            lastsecs = secs;
            return true;
        }
    };

    int railbuilt = 0;
    vector<railway> railways;

    int findrail(int n)
    {
        loopv(railways) if(railways[i].ent == n) return i;
        return -1;
    }

    int findrailparent(int n)
    {
        loopv(railways) if(railways[i].findparent(n) >= 0) return i;
        return -1;
    }

    int findrailchild(int n)
    {
        loopv(railways) if(railways[i].findchild(n) >= 0) return i;
        return -1;
    }

    void resetrails()
    {
        railways.setsize(0);
        railbuilt = 0;
    }

    void fixrails(int n)
    {
        if(!ents.inrange(n)) return;
        loopv(railways) if(railways[i].ent == n || railways[i].findchild(n) >= 0 || railways[i].findparent(n) >= 0) railways.remove(i--);
        cleardynentcache();
        railbuilt = 0;
    }

    void makerail(int n)
    {
        gameentity &e = *(gameentity *)ents[n];
        loopv(e.links)
        {
            int parent = e.links[i];
            if(!ents.inrange(parent) || ents[parent]->type == RAIL || !(enttype[RAIL].canlink&(1<<ents[parent]->type))) continue;
            gameentity &f = *(gameentity *)ents[parent];

            if(enttype[f.type].modesattr >= 0 && !m_check(f.attrs[enttype[f.type].modesattr], f.attrs[enttype[f.type].modesattr+1], game::gamemode, game::mutators)) continue;
            if(enttype[f.type].mvattr >= 0 && !checkmapvariant(f.attrs[enttype[f.type].mvattr])) continue;
            if(enttype[f.type].fxattr >= 0 && !checkmapeffects(f.attrs[enttype[f.type].fxattr])) continue;

            int cur = findrail(n);
            railway &w = railways.inrange(cur) ? railways[cur] : railways.add(railway(n, e.attrs[1], e.attrs[6]));
            w.addparent(parent);
            f.flags |= EF_DYNAMIC;
        }
    }

    void buildrails()
    {
        loopv(ents) ents[i]->flags &= ~EF_DYNAMIC;
        loopv(ents) if(ents[i]->type == RAIL) makerail(i);
        loopv(railways) if(!railways[i].build()) railways.remove(i--);
        railbuilt = totalmillis ? totalmillis : 1;
    }

    void runrails()
    {
        int secs = game::gametimeelapsed();
        if(!railbuilt) buildrails();
        loopv(railways) if(!railways[i].run(secs))
        {
            railways.remove(i--);
            railbuilt = 0;
        }
    }

    void initrails()
    {
        resetrails();
        buildrails();
    }

    void runinanimates()
    {
        loopv(inanimates)
        {
            inanimate *m = inanimates[i];
            bool moved = !m->moved.iszero(), resized = !m->resized.iszero();
            if(!moved && !resized) continue;
            vec origpos = m->o;
            float origr = m->radius, origx = m->xradius, origy = m->yradius, origh = m->height;
            int numdynents = game::numdynents();

            if(moved) m->o.sub(m->moved);
            if(resized)
            {
                m->xradius -= m->resized.x;
                m->yradius -= m->resized.y;
                m->radius = m->collidetype == COLLIDE_OBB ? sqrtf(m->xradius*m->xradius + m->yradius*m->yradius) : max(m->xradius, m->yradius);
                m->height -= m->resized.z;
            }

            for(int secs = curtime; secs > 0; )
            {
                int step = min(secs, physics::physframetime);
                float part = step/float(curtime);
                vec dir = moved ? vec(m->moved).mul(part) : vec(0, 0, 0), resize = resized ? vec(m->resized).mul(part) : vec(0, 0, 0), prevpos = m->o;
                float prevx = m->xradius, prevy = m->yradius, prevh = m->height;

                if(moved) m->o.add(dir);
                if(resized)
                {
                    m->xradius += resize.x;
                    m->yradius += resize.y;
                    m->radius = m->collidetype == COLLIDE_OBB ? sqrtf(m->xradius*m->xradius + m->yradius*m->yradius) : max(m->xradius, m->yradius);
                    m->height += resize.z;
                }

                loopj(numdynents)
                {
                    gameent *d = (gameent *)game::iterdynents(j);
                    if(!d || d->state != CS_ALIVE || m->findpassenger(d) >= 0) continue;

                    vec oldpos = d->o, oldnew = d->newpos, rescale = moved ? vec(d->o).sub(m->o) : vec(0, 0, 0);
                    if(resized && !rescale.iszero()) rescale.normalize().mul(resize);
                    bool under = d->o.z <= prevpos.z-prevh && d->o.x >= prevpos.x-prevx-d->xradius && d->o.x <= prevpos.x+prevx+d->xradius && d->o.y >= prevpos.y-prevy-d->yradius && d->o.y <= prevpos.y+prevy+d->yradius;

                    m->coltarget = d; // restricts inanimate collisions to this entity, and filters out the reverse collision

                    loopk(4) if(collide(m, vec(0, 0, 0), 0, true, true, 0, false))
                    {
                        if(k%2 ? !moved : !resized) continue;
                        if(m->coltype&(1<<INANIMATE_C_KILL)) game::suicide(d, HIT(TOUCH));

                        vec push = k%2 ? dir : rescale;
                        if(push.z > 0 || !under) push.z = 0;
                        if(push.iszero()) continue;
                        if(moved)
                        {
                            d->o.add(push);
                            d->newpos.add(push);
                        }

                        if(collide(d))
                        {
                            bool crush = true;
                            if(collidewall.z >= physics::slopez)
                            {
                                vec proj = vec(push).project(collidewall);
                                if(!proj.iszero())
                                {
                                    if(moved)
                                    {
                                        d->o = vec(oldpos).add(proj);
                                        d->newpos = vec(oldnew).add(proj);
                                    }
                                    if(!collide(d)) crush = false;
                                }
                            }

                            if(crush)
                            {
                                if(moved)
                                {
                                    d->o = oldpos;
                                    d->newpos = oldnew;
                                }
                                game::suicide(d, HIT(CRUSH));
                                break;
                            }

                        }
                        if(moved)
                        {
                            oldpos = d->o;
                            oldnew = d->newpos;
                        }
                    }
                }
                m->coltarget = NULL;
                secs -= step;
            }

            if(moved) m->o = origpos;
            if(resized)
            {
                m->xradius = origx;
                m->yradius = origy;
                m->radius = origr;
                m->height = origh;
            }

            if(moved && !(m->coltype&(1<<INANIMATE_C_NOPASS))) loopvj(m->passengers)
            {
                passenger &p = m->passengers[j];
                physent *d = p.ent;
                if(d->state != CS_ALIVE) continue;

                vec dir = vec(p.offset).rotate_around_z(m->yawed*RAD).sub(p.offset).add(m->moved).addz(m->resized.z);
                if(dir.iszero()) continue;

                vec oldpos = d->o, oldnew = d->newpos;
                m->coltarget = d; // filter collisions from the passenger

                for(int secs = curtime; secs > 0; )
                {
                    int step = min(secs, physics::physframetime);
                    float part = step/float(curtime);

                    vec curdir = vec(dir).mul(part);
                    d->o.add(curdir);
                    d->newpos.add(curdir);

                    if(collide(d) && !gameent::is(collideplayer))
                    {
                        d->o = oldpos;
                        d->newpos = oldnew;
                        if(gameent::is(d) && collidewall.z < 0) game::suicide((gameent *)d, HIT(CRUSH));
                        break;
                    }

                    oldpos = d->o;
                    oldnew = d->newpos;
                    if(m->yawed != 0) d->yaw += m->yawed*part;
                    if(m->pitched != 0) d->pitch += m->pitched*part;
                    secs -= step;
                }

                m->coltarget = NULL;
                game::fixrange(d->yaw, d->pitch);

            }

            m->passengers.shrink(0);
        }
    }

    void removepassenger(physent *d)
    {
        loopv(inanimates)
        {
            inanimate *m = inanimates[i];
            loopvj(m->passengers) if(m->passengers[j].ent == d) m->passengers.remove(i--);
        }
    }

    vector<extentity *> &getents() { return ents; }
    int firstent(int type) { return type >= 0 && type < MAXENTTYPES ? clamp(firstenttype[type], 0, ents.length()-1) : 0; }
    int firstuse(int type) { return type >= 0 && type < EU_MAX ? clamp(firstusetype[type], 0, ents.length()-1) : 0; }
    int lastent(int type) { return type >= 0 && type < MAXENTTYPES ? clamp(lastenttype[type], 0, ents.length()) : 0; }
    int lastuse(int type) { return type >= 0 && type < EU_MAX ? clamp(lastusetype[type], 0, ents.length()) : 0; }

    int numattrs(int type) { return clamp(type >= 0 && type < MAXENTTYPES ? enttype[type].numattrs : 0, 5, MAXENTATTRS); }
    ICOMMAND(0, entityattrs, "b", (int *n), intret(numattrs(*n)));

    #define ENTTYPE(value) ICOMMAND(0, entity##value, "b", (int *n), intret(*n >= 0 && *n < MAXENTTYPES ? enttype[*n].value : 0));
    ENTTYPE(priority);
    ENTTYPE(links);
    ENTTYPE(radius);
    ENTTYPE(usetype);
    ENTTYPE(modesattr);
    ENTTYPE(idattr);
    ENTTYPE(mvattr);
    ENTTYPE(fxattr);
    ENTTYPE(canlink);
    ENTTYPE(reclink);
    ENTTYPE(canuse);

    ICOMMAND(0, getentinfo, "b", (int *n),
    {
        if(*n < 0) intret(MAXENTTYPES);
        else if(*n < MAXENTTYPES) result(enttype[*n].name);
    });

    const char *getentattribute(int type, int attr, int attr1)
    {
        if(type < 0 || type >= MAXENTTYPES) return "";
        const char *attrname = enttype[type].attrs[attr];
        if(type == PARTICLES) switch(attr1)
        {
            case -1: break; // not given
            case 0: switch(attr) { case 0: break; case 1: attrname = "length"; break; case 2: attrname = "height"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "palette"; break; case 6: attrname = "palindex"; break; case 7: attrname = "size"; break; case 8: attrname = "blend"; break; case 9: attrname = "gravity"; break; case 10: attrname = "velocity"; break; default: if(attr < 11) attrname = ""; break; } break;
            case 1: switch(attr) { case 0: break; case 1: attrname = "dir"; break; case 11: attrname = "millis"; break; case 12: attrname = "variant"; break; default: attrname = ""; } break;
            case 2: switch(attr) { case 0: break; case 1: attrname = "dir"; break; case 11: attrname = "millis"; break; case 12: attrname = "variant"; break; default: attrname = ""; } break;
            case 3: switch(attr) { case 0: break; case 1: attrname = "size"; break; case 2: attrname = "colour"; break; case 3: attrname = "palette"; break; case 4: attrname = "palindex"; break; case 11: attrname = "millis"; break; case 12: attrname = "variant"; break; default: attrname = ""; } break;
            case 4: switch(attr) { case 0: break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "palette"; break; case 7: attrname = "palindex"; break; default: if(attr < 11) attrname = ""; break; } break;
            case 5: switch(attr) { case 0: break; case 1: attrname = "amt"; break; case 2: attrname = "colour"; break; case 3: attrname = "palette"; break; case 4: attrname = "palindex"; break; case 11: attrname = "millis"; break; case 12: attrname = "variant"; break; default: attrname = ""; } break;
            case 6: switch(attr) { case 0: break; case 1: attrname = "amt"; break; case 2: attrname = "colour"; break; case 3: attrname = "colour2"; break; case 4: attrname = "palette1"; break; case 5: attrname = "palindex1"; break; case 6: attrname = "palette2"; break; case 7: attrname = "palindex2"; break; case 11: attrname = "millis"; break; case 12: attrname = "variant"; break; default: attrname = ""; } break;
            case 7: switch(attr) { case 0: break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "palette"; break; case 7: attrname = "palindex"; break; default: if(attr < 11) attrname = ""; break; } break;
            case 8: switch(attr) { case 0: break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; default: if(attr < 11) attrname = ""; break; } break;
            case 9: switch(attr) { case 0: break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; default: if(attr < 11) attrname = ""; break; } break;
            case 10: switch(attr) {case 0: break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; default: if(attr < 11) attrname = ""; break; } break;
            case 11: switch(attr) {case 0: break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; default: if(attr < 11) attrname = ""; break; } break;
            case 12: switch(attr) {case 0: break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; default: if(attr < 11) attrname = ""; break; } break;
            case 13: switch(attr) {case 0: break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; default: if(attr < 11) attrname = ""; break; } break;
            case 14: switch(attr) {case 0: break; case 1: attrname = "radius"; break; case 2: attrname = "height"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "gravity"; break; case 7: attrname = "velocity"; break; case 8: attrname = "palette"; break; case 9: attrname = "palindex"; break; default: if(attr < 11) attrname = ""; break; } break;
            case 15: switch(attr) {case 0: break; case 1: attrname = "radius"; break; case 2: attrname = "height"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "gravity"; break; case 7: attrname = "velocity"; break; case 8: attrname = "palette"; break; case 9: attrname = "palindex"; break; default: if(attr < 11) attrname = ""; break; } break;
            case 32: case 33: case 34: case 35: switch(attr) { case 0: break; case 1: attrname = "red"; break; case 2: attrname = "green"; break; case 3: attrname = "blue"; break; default: if(attr < 11) attrname = ""; break; } break;
            default: break;
        }
        return attrname;
    }

    ICOMMAND(0, getentattr, "bbb", (int *n, int *p, int *a),
    {
        if(*n < 0) intret(MAXENTTYPES);
        else if(*n < MAXENTTYPES)
        {
            if(*p < 0) intret(numattrs(*n));
            else if(*p < numattrs(*n)) result(getentattribute(*n, *p, *a));
        }
    });

    int triggertime(extentity &e, bool delay)
    {
        switch(e.type)
        {
            case TRIGGER: case MAPMODEL: case PARTICLES: case MAPSOUND: case LIGHTFX: case TELEPORT: case PUSHER:
                return delay ? TRIGGERDELAY : TRIGGERTIME;
                break;
            default: break;
        }
        return 0;
    }

    int triggertime(int n, bool delay)
    {
        if(ents.inrange(n)) return triggertime(*ents[n], delay);
        return 0;
    }
    ICOMMAND(0, entitytriggertime, "bi", (int *n, int *d), intret(triggertime(*n, *d!=0)));

    void getentity(int id, int val, int ex)
    {
        if(id < 0) intret(ents.length());
        else if(ents.inrange(id))
        {
            if(val < 0) intret(3);
            else switch(val)
            {
                case 0: intret(ents[id]->type); break; // type
                case 1: // attrs
                {
                    if(ex < 0) intret(ents[id]->attrs.length());
                    else if(ents[id]->attrs.inrange(ex)) intret(ents[id]->attrs[ex]);
                    break;
                }
                case 2: // links
                {
                    if(ex < 0) intret(ents[id]->links.length());
                    else if(ents[id]->links.inrange(ex)) intret(ents[id]->links[ex]);
                    break;
                }
            }
        }
    }
    ICOMMAND(0, getentity, "bbb", (int *id, int *val, int *ex), getentity(*id, *val, *ex));

    const char *entinfo(int type, attrvector &attr, bool full, bool icon)
    {
        if(type < 0 || type >= MAXENTTYPES) return NULL;
        static string entinfostr;
        entinfostr[0] = 0;
        #define addentinfo(s) if(*(s)) \
        { \
            if(entinfostr[0]) concatstring(entinfostr, ", "); \
            concatstring(entinfostr, s); \
        }
        switch(type)
        {
            case PARTICLES:
            {
                switch(attr[0])
                {
                    case 0: addentinfo("fire-plume"); break;
                    case 1: addentinfo("smoke-vent"); break;
                    case 2: addentinfo("water-fountain"); break;
                    case 3: addentinfo("fireball"); break;
                    case 4: addentinfo("tape"); break;
                    case 7: addentinfo("lightning"); break;
                    case 8: addentinfo("fire"); break;
                    case 9: addentinfo("smoke"); break;
                    case 10: addentinfo("water"); break;
                    case 11: addentinfo("plasma"); break;
                    case 12: addentinfo("snow"); break;
                    case 13: addentinfo("sparks"); break;
                    case 14: addentinfo("flames"); break;
                    case 15: addentinfo("smoke-plume"); break;
                    case 6: addentinfo("progress-vs"); break;
                    case 5: addentinfo("progress"); break;
                    case 32: addentinfo("lensflare-plain"); break;
                    case 33: addentinfo("lensflare-sparkle"); break;
                    case 34: addentinfo("lensflare-sun"); break;
                    case 35: addentinfo("lensflare-sparklesun"); break;
                    default: break;
                }
                switch(attr[0])
                {
                    case 4: case 7: case 8: case 9: case 10: case 11: case 12: case 13:
                    {
                        if(attr[1] >= 256)
                        {
                            bool hasval = true;
                            int val = attr[1]-256;
                            switch(val%32)
                            {
                                case 0: case 1: case 2: addentinfo("circle"); break;
                                case 3: case 4: case 5: addentinfo("cylinder-shell"); break;
                                case 6: case 7: case 8: case 9: case 10: case 11: addentinfo("cone-shell"); break;
                                case 12: case 13: case 14: addentinfo("plane-volume"); break;
                                case 15: case 16: case 17: case 18: case 19: case 20: addentinfo("line-volume"); break;
                                case 21: case 22: case 23: hasval = false; addentinfo("sphere"); break;
                                case 24: case 25: case 26: addentinfo("plane-flat"); break;
                                default: hasval = false; addentinfo("default"); break;
                            }
                            if(hasval) switch((val%32)%3)
                            {
                                case 0: addentinfo("x-axis"); break;
                                case 1: addentinfo("y-axis"); break;
                                case 2: addentinfo("z-axis"); break;
                                default: break;
                            }
                            if(val%64 >= 32) addentinfo("inverted");
                            break;
                        }
                        // fall through
                    }
                    case 1: case 2:
                    {
                        switch(attr[1]%3)
                        {
                            case 0: addentinfo("x-axis"); break;
                            case 1: addentinfo("y-axis"); break;
                            case 2: addentinfo("z-axis"); break;
                        }
                        if(attr[1]%6 >= 3) addentinfo("inverted");
                        break;
                    }
                    default: break;
                }
                break;
            }
            case PLAYERSTART: case AFFINITY: case CHECKPOINT:
            {
                if(type != CHECKPOINT)
                {
                    if(valteam(attr[0], T_FIRST))
                    {
                        defformatstring(str, "team %s", game::colourteam(attr[0]));
                        addentinfo(str);
                    }
                }
                else
                {
                    const char *cpnames[CP_MAX+1] = { "respawn", "start", "finish", "last", "" };
                    addentinfo(cpnames[attr[6] < 0 || attr[6] >= CP_MAX ? CP_MAX : attr[6]]);
                }
                break;
            }
            case LIGHT:
            {
                if(full)
                {
                    if(attr[6]&(1<<LIGHT_NOSHADOW)) addentinfo("no-shadow");
                    if(attr[6]&(1<<LIGHT_STATIC)) addentinfo("static");
                    if(attr[6]&(1<<LIGHT_VOLUMETRIC)) addentinfo("volumetric");
                    if(attr[6]&(1<<LIGHT_NOSPEC)) addentinfo("no-specular");

                }
                break;
            }
            case WIND:
            {
                if(full)
                {
                    if(attr[0]&WIND_EMIT_IMPULSE)
                    {
                        addentinfo("impulse");
                    }
                    else addentinfo("smooth-periodic");
                    if(attr[0]&WIND_EMIT_VECTORED)
                    {
                        addentinfo("vectored");
                    }
                    else addentinfo("local");
                }
                break;
            }
            case LIGHTFX:
            {
                if(full)
                {
                    const char *lfxnames[LFX_MAX+1] = { "spotlight", "flicker", "pulse", "glow", "inv-pulse", "inv-glow", "normal" };
                    addentinfo(lfxnames[attr[0] < 0 || attr[0] >= LFX_MAX ? LFX_MAX : attr[0]]);
                    loopi(LFX_MAX-1) if(attr[4]&(1<<(LFX_S_MAX+i))) { defformatstring(ds, "+%s", lfxnames[i+1]); addentinfo(ds); break; }
                    if(attr[4]&LFX_S_RAND1) addentinfo("rnd-min");
                    if(attr[4]&LFX_S_RAND2) addentinfo("rnd-max");
                }
                break;
            }
            case ACTOR:
            {
                if(full && attr[0] >= 0 && attr[0] < A_TOTAL)
                {
                    addentinfo(actors[attr[0]+A_ENEMY].name);
                    addentinfo(W(attr[6] > 0 && attr[6] <= W_ALL ? attr[6]-1 : AA(attr[0]+A_ENEMY, weaponspawn), name));
                }
                break;
            }
            case WEAPON:
            {
                int wattr = m_attr(type, attr[0]);
                if(isweap(wattr))
                {
                    defformatstring(str, "\fs\f[%d]%s%s%s%s\fS", W(wattr, colour), icon ? "\f(" : "", icon ? hud::itemtex(type, wattr) : W(wattr, name), icon ? ")" : "", icon ? W(wattr, longname) : "");
                    addentinfo(str);
                    if(full)
                    {
                        if(attr[1]&W_F_FORCED) addentinfo("forced");
                    }
                }
                break;
            }
            case MAPMODEL:
            {
                mapmodelinfo *mmi = getmminfo(attr[0]);
                if(!mmi) break;
                addentinfo(mmi->name);
                if(full)
                {
                    if(attr[6]&(1<<MDLF_HIDE)) addentinfo("hide");
                    if(attr[6]&(1<<MDLF_NOCLIP)) addentinfo("no-clip");
                    if(attr[6]&(1<<MDLF_NOSHADOW)) addentinfo("no-shadow");
                }
                break;
            }
            case MAPSOUND:
            {
                if(mapsounds.inrange(attr[0]))
                {
                    int samples = mapsounds[attr[0]].samples.length();
                    defformatstring(ds, "%s (%d %s)", mapsounds[attr[0]].name, samples, samples == 1 ? "sample" : "samples");
                    addentinfo(ds);
                }
                if(full)
                {
                    if(attr[4]&SND_NOATTEN) addentinfo("no-atten");
                    if(attr[4]&SND_NODELAY) addentinfo("no-delay");
                    if(attr[4]&SND_NOCULL) addentinfo("no-cull");
                    if(attr[4]&SND_NOPAN) addentinfo("no-pan");
                    if(attr[4]&SND_NODIST) addentinfo("no-dist");
                    if(attr[4]&SND_NOQUIET) addentinfo("no-quiet");
                    if(attr[4]&SND_CLAMPED) addentinfo("clamped");
                }
                break;
            }
            case TRIGGER:
            {
                if(full)
                {
                    const char *trgnames[TR_MAX+1] = { "toggle", "link", "script", "once", "exit", "" }, *actnames[TA_MAX+1] = { "manual", "proximity", "action", "" };
                    addentinfo(trgnames[attr[1] < 0 || attr[1] >= TR_MAX ? TR_MAX : attr[1]]);
                    addentinfo(actnames[attr[2] < 0 || attr[2] >= TA_MAX ? TA_MAX : attr[2]]);
                    if(attr[4] >= 2) addentinfo(attr[4] ? "routed" : "inert");
                    addentinfo(attr[4]%2 ? "on" : "off");
                }
                break;
            }
            case PUSHER:
            {
                if(full) switch(attr[5])
                {
                    case 0: addentinfo("conditional-dir"); break;
                    case 1: addentinfo("add-to-dir"); break;
                    case 2: addentinfo("redirect-dir"); break;
                    case 3: addentinfo("absolute-dir"); break;
                    default: break;
                }
                break;
            }
            case TELEPORT:
            {
                if(full)
                {
                    if(attr[5] >= 3) addentinfo("pos-offset");
                    if(attr[5] >= 6) addentinfo("mod-velocity");
                    switch(attr[5]%3)
                    {
                        case 0: addentinfo("absolute-dir"); break;
                        case 1: addentinfo("relative-dir"); break;
                        case 2: addentinfo("keep-dir"); break;
                        default: break;
                    }
                    const char *telenames[TELE_MAX] = { "no-affinity" };
                    loopj(TELE_MAX) if(attr[8]&(1<<j)) { addentinfo(telenames[j]); }
                }
                break;
            }
            case RAIL:
            {
                if(full)
                {
                    const char *railnames[RAIL_MAX] = { "follow-yaw", "follow-pitch", "seek-next", "spline" };
                    loopj(RAIL_MAX) if(attr[1]&(1<<j)) addentinfo(railnames[j]);

                    const char *railcollides[INANIMATE_C_MAX] = { "touch-kill", "no-passenger" };
                    loopj(INANIMATE_C_MAX) if(attr[6]&(1<<j)) addentinfo(railcollides[j]);

                    if(attr[7] > 0 && attr[7] < ANIM_MAX)
                    {
                        defformatstring(str, "anim: %s", animnames[attr[7]]);
                        addentinfo(str);
                    }
                }
            }
            default: break;
        }
        if(full)
        {
            if(enttype[type].modesattr >= 0)
            {
                int a = attr[enttype[type].modesattr], b = attr[enttype[type].modesattr+1];
                if(a)
                {
                    int mode = a < 0 ? 0-a : a;
                    loopi(G_MAX-G_PLAY) if(mode&(1<<i))
                    {
                        string ds;
                        if(a < 0) formatstring(ds, "not %s", gametype[i+G_PLAY].name);
                        else formatstring(ds, "%s", gametype[i+G_PLAY].name);
                        addentinfo(ds);
                    }
                }
                if(b)
                {
                    int muts = b < 0 ? 0-b : b;
                    loopi(G_M_NUM) if(muts&(1<<i))
                    {
                        string ds;
                        if(b < 0) formatstring(ds, "not %s", mutstype[i].name);
                        else formatstring(ds, "%s", mutstype[i].name);
                        addentinfo(ds);
                    }
                }
            }
            if(enttype[type].fxattr >= 0)
            {
                int a = attr[enttype[type].fxattr];
                if(a < 0)
                {
                    int b = 0;
                    defformatstring(ds, "fx ");
                    loopi(3) if((0-a)&(1<<i))
                    {
                        concformatstring(ds, "%s%d", b ? "+" : "", i+1);
                        b++;
                    }
                    concatstring(ds, " only");
                    addentinfo(ds);
                }
                else if(a > 0)
                {
                    defformatstring(ds, "fx %d", a);
                    addentinfo(ds);
                }
            }
        }
        return entinfostr[0] ? entinfostr : "";
    }

    const char *entinfo(entity &e, bool full, bool icon)
    {
        gameentity &f = (gameentity &)e;
        return entinfo(f.type, f.attrs, full, icon);
    }

    const char *entmdlname(int type, attrvector &attr)
    {
        switch(type)
        {
            case AFFINITY: return "props/flag";
            case PLAYERSTART: return playertypes[0][1];
            case WEAPON:
            {
                int sweap = m_weapon(game::focus->actortype, game::gamemode, game::mutators), weap = m_attr(type, attr[0]);
                if(!isweap(weap)) break;
                const char *mdlname = !showentweapons || (showentweapons != 2 && game::focus->hasweap(weap, sweap)) ? weaptype[weap].ammo : weaptype[weap].item;
                return mdlname && *mdlname ? mdlname : "projectiles/cartridge";
            }
            case ACTOR:
            {
                if(attr[0] < 0 || attr[0] >= A_TOTAL) return "";
                const char *mdl = actors[attr[0]+A_ENEMY].mdl;
                if(!mdl || !*mdl) mdl = playertypes[0][1];
                return mdl;
            }
            default: break;
        }
        return "";
    }

    void useeffects(gameent *d, int cn, int ent, int ammoamt, bool spawn, int weap, int drop, int ammo)
    {
        gameentity &e = *(gameentity *)ents[ent];
        int sweap = m_weapon(d->actortype, game::gamemode, game::mutators), attr = m_attr(e.type, e.attrs[0]),
            colour = e.type == WEAPON && isweap(attr) ? W(attr, colour) : colourwhite;
        if(e.type == WEAPON && isweap(attr)) d->addicon(eventicon::WEAPON, lastmillis, game::eventiconshort, attr);
        if(isweap(weap))
        {
            d->setweapstate(weap, W_S_SWITCH, W(weap, delayswitch), lastmillis);
            d->weapammo[weap][W_A_CLIP] = -1;
            d->weapammo[weap][W_A_STORE] = 0;
        }
        d->useitem(ent, e.type, attr, ammoamt, sweap, lastmillis, W(attr, delayitem));
        playsound(e.type == WEAPON && attr >= W_OFFSET && attr < W_ALL ? WSND(attr, S_W_USE) : S_ITEMUSE, d->o, d, 0, -1, -1, -1, &d->wschan[WS_MAIN_CHAN]);
        if(game::dynlighteffects) adddynlight(d->center(), enttype[e.type].radius*2, vec::fromcolor(colour).mul(2.f), 250, 250);
        if(ents.inrange(drop) && ents[drop]->type == WEAPON)
        {
            gameentity &f = *(gameentity *)ents[drop];
            attr = m_attr(f.type, f.attrs[0]);
            if(isweap(attr)) projs::drop(d, attr, drop, ammo, d == game::player1 || d->ai, 0, weap);
        }
        if(cn >= 0)
        {
            gameent *m = game::getclient(cn);
            if(m) projs::destruct(m, PRJ_ENT, ent);
        }
        else if(e.spawned() != spawn)
        {
            e.setspawned(spawn);
            e.lastemit = lastmillis;
        }
    }

    /*
    static inline void collateents(octaentities &oe, const vec &pos, float xyrad, float zrad, bool alive, vector<actitem> &actitems)
    {
        vector<extentity *> &ents = entities::getents();
        loopv(oe.other)
        {
            int n = oe.other[i];
            extentity &e = *ents[n];
            if(enttype[e.type].usetype != EU_NONE && (enttype[e.type].usetype != EU_ITEM || (alive && e.spawned())))
            {
                float radius = enttype[e.type].radius;
                switch(e.type)
                {
                    case TRIGGER: case TELEPORT: case PUSHER: if(e.attrs[3] > 0) radius = e.attrs[3]; break;
                    case CHECKPOINT: if(e.attrs[0] > 0) radius = e.attrs[0]; break;
                }
                if(overlapsbox(pos, zrad, xyrad, e.pos(), radius, radius))
                {
                    actitem &t = actitems.add();
                    t.type = actitem::ENT;
                    t.target = n;
                    t.score = pos.squaredist(e.pos());
                }
            }
        }
    }

    static inline void collateents(cube *c, const ivec &o, int size, const ivec &bo, const ivec &br, const vec &pos, float xyrad, float zrad, bool alive, vector<actitem> &actitems)
    {
        loopoctabox(o, size, bo, br)
        {
            if(c[i].ext && c[i].ext->ents) collateents(*c[i].ext->ents, pos, xyrad, zrad, alive, actitems);
            if(c[i].children && size > octaentsize)
            {
                ivec co(i, o.x, o.y, o.z, size);
                collateents(c[i].children, co, size>>1, bo, br, pos, xyrad, zrad, alive, actitems);
            }
        }
    }

    void collateents(const vec &pos, float xyrad, float zrad, bool alive, vector<actitem> &actitems)
    {
        ivec bo = vec(pos).sub(vec(xyrad, xyrad, zrad)),
             br = vec(pos).add(vec(xyrad, xyrad, zrad)).add(1);
        int diff = (bo.x^br.x) | (bo.y^br.y) | (bo.z^br.z) | octaentsize,
            scale = worldscale-1;
        if(diff&~((1<<scale)-1) || uint(bo.x|bo.y|bo.z|br.x|br.y|br.z) >= uint(worldsize))
        {
            collateents(worldroot, ivec(0, 0, 0), 1<<scale, bo, br, pos, xyrad, zrad, alive, actitems);
            return;
        }
        cube *c = &worldroot[octastep(bo.x, bo.y, bo.z, scale)];
        if(c->ext && c->ext->ents) collateents(*c->ext->ents, pos, xyrad, zrad, alive, actitems);
        scale--;
        while(c->children && !(diff&(1<<scale)))
        {
            c = &c->children[octastep(bo.x, bo.y, bo.z, scale)];
            if(c->ext && c->ext->ents) collateents(*c->ext->ents, pos, xyrad, zrad, alive, actitems);
            scale--;
        }
        if(c->children && 1<<scale >= octaentsize) collateents(c->children, ivec(bo).mask(~((2<<scale)-1)), 1<<scale, bo, br, pos, xyrad, zrad, alive, actitems);
    }
    */

    static inline bool sortitems(const actitem &a, const actitem &b)
    {
        return a.score > b.score;
    }

    bool collateitems(dynent *d, vec &pos, float radius, vector<actitem> &actitems)
    {
        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(enttype[e.type].usetype != EU_NONE && (enttype[e.type].usetype != EU_ITEM || (d->state == CS_ALIVE && e.spawned())))
            {
                if(enttype[e.type].mvattr >= 0 && !checkmapvariant(e.attrs[enttype[e.type].mvattr])) continue;
                float eradius = enttype[e.type].radius, edist = pos.dist(e.pos());
                switch(e.type)
                {
                    case TRIGGER: case TELEPORT: case PUSHER: if(e.attrs[3] > 0) eradius = e.attrs[3]; break;
                    case CHECKPOINT: if(e.attrs[0] > 0) eradius = e.attrs[0]; break;
                }
                float diff = edist-radius;
                if(diff > eradius) continue;
                actitem &t = actitems.add();
                t.type = actitem::ENT;
                t.target = i;
                t.score = diff;
            }
        }
        if(d->state == CS_ALIVE) loopv(projs::projs)
        {
            projent &proj = *projs::projs[i];
            if(!proj.owner || proj.projtype != PRJ_ENT || !proj.ready()) continue;
            if(!ents.inrange(proj.id) || enttype[ents[proj.id]->type].usetype != EU_ITEM) continue;
            if(enttype[ents[proj.id]->type].mvattr >= 0 && !checkmapvariant(ents[proj.id]->attrs[enttype[ents[proj.id]->type].mvattr])) continue;
            if(!(enttype[ents[proj.id]->type].canuse&(1<<d->type))) continue;
            //if(!overlapsbox(m, eye, d->radius, proj.o, enttype[ents[proj.id]->type].radius, enttype[ents[proj.id]->type].radius))
            //    continue;
            float eradius = enttype[ents[proj.id]->type].radius, edist = pos.dist(proj.o);
            switch(ents[proj.id]->type)
            {
                case TRIGGER: case TELEPORT: case PUSHER: if(ents[proj.id]->attrs[3] > 0) eradius = ents[proj.id]->attrs[3]; break;
                case CHECKPOINT: if(ents[proj.id]->attrs[0] > 0) eradius = ents[proj.id]->attrs[0]; break;
            }
            float diff = edist-radius;
            if(diff > eradius) continue;
            actitem &t = actitems.add();
            t.type = actitem::PROJ;
            t.target = i;
            t.score = diff;
        }
        if(!actitems.empty())
        {
            actitems.sort(sortitems); // sort items so last is closest
            return true;
        }
        return false;
    }

    int triggerent = -1;
    gameent *triggerclient = NULL;
    ICOMMAND(0, triggerentnum, "", (), intret(triggerent));
    ICOMMAND(0, triggerclientnum, "", (), intret(triggerclient ? triggerclient->clientnum : -1));

    bool cantrigger(int n, gameent *d = NULL)
    {
        gameentity &e = *(gameentity *)ents[n];
        switch(e.type)
        {
            case TRIGGER:
            {
                if(!checkmapvariant(e.attrs[enttype[e.type].mvattr])) return false;
                if(!m_check(e.attrs[5], e.attrs[6], game::gamemode, game::mutators)) return false;
                if(d)
                {
                    int millis = d->lastused(n, true);
                    if(millis && lastmillis-millis < triggertime(e, true)) return false;
                }
                return true;
                break;
            }
            default: if(enttype[e.type].mvattr < 0 || checkmapvariant(e.attrs[enttype[e.type].mvattr])) return true; break;
        }
        return false;
    }

    ICOMMAND(0, triggerents, "ii", (int *id, int *all),
    {
        vector<char> buf;
        loopenti(TRIGGER) if(ents[i]->type == TRIGGER && ents[i]->attrs[0] == *id && (*all || cantrigger(i, game::player1)))
        {
            if(!buf.empty()) buf.add(' ');
            defformatstring(s, "%d", i);
            buf.put(s, strlen(s));
        }
        buf.add('\0');
        commandret->setstr(buf.disown());
    });

    void runtrigger(int n, gameent *d, bool act = true)
    {
        gameentity &e = *(gameentity *)ents[n];
        if(cantrigger(n, d))
        {
            e.lastemit = lastmillis;
            d->setused(n, lastmillis);
            switch(e.attrs[1])
            {
                case TR_EXIT: if(d->actortype >= A_BOT) break;
                case TR_TOGGLE: case TR_LINK: case TR_ONCE:
                {
                    client::addmsg(N_TRIGGER, "ri2", d->clientnum, n);
                    if(!e.spawned() || e.attrs[1] == TR_TOGGLE) setspawn(n, e.spawned() ? 0 : 1);
                    break;
                }
                case TR_SCRIPT:
                {
                    if(d->actortype >= A_BOT) break;
                    defformatstring(s, "on_trigger_%d", e.attrs[0]);
                    triggerent = n;
                    triggerclient = d;
                    RUNWORLD(s);
                    triggerent = -1;
                    triggerclient = NULL;
                    break;
                }
                default: break;
            }
            if(act && e.attrs[2] == TA_ACTION) d->action[AC_USE] = false;
        }
    }

    void runtriggers(int n, gameent *d)
    {
        loopenti(TRIGGER) if(ents[i]->type == TRIGGER && ents[i]->attrs[0] == n && ents[i]->attrs[2] == TA_MANUAL) runtrigger(i, d, false);
    }
    ICOMMAND(0, exectrigger, "i", (int *n), if(identflags&IDF_WORLD) runtriggers(*n, triggerclient ? triggerclient : game::player1));

    bool execitem(int n, int cn, dynent *d, vec &pos, float dist)
    {
        gameentity &e = *(gameentity *)ents[n];
        switch(enttype[e.type].usetype)
        {
            case EU_ITEM:
            {
                if(gameent::is(d) && (e.type != WEAPON || ((gameent *)d)->action[AC_USE]))
                {
                    gameent *f = (gameent *)d;
                    if(game::allowmove(f))
                    {
                        int sweap = m_weapon(f->actortype, game::gamemode, game::mutators), attr = m_attr(e.type, e.attrs[0]);
                        if(!isweap(attr)) return false;
                        if(!f->canuse(game::gamemode, game::mutators, e.type, attr, e.attrs, sweap, lastmillis, (1<<W_S_SWITCH)))
                        {
                            if(e.type != WEAPON) return false;
                            else if(!f->canuse(game::gamemode, game::mutators, e.type, attr, e.attrs, sweap, lastmillis, (1<<W_S_SWITCH)|(1<<W_S_RELOAD))) return true;
                            else if(!isweap(f->weapselect) || f->weapload[f->weapselect][W_A_CLIP] <= 0) return true;
                            else
                            {
                                int offset = f->weapload[f->weapselect][W_A_CLIP];
                                f->weapammo[f->weapselect][W_A_CLIP] = max(f->weapammo[f->weapselect][W_A_CLIP]-offset, 0);
                                if(W(f->weapselect, ammostore) > 0)
                                    f->weapammo[f->weapselect][W_A_STORE] = clamp(f->weapammo[f->weapselect][W_A_STORE]+offset, 0, W(f->weapselect, ammostore));
                                f->weapload[f->weapselect][W_A_CLIP] = -f->weapload[f->weapselect][W_A_CLIP];
                            }
                        }
                        client::addmsg(N_ITEMUSE, "ri4", f->clientnum, lastmillis-game::maptime, cn, n);
                        if(e.type == WEAPON)
                        {
                            f->setweapstate(f->weapselect, W_S_WAIT, PHYSMILLIS, lastmillis);
                            f->action[AC_USE] = false;
                        }
                        return false;
                    }
                    return true;
                }
                break;
            }
            case EU_AUTO:
            {
                if(e.type == TELEPORT)
                {
                    if(!checkmapvariant(e.attrs[enttype[e.type].mvattr])) break;
                    if(e.attrs[8]&(1<<TELE_NOAFFIN))
                    {
                        if(gameent::is(d) && physics::carryaffinity((gameent *)d)) break;
                        if(projent::is(d) && ((projent *)d)->type == PRJ_AFFINITY) break;
                    }
                    int millis = d->lastused(n, true);
                    if(millis && lastmillis-millis < triggertime(e)) break;
                    e.lastemit = lastmillis;
                    static vector<int> teleports;
                    teleports.shrink(0);
                    loopv(e.links)
                        if(e.links[i] != n && ents.inrange(e.links[i]) && ents[e.links[i]]->type == e.type)
                            teleports.add(e.links[i]);
                    if(teleports.empty()) break;
                    vec orig = d->o, ovel = d->vel;
                    float oyaw = d->yaw, opitch = d->pitch;
                    while(!teleports.empty())
                    {
                        int r = rnd(teleports.length()), q = teleports[r];
                        gameentity &f = *(gameentity *)ents[q];
                        d->o = vec(f.pos()).add(f.attrs[5] >= 3 ? vec(orig).sub(e.pos()) : vec(0, 0, d->height*0.5f));
                        float mag = vec(d->vel).add(d->falling).magnitude(), yaw = f.attrs[0] < 0 ? (lastmillis/5)%360 : f.attrs[0], pitch = f.attrs[1];
                        if(!projent::shot(d))
                        {
                            if(f.attrs[2] > 0) mag = max(mag, float(f.attrs[2]));
                            else if(f.attrs[2] < 0) mag = min(mag, float(-f.attrs[2]));
                        }
                        game::fixrange(yaw, pitch);
                        if(mag != 0 && f.attrs[5] < 6) d->vel = vec(yaw*RAD, pitch*RAD).mul(mag);
                        switch(f.attrs[5]%3)
                        {
                            case 2: break; // keep
                            case 1: // relative
                            {
                                float relyaw = (e.attrs[0] < 0 ? (lastmillis/5)%360 : e.attrs[0])-180, relpitch = e.attrs[1];
                                game::fixrange(relyaw, relpitch);
                                d->yaw = yaw+(d->yaw-relyaw);
                                d->pitch = pitch+(d->pitch-relpitch);
                                break;
                            }
                            case 0: default: // absolute
                            {
                                d->yaw = yaw;
                                d->pitch = pitch;
                                break;
                            }
                        }
                        game::fixrange(d->yaw, d->pitch);
                        if(mag == 0) d->vel = vec(0, 0, 0);
                        else if(f.attrs[5] >= 6) d->vel = vec(d->yaw*RAD, d->pitch*RAD).mul(mag);
                        if(physics::entinmap(d, gameent::is(d))) // entinmap first for getting position
                        {
                            f.lastemit = lastmillis;
                            d->setused(n, lastmillis);
                            d->setused(q, lastmillis);
                            d->resetinterp(true);
                            if(d->state == CS_ALIVE)
                            {
                                if(gameent::is(d))
                                {
                                    gameent *g = (gameent *)d;
                                    if(g == game::focus) game::resetcamera();
                                    execlink(g, n, true);
                                    execlink(g, q, true);
                                    g->resetair();
                                    ai::inferwaypoints(g, e.pos(), f.pos(), float(e.attrs[3] ? e.attrs[3] : enttype[e.type].radius)+ai::CLOSEDIST);
                                }
                                else if(projent::is(d))
                                {
                                    projent *g = (projent *)d;
                                    g->from = g->trailpos = g->deltapos = g->o;
                                    g->dest = vec(g->o).add(g->vel);
                                    g->bounced = true;
                                    g->lastbounce = lastmillis;
                                    g->movement = 0;
                                }
                            }
                            else if(gameent::is(d)) warpragdoll(d, d->vel, vec(f.pos()).sub(e.pos()));
                            return false; // gotcha
                        }
                        d->o = orig;
                        d->vel = ovel;
                        d->yaw = oyaw;
                        d->pitch = opitch;
                        teleports.remove(r); // must've really sucked, try another one
                    }
                    if(d->state == CS_ALIVE)
                    { // if we got here, the teleport failed for some reason
                        if(gameent::is(d)) game::suicide((gameent *)d, HIT(SPAWN));
                        else if(projent::is(d))
                        {
                            projent *g = (projent *)d;
                            switch(g->projtype)
                            {
                                case PRJ_ENT: case PRJ_AFFINITY:
                                {
                                    if(!g->beenused)
                                    {
                                        g->beenused = 1;
                                        g->lifetime = min(g->lifetime, g->fadetime);
                                    }
                                    if(g->lifetime > 0) break;
                                }
                                default: g->state = CS_DEAD; g->escaped = true; break;
                            }
                        }
                        else d->state = CS_DEAD;
                    }
                }
                else if(e.type == PUSHER)
                {
                    if(!checkmapvariant(e.attrs[enttype[e.type].mvattr])) break;
                    int millis = d->lastused(n, true);
                    if(e.attrs[5] != 3 && millis && lastmillis-millis < triggertime(e)) break;
                    e.lastemit = lastmillis;
                    d->setused(n, lastmillis);
                    float mag = e.attrs[2] != 0 ? e.attrs[2] : 1, maxrad = e.attrs[3] ? e.attrs[3] : enttype[PUSHER].radius, minrad = e.attrs[4];
                    if(dist > 0 && minrad > 0 && maxrad > minrad && dist > minrad && maxrad >= dist)
                        mag *= 1.f-clamp((dist-minrad)/float(maxrad-minrad), 0.f, 1.f);
                    if(!gameent::is(d)) mag *= d->weight/300.f;
                    vec dir(e.attrs[0]*RAD, e.attrs[1]*RAD), rel = vec(dir).mul(mag);
                    switch(e.attrs[5])
                    {
                        case 0:
                        {
                            loopk(3)
                            {
                                if((d->vel.v[k] > 1e-1f && rel.v[k] < -1e-1f) || (d->vel.v[k] < -1e-1f && rel.v[k] > 1e-1f) || (fabs(rel.v[k]) > fabs(d->vel.v[k])))
                                    d->vel.v[k] = rel.v[k];
                            }
                            break;
                        }
                        case 1: d->vel.add(rel); break;
                        case 2: rel.add(vec(dir).mul(vec(d->vel).add(d->falling).magnitude())); // fall through
                        case 3: d->vel = rel; break;
                        default: break;
                    }
                    if(d->state == CS_ALIVE)
                    {
                        if(gameent::is(d))
                        {
                            gameent *g = (gameent *)d;
                            if(e.attrs[5] != 3 || !millis || lastmillis-millis >= triggertime(e)) execlink(g, n, true);
                            if(e.attrs[10] > 0) g->doimpulse(IM_T_PUSHER, lastmillis+e.attrs[10]);
                            else g->resetair();
                        }
                        else if(projent::is(d))
                        {
                            projent *g = (projent *)d;
                            g->from = g->trailpos = g->deltapos = g->o;
                            g->dest = vec(g->o).add(g->vel);
                            g->bounced = true;
                            g->lastbounce = lastmillis;
                            g->movement = 0;
                        }
                    }
                    else if(gameent::is(d)) warpragdoll(d, d->vel);
                }
                else if(e.type == TRIGGER)
                {
                    if(d->state != CS_ALIVE || !gameent::is(d)) break;
                    if(!checkmapvariant(e.attrs[enttype[e.type].mvattr])) break;
                    gameent *g = (gameent *)d;
                    if((e.attrs[2] == TA_ACTION && g->action[AC_USE] && g == game::player1) || e.attrs[2] == TA_AUTO) runtrigger(n, g);
                }
                else if(e.type == CHECKPOINT)
                {
                    if(!checkmapvariant(e.attrs[enttype[e.type].mvattr])) break;
                    if(d->state != CS_ALIVE || !gameent::is(d) || !m_race(game::gamemode)) break;
                    if(!m_check(e.attrs[3], e.attrs[4], game::gamemode, game::mutators)) break;
                    gameent *g = (gameent *)d;
                    if(g->checkpoint == n || (m_ra_gauntlet(game::gamemode, game::mutators) && g->team != T_ALPHA)) break;
                    if(e.attrs[6] == CP_START)
                    {
                        if(g->cpmillis || (d->vel.iszero() && !d->move && !d->strafe)) break;
                        g->cpmillis = lastmillis;
                    }
                    else if(!g->cpmillis) break;
                    client::addmsg(N_TRIGGER, "ri2", g->clientnum, n);
                    g->checkpoint = n;
                }
                break;
            }
        }
        return false;
    }

    void checkitems(dynent *d)
    {
        static vector<actitem> actitems;
        actitems.setsize(0);
        vec pos = d->center();
        float radius = max(d->xradius, d->yradius);
        if(gameent::is(d)) radius = max(d->height*0.5f, radius);
        if(collateitems(d, pos, radius, actitems))
        {
            bool tried = false;
            while(!actitems.empty())
            {
                actitem &t = actitems.last();
                int ent = -1, cn = -1;
                float dist = 0;
                switch(t.type)
                {
                    case actitem::ENT:
                    {
                        if(!ents.inrange(t.target)) break;
                        ent = t.target;
                        dist = t.score;
                        break;
                    }
                    case actitem::PROJ:
                    {
                        if(!projs::projs.inrange(t.target)) break;
                        projent &proj = *projs::projs[t.target];
                        cn = proj.owner->clientnum;
                        ent = proj.id;
                        dist = t.score;
                        break;
                    }
                    default: break;
                }
                if(ents.inrange(ent) && execitem(ent, cn, d, pos, dist)) tried = true;
                actitems.pop();
            }
            if(tried && gameent::is(d))
            {
                gameent *e = (gameent *)d;
                if(e->action[AC_USE])
                {
                    game::errorsnd(e);
                    e->action[AC_USE] = false;
                }
            }
        }
    }

    void putitems(packetbuf &p)
    {
        loopv(ents)
        {
            if(i >= MAXENTS) break;
            if(enttype[ents[i]->type].syncs)
            {
                gameentity &e = *(gameentity *)ents[i];
                putint(p, i);
                putint(p, int(e.type));
                putint(p, min(e.attrs.length(), MAXENTATTRS));
                loopvj(e.attrs)
                {
                    if(j >= MAXENTATTRS) break;
                    putint(p, e.attrs[j]);
                }
                if(enttype[e.type].syncpos) loopj(3) putint(p, int(e.o[j]*DMF));
                if(enttype[e.type].synckin)
                {
                    putint(p, min(e.kin.length(), MAXENTKIN));
                    loopvj(e.kin)
                    {
                        if(j >= MAXENTKIN) break;
                        putint(p, e.kin[j]);
                    }
                }
            }
        }
    }

    void setspawn(int n, int m)
    {
        if(!ents.inrange(n)) return;
        gameentity &e = *(gameentity *)ents[n];
        bool on = m%2, spawned = e.spawned();
        e.setspawned(on);
        if(on) e.lastspawn = lastmillis;
        if(e.type == TRIGGER && cantrigger(n) && (e.attrs[1] == TR_TOGGLE || e.attrs[1] == TR_LINK || e.attrs[1] == TR_ONCE) && (m >= 2 || e.lastemit <= 0 || e.spawned() != spawned))
        {
            if(m >= 2) e.lastemit = -1;
            else if(e.lastemit > 0)
            {
                int last = lastmillis-e.lastemit, trig = triggertime(e, true);
                if(last > 0 && last < trig) e.lastemit = lastmillis-(trig-last);
                else e.lastemit = lastmillis;
            }
            else e.lastemit = lastmillis;
            execlink(NULL, n, false);
        }
    }
    ICOMMAND(0, entspawned, "i", (int *n), intret(ents.inrange(*n) && ents[*n]->spawned() ? 1 : 0));

    extentity *newent() { return new gameentity; }
    void deleteent(extentity *e) { delete (gameentity *)e; }

    void clearents()
    {
        while(ents.length()) deleteent(ents.pop());
        memset(firstenttype, 0, sizeof(firstenttype));
        memset(firstusetype, 0, sizeof(firstusetype));
        memset(lastenttype, 0, sizeof(lastenttype));
        memset(lastusetype, 0, sizeof(lastusetype));
    }

    bool cansee(int n)
    {
        if(game::player1->state != CS_EDITING && !(showentinfo&64)) return false;
        if(!ents.inrange(n)) return false;
        if(ents[n]->type == NOTUSED && (n != enthover && entgroup.find(n) < 0)) return false;
        return true;
    }

    void cleansound(int n)
    {
        gameentity &e = *(gameentity *)ents[n];
        if(issound(e.schan))
        {
            removesound(e.schan);
            e.schan = -1; // prevent clipping when moving around
            if(e.type == MAPSOUND) e.lastemit = lastmillis+500;
        }
    }

    void fixentity(int n, bool recurse, bool create, bool alter)
    {
        gameentity &e = *(gameentity *)ents[n];
        cleansound(n);
        e.attrs.setsize(numattrs(e.type), 0);
        loopvrev(e.links)
        {
            int ent = e.links[i];
            if(!canlink(n, ent, true)) e.links.remove(i);
            else if(ents.inrange(ent))
            {
                gameentity &f = *(gameentity *)ents[ent];
                if(((enttype[e.type].reclink&(1<<f.type)) || (enttype[f.type].reclink&(1<<e.type))) && f.links.find(n) < 0)
                {
                    f.links.add(n);
                    if(verbose) conoutf("\frWARNING: automatic reciprocal link between %d and %d added", n, ent);
                }
                else continue;
                if(recurse || ent < n) fixentity(ent, false);
            }
            else continue;
        }
        #define FIXEMIT \
            e.nextemit = 0; \
            loopv(e.links) if(ents.inrange(e.links[i])) \
            { \
                gameentity &f = *(gameentity *)ents[e.links[i]]; \
                if(f.type != TRIGGER || !cantrigger(e.links[i])) continue; \
                e.lastemit = f.lastemit; \
                e.setspawned(TRIGSTATE(f.spawned(), f.attrs[4])); \
                break; \
            }
        #define FIXDIRY(a) \
            while(e.attrs[a] < 0) e.attrs[a] += 360; \
            while(e.attrs[a] >= 360) e.attrs[a] -= 360;
        #define FIXDIRYP(a,b) \
            FIXDIRY(a) \
            while(e.attrs[b] < -180) e.attrs[b] += 361; \
            while(e.attrs[b] > 180) e.attrs[b] -= 361;
        #define FIXDIRYPL(a,b) \
            FIXDIRY(a) \
            while(e.attrs[b] < -90) e.attrs[b] += 181; \
            while(e.attrs[b] > 90) e.attrs[b] -= 181;
        #define FIXDIRR(c) \
            while(e.attrs[c] < -180) e.attrs[c] += 361; \
            while(e.attrs[c] > 180) e.attrs[c] -= 361;
        #define FIXDIRYPR(a,b,c) \
            FIXDIRYP(a,b) \
            FIXDIRR(c)
        #define FIXDIRYPRL(a,b,c) \
            FIXDIRYPL(a,b) \
            FIXDIRR(c)
        switch(e.type)
        {
            case LIGHT:
            {
                if(e.attrs[0] < 0) e.attrs[0] = 0; // radius, clamp
                while(e.attrs[1] < 0) e.attrs[1] += 256; // red, wrap around
                while(e.attrs[2] < 0) e.attrs[2] += 256; // green, wrap around
                while(e.attrs[3] < 0) e.attrs[3] += 256; // blue, wrap around
                while(e.attrs[4] < 0) e.attrs[4] += 4; // flare, wrap around
                while(e.attrs[4] >= 4) e.attrs[4] -= 4; // flare, wrap around
                while(e.attrs[5] < 0) e.attrs[5] += 101; // flarescale, wrap around
                if(e.attrs[6] < 0) e.attrs[6] = 0; // flags, clamp
                if(e.attrs[7] < 0) e.attrs[7] = 0; // palette, clamp
                if(e.attrs[8] < 0) e.attrs[8] = 0; // palindex, clamp
                break;
            }
            case MAPMODEL:
            {
                int nummapmodels = mapmodels.length();
                if(nummapmodels)
                {
                    while(e.attrs[0] < 0) e.attrs[0] += nummapmodels;
                    while(e.attrs[0] >= nummapmodels) e.attrs[0] -= nummapmodels;
                }
                FIXDIRYPR(1, 2, 3); // yaw, pitch, roll
                while(e.attrs[4] < 0) e.attrs[4] += 101; // blend
                while(e.attrs[4] > 100) e.attrs[4] -= 101; // wrap both ways
                if(e.attrs[5] < 0) e.attrs[5] += 101; // scale, wrap around
                if(e.attrs[6] < 0) e.attrs[6] = 0; // flags, clamp
                static const int mdlfmap[MDLF_MAX] = { EF_HIDE, EF_NOCOLLIDE, EF_NOSHADOW };
                loopj(MDLF_MAX)
                {
                    if(e.flags&mdlfmap[j] && !(e.attrs[6]&(1<<j))) e.flags &= ~mdlfmap[j];
                    else if(!(e.flags&mdlfmap[j]) && e.attrs[6]&(1<<j)) e.flags |= mdlfmap[j];
                }
                while(e.attrs[7] < 0) e.attrs[7] += 0x1000000; // colour
                while(e.attrs[7] > 0xFFFFFF) e.attrs[7] -= 0x1000000; // wrap both ways
                if(e.attrs[8] < 0) e.attrs[8] = 0; // palette, clamp
                if(e.attrs[9] < 0) e.attrs[9] = 0; // palindex, clamp
                break;
            }
            case PLAYERSTART:
                while(e.attrs[0] < 0) e.attrs[0] += T_COUNT; // team
                while(e.attrs[0] >= T_COUNT) e.attrs[0] -= T_COUNT; // wrap both ways
            case CHECKPOINT: // keeps going
                FIXDIRYPL(1, 2); // yaw, pitch
                if(e.type == CHECKPOINT)
                {
                    while(e.attrs[6] < 0) e.attrs[6] += CP_MAX; // cpid
                    while(e.attrs[6] >= CP_MAX) e.attrs[6] -= CP_MAX; // wrap both ways
                }
                break;
            case PARTICLES:
            {
                while(e.attrs[0] < 0) e.attrs[0] += 36; // particle types
                while(e.attrs[0] >= 36) e.attrs[0] -= 36; // wrap both ways
                FIXEMIT;
                break;
            }
            case MAPSOUND:
            {
                int numsounds = mapsounds.length();
                if(numsounds)
                {
                    while(e.attrs[0] < 0) e.attrs[0] += numsounds;
                    while(e.attrs[0] >= numsounds) e.attrs[0] -= numsounds;
                }
                if(e.attrs[1] < 0) e.attrs[1] = 0; // minrad, clamp
                if(e.attrs[2] < 0) e.attrs[2] = 0; // maxrad, clamp
                while(e.attrs[3] < 0) e.attrs[3] += 256; // volume
                while(e.attrs[3] > 255) e.attrs[3] -= 256; // wrap both ways
                if(e.attrs[4] < 0) e.attrs[4] = 0; // flags, clamp
                while(e.attrs[5] < 0) e.attrs[5] += 101; // blend
                while(e.attrs[5] > 100) e.attrs[5] -= 101; // wrap both ways
                FIXEMIT;
                break;
            }
            case LIGHTFX:
            {
                while(e.attrs[0] < 0) e.attrs[0] += LFX_MAX; // type
                while(e.attrs[0] >= LFX_MAX) e.attrs[0] -= LFX_MAX; // wrap both ways
                if(e.attrs[1] < 0) e.attrs[1] = 0; // mod, clamp
                if(e.attrs[2] < 0) e.attrs[2] = 0; // min, clamp
                if(e.attrs[3] < 0) e.attrs[3] = 0; // max, clamp
                if(e.attrs[4] < 0) e.attrs[4] = 0; // flags, clamp
                FIXEMIT;
                break;
            }
            case DECAL:
            {
                if(e.attrs[0] < 0) e.attrs[0] = 0; // type, clamp
                if(alter && !e.attrs[1] && !e.attrs[2] && !e.attrs[3])
                {
                    e.attrs[1] = (int)camera1->yaw;
                    e.attrs[2] = (int)camera1->pitch;
                    e.attrs[3] = (int)camera1->roll;
                }

                FIXDIRYPR(1, 2, 3); // yaw, pitch, roll
                if(e.attrs[4] <= 0) e.attrs[4] = 1; // scale, wrap around
                while(e.attrs[5] < 0) e.attrs[5] += 101; // blend
                while(e.attrs[5] > 100) e.attrs[5] -= 101; // wrap both ways
                while(e.attrs[6] < 0) e.attrs[6] += 0x1000000; // colour
                while(e.attrs[6] > 0xFFFFFF) e.attrs[6] -= 0x1000000; // wrap both ways
                if(e.attrs[7] < 0) e.attrs[7] = 0; // palette, clamp
                if(e.attrs[8] < 0) e.attrs[8] = 0; // palindex, clamp
                break;
            }
            case WIND:
            {
                if(e.attrs[0] < 0) e.attrs[0] = 0; // mode, clamp
                if(alter && !e.attrs[1]) e.attrs[1] = (int)camera1->yaw;
                FIXDIRY(1); // yaw
                while(e.attrs[2] < 0) e.attrs[2] += 256; // speed
                while(e.attrs[2] > 255) e.attrs[2] -= 256; // wrap both ways
                if(e.attrs[3] < 0) e.attrs[3] = 0; // radius, clamp
                if(e.attrs[4] < 0) e.attrs[4] = 0; // atten, clamp
                if(e.attrs[5] < 0) e.attrs[5] = 0; // interval, clamp
                if(e.attrs[6] < 0) e.attrs[6] = 0; // length, clamp
                break;
            }
            case PUSHER:
            {
                FIXDIRYPL(0, 1); // yaw, pitch
                if(e.attrs[3] < 0) e.attrs[3] = 0; // maxrad, clamp
                if(e.attrs[4] < 0) e.attrs[4] = 0; // minrad, clamp
                if(e.attrs[5] < 0) e.attrs[5] += 4; // type
                if(e.attrs[5] >= 4) e.attrs[5] -= 4; // wrap both ways
                break;
            }
            case TRIGGER:
            {
                while(e.attrs[1] < 0) e.attrs[1] += TR_MAX; // type
                while(e.attrs[1] >= TR_MAX) e.attrs[1] -= TR_MAX; // wrap both ways
                while(e.attrs[2] < 0) e.attrs[2] += TA_MAX; // action
                while(e.attrs[2] >= TA_MAX) e.attrs[2] -= TA_MAX; // wrap both ways
                if(e.attrs[3] < 0) e.attrs[3] = 1; // radius, clamp
                while(e.attrs[4] < 0) e.attrs[4] += 4; // state
                while(e.attrs[4] >= 4) e.attrs[4] -= 4; // wrap both ways
                if(cantrigger(n)) loopv(e.links) if(ents.inrange(e.links[i]) && (ents[e.links[i]]->type == MAPMODEL || ents[e.links[i]]->type == PARTICLES || ents[e.links[i]]->type == MAPSOUND || ents[e.links[i]]->type == LIGHTFX))
                {
                    ents[e.links[i]]->lastemit = e.lastemit;
                    ents[e.links[i]]->setspawned(TRIGSTATE(e.spawned(), e.attrs[4]));
                }
                break;
            }
            case WEAPON:
            {
                if(create && (e.attrs[0] < W_OFFSET || e.attrs[0] >= W_ALL)) e.attrs[0] = W_OFFSET; // don't be stupid when creating the entity
                while(e.attrs[0] < W_OFFSET) e.attrs[0] += W_ALL-W_OFFSET; // don't allow superimposed weaps
                while(e.attrs[0] >= W_ALL) e.attrs[0] -= W_ALL-W_OFFSET; // wrap both ways
                break;
            }
            case ACTOR:
            {
                while(e.attrs[0] < 0) e.attrs[0] += A_TOTAL; // type
                while(e.attrs[0] >= A_TOTAL) e.attrs[0] -= A_TOTAL; // wrap both ways
                FIXDIRYPL(1, 2); // yaw, pitch
                while(e.attrs[6] < 0) e.attrs[6] += W_ALL+1; // allow any weapon
                while(e.attrs[6] > W_ALL) e.attrs[6] -= W_ALL+1; // wrap both ways
                if(e.attrs[7] < 0) e.attrs[7] += 101; // health, wrap around
                if(e.attrs[8] < 0) e.attrs[8] += 101; // speed, wrap around
                if(e.attrs[9] < 0) e.attrs[9] += 101; // scale, wrap around
                if(create) numactors++;
                break;
            }
            case AFFINITY:
            {
                while(e.attrs[0] < 0) e.attrs[0] += T_COUNT; // team
                while(e.attrs[0] >= T_COUNT) e.attrs[0] -= T_COUNT; // wrap both ways
                FIXDIRYPL(1, 2); // yaw, pitch
                break;
            }
            case TELEPORT:
            {
                while(e.attrs[0] < -1) e.attrs[0] += 361; // yaw
                while(e.attrs[0] >= 360) e.attrs[0] -= 361; // has -1 for rotating effect
                while(e.attrs[1] < -90) e.attrs[1] += 181; // pitch
                while(e.attrs[1] > 90) e.attrs[1] -= 181; // wrap both ways
                if(e.attrs[3] < 0) e.attrs[3] = 0; // radius, clamp
                while(e.attrs[4] < 0) e.attrs[4] += 0x1000000; // colour
                while(e.attrs[4] > 0xFFFFFF) e.attrs[4] -= 0x1000000; // wrap both ways
                while(e.attrs[5] < 0) e.attrs[5] += 6; // type
                while(e.attrs[5] >= 6) e.attrs[5] -= 6; // wrap both ways
                if(e.attrs[6] < 0) e.attrs[6] = 0; // palette, clamp
                if(e.attrs[7] < 0) e.attrs[7] = 0; // palindex, clamp
                if(e.attrs[8] < 0) e.attrs[8] = 0; // flags, clamp
                break;
            }
            case RAIL:
            {
                if(e.attrs[0] < 0) e.attrs[0] = 0; // limit
                while(e.attrs[1] < 0) e.attrs[1] += RAIL_ALL+1;
                while(e.attrs[1] > RAIL_ALL) e.attrs[1] -= RAIL_ALL+1;
                FIXDIRYPL(2, 3); // yaw, pitch
                if(e.attrs[4] < 0) e.attrs[4] = 0;
                if(e.attrs[5] < 0) e.attrs[5] = 0;
                while(e.attrs[6] < 0) e.attrs[6] += INANIMATE_C_ALL+1;
                while(e.attrs[6] > INANIMATE_C_ALL) e.attrs[6] -= INANIMATE_C_ALL+1;
                while(e.attrs[7] < 0) e.attrs[7] += ANIM_MAX;
                while(e.attrs[7] >= ANIM_MAX) e.attrs[6] -= ANIM_MAX;
            }
            default: break;
        }
        #undef FIXEMIT
        #undef FIXDIRYPRL
        #undef FIXDIRYPL
        #undef FIXDIRYPR
        #undef FIXDIRYP
        #undef FIXDIRR
        #undef FIXDIRY
        if(enttype[e.type].mvattr >= 0)
        {
            while(e.attrs[enttype[e.type].mvattr] < 0) e.attrs[enttype[e.type].mvattr] += MPV_MAX;
            while(e.attrs[enttype[e.type].mvattr] >= MPV_MAX) e.attrs[enttype[e.type].mvattr] -= MPV_MAX;
        }
        if(enttype[e.type].fxattr >= 0)
        {
            while(e.attrs[enttype[e.type].fxattr] < -7) e.attrs[enttype[e.type].fxattr] += 11;
            while(e.attrs[enttype[e.type].fxattr] >= 4) e.attrs[enttype[e.type].fxattr] -= 11;
        }
        fixrails(n);
    }

    const char *findname(int type)
    {
        if(isent(type)) return enttype[type].name;
        return "";
    }

    int findtype(char *type)
    {
        loopi(MAXENTTYPES) if(!strcmp(type, enttype[i].name)) return i;
        return NOTUSED;
    }

    // these functions are called when the client touches the item
    void execlink(dynent *d, int index, bool local, int ignore)
    {
        if(!ents.inrange(index) || !maylink(ents[index]->type)) return;
        gameentity &e = *(gameentity *)ents[index];
        if(e.type == TRIGGER && !cantrigger(index)) return;
        if(!local) e.lastemit = lastmillis;
        bool commit = false;
        int fstent = min(firstent(MAPMODEL), min(firstent(LIGHTFX), min(firstent(PARTICLES), firstent(MAPSOUND)))),
            lstent = max(lastent(MAPMODEL), max(lastent(LIGHTFX), max(lastent(PARTICLES), lastent(MAPSOUND))));
        for(int i = fstent; i < lstent; ++i) if(ents[i]->links.find(index) >= 0)
        {
            gameentity &f = *(gameentity *)ents[i];
            if(ents.inrange(ignore) && ents[ignore]->links.find(index) >= 0) continue;
            bool both = e.links.find(i) >= 0;
            switch(f.type)
            {
                case MAPMODEL:
                {
                    f.lastemit = e.lastemit;
                    if(e.type == TRIGGER) f.setspawned(TRIGSTATE(e.spawned(), e.attrs[4]));
                    break;
                }
                case LIGHTFX:
                case PARTICLES:
                {
                    f.lastemit = e.lastemit;
                    if(e.type == TRIGGER) f.setspawned(TRIGSTATE(e.spawned(), e.attrs[4]));
                    else if(local) commit = true;
                    break;
                }
                case MAPSOUND:
                {
                    f.lastemit = e.lastemit;
                    if(e.type == TRIGGER) f.setspawned(TRIGSTATE(e.spawned(), e.attrs[4]));
                    else if(local) commit = true;
                    if(mapsounds.inrange(f.attrs[0]) && !issound(f.schan))
                    {
                        int flags = SND_MAP;
                        loopk(SND_LAST) if(f.attrs[4]&(1<<k)) flags |= 1<<k;
                        playsound(f.attrs[0], both ? f.pos() : e.pos(), NULL, flags, f.attrs[3] ? f.attrs[3] : -1, f.attrs[1] || f.attrs[2] ? f.attrs[1] : -1, f.attrs[2] ? f.attrs[2] : -1, &f.schan);
                    }
                    break;
                }
                default: break;
            }
        }
        if(d && gameent::is(d) && commit) client::addmsg(N_EXECLINK, "ri2", ((gameent *)d)->clientnum, index);
    }

    bool tryspawn(dynent *d, const vec &o, float yaw, float pitch)
    {
        game::fixfullrange(d->yaw = yaw, d->pitch = pitch, d->roll = 0);
        (d->o = o).z += d->height+d->aboveeye;
        return physics::entinmap(d, true);
    }

    void spawnplayer(gameent *d, int ent, bool suicide)
    {
        if(ent >= 0 && ents.inrange(ent))
        {
            vec pos = ents[ent]->o;
            switch(ents[ent]->type)
            {
                case PLAYERSTART: case ACTOR:
                    if(tryspawn(d, pos, ents[ent]->attrs[1], ents[ent]->attrs[2])) return;
                    break;
                case CHECKPOINT:
                {
                    float yaw = ents[ent]->attrs[1], pitch = ents[ent]->attrs[2];
                    if(m_ra_gauntlet(game::gamemode, game::mutators) && d->team != T_ALPHA)
                    {
                        yaw -= 180;
                        pitch = -pitch;
                    }
                    physics::droptofloor(pos, ENT_DUMMY);
                    if(tryspawn(d, pos, yaw, pitch)) return;
                    break;
                }
                default:
                    physics::droptofloor(pos, ENT_DUMMY);
                    if(tryspawn(d, pos, rnd(360), 0)) return;
                    break;
            }
        }
        else
        {
            vector<int> spawns;
            loopk(4)
            {
                spawns.shrink(0);
                switch(k)
                {
                    case 0:
                        if(m_team(game::gamemode, game::mutators))
                        {
                            loopenti(PLAYERSTART) if(ents[i]->type == PLAYERSTART)
                            {
                                gameentity &e = *(gameentity *)ents[i];
                                if(!checkmapvariant(e.attrs[enttype[e.type].mvattr])) continue;
                                if(e.attrs[0] != d->team || !m_check(e.attrs[3], e.attrs[4], game::gamemode, game::mutators)) continue;
                                spawns.add(i);
                            }
                        }
                        break;
                    case 1: case 2:
                        loopenti(PLAYERSTART) if(ents[i]->type == PLAYERSTART)
                        {
                            gameentity &e = *(gameentity *)ents[i];
                            if(!checkmapvariant(e.attrs[enttype[e.type].mvattr])) continue;
                            if(e.attrs[0] != d->team || (k != 2 && !m_check(e.attrs[3], e.attrs[4], game::gamemode, game::mutators))) continue;
                            spawns.add(i);
                        }
                        break;
                    case 3:
                        loopenti(WEAPON) if(ents[i]->type == WEAPON) spawns.add(i);
                        break;
                    default: break;
                }
                while(!spawns.empty())
                {
                    int r = rnd(spawns.length());
                    gameentity &e = *(gameentity *)ents[spawns[r]];
                    if(tryspawn(d, e.pos(), e.type == PLAYERSTART ? e.attrs[1] : rnd(360), e.type == PLAYERSTART ? e.attrs[2] : 0))
                        return;
                    spawns.remove(r); // must've really sucked, try another one
                }
            }
            d->yaw = d->pitch = d->roll = 0;
            d->o.x = d->o.y = d->o.z = worldsize;
            d->o.x *= 0.5f; d->o.y *= 0.5f;
            if(physics::entinmap(d, true)) return;
        }
        if(!m_edit(game::gamemode) && suicide) game::suicide(d, HIT(SPAWN));
    }

    void editent(int i, bool local)
    {
        extentity &e = *ents[i];
        cleansound(i);
        if(local && m_edit(game::gamemode) && game::player1->state == CS_EDITING)
            client::addmsg(N_EDITENT, "ri5iv", i, (int)(e.o.x*DMF), (int)(e.o.y*DMF), (int)(e.o.z*DMF), e.type, e.attrs.length(), e.attrs.length(), e.attrs.getbuf());
        if(e.type < MAXENTTYPES)
        {
            firstenttype[e.type] = min(firstenttype[e.type], i);
            firstusetype[enttype[e.type].usetype] = min(firstusetype[enttype[e.type].usetype], i);
            lastenttype[e.type] = max(lastenttype[e.type], i+1);
            lastusetype[enttype[e.type].usetype] = max(lastusetype[enttype[e.type].usetype], i+1);
        }
    }

    float dropheight(extentity &e)
    {
        if(e.type == MAPMODEL || e.type == AFFINITY) return 0.0f;
        return 4.0f;
    }

    bool maylink(int type, int ver)
    {
        if(enttype[type].links && enttype[type].links <= (ver ? ver : VERSION_GAME)) return true;
        return false;
    }

    bool canlink(int index, int node, bool msg)
    {
        if(ents.inrange(index) && ents.inrange(node))
        {
            if(index != node && maylink(ents[index]->type) && maylink(ents[node]->type) &&
                    (enttype[ents[index]->type].canlink&(1<<ents[node]->type)))
                        return true;
            if(msg)
                conoutf("\frEntity %s (%d) and %s (%d) are not linkable", enttype[ents[index]->type].name, index, enttype[ents[node]->type].name, node);

            return false;
        }
        if(msg) conoutf("\frEntity %d and %d are unable to be linked as one does not seem to exist", index, node);
        return false;
    }

    bool linkents(int index, int node, bool add, bool local, bool toggle)
    {
        if(ents.inrange(index) && ents.inrange(node) && index != node && canlink(index, node, local && verbose))
        {
            gameentity &e = *(gameentity *)ents[index], &f = *(gameentity *)ents[node];
            bool recip = (enttype[e.type].reclink&(1<<f.type)) || (enttype[f.type].reclink&(1<<e.type));
            int g = -1, h = -1;
            if((toggle || !add) && (g = e.links.find(node)) >= 0)
            {
                h = f.links.find(index);
                if(!add || !canlink(node, index) || (toggle && h >= 0))
                {
                    e.links.remove(g);
                    if(recip && h >= 0) f.links.remove(h);
                    fixentity(index, true);
                    if(local && m_edit(game::gamemode)) client::addmsg(N_EDITLINK, "ri3", 0, index, node);
                    if(verbose > 2) conoutf("\faEntity %s (%d) and %s (%d) delinked", enttype[ents[index]->type].name, index, enttype[ents[node]->type].name, node);
                    return true;
                }
                else if(toggle && canlink(node, index))
                {
                    f.links.add(index);
                    if(recip && (h = e.links.find(node)) < 0) e.links.add(node);
                    fixentity(node, true);
                    if(local && m_edit(game::gamemode)) client::addmsg(N_EDITLINK, "ri3", 1, node, index);
                    if(verbose > 2) conoutf("\faEntity %s (%d) and %s (%d) linked", enttype[ents[node]->type].name, node, enttype[ents[index]->type].name, index);
                    return true;
                }
            }
            else if(toggle && canlink(node, index) && (g = f.links.find(index)) >= 0)
            {
                f.links.remove(g);
                if(recip && (h = e.links.find(node)) >= 0) e.links.remove(h);
                fixentity(node, true);
                if(local && m_edit(game::gamemode)) client::addmsg(N_EDITLINK, "ri3", 0, node, index);
                if(verbose > 2) conoutf("\faEntity %s (%d) and %s (%d) delinked", enttype[ents[node]->type].name, node, enttype[ents[index]->type].name, index);
                return true;
            }
            else if(toggle || add)
            {
                e.links.add(node);
                if(recip && (h = f.links.find(index)) < 0) f.links.add(index);
                fixentity(index, true);
                if(local && m_edit(game::gamemode)) client::addmsg(N_EDITLINK, "ri3", 1, index, node);
                if(verbose > 2) conoutf("\faEntity %s (%d) and %s (%d) linked", enttype[ents[index]->type].name, index, enttype[ents[node]->type].name, node);
                return true;
            }
        }
        if(verbose > 2)
            conoutf("\frEntity %s (%d) and %s (%d) failed linking", enttype[ents[index]->type].name, index, enttype[ents[node]->type].name, node);
        return false;
    }

    void entitylink(int index, int node, bool both = true)
    {
        if(ents.inrange(index) && ents.inrange(node))
        {
            gameentity &e = *(gameentity *)ents[index], &f = *(gameentity *)ents[node];
            if(e.links.find(node) < 0) linkents(index, node, true, true, false);
            if(both && f.links.find(index) < 0) linkents(node, index, true, true, false);
        }
    }

    void readent(stream *g, int mver, char *gid, int gver, int id)
    {
    }

    void writeent(stream *g, int id)
    {
    }

    void remapents(vector<int> &idxs)
    {
        int numents[MAXENTTYPES], numinvalid = 0;
        memset(numents, 0, sizeof(numents));
        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.type < MAXENTTYPES) numents[e.type]++;
            else numinvalid++;
        }
        int offsets[MAXENTTYPES];
        memset(offsets, -1, sizeof(offsets));
        int priority = INT_MIN, nextpriority = INT_MIN;
        loopi(MAXENTTYPES) nextpriority = max(nextpriority, enttype[i].priority);
        int offset = 0;
        do
        {
            priority = nextpriority;
            nextpriority = INT_MIN;
            loopi(MAXENTTYPES) if(offsets[i] < 0)
            {
                if(enttype[i].priority >= priority) { offsets[i] = offset; offset += numents[i]; }
                else nextpriority = max(nextpriority, enttype[i].priority);
            }
        } while(nextpriority < priority);
        idxs.setsize(0);
        idxs.reserve(offset + numinvalid);
        while(idxs.length() < offset + numinvalid) idxs.add(-1);
        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            idxs[e.type < MAXENTTYPES ? offsets[e.type]++ : offset++] = i;
        }
    }

    void importent(gameentity &e, int id, int mver, int gver)
    {
        if(e.type != RAIL && gver <= 250) loopv(e.links)
        { // switch linking to a rail to linking from it
            int link = e.links[i];
            if(!ents.inrange(link) || ents[link]->type != RAIL) continue;
            gameentity &f = *(gameentity *)ents[link];
            f.links.add(id);
            e.links.remove(i--);
            conoutf("switched rail link between %d and %d", id, link);
        }
        switch(e.type)
        {
            case ACTOR: numactors++; break;
            case WEAPON:
            {
                if(gver <= 218)
                { // insert mine before rockets (9 -> 10) after grenades (8)
                    if(e.attrs[0] >= 9) e.attrs[0]++;
                }
                if(gver <= 221)
                { // insert zapper before rifle (7 -> 8) after plasma (6)
                    if(e.attrs[0] >= 7) e.attrs[0]++;
                }
                break;
            }
            case PLAYERSTART: case AFFINITY:
            {
                if(gver <= 244 && e.attrs[0] > T_OMEGA) e.type = NOTUSED;
                break;
            }
            case PARTICLES:
            {
                if(gver <= 244) switch(e.attrs[0])
                {
                    case 0: game::fixpalette(e.attrs[5], e.attrs[6], gver); break;
                    case 3: game::fixpalette(e.attrs[3], e.attrs[4], gver); break;
                    case 4: game::fixpalette(e.attrs[6], e.attrs[7], gver); break;
                    case 5: game::fixpalette(e.attrs[3], e.attrs[4], gver); break;
                    case 6: game::fixpalette(e.attrs[4], e.attrs[5], gver); game::fixpalette(e.attrs[6], e.attrs[7], gver); break;
                    case 7: game::fixpalette(e.attrs[6], e.attrs[7], gver); break;
                    case 8: game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                    case 9: game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                    case 10: game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                    case 11: game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                    case 12: game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                    case 13: game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                    case 14: game::fixpalette(e.attrs[8], e.attrs[9], gver); break;
                    case 15: game::fixpalette(e.attrs[8], e.attrs[9], gver); break;
                    default: break;
                }
                break;
            }
            case LIGHT: if(gver <= 244) game::fixpalette(e.attrs[7], e.attrs[8], gver); break;
            case MAPMODEL: if(gver <= 244) game::fixpalette(e.attrs[8], e.attrs[9], gver); break;
            case DECAL: if(gver <= 244) game::fixpalette(e.attrs[7], e.attrs[8], gver); break;
            case TELEPORT: if(gver <= 244) game::fixpalette(e.attrs[6], e.attrs[7], gver); break;
            case ROUTE: if(gver <= 223) e.type = NOTUSED; break;
            case RAIL:
            {
                if(gver <= 247) e.type = NOTUSED;
                if(gver <= 248 && (e.attrs[1]&(1<<RAIL_YAW) || e.attrs[1]&(1<<RAIL_PITCH))) e.attrs[1] |= (1<<RAIL_SEEK);
                if(gver <= 249 && e.attrs[4] < 0)
                {
                    e.attrs[5] = e.attrs[2]+e.attrs[4];
                    e.attrs[4] = 0-e.attrs[4];
                    e.attrs[5] = 0;
                }
                break;
            }
            default: break;
        }
        if(gver <= 244 && enttype[e.type].modesattr >= 0)
        {
            int mattr = enttype[e.type].modesattr+1;
            if(e.attrs[mattr] != 0)
            {
                static const int G_M_MULTI = 0, G_M_FREESTYLE = 10, G_M_OLDNUM = 20;
                int offset = 0, oldmuts = e.attrs[mattr] > 0 ? e.attrs[mattr] : 0-e.attrs[mattr], newmuts = 0;
                loopi(G_M_OLDNUM) switch(i)
                {
                    case G_M_MULTI: case G_M_FREESTYLE:
                        offset++;
                        break;
                    default:
                        if(oldmuts&(1<<i)) newmuts |= 1<<(i-offset);
                        break;
                }
                e.attrs[mattr] = e.attrs[mattr] > 0 ? newmuts : 0-newmuts;
            }
        }
    }

    void initents(int mver, char *gid, int gver)
    {
        lastroutenode = routeid = -1;
        numactors = lastroutetime = droproute = 0;
        airnodes.setsize(0);
        ai::oldwaypoints.setsize(0);
        progress(0, "Setting entity attributes...");
        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            e.attrs.setsize(numattrs(e.type), 0);
            if(gver < VERSION_GAME) importent(e, i, mver, gver);
            fixentity(i, false);
            progress((i+1)/float(ents.length()), "Setting entity attributes...");
        }
        memset(firstenttype, 0, sizeof(firstenttype));
        memset(firstusetype, 0, sizeof(firstusetype));
        memset(lastenttype, 0, sizeof(lastenttype));
        memset(lastusetype, 0, sizeof(lastusetype));
        if(m_onslaught(game::gamemode, game::mutators) && !numactors)
        {
            loopv(ents) if(ents[i]->type == PLAYERSTART || ents[i]->type == WEAPON)
            {
                extentity &e = *newent();
                ents.add(&e);
                e.type = ACTOR;
                e.o = ents[i]->o;
                e.attrs.add(0, numattrs(ACTOR));
                e.attrs[0] = A_ENEMY+(i%A_TOTAL);
                switch(ents[i]->type)
                {
                    case PLAYERSTART:
                        loopj(5) e.attrs[j+1] = ents[i]->attrs[j+1]; // yaw, pitch, mode, muts, id
                        break;
                    case WEAPON:
                        loopj(3) e.attrs[j+3] = ents[i]->attrs[j+2]; // mode, muts, id
                    default:
                        e.attrs[1] = (i%8)*45;
                        break;
                }
                numactors++;
            }
        }
        progress(0, "Preparing entities...");
        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.type >= 0 && e.type < MAXENTTYPES)
            {
                firstenttype[e.type] = min(firstenttype[e.type], i);
                firstusetype[enttype[e.type].usetype] = min(firstusetype[enttype[e.type].usetype], i);
                lastenttype[e.type] = max(lastenttype[e.type], i+1);
                lastusetype[enttype[e.type].usetype] = max(lastusetype[enttype[e.type].usetype], i+1);
            }
            if(enttype[e.type].usetype == EU_ITEM || e.type == TRIGGER) setspawn(i, 0);
            if(enttype[e.type].syncs && enttype[e.type].synckin) // find shared kin
            {
                loopvj(e.links) if(ents.inrange(e.links[j]))
                {
                    loopvk(ents) if(ents[k]->type == e.type && ents[k]->links.find(e.links[j]) >= 0)
                    {
                        gameentity &f = *(gameentity *)ents[k];
                        if(e.kin.find(k) < 0) e.kin.add(k);
                        if(f.kin.find(i) < 0) f.kin.add(i);
                    }
                }
            }
            progress((i+1)/float(ents.length()), "Preparing entities...");
        }
        initrails();
    }

    #define renderfocus(i,f) { gameentity &e = *(gameentity *)ents[i]; f; }
    void renderlinked(gameentity &e, int idx)
    {
        loopv(e.links)
        {
            int index = e.links[i];
            if(ents.inrange(index))
            {
                gameentity &f = *(gameentity *)ents[index];
                bool both = false;
                loopvj(f.links) if(f.links[j] == idx)
                {
                    both = true;
                    break;
                }
                part_trace(e.o, f.o, showentsize, 1, 1, both ? entlinkcolourboth : entlinkcolour, showentinterval);
            }
        }
    }

    bool shouldshowents(int level)
    {
        return max(showentradius, max(showentdir, showentlinks)) >= level;
    }

    void renderentshow(gameentity &e, int idx, int level)
    {
        if(e.o.squaredist(camera1->o) > showentdist*showentdist) return;
        #define entdirpart(o,yaw,pitch,length,fade,colour) { part_dir(o, yaw, pitch, length, showentsize, 1, fade, colour, showentinterval); }
        if(showentradius >= level)
        {
            switch(e.type)
            {
                case PLAYERSTART:
                {
                    part_radius(vec(e.o).add(vec(0, 0, actors[A_PLAYER].height*0.5f)), vec(actors[A_PLAYER].radius, actors[A_PLAYER].radius, actors[A_PLAYER].height*0.5f), showentsize, 1, 1, TEAM(e.attrs[0], colour));
                    break;
                }
                case ENVMAP:
                {
                    int s = e.attrs[0] ? clamp(e.attrs[0], 0, 10000) : envmapradius;
                    part_radius(e.o, vec(float(s)), showentsize, 1, 1, entradiuscolour);
                    break;
                }
                case ACTOR:
                {
                    int atype = clamp(e.attrs[0], 0, A_TOTAL-1)+A_ENEMY;
                    part_radius(vec(e.o).add(vec(0, 0, actors[atype].height*0.5f)), vec(actors[atype].radius, actors[atype].radius, actors[atype].height*0.5f), showentsize, 1, 1, TEAM(T_ENEMY, colour));
                    part_radius(e.o, vec(ai::ALERTMAX), showentsize, 1, 1, TEAM(T_ENEMY, colour));
                    break;
                }
                case MAPSOUND:
                {
                    part_radius(e.o, vec(float(e.attrs[1])), showentsize, 1, 1, entradiuscolour);
                    part_radius(e.o, vec(float(e.attrs[2])), showentsize, 1, 1, entradiuscolour);
                    break;
                }
                case WIND:
                {
                    part_radius(e.o, vec(float(e.attrs[3])), showentsize, 1, 1, entradiuscolour);
                    break;
                }
                case LIGHT:
                {
                    int radius = e.attrs[0], spotlight = -1;
                    vec color;
                    getlightfx(e, &radius, &spotlight, &color);
                    if(e.attrs[0] && e.attrs[0] != radius)
                        part_radius(e.o, vec(float(e.attrs[0])), showentsize, 1, 1, color.tohexcolor());
                    part_radius(e.o, vec(float(radius)), showentsize, 1, 1, color.tohexcolor());
                    if(ents.inrange(spotlight))
                    {
                        gameentity &f = *(gameentity *)ents[spotlight];
                        part_cone(e.o, vec(f.o).sub(e.o).normalize(), radius, clamp(int(f.attrs[1]), 1, 89), showentsize, 1, 1, color.tohexcolor());
                    }
                    break;
                }
                case AFFINITY:
                {
                    float radius = enttype[e.type].radius;
                    part_radius(e.o, vec(radius), showentsize, 1, 1, TEAM(e.attrs[0], colour));
                    radius = radius*2/3; // capture pickup dist
                    part_radius(e.o, vec(radius), showentsize, 1, 1, TEAM(e.attrs[0], colour));
                    break;
                }
                default:
                {
                    float radius = enttype[e.type].radius;
                    if((e.type == TRIGGER || e.type == TELEPORT || e.type == PUSHER || e.type == CHECKPOINT) && e.attrs[e.type == CHECKPOINT ? 0 : 3])
                        radius = e.attrs[e.type == CHECKPOINT ? 0 : 3];
                    if(radius > 0) part_radius(e.o, vec(radius), showentsize, 1, 1, entradiuscolour);
                    if(e.type == PUSHER && e.attrs[4] > 0 && e.attrs[4] < radius)
                        part_radius(e.o, vec(float(e.attrs[4])), showentsize, 1, 1, entradiuscolour);
                    break;
                }
            }
        }

        if(showentdir >= level)
        {
            switch(e.type)
            {
                case PLAYERSTART: case CHECKPOINT:
                {
                    entdirpart(e.o, e.attrs[1], e.attrs[2], 4.f, 1, TEAM(e.type == PLAYERSTART ? e.attrs[0] : T_NEUTRAL, colour));
                    break;
                }
                //case MAPMODEL:
                //{
                //    entdirpart(e.o, e.attrs[1], 360-e.attrs[3], 4.f, 1, entdircolour);
                //    break;
                //}
                case WIND:
                {
                    if(e.attrs[0]&WIND_EMIT_VECTORED) entdirpart(e.o, e.attrs[1], 0, entdirsize, 1, entdircolour);
                    break;
                }
                case ACTOR:
                {
                    entdirpart(e.o, e.attrs[1], e.attrs[2], 4.f, 1, TEAM(T_ENEMY, colour));
                    break;
                }
                case TELEPORT:
                {
                    if(e.attrs[0] < 0) { entdirpart(e.o, (lastmillis/5)%360, e.attrs[1], 4.f, 1, entdircolour); }
                    else { entdirpart(e.o, e.attrs[0], e.attrs[1], entdirsize, 1, entdircolour); }
                    break;
                }
                case PUSHER:
                {
                    entdirpart(e.o, e.attrs[0], e.attrs[1], 4.f+e.attrs[2], 1, entdircolour);
                    break;
                }
                case RAIL:
                {
                    loopv(railways) if(railways[i].ent == idx) entdirpart(e.o, railways[i].yaw, railways[i].pitch, entdirsize, 1, entdircolour);
                    break;
                }
                default: break;
            }
        }
        if(enttype[e.type].links && showentlinks >= level) renderlinked(e, idx);
    }

    void adddynlights()
    {
        loopv(railways)
        {
            railway &w = railways[i];
            loopvj(w.parents) if(ents.inrange(w.parents[j]))
            {
                int n = w.parents[j];
                gameentity &e = *(gameentity *)ents[n];
                if(e.type != LIGHT) continue;
                int radius = e.attrs[0], spotlight = -1;
                vec color(255, 255, 255);
                if(!getlightfx(e, &radius, &spotlight, &color, true, false)) continue;
                int spot = 0;
                vec dir(0, 0, 0);
                if(ents.inrange(spotlight))
                {
                    gameentity &f = *(gameentity *)ents[spotlight];
                    dir = vec(f.pos()).sub(e.pos()).safenormalize();
                    spot = clamp(int(f.attrs[1]), 1, 89);
                }
                adddynlight(e.pos(), radius, color, 0, 0, e.attrs[6], radius, color, NULL, dir, spot);
            }
        }
    }

    void reset()
    {
        resetrails();
        inanimates.deletecontents();
    }

    void allchanged(bool load)
    {
        if(load) reset();
    }

    void update()
    {
        runrails();
        loopenti(MAPSOUND)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.type == MAPSOUND && checkmapvariant(e.attrs[enttype[e.type].mvattr]) && mapsounds.inrange(e.attrs[0]))
            {
                bool triggered = false;
                loopvj(e.links)
                {
                    int n = e.links[j];
                    if(!ents.inrange(n)) continue;
                    triggered = true;
                    break;
                }
                if(issound(e.schan))
                {
                    if(triggered && sounds[e.schan].flags&SND_LOOP && !e.spawned() && (e.lastemit < 0 || lastmillis-e.lastemit > triggertime(e, true)))
                        removesound(e.schan);
                    else sounds[e.schan].pos = e.pos();
                }
                if(triggered || issound(e.schan)) continue;
                int flags = SND_MAP|SND_LOOP; // ambient sounds loop
                loopk(SND_LAST)  if(e.attrs[4]&(1<<k)) flags |= 1<<k;
                playsound(e.attrs[0], e.pos(), NULL, flags, e.attrs[3] ? e.attrs[3] : 255, e.attrs[1] || e.attrs[2] ? e.attrs[1] : -1, e.attrs[2] ? e.attrs[2] : -1, &e.schan);
            }
        }
        if((m_edit(game::gamemode) || m_race(game::gamemode)) && routeid >= 0 && droproute)
        {
            if(game::player1->state == CS_ALIVE)
            {   // don't start until the player begins moving
                if(lastroutenode >= 0 || game::player1->move || game::player1->strafe)
                {
                    const vec o = game::player1->feetpos();
                    int curnode = lastroutenode;
                    if(!ents.inrange(curnode) || ents[curnode]->o.dist(o) >= droproutedist)
                    {
                        curnode = -1;
                        loopenti(ROUTE) if(ents[i]->type == ROUTE && ents[i]->attrs[0] == routeid)
                        {
                            float dist = ents[i]->o.dist(o);
                            if(dist < droproutedist && (!ents.inrange(curnode) || dist < ents[curnode]->o.dist(o)))
                                curnode = i;
                        }
                    }
                    if(!ents.inrange(curnode))
                    {
                        attrvector attrs;
                        attrs.add(routeid);
                        attrs.add(int(game::player1->yaw));
                        attrs.add(int(game::player1->pitch));
                        attrs.add(game::player1->move);
                        attrs.add(game::player1->strafe);
                        attrs.add(0);
                        loopi(AC_MAX) if(game::player1->action[i] || (abs(game::player1->actiontime[i]) > lastroutetime))
                            attrs[5] |= (1<<i);
                        int n = newentity(o, int(ROUTE), attrs);
                        if(ents.inrange(lastroutenode)) ents[lastroutenode]->links.add(n);
                        curnode = n;
                        firstenttype[ROUTE] = min(firstenttype[ROUTE], n);
                        lastenttype[ROUTE] = max(lastenttype[ROUTE], n);
                        if(game::player1->airmillis) airnodes.add(n);
                    }
                    if(!game::player1->airmillis && !airnodes.empty()) airnodes.setsize(0);
                    if(lastroutenode != curnode) lastroutetime = lastmillis;
                    lastroutenode = curnode;
                }
            }
            else if(lastroutenode >= 0)
            {
                lastroutenode = -1;
                lastroutetime = 0;
                if(game::player1->state == CS_DEAD) loopv(airnodes) if(ents.inrange(airnodes[i])) ents[airnodes[i]]->type = ET_EMPTY;
                airnodes.setsize(0);
            }
        }
        runinanimates();
    }

    void render()
    {
        if(shouldshowents(game::player1->state == CS_EDITING ? 1 : (!entgroup.empty() || ents.inrange(enthover) ? 2 : 3))) loopv(ents) // important, don't render lines and stuff otherwise!
            renderfocus(i, renderentshow(e, i, game::player1->state == CS_EDITING ? ((entgroup.find(i) >= 0 || enthover == i) ? 1 : 2) : 3));
        int sweap = m_weapon(game::focus->actortype, game::gamemode, game::mutators),
            fstent = m_edit(game::gamemode) ? 0 : firstuse(EU_ITEM),
            lstent = m_edit(game::gamemode) ? ents.length() : lastuse(EU_ITEM);
        for(int i = fstent; i < lstent; i++)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.type <= NOTUSED || e.type >= MAXENTTYPES || (enttype[e.type].usetype == EU_ITEM && simpleitems)) continue;
            bool active = enttype[e.type].usetype == EU_ITEM && (e.spawned() || (e.lastemit && lastmillis-e.lastemit < 500));
            if(m_edit(game::gamemode) || active)
            {
                const char *mdlname = entmdlname(e.type, e.attrs);
                if(mdlname && *mdlname)
                {
                    modelstate mdl;
                    mdl.o = e.pos();
                    mdl.anim = ANIM_MAPMODEL|ANIM_LOOP;
                    mdl.flags = MDL_CULL_VFC|MDL_CULL_DIST|MDL_CULL_OCCLUDED;
                    int colour = -1;
                    if(!active)
                    {
                        if(showentmodels <= (e.type == PLAYERSTART || e.type == ACTOR ? 1 : 0)) continue;
                        if(e.type == AFFINITY || e.type == PLAYERSTART)
                        {
                            mdl.yaw = e.attrs[1]+(e.type == PLAYERSTART ? 90 : 0);
                            mdl.pitch = e.attrs[2];
                            colour = TEAM(e.attrs[0], colour);
                        }
                        else if(e.type == ACTOR)
                        {
                            mdl.yaw = e.attrs[1]+90;
                            mdl.pitch = e.attrs[2];
                            int weap = e.attrs[6] > 0 ? e.attrs[6]-1 : AA(e.attrs[0], weaponspawn);
                            mdl.size = e.attrs[9] > 0 ? e.attrs[9]/100.f : AA(e.attrs[0], scale);
                            if(isweap(weap)) colour = W(weap, colour);
                        }
                    }
                    else if(e.spawned())
                    {
                        int millis = lastmillis-e.lastspawn;
                        if(millis < 500) mdl.size = mdl.color.a = float(millis)/500.f;
                    }
                    else if(e.lastemit)
                    {
                        int millis = lastmillis-e.lastemit;
                        if(millis < 500) mdl.size = mdl.color.a = 1.f-(float(millis)/500.f);
                    }
                    if(e.type == WEAPON)
                    {
                        int attr = m_attr(e.type, e.attrs[0]);
                        if(isweap(attr))
                        {
                            colour = W(attr, colour);
                            if(!active || !game::focus->canuse(game::gamemode, game::mutators, e.type, attr, e.attrs, sweap, lastmillis, W_S_ALL, !showentfull))
                                mdl.color.a *= showentunavailable;
                            else mdl.color.a *= showentavailable;
                        }
                        else continue;
                    }
                    if(mdl.color.a > 0)
                    {
                        mdl.material[0] = bvec::fromcolor(game::getcolour(game::focus, game::playerovertone, game::playerovertonelevel));
                        mdl.material[1] = bvec::fromcolor(game::getcolour(game::focus, game::playerundertone, game::playerundertonelevel));
                        if(colour >= 0) mdl.material[2] = bvec::fromcolor(colour);
                        rendermodel(mdlname, mdl);
                    }
                }
            }
        }
        loopv(railways) loopvj(railways[i].parents)
        {
            railway &r = railways[i];
            int n = r.parents[j];
            if(!ents.inrange(n) || ents[n]->type != MAPMODEL) continue;
            gameentity &e = *(gameentity *)ents[n];
            const char *mdlname = mapmodelname(ents[n]->attrs[0]);
            if(!mdlname || !*mdlname) continue;
            modelstate mdl;
            mdl.o = e.pos();
            mdl.flags = MDL_CULL_VFC|MDL_CULL_DIST|MDL_CULL_OCCLUDED;
            getmapmodelstate(e, mdl);
            if(r.animtype > 0 && r.animtime > 0)
            {
                mdl.anim = r.animtype|ANIM_LOOP;
                mdl.basetime = r.animtime;
            }
            mdl.yaw += e.yaw;
            mdl.pitch += e.pitch;
            dynent *d = NULL;
            loopvj(inanimates) if(inanimates[j]->control == INANIMATE_RAIL && inanimates[j]->ent == n)
            {
                d = inanimates[j];
                break;
            }
            rendermodel(mdlname, mdl, d);
        }
    }

    void maketeleport(gameentity &e)
    {
        float yaw = e.attrs[0] < 0 ? (lastmillis/5)%360 : e.attrs[0], radius = float(e.attrs[3] ? e.attrs[3] : enttype[e.type].radius);
        int attr = int(e.attrs[4]), colour = (((attr&0xF)<<4)|((attr&0xF0)<<8)|((attr&0xF00)<<12))+0x0F0F0F;
        if(e.attrs[6] || e.attrs[7])
        {
            vec r = vec::fromcolor(colour).mul(game::getpalette(e.attrs[6], e.attrs[7]));
            colour = (int(r.x*255)<<16)|(int(r.y*255)<<8)|(int(r.z*255));
        }
        part_portal(e.pos(), radius, 1, yaw, e.attrs[1], PART_TELEPORT, 1, colour);
    }

    bool checkparticle(extentity &e)
    {
        if(!checkmapvariant(e.attrs[12]) || !checkmapeffects(e.attrs[13])) return false;
        gameentity &f = (gameentity &)e;
        if(f.attrs[11])
        {
            if((f.nextemit -= curtime) <= 0) f.nextemit = 0;
            if(f.nextemit) return false;
            f.nextemit += f.attrs[11];
        }
        bool ret = true;
        loopv(f.links)
        {
            int n = f.links[i];
            if(!ents.inrange(n)) continue;
            if(f.spawned() || (f.lastemit > 0 && lastmillis-f.lastemit <= triggertime(e, true))) return true;
            ret = false; // if there's a trigger and one isn't spawned, default to false
        }
        return ret;
    }

    void drawparticle(gameentity &e, const vec &o, int idx, bool spawned, bool active, float skew)
    {
        switch(e.type)
        {
            case TELEPORT:
                if(e.attrs[4]) maketeleport(e);
                break;
            case ROUTE:
            {
                if(e.attrs[0] != routeid || (!m_edit(game::gamemode) && !m_race(game::gamemode))) break;
                loopv(e.links) if(ents.inrange(e.links[i]) && ents[e.links[i]]->type == ROUTE && (!routemaxdist || o.dist(ents[e.links[i]]->o) <= routemaxdist))
                    part_flare(o, ents[e.links[i]]->o, 1, PART_LIGHTNING_FLARE, routecolour);
            }
            default: break;
        }

        vec off(0, 0, 2.f), pos = o, view = idx >= 0 ? e.pos() : o;
        if(enttype[e.type].usetype == EU_ITEM) pos.add(off);
        bool edit = m_edit(game::gamemode) && idx >= 0 && cansee(idx),
             isedit = edit && game::player1->state == CS_EDITING,
             hasent = isedit && (enthover == idx || entgroup.find(idx) >= 0),
             hastop = hasent && o.squaredist(camera1->o) <= showentdist*showentdist;
        int sweap = m_weapon(game::focus->actortype, game::gamemode, game::mutators),
            attr = e.type == WEAPON ? m_attr(e.type, e.attrs[0]) : e.attrs[0],
            colour = e.type == WEAPON && isweap(attr) ? W(attr, colour) : colourwhite, interval = lastmillis%1000;
        float fluc = interval >= 500 ? (1500-interval)/1000.f : (500+interval)/1000.f;
        if(enttype[e.type].usetype == EU_ITEM && (active || isedit))
        {
            float blend = fluc*skew, radius = fluc*0.5f;
            if(e.type == WEAPON && isweap(attr))
            {
                if(!active || !game::focus->canuse(game::gamemode, game::mutators, e.type, attr, e.attrs, sweap, lastmillis, W_S_ALL, !showentfull))
                {
                    if(isedit) blend *= showentavailable;
                    else if(showentunavailable > 0) blend *= showentunavailable;
                    else blend = 0;
                }
                else blend *= showentavailable;
                if(blend > 0)
                {
                    radius += game::focus->hasweap(attr, sweap) ? W(attr, itemhaloammo) : W(attr, itemhalo);
                    radius = max(radius*skew, 0.125f);
                }
            }
            else radius = max(enttype[e.type].radius*0.5f*skew, 0.125f);
            if(blend > 0)
            {
                if(simpleitems == 1)
                {
                    part_icon(view, textureload(hud::itemtex(e.type, attr), 3), simpleitemsize*skew, simpleitemblend*blend*skew, 0, 0, 1, colour);
                    if(radius < simpleitemsize*skew) radius = simpleitemsize*skew;
                    blend *= simpleitemhalo;
                }
                else
                {
                    radius *= haloitemsize;
                    blend *= haloitemblend;
                }
                vec offset = vec(view).sub(camera1->o).rescale(radius/2);
                offset.z = max(offset.z, -1.0f);
                part_create(PART_HINT_BOLD_SOFT, 1, offset.add(view), colour, radius, blend);
            }
        }
        if(edit)
        {
            part_create(hastop ? PART_EDIT_ONTOP : PART_EDIT, 1, o, hastop ? entselcolourtop : entselcolour, hastop ? entselsizetop : entselsize);
            if(showentinfo&(hasent ? 4 : 8))
            {
                defformatstring(s, "<super>%s%s (%d)", hastop ? "\fc" : "\fC", enttype[e.type].name, idx >= 0 ? idx : 0);
                part_textcopy(pos.add(off), s, hastop ? PART_TEXT_ONTOP : PART_TEXT);
                if(idx >= 0)
                {
                    loopv(railways)
                    {
                        if(railways[i].ent != idx && railways[i].findparent(idx) < 0) continue;
                        formatstring(s, "<little>railway [%d] %d ms (%d/%d) [%.1f/%.1f]", i, railways[i].millis, railways[i].length[0], railways[i].length[1], railways[i].yaw, railways[i].pitch);
                        part_textcopy(pos.add(vec(off).mul(0.5f)), s, hastop ? PART_TEXT_ONTOP : PART_TEXT);
                    }
                    loopv(railways)
                    {
                        int n = railways[i].findchild(idx);
                        if(n < 0) continue;
                        formatstring(s, "<tiny>in railway [%d] %d of %d [%.1f/%.1f]", i, n+1, railways[i].rails.length(), railways[i].rails[n].yaw, railways[i].rails[n].pitch);
                        part_textcopy(pos.add(vec(off).mul(0.35f)), s, hastop ? PART_TEXT_ONTOP : PART_TEXT);
                    }
                }
            }
        }
        if(isedit ? (showentinfo&(hasent ? 1 : 2)) : (enttype[e.type].usetype == EU_ITEM && active && showentdescs >= 3))
        {
            const char *itxt = entinfo(e.type, e.attrs, isedit);
            if(itxt && *itxt)
            {
                defformatstring(ds, "<emphasis>%s", itxt);
                part_textcopy(pos.add(off), ds, hastop ? PART_TEXT_ONTOP : PART_TEXT, 1, colourwhite);
            }
        }
        if(edit && showentinfo&(hasent ? 16 : 32)) loopk(numattrs(e.type))
        {
            const char *attrname = getentattribute(e.type, k, e.attrs[0]);
            if(attrname && *attrname)
            {
                string attrval; attrval[0] = 0;
                if(showentattrinfo&1)
                {
                    defformatstring(s, "\fs\fy%d\fS:", k+1);
                    concatstring(attrval, s);
                }
                if(showentattrinfo&2)
                {
                    if(*attrval) concatstring(attrval, " ");
                    concatstring(attrval, attrname);
                }
                if(showentattrinfo&4)
                {
                    if(*attrval) concatstring(attrval, " = ");
                    defformatstring(s, "\fs\fc%d\fS", e.attrs[k]);
                    concatstring(attrval, s);
                    if(enttype[e.type].mvattr == k)
                    {
                        formatstring(s, " (%s)", mapvariants[clamp(e.attrs[enttype[e.type].mvattr], 0, MPV_MAX-1)]);
                        concatstring(attrval, s);
                    }
                }
                defformatstring(s, "%s%s", hastop ? "\fw" : "\fW", attrval);
                part_textcopy(pos.add(off), s, hastop ? PART_TEXT_ONTOP : PART_TEXT);
            }
        }
    }

    void drawparticles()
    {
        bool hasroute = (m_edit(game::gamemode) || m_race(game::gamemode)) && routeid >= 0;
        int fstent = m_edit(game::gamemode) ? 0 : min(firstuse(EU_ITEM), firstent(hasroute ? ROUTE : TELEPORT)),
            lstent = m_edit(game::gamemode) ? ents.length() : max(lastuse(EU_ITEM), lastent(hasroute ? ROUTE : TELEPORT));

        loopv(railways) loopvj(railways[i].parents)
        {
            int n = railways[i].parents[j];
            if(!ents.inrange(n) || ents[n]->type != PARTICLES) continue;
            gameentity &e = *(gameentity *)ents[n];
            if(!checkparticle(e) || e.pos().dist(camera1->o) > maxparticledistance) continue;
            makeparticle(e.pos(), e.attrs);
        }

        for(int i = fstent; i < lstent; ++i)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.type == NOTUSED || e.attrs.empty()) continue;
            if(e.type != TELEPORT && e.type != ROUTE && !m_edit(game::gamemode) && enttype[e.type].usetype != EU_ITEM) continue;
            else if(e.o.dist(camera1->o) > maxparticledistance) continue;
            float skew = 1;
            bool active = false;
            if(e.spawned())
            {
                int millis = lastmillis-e.lastspawn;
                if(millis < 500) skew = float(millis)/500.f;
                active = true;
            }
            else if(e.lastemit)
            {
                int millis = lastmillis-e.lastemit;
                if(millis < 500)
                {
                    skew = 1.f-(float(millis)/500.f);
                    active = true;
                }
            }
            drawparticle(e, e.o, i, e.spawned(), active, skew);
        }

        loopv(projs::projs)
        {
            projent &proj = *projs::projs[i];
            if(proj.projtype != PRJ_ENT || !ents.inrange(proj.id) || !proj.ready()) continue;
            gameentity &e = *(gameentity *)ents[proj.id];
            if(e.type == NOTUSED || e.attrs.empty()) continue;
            float skew = 1;
            if(proj.fadetime && proj.lifemillis)
            {
                int interval = min(proj.lifemillis, proj.fadetime);
                if(proj.lifetime < interval) skew = float(proj.lifetime)/float(interval);
                else if(proj.lifemillis > interval)
                {
                    interval = min(proj.lifemillis-interval, proj.fadetime);
                    if(proj.lifemillis-proj.lifetime < interval) skew = float(proj.lifemillis-proj.lifetime)/float(interval);
                }
            }
            drawparticle(e, proj.o, -1, true, true, skew);
        }
    }
}
