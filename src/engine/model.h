enum { MDL_MD2 = 0, MDL_MD3, MDL_MD5, MDL_OBJ, MDL_SMD, MDL_IQM, NUMMODELTYPES };

struct model
{
    char *name;
    float spinyaw, spinpitch, spinroll, offsetyaw, offsetpitch, offsetroll;
    bool collide, ellipsecollide, shadow, alphadepth, depthoffset;
    float scale;
    vec translate;
    BIH *bih;
    vec bbcenter, bbradius, bbextend, collidecenter, collideradius;
    float rejectradius, height, collidexyradius, collideheight;
    int batch;

    model(const char *name) : name(name ? newstring(name) : NULL), spinyaw(0), spinpitch(0), spinroll(0), offsetyaw(0), offsetpitch(0), offsetroll(0), collide(true), ellipsecollide(false), shadow(true), alphadepth(true), depthoffset(false), scale(1.0f), translate(0, 0, 0), bih(0), bbcenter(0, 0, 0), bbradius(-1, -1, -1), bbextend(0, 0, 0), collidecenter(0, 0, 0), collideradius(-1, -1, -1), rejectradius(-1), height(0.45f), collidexyradius(0), collideheight(0), batch(-1) {}
    virtual ~model() { DELETEA(name); DELETEP(bih); }
    virtual void calcbb(vec &center, vec &radius) = 0;
    virtual void render(int anim, int basetime, int basetime2, const vec &o, float yaw, float pitch, float roll, dynent *d, modelattach *a = NULL, const vec &color = vec(0, 0, 0), const vec &dir = vec(0, 0, 0), const bvec *material = NULL, float transparent = 1, float size = 1) = 0;
    virtual bool load() = 0;
    virtual int type() const = 0;
    virtual BIH *setBIH() { return 0; }
    virtual bool envmapped() { return false; }
    virtual bool skeletal() const { return false; }

    virtual void setshader(Shader *shader) {}
    virtual void setenvmap(float envmapmin, float envmapmax, Texture *envmap) {}
    virtual void setspec(float spec) {}
    virtual void setambient(float ambient) {}
    virtual void setglow(float glow, float glowdelta, float glowpulse) {}
    virtual void setglare(float specglare, float glowglare) {}
    virtual void setalphatest(float alpha) {}
    virtual void setalphablend(bool blend) {}
    virtual void setfullbright(float fullbright) {}
    virtual void setcullface(bool cullface) {}
    virtual void setmaterial(int material, int material2) {}

    virtual void preloadBIH() { if(!bih) setBIH(); }
    virtual void preloadshaders() {}
    virtual void preloadmeshes() {}
    virtual void cleanup() {}

    virtual void startrender() {}
    virtual void endrender() {}

    void boundbox(vec &center, vec &radius)
    {
        if(bbradius.x < 0)
        {
            calcbb(bbcenter, bbradius);
            bbradius.add(bbextend);
        }
        center = bbcenter;
        radius = bbradius;
    }

    float collisionbox(vec &center, vec &radius)
    {
        if(collideradius.x < 0)
        {
            boundbox(collidecenter, collideradius);
            if(collidexyradius)
            {
                collidecenter.x = collidecenter.y = 0;
                collideradius.x = collideradius.y = collidexyradius;
            }
            if(collideheight)
            {
                collidecenter.z = collideradius.z = collideheight/2;
            }
            rejectradius = collideradius.magnitude();
        }
        center = collidecenter;
        radius = collideradius;
        return rejectradius;
    }

    float boundsphere(vec &center)
    {
        vec radius;
        boundbox(center, radius);
        return radius.magnitude();
    }

    float above()
    {
        vec center, radius;
        boundbox(center, radius);
        return center.z+radius.z;
    }
};

