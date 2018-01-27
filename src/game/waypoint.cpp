#include "game.h"

namespace ai
{
    using namespace game;

    vector<waypoint> waypoints;
    vector<oldwaypoint> oldwaypoints;

    bool clipped(const vec &o)
    {
        int material = lookupmaterial(o), clipmat = material&MATF_CLIP;
        return clipmat == MAT_CLIP || clipmat == MAT_AICLIP || material&MAT_DEATH || material&MAT_HURT || (material&MATF_VOLUME) == MAT_LAVA;
    }

    int getpull(const vec &o)
    {
        vec pos = o; pos.z += JUMPMIN;
        if(!insideworld(vec(pos.x, pos.y, min(pos.z, worldsize - 1e-3f)))) return -2;
        float dist = raycube(pos, vec(0, 0, -1), 0, RAY_CLIPMAT);
        int posmat = lookupmaterial(pos), pull = 1;
        if(isliquid(posmat&MATF_VOLUME)) pull *= 5;
        if(dist >= 0)
        {
            pull = int(dist/JUMPMIN);
            pos.z -= clamp(dist-8.0f, 0.0f, pos.z);
            int trgmat = lookupmaterial(pos);
            if(trgmat&MAT_DEATH || trgmat&MAT_HURT || (trgmat&MATF_VOLUME) == MAT_LAVA) pull *= 100;
            else if(isliquid(trgmat&MATF_VOLUME)) pull *= 2;
        }
        return pull;
    }

    enum
    {
        WPCACHE_STATIC = 0,
        WPCACHE_DYNAMIC,
        NUMWPCACHES
    };

    struct wpcache
    {
        struct node
        {
            float split[2];
            uint child[2];

            int axis() const { return child[0]>>30; }
            int childindex(int which) const { return child[which]&0x3FFFFFFF; }
            bool isleaf(int which) const { return (child[1]&(1<<(30+which)))!=0; }
        };

        vector<node> nodes;
        int firstwp, lastwp;
        vec bbmin, bbmax;

        wpcache() { clear(); }

        void clear()
        {
            nodes.setsize(0);
            firstwp = lastwp = -1;
            bbmin = vec(1e16f, 1e16f, 1e16f);
            bbmax = vec(-1e16f, -1e16f, -1e16f);
        }

        void build(int first = 0, int last = -1)
        {
            if(last < 0) last = waypoints.length();
            vector<int> indices;
            for(int i = first; i < last; i++)
            {
                waypoint &w = waypoints[i];
                indices.add(i);
                if(firstwp < 0) firstwp = i;
                float radius = WAYPOINTRADIUS;
                bbmin.min(vec(w.o).sub(radius));
                bbmax.max(vec(w.o).add(radius));
            }
            if(first < last) lastwp = max(lastwp, last-1);
            if(indices.length())
            {
                nodes.reserve(indices.length());
                build(indices.getbuf(), indices.length(), bbmin, bbmax);
            }
        }

        void build(int *indices, int numindices, const vec &vmin, const vec &vmax)
        {
            int axis = 2;
            loopk(2) if(vmax[k] - vmin[k] > vmax[axis] - vmin[axis]) axis = k;

            vec leftmin(1e16f, 1e16f, 1e16f), leftmax(-1e16f, -1e16f, -1e16f), rightmin(1e16f, 1e16f, 1e16f), rightmax(-1e16f, -1e16f, -1e16f);
            float split = 0.5f*(vmax[axis] + vmin[axis]), splitleft = -1e16f, splitright = 1e16f;
            int left, right;
            for(left = 0, right = numindices; left < right;)
            {
                waypoint &w = waypoints[indices[left]];
                float radius = WAYPOINTRADIUS;
                if(max(split - (w.o[axis]-radius), 0.0f) > max((w.o[axis]+radius) - split, 0.0f))
                {
                    ++left;
                    splitleft = max(splitleft, w.o[axis]+radius);
                    leftmin.min(vec(w.o).sub(radius));
                    leftmax.max(vec(w.o).add(radius));
                }
                else
                {
                    --right;
                    swap(indices[left], indices[right]);
                    splitright = min(splitright, w.o[axis]-radius);
                    rightmin.min(vec(w.o).sub(radius));
                    rightmax.max(vec(w.o).add(radius));
                }
            }

            if(!left || right == numindices)
            {
                leftmin = rightmin = vec(1e16f, 1e16f, 1e16f);
                leftmax = rightmax = vec(-1e16f, -1e16f, -1e16f);
                left = right = numindices/2;
                splitleft = -1e16f;
                splitright = 1e16f;
                loopi(numindices)
                {
                    waypoint &w = waypoints[indices[i]];
                    float radius = WAYPOINTRADIUS;
                    if(i < left)
                    {
                        splitleft = max(splitleft, w.o[axis]+radius);
                        leftmin.min(vec(w.o).sub(radius));
                        leftmax.max(vec(w.o).add(radius));
                    }
                    else
                    {
                        splitright = min(splitright, w.o[axis]-radius);
                        rightmin.min(vec(w.o).sub(radius));
                        rightmax.max(vec(w.o).add(radius));
                    }
                }
            }

            int offset = nodes.length();
            node &curnode = nodes.add();
            curnode.split[0] = splitleft;
            curnode.split[1] = splitright;

            if(left <= 1) curnode.child[0] = (axis<<30) | (left > 0 ? indices[0] : 0x3FFFFFFF);
            else
            {
                curnode.child[0] = (axis<<30) | (nodes.length()-offset);
                if(left) build(indices, left, leftmin, leftmax);
            }

            if(numindices-right <= 1) curnode.child[1] = (1<<31) | (left <= 1 ? 1<<30 : 0) | (numindices-right > 0 ? indices[right] : 0x3FFFFFFF);
            else
            {
                curnode.child[1] = (left <= 1 ? 1<<30 : 0) | (nodes.length()-offset);
                if(numindices-right) build(&indices[right], numindices-right, rightmin, rightmax);
            }
        }
    } wpcaches[NUMWPCACHES];

    static int invalidatedwpcaches = 0, clearedwpcaches = (1<<NUMWPCACHES)-1, numinvalidatewpcaches = 0, lastwpcache = 0;

    static inline void invalidatewpcache(int wp)
    {
        if(++numinvalidatewpcaches >= 1000) { numinvalidatewpcaches = 0; invalidatedwpcaches = (1<<NUMWPCACHES)-1; }
        else
        {
            loopi(WPCACHE_DYNAMIC) if(wp >= wpcaches[i].firstwp && wp <= wpcaches[i].lastwp) { invalidatedwpcaches |= 1<<i; return; }
            invalidatedwpcaches |= 1<<WPCACHE_DYNAMIC;
        }
    }

    void clearwpcache(bool full = true)
    {
        loopi(NUMWPCACHES) if(full || invalidatedwpcaches&(1<<i)) { wpcaches[i].clear(); clearedwpcaches |= 1<<i; }
        if(full || invalidatedwpcaches == (1<<NUMWPCACHES)-1)
        {
            numinvalidatewpcaches = 0;
            lastwpcache = 0;
        }
        invalidatedwpcaches = 0;
    }
    ICOMMAND(0, clearwpcache, "", (), clearwpcache());

    void buildwpcache()
    {
        loopi(NUMWPCACHES) if(wpcaches[i].firstwp < 0)
            wpcaches[i].build(i > 0 ? wpcaches[i-1].lastwp+1 : 1, i+1 >= NUMWPCACHES || wpcaches[i+1].firstwp < 0 ? -1 : wpcaches[i+1].firstwp);
        clearedwpcaches = 0;
        lastwpcache = waypoints.length();

        wpavoid.clear();
        loopv(waypoints) if(waypoints[i].weight() < 0) wpavoid.avoidnear(NULL, waypoints[i].o.z + WAYPOINTRADIUS, waypoints[i].o, WAYPOINTRADIUS);
    }

    struct wpcachestack
    {
        wpcache::node *node;
        float tmin, tmax;
    };

    vector<wpcache::node *> wpcachestack;

    int closestwaypoint(const vec &pos, float mindist, bool links)
    {
        if(waypoints.empty()) return -1;
        if(clearedwpcaches) buildwpcache();

        #define CHECKCLOSEST(index) do { \
            int n = (index); \
            if(n < waypoints.length()) \
            { \
                waypoint &w = waypoints[n]; \
                if(!links || w.haslinks()) \
                { \
                    float dist = w.o.squaredist(pos); \
                    if(dist < mindist*mindist) { closest = n; mindist = sqrtf(dist); } \
                } \
            } \
        } while(0)
        int closest = -1;
        wpcache::node *curnode;
        loop(which, NUMWPCACHES) if(wpcaches[which].firstwp >= 0) for(curnode = &wpcaches[which].nodes[0], wpcachestack.setsize(0);;)
        {
            int axis = curnode->axis();
            float dist1 = pos[axis] - curnode->split[0], dist2 = curnode->split[1] - pos[axis];
            if(dist1 >= mindist)
            {
                if(dist2 < mindist)
                {
                    if(!curnode->isleaf(1)) { curnode += curnode->childindex(1); continue; }
                    CHECKCLOSEST(curnode->childindex(1));
                }
            }
            else if(curnode->isleaf(0))
            {
                CHECKCLOSEST(curnode->childindex(0));
                if(dist2 < mindist)
                {
                    if(!curnode->isleaf(1)) { curnode += curnode->childindex(1); continue; }
                    CHECKCLOSEST(curnode->childindex(1));
                }
            }
            else
            {
                if(dist2 < mindist)
                {
                    if(!curnode->isleaf(1)) wpcachestack.add(curnode + curnode->childindex(1));
                    else CHECKCLOSEST(curnode->childindex(1));
                }
                curnode += curnode->childindex(0);
                continue;
            }
            if(wpcachestack.empty()) break;
            curnode = wpcachestack.pop();
        }
        for(int i = lastwpcache; i < waypoints.length(); i++) { CHECKCLOSEST(i); }
        return closest;
    }

    void findwaypointswithin(const vec &pos, float mindist, float maxdist, vector<int> &results)
    {
        if(waypoints.empty()) return;
        if(clearedwpcaches) buildwpcache();

        float mindist2 = mindist*mindist, maxdist2 = maxdist*maxdist;
        #define CHECKWITHIN(index) do { \
            int n = (index); \
            if(n < waypoints.length()) \
            { \
                const waypoint &w = waypoints[n]; \
                float dist = w.o.squaredist(pos); \
                if(dist > mindist2 && dist < maxdist2) results.add(n); \
            } \
        } while(0)
        wpcache::node *curnode;
        loop(which, NUMWPCACHES) if(wpcaches[which].firstwp >= 0) for(curnode = &wpcaches[which].nodes[0], wpcachestack.setsize(0);;)
        {
            int axis = curnode->axis();
            float dist1 = pos[axis] - curnode->split[0], dist2 = curnode->split[1] - pos[axis];
            if(dist1 >= maxdist)
            {
                if(dist2 < maxdist)
                {
                    if(!curnode->isleaf(1)) { curnode += curnode->childindex(1); continue; }
                    CHECKWITHIN(curnode->childindex(1));
                }
            }
            else if(curnode->isleaf(0))
            {
                CHECKWITHIN(curnode->childindex(0));
                if(dist2 < maxdist)
                {
                    if(!curnode->isleaf(1)) { curnode += curnode->childindex(1); continue; }
                    CHECKWITHIN(curnode->childindex(1));
                }
            }
            else
            {
                if(dist2 < maxdist)
                {
                    if(!curnode->isleaf(1)) wpcachestack.add(curnode + curnode->childindex(1));
                    else CHECKWITHIN(curnode->childindex(1));
                }
                curnode += curnode->childindex(0);
                continue;
            }
            if(wpcachestack.empty()) break;
            curnode = wpcachestack.pop();
        }
        for(int i = lastwpcache; i < waypoints.length(); i++) { CHECKWITHIN(i); }
    }

    void avoidset::avoidnear(void *owner, float above, const vec &pos, float limit)
    {
        if(ai::waypoints.empty()) return;
        if(clearedwpcaches) buildwpcache();

        float limit2 = limit*limit;
        #define CHECKNEAR(index) do { \
            int n = (index); \
            if(n < ai::waypoints.length()) \
            { \
                const waypoint &w = ai::waypoints[n]; \
                if(w.o.squaredist(pos) < limit2) add(owner, above, n); \
            } \
        } while(0)
        wpcache::node *curnode;
        loop(which, NUMWPCACHES) if(wpcaches[which].firstwp >= 0) for(curnode = &wpcaches[which].nodes[0], wpcachestack.setsize(0);;)
        {
            int axis = curnode->axis();
            float dist1 = pos[axis] - curnode->split[0], dist2 = curnode->split[1] - pos[axis];
            if(dist1 >= limit)
            {
                if(dist2 < limit)
                {
                    if(!curnode->isleaf(1)) { curnode += curnode->childindex(1); continue; }
                    CHECKNEAR(curnode->childindex(1));
                }
            }
            else if(curnode->isleaf(0))
            {
                CHECKNEAR(curnode->childindex(0));
                if(dist2 < limit)
                {
                    if(!curnode->isleaf(1)) { curnode += curnode->childindex(1); continue; }
                    CHECKNEAR(curnode->childindex(1));
                }
            }
            else
            {
                if(dist2 < limit)
                {
                    if(!curnode->isleaf(1)) wpcachestack.add(curnode + curnode->childindex(1));
                    else CHECKNEAR(curnode->childindex(1));
                }
                curnode += curnode->childindex(0);
                continue;
            }
            if(wpcachestack.empty()) break;
            curnode = wpcachestack.pop();
        }
        for(int i = lastwpcache; i < ai::waypoints.length(); i++) { CHECKNEAR(i); }
    }

    int avoidset::remap(gameent *d, int n, vec &pos, bool retry)
    {
        if(obstacles.empty()) return n;
        int cur = 0;
        loopv(obstacles)
        {
            obstacle &ob = obstacles[i];
            int next = cur + ob.numwaypoints;
            if(ob.owner != d)
            {
                for(; cur < next; cur++) if(waypoints[cur] == n)
                {
                    if(ob.above < 0) return retry ? n : -1;
                    vec above(pos.x, pos.y, ob.above);
                    if(above.z-d->o.z >= ai::JUMPMAX)
                        return retry ? n : -1; // too much scotty
                    int node = closestwaypoint(above, ai::CLOSEDIST, true);
                    if(ai::iswaypoint(node) && node != n)
                    { // try to reroute above their head?
                        if(!find(node, d))
                        {
                            pos = ai::waypoints[node].o;
                            return node;
                        }
                        else return retry ? n : -1;
                    }
                    else
                    {
                        vec old = d->o;
                        d->o = vec(above).add(vec(0, 0, d->height));
                        bool col = collide(d, vec(0, 0, 1));
                        d->o = old;
                        if(!col)
                        {
                            pos = above;
                            return n;
                        }
                        else return retry ? n : -1;
                    }
                }
            }
            cur = next;
        }
        return n;
    }

    static inline float heapscore(waypoint *q) { return q->score(); }

    bool route(gameent *d, int node, int goal, vector<int> &route, const avoidset &obstacles, int retries)
    {
        if(waypoints.empty() || !iswaypoint(node) || !iswaypoint(goal) || goal == node || !waypoints[node].haslinks())
            return false;

        static ushort routeid = 1;
        static vector<waypoint *> queue;

        if(!routeid)
        {
            loopv(waypoints) waypoints[i].route = 0;
            routeid = 1;
        }

        if(d)
        {
            if(retries <= 1 && d->ai) loopi(NUMPREVNODES) if(d->ai->prevnodes[i] != node && iswaypoint(d->ai->prevnodes[i]))
            {
                waypoints[d->ai->prevnodes[i]].route = routeid;
                waypoints[d->ai->prevnodes[i]].curscore = -1;
                waypoints[d->ai->prevnodes[i]].estscore = 0;
            }
            if(retries <= 0)
            {
                loopavoid(obstacles, d,
                {
                    if(iswaypoint(wp) && wp != node && wp != goal && waypoints[node].find(wp) < 0 && waypoints[goal].find(wp) < 0)
                    {
                        waypoints[wp].route = routeid;
                        waypoints[wp].curscore = -1;
                        waypoints[wp].estscore = 0;
                    }
                });
            }
        }

        waypoints[node].route = routeid;
        waypoints[node].curscore = waypoints[node].estscore = 0;
        waypoints[node].prev = 0;
        queue.setsize(0);
        queue.add(&waypoints[node]);
        route.setsize(0);

        int lowest = -1;
        while(!queue.empty())
        {
            waypoint &m = *queue.removeheap();
            float prevscore = m.curscore;
            m.curscore = -1;
            loopi(MAXWAYPOINTLINKS)
            {
                int link = m.links[i];
                if(!link) break;
                if(iswaypoint(link) && (link == node || link == goal || waypoints[link].haslinks()))
                {
                    waypoint &n = waypoints[link];
                    int weight = max(n.weight(), 1);
                    float curscore = prevscore + n.o.dist(m.o)*weight;
                    if(n.route == routeid && curscore >= n.curscore) continue;
                    n.curscore = curscore;
                    n.prev = ushort(&m - &waypoints[0]);
                    if(n.route != routeid)
                    {
                        n.estscore = n.o.dist(waypoints[goal].o)*weight;
                        if(n.estscore <= WAYPOINTRADIUS*4 && (lowest < 0 || n.estscore <= waypoints[lowest].estscore))
                            lowest = link;
                        n.route = routeid;
                        if(link == goal) goto foundgoal;
                        queue.addheap(&n);
                    }
                    else loopvj(queue) if(queue[j] == &n) { queue.upheap(j); break; }
                }
            }
        }
        foundgoal:

        routeid++;

        if(lowest >= 0) // otherwise nothing got there
        {
            for(waypoint *m = &waypoints[lowest]; m > &waypoints[0]; m = &waypoints[m->prev])
                route.add(m - &waypoints[0]); // just keep it stored backward
        }

        return !route.empty();
    }

    string loadedwaypoints = "";
    VARF(0, dropwaypoints, 0, 0, 1, if(dropwaypoints) getwaypoints());

    int addwaypoint(const vec &o, int pull = -1)
    {
        if(waypoints.length() > MAXWAYPOINTS) return -1;
        int n = waypoints.length();
        waypoints.add(waypoint(o, pull >= 0 ? pull : getpull(o)));
        invalidatewpcache(n);
        return n;
    }

    void linkwaypoint(waypoint &a, int n)
    {
        loopi(MAXWAYPOINTLINKS)
        {
            if(a.links[i] == n) return;
            if(!a.links[i]) { a.links[i] = n; return; }
        }
        a.links[rnd(MAXWAYPOINTLINKS)] = n;
    }

    static inline bool shouldnavigate()
    {
        if(dropwaypoints) return true;
        loopvrev(players) if(players[i] && players[i]->actortype != A_PLAYER) return true;
        return false;
    }

    static inline bool shoulddrop(gameent *d)
    {
        return !d->ai && (dropwaypoints || !loadedwaypoints[0]);
    }

    void inferwaypoints(gameent *d, const vec &o, const vec &v, float mindist)
    {
        if(!shouldnavigate()) return;
        if(!shoulddrop(d) || clipped(o) || clipped(v))
        {
            d->lastnode = closestwaypoint(v, CLOSEDIST, false);
            return;
        }
        int from = closestwaypoint(o, mindist, false), to = closestwaypoint(v, mindist, false);
        if(!iswaypoint(from)) from = addwaypoint(o);
        if(!iswaypoint(to)) to = addwaypoint(v);
        if(d->lastnode != from && iswaypoint(d->lastnode) && iswaypoint(from))
            linkwaypoint(waypoints[d->lastnode], from);
        if(iswaypoint(to))
        {
            if(from != to && iswaypoint(from) && iswaypoint(to))
                linkwaypoint(waypoints[from], to);
            d->lastnode = to;
        }
    }

    void navigate(gameent *d)
    {
        if(d->state != CS_ALIVE) { d->lastnode = -1; return; }
        vec v(d->feetpos());
        bool dropping = shoulddrop(d) && !clipped(v);
        float dist = dropping ? WAYPOINTRADIUS : CLOSEDIST;
        int curnode = closestwaypoint(v, dist, false), prevnode = d->lastnode;
        if(!iswaypoint(curnode) && dropping) curnode = addwaypoint(v);
        if(iswaypoint(curnode))
        {
            if(dropping && d->lastnode != curnode && iswaypoint(d->lastnode))
            {
                linkwaypoint(waypoints[d->lastnode], curnode);
                if(!d->airmillis) linkwaypoint(waypoints[curnode], d->lastnode);
            }
            d->lastnode = curnode;
            waypoints[d->lastnode].drag += curtime;
            if(d->ai && iswaypoint(prevnode) && d->lastnode != prevnode) d->ai->addprevnode(prevnode);
        }
        else if(!iswaypoint(d->lastnode) || waypoints[d->lastnode].o.squaredist(v) > dist*dist)
        {
            dist = RETRYDIST; // workaround
            d->lastnode = closestwaypoint(v, dist, false);
        }
    }

    void navigate()
    {
        if(shouldnavigate())
        {
            navigate(game::player1);
            loopv(players) if(players[i]) navigate(players[i]);
        }
        if(invalidatedwpcaches) clearwpcache(false);
    }

    void clearwaypoints(bool full)
    {
        waypoints.setsize(0);
        clearwpcache();
        if(full) loadedwaypoints[0] = '\0';
    }
    ICOMMAND(0, clearwaypoints, "", (), clearwaypoints(true));

    void remapwaypoints()
    {
        vector<ushort> remap;
        int total = 0;
        loopv(waypoints) remap.add(waypoints[i].links[1] == 0xFFFF ? 0 : total++);
        total = 0;
        loopvj(waypoints)
        {
            if(waypoints[j].links[1] == 0xFFFF) continue;
            waypoint &w = waypoints[total];
            if(j != total) w = waypoints[j];
            int k = 0;
            loopi(MAXWAYPOINTLINKS)
            {
                int link = w.links[i];
                if(!link) break;
                if((w.links[k] = remap[link])) k++;
            }
            if(k < MAXWAYPOINTLINKS) w.links[k] = 0;
            total++;
        }
        waypoints.setsize(total);
    }

    bool checkteleport(const vec &o, const vec &v)
    {
        if(o.dist(v) <= CLOSEDIST) return true;
        loopenti(TELEPORT) if(entities::ents[i]->type == TELEPORT)
        {
            gameentity &e = *(gameentity *)entities::ents[i];
            if(o.dist(e.o) > (e.attrs[3] ? e.attrs[3] : enttype[e.type].radius)+CLOSEDIST) continue;
            loopvj(e.links) if(entities::ents.inrange(e.links[j]) && entities::ents[e.links[j]]->type == TELEPORT)
            {
                gameentity &f = *(gameentity *)entities::ents[e.links[j]];
                if(v.dist(f.o) > (f.attrs[3] ? f.attrs[3] : enttype[f.type].radius)+CLOSEDIST) continue;
                return true;
            }
        }
        return false;
    }

    bool cleanwaypoints()
    {
        int cleared = 0;
        for(int i = 1; i < waypoints.length(); i++)
        {
            waypoint &w = waypoints[i];
            if(clipped(w.o))
            {
                w.links[0] = 0;
                w.links[1] = 0xFFFF;
                cleared++;
            }
            else loopk(MAXWAYPOINTLINKS)
            {
                int link = w.links[k];
                if(!link) continue;
                waypoint &v = waypoints[link];
                if(!checkteleport(w.o, v.o))
                {
                    int highest = MAXWAYPOINTLINKS-1;
                    loopj(MAXWAYPOINTLINKS) if(!w.links[j]) { highest = j-1; break; }
                    w.links[k] = w.links[highest];
                    w.links[highest] = 0;
                    k--;
                }
            }
            //if(!w.haslinks()) conoutf("Warning: waypoint %d has no links after import", i);

        }
        if(cleared)
        {
            player1->lastnode = -1;
            loopv(players) if(players[i]) players[i]->lastnode = -1;
            remapwaypoints();
            clearwpcache();
            return true;
        }
        return false;
    }

    bool getwaypointfile(const char *mname, char *wptname)
    {
        if(!mname || !*mname) mname = mapname;
        if(!*mname) return false;
        nformatstring(wptname, MAXSTRLEN, "%s.wpt", mname);
        path(wptname);
        return true;
    }

    bool loadwaypoints(bool force, const char *mname)
    {
        string wptname;
        if(!getwaypointfile(mname, wptname)) return false;
        if(!force && (waypoints.length() || !strcmp(loadedwaypoints, wptname))) return true;

        stream *f = opengzfile(wptname, "rb");
        if(!f) return false;
        char magic[4];
        if(f->read(magic, 4) < 4 || memcmp(magic, "OWPT", 4)) { delete f; return false; }

        copystring(loadedwaypoints, wptname);

        waypoints.setsize(0);
        waypoints.add(vec(0, 0, 0));
        ushort numwp = f->getlil<ushort>();
        loopi(numwp)
        {
            if(f->end()) break;
            vec o;
            o.x = f->getlil<float>();
            o.y = f->getlil<float>();
            o.z = f->getlil<float>();
            waypoint &w = waypoints.add(waypoint(o, getpull(o)));
            int numlinks = f->getchar(), k = 0;
            loopj(numlinks) if((w.links[k] = f->getlil<ushort>()) != 0) if(++k >= MAXWAYPOINTLINKS) break;
            //if(!w.haslinks()) conoutf("Warning: waypoint %d has no links", i);
        }

        delete f;
        conoutf("Loaded %d waypoints from %s", numwp, wptname);

        if(!cleanwaypoints()) clearwpcache();
        game::specreset();
        return true;
    }
    ICOMMAND(0, loadwaypoints, "s", (char *mname), if(!(identflags&IDF_WORLD)) getwaypoints(true, mname));

    void savewaypoints(bool force, const char *mname)
    {
        if((!dropwaypoints && !force) || waypoints.empty()) return;

        string wptname;
        if(!getwaypointfile(mname, wptname)) return;

        stream *f = opengzfile(wptname, "wb");
        if(!f) return;
        f->write("OWPT", 4);
        f->putlil<ushort>(waypoints.length()-1);
        for(int i = 1; i < waypoints.length(); i++)
        {
            waypoint &w = waypoints[i];
            f->putlil<float>(w.o.x);
            f->putlil<float>(w.o.y);
            f->putlil<float>(w.o.z);
            int numlinks = 0;
            loopj(MAXWAYPOINTLINKS) { if(!w.links[j]) break; numlinks++; }
            f->putchar(numlinks);
            loopj(numlinks) f->putlil<ushort>(w.links[j]);
        }

        delete f;
        conoutf("Saved %d waypoints to %s", waypoints.length()-1, wptname);
    }

    ICOMMAND(0, savewaypoints, "s", (char *mname), if(!(identflags&IDF_WORLD)) savewaypoints(true, mname));

    bool importwaypoints()
    {
        if(oldwaypoints.empty()) return false;
        string wptname;
        if(getwaypointfile(mapname, wptname)) copystring(loadedwaypoints, wptname);
        waypoints.setsize(0);
        waypoints.add(vec(0, 0, 0));
        loopv(oldwaypoints)
        {
            oldwaypoint &v = oldwaypoints[i];
            loopvj(v.links) loopvk(oldwaypoints) if(v.links[j] == oldwaypoints[k].ent)
            {
                v.links[j] = k+1;
                break;
            }
            waypoint &w = waypoints.add(waypoint(v.o, getpull(v.o)));
            int k = 0;
            loopvj(v.links) if((w.links[k] = v.links[j]) != 0) if(++k >= MAXWAYPOINTLINKS) break;
            //if(!w.haslinks()) conoutf("Warning: imported waypoint %d has no links", i);
        }
        conoutf("Imported %d waypoints from the map file", oldwaypoints.length());
        oldwaypoints.setsize(0);
        if(!cleanwaypoints()) clearwpcache();
        return true;
    }

    bool getwaypoints(bool force, const char *mname, bool check)
    {
        //if(check && (client::demoplayback || loadedwaypoints[0])) return false;
        if(check && loadedwaypoints[0]) return false;
        return loadwaypoints(force, mname) || importwaypoints();
    }

    void delwaypoint(int n)
    {
        if(n < 0)
        {
            if(noedit(true)) return;
            n = closestwaypoint(camera1->o);
        }
        if(!iswaypoint(n)) return;
        waypoints[n].links[0] = 0;
        waypoints[n].links[1] = 0xFFFF;
        remapwaypoints();
        clearwpcache();
    }
    ICOMMAND(0, delwaypoint, "b", (int *n), delwaypoint(*n));


    void delselwaypoints()
    {
        if(noedit(true)) return;
        vec o = vec(sel.o).sub(0.1f), s = vec(sel.s).mul(sel.grid).add(o).add(0.1f);
        int cleared = 0;
        for(int i = 1; i < waypoints.length(); i++)
        {
            waypoint &w = waypoints[i];
            if(w.o.x >= o.x && w.o.x <= s.x && w.o.y >= o.y && w.o.y <= s.y && w.o.z >= o.z && w.o.z <= s.z)
            {
                w.links[0] = 0;
                w.links[1] = 0xFFFF;
                cleared++;
            }
        }
        if(cleared)
        {
            player1->lastnode = -1;
            remapwaypoints();
            clearwpcache();
        }
    }
    COMMAND(0, delselwaypoints, "");

    void moveselwaypoints(vec &offset)
    {
        if(noedit(true)) return;
        vec o = vec(sel.o).sub(0.1f), s = vec(sel.s).mul(sel.grid).add(o).add(0.1f);
        int moved = 0;
        for(int i = 1; i < waypoints.length(); i++)
        {
            waypoint &w = waypoints[i];
            if(w.o.x >= o.x && w.o.x <= s.x && w.o.y >= o.y && w.o.y <= s.y && w.o.z >= o.z && w.o.z <= s.z)
            {
                w.o.add(offset);
                moved++;
            }
        }
        if(moved)
        {
            player1->lastnode = -1;
            clearwpcache();
        }
    }
    ICOMMAND(0, moveselwaypoints, "fff", (float *x, float *y, float *z), vec v(*x, *y, *z); moveselwaypoints(v));
}
