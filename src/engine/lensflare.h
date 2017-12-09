static const struct flaretype
{
    int type;             /* flaretex index, 0..5, -1 for 6+random shine */
    float loc;            /* postion on axis */
    float scale;          /* texture scaling */
    uchar alpha;          /* color alpha */
} flaretypes[] =
{
    {2,  1.30f, 0.04f, 255}, //flares
    {3,  1.00f, 0.10f, 192},
    {1,  0.50f, 0.20f, 128},
    {3,  0.20f, 0.05f, 128},
    {0,  0.00f, 0.04f, 128},
    {5, -0.25f, 0.07f, 192},
    {5, -0.40f, 0.02f, 224},
    {5, -0.60f, 0.04f, 192},
    {5, -1.00f, 0.03f, 64},
    {-1, 1.00f, 0.30f, 255}, //shine - red, green, blue
    {-2, 1.00f, 0.20f, 255},
    {-3, 1.00f, 0.25f, 255}
};

struct flare
{
    vec o, center;
    float size;
    bvec color;
    int sparkle; // 0 = off, 1 = sparkles and flares, 2 = only sparkles
};

VAR(IDF_PERSIST, flarelights, 0, 1, 7); // 0 = off, &1 = defined lights, &2 = all lights, &4 = all sparkle
VAR(IDF_PERSIST, flarecutoff, 0, 1000, VAR_MAX);
VAR(IDF_PERSIST, flaresize, 1, 100, VAR_MAX);
VAR(IDF_PERSIST, flareshine, 1, 10, VAR_MAX);
FVAR(IDF_PERSIST, flareblend, 0, 0.5f, 1);
FVAR(IDF_PERSIST, flareadjust, 0, 0.7f, 1);

struct flarerenderer : partrenderer
{
    int maxflares, numflares;
    unsigned int shinetime;
    flare *flares;

    flarerenderer(const char *texname, int maxflares, int flags = 0)
        : partrenderer(texname, 3, PT_FLARE|PT_NOLAYER|flags), maxflares(maxflares), numflares(0), shinetime(0)
    {
        flares = new flare[maxflares];
    }
    ~flarerenderer()
    {
        delete[] flares;
    }

    void reset()
    {
        numflares = 0;
    }

    void newflare(const vec &o,  const vec &center, uchar r, uchar g, uchar b, float mod, float size, bool sun, int sparkle)
    {
        if(numflares >= maxflares) return;
        //occlusion check (neccessary as depth testing is turned off)
        vec dir = vec(camera1->o).sub(o);
        float dist = dir.magnitude();
        dir.mul(1/dist);
        if(raycube(o, dir, dist, RAY_CLIPMAT|RAY_POLY) < dist) return;
        flare &f = flares[numflares++];
        f.o = o;
        f.center = center;
        f.size = size;
        f.color = bvec(uchar(r*mod), uchar(g*mod), uchar(b*mod));
        f.sparkle = sparkle;
    }

    bool generate(const vec &o, vec &center, vec &flaredir, float &mod, float &size, bool sun, float radius)
    {
        //frustrum + fog check
        if(isvisiblesphere(0.0f, o) > (sun?VFC_FOGGED:VFC_FULL_VISIBLE)) return false;
        //find closest point between camera line of sight and flare pos
        flaredir = vec(o).sub(camera1->o);
        center = vec(camdir).mul(flaredir.dot(camdir)).add(camera1->o);
        if(sun) //fixed size
        {
            mod = 1.0;
            size = flaredir.magnitude() * flaresize / 100.0f;
        }
        else
        {
            mod = (flarecutoff-vec(o).sub(center).squaredlen())/flarecutoff;
            if(mod < 0.0f) return false;
            size = flaresize / 5.0f;
        }
        return true;
    }

    void addflare(const vec &o, uchar r, uchar g, uchar b, bool sun, int sparkle)
    {
        vec flaredir, center;
        float mod = 0, size = 0;
        if(generate(o, center, flaredir, mod, size, sun, sun ? 0.f : flarecutoff))
            newflare(o, center, r, g, b, mod, size, sun, sparkle);
    }

    void update()
    {
        numflares = 0; //regenerate flarelist each frame
        shinetime = lastmillis/flareshine;
    }

    void drawflares()
    {
        if(!flarelights) return;
        const vector<extentity *> &ents = entities::getents();
        loopenti(ET_LIGHT)
        {
            extentity &e = *ents[i];
            if(e.type != ET_LIGHT || (!(flarelights&2) && !(flarelights&1 && e.attrs[4])) || !checkmapvariant(e.attrs[9])) continue;
            bool sun = false;
            int sparkle = 0;
            uchar r = e.attrs[1], g = e.attrs[2], b = e.attrs[3];
            float scale = 1.f;
            if(!e.attrs[0] || e.attrs[4]&1) sun = true;
            if(!e.attrs[0] || e.attrs[4]&2 || flarelights&4) sparkle = sun ? 1 : 2;
            if(e.attrs[5] > 0) scale = e.attrs[5]/100.f;
            vec flaredir, center;
            float mod = 0, size = 0;
            if(generate(e.o, center, flaredir, mod, size, sun, sun ? 0.f : e.attrs[0]*flaresize/100.f))
                newflare(e.o, center, r, g, b, mod, size*scale, sun, sparkle);
        }
    }

    int count()
    {
        return numflares;
    }

    bool haswork()
    {
        return (numflares != 0);
    }

    void render()
    {
        glDisable(GL_DEPTH_TEST);
        glBindTexture(GL_TEXTURE_2D, tex->id);
        gle::defattrib(gle::ATTRIB_VERTEX, 3, GL_FLOAT);
        gle::defattrib(gle::ATTRIB_TEXCOORD0, 2, GL_FLOAT);
        gle::defattrib(gle::ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE);
        gle::begin(GL_QUADS);
        loopi(numflares)
        {
            const flare &f = flares[i];
            float blend = flareblend;
            vec axis = vec(f.o).sub(f.center);
            if(flareadjust > 0)
            {
                float yaw, pitch;
                vec dir = vec(f.o).sub(camera1->o).normalize();
                vectoyawpitch(dir, yaw, pitch);
                yaw -= camera1->yaw;
                while(yaw < -180.0f) yaw += 360.0f;
                while(yaw >= 180.0f) yaw -= 360.0f;
                if(yaw < 0) yaw = -yaw;
                blend *= 1-min(yaw/(curfov*0.5f)*flareadjust, 1.f);
                pitch -= camera1->pitch;
                while(pitch < -180.0f) pitch += 360.0f;
                while(pitch >= 180.0f) pitch -= 360.0f;
                if(pitch < 0) pitch = -pitch;
                blend *= 1-min(pitch/(fovy*0.5f)*flareadjust, 1.f);
            }
            bvec4 color(f.color, 255);
            loopj(f.sparkle ? (f.sparkle != 2 ? 12 : 3) : 9)
            {
                int q = f.sparkle != 2 ? j : j+9;
                const flaretype &ft = flaretypes[q];
                vec o = vec(axis).mul(ft.loc).add(f.center);
                float sz = ft.scale * f.size;
                int tex = ft.type;
                if(ft.type < 0) //sparkles - always done last
                {
                    shinetime = (shinetime + 1) % 10;
                    tex = 6+shinetime;
                    color.r = 0;
                    color.g = 0;
                    color.b = 0;
                    color[-ft.type-1] = f.color[-ft.type-1]; //only want a single channel
                }
                color.a = uchar(ceilf(ft.alpha*blend));
                const float tsz = 0.25f; //flares are aranged in 4x4 grid
                float tx = tsz*(tex&0x03), ty = tsz*((tex>>2)&0x03);
                gle::attribf(o.x+(-camright.x+camup.x)*sz, o.y+(-camright.y+camup.y)*sz, o.z+(-camright.z+camup.z)*sz);
                    gle::attribf(tx,     ty+tsz);
                    gle::attrib(color);
                gle::attribf(o.x+( camright.x+camup.x)*sz, o.y+( camright.y+camup.y)*sz, o.z+( camright.z+camup.z)*sz);
                    gle::attribf(tx+tsz, ty+tsz);
                    gle::attrib(color);
                gle::attribf(o.x+( camright.x-camup.x)*sz, o.y+( camright.y-camup.y)*sz, o.z+( camright.z-camup.z)*sz);
                    gle::attribf(tx+tsz, ty);
                    gle::attrib(color);
                gle::attribf(o.x+(-camright.x-camup.x)*sz, o.y+(-camright.y-camup.y)*sz, o.z+(-camright.z-camup.z)*sz);
                    gle::attribf(tx,     ty);
                    gle::attrib(color);
            }
        }
        gle::end();
        glEnable(GL_DEPTH_TEST);
    }

    //square per round hole - use addflare(..) instead
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, float blend = 1, int grav = 0, int collide = 0, physent *pl = NULL) { return NULL; }
};
static flarerenderer flares("<grey>particles/lensflares", 128);
