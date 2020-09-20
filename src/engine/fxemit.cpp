#include "cube.h"
#include "engine.h"

namespace fx
{
    static void calcdir(const vec &from, const vec &to, vec &dir, vec &up, vec &right)
    {
        dir = vec(to).sub(from).normalize();
        if(fabsf(dir.z) == 1.0f)
        {
            up = vec(0, dir.z, 0);
            right = vec(dir.z, 0, 0);
        }
        else
        {
            up = vec(0, 0, 1);
            right.cross(up, dir).normalize();
            up.cross(right, dir).normalize();
        }
    }

    static inline void offsetpos(vec &pos, const vec &offset, bool rel = false,
        const vec &dir = vec(0), const vec &up = vec(0), const vec &right = vec(0))
    {
        if(rel)
        {
            pos.madd(right, offset.x);
            pos.madd(dir, offset.y);
            pos.madd(up, offset.z);
        }
        else pos.add(offset);
    }

    static float getscale(instance &inst)
    {
        return inst.getprop<float>(FX_PROP_SCALE) * inst.e->scale;
    }

    static bool getpos(instance &inst, vec &from, vec &to, float scale)
    {
        bool hasoffset = false;
        bool endfromprev = inst.getprop<int>(FX_PROP_END_FROM_PREV);

        vec emit_from = inst.e->from;
        vec emit_to = endfromprev ? inst.e->prevfrom : inst.e->to;
        if(endfromprev) hasoffset = true;

        if(inst.getprop<int>(FX_PROP_POS_FLIP))
        {
            from = emit_to;
            to = emit_from;
            hasoffset = true;
        }
        else
        {
            from = emit_from;
            to = emit_to;
        }

        vec dir(0), up(0), right(0);
        vec fromoffset = inst.getprop<vec>(FX_PROP_POS_OFFSET);
        vec tooffset = inst.getprop<vec>(FX_PROP_END_OFFSET);
        vec endfrompos = inst.getprop<vec>(FX_PROP_END_FROM_POS);
        vec posfromend = inst.getprop<vec>(FX_PROP_POS_FROM_END);
        bool reloffset = inst.getprop<int>(FX_RPOP_REL_OFFSET);

        if(reloffset) calcdir(from, to, dir, up, right);

        if(!endfrompos.iszero())
        {
            to = from;
            tooffset.add(endfrompos);
        }
        else if(!posfromend.iszero())
        {
            from = to;
            fromoffset.add(posfromend);
        }

        if(!fromoffset.iszero())
        {
            offsetpos(from, fromoffset.mul(scale), reloffset, dir, up, right);
            hasoffset = true;
        }

        if(!tooffset.iszero())
        {
            offsetpos(to, tooffset.mul(scale), reloffset, dir, up, right);
            hasoffset = true;
        }

        return hasoffset;
    }

    static void calcto(instance &inst, const vec &from, vec &to, float length)
    {
        vec dir = vec(to).sub(from).normalize();
        to = from;
        to.add(dir.mul(length));
    }

    static float getblend(instance &inst)
    {
        float blend = inst.getprop<float>(FX_PROP_BLEND);
        int fadein = inst.getprop<int>(FX_PROP_FADEIN);
        int fadeout = inst.getprop<int>(FX_PROP_FADEOUT);
        int fadeoutbegin = inst.endmillis - fadeout;

        if(fadein)
            blend *= clamp(float(lastmillis - inst.beginmillis) / float(fadein), 0.0f, 1.0f);
        if(fadeout)
            blend *= 1.0f - clamp(float(lastmillis - fadeoutbegin) / float(fadeout), 0.0f, 1.0f);

        return blend * inst.e->blend;
    }

    static bvec getcolor(instance &inst, int colprop)
    {
        return inst.getprop<int>(FX_PROP_COLORIZED) ?
            inst.e->color : inst.getextprop<bvec>(colprop);
    }

    static void particlefx(instance &inst)
    {
        float scale = getscale(inst);
        vec from, to;
        bool hasoffset = getpos(inst, from, to, scale);

        float blend = getblend(inst);
        int color = getcolor(inst, FX_PART_COLOR).tohexcolor();

        // tracking not supported when using offsets
        physent *trackent = !hasoffset && inst.getextprop<int>(FX_PART_TRACK) ?
            inst.e->pl :
            NULL;

        switch(inst.getextprop<int>(FX_PART_TYPE))
        {
            case FX_PART_TYPE_SINGLE:
            {
                int regdelay = inst.getextprop<int>(FX_PART_REGDELAY);

                if(regdelay)
                    regular_part_create(
                        inst.getextprop<int>(FX_PART_PARTICLE),
                        inst.getextprop<int>(FX_PART_FADE),
                        from,
                        color,
                        inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                        blend,
                        inst.getextprop<float>(FX_PART_GRAVITY),
                        inst.getextprop<int>(FX_PART_COLLIDE),
                        trackent,
                        regdelay
                    );
                else
                    part_create(
                        inst.getextprop<int>(FX_PART_PARTICLE),
                        inst.getextprop<int>(FX_PART_FADE),
                        from,
                        color,
                        inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                        blend,
                        inst.getextprop<float>(FX_PART_GRAVITY),
                        inst.getextprop<int>(FX_PART_COLLIDE),
                        trackent
                    );
                break;
            }

            case FX_PART_TYPE_SPLASH:
            {
                int regdelay = inst.getextprop<int>(FX_PART_REGDELAY);

                if(regdelay)
                    regular_part_splash(
                        inst.getextprop<int>(FX_PART_PARTICLE),
                        inst.getextprop<int>(FX_PART_NUM),
                        inst.getextprop<int>(FX_PART_FADE),
                        from,
                        color,
                        inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                        blend,
                        inst.getextprop<float>(FX_PART_GRAVITY),
                        inst.getextprop<int>(FX_PART_COLLIDE),
                        inst.getextprop<float>(FX_PART_SHAPESIZE) * scale,
                        inst.getextprop<float>(FX_PART_VEL),
                        regdelay
                    );
                else
                    part_splash(
                        inst.getextprop<int>(FX_PART_PARTICLE),
                        inst.getextprop<int>(FX_PART_NUM),
                        inst.getextprop<int>(FX_PART_FADE),
                        from,
                        color,
                        inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                        blend,
                        inst.getextprop<float>(FX_PART_GRAVITY),
                        inst.getextprop<int>(FX_PART_COLLIDE),
                        inst.getextprop<float>(FX_PART_SHAPESIZE) * scale,
                        inst.getextprop<float>(FX_PART_VEL)
                    );
                break;
            }

            case FX_PART_TYPE_SHAPE:
                regularshape(
                    inst.getextprop<int>(FX_PART_PARTICLE),
                    inst.getextprop<float>(FX_PART_SHAPESIZE) * scale,
                    color,
                    inst.getextprop<int>(FX_PART_SHAPE),
                    inst.getextprop<int>(FX_PART_NUM),
                    inst.getextprop<int>(FX_PART_FADE),
                    from,
                    inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                    blend,
                    inst.getextprop<float>(FX_PART_GRAVITY),
                    inst.getextprop<int>(FX_PART_COLLIDE),
                    inst.getextprop<float>(FX_PART_VEL)
                );
                break;

            case FX_PART_TYPE_FLARE:
            {
                float flarelen = inst.getextprop<float>(FX_PART_SHAPESIZE);
                if(flarelen > 0)
                    calcto(inst, from, to, inst.getextprop<float>(FX_PART_SHAPESIZE) * scale);

                part_flare(
                    from,
                    to,
                    inst.getextprop<int>(FX_PART_FADE),
                    inst.getextprop<int>(FX_PART_PARTICLE),
                    color,
                    inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                    blend,
                    inst.getextprop<int>(FX_PART_GRAVITY),
                    inst.getextprop<int>(FX_PART_COLLIDE),
                    trackent
                );
                break;
            }

            case FX_PART_TYPE_TRAIL:
                part_trail(
                    inst.getextprop<int>(FX_PART_PARTICLE),
                    inst.getextprop<int>(FX_PART_FADE),
                    from,
                    to,
                    color,
                    inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                    blend,
                    inst.getextprop<int>(FX_PART_GRAVITY),
                    inst.getextprop<int>(FX_PART_COLLIDE)
                );
                break;

            case FX_PART_TYPE_EXPLODE:
                part_explosion(
                    from,
                    inst.getextprop<float>(FX_PART_MAXPARTSIZE) * scale,
                    inst.getextprop<int>(FX_PART_PARTICLE),
                    inst.getextprop<int>(FX_PART_FADE),
                    color,
                    inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                    blend,
                    inst.getextprop<int>(FX_PART_GRAVITY),
                    inst.getextprop<int>(FX_PART_COLLIDE),
                    trackent
                );
                break;

            case FX_PART_TYPE_TEXT:
                part_text(
                    from,
                    inst.getextprop<char *>(FX_PART_TEXT),
                    inst.getextprop<int>(FX_PART_PARTICLE),
                    inst.getextprop<int>(FX_PART_FADE),
                    color,
                    inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                    blend,
                    inst.getextprop<int>(FX_PART_GRAVITY),
                    inst.getextprop<int>(FX_PART_COLLIDE),
                    trackent
                );
                break;
        }
    }

    static void lightfx(instance &inst)
    {
        float scale = getscale(inst);
        vec from, to;
        getpos(inst, from, to, scale);

        float radius = inst.getextprop<float>(FX_LIGHT_RADIUS) * scale;
        vec color = getcolor(inst, FX_LIGHT_COLOR).mul(getblend(inst)).tocolor();

        adddynlight(from, radius, vec(color));
    }

    static void soundfx(instance &inst)
    {
        float scale = getscale(inst);
        vec from, to;
        getpos(inst, from, to, scale);

        int vol = inst.getextprop<int>(FX_SOUND_VOL) * getblend(inst);
        int sound = inst.soundhook;
        int flags = inst.getextprop<int>(FX_SOUND_FLAGS);

        if(!inst.emitted) // first emission
        {
            fxdef &def = getfxdef(inst.fxindex);

            playsound(
                def.sound->index,
                from,
                NULL,
                flags | SND_UNMAPPED,
                vol,
                inst.getextprop<int>(FX_SOUND_MAXRAD),
                inst.getextprop<int>(FX_SOUND_MINRAD),
                &inst.soundhook
            );
        }
        else if(issound(sound))
        {
            sounds[sound].vol = vol;
            sounds[sound].pos = from;
        }
    }

    static void windfx(instance &inst)
    {
        float scale = getscale(inst);
        vec from, to;
        getpos(inst, from, to, scale);

        float speed = inst.getextprop<float>(FX_WIND_SPEED) * getblend(inst);

        addwind(
            from,
            inst.getextprop<int>(FX_WIND_MODE),
            speed,
            &inst.windhook,
            inst.getextprop<int>(FX_WIND_YAW),
            inst.getextprop<int>(FX_WIND_INTERVAL),
            inst.getextprop<int>(FX_WIND_CYCLELEN),
            inst.getextprop<int>(FX_WIND_RADIUS),
            inst.getextprop<float>(FX_WIND_ATTEN)
        );
    }

    static void stainfx(instance &inst)
    {
        if(inst.emitted) return;

        float scale = getscale(inst);
        vec from, to, dir;
        getpos(inst, from, to, scale);

        if(to.isnormalized()) dir = to;
        else dir = vec(to).sub(from).safenormalize();

        float radius = inst.getextprop<float>(FX_STAIN_RADIUS) * scale;

        addstain(
            inst.getextprop<int>(FX_STAIN_TYPE),
            from,
            dir,
            radius,
            getcolor(inst, FX_STAIN_COLOR)
        );
    }

    void instance::emitfx()
    {
        fxdef &def = getfxdef(fxindex);

        switch(def.type)
        {
            case FX_TYPE_PARTICLE: particlefx(*this); break;
            case FX_TYPE_LIGHT: lightfx(*this); break;
            case FX_TYPE_SOUND: soundfx(*this); break;
            case FX_TYPE_WIND: windfx(*this); break;
            case FX_TYPE_STAIN: stainfx(*this); break;
        }
    }
}
