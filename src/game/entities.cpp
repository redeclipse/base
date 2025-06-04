#include "game.h"

namespace entities
{
    int firstenttype[MAXENTTYPES], firstusetype[EU_MAX], lastenttype[MAXENTTYPES], lastusetype[EU_MAX], numactors = 0, lastroutenode = -1, lastroutefloor = -1, lastroutetime = 0;

    vector<extentity *> ents;
    vector<int> airnodes;
    vector<inanimate *> inanimates;

    VAR(IDF_PERSIST, showentmodels, 0, 2, 2);
    VAR(IDF_PERSIST, showentweapons, 0, 0, 2);

    VAR(IDF_PERSIST, showentdir, 0, 1, 3); // 0 = off, 1 = only selected, 2 = always when editing, 3 = always in editmode
    VAR(IDF_PERSIST, showentradius, 0, 1, 3);
    VAR(IDF_PERSIST, showentlinks, 0, 1, 3);
    VAR(IDF_PERSIST, showentdynamic, 0, 1, 3);
    VAR(IDF_PERSIST, showentrails, 0, 1, 3);
    VAR(IDF_PERSIST, showentinterval, 0, 32, VAR_MAX);
    VAR(IDF_PERSIST, showentfull, 0, 0, 1);
    FVAR(IDF_PERSIST, showentsize, 0, 3, 10);
    FVAR(IDF_PERSIST, showentavailable, 0, 1, 1);
    FVAR(IDF_PERSIST, showentunavailable, 0, 0.5f, 1);

    DEFUIVARS(entityedit, SURFACE_WORLD, -1.f, 0.f, 1.f, 4.f, 256.f, 0.f, 0.f);
    DEFUIVARS(entityitem, SURFACE_WORLD, -1.f, 0.f, 1.f, 4.f, 512.f, 0.f, 0.f);
    DEFUIVARS(entityproj, SURFACE_WORLD, -1.f, 0.f, 1.f, 4.f, 512.f, 0.f, 0.f);

    VAR(IDF_PERSIST, entityicons, 0, 1, 1);
    VAR(IDF_PERSIST, entityhalos, 0, 1, 1);
    FVAR(IDF_PERSIST, entselblend, 0, 1, 1);
    FVAR(IDF_PERSIST, entselblendtop, 0, 1, 1);
    FVAR(IDF_PERSIST, entselsize, 0, 0.75f, FVAR_MAX);
    FVAR(IDF_PERSIST, entselsizetop, 0, 1, FVAR_MAX);
    FVAR(IDF_PERSIST, entdirsize, 0, 10, FVAR_MAX);
    FVAR(IDF_PERSIST, entrailoffset, 0, 0.1f, FVAR_MAX);

    FVAR(IDF_PERSIST, entitymaxdist, 0, 1024, FVAR_MAX);
    FVAR(IDF_PERSIST, entityshowmaxdist, 0, 512, FVAR_MAX);
    FVAR(IDF_PERSIST, entityiconmaxdist, 0.f, 256, FVAR_MAX);

    VAR(IDF_PERSIST, entityeffect, 0, 1, 1);
    FVAR(IDF_PERSIST, entityeffecttime, 0, 1.5f, FVAR_MAX);
    FVAR(IDF_PERSIST, entityeffectfade, 0, 1.0f, 16);
    FVAR(IDF_PERSIST, entityeffectslice, 0, 0.125f, 1);
    FVAR(IDF_PERSIST, entityeffectblend, 0, 1.0f, 1);
    FVAR(IDF_PERSIST, entityeffectbright, -16, 1.0f, 16);

    VAR(IDF_PERSIST|IDF_HEX, entselcolour, 0, 0xFFFFFF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, entselcolourtop, 0, 0xFF88FF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, entselcolourdyn, 0, 0x88FFFF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, entselcolourrail, 0, 0xFFFF88, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, entlinkcolour, 0, 0xFF88FF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, entlinkcolourboth, 0, 0x88FF88, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, entdircolour, 0, 0xFFFFFF, 0xFFFFFF);
    VAR(IDF_PERSIST|IDF_HEX, entradiuscolour, 0, 0xFFFFFF, 0xFFFFFF);

    VAR(0, mapsoundautomute, 0, 0, 1);

    VARF(0, routeid, -1, -1, VAR_MAX, lastroutenode = -1; lastroutetime = 0; airnodes.setsize(0)); // selected route in speedrun
    VARF(0, droproute, 0, 0, 1, lastroutenode = -1; lastroutetime = 0; airnodes.setsize(0); if(routeid < 0) routeid = 0);
    VAR(0, droproutedist, 1, 16, VAR_MAX);
    VAR(0, routemaxdist, 0, 64, VAR_MAX);
    VAR(IDF_PERSIST, showroutenames, 0, 1, 1);
    FVAR(IDF_PERSIST, routenameblend, 0, 1, 1);
    SVARF(IDF_MAP, routenames, "Easy Medium Hard", { string s; if(filterstring(s, routenames)) { delete[] routenames; routenames = newstring(s); } });
    SVARF(IDF_MAP, routecolours, "0x00FF00 0xFF7700 0xFF0000", { string s; if(filterstring(s, routecolours)) { delete[] routecolours; routecolours = newstring(s); } });

    void physents(physent *d)
    {
        d->movescale = d->gravityscale = d->coastscale = 1;

        if(d->isnophys()) return;

        vec from = dynent::is(d) ? ((dynent *)d)->center() : d->center();
        loopenti(PHYSICS) if(ents[i]->type == PHYSICS && isallowed(i))
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.attrs[0] < 0 || e.attrs[0] >= PHYSICS_MAX || (!e.attrs[1] && e.attrs[0] != PHYSICS_GRAVITY)) continue;

            float value = e.attrs[1] / 100.f, dist = 0, maxdist = 0;
            vec to = e.pos();

            if(e.flags&EF_BBZONE)
            {
                int axis = -1;
                loopj(3)
                {
                    // we need to process the bb anyway, so don't use insidebb
                    float offset = fabs(to[j] - from[j]);
                    if(offset > e.attrs[2 + j])
                    {
                        axis = -1;
                        break; // bb rejected
                    }

                    // falloff is calculated based on distance to nearest edge
                    float left = e.attrs[2 + j] - offset;
                    if(axis >= 0 && left > maxdist) continue;

                    axis = j;
                    dist = offset;
                    maxdist = left;
                }

                if(axis < 0) continue; // rejected
                maxdist = e.attrs[2 + axis];
            }
            else
            {
                maxdist = max(e.attrs[2], e.attrs[3], e.attrs[4]);
                if(maxdist <= 0) maxdist = enttype[e.type].radius;

                dist = from.dist(e.pos());
                if(dist > maxdist) continue;
            }

            if(e.attrs[5] && dist > 0 && maxdist > 0) // falloff
                value += (1 - value) * dist / maxdist;

            switch(e.attrs[0])
            {
                case PHYSICS_MOVEMENT: d->movescale *= value; break;
                case PHYSICS_GRAVITY: d->gravityscale *= value; break;
                case PHYSICS_COASTING: d->coastscale *= value; break;
                default: break;
            }
        }
    }

    struct rail
    {
        int ent, length, rotstart, rotend, rotlen, flags, animtype;
        float yaw, pitch, animspeed;
        vec pos, dir, offset;

        rail() : ent(-1), length(0), rotstart(0), rotend(0), rotlen(0), flags(0), animtype(0), yaw(0), pitch(0), animspeed(0), pos(0, 0, 0), dir(0, 0, 0), offset(0, 0, 0) {}
        rail(int n, const vec &o, int d = 0, int f = 0, int at = 0, float as = 0) : ent(n), length(d), rotstart(0), rotend(0), rotlen(0), flags(f), animtype(at), yaw(0), pitch(0), animspeed(as), pos(o), dir(0, 0, 0), offset(0, 0, 0) {}
        ~rail() {}
    };

    struct railway
    {
        int ent, retpoint, curpoint, lastpoint, flags, length[2], lastsecs, curstep, millis, coltype, animtype, animoffset, animtime;
        float yaw, pitch, lastyaw, lastpitch, animspeed;
        vec dir, offset, lastoffset, lastdir;

        vector<rail> rails;
        vector<int> parents;

        railway() :
            ent(-1), retpoint(0), curpoint(-1), lastpoint(-1), flags(0), lastsecs(0), curstep(0), millis(0), coltype(0), animtype(0), animoffset(0), animtime(0),
            yaw(0), pitch(0), lastyaw(0), lastpitch(0), animspeed(0),
            dir(0, 0, 0), offset(0, 0, 0), lastoffset(0, 0, 0), lastdir(0, 0, 0)
            { reset(); }

        railway(int n, int f = 0, int c = 0, int at = 0, int ao = 0, float as = 0) :
            ent(n), retpoint(0), curpoint(-1), lastpoint(-1), flags(f), lastsecs(0), curstep(0), millis(0), coltype(c), animtype(at), animoffset(ao), animtime(0),
            yaw(0), pitch(0), lastyaw(0), lastpitch(0), animspeed(as),
            dir(0, 0, 0), offset(0, 0, 0), lastoffset(0, 0, 0), lastdir(0, 0, 0)
            { reset(); }

        ~railway()
        {
            cleanup();
        }

        void cleanup()
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
            retpoint = 0;
            rails.setsize(0);
            loopi(2) length[i] = 0;
        }

        int findchild(int n, int v = 0)
        {
            int r = 0;
            loopv(rails) if(rails[i].ent == n)
            {
                if(r == v) return i;
                r++;
            }
            return -1;
        }

        int children(int n)
        {
            int r = 0;
            loopv(rails) if(rails[i].ent == n) r++;
            return r;
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

            if(offset == -1 && ((cur == retpoint && iter) || (!iter && !cur)))
                index = rails.length() - 1;
            else
            {
                index += offset;
                if(index >= rails.length()) index %= rails.length() - retpoint;
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
                rails.add(rail(i, e.o, max(e.attrs[0], 0), e.attrs[1], e.attrs[7], e.attrs[8]/100.f));
                i = -1;

                loopvj(e.links)
                {
                    int link = e.links[j];
                    if(!ents.inrange(link) || ents[link]->type != RAIL) continue;
                    int cur = findchild(link);
                    if(cur >= 0)
                    {
                        retpoint = cur;
                        break;
                    }
                    i = link;
                    break;
                }
            }

            if(!rails.empty())
            { // calculate the telemetry of the line
                if(retpoint < 0) retpoint = 0;
                loopv(rails)
                {
                    rail &r = rails[i], &s = rails.inrange(i+1) ? rails[i+1] : rails[retpoint];
                    int oldlen = r.length;

                    r.offset = vec(s.pos).sub(r.pos);
                    if(flags&(1<<RAIL_SPEED)) r.length = int(r.offset.magnitude()*r.length/100.f);

                    if(flags&(1<<RAIL_YAW) || flags&(1<<RAIL_PITCH))
                    {
                        gameentity &e = *(gameentity *)ents[r.ent];
                        float scale = flags&(1<<RAIL_SPEED) && r.length != oldlen ? r.length/float(oldlen) : 1.f;
                        r.rotstart = clamp(int(e.attrs[5]*scale), 0, r.length);
                        r.rotend = clamp(r.rotstart+(e.attrs[4] > 0 ? int(e.attrs[4]*scale) : r.length), r.rotstart, r.length);
                        r.rotlen = clamp(r.rotend-r.rotstart, 0, r.length);
                    }

                    length[0] += r.length;
                    if(i >= retpoint) length[1] += r.length;
                }
                if(length[0] <= 0) return false;

                if(flags&(1<<RAIL_YAW) || flags&(1<<RAIL_PITCH)) loopv(rails)
                {
                    rail &r = rails[i], &s = flags&(1<<RAIL_PREV) ? getrail(i, -1, 1) : (flags&(1<<RAIL_NEXT) ? getrail(i, 1, 1) : r);

                    if(flags&(1<<RAIL_SEEK))
                    {
                        r.dir = vec(s.offset).safenormalize();
                        vectoyawpitch(r.dir, r.yaw, r.pitch);
                    }
                    else
                    {
                        gameentity &e = *(gameentity *)ents[s.ent];
                        if(flags&(1<<RAIL_YAW)) r.yaw = e.attrs[2];
                        if(flags&(1<<RAIL_PITCH)) r.pitch = e.attrs[3];
                        r.dir = vec(r.yaw*RAD, r.pitch*RAD);
                    }
                }

                return true;
            }

            return false;
        }

        void generate(int index, int iter, float amt, int step, vec &off, vec &aim)
        {
            rail &rcur = rails[index], &rprev = getrail(index, -1, iter),
                 &rnext = getrail(index, 1, iter), &rnext2 = getrail(index, 2, iter);

            if(flags&(1<<RAIL_SPLINE))
            {
                vec spline[4] = { rprev.pos, rcur.pos, rnext.pos, rnext2.pos };
                off = catmullrom(spline, amt);
            }
            else off = vec(rcur.pos).add(vec(rcur.offset).mul(amt));

            if(flags&(1<<RAIL_YAW) || flags&(1<<RAIL_PITCH))
            {
                float part = step >= rcur.rotend ? 1.f : (step >= rcur.rotstart ? (step-rcur.rotstart)/float(max(rcur.rotlen, 1)) : 0.f);
                if(flags&(1<<RAIL_SPLINE))
                {
                    vec spline[4] = { rprev.dir, rcur.dir, rnext.dir, rnext2.dir };
                    aim = catmullrom(spline, part);
                }
                else aim = vec(rcur.dir).mul(1-part).add(vec(rnext.dir).mul(part)).safenormalize();
            }
        }

        bool run(int secs)
        {
            if(rails.empty()) return false;
            if(lastsecs >= secs)
            { // rail has already run this timestep
                curstep = 0;
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
                        m->curstep = 0;
                        m->yawed = m->pitched = 0;
                        m->moved = vec(0, 0, 0);
                        m->resized = vec(0, 0, 0);
                        break;
                    }
                }
                return true;
            }

            int elapsed = secs, start = 0, iter = 0;
            if(elapsed >= length[0])
            { // allow the loop point to be different from the start
                elapsed -= length[0];
                start = retpoint;
                iter++;
            }

            curstep = lastsecs ? secs-lastsecs : 0;
            lastpoint = curpoint;
            lastyaw = yaw;
            lastpitch = pitch;
            lastoffset = offset;
            lastdir = dir;

            millis = elapsed%max(length[iter], 1);
            offset = rails[0].pos;
            if(flags&(1<<RAIL_YAW) || flags&(1<<RAIL_PITCH)) dir = rails[0].dir;

            int span = 0, anim = rails[0].animtype;
            float aspeed = rails[0].animspeed;
            bool moved = false;
            for(int i = start; i < rails.length(); i++)
            { // look for the station on the timetable
                rail &r = rails[i];

                if(r.length > 0 && millis <= span+r.length)
                { // interpolate toward the next station
                    int step = millis-span;
                    float amt = step/float(r.length);

                    moved = true;
                    curpoint = i;
                    anim = r.animtype;
                    aspeed = r.animspeed;
                    generate(i, iter, amt, step, offset, dir);

                    break;
                }

                span += r.length;
            }
            if(lastpoint != curpoint && rails.inrange(curpoint)) execlink(NULL, rails[curpoint].ent, false);

            offset.sub(rails[0].pos); // all coordinates translate based on first rail
            if(flags&(1<<RAIL_YAW) || flags&(1<<RAIL_PITCH)) vectoyawpitch(dir, yaw, pitch);

            if(anim != animtype)
            {
                animtype = anim;
                animtime = anim ? lastmillis : 0;
            }
            animspeed = aspeed;

            bool checkteleport = curpoint >= 0 && lastpoint >= 0 && lastpoint != curpoint;
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

                    bool slice = curstep && moved;
                    if(!m)
                    {
                        m = new inanimate;
                        m->ent = parent;
                        m->control = INANIMATE_RAIL;
                        m->coltype = coltype;
                        m->aboveeye = 0;
                        if(mmi->m->collide != COLLIDE_ELLIPSE) m->collidetype = COLLIDE_OBB;
                        inanimates.add(m);
                        slice = false;
                    }

                    if(slice && checkteleport)
                    { // only do this if we need it
                        int teleported = -1;
                        loopk(rails.length()-1)
                        {
                            int index = lastpoint + k + 1;
                            if(index >= rails.length()) index -= rails.length() - retpoint;
                            if(index == curpoint) break;
                            teleported = index;
                        }
                        if(teleported >= 0)
                        {
                            generate(teleported, iter, 1, rails[teleported].length, lastoffset, lastdir);
                            lastoffset.sub(rails[0].pos); // all coordinates translate based on first rail
                            if(flags&(1<<RAIL_YAW) || flags&(1<<RAIL_PITCH)) vectoyawpitch(lastdir, lastyaw, lastpitch);
                        }
                        checkteleport = false;
                    }

                    vec newpos = e.pos(), oldsize(m->xradius, m->yradius, m->height);
                    float newyaw = e.attrs[1]+e.yaw, newpitch = e.attrs[2]+e.pitch, newroll = e.attrs[3];
                    fixrange(newyaw, newpitch);

                    vec center, radius;
                    mmi->m->collisionbox(center, radius);
                    if(e.attrs[5])
                    {
                        float scale = e.attrs[5]/100.f;
                        center.mul(scale);
                        radius.mul(scale);
                    }
                    rotatebb(center, radius, int(newyaw), int(newpitch), int(newroll));

                    float xradius = radius.x + fabs(center.x), yradius = radius.y + fabs(center.y), rradius = m->collidetype == COLLIDE_OBB ? sqrtf(xradius*xradius + yradius*yradius) : max(xradius, yradius),
                          offz = center.z-radius.z, height = max(offz, 0.f) + radius.z*2*mmi->m->height + radius.z*2*(1.0f-mmi->m->height);

                    newpos.z += height;
                    if(offz < 0) newpos.z += offz;

                    int numdynents = game::numdynents();
                    if(slice)
                    {
                        vec prevpos = vec(e.o).add(lastoffset);
                        prevpos.z += height;
                        if(offz < 0) prevpos.z += offz;

                        m->o = prevpos;
                        m->curstep = curstep;
                        m->yawed = yaw-lastyaw;
                        m->pitched = pitch-lastpitch;
                        m->moved = vec(newpos).sub(prevpos);
                        m->resized = vec(xradius, yradius, height).sub(oldsize);

                        for(int s = curstep; s > 0; )
                        {
                            int step = min(s, physics::physframetime);
                            float part = step/float(curstep);
                            vec dir = vec(m->moved).mul(part);
                            vec resize = vec(m->resized).mul(part);

                            m->o.add(dir);
                            m->xradius += resize.x;
                            m->yradius += resize.y;
                            m->radius = m->collidetype == COLLIDE_OBB ? sqrtf(xradius*xradius + yradius*yradius) : max(xradius, yradius);
                            m->zradius += resize.z;
                            m->height = m->zradius;

                            loopj(numdynents)
                            {
                                gameent *d = (gameent *)game::iterdynents(j);
                                if(!d || d->state != CS_ALIVE || m->findpassenger(d) >= 0) continue;

                                vec rescale = vec(d->o.x, d->o.y, 0.f).sub(vec(m->o.x, m->o.y, 0.f)).safenormalize().mul(vec(resize.x, resize.y, 0.f)),
                                    curdir = vec(rescale).add(dir), oldpos = d->o, oldnew = d->newpos;
                                m->coltarget = d; // restricts inanimate collisions to this entity, and filters out the reverse collision

                                bool crush = false;
                                if(collide(m, vec(0, 0, 0), 0, true, true, 0, false) && collideplayer == d)
                                {
                                    if(m->coltype&(1<<INANIMATE_C_KILL)) game::suicide(d, HIT_TOUCH);
                                    else
                                    {
                                        if(curdir.iszero()) crush = true;
                                        else
                                        {
                                            d->o.add(curdir);
                                            d->newpos.add(curdir);

                                            if(collide(d, vec(0, 0, 0), 0, true, true))
                                            {
                                                crush = true;
                                                vec proj = vec(curdir).project(collidewall);
                                                if(!proj.iszero())
                                                {
                                                    d->o.add(proj);
                                                    d->newpos.add(proj);
                                                    if(!collide(d, vec(0, 0, 0), 0, true, true)) crush = false;
                                                }
                                            }
                                        }
                                    }
                                }
                                if(crush)
                                {
                                    d->o = oldpos;
                                    d->newpos = oldnew;
                                    game::suicide(d, HIT_CRUSH);
                                    break;
                                }
                                m->coltarget = NULL;
                            }

                            if(!(m->coltype&(1<<INANIMATE_C_NOPASS))) loopvjrev(m->passengers)
                            {
                                passenger &p = m->passengers[j];
                                physent *d = p.ent;
                                if(d->state != CS_ALIVE) continue;

                                vec rotate = vec(p.offset).rotate_around_z(m->yawed*RAD).sub(p.offset).mul(part),
                                    curdir = vec(rotate).add(dir).addz(resize.z < 0 ? resize.z*0.5f : resize.z),
                                    oldpos = d->o, oldnew = d->newpos;
                                m->coltarget = d; // filter collisions from the passenger

                                d->o.add(curdir);
                                d->newpos.add(curdir);

                                if(collide(d, vec(0, 0, 0), 0, true, true) && !gameent::is(collideplayer))
                                {
                                    d->o = oldpos;
                                    d->newpos = oldnew;
                                    if(gameent::is(d) && collidewall.z < 0) game::suicide((gameent *)d, HIT_CRUSH);
                                    m->passengers.remove(j);
                                    continue;
                                }
                                if(m->yawed != 0) d->yaw += m->yawed*part;
                                if(m->pitched != 0) d->pitch += m->pitched*part;
                                fixrange(d->yaw, d->pitch);
                                p.offset = vec(d->o).sub(m->o);

                                m->coltarget = NULL;
                            }

                            prevpos = m->o;
                            s -= step;
                        }
                    }
                    else
                    {
                        m->curstep = 0;
                        m->yawed = m->pitched = 0;
                        m->moved = vec(0, 0, 0);
                        m->resized = vec(0, 0, 0);
                    }


                    m->o = newpos;
                    m->yaw = newyaw;
                    m->pitch = newpitch,
                    m->roll = newroll,
                    m->xradius = xradius;
                    m->yradius = yradius;
                    m->radius = rradius;
                    m->height = m->zradius = height;
                    m->resetinterp();

                    loopj(numdynents)
                    {
                        gameent *d = (gameent *)game::iterdynents(j);
                        if(!d || d->state != CS_ALIVE || (d != game::player1 && !d->ai) || m->findpassenger(d) >= 0) continue;
                        m->coltarget = d; // restricts inanimate collisions to this entity, and filters out the reverse collision
                        if(collide(m, vec(0, 0, 0), 0, true, true, 0, false) && collideplayer == d && collideinside) game::suicide(d, HIT_CRUSH);
                        m->coltarget = NULL;
                    }
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
            if(!ents.inrange(parent) || ents[parent]->type == RAIL || !(enttype[RAIL].canlink&(1<<ents[parent]->type)) || !isallowed(parent)) continue;
            gameentity &f = *(gameentity *)ents[parent];
            int cur = findrail(n);
            railway &w = railways.inrange(cur) ? railways[cur] : railways.add(railway(n, e.attrs[1], e.attrs[6], e.attrs[7], e.attrs[9], e.attrs[8]/100.f));
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
        int secs = game::gettimesync();
        if(!railbuilt) buildrails();
        loopv(railways) if(!railways[i].run(secs))
        {
            railways.remove(i--);
            railbuilt = 0;
        }
    }

    void updaterails()
    {
        loopv(railways) railways[i].lastsecs = 0;
    }

    void initrails()
    {
        resetrails();
        buildrails();
    }

    void removepassenger(physent *d)
    {
        loopv(inanimates)
        {
            inanimate *m = inanimates[i];
            loopvjrev(m->passengers) if(m->passengers[j].ent == d) m->passengers.remove(j);
        }
    }

    void localpassenger(inanimate *m, physent *d)
    {
        if(gameent::is(d) && d != game::player1 && !((gameent *)d)->ai) return;
        float dist = m->headpos().squaredist(d->feetpos());
        loopv(inanimates)
        {
            inanimate *t = inanimates[i];
            if(t == m) continue;
            int cur = t->findpassenger(d);
            if(cur < 0) continue;
            if(!t->passengers[cur].local) return; // don't override remote passengers
            if(t->headpos().squaredist(d->feetpos()) > dist)
            {
                t->passengers.remove(cur);
                break;
            }
            else return;
        }
        m->localpassenger(d);
        d->physstate = PHYS_FLOOR;
    }

    inanimate *remotepassenger(int ent, physent *d, const vec &offset)
    {
        inanimate *r = NULL;
        loopv(inanimates)
        {
            inanimate *m = inanimates[i];
            if(r || m->ent != ent)
            {
                int cur = m->findpassenger(d);
                if(cur >= 0) m->passengers.remove(cur);
                continue;
            }
            m->remotepassenger(d, offset);
            r = m;
        }
        return r;
    }

    void updatepassengers()
    {
        loopv(inanimates)
        {
            inanimate *m = inanimates[i];
            loopvjrev(m->passengers)
            {
                if(!m->passengers[j].local) continue;
                m->passengers.remove(j);
            }
        }
    }

    inanimate *currentpassenger(physent *d)
    {
        loopv(inanimates)
        {
            inanimate *m = inanimates[i];
            if(m->ent < 0) continue;
            loopvj(m->passengers)
            {
                int cur = m->findpassenger(d);
                if(cur >= 0) return m;
            }
        }
        return NULL;
    }

    vector<extentity *> &getents() { return ents; }
    int firstent(int type) { return type >= 0 && type < MAXENTTYPES ? clamp(firstenttype[type], 0, ents.length()-1) : 0; }
    int firstuse(int type) { return type >= 0 && type < EU_MAX ? clamp(firstusetype[type], 0, ents.length()-1) : 0; }
    int lastent(int type) { return type >= 0 && type < MAXENTTYPES ? clamp(lastenttype[type], 0, ents.length()) : 0; }
    int lastuse(int type) { return type >= 0 && type < EU_MAX ? clamp(lastusetype[type], 0, ents.length()) : 0; }

    int numattrs(int type, bool unused) { return clamp(type >= 0 && type < MAXENTTYPES ? enttype[type].numattrs : 0, unused ? 5 : 0, MAXENTATTRS); }
    ICOMMAND(0, entityattrs, "bb", (int *n, int *used), intret(numattrs(*n, !*used)));

    bool isallowed(const extentity &e)
    {
        if(m_dark(game::gamemode, game::mutators) && e.type == LIGHT) return false;
        if(enttype[e.type].modesattr >= 0 && !m_check(e.attrs[enttype[e.type].modesattr], e.attrs[enttype[e.type].modesattr+1], game::gamemode, game::mutators)) return false;
        if(enttype[e.type].mvattr >= 0 && !checkmapvariant(e.attrs[enttype[e.type].mvattr])) return false;
        if(enttype[e.type].fxattr >= 0 && !checkmapeffects(e.attrs[enttype[e.type].fxattr])) return false;
        return true;
    }

    bool isallowed(int n)
    {
        if(!ents.inrange(n)) return false;
        extentity &e = *(extentity *)ents[n];
        return isallowed(e);
    }

    bool getdynamic(const extentity &e, vec &pos, float *yaw, float *pitch)
    {
        if(!e.dynamic())
        {
            pos = e.o;
            if(yaw) *yaw = enttype[e.type].yawattr >= 0 ? e.attrs[enttype[e.type].yawattr] : 0;
            if(pitch) *pitch = enttype[e.type].pitchattr >= 0 ? e.attrs[enttype[e.type].pitchattr] : 0;

            return false;
        }

        gameentity &f = *(gameentity *)&e;

        pos = f.pos();
        if(yaw) *yaw = f.yaw;
        if(pitch) *pitch = f.pitch;

        return true;
    }

    bool getdynamic(int n, vec &pos, float *yaw, float *pitch)
    {
        if(!ents.inrange(n)) return false;
        extentity &e = *(extentity *)ents[n];
        return getdynamic(e, pos, yaw, pitch);
    }

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

    ICOMMAND(0, getentinfo, "bi", (int *n, int *x),
    {
        if(*n < 0) intret(MAXENTTYPES);
        else if(*n < MAXENTTYPES) result(*x ? enttype[*n].displayname : enttype[*n].name);
    });

    const char *getentattribute(int type, int attr, int attr1)
    {
        if(type < 0 || type >= MAXENTTYPES) return "";
        const char *attrname = enttype[type].attrs[attr];
        if(type == PARTICLES && attr < 12) switch(attr1)
        {
            case -1: break; // not given
            case 0:  switch(attr) { case 0: break; case 1: attrname = "length"; break; case 2: attrname = "height"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "palette"; break; case 6: attrname = "palindex"; break; case 7: attrname = "size"; break; case 8: attrname = "blend"; break; case 9: attrname = "gravity"; break; case 10: attrname = "velocity"; break; default: attrname = ""; break; } break;
            case 1:  switch(attr) { case 0: break; case 1: attrname = "dir"; break; break; default: attrname = ""; } break;
            case 2:  switch(attr) { case 0: break; case 1: attrname = "dir"; break; break; default: attrname = ""; } break;
            case 3:  switch(attr) { case 0: break; case 1: attrname = "size"; break; case 2: attrname = "colour"; break; case 3: attrname = "palette"; break; case 4: attrname = "palindex"; break; break; default: attrname = ""; } break;
            case 5:  switch(attr) { case 0: break; case 1: attrname = "amt"; break; case 2: attrname = "colour"; break; case 3: attrname = "palette"; break; case 4: attrname = "palindex"; break; break; default: attrname = ""; } break;
            case 6:  switch(attr) { case 0: break; case 1: attrname = "amt"; break; case 2: attrname = "colour"; break; case 3: attrname = "colour2"; break; case 4: attrname = "palette1"; break; case 5: attrname = "palindex1"; break; case 6: attrname = "palette2"; break; case 7: attrname = "palindex2"; break; break; default: attrname = ""; } break;
            case 4:
            case 7:  switch(attr) { case 0: break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "palette"; break; case 7: attrname = "palindex"; break; default: attrname = ""; break; } break;
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 16:
            case 17:
            case 18: switch(attr) {case 0: break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; case 11: attrname = "blend"; break; default: attrname = ""; break; } break;
            case 14:
            case 15: switch(attr) {case 0: break; case 1: attrname = "radius"; break; case 2: attrname = "height"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "gravity"; break; case 7: attrname = "velocity"; break; case 8: attrname = "palette"; break; case 9: attrname = "palindex"; break; case 10: attrname = "blend"; break; default: attrname = ""; break; } break;
            case 32:
            case 33:
            case 34:
            case 35: switch(attr) { case 0: break; case 1: attrname = "red"; break; case 2: attrname = "green"; break; case 3: attrname = "blue"; break; default: attrname = ""; break; } break;
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

    FVAR(IDF_PERSIST, entityviewspin, FVAR_MIN, -10.0f, FVAR_MAX);
    FVAR(IDF_PERSIST, entityviewpitch, -89.9f, -22.5f, 89.9f);
    FVAR(IDF_PERSIST, entityviewzdist, 0, 2.0f, FVAR_MAX);
    FVAR(IDF_PERSIST, entityviewpulldist, 0, 8.0f, FVAR_MAX);

    void getentity(int id, int val, int ex, bool mod)
    {
        if(id < 0) intret(ents.length());
        else if(ents.inrange(id))
        {
            if(val < 0) intret(6);
            else switch(val)
            {
                case 0: intret(ents[id]->type); break; // type
                case 1: // attrs
                {
                    if(ex < 0) intret(ents[id]->attrs.length());
                    else if(ents[id]->attrs.inrange(ex))
                    {
                        if(mod && ex == 0) intret(m_attr(ents[id]->type, ents[id]->attrs[ex]));
                        else intret(ents[id]->attrs[ex]);
                    }
                    break;
                }
                case 2: // links
                {
                    if(ex < 0) intret(ents[id]->links.length());
                    else if(ents[id]->links.inrange(ex)) intret(ents[id]->links[ex]);
                    break;
                }
                case 3: // info
                {
                    if(ex < 0) intret(9);
                    else
                    {
                        gameentity &e = *(gameentity *)ents[id];
                        switch(ex)
                        {
                            case 0: intret(e.spawned()); break;
                            case 1: intret(e.lastspawn); break;
                            case 2: intret(e.lastemit); break;
                            case 3: result(e.o); break;
                            case 4: floatret(e.o.x); break;
                            case 5: floatret(e.o.y); break;
                            case 6: floatret(e.o.z); break;
                            case 7: intret(e.spawndelay); break;
                            case 8: floatret(!e.spawned() && e.lastspawn && e.spawndelay ? (lastmillis - e.lastspawn) / float(e.spawndelay) : -1.0f); break;
                            default: break;
                        }
                    }
                    break;
                }
                case 4: // dynamic
                {
                    if(ex < 0) intret(6);
                    else
                    {
                        vec pos;
                        float yaw = 0.0f, pitch = 0.0f;
                        getdynamic(id, pos, &yaw, &pitch);
                        switch(ex)
                        {
                            case 0: result(pos); break;
                            case 1: floatret(pos.x); break;
                            case 2: floatret(pos.y); break;
                            case 3: floatret(pos.z); break;
                            case 4: floatret(yaw); break;
                            case 5: floatret(pitch); break;
                            default: break;
                        }
                    }
                    break;
                }
                case 5: // pulled back view
                {
                    if(ex < 0) intret(6);
                    else
                    {
                        float yaw = entityviewspin * lastmillis / 1000.0f, pitch = entityviewpitch;
                        gameentity &e = *(gameentity *)ents[id];
                        vec origpos = vec(e.o).addz(entityviewzdist);

                        if(!e.lastthirdpos || e.lastthirdpos != totalmillis)
                        {
                            float pulldist = max(float(enttype[e.type].radius), entityviewpulldist);
                            vec pos = origpos;

                            if(e.type == MAPMODEL)
                            {
                                mapmodelinfo *mmi = getmminfo(e.attrs[0]);
                                if(mmi && mmi->m)
                                {
                                    vec center, radius;
                                    mmi->m->collisionbox(center, radius);
                                    if(e.attrs[5])
                                    {
                                        float scale = e.attrs[5]/100.f;
                                        center.mul(scale);
                                        radius.mul(scale);
                                    }
                                    rotatebb(center, radius, int(e.attrs[1]), int(e.attrs[2]), int(e.attrs[3]));
                                    radius.add(center.abs());
                                    pos.sub(vec(yaw * RAD, 0.0f).mul(max(radius.x, radius.y)));
                                    pos.z += radius.z;
                                }
                            }

                            e.thirdpos = game::thirdpos(pos, yaw, pitch, pulldist);
                            if(e.thirdpos == pos && game::camcheck(pos, int(pulldist)))
                                e.thirdpos = game::thirdpos(pos, yaw, pitch, pulldist);

                            e.lastthirdpos = totalmillis;
                        }

                        switch(ex)
                        {
                            case 0: result(e.thirdpos); break;
                            case 1: floatret(e.thirdpos.x); break;
                            case 2: floatret(e.thirdpos.y); break;
                            case 3: floatret(e.thirdpos.z); break;
                            case 4: vectoyawpitch(vec(origpos).sub(e.thirdpos).normalize(), yaw, pitch); floatret(yaw); break;
                            case 5: vectoyawpitch(vec(origpos).sub(e.thirdpos).normalize(), yaw, pitch); floatret(pitch); break;
                            default: break;
                        }
                    }
                    break;
                }
            }
        }
    }

    ICOMMAND(0, getentity, "bbbi", (int *id, int *val, int *ex, int *mod), getentity(*id, *val, *ex, *mod != 0));

    const char *getenttex(int id)
    {
        static string enttex = "";

        if(ents.inrange(id))
        {
            gameentity &e = *(gameentity *)ents[id];

            int attr = m_attr(e.type, e.attrs[0]);
            formatstring(enttex, "<grey>textures/%s/%s", e.type == WEAPON ? "weapons" : "icons/edit", e.type == WEAPON ? W_STR[isweap(attr) ? attr : W_PISTOL] : enttype[e.type].name);

            Texture *t = textureload(enttex, 0, true, false);
            if(t && t != notexture) return enttex;
        }

        return "<grey>textures/icons/question";

    }
    ICOMMAND(0, getenttex, "b", (int *id), result(getenttex(*id)));

    const char *getweaptex(int id)
    {
        static string weaptex = "";
        if(isweap(id))
        {
            formatstring(weaptex, "<grey>textures/weapons/%s", W_STR[id]);
            Texture *t = textureload(weaptex, 0, true, false);
            if(t && t != notexture) return weaptex;
        }
        return "<grey>textures/icons/question";

    }
    ICOMMAND(0, getweaptex, "b", (int *id), result(getweaptex(*id)));

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
                    case 16: addentinfo("haze"); break;
                    case 17: addentinfo("haze-flame"); break;
                    case 18: addentinfo("haze-tape"); break;
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
                    case 4: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 16: case 17:
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
                                case 0: addentinfo("z-axis"); break;
                                case 1: addentinfo("x-axis"); break;
                                case 2: addentinfo("y-axis"); break;
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
                            case 0: addentinfo("z-axis"); break;
                            case 1: addentinfo("x-axis"); break;
                            case 2: addentinfo("y-axis"); break;
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
                    if(attr[6]&L_NOSHADOW) addentinfo("no-shadow");
                    if(attr[6]&L_NODYNSHADOW) addentinfo("no-dynshadow");
                    if(attr[6]&L_VOLUMETRIC) addentinfo("volumetric");
                    if(attr[6]&L_NOSPEC) addentinfo("no-specular");
                    if(attr[6]&L_SMALPHA) addentinfo("color-shadow");

                }
                break;
            }
            case LIGHTFX:
            {
                if(full)
                {
                    addentinfo(attr[0] < 0 || attr[0] >= LFX_MAX ? "normal" : LFX_STR[attr[0]]);
                    loopi(LFX_MAX-1) if(attr[4]&(1<<(LFX_S_MAX+i))) { defformatstring(ds, "+%s", LFX_STR[i+1]); addentinfo(ds); break; }
                    if(attr[4]&LFX_S_RAND1) addentinfo("rnd-min");
                    if(attr[4]&LFX_S_RAND2) addentinfo("rnd-max");
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
            case SOUNDENV:
            {
                int sattr = attr[0] - 1;
                if(sattr < 0 || !soundenvs.inrange(sattr)) break;
                addentinfo(soundenvs[sattr]->name);
                break;
            }
            case PHYSICS:
            {
                if(attr[0] < 0 || attr[0] >= PHYSICS_MAX) break;
                addentinfo(PHYSICS_STR[attr[0]]);
            }
            case ACTOR:
            {
                if(full && attr[0] >= 0 && attr[0] < A_TOTAL)
                {
                    addentinfo(actors[attr[0] + A_ENEMY].name);
                    addentinfo(W(attr[6] > 0 && attr[6] <= W_ALL ? attr[6]-1 : A(attr[0] + A_ENEMY, weaponspawn), name));
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
                if(attr[0] == -1) { addentinfo("announcer"); }
                else if(mapsounds.inrange(attr[0]))
                {
                    int samples = mapsounds[attr[0]].samples.length();
                    defformatstring(ds, "%s (%d %s)", mapsounds[attr[0]].name, samples, samples == 1 ? "sample" : "samples");
                    addentinfo(ds);
                }
                if(full)
                {
                    if(attr[6]&SND_NOATTEN) addentinfo("no-atten");
                    if(attr[6]&SND_NODELAY) addentinfo("no-delay");
                    if(attr[6]&SND_PRIORITY) addentinfo("priority");
                    if(attr[6]&SND_NOPAN) addentinfo("no-pan");
                    if(attr[6]&SND_NODIST) addentinfo("no-dist");
                    if(attr[6]&SND_NOENV) addentinfo("no-env");
                    if(attr[6]&SND_CLAMPED) addentinfo("clamped");
                }
                break;
            }
            case TRIGGER:
            {
                if(full)
                {
                    const char *trgnames[TRIG_MAX+1] = { "toggle", "link", "script", "once", "exit", "" }, *actnames[TRIG_A_MAX+1] = { "manual", "proximity", "action", "" };
                    addentinfo(trgnames[attr[1] < 0 || attr[1] >= TRIG_MAX ? TRIG_MAX : attr[1]]);
                    addentinfo(actnames[attr[2] < 0 || attr[2] >= TRIG_A_MAX ? TRIG_A_MAX : attr[2]]);
                    addentinfo(attr[4]&(1<<TRIG_S_INVERTED) ? "on" : "off");
                    addentinfo(attr[4]&(1<<TRIG_S_ROUTED) ? "routed" : "unrouted");
                    addentinfo(attr[4]&(1<<TRIG_S_ONEWAY) ? "one-way" : "both-ways");
                    addentinfo(attr[4]&(1<<TRIG_S_PERSIST) ? "persist" : "reset");
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
                    const char *railnames[RAIL_MAX] = { "follow-yaw", "follow-pitch", "seek", "spline", "set-speed", "prev", "next" };
                    loopj(RAIL_MAX) if(attr[1]&(1<<j)) addentinfo(railnames[j]);

                    const char *railcollides[INANIMATE_C_MAX] = { "touch-kill", "no-passenger" };
                    loopj(INANIMATE_C_MAX) if(attr[6]&(1<<j)) addentinfo(railcollides[j]);

                    if(attr[7] > 0 && attr[7] < ANIM_MAX)
                    {
                        defformatstring(str, "anim: %s", animnames[attr[7]]);
                        addentinfo(str);
                    }
                }
                break;
            }
            case CAMERA:
            {
                if(full)
                {
                    if(attr[0] >= 0 && attr[1] < CAMERA_MAX)
                    {
                        const char *cameranames[CAMERA_MAX] = { "normal", "mapshot" };
                        addentinfo(cameranames[attr[0]]);
                    }
                    const char *cameraflags[INANIMATE_C_MAX] = { "static-view" };
                    loopj(CAMERA_F_MAX) if(attr[1]&(1<<j)) addentinfo(cameraflags[j]);
                }
                break;
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
                    loopi(G_M_MAX) if(muts&(1<<i))
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
                const char *mdl = actors[attr[0] + A_ENEMY].mdl;
                if(!mdl || !*mdl) mdl = playertypes[0][1];
                return mdl;
            }
            default: break;
        }
        return "";
    }

    void useeffects(gameent *d, int cn, int ent, int ammoamt, bool spawn, int weap, int drop, int ammo, int delay)
    {
        gameentity &e = *(gameentity *)ents[ent];
        int sweap = m_weapon(d->actortype, game::gamemode, game::mutators), attr = m_attr(e.type, e.attrs[0]),
            colour = e.type == WEAPON && isweap(attr) ? W(attr, colour) : colourwhite;

        if(isweap(weap))
        {
            d->setweapstate(weap, W_S_SWITCH, W(weap, delayswitch), lastmillis);
            d->weapammo[weap][W_A_CLIP] = -1;
            d->weapammo[weap][W_A_STORE] = 0;
        }

        d->useitem(ent, e.type, attr, ammoamt, sweap, lastmillis, W(attr, delayitem));

        emitsound(e.type == WEAPON && attr >= W_OFFSET && attr < W_ALL ? WSND(attr, S_W_USE) : S_ITEMUSE, weapons::getweapsoundpos(d, TAG_ORIGIN), d, &d->wschan[WS_MAIN_CHAN]);
        if(game::dynlighteffects) adddynlight(d->center(), enttype[e.type].radius*2, vec::fromcolor(colour).mul(2.f), 250, 250, L_NOSHADOW|L_NODYNSHADOW);

        if(ents.inrange(drop) && ents[drop]->type == WEAPON)
        {
            gameentity &f = *(gameentity *)ents[drop];
            attr = m_attr(f.type, f.attrs[0]);
            if(isweap(attr)) projs::drop(d, attr, drop, ammo, d == game::player1 || d->ai, weap);
        }

        if(cn >= 0)
        {
            gameent *m = game::getclient(cn);
            if(m) projs::destruct(m, PROJ_ENTITY, ent);
        }
        else if(e.spawned() != spawn)
        {
            e.setspawned(spawn);
            e.lastemit = lastmillis;
            e.lastspawn = lastmillis;
            e.spawndelay = delay;
        }

        if(gs_playing(game::gamestate) && e.type == WEAPON && (itemannounceuse&(1<<attr)) != 0)
        {
            gamelog *log = new gamelog(GAMELOG_EVENT);
            log->addlist("args", "type", "item");
            log->addlist("args", "action", "use");
            log->addlist("args", "entity", ent);
            log->addlist("args", "attr", attr);
            log->addlist("args", "delay", delay);
            log->addlist("args", "spawn", spawn ? 1 : 0);
            log->addlist("args", "colour", colourwhite);
            log->addlistf("args", "console", "%s picked up a %s", game::colourname(d), e.type == WEAPON && isweap(attr) ? W(attr, longname) : enttype[e.type].name);
            log->addclient("client", d);
            if(!log->push()) DELETEP(log);
        }
    }

    bool collateitems(dynent *d, vec &pos, float radius)
    {
        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.flags&EF_VIRTUAL) continue; // skip virtual entities
            if(enttype[e.type].usetype != EU_NONE && (enttype[e.type].usetype != EU_ITEM || (d->isalive() && e.spawned())) && isallowed(e))
            {
                float eradius = enttype[e.type].radius, edist = pos.dist(e.pos());
                switch(e.type)
                {
                    case TRIGGER: case TELEPORT: case PUSHER: if(e.attrs[3] > 0) eradius = e.attrs[3]; break;
                    case CHECKPOINT: if(e.attrs[0] > 0) eradius = e.attrs[0]; break;
                }

                float diff = edist-radius;
                if(diff > eradius) continue;

                d->logitem(ACTITEM_ENT, i, diff);
            }
        }

        if(d->isalive()) loopv(projs::typeprojs[PROJ_ENTITY])
        {
            projent &proj = *projs::typeprojs[PROJ_ENTITY][i];
            if(!proj.owner || proj.projtype != PROJ_ENTITY || !proj.ready()) continue;
            if(!ents.inrange(proj.id) || enttype[ents[proj.id]->type].usetype != EU_ITEM || !isallowed(proj.id)) continue;
            if(!(enttype[ents[proj.id]->type].canuse&(1<<d->type))) continue;

            float eradius = enttype[ents[proj.id]->type].radius, edist = pos.dist(proj.o);
            switch(ents[proj.id]->type)
            {
                case TRIGGER: case TELEPORT: case PUSHER: if(ents[proj.id]->attrs[3] > 0) eradius = ents[proj.id]->attrs[3]; break;
                case CHECKPOINT: if(ents[proj.id]->attrs[0] > 0) eradius = ents[proj.id]->attrs[0]; break;
            }

            float diff = edist-radius;
            if(diff > eradius) continue;

            d->logitem(ACTITEM_PROJ, proj.id, diff, proj.seqid);
        }

        return d->updateitems();
    }

    int triggerent = -1;
    gameent *triggerclient = NULL;
    ICOMMANDV(0, triggerent, triggerent);
    ICOMMANDV(0, triggerclient, triggerclient ? triggerclient->clientnum : -1);

    bool cantrigger(int n, gameent *d = NULL)
    {
        gameentity &e = *(gameentity *)ents[n];
        switch(e.type)
        {
            case TRIGGER:
            {
                if(!isallowed(e)) return false;
                if(e.attrs[2] == TRIG_A_MANUAL) return (identflags&IDF_MAP) != 0;

                if(d)
                {
                    bool spawn = (e.attrs[4]&(1<<TRIG_S_INVERTED)) != 0;
                    switch(e.attrs[1])
                    {
                        case TRIG_TOGGLE:
                            if(e.attrs[4]&(1<<TRIG_S_ONEWAY) && e.spawned() != spawn) return false;
                            break;
                        case TRIG_ONCE:
                            if(e.spawned() != spawn) return false;
                            break;
                        case TRIG_EXIT:
                            if(e.spawned()) return false;
                            break;
                        case TRIG_SCRIPT: break;
                    }

                    int millis = d->lastused(n);
                    if(millis && lastmillis - millis < triggertime(e, true)) return false;
                }
                return true;
            }
            default: if(isallowed(e)) return true; break;
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
                case TRIG_EXIT: if(d->actortype >= A_BOT) break;
                case TRIG_TOGGLE: case TRIG_LINKED: case TRIG_ONCE:
                {
                    if(e.attrs[2] != TRIG_A_MANUAL) client::addmsg(N_TRIGGER, "ri2", d->clientnum, n);
                    if(!e.spawned() || e.attrs[1] == TRIG_TOGGLE) setspawn(n, e.spawned() ? 0 : 1);
                    break;
                }
                case TRIG_SCRIPT:
                {
                    if(d->actortype >= A_BOT) break;
                    defformatstring(s, "on_trigger_%d", e.attrs[0]);
                    triggerent = n;
                    triggerclient = d;
                    RUNMAP(s);
                    triggerent = -1;
                    triggerclient = NULL;
                    break;
                }
                default: break;
            }
            if(act && e.attrs[2] == TRIG_A_ACTION) d->action[AC_USE] = false;
        }
    }

    void runtriggers(int n, gameent *d)
    {
        loopenti(TRIGGER) if(ents[i]->type == TRIGGER && ents[i]->attrs[0] == n && ents[i]->attrs[2] == TRIG_A_MANUAL) runtrigger(i, d, false);
    }
    ICOMMAND(0, exectrigger, "i", (int *n), if(identflags&IDF_MAP) runtriggers(*n, triggerclient ? triggerclient : game::player1));

    bool execitem(int n, int cn, dynent *d, float dist, bool local)
    {
        gameentity &e = *(gameentity *)ents[n];
        switch(enttype[e.type].usetype)
        {
            case EU_ITEM:
            {
                if(local && gameent::is(d) && (e.type != WEAPON || ((gameent *)d)->action[AC_USE]))
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
                    if(!isallowed(e)) break;

                    if(e.attrs[8]&(1<<TELE_NOAFFIN))
                    {
                        if(gameent::is(d) && physics::hasaffinity((gameent *)d)) break;
                        if(projent::is(d) && ((projent *)d)->type == PROJ_AFFINITY) break;
                    }

                    int millis = d->lastused(n);
                    if(millis && lastmillis-millis < triggertime(e)) break;
                    e.lastemit = lastmillis;

                    static vector<int> teleports;
                    teleports.setsize(0);
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

                        fixrange(yaw, pitch);
                        if(mag != 0 && f.attrs[5] < 6) d->vel = vec(yaw*RAD, pitch*RAD).mul(mag);

                        switch(f.attrs[5]%3)
                        {
                            case 2: break; // keep
                            case 1: // relative
                            {
                                float relyaw = (e.attrs[0] < 0 ? (lastmillis/5)%360 : e.attrs[0])-180, relpitch = e.attrs[1];
                                fixrange(relyaw, relpitch);
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

                        fixrange(d->yaw, d->pitch);

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
                        if(gameent::is(d)) game::suicide((gameent *)d, HIT_SPAWN);
                        else if(projent::is(d))
                        {
                            projent *g = (projent *)d;
                            switch(g->projtype)
                            {
                                case PROJ_ENTITY: case PROJ_AFFINITY:
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
                    if(!isallowed(e)) break;

                    int millis = d->lastused(n);
                    if(e.attrs[5] != 3 && millis && lastmillis-millis < triggertime(e)) break;

                    bool inhibit = false;
                    loopenti(PUSHER) if(ents[i]->type == PUSHER)
                    { // check for a previous pusher in a chain
                        gameentity &f = *(gameentity *)ents[i];
                        loopvj(f.links) if(f.links[j] == n)
                        { // pusher is part of this chain
                            loopvkrev(d->used) if(ents.inrange(d->used[k].ent) && ents[d->used[k].ent]->type == PUSHER)
                            {
                                if(d->used[k].ent != i) inhibit = true; // not the previous in a chain
                                else
                                {
                                    int fmillis = d->lastused(i);
                                    if(fmillis && lastmillis-fmillis >= triggertime(e)) inhibit = true;
                                }
                                break;
                            }
                            break;
                        }
                        if(inhibit) break;
                    }
                    if(inhibit) break;

                    e.lastemit = lastmillis;
                    d->setused(n, lastmillis);

                    float mag = e.attrs[2] != 0 ? e.attrs[2] : 1, maxrad = e.attrs[3] ? e.attrs[3] : enttype[PUSHER].radius, minrad = e.attrs[4];
                    if(dist > 0 && minrad > 0 && maxrad > minrad && dist > minrad && maxrad >= dist) mag *= 1.f-clamp((dist-minrad)/float(maxrad-minrad), 0.f, 1.f);
                    mag *= d->weight/250.f;

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
                            if(e.attrs[5] != 3 || !millis || lastmillis - millis >= triggertime(e)) execlink(g, n, true);
                            g->doimpulse(IM_T_PUSHER, lastmillis, e.attrs[10]);
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
                    if(!local || d->state != CS_ALIVE || !gameent::is(d) || !isallowed(e)) break;

                    gameent *g = (gameent *)d;
                    if((e.attrs[2] == TRIG_A_ACTION && g->action[AC_USE] && g == game::player1) || e.attrs[2] == TRIG_A_AUTO)
                        runtrigger(n, g);
                }
                else if(e.type == CHECKPOINT)
                {
                    if(!local || d->state != CS_ALIVE || !gameent::is(d) || !m_speedrun(game::gamemode) || !isallowed(e)) break;

                    gameent *g = (gameent *)d;
                    if(m_ra_gauntlet(game::gamemode, game::mutators) && g->team != T_ALPHA) break;
                    if(!g->cpnodes.empty() && g->cpnodes.find(n) >= 0) break;
                    g->setcheckpoint(n, lastmillis, e.attrs[6]);
                    client::addmsg(N_TRIGGER, "ri2", g->clientnum, n);
                }
                break;
            }
        }
        return false;
    }

    void checkitems(dynent *d)
    {
        d->lastactitem = lastmillis;

        if(!gs_playing(game::gamestate)) return;

        bool local = false;
        if(d != game::player1 && (!gameent::is(d) || !((gameent *)d)->ai))
        {
            if(!d->isnotalive()) return;
        }
        else
        {
            if(!d->isactive()) return;
            local = true;
        }

        vec pos = d->center();
        float radius = max(d->xradius, d->yradius);
        if(gameent::is(d)) radius = max(d->height*0.5f, radius);

        if(collateitems(d, pos, radius))
        {
            bool tried = false;
            loopv(d->actitems)
            {
                actitem &t = d->actitems[i];

                if(t.millis != d->lastactitem) break; // rest are invalid

                int ent = -1, cn = -1;
                float dist = 0;

                switch(t.type)
                {
                    case ACTITEM_ENT:
                    {
                        if(!ents.inrange(t.ent)) break;

                        ent = t.ent;
                        dist = t.score;
                        break;
                    }
                    case ACTITEM_PROJ:
                    {
                        projent *proj = projs::findprojseq(PROJ_ENTITY, t.id);
                        if(!proj || !proj->owner) break;
                        cn = proj->owner->clientnum;
                        ent = proj->id;
                        dist = t.score;
                        break;
                    }
                    default: break;
                }

                if(ents.inrange(ent) && execitem(ent, cn, d, dist, local)) tried = true;
            }

            if(local && tried && gameent::is(d))
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
                putint(p, e.flags&EF_VIRTUAL ? int(0 - e.type) : int(e.type));
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

    void setspawn(int n, int m, int p)
    {
        if(!ents.inrange(n)) return;
        gameentity &e = *(gameentity *)ents[n];
        bool on = m%2, spawned = e.spawned();

        e.setspawned(on);

        if(e.type == TRIGGER)
        {
            if(cantrigger(n) && (e.attrs[1] == TRIG_TOGGLE || e.attrs[1] == TRIG_LINKED || e.attrs[1] == TRIG_ONCE) && (m >= 2 || e.lastemit <= 0 || e.spawned() != spawned))
            {
                if(m >= 2) e.lastemit = -1;
                else if(e.lastemit > 0)
                {
                    int last = lastmillis - e.lastemit, trig = triggertime(e, true);
                    if(last > 0 && last < trig) e.lastemit = lastmillis - (trig - last);
                    else e.lastemit = lastmillis;
                }
                else e.lastemit = lastmillis;

                execlink(NULL, n, false);
            }
        }
        else e.lastemit = lastmillis;
        e.lastspawn = lastmillis;
        e.spawndelay = p;

        if(gs_playing(game::gamestate) && enttype[e.type].usetype == EU_ITEM)
        {
            int attr = m_attr(e.type, e.attrs[0]);

            if(e.spawned())
            {
                static fx::FxHandle fx = fx::getfxhandle("FX_ITEM_SPAWN");
                fx::createfx(fx)
                    .setfrom(e.pos())
                    .setscale(enttype[e.type].radius*0.125f)
                    .setparam(0, attr);
            }

            if(gs_playing(game::gamestate) && e.type == WEAPON && (itemannouncespawn&(1<<attr)) != 0)
            {
                gamelog *log = new gamelog(GAMELOG_EVENT);
                log->addlist("args", "type", "item");
                log->addlist("args", "action", "spawn");
                log->addlist("args", "entity", n);
                log->addlist("args", "attr", attr);
                log->addlist("args", "spawn", m);
                log->addlist("args", "colour", colourwhite);
                log->addlistf("args", "console", "A %s has spawned", e.type == WEAPON && isweap(attr) ? W(attr, longname) : enttype[e.type].name);
                if(!log->push()) DELETEP(log);
            }
        }
    }
    ICOMMAND(0, entspawned, "i", (int *n), intret(ents.inrange(*n) && ents[*n]->spawned() ? 1 : 0));

    extentity *newent() { return new gameentity; }
    void deleteent(extentity *e) { delete (gameentity *)e; }

    void clearents()
    {
        entindex = -1;
        while(ents.length()) deleteent(ents.pop());
        memset(firstenttype, 0, sizeof(firstenttype));
        memset(firstusetype, 0, sizeof(firstusetype));
        memset(lastenttype, 0, sizeof(lastenttype));
        memset(lastusetype, 0, sizeof(lastusetype));
    }

    bool cansee(int n)
    {
        if(!game::player1->isediting()) return false;
        if(!ents.inrange(n) || ents[n]->flags&EF_VIRTUAL) return false;
        if(ents[n]->type == NOTUSED && (enthover.find(n) < 0 && entgroup.find(n) < 0)) return false;
        return true;
    }

    void cleansound(int n)
    {
        gameentity &e = *(gameentity *)ents[n];
        if(issound(e.schan))
        {
            soundsources[e.schan].clear();
            e.schan = -1; // prevent clipping when moving around
            if(e.type == MAPSOUND) e.lastemit = lastmillis + 1000;
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
                    if(verbose) conoutf(colourred, "WARNING: automatic reciprocal link between %d and %d added", n, ent);
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
                static const int mdlfmap[MDLF_MAX] = { EF_HIDE, EF_NOCOLLIDE, EF_NOSHADOW, EF_NOTRIGCOL };
                loopj(MDLF_MAX)
                {
                    if(e.flags&mdlfmap[j] && !(e.attrs[6]&(1<<j))) e.flags &= ~mdlfmap[j];
                    else if(!(e.flags&mdlfmap[j]) && e.attrs[6]&(1<<j)) e.flags |= mdlfmap[j];
                }
                while(e.attrs[7] < 0) e.attrs[7] += 0x1000000; // colour
                while(e.attrs[7] > 0xFFFFFF) e.attrs[7] -= 0x1000000; // wrap both ways
                while(e.attrs[18] < 0) e.attrs[18] += ANIM_MAX;
                while(e.attrs[18] >= ANIM_MAX) e.attrs[18] -= ANIM_MAX;
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
                    while(e.attrs[0] < -1) e.attrs[0] += numsounds+1;
                    while(e.attrs[0] >= numsounds) e.attrs[0] -= numsounds+1;
                }
                else if(e.attrs[0] < -1) e.attrs[0] = -1;

                if(e.attrs[1] < 0) e.attrs[1] = 100; // gain, clamp
                if(e.attrs[2] < 0) e.attrs[2] = 100; // pitch, clamp
                if(e.attrs[3] < 0) e.attrs[3] = 100; // rolloff, clamp
                if(e.attrs[4] < 0) e.attrs[4] = 0; // refdist, clamp
                if(e.attrs[5] < 0) e.attrs[5] = 0; // maxdist, clamp
                if(e.attrs[6] < 0) e.attrs[6] = 0; // flags, clamp
                FIXEMIT;
                break;
            }
            case LIGHTFX:
            {
                while(e.attrs[0] < 0) e.attrs[0] += LFX_MAX; // type
                while(e.attrs[0] >= LFX_MAX) e.attrs[0] -= LFX_MAX; // wrap both ways
                if(e.attrs[2] < 0) e.attrs[2] = 0; // min, clamp
                if(e.attrs[3] < 0) e.attrs[3] = 0; // max, clamp
                if(e.attrs[4] < 0) e.attrs[4] = 0; // flags, clamp
                FIXEMIT;
                break;
            }
            case PHYSICS:
            {
                while(e.attrs[0] < 0) e.attrs[0] += PHYSICS_MAX; // type
                while(e.attrs[0] >= PHYSICS_MAX) e.attrs[0] -= PHYSICS_MAX; // wrap both ways
                if(e.attrs[2] < 0) e.attrs[1] = 0; // width, clamp
                if(e.attrs[3] < 0) e.attrs[2] = 0; // length, clamp
                if(e.attrs[4] < 0) e.attrs[3] = 0; // height, clamp
                while(e.attrs[5] < 0) e.attrs[4] += 101; // falloff, wrap
                while(e.attrs[5] > 100) e.attrs[4] -= 101; // falloff, wrap
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
                while(e.attrs[5] < 0) e.attrs[5] += 4; // type
                while(e.attrs[5] >= 4) e.attrs[5] -= 4; // wrap both ways
                break;
            }
            case TRIGGER:
            {
                while(e.attrs[1] < 0) e.attrs[1] += TRIG_MAX; // type
                while(e.attrs[1] >= TRIG_MAX) e.attrs[1] -= TRIG_MAX; // wrap both ways
                while(e.attrs[2] < 0) e.attrs[2] += TRIG_A_MAX; // action
                while(e.attrs[2] >= TRIG_A_MAX) e.attrs[2] -= TRIG_A_MAX; // wrap both ways
                if(e.attrs[3] < 0) e.attrs[3] = 1; // radius, clamp
                while(e.attrs[4] < 0) e.attrs[4] += TRIG_S_ALL+1; // state
                while(e.attrs[4] >= TRIG_S_ALL+1) e.attrs[4] -= TRIG_S_ALL+1; // wrap both ways
                if(cantrigger(n)) loopv(e.links) if(ents.inrange(e.links[i]) && (ents[e.links[i]]->type == MAPMODEL || ents[e.links[i]]->type == PARTICLES || (ents[e.links[i]]->type == MAPSOUND && ents[e.links[i]]->attrs[0] >= 0) || ents[e.links[i]]->type == LIGHTFX))
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
                while(e.attrs[7] >= ANIM_MAX) e.attrs[7] -= ANIM_MAX;
                if(e.attrs[8] < 0) e.attrs[8] = 0; // anim speed, clamp
                break;
            }
            case CAMERA:
            {
                while(e.attrs[0] < 0) e.attrs[0] += CAMERA_MAX;
                while(e.attrs[0] >= CAMERA_MAX) e.attrs[0] -= CAMERA_MAX;
                while(e.attrs[1] < 0) e.attrs[1] += CAMERA_F_ALL+1;
                while(e.attrs[1] > CAMERA_F_ALL) e.attrs[1] -= CAMERA_F_ALL+1;
                FIXDIRYPL(2, 3);
                if(e.attrs[4] < 0) e.attrs[4] = 0; // maxdist, limit
                if(e.attrs[5] < 0) e.attrs[5] = 0; // mindist, limit
                while(e.attrs[11] < 0) e.attrs[11] += 151; // fov, clamp
                while(e.attrs[11] >= 150) e.attrs[11] -= 151; // fov, clamp
                break;
            }
            case MAPUI:
            {
                while(e.attrs[1] < 0) e.attrs[1] += UI::MAPUI_ALL+1; // flags, clamp
                while(e.attrs[1] > UI::MAPUI_ALL) e.attrs[1] -= UI::MAPUI_ALL+1; // flags, clamp
                while(e.attrs[2] < -1) e.attrs[2] += 361; // yaw
                while(e.attrs[2] >= 360) e.attrs[2] -= 361; // has -1 for rotating effect
                while(e.attrs[3] < -181) e.attrs[3] += 362; // pitch
                while(e.attrs[3] > 181) e.attrs[3] -= 362; // has -181/181 for rotating effect
                if(e.attrs[4] < 0) e.attrs[4] = 0; // radius, limit
                while(e.attrs[6] < 0) e.attrs[6] += 181; // yaw detent, clamp
                while(e.attrs[6] > 180) e.attrs[6] -= 181; // yaw detent, clamp
                while(e.attrs[7] < 0) e.attrs[7] += 181; // pitch detent, clamp
                while(e.attrs[7] > 180) e.attrs[7] -= 181; // pitch detent, clamp
                while(e.attrs[8] < 0) e.attrs[8] += 0xFFFFFF + 1; // colour, clamp
                while(e.attrs[8] > 0xFFFFFF) e.attrs[8] -= 0xFFFFFF + 1; // colour, clamp
                while(e.attrs[9] < 0) e.attrs[9] += 101; // blend, clamp
                while(e.attrs[9] > 100) e.attrs[9] -= 101; // blend, clamp
                break;
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

        if(enttype[e.type].palattr >= 0)
        {
            if(e.attrs[enttype[e.type].palattr] < 0) e.attrs[enttype[e.type].palattr] = 0;
            if(e.attrs[enttype[e.type].palattr + 1] < 0) e.attrs[enttype[e.type].palattr + 1] = 0;
        }
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

        if(e.type == ET_PHYSICS)
        {
            int proceed = 0;
            loopj(3) if(e.attrs[2 + j]) proceed++;
            if(proceed >= 3) e.flags |= EF_BBZONE;
            else e.flags &= ~EF_BBZONE;
        }
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
    int announcerchan = -1;
    int announce(int idx, gameent *d, int chan, int flags, float gain)
    {
        int sourceidx = -1;

        if(d)
        {
            physent *pl = d;
            vec *pos = game::getplayersoundpos(d);
            int *hook = chan >= 0 && chan < PLCHAN_MAX ? &d->plchan[chan] : NULL;

            if(d == game::focus)
            {
                pl = camera1;
                pos = &camera1->o;
                hook = NULL;
            }

            return emitsound(idx, pos, pl, hook, flags|SND_PRIORITY|SND_TRACKED|SND_NOATTEN|SND_NOENV, gain);
        }
        bool found = false;
        loopenti(MAPSOUND)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.attrs[0] >= 0) continue;
            int eflags = flags|SND_TRACKED;
            loopk(SND_LAST) if(e.attrs[6]&(1<<k)) flags |= 1<<k;
            float entgain = e.attrs[1] > 0 ? e.attrs[1]/100.f : 1.f, pitch = e.attrs[2] > 0 ? e.attrs[2]/100.f : 1.f,
                  rolloff = e.attrs[3] > 0 ? e.attrs[3]/100.f : -1.f, refdist = e.attrs[4] > 0 ? e.attrs[4]/100.f : -1.f, maxdist = e.attrs[5] > 0 ? e.attrs[5]/100.f : -1.f;

            sourceidx = emitsound(idx, e.getpos(), NULL, &e.schan, eflags, entgain * gain, pitch, rolloff, refdist, maxdist);

            if(sourceidx >= 0) found = true;
        }
        if(!found) sourceidx = emitsoundpos(idx, vec(worldsize/2, worldsize/2, worldsize), &announcerchan, flags|SND_PRIORITY, gain);

        return sourceidx;
    }

    ICOMMAND(0, announce, "iiif", (int *sound, int *oncamera, int *flags, float *gain),
    {
        intret(announce(*sound, *oncamera ? game::focus : NULL, -1, *flags, *gain));
    });

    int emitmapsound(gameentity &e, bool looping)
    {
        if(issound(e.schan))
        {
            e.getcurpos();
            return e.schan;
        }

        int flags = SND_MAP|SND_TRACKED|SND_VELEST;
        if(looping) flags |= SND_LOOP;
        loopk(SND_LAST) if(e.attrs[6]&(1<<k)) flags |= 1<<k;

        float gain = e.attrs[1] > 0 ? e.attrs[1] / 100.f : 1.f, pitch = e.attrs[2] > 0 ? e.attrs[2] / 100.f : 1.f,
              rolloff = e.attrs[3] > 0 ? e.attrs[3] / 100.f : -1.f, refdist = e.attrs[4] > 0 ? e.attrs[4] / 100.f : -1.f, maxdist = e.attrs[5] > 0 ? e.attrs[5] / 100.f : -1.f,
              offset = e.attrs[10] / 1000.0f;

        return emitsound(e.attrs[0], e.getpos(), NULL, &e.schan, flags, gain, pitch, rolloff, refdist, maxdist, 0, offset, e.attrs[11]);
    }

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
            if(!isallowed(f)) continue;
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
                    if(f.attrs[0] < 0) break;
                    f.lastemit = e.lastemit;
                    if(e.type == TRIGGER) f.setspawned(TRIGSTATE(e.spawned(), e.attrs[4]));
                    else if(local) commit = true;
                    emitmapsound(f, false);
                    break;
                }
                default: break;
            }
        }
        if(d && gameent::is(d) && commit) client::addmsg(N_EXECLINK, "ri2", ((gameent *)d)->clientnum, index);
    }

    bool tryspawn(dynent *d, const vec &o, float yaw, float pitch)
    {
        fixfullrange(d->yaw = yaw, d->pitch = pitch, d->roll = 0);
        (d->o = o).z += d->height+d->aboveeye;
        return physics::entinmap(d, true);
    }

    void spawnplayer(gameent *d, int ent, bool suicide)
    {
        if(ent >= 0 && ents.inrange(ent))
        {
            gameentity &e = *(gameentity *)ents[ent];
            vec pos = e.pos();
            switch(e.type)
            {
                case PLAYERSTART: case ACTOR:
                    if(tryspawn(d, pos, e.attrs[1], e.attrs[2])) return;
                    break;
                case CHECKPOINT:
                {
                    float yaw = e.attrs[1], pitch = e.attrs[2];
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
                            loopenti(PLAYERSTART) if(ents[i]->type == PLAYERSTART && isallowed(i))
                            {
                                gameentity &e = *(gameentity *)ents[i];
                                if(e.attrs[0] != d->team) continue;
                                spawns.add(i);
                            }
                        }
                        break;
                    case 1: case 2:
                        loopenti(PLAYERSTART) if(ents[i]->type == PLAYERSTART && (k == 2 || isallowed(i)))
                        {
                            gameentity &e = *(gameentity *)ents[i];
                            if(k == 1 && e.attrs[0] != d->team) continue;
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
        if(!m_edit(game::gamemode) && suicide) game::suicide(d, HIT_SPAWN);
    }

    void editent(int i, bool local)
    {
        extentity &e = *ents[i];
        cleansound(i);
        updateenvzone(&e);

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
                conoutf(colourred, "Entity %s (%d) and %s (%d) are not linkable", enttype[ents[index]->type].name, index, enttype[ents[node]->type].name, node);

            return false;
        }
        if(msg) conoutf(colourred, "Entity %d and %d are unable to be linked as one does not seem to exist", index, node);
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
                    if(verbose > 2) conoutf(colourgrey, "Entity %s (%d) and %s (%d) delinked", enttype[ents[index]->type].name, index, enttype[ents[node]->type].name, node);
                    return true;
                }
                else if(toggle && canlink(node, index))
                {
                    f.links.add(index);
                    if(recip && e.links.find(node) < 0) e.links.add(node);
                    fixentity(node, true);
                    if(local && m_edit(game::gamemode)) client::addmsg(N_EDITLINK, "ri3", 1, node, index);
                    if(verbose > 2) conoutf(colourgrey, "Entity %s (%d) and %s (%d) linked", enttype[ents[node]->type].name, node, enttype[ents[index]->type].name, index);
                    return true;
                }
            }
            else if(toggle && canlink(node, index) && (g = f.links.find(index)) >= 0)
            {
                f.links.remove(g);
                if(recip && (h = e.links.find(node)) >= 0) e.links.remove(h);
                fixentity(node, true);
                if(local && m_edit(game::gamemode)) client::addmsg(N_EDITLINK, "ri3", 0, node, index);
                if(verbose > 2) conoutf(colourgrey, "Entity %s (%d) and %s (%d) delinked", enttype[ents[node]->type].name, node, enttype[ents[index]->type].name, index);
                return true;
            }
            else if(toggle || add)
            {
                e.links.add(node);
                if(recip && f.links.find(index) < 0) f.links.add(index);
                fixentity(index, true);
                if(local && m_edit(game::gamemode)) client::addmsg(N_EDITLINK, "ri3", 1, index, node);
                if(verbose > 2) conoutf(colourgrey, "Entity %s (%d) and %s (%d) linked", enttype[ents[index]->type].name, index, enttype[ents[node]->type].name, node);
                return true;
            }
        }
        if(verbose > 2)
            conoutf(colourred, "Entity %s (%d) and %s (%d) failed linking", enttype[ents[index]->type].name, index, enttype[ents[node]->type].name, node);
        return false;
    }

    void unlinkent(int index)
    {
        if(ents.inrange(index))
        {
            gameentity &e = *(gameentity *)ents[index];
            if(e.links.empty()) return;

            loopv(e.links) if(ents.inrange(e.links[i]))
            {
                gameentity &f = *(gameentity *)ents[e.links[i]];
                if(f.links.empty()) continue;

                int linkidx = f.links.find(index);
                if(linkidx >= 0) f.links.remove(linkidx);
            }

            e.links.shrink(0);
        }
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
        if(gver <= 255) switch(e.type)
        { // adding in modes/muts attr, this is out of order because it is an attr move and palttr/modesattr needs it to be properly reflected below
            case LIGHT: case PARTICLES: case MAPSOUND: case MAPMODEL: case LIGHTFX: case DECAL: case WIND: case TELEPORT:
            {
                for(int q = enttype[e.type].numattrs-1; q >= enttype[e.type].modesattr+2; q--) e.attrs[q] = e.attrs[q-2];
                loopi(2) e.attrs[enttype[e.type].modesattr+i] = 0;
                break;
            }
            default: break;
        }

        if(mver <= 51 && e.type == MAPSOUND)
        {
            int gain = e.attrs[3] > 0 ? int(e.attrs[3] / 255.f * 100) : 0, rolloff = e.attrs[1] > soundrolloff ? int(soundrolloff / float(e.attrs[1]) * 100) : 0, refdist = e.attrs[2] > soundrefdist ? e.attrs[2] : 0;
            loopi(4) e.attrs[6-i] = e.attrs[4-i];
            e.attrs[1] = gain;
            e.attrs[2] = 0;
            e.attrs[3] = rolloff;
            e.attrs[4] = refdist;
            e.attrs[5] = 0;
        }

        if(gver <= 218 && e.type == WEAPON)
        { // insert mine before rockets (9 -> 10) after grenades (8)
            if(e.attrs[0] >= 9) e.attrs[0]++;
        }

        if(gver <= 221 && e.type == WEAPON)
        { // insert zapper before rifle (7 -> 8) after plasma (6)
            if(e.attrs[0] >= 7) e.attrs[0]++;
        }

        if(gver <= 223 && e.type == ROUTE) e.type = NOTUSED; // removing old route entity

        bool fixedpalette = false;
        if(gver <= 244)
        {
            if((e.type == PLAYERSTART || e.type == AFFINITY) && e.attrs[0] > T_OMEGA) e.type = NOTUSED;
            if(e.type == PARTICLES)
            {
                switch(e.attrs[0])
                {
                    case 0: fixedpalette = true; game::fixpalette(e.attrs[5], e.attrs[6], gver); break;
                    case 3: fixedpalette = true; game::fixpalette(e.attrs[3], e.attrs[4], gver); break;
                    case 4: fixedpalette = true; game::fixpalette(e.attrs[6], e.attrs[7], gver); break;
                    case 5: fixedpalette = true; game::fixpalette(e.attrs[3], e.attrs[4], gver); break;
                    case 6: fixedpalette = true; game::fixpalette(e.attrs[4], e.attrs[5], gver); game::fixpalette(e.attrs[6], e.attrs[7], gver); break;
                    case 7: fixedpalette = true; game::fixpalette(e.attrs[6], e.attrs[7], gver); break;
                    case 8: fixedpalette = true; game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                    case 9: fixedpalette = true; game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                    case 10: fixedpalette = true; game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                    case 11: fixedpalette = true; game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                    case 12: fixedpalette = true; game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                    case 13: fixedpalette = true; game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                    case 14: fixedpalette = true; game::fixpalette(e.attrs[8], e.attrs[9], gver); break;
                    case 15: fixedpalette = true; game::fixpalette(e.attrs[8], e.attrs[9], gver); break;
                    default: break;
                }
            }
            else if(enttype[e.type].palattr >= 0)
            {
                game::fixpalette(e.attrs[enttype[e.type].palattr], e.attrs[enttype[e.type].palattr+1], gver);
                fixedpalette = true;
            }
            if(enttype[e.type].modesattr >= 0)
            { // removing freestyle and multi
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

        if(gver <= 247 && e.type == RAIL) e.type = NOTUSED;

        if(gver <= 248 && e.type == RAIL && (e.attrs[1]&(1<<RAIL_YAW) || e.attrs[1]&(1<<RAIL_PITCH))) e.attrs[1] |= (1<<RAIL_SEEK);

        if(gver <= 249 && e.type == RAIL && e.attrs[4] < 0)
        {
            e.attrs[5] = e.attrs[2]+e.attrs[4];
            e.attrs[4] = 0-e.attrs[4];
            e.attrs[5] = 0;
        }

        if(e.type != RAIL && gver <= 250) loopv(e.links)
        { // switch linking to a rail / linking from it
            int link = e.links[i];
            if(!ents.inrange(link) || ents[link]->type != RAIL) continue;
            gameentity &f = *(gameentity *)ents[link];
            f.links.add(id);
            e.links.remove(i--);
            conoutf(colourwhite, "switched rail link between %d and %d", id, link);
        }

        if(gver <= 259 && e.type == TELEPORT && e.attrs[4]) e.attrs[4] = (((e.attrs[4]&0xF)<<4)|((e.attrs[4]&0xF0)<<8)|((e.attrs[4]&0xF00)<<12))+0x0F0F0F;

        if(gver <= 269)
        { // adding murder in the dark
            if(enttype[e.type].modesattr >= 0)
            {
                int mattr = enttype[e.type].modesattr+1;
                if(e.attrs[mattr] != 0)
                {
                    static const int G_M_OLDNUM = 16, G_M_START = 13; // move game all game specific
                    int oldmuts = e.attrs[mattr] > 0 ? e.attrs[mattr] : 0-e.attrs[mattr], newmuts = 0;
                    loopi(G_M_OLDNUM)
                    {
                        if(!(oldmuts&(1<<i))) continue;
                        if(i >= G_M_START) newmuts |= (1<<(i+1)); // move forward
                        else newmuts = (1<<i); // retain as-is
                    }
                    e.attrs[mattr] = e.attrs[mattr] > 0 ? newmuts : 0-newmuts;
                }
            }
        }

        if(gver <= 273 && e.type == WEAPON)
        { // insert corroder before grenade (9 -> 10) after rifle (8)
            if(e.attrs[0] >= 9) e.attrs[0]++;

            if(!fixedpalette)
            {
                if(e.type == PARTICLES)
                {
                    switch(e.attrs[0])
                    {
                        case 0: fixedpalette = true; game::fixpalette(e.attrs[5], e.attrs[6], gver); break;
                        case 3: fixedpalette = true; game::fixpalette(e.attrs[3], e.attrs[4], gver); break;
                        case 4: fixedpalette = true; game::fixpalette(e.attrs[6], e.attrs[7], gver); break;
                        case 5: fixedpalette = true; game::fixpalette(e.attrs[3], e.attrs[4], gver); break;
                        case 6: fixedpalette = true; game::fixpalette(e.attrs[4], e.attrs[5], gver); game::fixpalette(e.attrs[6], e.attrs[7], gver); break;
                        case 7: fixedpalette = true; game::fixpalette(e.attrs[6], e.attrs[7], gver); break;
                        case 8: fixedpalette = true; game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                        case 9: fixedpalette = true; game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                        case 10: fixedpalette = true; game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                        case 11: fixedpalette = true; game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                        case 12: fixedpalette = true; game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                        case 13: fixedpalette = true; game::fixpalette(e.attrs[9], e.attrs[10], gver); break;
                        case 14: fixedpalette = true; game::fixpalette(e.attrs[8], e.attrs[9], gver); break;
                        case 15: fixedpalette = true; game::fixpalette(e.attrs[8], e.attrs[9], gver); break;
                        default: break;
                    }
                }
                else if(enttype[e.type].palattr >= 0)
                {
                    game::fixpalette(e.attrs[enttype[e.type].palattr], e.attrs[enttype[e.type].palattr+1], gver);
                    fixedpalette = true;
                }
            }
        }
    }

    int getfirstroute()
    {
        if(lastroutenode == routeid) return -1;

        int firstroute = -1;
        loopenti(ROUTE) if(ents[i]->type == ROUTE && ents[i]->attrs[0] == routeid)
        {
            firstroute = i;
            break;
        }
        return firstroute;
    }

    void initents(int mver, char *gid, int gver)
    {
        int numcorroders = 0, nummines = 0;
        lastroutenode = routeid = -1;
        numactors = lastroutetime = droproute = 0;
        airnodes.setsize(0);
        ai::oldwaypoints.setsize(0);
        progress(0, "Setting entity attributes..");

        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            e.attrs.setsize(numattrs(e.type), 0);
            if(gver < VERSION_GAME) importent(e, i, mver, gver);
            fixentity(i, false);
            if(e.type == ACTOR)
            {
                int atype = clamp(e.attrs[0], 0, A_TOTAL-1) + A_ENEMY;
                if(atype < A_ENVIRONMENT) numactors++;
            }
            if(e.type == WEAPON)
            {
                if(e.attrs[0] == W_MINE) nummines++;
                if(e.attrs[0] == W_CORRODER) numcorroders++;
            }
            progress((i+1)/float(ents.length()), "Setting entity attributes..");
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
                e.flags |= EF_VIRTUAL;
                e.o = ents[i]->o;
                e.attrs.add(0, numattrs(ACTOR));
                e.attrs[0] = A_ENEMY + (i % A_CLAMP);
                
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

        if(!m_edit(game::gamemode))
        {
            if(!numcorroders)
            {
                int iter = 0;
                bool iterchk = nummines > 2;
                loopv(ents)
                {
                    if(ents[i]->type != WEAPON || ents[i]->attrs[0] != W_MINE) continue;
                    if(iterchk && (iter++)%2 != 0) continue;
                    ents[i]->attrs[0] = W_CORRODER;
                }
            }

            loopi(W_ALL) // create virtual weapon entities
            {
                extentity &e = *newent();
                ents.add(&e);
                e.type = WEAPON;
                e.flags |= EF_VIRTUAL;
                e.o = vec(0, 0, 0);
                e.attrs.add(0, numattrs(WEAPON));
                e.attrs[0] = i;
            }
        }

        progress(0, "Preparing entities..");

        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];

            if(e.flags&EF_VIRTUAL) continue; // skip virtual entities

            if(e.type >= 0 && e.type < MAXENTTYPES)
            {
                firstenttype[e.type] = min(firstenttype[e.type], i);
                firstusetype[enttype[e.type].usetype] = min(firstusetype[enttype[e.type].usetype], i);
                lastenttype[e.type] = max(lastenttype[e.type], i+1);
                lastusetype[enttype[e.type].usetype] = max(lastusetype[enttype[e.type].usetype], i+1);
            }

            if(enttype[e.type].usetype == EU_ITEM || e.type == TRIGGER) setspawn(i, 0);

            if(enttype[e.type].syncs && enttype[e.type].synckin) loopvj(e.links)
            {
                int n = e.links[j];
                if(!ents.inrange(n) || ents[n]->type != e.type) continue;
                if(e.kin.find(n) < 0) e.kin.add(n);
            }

            progress((i+1)/float(ents.length()), "Preparing entities..");
        }
        initrails();
        initmapsound();
    }

    #define renderfocus(i,f) \
    { \
        gameentity &e = *(gameentity *)ents[i]; \
        loopj(2) { f; } \
    }
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

    void renderentshow(gameentity &e, int idx, int level, bool dynamic = false)
    {
        if(dynamic && (e.dynamic() || showentdynamic >= level)) return;
        vec pos = dynamic ? e.pos() : e.o;
        if(pos.squaredist(camera1->o) > entityshowmaxdist * entityshowmaxdist) return;
        #define entdirpart(o,y,p,length,fade,colour) \
        { \
            float targyaw = y, targpitch = p; \
            if(dynamic) \
            { \
                targyaw += e.yaw; \
                targpitch += e.pitch; \
                fixrange(targyaw, targpitch); \
            } \
            part_dir(o, targyaw, targpitch, length, showentsize, 1, fade, colour, showentinterval); \
        }
        if(showentradius >= level)
        {
            switch(e.type)
            {
                case PLAYERSTART:
                {
                    part_radius(vec(pos).add(vec(0, 0, actors[A_PLAYER].height*0.5f)), vec(actors[A_PLAYER].radius, actors[A_PLAYER].radius, actors[A_PLAYER].height*0.5f), showentsize, 1, 1, TEAM(e.attrs[0], colour));
                    break;
                }
                case ENVMAP:
                {
                    int s = e.attrs[0] ? clamp(e.attrs[0], 0, 10000) : envmapradius;
                    if(s > 0) part_radius(pos, vec(float(s)), showentsize, 1, 1, entradiuscolour);
                    break;
                }
                case ACTOR:
                {
                    int atype = clamp(e.attrs[0], 0, int(A_TOTAL-1)) + A_ENEMY, team = atype >= A_ENVIRONMENT ? T_ENVIRONMENT : T_ENEMY;
                    part_radius(vec(pos).add(vec(0, 0, actors[atype].height*0.5f)), vec(actors[atype].radius, actors[atype].radius, actors[atype].height*0.5f), showentsize, 1, 1, TEAM(team, colour));
                    part_radius(pos, vec(ai::ALERTMAX), showentsize, 1, 1, TEAM(team, colour));
                    break;
                }
                case MAPSOUND:
                {
                    if(e.attrs[4] > 0) part_radius(pos, vec(float(e.attrs[4])), showentsize, 1, 1, entradiuscolour);
                    if(e.attrs[5] > 0) part_radius(pos, vec(float(e.attrs[5])), showentsize, 1, 1, entradiuscolour);
                    break;
                }
                case WIND:
                {
                    if(e.attrs[3] > 0) part_radius(pos, vec(float(e.attrs[3])), showentsize, 1, 1, entradiuscolour);
                    break;
                }
                case LIGHT:
                {
                    int radius = e.attrs[0], spotlight = -1;
                    vec color(1, 1, 1);
                    getlightfx(e, &radius, &spotlight, &color, true);
                    if(e.attrs[0] > 0 && e.attrs[0] != radius)
                        part_radius(pos, vec(float(e.attrs[0])), showentsize, 1, 1, color.tohexcolor());
                    if(radius > 0) part_radius(pos, vec(float(radius)), showentsize, 1, 1, color.tohexcolor());
                    if(ents.inrange(spotlight))
                    {
                        gameentity &f = *(gameentity *)ents[spotlight];
                        part_cone(pos, vec(dynamic ? f.pos() : f.o).sub(pos).safenormalize(), radius, clamp(int(f.attrs[1]), 1, 89), showentsize, 1, 1, color.tohexcolor());
                    }
                    break;
                }
                case PHYSICS:
                {
                    if(e.flags&EF_BBZONE) break;
                    float radius = max(e.attrs[2], e.attrs[3], e.attrs[4]);
                    if(!radius) radius = enttype[e.type].radius;
                    part_radius(pos, vec(radius), showentsize, 1, 1, entradiuscolour);
                }
                case AFFINITY:
                {
                    float radius = enttype[e.type].radius;
                    part_radius(pos, vec(radius), showentsize, 1, 1, TEAM(e.attrs[0], colour));
                    radius = radius*3/4; // capture pickup dist
                    part_radius(pos, vec(radius), showentsize, 1, 1, TEAM(e.attrs[0], colour));
                    break;
                }
                case CAMERA:
                {
                    if(e.attrs[4] > 0) part_radius(pos, vec(float(e.attrs[4])), showentsize, 1, 1, entradiuscolour);
                    if(e.attrs[5] > 0) part_radius(pos, vec(float(e.attrs[5])), showentsize, 1, 1, entradiuscolour);
                    part_cone(pos, vec(e.attrs[2]*RAD, e.attrs[3]*RAD).safenormalize(), 128, e.attrs[11] > 0 ? clamp(e.attrs[11], 1, 89) : 89, 0, showentsize, 1, 1, entradiuscolour);
                    break;
                }
                case MAPUI:
                {
                    if(e.attrs[4] > 0) part_radius(pos, vec(float(e.attrs[4])), showentsize, 1, 1, entradiuscolour);
                    break;
                }
                default:
                {
                    float radius = enttype[e.type].radius;
                    if((e.type == TRIGGER || e.type == TELEPORT || e.type == PUSHER || e.type == CHECKPOINT) && e.attrs[e.type == CHECKPOINT ? 0 : 3])
                        radius = e.attrs[e.type == CHECKPOINT ? 0 : 3];
                    if(radius > 0) part_radius(pos, vec(radius), showentsize, 1, 1, entradiuscolour);
                    if(e.type == PUSHER && e.attrs[4] > 0 && e.attrs[4] < radius)
                        part_radius(pos, vec(float(e.attrs[4])), showentsize, 1, 1, entradiuscolour);
                    break;
                }
            }
        }

        if(showentdir >= level)
        {
            switch(e.type)
            {
                case PLAYERSTART: case CHECKPOINT: case ROUTE:
                {
                    entdirpart(pos, e.attrs[1], e.attrs[2], 4.f, 1, TEAM(e.type == PLAYERSTART ? e.attrs[0] : T_NEUTRAL, colour));
                    break;
                }
                //case MAPMODEL:
                //{
                //    entdirpart(pos, e.attrs[1], 360-e.attrs[3], 4.f, 1, entdircolour);
                //    break;
                //}
                case WIND:
                {
                    if(e.attrs[0]&WIND_EMIT_VECTORED) entdirpart(pos, e.attrs[1], 0, entdirsize, 1, entdircolour);
                    break;
                }
                case ACTOR:
                {
                    int atype = clamp(e.attrs[0], 0, int(A_TOTAL-1)) + A_ENEMY, team = atype >= A_ENVIRONMENT ? T_ENVIRONMENT : T_ENEMY;
                    entdirpart(pos, e.attrs[1], e.attrs[2], 4.f, 1, TEAM(team, colour));
                    break;
                }
                case TELEPORT:
                {
                    if(e.attrs[0] < 0) { entdirpart(pos, (lastmillis/5)%360, e.attrs[1], 4.f, 1, entdircolour); }
                    else { entdirpart(pos, e.attrs[0], e.attrs[1], entdirsize, 1, entdircolour); }
                    break;
                }
                case PUSHER:
                {
                    entdirpart(pos, e.attrs[0], e.attrs[1], 4.f+e.attrs[2], 1, entdircolour);
                    break;
                }
                case RAIL:
                {
                    entdirpart(pos, e.attrs[2], e.attrs[3], 4.f, 1, entdircolour);
                    break;
                }
                case CAMERA:
                {
                    entdirpart(pos, e.attrs[2], e.attrs[3], 4.f, 1, entdircolour);
                    break;
                }
                case MAPUI:
                {
                    entdirpart(pos, e.attrs[2], e.attrs[3], 4.f, 1, entdircolour);
                    break;
                }
                default: break;
            }
        }
        if(enttype[e.type].links && showentlinks >= level) renderlinked(e, idx);
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

    static bool hasmapsoundsel()
    {
        bool result = false;

        loopv(entgroup) if(ents[entgroup[i]]->type == MAPSOUND)
        {
            result = true;
            break;
        }

        return result;
    }

    void update()
    {
        loopv(ents)
            if(ents[i]->type != NOTUSED && !(ents[i]->flags&EF_VIRTUAL))
                ((gameentity *)ents[i])->getcurpos();
        
        loopenti(MAPSOUND)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.type == MAPSOUND && e.attrs[0] >= 0 && isallowed(e))
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
                    // Mute non-selected mapsounds
                    soundsources[e.schan].mute =
                        game::player1->state == CS_EDITING &&
                        hasmapsoundsel() &&
                        mapsoundautomute &&
                        entgroup.find(i) < 0;

                    if(triggered && soundsources[e.schan].flags&SND_LOOP && !e.spawned() && (e.lastemit < 0 || lastmillis-e.lastemit > triggertime(e, true)))
                    {
                        soundsources[e.schan].clear();
                        e.schan = -1;
                    }
                    continue;
                }
                if(triggered) continue;
                emitmapsound(e, true);
            }
        }
        
        if((m_edit(game::gamemode) || m_speedrun(game::gamemode)) && routeid >= 0 && droproute)
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
    }

    int showlevel(int n)
    {
        return game::player1->state == CS_EDITING ? ((entgroup.find(n) >= 0 || enthover.find(n) >= 0) ? 1 : 2) : 3;
    }

    bool radarallow(const vec &o, int id, vec &dir, float &dist, bool justtest = false)
    {
        if(!ents.inrange(id) || m_hard(game::gamemode, game::mutators)) return false;
        if(justtest) return true;
        dir = vec(((gameentity *)ents[id])->pos()).sub(o);
        dist = dir.magnitude();
        if(hud::radarlimited(dist)) return false;
        return true;
    }

    bool haloallow(const vec &o, int id, bool justtest)
    {
        if(!ents.inrange(id)) return false;
        if(drawtex != DRAWTEX_HALO) return true;
        if(!entityhalos || !halosurf.check()) return false;
        if(enttype[ents[id]->type].usetype != EU_ITEM && !game::player1->isediting()) return false;

        vec dir(0, 0, 0);
        float dist = -1;
        if(!radarallow(o, id, dir, dist, justtest)) return false;
        if(dist > halodist) return false;

        return true;
    }

    void render()
    {
        float offset = entrailoffset;
        if(drawtex != DRAWTEX_HALO) loopv(railways)
        {
            railway &r = railways[i];
            if(!drawtex && showentrails)
            {
                bool draw = showentrails >= showlevel(r.ent);
                if(!draw) loopvj(r.parents) if(showentrails >= showlevel(r.parents[j]))
                {
                    draw = true;
                    break;
                }
                if(draw)
                {
                    loopvj(r.rails) part_trace(vec(r.rails[j].pos).addz(offset), vec(r.rails[r.rails.inrange(j+1) ? j+1 : r.retpoint].pos).addz(offset), 1, 1, 1, entselcolourrail);
                    offset += entrailoffset;
                }
            }

            if(editmode && !showmapmodels) return;
            loopvj(r.parents)
            {
                int n = r.parents[j];
                if(!ents.inrange(n) || ents[n]->type != MAPMODEL) continue;

                gameentity &e = *(gameentity *)ents[n];
                const char *mdlname = mapmodelname(ents[n]->attrs[0]);
                if(!mdlname || !*mdlname) continue;
                modelstate mdl;
                mdl.o = e.pos();
                mdl.flags = MDL_CULL_VFC|MDL_CULL_DIST|MDL_CULL_OCCLUDED;
                getmapmodelstate(e, mdl);
                if(r.animtype > 0) mdl.anim = r.animtype|ANIM_LOOP;
                if(r.animtime > 0) mdl.basetime = r.animtime;
                if(r.animoffset > 0) mdl.basetime += r.animoffset;
                if(r.animspeed > 0) mdl.speed = 1/r.animspeed;
                mdl.yaw += e.yaw;
                mdl.pitch += e.pitch;
                dynent *d = NULL;
                loopvk(inanimates) if(inanimates[k]->control == INANIMATE_RAIL && inanimates[k]->ent == n)
                {
                    d = inanimates[k];
                    break;
                }
                rendermodel(mdlname, mdl, d);
            }
        }

        if(!(DRAWTEX_GAMEHALO&(1<<drawtex))) return;

        bool cansee = DRAWTEX_GAMEHALO&(1<<drawtex) && game::player1->isediting() && !editinhibit,
             shouldshow = !drawtex && shouldshowents(cansee ? 1 : (!entgroup.empty() || !enthover.empty() ? 2 : 3));

        int sweap = m_weapon(game::focus->actortype, game::gamemode, game::mutators),
            fstent = cansee ? 0 : firstuse(EU_ITEM),
            lstent = cansee ? ents.length() : lastuse(EU_ITEM);

        for(int i = fstent; i < lstent; i++)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.type <= NOTUSED || e.type >= MAXENTTYPES || e.flags&EF_VIRTUAL || !haloallow(camera1->o, i)) continue;
            if(!cansee && (enttype[e.type].usetype != EU_ITEM || (!e.spawned() && (e.lastemit && lastmillis - e.lastemit > 500)))) continue;
            if(shouldshow) renderfocus(i, renderentshow(e, i, showlevel(i), j != 0));

            const char *mdlname = entmdlname(e.type, e.attrs);
            if(!mdlname || !*mdlname) continue;

            modelstate mdl;
            mdl.o = e.pos();
            mdl.anim = ANIM_MAPMODEL|ANIM_LOOP;
            mdl.flags = MDL_CULL_VFC|MDL_CULL_DIST|MDL_CULL_OCCLUDED;

            int colour = -1;
            if(cansee)
            {
                if(enttype[e.type].usetype != EU_ITEM)
                {
                    if(showentmodels <= (e.type == PLAYERSTART || e.type == ACTOR ? 1 : 0))
                        continue;

                    if(e.type == AFFINITY || e.type == PLAYERSTART)
                    {
                        mdl.yaw = e.attrs[1];
                        mdl.pitch = e.attrs[2];
                        colour = TEAM(e.attrs[0], colour);
                    }
                    else if(e.type == ACTOR)
                    {
                        if(e.attrs[0] < 0 || e.attrs[0] >= A_TOTAL) continue;

                        mdl.yaw = e.attrs[1];
                        mdl.pitch = e.attrs[2];
                        mdl.size = (e.attrs[9] > 0 ? e.attrs[9] / 100.0f : 1.0f) * A(e.attrs[0], scale);

                        colour = TEAM((e.attrs[0] + A_ENEMY) >= A_ENVIRONMENT ? T_ENVIRONMENT : T_ENEMY, colour);
                    }
                }

                if(enthover.find(i) >= 0 || entgroup.find(i) >= 0)
                {
                    if(drawtex == DRAWTEX_HALO) mdl.flags |= MDL_HALO_TOP;
                    mdl.color.a *= showentavailable;
                }
                else mdl.color.a *= showentunavailable;
            }
            else if(e.spawned())
            {
                int millis = lastmillis - e.lastspawn;
                float span = millis / 250.f;

                if(span < 1.0f) mdl.o.z += 32 * (1.0f - span);

                if(drawtex != DRAWTEX_HALO && entityeffect && enttype[e.type].usetype == EU_ITEM)
                {
                    int timeoffset = int(ceilf(entityeffecttime * itemfadetime));
                    
                    if(millis < timeoffset)
                    {
                        int partoffset = timeoffset / 2;
                        float partamt = millis / float(partoffset);
                        
                        if(partamt >= 1.0f)
                        {
                            partamt = 2.0f - partamt;
                            mdl.effecttype = MDLFX_SHIMMER;
                        }
                        else mdl.effecttype = MDLFX_DISSOLVE;
                        
                        mdl.effectcolor = vec4(pulsehexcol(PULSE_HEALTH), entityeffectblend);
                        mdl.effectparams = vec4(partamt, entityeffectslice, entityeffectfade / entityeffectslice, entityeffectbright);
                    }
                }
                else if(span < 1.0f) mdl.size = mdl.color.a = span;
            }
            else if(e.lastemit)
            {
                int millis = lastmillis - e.lastemit;
                if(millis < 500) mdl.size = mdl.color.a = 1.f - (millis / 500.0f);
            }

            if(e.type == WEAPON)
            {
                int attr = m_attr(e.type, e.attrs[0]);
                if(isweap(attr))
                {
                    colour = W(attr, colour);
                    mdl.effectcolor.mul(vec::fromcolor(colour));

                    if(e.spawned() && (game::focus->isobserver() || game::focus->canuse(game::gamemode, game::mutators, e.type, attr, e.attrs, sweap, lastmillis, W_S_ALL, !showentfull)))
                    {
                        if(drawtex == DRAWTEX_HALO && attr >= W_SUPER && attr < W_ALL)
                            mdl.flags |= MDL_HALO_TOP;
                        mdl.color.a *= showentavailable;
                    }
                    else mdl.color.a *= showentunavailable;
                }
                else if(!cansee) continue;
            }
            else if(!cansee) continue;

            if(mdl.color.a <= 0) continue;

            loopk(MAXMDLMATERIALS) mdl.material[k] = bvec::fromcolor(colour);

            game::haloadjust(mdl.o, mdl);
            rendermodel(mdlname, mdl);
        }
    }

    struct teledest
    {
        int id, ed;
    };

    void maketeleport(gameentity &e, const vec &o)
    {
        static vector<teledest> teledests;
        teledests.setsize(0);

        loopv(e.links)
        {
            int id = e.links[i];
            if(!ents.inrange(id) || ents[id]->type != TELEPORT) continue;
            loopvk(ents[id]->links)
            {
                int ed = ents[id]->links[k];
                if(!ents.inrange(ed) || ents[ed]->type != ENVMAP) continue;
                teledest &d = teledests.add();
                d.id = id;
                d.ed = ed;
                break;
            }
        }

        int destid = -1, colour = e.attrs[4] ? e.attrs[4] : (e.attrs[6] || e.attrs[7] ? 0xFFFFFF : 0);
        if(!teledests.empty())
        {
            destid = (lastmillis%(teledests.length()*1000))/1000;
            if(!colour) colour = 0x8888FF;
        }

        if(colour)
        {
            if(e.attrs[6] || e.attrs[7])
            {
                vec r = vec::fromcolor(colour).mul(game::getpalette(e.attrs[6], e.attrs[7]));
                colour = (int(r.x*255)<<16)|(int(r.y*255)<<8)|(int(r.z*255));
            }

            int hintcolor = e.attrs[15] > 0 ? e.attrs[15] : vec::fromcolor(colour).neg().tohexcolor();
            float yaw = e.attrs[0] < 0 ? (lastmillis/5)%360 : e.attrs[0], blend = e.attrs[12] ? e.attrs[12]/100.f : 1.f,
                  size = e.attrs[13] > 0 ? e.attrs[13]/100.f : float(e.attrs[3] > 0 ? e.attrs[3] : enttype[e.type].radius), pitch = e.attrs[1],
                  hintblend = e.attrs[16] > 0 ? e.attrs[16]/100.f : 0.f;

            if(destid >= 0)
            {
                teledest &d = teledests[destid];
                gameentity &f = *(gameentity *)ents[d.id];
                GLuint envmap = entityenvmap(d.ed);
                float envblend = e.attrs[14] ? e.attrs[14]/100.f : 0.75f, destyaw = (f.attrs[0] < 0 ? (lastmillis/5)%360 : f.attrs[0])-yaw, destpitch = f.attrs[1]-pitch;
                part_portal(o, size, blend, yaw, pitch, PART_PORTAL_ENV, 1, colour, envmap, envblend, destyaw, destpitch, hintcolor, hintblend);
            }
            else part_portal(o, size, blend, yaw, pitch, PART_PORTAL, 1, colour, 0, 1, 0, 0, hintcolor, hintblend);
        }
    }

    bool checkparticle(extentity &e)
    {
        if(!isallowed(e)) return false;
        gameentity &f = (gameentity &)e;
        if(f.attrs[12])
        {
            if((f.nextemit -= curtime) <= 0) f.nextemit = 0;
            if(f.nextemit) return false;
            f.nextemit += f.attrs[12];
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

    void drawparticle(gameentity &e, const vec &o, int idx = -1)
    {
        switch(e.type)
        {
            case TELEPORT:
                maketeleport(e, o);
                break;
            case ROUTE:
            {
                if(e.attrs[0] != routeid || (!m_edit(game::gamemode) && !m_speedrun(game::gamemode)) || (game::player1->isediting() && editinhibit)) break;
                loopv(e.links) if(ents.inrange(e.links[i]) && ents[e.links[i]]->type == ROUTE)
                {
                    gameentity &f = *(gameentity *)ents[e.links[i]];
                    if(!routemaxdist || o.dist(f.pos()) <= routemaxdist) continue;
                    int col = 0xFF22FF;
                    char *rcol = indexlist(routecolours, routeid);
                    if(rcol)
                    {
                        if(*rcol) col = parseint(rcol);
                        delete[] rcol;
                    }
                    part_flare(o, f.pos(), 1, PART_LIGHTNING_FLARE, col);
                }

                if(showroutenames && getfirstroute() == idx)
                {
                    char *name = indexlist(routenames, routeid);
                    if(name)
                    {
                        if(*name)
                        {
                            vec above = o;
                            above.z += 6;
                            defformatstring(routename, "<%s>%s", textfontbold, name);
                            part_textcopy(above, routename, PART_TEXT, 1, colourwhite, 2, routenameblend);
                            above.z += 2;
                            part_text(above, "Route", PART_TEXT, 1, colourwhite, 2, routenameblend);
                        }
                        delete[] name;
                    }
                }
            }
            default: break;
        }
    }

    void checkui()
    {
        bool editcheck = game::player1->isediting() && !editinhibit;

        if((editcheck ? entityeditui : entityitemui) >= 0)
        {
            int fstent = editcheck ? 0 : firstuse(EU_ITEM), lstent = editcheck ? ents.length() : lastuse(EU_ITEM);

            for(int i = fstent; i < lstent; ++i)
            {
                gameentity &e = *(gameentity *)ents[i];

                if(e.type == NOTUSED || e.attrs.empty()) continue;
                if(!editcheck && (enttype[e.type].usetype != EU_ITEM || !isallowed(e))) continue;

                vec curpos = vec(editcheck ? e.o : e.pos()).addz(clamp(enttype[e.type].radius / 2, 2, 4));
                if(curpos.squaredist(camera1->o) > (!editcheck || enthover.find(i) >= 0 || entgroup.find(i) >= 0 ? entityitemuimaxdist * entityitemuimaxdist : entityedituimaxdist * entityedituimaxdist))
                    continue;

                if(editcheck) { MAKEUI(entityedit, i, enthover.find(i) >= 0 || entgroup.find(i) >= 0, curpos); }
                else { MAKEUI(entityitem, i, false, curpos); }
            }
        }

        if(editcheck || entityprojui < 0) return;

        loopv(projs::typeprojs[PROJ_ENTITY])
        {
            projent &proj = *projs::typeprojs[PROJ_ENTITY][i];
            if(proj.projtype != PROJ_ENTITY || !ents.inrange(proj.id) || !proj.ready()) continue;

            gameentity &e = *(gameentity *)ents[proj.id];
            if(e.type == NOTUSED || e.attrs.empty() || enttype[e.type].usetype != EU_ITEM || !isallowed(e)) continue;

            vec curpos = vec(proj.o).addz(clamp(enttype[e.type].radius / 2, 2, 4));
            if(curpos.squaredist(camera1->o) > entityitemuimaxdist * entityitemuimaxdist) continue;

            MAKEUI(entityproj, proj.seqid, false, curpos);
        }
    }

    void drawparticles()
    {
        loopv(railways) loopvj(railways[i].parents)
        {
            int n = railways[i].parents[j];
            if(!ents.inrange(n) || ents[n]->type != PARTICLES) continue;

            gameentity &e = *(gameentity *)ents[n];
            if(!checkparticle(e) || e.pos().squaredist(camera1->o) > maxparticledistance * maxparticledistance) continue;

            makeparticle(e.pos(), e.attrs);
        }

        bool hasroute = (m_edit(game::gamemode) || m_speedrun(game::gamemode)) && routeid >= 0,
             editcheck = entityicons && game::player1->isediting() && !editinhibit;
        int fstent = m_edit(game::gamemode) ? 0 : min(firstuse(EU_ITEM), firstent(hasroute ? ROUTE : TELEPORT)),
            lstent = m_edit(game::gamemode) ? ents.length() : max(lastuse(EU_ITEM), lastent(hasroute ? ROUTE : TELEPORT));

        for(int i = fstent; i < lstent; ++i)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.type == NOTUSED || e.attrs.empty()) continue;
            if(!editcheck && e.type != TELEPORT && e.type != ROUTE && enttype[e.type].usetype != EU_ITEM) continue; // they don't do anything

            vec pos = editcheck ? e.o : e.pos();
            bool hassel = enthover.find(i) >= 0 || entgroup.find(i) >= 0;
            float dist = pos.squaredist(camera1->o);
            if(editcheck && (hassel || dist <= entityiconmaxdist * entityiconmaxdist))
            {
                bool ontop = hassel && dist <= entityshowmaxdist * entityshowmaxdist,
                     cansee = getvisible(camera1->o, camera1->yaw, camera1->pitch, pos, curfov, fovy, max(enttype[e.type].radius, 4), ontop ? -1 : VFC_PART_VISIBLE),
                     dotop = ontop && e.dynamic(), visiblepos = dotop && getvisible(camera1->o, camera1->yaw, camera1->pitch, e.pos(), curfov, fovy, max(enttype[e.type].radius, 4), ontop ? -1 : VFC_PART_VISIBLE);

                Texture *tex = textureload(getenttex(i), 3);
                loopj(dotop ? 2 : 1) if(j ? visiblepos : cansee)
                {
                    if(j && (cansee || visiblepos)) part_line(pos, e.pos(), entselsize, 1, 1, entselcolourdyn);
                    if(tex && tex != notexture)
                    {
                        if(ontop) part_icon_ontop(j ? e.pos() : pos, tex, !j ? entselsizetop : entselsize, !j ? entselblendtop : entselblend, 0, 0, 1, j ? entselcolourdyn : entselcolourtop);
                        else part_icon(j ? e.pos() : pos, tex, entselsize, entselblend, 0, 0, 1, j ? entselcolourdyn : entselcolour);
                    }
                    else part_create(ontop ? PART_ENTITY_ONTOP : PART_ENTITY, 1, j ? e.pos() : pos, j ? entselcolourdyn : (ontop ? entselcolourtop : entselcolour), ontop && !j ? entselsizetop : entselsize, ontop && !j ? entselblendtop : entselblend);
                }
            }

            drawparticle(e, pos, i);
        }

        loopv(projs::typeprojs[PROJ_ENTITY])
        {
            projent &proj = *projs::typeprojs[PROJ_ENTITY][i];
            if(proj.projtype != PROJ_ENTITY || !ents.inrange(proj.id) || !proj.ready()) continue;

            gameentity &e = *(gameentity *)ents[proj.id];
            if(e.type == NOTUSED || e.attrs.empty()) continue;

            #if 0 // legacy but may be useful in future
            float skew = 1, dist = proj.o.squaredist(camera1->o);
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
            #endif

            drawparticle(e, proj.o);
        }
    }

    bool getcamera(vec &pos, float &yaw, float &pitch, float &fov)
    {
        fov = 90;
        if(ents.empty()) return false;

        vector<int> cameras;
        cameras.setsize(0);

        loopk(3)
        {
            loopv(ents)
            {
                gameentity &e = *(gameentity *)ents[i];
                if(e.flags&EF_VIRTUAL) continue;
                if(k >= 2 ? e.type != PLAYERSTART : (e.type != CAMERA || (!k && (e.attrs[0] != CAMERA_MAPSHOT || !isallowed(e))))) continue;
                cameras.add(i);
            }
            if(!cameras.empty()) break;
        }

        if(cameras.empty()) return false;

        int cam = rnd(cameras.length());
        if(!cameras.inrange(cam)) return false;

        gameentity &e = *(gameentity *)ents[cameras[cam]];

        pos = e.pos();
        yaw = e.attrs[e.type == PLAYERSTART ? 1 : 2] + e.yaw;
        pitch = e.attrs[e.type == PLAYERSTART ? 2 : 3] + e.pitch;
        if(e.type == CAMERA && e.attrs[11] > 0) fov = e.attrs[11];

        fixrange(yaw, pitch);

        return true;
    }
}
