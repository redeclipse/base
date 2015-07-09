static struct spherevert
{
    vec pos;
    float s, t;
} *sphereverts = NULL;
static GLushort *sphereindices = NULL;
static int spherenumverts = 0, spherenumindices = 0;
static GLuint spherevbuf = 0, sphereebuf = 0;

static void initsphere(int slices, int stacks)
{
    DELETEA(sphereverts);
    spherenumverts = (stacks+1)*(slices+1);
    sphereverts = new spherevert[spherenumverts];
    float ds = 1.0f/slices, dt = 1.0f/stacks, t = 1.0f;
    loopi(stacks+1)
    {
        float rho = M_PI*(1-t), s = 0.0f, sinrho = i && i < stacks ? sin(rho) : 0, cosrho = !i ? 1 : (i < stacks ? cos(rho) : -1);
        loopj(slices+1)
        {
            float theta = j==slices ? 0 : 2*M_PI*s;
            spherevert &v = sphereverts[i*(slices+1) + j];
            v.pos = vec(-sin(theta)*sinrho, cos(theta)*sinrho, cosrho);
            v.s = s;
            v.t = t;
            s += ds;
        }
        t -= dt;
    }

    DELETEA(sphereindices);
    spherenumindices = (stacks-1)*slices*3*2;
    sphereindices = new ushort[spherenumindices];
    GLushort *curindex = sphereindices;
    loopi(stacks)
    {
        loopk(slices)
        {
            int j = i%2 ? slices-k-1 : k;
            if(i)
            {
                *curindex++ = i*(slices+1)+j;
                *curindex++ = (i+1)*(slices+1)+j;
                *curindex++ = i*(slices+1)+j+1;
            }
            if(i+1 < stacks)
            {
                *curindex++ = i*(slices+1)+j+1;
                *curindex++ = (i+1)*(slices+1)+j;
                *curindex++ = (i+1)*(slices+1)+j+1;
            }
        }
    }

    if(hasVBO)
    {
        if(!spherevbuf) glGenBuffers_(1, &spherevbuf);
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, spherevbuf);
        glBufferData_(GL_ARRAY_BUFFER_ARB, spherenumverts*sizeof(spherevert), sphereverts, GL_STATIC_DRAW_ARB);
        DELETEA(sphereverts);

        if(!sphereebuf) glGenBuffers_(1, &sphereebuf);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, sphereebuf);
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER_ARB, spherenumindices*sizeof(GLushort), sphereindices, GL_STATIC_DRAW_ARB);
        DELETEA(sphereindices);
    }
}

static void setupexplosion()
{
    if(glaring) SETSHADER(explosionglare);
    else if(!reflecting && !refracting && depthfx && depthfxtex.rendertex && numdepthfxranges>0)
    {
        if(depthfxtex.target==GL_TEXTURE_RECTANGLE_ARB)
        {
            if(!depthfxtex.highprecision()) SETSHADER(explosionsoft8rect);
            else SETSHADER(explosionsoftrect);
        }
        else
        {
            if(!depthfxtex.highprecision()) SETSHADER(explosionsoft8);
            else SETSHADER(explosionsoft);
        }
    }
    else SETSHADER(explosion);

    if(!sphereverts && !spherevbuf) initsphere(12, 6);

    if(hasVBO)
    {
       glBindBuffer_(GL_ARRAY_BUFFER_ARB, spherevbuf);
       glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, sphereebuf);
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(spherevert), &sphereverts->pos);
    glTexCoordPointer(2, GL_FLOAT, sizeof(spherevert), &sphereverts->s);
}

static void drawexpverts(int numverts, int numindices, GLushort *indices)
{
    if(hasDRE) glDrawRangeElements_(GL_TRIANGLES, 0, numverts-1, numindices, GL_UNSIGNED_SHORT, indices);
    else glDrawElements(GL_TRIANGLES, numindices, GL_UNSIGNED_SHORT, indices);
    xtraverts += numindices;
    glde++;
}

static void drawexplosion(bool inside, uchar r, uchar g, uchar b, uchar a)
{
    int passes = !reflecting && !refracting && inside ? 2 : 1;
    if(inside) glScalef(1, 1, -1);
    loopi(passes)
    {
        glColor4ub(r, g, b, i ? a/2 : a);
        if(i) glDepthFunc(GL_GEQUAL);
        drawexpverts(spherenumverts, spherenumindices, sphereindices);
        if(i) glDepthFunc(GL_LESS);
    }
}

static void cleanupexplosion()
{
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    if(hasVBO)
    {
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    }
}

static void deleteexplosions()
{
    if(spherevbuf) { glDeleteBuffers_(1, &spherevbuf); spherevbuf = 0; }
    if(sphereebuf) { glDeleteBuffers_(1, &sphereebuf); sphereebuf = 0; }
    DELETEA(sphereverts);
    DELETEA(sphereindices);
}

static const float WOBBLE = 1.25f;

struct explosionrenderer : sharedlistrenderer
{
    explosionrenderer(const char *texname)
        : sharedlistrenderer(texname, 0, PT_FIREBALL|PT_GLARE)
    {}

    void startrender()
    {
        setupexplosion();
    }

    void endrender()
    {
        cleanupexplosion();
        particleshader->set();
    }

    void cleanup()
    {
        deleteexplosions();
    }

    int finddepthfxranges(void **owners, float *ranges, int numranges, int maxranges, vec &bbmin, vec &bbmax)
    {
        static struct explosionent : physent
        {
            explosionent()
            {
                type = ENT_CAMERA;
            }
        } e;

        for(sharedlistparticle *p = list; p; p = p->next)
        {
            int ts = p->fade <= 5 ? 1 : lastmillis-p->millis;
            float pmax = p->val,
                  size = p->fade ? float(ts)/p->fade : 1,
                  psize = (p->size + pmax * size)*WOBBLE;
            if(2*(p->size + pmax)*WOBBLE < depthfxblend ||
               (!depthfxtex.highprecision() && !depthfxtex.emulatehighprecision() && psize > depthfxscale - depthfxbias) ||
               isfoggedsphere(psize, p->o)) continue;

            e.o = p->o;
            e.radius = e.xradius = e.yradius = e.height = e.aboveeye = psize;
            if(!::collide(&e, vec(0, 0, 0), 0, false)) continue;

            if(depthfxscissor==2 && !depthfxtex.addscissorbox(p->o, psize)) continue;

            vec dir = camera1->o;
            dir.sub(p->o);
            float dist = dir.magnitude();
            dir.mul(psize/dist).add(p->o);
            float depth = depthfxtex.eyedepth(dir);

            loopk(3)
            {
                bbmin[k] = min(bbmin[k], p->o[k] - psize);
                bbmax[k] = max(bbmax[k], p->o[k] + psize);
            }

            int pos = numranges;
            loopi(numranges) if(depth < ranges[i]) { pos = i; break; }
            if(pos >= maxranges) continue;

            if(numranges > pos)
            {
                int moved = min(numranges-pos, maxranges-(pos+1));
                memmove(&ranges[pos+1], &ranges[pos], moved*sizeof(float));
                memmove(&owners[pos+1], &owners[pos], moved*sizeof(void *));
            }
            if(numranges < maxranges) numranges++;

            ranges[pos] = depth;
            owners[pos] = p;
        }

        return numranges;
    }

    void renderpart(sharedlistparticle *p, int blend, int ts, float size, uchar *color)
    {
        float pmax = p->val,
              fsize = p->fade ? float(ts)/p->fade : 1,
              psize = size + pmax * fsize;
        int pblend = int(blend*p->blend);
        if(isfoggedsphere(psize*WOBBLE, p->o)) return;

        glPushMatrix();
        glTranslatef(p->o.x, p->o.y, p->o.z);

        bool inside = p->o.dist(camera1->o) <= psize*WOBBLE;
        vec oc(p->o);
        oc.sub(camera1->o);
        if(reflecting) oc.z = p->o.z - reflectz;

        float yaw = inside ? camera1->yaw : atan2(oc.y, oc.x)/RAD - 90,
        pitch = (inside ? camera1->pitch : asin(oc.z/oc.magnitude())/RAD) - 90;

        vec s(1, 0, 0), t(0, 1, 0);
        s.rotate(pitch*RAD, vec(-1, 0, 0));
        s.rotate(yaw*RAD, vec(0, 0, -1));
        t.rotate(pitch*RAD, vec(-1, 0, 0));
        t.rotate(yaw*RAD, vec(0, 0, -1));

        vec rotdir = vec(-1, 1, -1).normalize();
        s.rotate(-lastmillis/7.0f*RAD, rotdir);
        t.rotate(-lastmillis/7.0f*RAD, rotdir);

        setlocalparamf("texgenS", SHPARAM_VERTEX, 2, s.x, s.y, s.z);
        setlocalparamf("texgenT", SHPARAM_VERTEX, 3, t.x, t.y, t.z);

        vec center = vec(p->o).mul(0.015f);
        setlocalparamf("center", SHPARAM_VERTEX, 0, center.x, center.y, center.z);
        setlocalparamf("animstate", SHPARAM_VERTEX, 1, fsize, psize, pmax, float(lastmillis));
        setlocalparamf("blendparams", SHPARAM_PIXEL, 2, inside ? 0.5f : 4, inside ? 0.25f : 0);
        binddepthfxparams(depthfxblend, inside ? pblend/512.f : 0, 2*(size + pmax)*WOBBLE >= depthfxblend, p);

        glRotatef(lastmillis/7.0f, -rotdir.x, rotdir.y, -rotdir.z);
        glScalef(-psize, psize, -psize);
        drawexplosion(inside, color[0], color[1], color[2], pblend);

        glPopMatrix();
    }
};
static explosionrenderer explosions("<grey>particles/explosion"),
    shockwaves("<grey>particles/solid"), shockballs("<grey>particles/shockball");

