#include "engine.h"

extern vec hitsurface;

bool BIH::triintersect(const mesh &m, int tidx, const vec &mo, const vec &mray, float maxdist, float &dist, int mode)
{
    const tri &t = m.tris[tidx];
    vec a = m.getpos(t.vert[0]), b = m.getpos(t.vert[1]).sub(a), c = m.getpos(t.vert[2]).sub(a),
        n = vec().cross(b, c), r = vec(a).sub(mo), e = vec().cross(r, mray);
    float det = mray.dot(n), v, w, f;
    if(det >= 0)
    {
        if(!(mode&RAY_SHADOW) && m.flags&MESH_CULLFACE) return false;
        v = e.dot(c);
        if(v < 0 || v > det) return false;
        w = -e.dot(b);
        if(w < 0 || v + w > det) return false;
        f = r.dot(n)*m.scale;
        if(f < 0 || f > maxdist*det || !det) return false;
    }
    else
    {
        v = e.dot(c);
        if(v > 0 || v < det) return false;
        w = -e.dot(b);
        if(w > 0 || v + w < det) return false;
        f = r.dot(n)*m.scale;
        if(f > 0 || f < maxdist*det) return false;
    }
    float invdet = 1/det;
    if(m.flags&MESH_ALPHA && (mode&RAY_ALPHAPOLY)==RAY_ALPHAPOLY && (m.tex->alphamask || loadalphamask(m.tex)))
    {
        vec2 at = m.gettc(t.vert[0]), bt = m.gettc(t.vert[1]).sub(at).mul(v*invdet), ct = m.gettc(t.vert[2]).sub(at).mul(w*invdet);
        at.add(bt).add(ct);
        int si = clamp(int(m.tex->xs * at.x), 0, m.tex->xs-1),
            ti = clamp(int(m.tex->ys * at.y), 0, m.tex->ys-1);
        if(!(m.tex->alphamask[ti*((m.tex->xs+7)/8) + si/8] & (1<<(si%8)))) return false;
    }
    if(!(mode&RAY_SHADOW)) hitsurface = m.xformnorm.transform(n).normalize();
    dist = f*invdet;
    return true;
}

struct traversestate
{
    BIH::node *node;
    float tmin, tmax;
};

inline bool BIH::traverse(const mesh &m, const vec &o, const vec &ray, const vec &invray, float maxdist, float &dist, int mode, node *curnode, float tmin, float tmax)
{
    traversestate stack[128];
    int stacksize = 0;
    ivec order(ray.x>0 ? 0 : 1, ray.y>0 ? 0 : 1, ray.z>0 ? 0 : 1);
    vec mo = m.invxform.transform(o), mray = m.invxformnorm.transform(ray);
    for(;;)
    {
        int axis = curnode->axis();
        int nearidx = order[axis], faridx = nearidx^1;
        float nearsplit = (curnode->split[nearidx] - o[axis])*invray[axis],
              farsplit = (curnode->split[faridx] - o[axis])*invray[axis];

        if(nearsplit <= tmin)
        {
            if(farsplit < tmax)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    tmin = max(tmin, farsplit);
                    continue;
                }
                else if(triintersect(m, curnode->childindex(faridx), mo, mray, maxdist, dist, mode)) return true;
            }
        }
        else if(curnode->isleaf(nearidx))
        {
            if(triintersect(m, curnode->childindex(nearidx), mo, mray, maxdist, dist, mode)) return true;
            if(farsplit < tmax)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    tmin = max(tmin, farsplit);
                    continue;
                }
                else if(triintersect(m, curnode->childindex(faridx), mo, mray, maxdist, dist, mode)) return true;
            }
        }
        else
        {
            if(farsplit < tmax)
            {
                if(!curnode->isleaf(faridx))
                {
                    if(stacksize < int(sizeof(stack)/sizeof(stack[0])))
                    {
                        traversestate &save = stack[stacksize++];
                        save.node = curnode + curnode->childindex(faridx);
                        save.tmin = max(tmin, farsplit);
                        save.tmax = tmax;
                    }
                    else
                    {
                        if(traverse(m, o, ray, invray, maxdist, dist, mode, curnode + curnode->childindex(nearidx), tmin, min(tmax, nearsplit))) return true;
                        curnode += curnode->childindex(faridx);
                        tmin = max(tmin, farsplit);
                        continue;
                    }
                }
                else if(triintersect(m, curnode->childindex(faridx), mo, mray, maxdist, dist, mode)) return true;
            }
            curnode += curnode->childindex(nearidx);
            tmax = min(tmax, nearsplit);
            continue;
        }
        if(stacksize <= 0) return false;
        traversestate &restore = stack[--stacksize];
        curnode = restore.node;
        tmin = restore.tmin;
        tmax = restore.tmax;
    }
}

inline bool BIH::traverse(const vec &o, const vec &ray, float maxdist, float &dist, int mode)
{
    vec invray(ray.x ? 1/ray.x : 1e16f, ray.y ? 1/ray.y : 1e16f, ray.z ? 1/ray.z : 1e16f);
    loopi(nummeshes)
    {
        mesh &m = meshes[i];
        if(!(m.flags&MESH_RENDER) || (!(mode&RAY_SHADOW) && m.flags&MESH_NOCLIP)) continue;
        float t1 = (m.bbmin.x - o.x)*invray.x,
              t2 = (m.bbmax.x - o.x)*invray.x,
              tmin, tmax;
        if(invray.x > 0) { tmin = t1; tmax = t2; } else { tmin = t2; tmax = t1; }
        t1 = (m.bbmin.y - o.y)*invray.y;
        t2 = (m.bbmax.y - o.y)*invray.y;
        if(invray.y > 0) { tmin = max(tmin, t1); tmax = min(tmax, t2); } else { tmin = max(tmin, t2); tmax = min(tmax, t1); }
        t1 = (m.bbmin.z - o.z)*invray.z;
        t2 = (m.bbmax.z - o.z)*invray.z;
        if(invray.z > 0) { tmin = max(tmin, t1); tmax = min(tmax, t2); } else { tmin = max(tmin, t2); tmax = min(tmax, t1); }
        tmax = min(tmax, maxdist);
        if(tmin < tmax && traverse(m, o, ray, invray, maxdist, dist, mode, m.nodes, tmin, tmax)) return true;
    }
    return false;
}

void BIH::build(mesh &m, ushort *indices, int numindices, const ivec &vmin, const ivec &vmax)
{
    int axis = 2;
    loopk(2) if(vmax[k] - vmin[k] > vmax[axis] - vmin[axis]) axis = k;

    ivec leftmin, leftmax, rightmin, rightmax;
    int splitleft, splitright;
    int left, right;
    loopk(3)
    {
        leftmin = rightmin = ivec(INT_MAX, INT_MAX, INT_MAX);
        leftmax = rightmax = ivec(INT_MIN, INT_MIN, INT_MIN);
        int split = (vmax[axis] + vmin[axis])/2;
        for(left = 0, right = numindices, splitleft = SHRT_MIN, splitright = SHRT_MAX; left < right;)
        {
            const tribb &tri = m.tribbs[indices[left]];
            ivec trimin = ivec(tri.center).sub(ivec(tri.radius)),
                 trimax = ivec(tri.center).add(ivec(tri.radius));
            int amin = trimin[axis], amax = trimax[axis];
            if(max(split - amin, 0) > max(amax - split, 0))
            {
                ++left;
                splitleft = max(splitleft, amax);
                leftmin.min(trimin);
                leftmax.max(trimax);
            }
            else
            {
                --right;
                swap(indices[left], indices[right]);
                splitright = min(splitright, amin);
                rightmin.min(trimin);
                rightmax.max(trimax);
            }
        }
        if(left > 0 && right < numindices) break;
        axis = (axis+1)%3;
    }

    if(!left || right==numindices)
    {
        leftmin = rightmin = ivec(INT_MAX, INT_MAX, INT_MAX);
        leftmax = rightmax = ivec(INT_MIN, INT_MIN, INT_MIN);
        left = right = numindices/2;
        splitleft = SHRT_MIN;
        splitright = SHRT_MAX;
        loopi(numindices)
        {
            const tribb &tri = m.tribbs[indices[i]];
            ivec trimin = ivec(tri.center).sub(ivec(tri.radius)),
                 trimax = ivec(tri.center).add(ivec(tri.radius));
            if(i < left)
            {
                splitleft = max(splitleft, trimax[axis]);
                leftmin.min(trimin);
                leftmax.max(trimax);
            }
            else
            {
                splitright = min(splitright, trimin[axis]);
                rightmin.min(trimin);
                rightmax.max(trimax);
            }
        }
    }

    int offset = m.numnodes++;
    node &curnode = m.nodes[offset];
    curnode.split[0] = short(splitleft);
    curnode.split[1] = short(splitright);

    if(left==1) curnode.child[0] = (axis<<14) | indices[0];
    else
    {
        curnode.child[0] = (axis<<14) | (m.numnodes - offset);
        build(m, indices, left, leftmin, leftmax);
    }

    if(numindices-right==1) curnode.child[1] = (1<<15) | (left==1 ? 1<<14 : 0) | indices[right];
    else
    {
        curnode.child[1] = (left==1 ? 1<<14 : 0) | (m.numnodes - offset);
        build(m, &indices[right], numindices-right, rightmin, rightmax);
    }
}

BIH::BIH(vector<mesh> &buildmeshes)
  : meshes(NULL), nummeshes(0), nodes(NULL), numnodes(0), tribbs(NULL), numtris(0), bbmin(1e16f, 1e16f, 1e16f), bbmax(-1e16f, -1e16f, -1e16f), center(0, 0, 0), radius(0), entradius(0)
{
    if(buildmeshes.empty()) return;
    loopv(buildmeshes) numtris += buildmeshes[i].numtris;
    if(!numtris) return;

    nummeshes = buildmeshes.length();
    meshes = new mesh[nummeshes];
    memcpy(meshes, buildmeshes.getbuf(), sizeof(mesh)*buildmeshes.length());
    tribbs = new tribb[numtris];
    tribb *dsttri = tribbs;
    loopi(nummeshes)
    {
        mesh &m = meshes[i];
        m.scale = m.xform.a.magnitude();
        m.invscale = 1/m.scale;
        m.xformnorm = matrix3(m.xform);
        m.xformnorm.normalize();
        m.invxform.invert(m.xform);
        m.invxformnorm = matrix3(m.invxform);
        m.invxformnorm.normalize();
        m.tribbs = dsttri;
        const tri *srctri = m.tris;
        vec mmin(1e16f, 1e16f, 1e16f), mmax(-1e16f, -1e16f, -1e16f);
        loopj(m.numtris)
        {
            vec s0 = m.getpos(srctri->vert[0]), s1 = m.getpos(srctri->vert[1]), s2 = m.getpos(srctri->vert[2]),
                v0 = m.xform.transform(s0), v1 = m.xform.transform(s1), v2 = m.xform.transform(s2),
                vmin = vec(v0).min(v1).min(v2),
                vmax = vec(v0).max(v1).max(v2);
            mmin.min(vmin);
            mmax.max(vmax);
            ivec imin = ivec::floor(vmin), imax = ivec::ceil(vmax);
            dsttri->center = svec(ivec(imin).add(imax).div(2));
            dsttri->radius = svec(ivec(imax).sub(imin).add(1).div(2));
            ++srctri;
            ++dsttri;
        }
        loopk(3) if(fabs(mmax[k] - mmin[k]) < 0.125f)
        {
            float mid = (mmin[k] + mmax[k]) / 2;
            mmin[k] = mid - 0.0625f;
            mmax[k] = mid + 0.0625f;
        }
        m.bbmin = mmin;
        m.bbmax = mmax;
        bbmin.min(mmin);
        bbmax.max(mmax);
    }

    center = vec(bbmin).add(bbmax).mul(0.5f);
    radius = vec(bbmax).sub(bbmin).mul(0.5f).magnitude();
    entradius = max(bbmin.squaredlen(), bbmax.squaredlen());

    nodes = new node[numtris];
    node *curnode = nodes;
    ushort *indices = new ushort[numtris];
    loopi(nummeshes)
    {
        mesh &m = meshes[i];
        m.nodes = curnode;
        loopj(m.numtris) indices[j] = j;
        build(m, indices, m.numtris, ivec::floor(m.bbmin), ivec::ceil(m.bbmax));
        curnode += m.numnodes;
    }
    delete[] indices;
    numnodes = int(curnode - nodes);
}

BIH::~BIH()
{
    delete[] meshes;
    delete[] nodes;
    delete[] tribbs;
}

bool mmintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist)
{
    model *m = loadmapmodel(e.attrs[0]);
    if(!m) return false;
    if(mode&RAY_SHADOW)
    {
        if(!m->shadow || e.flags&EF_NOSHADOW) return false;
    }
    else if((mode&RAY_ENTS)!=RAY_ENTS && (!m->collide || e.flags&EF_NOCOLLIDE)) return false;
    if(!m->bih && !m->setBIH()) return false;
    float scale = e.attrs[5] ? 100.0f/e.attrs[5] : 1.0f;
    vec mo = vec(o).sub(e.o).mul(scale), mray(ray);
    float v = mo.dot(mray), inside = m->bih->entradius - mo.squaredlen();
    if((inside < 0 && v > 0) || inside + v*v < 0) return false;
    int yaw = e.attrs[1], pitch = e.attrs[2], roll = e.attrs[3];
    if(yaw != 0)
    {
        const vec2 &rot = sincosmod360(-yaw);
        mo.rotate_around_z(rot);
        mray.rotate_around_z(rot);
    }
    if(pitch != 0)
    {
        const vec2 &rot = sincosmod360(-pitch);
        mo.rotate_around_x(rot);
        mray.rotate_around_x(rot);
    }
    if(roll != 0)
    {
        const vec2 &rot = sincosmod360(roll);
        mo.rotate_around_y(rot);
        mray.rotate_around_y(rot);
    }
    if(m->bih->traverse(mo, mray, maxdist ? maxdist*scale : 1e16f, dist, mode))
    {
        dist /= scale;
        if(!(mode&RAY_SHADOW))
        {
            if(roll != 0) hitsurface.rotate_around_y(sincosmod360(-roll));
            if(pitch != 0) hitsurface.rotate_around_x(sincosmod360(pitch));
            if(yaw != 0) hitsurface.rotate_around_z(sincosmod360(yaw));
        }
        return true;
    }
    return false;
}

static inline float segmentdistance(const vec &d1, const vec &d2, const vec &r)
{
    float a = d1.squaredlen(), e = d2.squaredlen(), f = d2.dot(r), s, t;
    if(a <= 1e-4f)
    {
        if(e <= 1e-4f) return r.squaredlen();
        s = 0;
        t = clamp(-f / e, 0.0f, 1.0f);
    }
    else
    {
        float c = d1.dot(r);
        if(e <= 1e-4f)
        {
            t = 0;
            s = clamp(c / a, 0.0f, 1.0f);
        }
        else
        {
            float b = d1.dot(d2), denom = a*e - b*b;
            s = denom ? clamp((c*e - b*f) / denom, 0.0f, 1.0f) : 0.0f;
            t = b*s - f;
            if(t < 0)
            {
                t = 0;
                s = clamp(c / a, 0.0f, 1.0f);
            }
            else if(t > e)
            {
                t = 1;
                s = clamp((b + c) / a, 0.0f, 1.0f);
            }
            else t /= e;
        }
    }
    vec c1 = vec(d1).mul(s),
        c2 = vec(d2).mul(t);
    return vec(c2).sub(c1).add(r).squaredlen();
}

static inline float trisegmentdistance(const vec &a, const vec &b, const vec &c, const vec &p, const vec &q)
{
    vec pq = vec(q).sub(p), ab = vec(b).sub(a), bc = vec(c).sub(b), ca = vec(a).sub(c),
        ap = vec(p).sub(a), bp = vec(p).sub(b), cp = vec(p).sub(c),
        aq = vec(q).sub(a), bq = vec(q).sub(b),
        n, nab, nbc, nca;
    n.cross(ab, bc);
    nab.cross(n, ab);
    nbc.cross(n, bc);
    nca.cross(n, ca);
    float dp = n.dot(ap), dq = n.dot(aq), dist;
    if(ap.dot(nab) < 0) // P outside AB
    {
        dist = segmentdistance(ab, pq, ap);
        if(bq.dot(nbc) < 0) dist = min(dist, segmentdistance(bc, pq, bp)); // Q outside BC
        else if(aq.dot(nca) < 0) dist = min(dist, segmentdistance(pq, ca, cp)); // Q outside CA
        else if(aq.dot(nab) >= 0) dist = min(dist, dq*dq/n.squaredlen()); // Q inside AB
        else return dist;
    }
    else if(bp.dot(nbc) < 0) // P outside BC
    {
        dist = segmentdistance(bc, pq, bp);
        if(aq.dot(nca) < 0) dist = min(dist, segmentdistance(ca, pq, cp)); // Q outside CA
        else if(aq.dot(nab) < 0) dist = min(dist, segmentdistance(ab, pq, ap)); // Q outside AB
        else if(bq.dot(nbc) >= 0) dist = min(dist, dq*dq/n.squaredlen()); // Q inside BC
        else return dist;
    }
    else if(cp.dot(nca) < 0) // P outside CA
    {
        dist = segmentdistance(ca, pq, cp);
        if(aq.dot(nab) < 0) dist = min(dist, segmentdistance(ab, pq, ap)); // Q outside AB
        else if(bq.dot(nbc) < 0) dist = min(dist, segmentdistance(bc, pq, bp)); // Q outside BC
        else if(aq.dot(nca) >= 0) dist = min(dist, dq*dq/n.squaredlen()); // Q inside CA
        else return dist;
    }
    else if(aq.dot(nab) < 0) dist = min(segmentdistance(ab, pq, ap), dp); // Q outside AB
    else if(bq.dot(nbc) < 0) dist = min(segmentdistance(bc, pq, bp), dp); // Q outside BC
    else if(aq.dot(nca) < 0) dist = min(segmentdistance(ca, pq, cp), dp); // Q outside CA
    else // both P and Q inside
    {
        if(dp > 0 ? dq <= 0 : dq >= 0) return 0; // P and Q on different sides of triangle
        dist = min(dp*dp, dq*dq)/n.squaredlen();
        return dist;
    }
    if(dp > 0 ? dq >= 0 : dq <= 0) return dist; // both P and Q on same side of triangle
    vec e = vec().cross(pq, ap);
    float det = fabs(dq - dp), v = ca.dot(e);
    if(v < 0 || v > det) return dist;
    float w = ab.dot(e);
    if(w < 0 || v + w > det) return dist;
    return 0; // segment intersects triangle
}

static inline bool triboxoverlap(const vec &radius, const vec &a, const vec &b, const vec &c)
{
    vec ab = vec(b).sub(a), bc = vec(c).sub(b), ca = vec(a).sub(c);

    #define TESTAXIS(v0, v1, v2, e, s, t) { \
        float p = v0.s*v1.t - v0.t*v1.s, \
              q = v2.s*e.t - v2.t*e.s, \
              r = radius.s*fabs(e.t) + radius.t*fabs(e.s); \
        if(p < q) { if(q < -r || p > r) return false; } \
        else if(p < -r || q > r) return false; \
    }

    TESTAXIS(a, b, c, ab, z, y);
    TESTAXIS(a, b, c, ab, x, z);
    TESTAXIS(a, b, c, ab, y, x);

    TESTAXIS(b, c, a, bc, z, y);
    TESTAXIS(b, c, a, bc, x, z);
    TESTAXIS(b, c, a, bc, y, x);

    TESTAXIS(c, a, b, ca, z, y);
    TESTAXIS(c, a, b, ca, x, z);
    TESTAXIS(c, a, b, ca, y, x);

    #define TESTFACE(w) { \
        if(a.w < b.w) \
        { \
            if(b.w < c.w) { if(c.w < -radius.w || a.w > radius.w) return false; } \
            else if(b.w < -radius.w || min(a.w, c.w) > radius.w) return false; \
        } \
        else if(a.w < c.w) { if(c.w < -radius.w || b.w > radius.w) return false; } \
        else if(a.w < -radius.w || min(b.w, c.w) > radius.w) return false; \
    }

    TESTFACE(x);
    TESTFACE(y);
    TESTFACE(z);

    return true;
}

template<>
inline void BIH::tricollide<COLLIDE_ELLIPSE>(const mesh &m, int tidx, physent *d, const vec &dir, float cutoff, const vec &center, const vec &radius, const matrix4x3 &orient, float &dist, const ivec &bo, const ivec &br)
{
    if(m.tribbs[tidx].outside(bo, br)) return;

    const tri &t = m.tris[tidx];
    vec a = m.getpos(t.vert[0]), b = m.getpos(t.vert[1]), c = m.getpos(t.vert[2]),
        zdir = vec(orient.rowz()).mul(m.invscale*m.invscale*(radius.z - radius.x));
    if(trisegmentdistance(a, b, c, vec(center).sub(zdir), vec(center).add(zdir)) > m.invscale*m.invscale*radius.x*radius.x) return;

    vec n;
    n.cross(a, b, c).normalize();
    float pdist = (n.dot(vec(center).sub(a)) - fabs(n.dot(zdir)))*m.scale - radius.x;
    if(pdist > 0 || pdist <= dist) return;

    collideinside = true;

    n = orient.transformnormal(n).mul(m.invscale);

    if(!dir.iszero())
    {
        if(n.dot(dir) >= -cutoff*dir.magnitude()) return;
        if(d->type==ENT_PLAYER &&
            pdist < (dir.z*n.z < 0 ?
               2*radius.z*(d->zmargin/(d->aboveeye+d->height)-(dir.z < 0 ? 1/3.0f : 1/4.0f)) :
               (dir.x*n.x < 0 || dir.y*n.y < 0 ? -radius.x : 0)))
            return;
    }

    dist = pdist;
    collidewall = n;
}

template<>
inline void BIH::tricollide<COLLIDE_OBB>(const mesh &m, int tidx, physent *d, const vec &dir, float cutoff, const vec &center, const vec &radius, const matrix4x3 &orient, float &dist, const ivec &bo, const ivec &br)
{
    if(m.tribbs[tidx].outside(bo, br)) return;

    const tri &t = m.tris[tidx];
    vec a = orient.transform(m.getpos(t.vert[0])), b = orient.transform(m.getpos(t.vert[1])), c = orient.transform(m.getpos(t.vert[2]));
    if(!triboxoverlap(radius, a, b, c)) return;

    vec n;
    n.cross(a, b, c).normalize();
    float pdist = -n.dot(a), r = radius.absdot(n);
    if(fabs(pdist) > r) return;

    pdist -= r;
    if(pdist <= dist) return;

    collideinside = true;

    if(!dir.iszero())
    {
        if(n.dot(dir) >= -cutoff*dir.magnitude()) return;
        if(d->type==ENT_PLAYER &&
            pdist < (dir.z*n.z < 0 ?
               2*radius.z*(d->zmargin/(d->aboveeye+d->height)-(dir.z < 0 ? 1/3.0f : 1/4.0f)) :
               (dir.x*n.x < 0 || dir.y*n.y < 0 ? -max(radius.x, radius.y) : 0)))
            return;
    }

    dist = pdist;
    collidewall = n;
}

template<int C>
inline void BIH::collide(const mesh &m, physent *d, const vec &dir, float cutoff, const vec &center, const vec &radius, const matrix4x3 &orient, float &dist, node *curnode, const ivec &bo, const ivec &br)
{
    node *stack[128];
    int stacksize = 0;
    ivec bmin = ivec(bo).sub(br), bmax = ivec(bo).add(br);
    for(;;)
    {
        int axis = curnode->axis();
        const int nearidx = 0, faridx = nearidx^1;
        int nearsplit = bmin[axis] - curnode->split[nearidx],
            farsplit = curnode->split[faridx] - bmax[axis];

        if(nearsplit > 0)
        {
            if(farsplit <= 0)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    continue;
                }
                else tricollide<C>(m, curnode->childindex(faridx), d, dir, cutoff, center, radius, orient, dist, bo, br);
            }
        }
        else if(curnode->isleaf(nearidx))
        {
            tricollide<C>(m, curnode->childindex(nearidx), d, dir, cutoff, center, radius, orient, dist, bo, br);
            if(farsplit <= 0)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    continue;
                }
                else tricollide<C>(m, curnode->childindex(faridx), d, dir, cutoff, center, radius, orient, dist, bo, br);
            }
        }
        else
        {
            if(farsplit <= 0)
            {
                if(!curnode->isleaf(faridx))
                {
                    if(stacksize < int(sizeof(stack)/sizeof(stack[0])))
                    {
                        stack[stacksize++] = curnode + curnode->childindex(faridx);
                    }
                    else
                    {
                        collide<C>(m, d, dir, cutoff, center, radius, orient, dist, &nodes[curnode->childindex(nearidx)], bo, br);
                        curnode += curnode->childindex(faridx);
                        continue;
                    }
                }
                else tricollide<C>(m, curnode->childindex(faridx), d, dir, cutoff, center, radius, orient, dist, bo, br);
            }
            curnode += curnode->childindex(nearidx);
            continue;
        }
        if(stacksize <= 0) return;
        curnode = stack[--stacksize];
    }
}


bool BIH::ellipsecollide(physent *d, const vec &dir, float cutoff, const vec &o, int yaw, int pitch, int roll, float scale)
{
    if(!numnodes) return false;

    vec center(d->o.x, d->o.y, d->o.z + 0.5f*(d->aboveeye - d->height)),
        radius(d->radius, d->radius, 0.5f*(d->height + d->aboveeye));
    center.sub(o);
    if(scale != 1) { float invscale = 1/scale; center.mul(invscale); radius.mul(invscale); }

    matrix3 orient;
    orient.identity();
    if(yaw) orient.rotate_around_z(sincosmod360(yaw));
    if(pitch) orient.rotate_around_x(sincosmod360(pitch));
    if(roll) orient.rotate_around_y(sincosmod360(-roll));

    vec bo = orient.transposedtransform(center), br = orient.abstransposedtransform(radius);
    if(bo.x + br.x < bbmin.x || bo.y + br.y < bbmin.y || bo.z + br.z < bbmin.z ||
       bo.x - br.x > bbmax.x || bo.y - br.y > bbmax.y || bo.z - br.z > bbmax.z)
        return false;

    ivec imin = ivec::floor(vec(bo).sub(br)), imax = ivec::ceil(vec(bo).add(br)),
         icenter = ivec(imin).add(imax).div(2),
         iradius = ivec(imax).sub(imin).add(1).div(2);

    float dist = -1e10f;
    loopi(nummeshes)
    {
        mesh &m = meshes[i];
        if(!(m.flags&MESH_COLLIDE) || m.flags&MESH_NOCLIP) continue;
        matrix4x3 morient;
        morient.mul(orient, m.xform);
        collide<COLLIDE_ELLIPSE>(m, d, dir, cutoff, m.invxform.transform(bo), radius, morient, dist, m.nodes, icenter, iradius);
    }
    return dist > -1e9f;
}

bool BIH::boxcollide(physent *d, const vec &dir, float cutoff, const vec &o, int yaw, int pitch, int roll, float scale)
{
    if(!numnodes) return false;

    vec center(d->o.x, d->o.y, d->o.z + 0.5f*(d->aboveeye - d->height)),
        radius(d->xradius, d->yradius, 0.5f*(d->height + d->aboveeye));
    center.sub(o);
    if(scale != 1) { float invscale = 1/scale; center.mul(invscale); radius.mul(invscale); }

    matrix3 orient;
    orient.identity();
    if(yaw) orient.rotate_around_z(sincosmod360(yaw));
    if(pitch) orient.rotate_around_x(sincosmod360(pitch));
    if(roll) orient.rotate_around_y(sincosmod360(-roll));

    vec bo = orient.transposedtransform(center), br = orient.abstransposedtransform(vec(d->radius, d->radius, radius.z));
    if(bo.x + br.x < bbmin.x || bo.y + br.y < bbmin.y || bo.z + br.z < bbmin.z ||
       bo.x - br.x > bbmax.x || bo.y - br.y > bbmax.y || bo.z - br.z > bbmax.z)
        return false;

    ivec imin = ivec::floor(vec(bo).sub(br)), imax = ivec::ceil(vec(bo).add(br)),
         icenter = ivec(imin).add(imax).div(2),
         iradius = ivec(imax).sub(imin).add(1).div(2);

    matrix3 drot, dorient;
    drot.setyaw(d->yaw*RAD);
    vec ddir = drot.transform(dir), dcenter = drot.transform(center).neg();
    dorient.mul(drot, orient);

    float dist = -1e10f;
    loopi(nummeshes)
    {
        mesh &m = meshes[i];
        if(!(m.flags&MESH_COLLIDE) || m.flags&MESH_NOCLIP) continue;
        matrix4x3 morient;
        morient.mul(dorient, dcenter, m.xform);
        collide<COLLIDE_OBB>(m, d, ddir, cutoff, center, radius, morient, dist, m.nodes, icenter, iradius);
    }
    if(dist > -1e9f)
    {
        collidewall = drot.transposedtransform(collidewall);
        return true;
    }
    return false;
}

inline void BIH::genstaintris(stainrenderer *s, const mesh &m, int tidx, const vec &center, float radius, const matrix4x3 &orient, const ivec &bo, const ivec &br)
{
    if(m.tribbs[tidx].outside(bo, br)) return;

    const tri &t = m.tris[tidx];
    vec v[3] =
    {
        orient.transform(m.getpos(t.vert[0])),
        orient.transform(m.getpos(t.vert[1])),
        orient.transform(m.getpos(t.vert[2]))
    };

    genstainmmtri(s, v);
}

void BIH::genstaintris(stainrenderer *s, const mesh &m, const vec &center, float radius, const matrix4x3 &orient, node *curnode, const ivec &bo, const ivec &br)
{
    node *stack[128];
    int stacksize = 0;
    ivec bmin = ivec(bo).sub(br), bmax = ivec(bo).add(br);
    for(;;)
    {
        int axis = curnode->axis();
        const int nearidx = 0, faridx = nearidx^1;
        int nearsplit = bmin[axis] - curnode->split[nearidx],
            farsplit = curnode->split[faridx] - bmax[axis];

        if(nearsplit > 0)
        {
            if(farsplit <= 0)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    continue;
                }
                else genstaintris(s, m, curnode->childindex(faridx), center, radius, orient, bo, br);
            }
        }
        else if(curnode->isleaf(nearidx))
        {
            genstaintris(s, m, curnode->childindex(nearidx), center, radius, orient, bo, br);
            if(farsplit <= 0)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    continue;
                }
                else genstaintris(s, m, curnode->childindex(faridx), center, radius, orient, bo, br);
            }
        }
        else
        {
            if(farsplit <= 0)
            {
                if(!curnode->isleaf(faridx))
                {
                    if(stacksize < int(sizeof(stack)/sizeof(stack[0])))
                    {
                        stack[stacksize++] = curnode + curnode->childindex(faridx);
                    }
                    else
                    {
                        genstaintris(s, m, center, radius, orient, &nodes[curnode->childindex(nearidx)], bo, br);
                        curnode += curnode->childindex(faridx);
                        continue;
                    }
                }
                else genstaintris(s, m, curnode->childindex(faridx), center, radius, orient, bo, br);
            }
            curnode += curnode->childindex(nearidx);
            continue;
        }
        if(stacksize <= 0) return;
        curnode = stack[--stacksize];
    }
}

void BIH::genstaintris(stainrenderer *s, const vec &staincenter, float stainradius, const vec &o, int yaw, int pitch, int roll, float scale)
{
    if(!numnodes) return;

    vec center = vec(staincenter).sub(o);
    float radius = stainradius;
    if(scale != 1) { float invscale = 1/scale; center.mul(invscale); radius *= invscale; }

    matrix3 orient;
    orient.identity();
    if(yaw) orient.rotate_around_z(sincosmod360(yaw));
    if(pitch) orient.rotate_around_x(sincosmod360(pitch));
    if(roll) orient.rotate_around_y(sincosmod360(-roll));

    vec bo = orient.transposedtransform(center);
    if(bo.x + radius < bbmin.x || bo.y + radius < bbmin.y || bo.z + radius < bbmin.z ||
       bo.x - radius > bbmax.x || bo.y - radius > bbmax.y || bo.z - radius > bbmax.z)
        return;

    orient.scale(scale);

    ivec imin = ivec::floor(vec(bo).sub(radius)), imax = ivec::ceil(vec(bo).add(radius)),
         icenter = ivec(imin).add(imax).div(2),
         iradius = ivec(imax).sub(imin).add(1).div(2);

    loopi(nummeshes)
    {
        mesh &m = meshes[i];
        if(!(m.flags&MESH_RENDER) || m.flags&MESH_ALPHA) continue;
        matrix4x3 morient;
        morient.mul(orient, o, m.xform);
        genstaintris(s, m, m.invxform.transform(bo), radius, morient, m.nodes, icenter, iradius);
    }
}

