#include "game.h"

namespace ai
{
    using namespace game;

    int waypointversion = -1;
    vector<waypoint> waypoints;
    vector<oldwaypoint> oldwaypoints;

    bool clipped(const vec &o, bool full)
    {
        if(!insideworld(o)) return true;
        int material = lookupmaterial(o), clipmat = material&MATF_CLIP;
        return (full && (clipmat == MAT_CLIP || clipmat == MAT_AICLIP)) || material&MAT_DEATH || material&MAT_HURT || (material&MATF_VOLUME) == MAT_LAVA;
    }

    int getpull(const vec &o)
    {
        vec pos = vec(o).addz(1);
        if(!insideworld(vec(pos.x, pos.y, min(pos.z, worldsize - 1e-3f)))) return -2;
        float dist = raycube(pos, vec(0, 0, -1), 0, RAY_CLIPMAT);
        int pull = 1;
        if(dist >= 0)
        {
            pull = int(dist / JUMPMIN);
            pos.z -= clamp(dist - WAYPOINTRADIUS, 0.0f, pos.z);
            int trgmat = lookupmaterial(pos);
            if(trgmat&MAT_DEATH || trgmat&MAT_HURT) pull *= 100;
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

                    if(!physics::movepitch(d, true) && above.z - d->o.z >= ai::JUMPMAX)
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
                        d->o = above;
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
    VAR(IDF_PERSIST, explodewaypoints, 0, 1, 3);
    VAR(IDF_PERSIST, explodewaypointsup, 0, 0, VAR_MAX);
    VAR(IDF_PERSIST, explodewaypointsdown, 0, 1, VAR_MAX);
    VAR(IDF_PERSIST, explodewaypointsaround, 0, 1, VAR_MAX);
    VAR(IDF_PERSIST, explodewaypointsmax, 0, MAXWAYPOINTFILL / 2, MAXWAYPOINTFILL);

    static const vec recursedirs[6] = {
        vec(1, 0, 0), vec(-1, 0, 0), vec(0, 1, 0), vec(0, -1, 0), vec(0, 0, -1), vec(0, 0, 1)
    };

    int recursewaypoint(int n, int dirs = 5, bool linkup = false, bool saved = true, int m = -1)
    {
        if(m < 0) m = explodewaypointsmax;
        if(waypoints.length() >= m) return 0;

        if(clipped(waypoints[n].o)) return 0;

        int created = 0;
        vec wpos = vec(waypoints[n].o).addz(1);
        dirs = clamp(dirs, 1, 6);
        loopi(dirs)
        {
            if(waypoints.length() >= m) break;

            float dist = raycube(wpos, recursedirs[i], 0, RAY_CLIPMAT),
                  dradius = i >= 4 ? WAYPOINTRADIUS * 2 : WAYPOINTRADIUS;

            if(dist >= dradius)
            {
                vec o;
                int posmat = 0;
                bool bail = false;
                loopj(3)
                {
                    float jradius = dradius;
                    bool fullclip = false;
                    switch(j)
                    {
                        case 0: jradius = dist + GUARDRADIUS; break;
                        case 1: jradius = dist - GUARDRADIUS; break;
                        case 2: jradius = min(dradius, dist); fullclip = true; break;
                    }

                    o = vec(waypoints[n].o).add(vec(recursedirs[i]).mul(jradius));
                    if(clipped(o, fullclip)) { bail = true; break; }
                }
                if(bail) continue;

                int close = closestwaypoint(o, WAYPOINTRADIUS, false);
                if(iswaypoint(close))
                {
                    if(i >= 5 && !linkup) continue;

                    waypoint &c = waypoints[close];
                    vec cpos = vec(c.o).addz(1);

                    float cdist = raycube(cpos, vec(cpos).sub(wpos).safenormalize(), 0, RAY_CLIPMAT);
                    if(cdist < WAYPOINTRADIUS) continue;

                    loopj(MAXWAYPOINTLINKS)
                    {
                        if(c.links[j] == n) continue;
                        if(!c.links[j]) { c.links[j] = n; break; }
                    }

                    loopj(MAXWAYPOINTLINKS)
                    {
                        if(waypoints[n].links[j] == close) continue;
                        if(!waypoints[n].links[j]) { waypoints[n].links[j] = close; break; }
                    }

                    continue;
                }

                int pull = 1;
                if(isliquid(posmat&MATF_VOLUME)) pull *= 5;
                pull = int(dist / JUMPMIN);

                int r = waypoints.length();
                waypoint &a = waypoints.add(waypoint(o, pull, WAYPOINTVERSION, saved));
                if(waypoints.length() <= r) break;

                vec apos = vec(a.o).addz(1);
                loopk(linkup ? 6 : 5)
                {
                    float adist = raycube(apos, recursedirs[k], 0, RAY_CLIPMAT);
                    if(adist < WAYPOINTRADIUS) continue;

                    vec kpos = vec(a.o).add(vec(recursedirs[k]).mul(min(float(WAYPOINTRADIUS), adist)));
                    close = closestwaypoint(kpos, WAYPOINTRADIUS, false);

                    if(!iswaypoint(close)) continue;

                    waypoint &c = waypoints[close];
                    vec cpos = vec(c.o).addz(1);

                    float cdist = raycube(apos, vec(cpos).sub(wpos).safenormalize(), 0, RAY_CLIPMAT);
                    if(cdist < WAYPOINTRADIUS) continue;

                    loopj(MAXWAYPOINTLINKS)
                    {
                        if(c.links[j] == r) continue;
                        if(!c.links[j]) { c.links[j] = r; break; }
                    }

                    loopj(MAXWAYPOINTLINKS)
                    {
                        if(a.links[j] == close) continue;
                        if(!a.links[j]) { a.links[j] = close; break; }
                    }
                }

                created++;

                if(i >= 5 && !linkup) continue;
                loopj(MAXWAYPOINTLINKS)
                {
                    if(a.links[j] == n) continue;
                    if(!a.links[j]) { a.links[j] = n; continue; }
                }
                a.links[rnd(MAXWAYPOINTLINKS)] = n;
            }
        }

        return created;
    }

    void remapwaypoints();

    int explodewaypointmesh(int n = 4, int dirs = 4, bool linkup = false, bool saved = false, int m = -1)
    {
        if(m < 0) m = explodewaypointsmax;

        vector<int> considered;
        int created = 0;

        clearwpcache();

        loopk(n)
        {
            if(waypoints.length() >= m) break;

            vector<int> origwaypoints;
            loopv(waypoints) if(iswaypoint(i) && considered.find(i) < 0)
                origwaypoints.add(i);

            int count = origwaypoints.length(), createbefore = created;
            loopv(origwaypoints)
            {
                progress(i / float(count), "Exploding waypoints: Pass %d of %d, created %d", k + 1, n, created);
                int create = recursewaypoint(origwaypoints[i], dirs, linkup, saved);
                if(!create) continue;
                created += create;
                considered.add(i);
            }
            if(createbefore == created) break;
        }

        remapwaypoints();

        return created;
    }
    ICOMMAND(0, explodewaypointmesh, "bbibb", (int *n, int *p, int *u, int *s, int *m), intret(explodewaypointmesh(*n > 0 ? *n : 4, *p > 0 ? *p : 4, *u != 0, *s != 0, *m)));

    int addwaypoint(const vec &o, int pull = -1, bool saved = true)
    {
        if(waypoints.length() > MAXWAYPOINTS) return -1;
        int n = waypoints.length();
        waypoints.add(waypoint(o, pull >= 0 ? pull : getpull(o), WAYPOINTVERSION, saved));
        recursewaypoint(n);
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

    void unlinkwaypoint(waypoint &a, int n)
    {
        int index = 0;
        loopi(MAXWAYPOINTLINKS)
        {
            if(a.links[i] == n) continue;
            a.links[index++] = a.links[i];
        }
    }

    static inline bool shouldnavigate()
    {
        if(dropwaypoints) return true;
        loopvrev(players) if(players[i] && players[i]->actortype != A_PLAYER) return true;
        return false;
    }

    static inline bool shoulddrop(gameent *d)
    {
        return !d->ai && (dropwaypoints || m_play(game::gamemode));
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
        if(!iswaypoint(from))
        {
            from = addwaypoint(o);
            to = closestwaypoint(v, mindist, false);
        }
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
        vec v(ai::getbottom(d));
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
    ICOMMAND(0, clearwaypoints, "", (), clearwaypoints(false));

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
                if((w.links[k] = remap[link]))
                    if(++k >= MAXWAYPOINTLINKS) break;
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

                if(!waypoints.inrange(link))
                {
                    conoutf(colourred, "Error: waypoint %d has link to invalid waypoint %d", i, link);
                    w.links[k] = 0;
                    continue;
                }

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
            //if(!w.haslinks()) conoutf(colourorange, "Warning: waypoint %d has no links after import", i);

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
        waypointversion = -1;
        if(!getwaypointfile(mname, wptname)) return false;
        if(!force && (waypoints.length() || !strcmp(loadedwaypoints, wptname))) return true;

        stream *f = opengzfile(wptname, "rb");
        if(!f)
        {
            conoutf(colourred, "Cannot open waypoint file: %s", wptname);
            return false;
        }

        char magic[4];
        if(f->read(magic, 4) < 4)
        {
            conoutf(colourred, "Cannot read waypoint file: %s", wptname);
            delete f;
            return false;
        }

        if(!memcmp(magic, "OWPT", 4)) waypointversion = 0;
        else if(!memcmp(magic, "RWPT", 4))
        {
            waypointversion = f->getlil<int>();
            if(waypointversion > WAYPOINTVERSION)
            {
                conoutf(colourred, "Waypoint file %s is from a newer version", wptname);
                waypointversion = -1;
                delete f;
                return false;
            }
        }
        else
        {
            conoutf(colourred, "Invalid waypoint file %s: %s", wptname, magic);
            waypointversion = -1;
            delete f;
            return false;
        }

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

            int version = waypointversion >= 1 ? f->getlil<int>() : 1; // start fresh at 1
            waypoint &w = waypoints.add(waypoint(o, getpull(o), version, true));

            int numlinks = f->getchar(), k = 0;
            loopj(numlinks) if((w.links[k] = f->getlil<ushort>()) != 0) if(++k >= MAXWAYPOINTLINKS) break;
        }

        delete f;
        conoutf(colourwhite, "Loaded %d waypoints from %s", numwp, wptname);

        if(explodewaypoints&(m_edit(game::gamemode) ? 2 : 1))
        {
            conoutf(colourwhite, "Exploding waypoints..");
            if(explodewaypointsaround) explodewaypointmesh(explodewaypointsaround, 4);
            if(explodewaypointsdown) explodewaypointmesh(explodewaypointsdown, 5);
            if(explodewaypointsup) explodewaypointmesh(explodewaypointsup, 6, true);
        }

        if(!cleanwaypoints()) clearwpcache();
        game::specreset();
        return true;
    }
    ICOMMAND(0, loadwaypoints, "s", (char *mname), if(!(identflags&IDF_MAP)) getwaypoints(true, mname));

    void savewaypoints(bool force, const char *mname)
    {
        if((!dropwaypoints && !force) || waypoints.empty()) return;

        string wptname;
        if(!getwaypointfile(mname, wptname)) return;

        stream *f = opengzfile(wptname, "wb");
        if(!f) return;
        f->write("RWPT", 4);
        f->putlil<int>(WAYPOINTVERSION);

        vector<int> savedwaypoints, removedwaypoints;
        loopv(waypoints)
        {
            if(iswaypoint(i))
            {
                if(waypoints[i].saved) savedwaypoints.add(i);
                else removedwaypoints.add(i);
            }
        }

        loopv(savedwaypoints)
        {
            waypoint &w = waypoints[i];
            loopvj(removedwaypoints)
            {
                int n = removedwaypoints[j];
                unlinkwaypoint(w, n);
                loopk(MAXWAYPOINTLINKS) if(w.links[k] > n) w.links[k]--;
            }
        }

        f->putlil<ushort>(savedwaypoints.length());
        loopv(savedwaypoints)
        {
            int n = savedwaypoints[i];
            waypoint &w = waypoints[n];

            f->putlil<float>(w.o.x);
            f->putlil<float>(w.o.y);
            f->putlil<float>(w.o.z);

            f->putlil<int>(w.version);

            int numlinks = 0;
            loopj(MAXWAYPOINTLINKS) { if(!w.links[j]) break; numlinks++; }
            f->putchar(numlinks);
            loopj(numlinks) f->putlil<ushort>(w.links[j]);
        }

        delete f;
        conoutf(colourwhite, "Saved %d waypoints to %s", waypoints.length()-1, wptname);
    }

    ICOMMAND(0, savewaypoints, "s", (char *mname), if(!(identflags&IDF_MAP)) savewaypoints(true, mname));

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
            waypoint &w = waypoints.add(waypoint(v.o, getpull(v.o), 0, true));
            int k = 0;
            loopvj(v.links) if((w.links[k] = v.links[j]) != 0) if(++k >= MAXWAYPOINTLINKS) break;
        }
        conoutf(colourwhite, "Imported %d waypoints from the map file", oldwaypoints.length());
        oldwaypoints.setsize(0);
        if(!cleanwaypoints()) clearwpcache();
        return true;
    }

    bool getwaypoints(bool force, const char *mname, bool check)
    {
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

    void moveselwaypoints(const vec &offset)
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
    ICOMMAND(0, moveselwaypoints, "fff", (float *x, float *y, float *z), moveselwaypoints(vec(*x, *y, *z)));
}
