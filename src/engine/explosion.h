VAR(IDF_PERSIST, softexplosion, 0, 1, 1);
VAR(IDF_PERSIST, softexplosionblend, 1, 16, 64);

namespace sphere
{
    struct vert
    {
        vec pos;
        ushort s, t;
    } *verts = NULL;
    GLushort *indices = NULL;
    int numverts = 0, numindices = 0;
    GLuint vbuf = 0, ebuf = 0;

    void init(int slices, int stacks)
    {
        numverts = (stacks+1)*(slices+1);
        verts = new vert[numverts];
        float ds = 1.0f/slices, dt = 1.0f/stacks, t = 1.0f;
        loopi(stacks+1)
        {
            float rho = M_PI*(1-t), s = 0.0f, sinrho = i && i < stacks ? sin(rho) : 0, cosrho = !i ? 1 : (i < stacks ? cos(rho) : -1);
            loopj(slices+1)
            {
                float theta = j==slices ? 0 : 2*M_PI*s;
                vert &v = verts[i*(slices+1) + j];
                v.pos = vec(sin(theta)*sinrho, cos(theta)*sinrho, -cosrho);
                v.s = ushort(s*0xFFFF);
                v.t = ushort(t*0xFFFF);
                s += ds;
            }
            t -= dt;
        }

        numindices = (stacks-1)*slices*3*2;
        indices = new ushort[numindices];
        GLushort *curindex = indices;
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

        if(!vbuf) glGenBuffers_(1, &vbuf);
        gle::bindvbo(vbuf);
        glBufferData_(GL_ARRAY_BUFFER, numverts*sizeof(vert), verts, GL_STATIC_DRAW);
        DELETEA(verts);

        if(!ebuf) glGenBuffers_(1, &ebuf);
        gle::bindebo(ebuf);
        glBufferData_(GL_ELEMENT_ARRAY_BUFFER, numindices*sizeof(GLushort), indices, GL_STATIC_DRAW);
        DELETEA(indices);
    }

    void cleanup()
    {
        if(vbuf) { glDeleteBuffers_(1, &vbuf); vbuf = 0; }
        if(ebuf) { glDeleteBuffers_(1, &ebuf); ebuf = 0; }
    }
    void enable()
    {
        if(!vbuf) init(12, 6);

        gle::bindvbo(vbuf);
        gle::bindebo(ebuf);

        gle::vertexpointer(sizeof(vert), &verts->pos);
        gle::texcoord0pointer(sizeof(vert), &verts->s, GL_UNSIGNED_SHORT, 2, GL_TRUE);
        gle::enablevertex();
        gle::enabletexcoord0();
    }

    void draw()
    {
        glDrawRangeElements_(GL_TRIANGLES, 0, numverts-1, numindices, GL_UNSIGNED_SHORT, indices);
        xtraverts += numindices;
        glde++;
    }

    void disable()
    {
        gle::disablevertex();
        gle::disabletexcoord0();

        gle::clearvbo();
        gle::clearebo();
    }
}

static const float WOBBLE = 1.25f;

struct explosionrenderer : sharedlistrenderer
{
    explosionrenderer(const char *texname)
        : sharedlistrenderer(texname, 0, PT_EXPLOSION|PT_SHADER)
    {}

    void startrender()
    {
        if(softparticles && softexplosion) SETSHADER(explosionsoft);
        else SETSHADER(explosion);

        sphere::enable();
    }

    void endrender()
    {
        sphere::disable();
    }

    void cleanup()
    {
        sphere::cleanup();
    }

    void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int gravity)
    {
        pe.maxfade = max(pe.maxfade, fade);
        pe.extendbb(o, (size+1+pe.ent->attrs[1])*WOBBLE);
    }

    void renderpart(sharedlistparticle *p, int blend, int ts, float size)
    {
        float pmax = p->val,
              fsize = p->fade ? float(ts)/p->fade : 1,
              psize = p->size + pmax * fsize;

        if(isfoggedsphere(psize*WOBBLE, p->o)) return;

        vec dir = vec(p->o).sub(camera1->o), s, t;
        float dist = dir.magnitude();
        bool inside = dist <= psize*WOBBLE;
        if(inside)
        {
            s = camright;
            t = camup;
        }
        else
        {
            float mag2 = dir.magnitude2();
            dir.x /= mag2;
            dir.y /= mag2;
            dir.z /= dist;
            s = vec(dir.y, -dir.x, 0);
            t = vec(dir.x*dir.z, dir.y*dir.z, -mag2/dist);
        }

        matrix3 rot(lastmillis/1000.0f*143*RAD, vec(1/SQRT3, 1/SQRT3, 1/SQRT3));
        LOCALPARAM(texgenS, rot.transposedtransform(s));
        LOCALPARAM(texgenT, rot.transposedtransform(t));

        matrix4 m(rot, p->o);
        m.scale(psize, psize, inside ? -psize : psize);
        m.mul(camprojmatrix, m);
        LOCALPARAM(explosionmatrix, m);

        LOCALPARAM(center, p->o);
        LOCALPARAMF(blendparams, inside ? 0.5f : 4, inside ? 0.25f : 0);
        if(2*(p->size + pmax)*WOBBLE >= softexplosionblend)
        {
            LOCALPARAMF(softparams, -1.0f/softexplosionblend, 0, inside ? blend/(2*255.0f) : 0);
        }
        else
        {
            LOCALPARAMF(softparams, 0, -1, inside ? blend/(2*255.0f) : 0);
        }

        vec color = p->color.tocolor().mul(ldrscale);
        float alpha = blend/255.0f;

        loopi(inside ? 2 : 1)
        {
            gle::color(color, i ? alpha/2 : alpha);
            if(i) glDepthFunc(GL_GEQUAL);
            sphere::draw();
            if(i) glDepthFunc(GL_LESS);
        }
    }
};
static explosionrenderer explosions("<grey>particles/explosion"),
    shockwaves("<grey>particles/solid"), shockballs("<grey>particles/shockball");

