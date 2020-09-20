#include "cube.h"
#include "engine.h"
#include "fxprop.h"

namespace fx
{
    static slotmanager<fxdef> fxdefs;

    bool isfx(int index) { return fxdefs.inrange(index); }
    fxdef &getfxdef(int index) { return fxdefs[index]; }
    int getfxindex(const char *name) { return fxdefs.getindex(name); }
    slot *getfxslot(const char *name) { return fxdefs.getslot(name); }

    fxproperty::~fxproperty()
    {
        if(rand) delete rand;
        if(lerp) delete lerp;
    }

    fxdef *newfx;
    int newfxindex;
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
            "random", "lerp",
            "<invalid>"
        };

        return strings[min(mod, (int)FX_MODS)];
    }

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
        propertydef *defs;
        int num;
    } moddefs[FX_MODS] =
    {
        { NULL, 0 },
        { propdeflerp, FX_MOD_LERP_PROPS }
    };

    static float calclerptime(instance &inst, fxproperty &prop)
    {
        float t = 1.0f;
        int lerpmode = prop.lerp->props[FX_MOD_LERP_PROP_MODE].get<int>();

        if(lerpmode == FX_MOD_LERP_PARAM)
        {
            int lerpparam = prop.lerp->props[FX_MOD_LERP_PROP_PARAM].get<int>();
            t = inst.e->params[lerpparam];
        }
        else
        {
            int begin = prop.lerp->props[FX_MOD_LERP_PROP_MODE].get<int>() == FX_MOD_LERP_ACTIVE ?
                inst.e->beginmillis : inst.beginmillis;
            int end = begin + prop.lerp->props[FX_MOD_LERP_PROP_TIME].get<int>();

            t = float(lastmillis-begin) / float(end-begin);
        }

        t = clamp(t, 0.0f, 1.0f);
        if(prop.lerp->props[FX_MOD_LERP_PROP_SQUARE].get<int>()) t *= t;

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
            T lerptarget = prop.lerp->lerp.get<T>();
            if(def->modflags & BIT(FX_MOD_FLAG_LERP360)) val = lerp360(val, lerptarget, t);
            else val = lerp(val, lerptarget, t);
        }

        if(prop.rand)
            val += inst.e->rand.x * prop.rand->get<T>();

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
            T lerptarget = prop.lerp->lerp.get<T>();
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
            newfx->sound = gamesounds.getslot(soundname);
        }
    }

    static void fxregister(const char *name, int type, uint *code)
    {
        newfxindex = fxdefs.add(name);
        newfx = &getfxdef(newfxindex);
        newfx->name = fxdefs.getname(newfxindex);
        newfx->type = type;
        newfx->sound = newfx->endfx = NULL;
        newfx->children.shrink(0);

        if(fxdebug) conoutf("New FX registered: %s, index %d, type %s", name,
            newfxindex, fxtypestring(newfx->type));

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
            conoutf("\frError: %s, FX property %s modifier %d not supported",
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
            conoutf("\frError: %s, cannot set modifier property %s, not currently setting a modifier",
                newfx->getname(), name);
            return;
        }

        if(!lastfxprop)
        {
            conoutf("\frError: %s, no last property when setting modifier %s",
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
        }

        if(!modprops)
        {
            conoutf("\frError: %s, no FX %s modifier properties available", newfx->getname(), name);
            return;
        }

        property *modp = findprop(name, modprops, moddefs[curmodifier].num);

        if(!modp)
        {
            conoutf("\frError: %s, FX %s modifier property %s not found",
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
                    p.lerp->lerp.setdef(p.getdef());
                    initprops(p.lerp->props, moddefs[mod].defs, moddefs[mod].num);
                }

                p.lerp->lerp.set(value);
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
            conoutf("\frError: cannot assign property %s, not loading FX", name);
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
                    conoutf("\frError: %s, FX property %s not found", newfx->getname(), name);
                    return;
                }
            }

            if(mod) setfxmod(*p, mod-1, value, modcode);
            else p->set(value);

            lastfxprop = p;
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
            conoutf("\frError: cannot assign parent to %s, not loading FX",
                newfx->name);
            return;
        }

        int parentindex = getfxindex(name);

        if(parentindex < 0)
        {
            conoutf("\frError: cannot assign parent to %s, FX %s does not exist",
                newfx->name, name);
            return;
        }

        if(parentindex == newfxindex)
        {
            conoutf("\frError: cannot assign parent %s to itself",
                newfx->name);
            return;
        }

        fxdef &parent = getfxdef(parentindex);

        if(parent.children.find(newfxindex) < 0) parent.children.add(newfxindex);
        else
        {
            conoutf("\fyWarning: %s already assigned to parent %s", newfx->name, name);
            return;
        }
    }

    static void setfxend(const char *name)
    {
        if(!newfx)
        {
            conoutf("\frError: cannot assign end FX to %s, not loading FX",
                newfx->name);
            return;
        }

        newfx->endfx = fxdefs.getslot(name);
    }

    ICOMMAND(0, fxend, "s", (char *name),
        setfxend(name));
    ICOMMAND(0, fxparent, "s", (char *name),
        setfxparent(name));

    ICOMMAND(0, fxproptype, "si", (char *name, int *exttype),
        intret(getfxproptype(name, *exttype)));
    ICOMMAND(0, fxmodproptype, "si", (char *name, int *mod),
        intret(getfxmodproptype(name, *mod)));

    ICOMMAND(0, fxpropi, "siie", (char *name, int *ival, int *mod, uint *modcode),
        setfxprop(name, *mod, *ival, modcode))
    ICOMMAND(0, fxpropf, "sfie", (char *name, float *fval, int *mod, uint *modcode),
        setfxprop(name, *mod, *fval, modcode))
    ICOMMAND(0, fxpropc, "siiiie", (char *name, int *r, int *g, int *b, int *mod, uint *modcode),
        setfxprop(name, *mod, bvec(*r, *g, *b), modcode))
    ICOMMAND(0, fxpropiv, "siiiie", (char *name, int *x, int *y, int *z, int *mod, uint *modcode),
        setfxprop(name, *mod, ivec(*x, *y, *z), modcode))
    ICOMMAND(0, fxpropfv, "sfffie", (char *name, float *x, float *y, float *z, int *mod, uint *modcode),
        setfxprop(name, *mod, vec(*x, *y, *z), modcode))
    ICOMMAND(0, fxprops, "ssie", (char *name, char *str, int *mod, uint *modcode),
        setfxprop(name, *mod, str, modcode))
}
