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
    if(m.flags&MESH_ALPHA && (mode&RAY_ALPHAPOLY)==RAY_ALPHAPOLY && (m.tex->alphamask || (lightmapping <= 1 && loadalphamask(m.tex))))
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
    bool hit = false;
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
                else if(triintersect(m, curnode->childindex(faridx), mo, mray, maxdist, maxdist, mode))
                {
                    if(mode&RAY_SHADOW) { dist = maxdist; return true; }
                    hit = true;
                }
            }
        }
        else if(curnode->isleaf(nearidx))
        {
            if(triintersect(m, curnode->childindex(nearidx), mo, mray, maxdist, maxdist, mode))
            {
                if(mode&RAY_SHADOW) { dist = maxdist; return true; }
                hit = true;
            }
            if(farsplit < tmax)
            {
                if(!curnode->isleaf(faridx))
                {
                    curnode += curnode->childindex(faridx);
                    tmin = max(tmin, farsplit);
                    continue;
                }
                else if(triintersect(m, curnode->childindex(faridx), mo, mray, maxdist, maxdist, mode))
                {
                    if(mode&RAY_SHADOW) { dist = maxdist; return true; }
                    hit = true;
                }
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
                        if(traverse(m, o, ray, invray, maxdist, maxdist, mode, curnode + curnode->childindex(nearidx), tmin, min(tmax, nearsplit)))
                        {
                            if(mode&RAY_SHADOW) { dist = maxdist; return true; }
                            hit = true;
                        }
                        curnode += curnode->childindex(faridx);
                        tmin = max(tmin, farsplit);
                        continue;
                    }
                }
                else if(triintersect(m, curnode->childindex(faridx), mo, mray, maxdist, maxdist, mode))
                {
                    if(mode&RAY_SHADOW) { dist = maxdist; return true; }
                    hit = true;
                }
            }
            curnode += curnode->childindex(nearidx);
            tmax = min(tmax, nearsplit);
            continue;
        }
        if(stacksize <= 0) { if(hit) { dist = maxdist; return true; } return false; }
        traversestate &restore = stack[--stacksize];
        curnode = restore.node;
        tmin = restore.tmin;
        tmax = restore.tmax;
    }
}

inline bool BIH::traverse(const vec &o, const vec &ray, float maxdist, float &dist, int mode)
{
    vec invray(ray.x ? 1/ray.x : 1e16f, ray.y ? 1/ray.y : 1e16f, ray.z ? 1/ray.z : 1e16f);
    bool hit = false;
    loopi(nummeshes)
    {
        mesh &m = meshes[i];
        if(!(mode&RAY_SHADOW) && m.flags&MESH_NOCLIP) continue;
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
        if(tmin < tmax && traverse(m, o, ray, invray, maxdist, maxdist, mode, m.nodes, tmin, tmax))
        {
            if(mode&RAY_SHADOW) { dist = maxdist; return true; }
            hit = true;
        }
    }
    if(hit) { dist = maxdist; return true; }
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
        if(!m->shadow || e.attrs[6]&MMT_NOSHADOW) return false;
    }
    else if((mode&RAY_ENTS)!=RAY_ENTS && !m->collide) return false;
    if(!m->bih && (lightmapping > 1 || !m->setBIH())) return false;
    float scale = e.attrs[5] ? 100.0f/e.attrs[5] : 1.0f;
    int yaw = e.attrs[1], pitch = e.attrs[2], roll = e.attrs[3];
    vec mo = vec(o).sub(e.o).mul(scale), mray(ray);
    float v = mo.dot(mray), inside = m->bih->entradius - mo.squaredlen();
    if((inside < 0 && v > 0) || inside + v*v < 0) return false;
    if(yaw != 0)
    {
        const vec2 &rot = sincosmod360(-yaw);
        mo.rotate_around_z(rot);
        mray.rotate_around_z(rot);
    }
    if(roll != 0)
    {
        const vec2 &rot = sincosmod360(roll);
        mo.rotate_around_x(rot);
        mray.rotate_around_x(rot);
    }
    if(pitch != 0)
    {
        const vec2 &rot = sincosmod360(pitch);
        mo.rotate_around_y(rot);
        mray.rotate_around_y(rot);
    }
    if(m->bih->traverse(mo, mray, maxdist ? maxdist*scale : 1e16f, dist, mode))
    {
        dist /= scale;
        if(!(mode&RAY_SHADOW))
        {
            if(pitch != 0) hitsurface.rotate_around_y(sincosmod360(-pitch));
            if(roll != 0) hitsurface.rotate_around_x(sincosmod360(-roll));
            if(yaw != 0) hitsurface.rotate_around_z(sincosmod360(yaw));
        }
        return true;
    }
    return false;
}

