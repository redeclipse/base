#include "cube.h"
#include "engine.h"
#include "fxprop.h"

namespace fx
{
    static const struct
    {
        fxpropertydef *defs;
        int num;
    } extdefs[FX_TYPES] =
    {
        { propdefpart, FX_PART_PROPS },
        { propdeflight, FX_LIGHT_PROPS },
        { propdefsound, FX_SOUND_PROPS },
        { propdefwind, FX_WIND_PROPS },
        { propdefstain, FX_STAIN_PROPS }
    };

    static const struct
    {
        propertydef *moddef;
        propertydef *defs;
        int num;
    } moddefs[FX_MODS] =
    {
        { NULL, NULL, 0 },
        { NULL, propdeflerp, FX_MOD_LERP_PROPS },
        { &paramdef, propdefparam, FX_MOD_PARAM_PROPS }
    };

    static Slotmanager<fxdef> fxdefs;

    FxHandle getfxhandle(const char *name) { return fxdefs[name]; }
    bool hasfx(const char *name) { return fxdefs.hasslot(name); }

    fxproperty::~fxproperty()
    {
        if(rand) delete rand;
        if(lerp) delete lerp;
    }

    void fxproperty::pack(vector<uchar> &buf) const
    {
        static constexpr int EXTRA_RAND = 1;
        static constexpr int EXTRA_LERP = (1 << 1);
        static constexpr int EXTRA_PARAM = (1 << 2);

        property::pack(buf);

        uchar extra = 0;
        if(rand) extra |= EXTRA_RAND;
        if(lerp) extra |= EXTRA_LERP;
        if(param) extra |= EXTRA_PARAM;
        buf.put(extra);

        if(rand) rand->pack(buf);
        if(lerp) lerp->pack(buf);
        if(param) param->pack(buf);
    }

    int fxproperty::unpack(uchar *buf, size_t bufsize)
    {
        static constexpr int EXTRA_RAND = 1;
        static constexpr int EXTRA_LERP = (1 << 1);
        static constexpr int EXTRA_PARAM = (1 << 2);

        int bufread = property::unpack(buf, bufsize);
        if(bufread)
        {
            if(bufsize - bufread < 1)
            {
                conoutf(colourred, "Error unpacking fxprop '%s': not enough data!", def->name);
                return 0;
            }

            uchar extra = buf[bufread++];

            if(extra & EXTRA_RAND)
            {
                if(!rand)
                {
                    rand = new fxproperty;
                    rand->setdef(getdef());
                }

                int randbufread = rand->unpack(buf + bufread, bufsize - bufread);
                if(!randbufread) return 0;
                bufread += randbufread;
            }

            if(extra & EXTRA_LERP)
            {
                if(!lerp)
                {
                    lerp = new propmodlerp;
                    lerp->setdef(getdef());
                    initprops(lerp->props, moddefs[FX_MOD_LERP].defs, moddefs[FX_MOD_LERP].num);
                }

                int lerpbufread = lerp->unpack(buf + bufread, bufsize - bufread);
                if(!lerpbufread) return 0;
                bufread += lerpbufread;
            }

            if(extra & EXTRA_PARAM)
            {
                if(!param)
                {
                    param = new propmodparam;
                    param->setdef(moddefs[FX_MOD_PARAM].moddef);
                    initprops(param->props, moddefs[FX_MOD_PARAM].defs, moddefs[FX_MOD_PARAM].num);
                }

                int parambufread = param->unpack(buf + bufread, bufsize - bufread);
                if(!parambufread) return 0;
                bufread += parambufread;
            }
        }

        return bufread;
    }

    void propmodlerp::pack(vector<uchar> &buf) const
    {
        fxproperty::pack(buf);
        loopi(FX_MOD_LERP_PROPS) props[i].pack(buf);
    }

    int propmodlerp::unpack(uchar *buf, size_t bufsize)
    {
        int bufread = fxproperty::unpack(buf, bufsize);
        if(bufread) loopi(FX_MOD_LERP_PROPS)
        {
            int propbufread = props[i].unpack(buf + bufread, bufsize - bufread);
            if(!propbufread) return 0;
            bufread += propbufread;
        }

        return bufread;
    }

    void propmodparam::pack(vector<uchar> &buf) const
    {
        loopi(FX_MOD_PARAM_PROPS) props[i].pack(buf);
    }

    int propmodparam::unpack(uchar *buf, size_t bufsize)
    {
        int bufread = 0;
        loopi(FX_MOD_PARAM_PROPS)
        {
            int propbufread = props[i].unpack(buf + bufread, bufsize - bufread);
            if(!propbufread) return 0;
            bufread += propbufread;
        }

        return bufread;
    }

    fxdef *newfx;
    FxHandle newfxhandle;
    fxproperty *lastfxprop;
    static int curmodifier = -1;

    static const char *fxtypestring(int type)
    {
        static const char *strings[FX_TYPES + 1] =
        {
            "particle", "light", "sound", "wind", "stain",
            "<invalid>"
        };

        return strings[min(type, (int)FX_TYPES)];
    }

    static const char *fxmodstring(int mod)
    {
        static const char *strings[FX_MODS + 1] =
        {
            "random", "lerp", "parameter",
            "<invalid>"
        };

        return strings[min(mod, (int)FX_MODS)];
    }

    static float calclerptime(instance &inst, fxproperty &prop)
    {
        float t = 1.0f;
        int lerpmode = prop.lerp->props[FX_MOD_LERP_PROP_MODE].get<int>();

        switch(lerpmode)
        {
            case FX_MOD_LERP_SPEED:
                t = inst.e->from.dist(inst.e->prevfrom);
                break;
            case FX_MOD_LERP_CAMFACING:
            {
                vec dir = vec(inst.from).sub(inst.to).normalize();
                t = clamp(dir.dot(camdir), 0.0f, 1.0f);
                break;
            }
            case FX_MOD_LERP_ITER:
            {
                if(inst.iters > 1) t = float(inst.curiter) / float(inst.iters - 1);
                break;
            }
            case FX_MOD_LERP_PARAM:
            {
                int lerpparam = prop.lerp->props[FX_MOD_LERP_PROP_PARAM].get<int>();
                t = inst.e->params[lerpparam];
                break;
            }
            default:
            {
                int begin = prop.lerp->props[FX_MOD_LERP_PROP_MODE].get<int>() == FX_MOD_LERP_ACTIVE ?
                    inst.e->beginmillis : inst.beginmillis;
                int end = begin + prop.lerp->props[FX_MOD_LERP_PROP_TIME].get<int>();

                t = float(lastmillis-begin) / float(end-begin);
                break;
            }
        }

        float lerpmin = prop.lerp->props[FX_MOD_LERP_PROP_SCALEMIN].get<float>();
        float lerpmax = prop.lerp->props[FX_MOD_LERP_PROP_SCALEMAX].get<float>();

        // Scale by normalized range
        if(lerpmin != lerpmax)
            t = (t - lerpmin) / (lerpmax - lerpmin);

        t = clamp(t, 0.0f, 1.0f);

        switch(prop.lerp->props[FX_MOD_LERP_PROP_SHAPE].get<int>())
        {
            case FX_MOD_LERP_SHAPE_SQUARE_IN:
                t *= t;
                break;

            case FX_MOD_LERP_SHAPE_SQUARE_OUT:
                t = 1.0f - (1.0f - t) * (1.0f - t);
                break;

            case FX_MOD_LERP_SHAPE_SMOOTH:
                t = smoothinterp(t);
                break;
        }

        return t;
    }

    template<class T>
    static void calcmodifiers(instance &inst, fxproperty &prop, void *value)
    {
        T &val = *(T *)value;
        const fxpropertydef *def = prop.getdef();

        if(prop.lerp)
        {
            float t = calclerptime(inst, prop);
            T lerptarget = prop.lerp->get<T>();
            if(def->modflags & BIT(FX_MOD_FLAG_LERP360)) val = lerp360(val, lerptarget, t);
            else val = lerp(val, lerptarget, t);
        }

        if(prop.rand)
            val += inst.e->rand.x * prop.rand->get<T>();

        if(prop.param)
        {
            float scale = prop.param->props[FX_MOD_PARAM_PROP_SCALE].get<vec>().x;
            float param = inst.e->params[prop.param->get<int>()] * scale;

            switch(prop.param->props[FX_MOD_PARAM_PROP_MODE].get<int>())
            {
                case FX_MOD_PARAM_ADD:
                    val += param;
                    break;

                case FX_MOD_PARAM_MUL:
                    val *= param;
                    break;
            }
        }

        val = clamp(val, def->minval, def->maxval);
    }

    template<class T>
    static void calcmodifiersv(instance &inst, fxproperty &prop, void *value)
    {
        T &val = *(T *)value;
        const fxpropertydef *def = prop.getdef();

        if(prop.lerp)
        {
            float t = calclerptime(inst, prop);
            T lerptarget = prop.lerp->get<T>();
            if(def->modflags & BIT(FX_MOD_FLAG_LERP360))
            {
                val.x = lerp360(val.x, lerptarget.x, t);
                val.y = lerp360(val.y, lerptarget.y, t);
                val.z = lerp360(val.z, lerptarget.z, t);
            }
            else
            {
                val.x = lerp(val.x, lerptarget.x, t);
                val.y = lerp(val.y, lerptarget.y, t);
                val.z = lerp(val.z, lerptarget.z, t);
            }
        }

        if(prop.rand)
        {
            T rand = prop.rand->get<T>();
            val.add(rand.mul(inst.e->rand));
        }

        if(prop.param)
        {
            vec scale = prop.param->props[FX_MOD_PARAM_PROP_SCALE].get<vec>();
            float param = inst.e->params[prop.param->get<int>()];
            vec paramv = vec(param, param, param).mul(scale);

            switch(prop.param->props[FX_MOD_PARAM_PROP_MODE].get<int>())
            {
                case FX_MOD_PARAM_ADD:
                    val.add(T(paramv));
                    break;

                case FX_MOD_PARAM_MUL:
                    val.x *= paramv.x;
                    val.y *= paramv.y;
                    val.z *= paramv.z;
                    break;
            }
        }

        val = T(val).min(def->maxval.get<T>()).max(def->minval.get<T>());
    }

    static void resetdef()
    {
        initprops(newfx->props, propdefstd, FX_STD_PROPS);
        initprops(newfx->getextprops(), extdefs[newfx->type].defs, extdefs[newfx->type].num);
    }

    static void initdef()
    {
        if(newfx->type == FX_TYPE_SOUND)
        {
            const char *soundname = newfx->getextprops()[FX_SOUND_SOUND].get<char *>();
            newfx->sound = gamesounds[soundname];
        }
    }

    static void fxregister(const char *name, int type, uint *code)
    {
        if(newfx)
        {
            conoutf(colourred, "FX registration error: already registering %s, tried to register %s", newfx->name, name);
            return;
        }

        newfxhandle = fxdefs.add(name);
        newfx = &newfxhandle.get();
        newfx->name = fxdefs.getname(newfxhandle);
        newfx->type = type;
        newfx->sound = SoundHandle();
        newfx->children.shrink(0);

        if(fxdebug) conoutf(colourwhite, "New FX registered: %s, index %d, type %s", name,
            newfxhandle.getindex(), fxtypestring(newfx->type));

        resetdef();
        execute(code);
        initdef();

        newfx = NULL;
        lastfxprop = NULL;
        curmodifier = -1;
    }

    ICOMMAND(0, registerfx, "sie", (char *name, int *type, uint *code),
        fxregister(name, *type, code));

    static void assignmodfun(fxproperty &p)
    {
        switch(p.getdef()->type)
        {
            case PROP_INT: p.calcmodifiers = calcmodifiers<int>; break;
            case PROP_FLOAT: p.calcmodifiers = calcmodifiers<float>; break;
            case PROP_COLOR: p.calcmodifiers = calcmodifiersv<bvec>; break;
            case PROP_IVEC: p.calcmodifiers = calcmodifiersv<ivec>; break;
            case PROP_FVEC: p.calcmodifiers = calcmodifiersv<vec>; break;
        }
    }

    static bool fxmodcheck(fxproperty &p, int mod)
    {
        if(!(p.getdef()->modflags & BIT(mod)))
        {
            conoutf(colourred, "Error: %s, FX property %s modifier %d not supported",
                newfx->getname(), p.getdef()->name, mod);

            return false;
        }

        return true;
    }

    template<class T>
    static void setfxmodprop(const char *name, const T &value)
    {
        if(curmodifier < 0)
        {
            conoutf(colourred, "Error: %s, cannot set modifier property %s, not currently setting a modifier",
                newfx->getname(), name);
            return;
        }

        if(!lastfxprop)
        {
            conoutf(colourred, "Error: %s, no last property when setting modifier %s",
                newfx->getname(), name);
            return;
        }

        if(!fxmodcheck(*lastfxprop, curmodifier)) return;

        property *modprops = NULL;

        switch(curmodifier)
        {
            case FX_MOD_LERP:
                if(lastfxprop->lerp) modprops = lastfxprop->lerp->props;
                break;

            case FX_MOD_PARAM:
                if(lastfxprop->param) modprops = lastfxprop->param->props;
                break;
        }

        if(!modprops)
        {
            conoutf(colourred, "Error: %s, no FX %s modifier properties available", newfx->getname(), name);
            return;
        }

        property *modp = findprop(name, modprops, moddefs[curmodifier].num);

        if(!modp)
        {
            conoutf(colourred, "Error: %s, FX %s modifier property %s not found",
                newfx->getname(), fxmodstring(curmodifier), name);
            return;
        }

        modp->set(value);
    }

    template<class T>
    static void setfxmod(fxproperty &p, int mod, const T &value, uint *code)
    {
        if(!fxmodcheck(p, mod)) return;

        if(!p.calcmodifiers) assignmodfun(p);

        switch(mod)
        {
            case FX_MOD_RAND:
                if(!p.rand)
                {
                    p.rand = new fxproperty;
                    p.rand->setdef(p.getdef());
                }

                p.rand->set(value);
                break;

            case FX_MOD_LERP:
                if(!p.lerp)
                {
                    p.lerp = new propmodlerp;
                    p.lerp->setdef(p.getdef());
                    initprops(p.lerp->props, moddefs[mod].defs, moddefs[mod].num);
                }

                p.lerp->set(value);
                break;

            case FX_MOD_PARAM:
                if(!p.param)
                {
                    p.param = new propmodparam;
                    p.param->setdef(moddefs[mod].moddef);
                    initprops(p.param->props, moddefs[mod].defs, moddefs[mod].num);
                }

                p.param->set(value);
                break;
        }

        curmodifier = mod;
        execute(code);
        curmodifier = -1;
    }

    template<class T>
    static void setfxprop(const char *name, int mod, const T &value, uint *modcode)
    {
        if(!newfx)
        {
            conoutf(colourred, "Error: cannot assign property %s, not loading FX", name);
            return;
        }

        if(curmodifier >= 0) setfxmodprop(name, value);
        else
        {
            fxproperty *p = findprop(name, newfx->props, FX_STD_PROPS);
            if(!p)
            {
                p = findprop(name, newfx->getextprops(), extdefs[newfx->type].num);
                if(!p)
                {
                    conoutf(colourred, "Error: %s, FX property %s not found", newfx->getname(), name);
                    return;
                }
            }

            lastfxprop = p;

            if(mod) setfxmod(*p, mod-1, value, modcode);
            else p->set(value);
        }

    }

    static int getfxproptype(const char *name, int exttype)
    {
        fxpropertydef *def = findpropdef(name, propdefstd, FX_STD_PROPS);
        if(!def) def = findpropdef(name, extdefs[exttype].defs, extdefs[exttype].num);

        return def ? def->type : -1;
    }

    static int getfxmodproptype(const char *name, int mod)
    {
        propertydef *def = NULL;

        mod--; // switch to 0 indexing
        def = findpropdef(name, moddefs[mod].defs, moddefs[mod].num);

        return def ? def->type : -1;
    }

    static void setfxparent(const char *name)
    {
        if(!newfx)
        {
            conoutf(colourred, "Error: cannot assign parent to %s, not loading FX",
                newfx->name);
            return;
        }

        FxHandle parenthandle = fxdefs[name];

        if(!parenthandle.isvalid())
        {
            conoutf(colourred, "Error: cannot assign parent to %s, FX %s does not exist",
                newfx->name, name);
            return;
        }

        if(parenthandle == newfxhandle)
        {
            conoutf(colourred, "Error: cannot assign parent %s to itself",
                newfx->name);
            return;
        }

        fxdef &parent = parenthandle.get();

        if(parent.children.find(newfxhandle) < 0) parent.children.add(newfxhandle);
        else
        {
            conoutf(colouryellow, "Warning: %s already assigned to parent %s", newfx->name, name);
            return;
        }
    }

    static void setfxend(const char *name)
    {
        if(!newfx)
        {
            conoutf(colourred, "Error: cannot assign end FX to %s, not loading FX",
                newfx->name);
            return;
        }

        newfx->endfx = fxdefs[name];
    }

    ICOMMAND(0, fxcleardefs, "", (),
    {
        clear();
        fxdefs.clear();
    });

    ICOMMAND(0, fxend, "s", (char *name),
        setfxend(name));
    ICOMMAND(0, fxparent, "s", (char *name),
        setfxparent(name));

    ICOMMAND(0, fxproptype, "si", (char *name, int *exttype),
        intret(getfxproptype(name, *exttype)));
    ICOMMAND(0, fxmodproptype, "si", (char *name, int *mod),
        intret(getfxmodproptype(name, *mod)));

    ICOMMAND(0, fxpropi, "siie", (char *name, int *ival, int *mod, uint *modcode),
        setfxprop(name, *mod, *ival, modcode));
    ICOMMAND(0, fxpropf, "sfie", (char *name, float *fval, int *mod, uint *modcode),
        setfxprop(name, *mod, *fval, modcode));
    ICOMMAND(0, fxpropc, "siiiie", (char *name, int *r, int *g, int *b, int *mod, uint *modcode),
        setfxprop(name, *mod, bvec(*r, *g, *b), modcode));
    ICOMMAND(0, fxpropiv, "siiiie", (char *name, int *x, int *y, int *z, int *mod, uint *modcode),
        setfxprop(name, *mod, ivec(*x, *y, *z), modcode));
    ICOMMAND(0, fxpropfv, "sfffie", (char *name, float *x, float *y, float *z, int *mod, uint *modcode),
        setfxprop(name, *mod, vec(*x, *y, *z), modcode));
    ICOMMAND(0, fxprops, "ssie", (char *name, char *str, int *mod, uint *modcode),
        setfxprop(name, *mod, str, modcode));
}
