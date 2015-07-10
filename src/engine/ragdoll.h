VAR(0, ragdolltimestepmin, 1, 5, 50);
VAR(0, ragdolltimestepmax, 1, 10, 50);
FVAR(0, ragdollrotfric, 0, 0.85f, 1);
FVAR(0, ragdollrotfricstop, 0, 0.1f, 1);

struct ragdollskel
{
    struct vert
    {
        vec pos;
        float radius, weight;
    };

    struct tri
    {
        int vert[3];

        bool shareverts(const tri &t) const
        {
            loopi(3) loopj(3) if(vert[i] == t.vert[j]) return true;
            return false;
        }
    };

    struct distlimit
    {
        int vert[2];
        float mindist, maxdist;
    };

    struct rotlimit
    {
        int tri[2];
        float maxangle;
        matrix3x3 middle;
    };

    struct rotfriction
    {
        int tri[2];
        matrix3x3 middle;
    };

    struct joint
    {
        int bone, tri, vert[3];
        float weight;
        matrix3x4 orient;
    };

    struct reljoint
    {
        int bone, parent;
    };

    bool loaded, animjoints;
    int eye;
    vector<vert> verts;
    vector<tri> tris;
    vector<distlimit> distlimits;
    vector<rotlimit> rotlimits;
    vector<rotfriction> rotfrictions;
    vector<joint> joints;
    vector<reljoint> reljoints;

    ragdollskel() : loaded(false), animjoints(false), eye(-1) {}

    void setupjoints()
    {
        loopv(verts) verts[i].weight = 0;
        loopv(joints)
        {
            joint &j = joints[i];
            j.weight = 0;
            vec pos(0, 0, 0);
            loopk(3) if(j.vert[k]>=0)
            {
                pos.add(verts[j.vert[k]].pos);
                j.weight++;
                verts[j.vert[k]].weight++;
            }
            if(j.weight) j.weight = 1/j.weight;
            pos.mul(j.weight);

            tri &t = tris[j.tri];
            matrix3x3 m;
            const vec &v1 = verts[t.vert[0]].pos,
                      &v2 = verts[t.vert[1]].pos,
                      &v3 = verts[t.vert[2]].pos;
            m.a = vec(v2).sub(v1).normalize();
            m.c.cross(m.a, vec(v3).sub(v1)).normalize();
            m.b.cross(m.c, m.a);

            j.orient = matrix3x4(m, m.transform(pos).neg());
        }
        loopv(verts) if(verts[i].weight) verts[i].weight = 1/verts[i].weight;
        reljoints.shrink(0);
    }

    void setuprotfrictions()
    {
        rotfrictions.shrink(0);
        loopv(tris) for(int j = i+1; j < tris.length(); j++) if(tris[i].shareverts(tris[j]))
        {
            rotfriction &r = rotfrictions.add();
            r.tri[0] = i;
            r.tri[1] = j;
        }
    }

    void setup()
    {
        setupjoints();
        setuprotfrictions();

        loaded = true;
    }

    void addreljoint(int bone, int parent)
    {
        reljoint &r = reljoints.add();
        r.bone = bone;
        r.parent = parent;
    }
};

struct ragdolldata
{
    struct vert
    {
        vec oldpos, pos, newpos;
        float weight;
        bool collided, stuck;

        vert() : pos(0, 0, 0), newpos(0, 0, 0), weight(0), collided(false), stuck(true) {}
    };

    ragdollskel *skel;
    int millis, collidemillis, collisions, floating, lastmove, unsticks;
    vec offset, center;
    float radius, ztop, zbottom, timestep, scale;
    vert *verts;
    matrix3x3 *tris;
    matrix3x4 *animjoints;
    dualquat *reljoints;

    ragdolldata(ragdollskel *skel, float scale = 1)
        : skel(skel),
          millis(lastmillis),
          collidemillis(0),
          collisions(0),
          floating(0),
          lastmove(lastmillis),
          unsticks(INT_MAX),
          timestep(0),
          scale(scale),
          verts(new vert[skel->verts.length()]),
          tris(new matrix3x3[skel->tris.length()]),
          animjoints(!skel->animjoints || skel->joints.empty() ? NULL : new matrix3x4[skel->joints.length()]),
          reljoints(skel->reljoints.empty() ? NULL : new dualquat[skel->reljoints.length()])
    {
    }

    ~ragdolldata()
    {
        delete[] verts;
        delete[] tris;
        if(animjoints) delete[] animjoints;
        if(reljoints) delete[] reljoints;
    }

    void calcanimjoint(int i, const matrix3x4 &anim)
    {
        if(!animjoints) return;
        ragdollskel::joint &j = skel->joints[i];
        vec pos(0, 0, 0);
        loopk(3) if(j.vert[k]>=0) pos.add(verts[j.vert[k]].pos);
        pos.mul(j.weight);

        ragdollskel::tri &t = skel->tris[j.tri];
        matrix3x3 m;
        const vec &v1 = verts[t.vert[0]].pos,
                  &v2 = verts[t.vert[1]].pos,
                  &v3 = verts[t.vert[2]].pos;
        m.a = vec(v2).sub(v1).normalize();
        m.c.cross(m.a, vec(v3).sub(v1)).normalize();
        m.b.cross(m.c, m.a);
        animjoints[i].mul(m, m.transform(pos).neg(), anim);
    }

    void calctris()
    {
        loopv(skel->tris)
        {
            ragdollskel::tri &t = skel->tris[i];
            matrix3x3 &m = tris[i];
            const vec &v1 = verts[t.vert[0]].pos,
                      &v2 = verts[t.vert[1]].pos,
                      &v3 = verts[t.vert[2]].pos;
            m.a = vec(v2).sub(v1).normalize();
            m.c.cross(m.a, vec(v3).sub(v1)).normalize();
            m.b.cross(m.c, m.a);
        }
    }

    void calcboundsphere()
    {
        center = vec(0, 0, 0);
        loopv(skel->verts) center.add(verts[i].pos);
        center.div(skel->verts.length());
        radius = ztop = zbottom = 0;
        loopv(skel->verts)
        {
            radius = max(radius, verts[i].pos.dist(center));
            ztop = max(ztop, verts[i].pos.z);
            zbottom = min(zbottom, verts[i].pos.z);
        }
        ztop -= center.z;
        zbottom -= center.z;
    }

    void init(dynent *d)
    {
        float ts = ragdolltimestepmin/1000.0f;
        loopv(skel->verts) (verts[i].oldpos = verts[i].pos).sub(vec(d->vel).add(d->falling).mul(ts));
        timestep = ts;

        calctris();
        calcboundsphere();
        offset = d->o;
        offset.sub(skel->eye >= 0 ? verts[skel->eye].pos : center);
        offset.z += (d->height + d->aboveeye)/2;
    }

    void move(dynent *pl, float ts);
    void updatepos();
    void constrain();
    void constraindist();
    void applyrotlimit(ragdollskel::tri &t1, ragdollskel::tri &t2, float angle, const vec &axis);
    void constrainrot();
    void calcrotfriction();
    void applyrotfriction(float ts);
    void tryunstick(float speed);
    void warppos(const vec &vel, const vec &offset);
    void twitch(float vel);

    static inline bool collidevert(const vec &pos, const vec &dir, float radius)
    {
        static struct vertent : physent
        {
            vertent()
            {
                type = ENT_RAGDOLL;
                radius = xradius = yradius = height = aboveeye = 1;
            }
        } v;
        v.o = pos;
        if(v.radius != radius) v.radius = v.xradius = v.yradius = v.height = v.aboveeye = radius;
        return collide(&v, dir, 0, false);
    }
};

/*
    seed particle position = avg(modelview * base2anim * spherepos)
    mapped transform = invert(curtri) * origtrig
    parented transform = parent{invert(curtri) * origtrig} * (invert(parent{base2anim}) * base2anim)
*/

void ragdolldata::constraindist()
{
    float invscale = 1.0f/scale;
    loopv(skel->distlimits)
    {
        ragdollskel::distlimit &d = skel->distlimits[i];
        vert &v1 = verts[d.vert[0]], &v2 = verts[d.vert[1]];
        vec dir = vec(v2.pos).sub(v1.pos);
        float dist = dir.magnitude()*invscale, cdist;
        if(dist < d.mindist) cdist = d.mindist;
        else if(dist > d.maxdist) cdist = d.maxdist;
        else continue;
        if(dist > 1e-4f) dir.mul(cdist*0.5f/dist);
        else dir = vec(0, 0, cdist*0.5f/invscale);
        vec center = vec(v1.pos).add(v2.pos).mul(0.5f);
        v1.newpos.add(vec(center).sub(dir));
        v1.weight++;
        v2.newpos.add(vec(center).add(dir));
        v2.weight++;
    }
}

inline void ragdolldata::applyrotlimit(ragdollskel::tri &t1, ragdollskel::tri &t2, float angle, const vec &axis)
{
    vert &v1a = verts[t1.vert[0]], &v1b = verts[t1.vert[1]], &v1c = verts[t1.vert[2]],
         &v2a = verts[t2.vert[0]], &v2b = verts[t2.vert[1]], &v2c = verts[t2.vert[2]];
    vec m1 = vec(v1a.pos).add(v1b.pos).add(v1c.pos).div(3),
        m2 = vec(v2a.pos).add(v2b.pos).add(v2c.pos).div(3),
        q1a, q1b, q1c, q2a, q2b, q2c;
    float w1 = q1a.cross(axis, vec(v1a.pos).sub(m1)).magnitude() +
               q1b.cross(axis, vec(v1b.pos).sub(m1)).magnitude() +
               q1c.cross(axis, vec(v1c.pos).sub(m1)).magnitude(),
          w2 = q2a.cross(axis, vec(v2a.pos).sub(m2)).magnitude() +
               q2b.cross(axis, vec(v2b.pos).sub(m2)).magnitude() +
               q2c.cross(axis, vec(v2c.pos).sub(m2)).magnitude();
    angle /= w1 + w2 + 1e-9f;
    float a1 = angle*w2, a2 = -angle*w1,
          s1 = sinf(a1), s2 = sinf(a2);
    vec c1 = vec(axis).mul(1 - cosf(a1)), c2 = vec(axis).mul(1 - cosf(a2));
    v1a.newpos.add(vec().cross(c1, q1a).add(vec(q1a).mul(s1)).add(v1a.pos));
    v1a.weight++;
    v1b.newpos.add(vec().cross(c1, q1b).add(vec(q1b).mul(s1)).add(v1b.pos));
    v1b.weight++;
    v1c.newpos.add(vec().cross(c1, q1c).add(vec(q1c).mul(s1)).add(v1c.pos));
    v1c.weight++;
    v2a.newpos.add(vec().cross(c2, q2a).add(vec(q2a).mul(s2)).add(v2a.pos));
    v2a.weight++;
    v2b.newpos.add(vec().cross(c2, q2b).add(vec(q2b).mul(s2)).add(v2b.pos));
    v2b.weight++;
    v2c.newpos.add(vec().cross(c2, q2c).add(vec(q2c).mul(s2)).add(v2c.pos));
    v2c.weight++;
}

void ragdolldata::constrainrot()
{
    loopv(skel->rotlimits)
    {
        ragdollskel::rotlimit &r = skel->rotlimits[i];
        matrix3x3 rot;
        rot.transposemul(tris[r.tri[0]], r.middle);
        rot.mul(tris[r.tri[1]]);

        vec axis;
        float angle;
        if(!rot.calcangleaxis(angle, axis)) continue;
        angle = r.maxangle - fabs(angle);
        if(angle >= 0) continue;
        angle += 1e-3f;

        applyrotlimit(skel->tris[r.tri[0]], skel->tris[r.tri[1]], angle, axis);
    }
}

void ragdolldata::calcrotfriction()
{
    loopv(skel->rotfrictions)
    {
        ragdollskel::rotfriction &r = skel->rotfrictions[i];
        r.middle.multranspose(tris[r.tri[0]], tris[r.tri[1]]);
    }
}

void ragdolldata::applyrotfriction(float ts)
{
    calctris();
    float stopangle = 2*M_PI*ts*ragdollrotfricstop, rotfric = 1.0f - pow(ragdollrotfric, ts*1000.0f/ragdolltimestepmin);
    loopv(skel->rotfrictions)
    {
        ragdollskel::rotfriction &r = skel->rotfrictions[i];
        matrix3x3 rot;
        rot.transposemul(tris[r.tri[0]], r.middle);
        rot.mul(tris[r.tri[1]]);

        vec axis;
        float angle;
        if(rot.calcangleaxis(angle, axis))
        {
            angle *= -(fabs(angle) >= stopangle ? rotfric : 1.0f);
            applyrotlimit(skel->tris[r.tri[0]], skel->tris[r.tri[1]], angle, axis);
        }
    }
    loopv(skel->verts)
    {
        vert &v = verts[i];
        if(v.weight) v.pos = v.newpos.div(v.weight);
        v.newpos = vec(0, 0, 0);
        v.weight = 0;
    }
}

void ragdolldata::tryunstick(float speed)
{
    vec unstuck(0, 0, 0);
    int stuck = 0;
    loopv(skel->verts)
    {
        vert &v = verts[i];
        if(v.stuck)
        {
            if(collidevert(v.pos, vec(0, 0, 0), skel->verts[i].radius)) { stuck++; continue; }
            v.stuck = false;
        }
        unstuck.add(v.pos);
    }
    unsticks = 0;
    if(!stuck || stuck >= skel->verts.length()) return;
    unstuck.div(skel->verts.length() - stuck);
    loopv(skel->verts)
    {
        vert &v = verts[i];
        if(v.stuck)
        {
            v.pos.add(vec(unstuck).sub(v.pos).rescale(speed));
            unsticks++;
        }
    }
}

void ragdolldata::warppos(const vec &vel, const vec &offset)
{
    float ts = ragdolltimestepmin/1000.0f;
    vec frame = vec(vel).mul(ts);
    loopv(skel->verts)
    {
        vert &v = verts[i];
        v.oldpos = v.pos.add(offset);
        v.oldpos.sub(frame);
    }
    collidemillis = 0;
}

void ragdolldata::twitch(float vel)
{
    float ts = ragdolltimestepmin/1000.0f;
    loopv(skel->verts)
    {
        vert &v = verts[i];
        v.oldpos.add(vec(rnd(201)-100, rnd(201)-100, rnd(201)-100).div(100.f).normalize().mul(vel).mul(ts));
    }
    collidemillis = 0;
}

void ragdolldata::updatepos()
{
    loopv(skel->verts)
    {
        vert &v = verts[i];
        if(v.weight)
        {
            v.newpos.div(v.weight);
            if(!collidevert(v.newpos, vec(v.newpos).sub(v.pos), skel->verts[i].radius)) v.pos = v.newpos;
            else
            {
                vec dir = vec(v.newpos).sub(v.oldpos);
                if(dir.dot(collidewall) < 0) v.oldpos = vec(v.pos).sub(dir.reflect(collidewall));
                v.collided = true;
            }
        }
        v.newpos = vec(0, 0, 0);
        v.weight = 0;
    }
}

VAR(0, ragdollconstrain, 1, 5, 100);

void ragdolldata::constrain()
{
    loopi(ragdollconstrain)
    {
        constraindist();
        updatepos();

        calctris();
        constrainrot();
        updatepos();
    }
}

FVAR(0, ragdollbodyfric, 0, 0.95f, 1);
FVAR(0, ragdollbodyfricscale, 0, 2, 10);
FVAR(0, ragdollliquidfric, 0, 0.85f, 1);
FVAR(0, ragdollgroundfric, 0, 0.8f, 1);
FVAR(0, ragdollairfric, 0, 0.996f, 1);
FVAR(0, ragdollgravity, 0, 1, 1000);
FVAR(0, ragdollelasticity, 0, 1, 1000);
FVAR(0, ragdollunstick, 0, 10, 1e3f);
VAR(0, ragdollexpireoffset, 0, 1500, 30000);
VAR(0, ragdollliquidexpireoffset, 0, 3000, 30000);

void ragdolldata::move(dynent *pl, float ts)
{
    if(collidemillis && lastmillis > collidemillis) return;

    physics::updatematerial(pl, pl->center(), pl->feetpos(), true);
    float gravity = physics::gravityvel(pl)*ragdollgravity;

    calcrotfriction();
    float tsfric = timestep ? ts/timestep : 1,
          airfric = ragdollairfric + min((ragdollbodyfricscale*collisions)/skel->verts.length(), 1.0f)*(ragdollbodyfric - ragdollairfric);
    bool liquid = physics::liquidcheck(pl);
    collisions = 0;
    loopv(skel->verts)
    {
        vert &v = verts[i];
        vec dpos = vec(v.pos).sub(v.oldpos);
        dpos.z -= gravity*ts*ts;
        if(liquid) dpos.z += 0.25f*sinf(detrnd(size_t(this)+i, 360)*RAD + lastmillis/10000.0f*M_PI)*ts*pl->submerged;
        dpos.mul(pow((liquid ? physics::liquidmerge(pl, 1.f, ragdollliquidfric) : 1.f) * (v.collided ? ragdollgroundfric : airfric), ts*1000.0f/ragdolltimestepmin)*tsfric);
        v.oldpos = v.pos;
        v.pos.add(dpos);
    }
    applyrotfriction(ts);
    loopv(skel->verts)
    {
        vert &v = verts[i];
        //if(v.pos.z < 0) { v.pos.z = 0; v.oldpos = v.pos; collisions++; }
        vec dir = vec(v.pos).sub(v.oldpos);
        v.collided = collidevert(v.pos, dir, skel->verts[i].radius);
        if(v.collided)
        {
            v.pos = v.oldpos;
            v.oldpos.sub(dir.reflect(collidewall).mul(ragdollelasticity));
            collisions++;
        }
    }

    if(unsticks && ragdollunstick) tryunstick(ts*ragdollunstick);

    timestep = ts;
    if(collisions)
    {
        floating = 0;
        if(!collidemillis) collidemillis = lastmillis + (liquid ? ragdollliquidexpireoffset : ragdollexpireoffset);
    }
    else if(++floating > 1 && lastmillis < collidemillis) collidemillis = 0;

    constrain();
    calctris();
    calcboundsphere();
}

FVAR(0, ragdolleyesmooth, 0, 0.5f, 1);
VAR(0, ragdolleyesmoothmillis, 1, 50, 10000);

bool validragdoll(dynent *d, int millis)
{
    return d->ragdoll && d->ragdoll->millis >= millis;
}

void moveragdoll(dynent *d, bool smooth)
{
    if(!curtime || !d->ragdoll || (d->state != CS_DEAD && d->state != CS_WAITING)) return;

    if(!d->ragdoll->collidemillis || lastmillis < d->ragdoll->collidemillis)
    {
        int lastmove = d->ragdoll->lastmove;
        while(d->ragdoll->lastmove + (lastmove == d->ragdoll->lastmove ? ragdolltimestepmin : ragdolltimestepmax) <= lastmillis)
        {
            int timestep = min(ragdolltimestepmax, lastmillis - d->ragdoll->lastmove);
            d->ragdoll->move(d, timestep/1000.0f);
            d->ragdoll->lastmove += timestep;
        }
    }
    #if 0
    if(d->state == CS_DEAD || d->state == CS_WAITING)
    {
        vec eye = d->ragdoll->skel->eye >= 0 ? d->ragdoll->verts[d->ragdoll->skel->eye].pos : d->ragdoll->center;
        eye.add(d->ragdoll->offset);
        if(smooth)
        {
            float k = pow(ragdolleyesmooth, float(curtime)/ragdolleyesmoothmillis);
            d->o.mul(k).add(eye.mul(1-k));
        }
        else d->o = eye;
    }
    #endif
}

void cleanragdoll(dynent *d)
{
    DELETEP(d->ragdoll);
}

void warpragdoll(dynent *d, const vec &vel, const vec &offset)
{
    if(!d->ragdoll || (d->state != CS_DEAD && d->state != CS_WAITING)) return;
    d->ragdoll->warppos(vel, offset);
}

void twitchragdoll(dynent *d, float vel)
{
    if(!d->ragdoll || (d->state != CS_DEAD && d->state != CS_WAITING)) return;
    d->ragdoll->twitch(vel);
}

vec rdabove(dynent *d, float offset)
{
    if((d->type != ENT_PLAYER && d->type != ENT_AI) || !d->ragdoll || (d->state != CS_DEAD && d->state != CS_WAITING)) return d->physent::abovehead(offset);
    return vec(d->ragdoll->center).add(vec(0, 0, 1+d->aboveeye+offset+d->ragdoll->ztop));
}
vec rdbottom(dynent *d, float offset)
{
    if((d->type != ENT_PLAYER && d->type != ENT_AI) || !d->ragdoll || (d->state != CS_DEAD && d->state != CS_WAITING)) return d->physent::feetpos(offset);
    return vec(d->ragdoll->center).add(vec(0, 0, offset+d->ragdoll->zbottom));
}
vec rdtop(dynent *d, float offset)
{
    if((d->type != ENT_PLAYER && d->type != ENT_AI) || !d->ragdoll || (d->state != CS_DEAD && d->state != CS_WAITING)) return d->physent::headpos(offset);
    return vec(d->ragdoll->center).add(vec(0, 0, offset+d->ragdoll->ztop));
}
vec rdcenter(dynent *d)
{
    if((d->type != ENT_PLAYER && d->type != ENT_AI) || !d->ragdoll || (d->state != CS_DEAD && d->state != CS_WAITING)) return d->physent::center();
    return d->ragdoll->center;
}
