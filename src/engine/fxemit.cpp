#include "cube.h"
#include "engine.h"

namespace fx
{
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
        int colorized = inst.getprop<int>(FX_PROP_COLORIZED);

        switch(colorized)
        {
            case FX_COLORIZE_DISABLED:
                return inst.getextprop<bvec>(colprop);

            case FX_COLORIZE_PARAM:
            {
                int weap = inst.getprop<int>(FX_PROP_WEAPON);
                if(weap >= 0) return bvec(game::getweapcolor(weap));
                else return inst.e->color;
            }

            case FX_COLORIZE_PALETTE:
                return bvec(pulsehexcol(inst.getprop<int>(FX_PROP_PALETTE)));

            default:
                ASSERT(0); // Not possible unless clamping on the colorized property is broken
        }

        // Will never happen, leaving it a as a decoration
        return inst.getextprop<bvec>(colprop);
    }

    static void particlefx(instance &inst)
    {
        float scale = inst.getscale();

        float blend = getblend(inst);
        int color = getcolor(inst, FX_PART_COLOR).tohexcolor(),
            envcolor = getcolor(inst, FX_PART_ENVCOLOR).tohexcolor();

        // Particle stains need to be offset by 1
        int collidestain = inst.getextprop<int>(FX_PART_COLLIDE) + 1;

        // particle tracking not supported when overriding positions
        physent *trackent = inst.canparttrack && inst.getextprop<int>(FX_PART_TRACK) ?
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
                        inst.from,
                        color,
                        inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                        blend,
                        inst.getextprop<bvec>(FX_PART_HINTCOLOR).tohexcolor(),
                        inst.getextprop<float>(FX_PART_HINTBLEND),
                        inst.getextprop<float>(FX_PART_GRAVITY),
                        collidestain,
                        trackent,
                        regdelay,
                        inst.getextprop<float>(FX_PART_PARTSIZECHANGE) * scale,
                        envcolor,
                        inst.getextprop<float>(FX_PART_ENVBLEND)
                    );
                else
                    part_create(
                        inst.getextprop<int>(FX_PART_PARTICLE),
                        inst.getextprop<int>(FX_PART_FADE),
                        inst.from,
                        color,
                        inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                        blend,
                        inst.getextprop<bvec>(FX_PART_HINTCOLOR).tohexcolor(),
                        inst.getextprop<float>(FX_PART_HINTBLEND),
                        inst.getextprop<float>(FX_PART_GRAVITY),
                        collidestain,
                        trackent,
                        inst.getextprop<float>(FX_PART_PARTSIZECHANGE) * scale,
                        envcolor,
                        inst.getextprop<float>(FX_PART_ENVBLEND)
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
                        inst.from,
                        color,
                        inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                        blend,
                        inst.getextprop<bvec>(FX_PART_HINTCOLOR).tohexcolor(),
                        inst.getextprop<float>(FX_PART_HINTBLEND),
                        inst.getextprop<float>(FX_PART_GRAVITY),
                        collidestain,
                        inst.getextprop<float>(FX_PART_SHAPESIZE) * scale,
                        inst.getextprop<float>(FX_PART_VEL),
                        regdelay,
                        inst.getextprop<float>(FX_PART_PARTSIZECHANGE) * scale,
                        envcolor,
                        inst.getextprop<float>(FX_PART_ENVBLEND)
                    );
                else
                    part_splash(
                        inst.getextprop<int>(FX_PART_PARTICLE),
                        inst.getextprop<int>(FX_PART_NUM),
                        inst.getextprop<int>(FX_PART_FADE),
                        inst.from,
                        color,
                        inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                        blend,
                        inst.getextprop<bvec>(FX_PART_HINTCOLOR).tohexcolor(),
                        inst.getextprop<float>(FX_PART_HINTBLEND),
                        inst.getextprop<float>(FX_PART_GRAVITY),
                        collidestain,
                        inst.getextprop<float>(FX_PART_SHAPESIZE) * scale,
                        inst.getextprop<float>(FX_PART_VEL),
                        inst.getextprop<float>(FX_PART_PARTSIZECHANGE) * scale,
                        envcolor,
                        inst.getextprop<float>(FX_PART_ENVBLEND)
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
                    inst.from,
                    inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                    blend,
                    inst.getextprop<bvec>(FX_PART_HINTCOLOR).tohexcolor(),
                    inst.getextprop<float>(FX_PART_HINTBLEND),
                    inst.getextprop<float>(FX_PART_GRAVITY),
                    collidestain,
                    inst.getextprop<float>(FX_PART_VEL),
                    inst.getextprop<float>(FX_PART_PARTSIZECHANGE) * scale,
                    envcolor,
                    inst.getextprop<float>(FX_PART_ENVBLEND)
                );
                break;

            case FX_PART_TYPE_FLARE:
            {
                float flarelen = inst.getextprop<float>(FX_PART_SHAPESIZE);
                if(flarelen > 0)
                    calcto(inst, inst.from, inst.to, inst.getextprop<float>(FX_PART_SHAPESIZE) * scale);

                part_flare(
                    inst.from,
                    inst.to,
                    inst.getextprop<int>(FX_PART_FADE),
                    inst.getextprop<int>(FX_PART_PARTICLE),
                    color,
                    inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                    blend,
                    inst.getextprop<bvec>(FX_PART_HINTCOLOR).tohexcolor(),
                    inst.getextprop<float>(FX_PART_HINTBLEND),
                    inst.getextprop<int>(FX_PART_GRAVITY),
                    collidestain,
                    trackent,
                    inst.getextprop<float>(FX_PART_PARTSIZECHANGE) * scale,
                    envcolor,inst.getextprop<float>(FX_PART_ENVBLEND)
                );
                break;
            }

            case FX_PART_TYPE_TRAIL:
                part_trail(
                    inst.getextprop<int>(FX_PART_PARTICLE),
                    inst.getextprop<int>(FX_PART_FADE),
                    inst.from,
                    inst.to,
                    color,
                    inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                    blend,
                    inst.getextprop<bvec>(FX_PART_HINTCOLOR).tohexcolor(),
                    inst.getextprop<float>(FX_PART_HINTBLEND),
                    inst.getextprop<int>(FX_PART_GRAVITY),
                    collidestain,
                    inst.getextprop<float>(FX_PART_PARTSIZECHANGE) * scale,
                    inst.getextprop<float>(FX_PART_VEL),
                    inst.getextprop<float>(FX_PART_SHAPESIZE) * (1.0f / scale),
                    envcolor,
                    inst.getextprop<float>(FX_PART_ENVBLEND)
                );
                break;

            case FX_PART_TYPE_EXPLODE:
                part_explosion(
                    inst.from,
                    inst.getextprop<float>(FX_PART_MAXPARTSIZE) * scale,
                    inst.getextprop<int>(FX_PART_PARTICLE),
                    inst.getextprop<int>(FX_PART_FADE),
                    color,
                    inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                    blend,
                    inst.getextprop<bvec>(FX_PART_HINTCOLOR).tohexcolor(),
                    inst.getextprop<float>(FX_PART_HINTBLEND),
                    inst.getextprop<int>(FX_PART_GRAVITY),
                    collidestain,
                    trackent
                );
                break;

            case FX_PART_TYPE_TEXT:
                part_text(
                    inst.from,
                    inst.getextprop<char *>(FX_PART_TEXT),
                    inst.getextprop<int>(FX_PART_PARTICLE),
                    inst.getextprop<int>(FX_PART_FADE),
                    color,
                    inst.getextprop<float>(FX_PART_PARTSIZE) * scale,
                    blend,
                    inst.getextprop<int>(FX_PART_GRAVITY),
                    collidestain,
                    trackent,
                    inst.getextprop<float>(FX_PART_PARTSIZECHANGE) * scale,
                    envcolor,
                    inst.getextprop<float>(FX_PART_ENVBLEND)
                );
                break;
        }
    }

    static void lightfx(instance &inst)
    {
        float scale = inst.getscale();

        float gain = inst.getextprop<float>(FX_LIGHT_GAIN) * getblend(inst);
        float radius = inst.getextprop<float>(FX_LIGHT_RADIUS) * scale;
        vec color = getcolor(inst, FX_LIGHT_COLOR).tocolor().mul(gain);
        int flags = inst.getextprop<int>(FX_LIGHT_FLAGS);
        int spot = inst.getextprop<int>(FX_LIGHT_SPOT);

        vec dir = vec(inst.to).sub(inst.from).normalize();

        adddynlight(inst.from, radius, vec(color), 0, 0, flags, 0.0f, vec(0, 0, 0), NULL,
            spot > 0 ? dir : vec(0, 0, 0), spot);
    }

    static void soundfx(instance &inst)
    {
        bool onplayer = inst.getextprop<int>(FX_SOUND_ONPLAYER) && inst.e->pl;
        float gain = inst.getextprop<float>(FX_SOUND_GAIN) * getblend(inst);
        int sound = inst.soundhook;

        if(!inst.emitted) // first emission
        {
            fxdef &def = inst.fxhandle.get();

            int extraflags = 0;
            int flags = inst.getextprop<int>(FX_SOUND_FLAGS);

            int soundindex = def.sound.getindex();
            int weap = inst.getprop<int>(FX_PROP_WEAPON);
            int weapsound = inst.getextprop<int>(FX_SOUND_WEAPONSOUND);

            if(weap >= 0 && weapsound >= 0) soundindex = game::getweapsound(weap, weapsound);
            else extraflags |= SND_UNMAPPED;

            physent *ent = NULL;
            vec *soundpos = NULL;

            if(onplayer)
            {
                soundpos = game::getplayersoundpos(inst.e->pl);
                ent = inst.e->pl;
            }
            else
            {
                soundpos = &inst.from;
                extraflags |= SND_VELEST;
            }

            emitsound(
                soundindex,
                soundpos,
                ent,
                &inst.soundhook,
                flags | extraflags,
                max(gain, 0.00001f),
                inst.getextprop<float>(FX_SOUND_PITCH),
                inst.getextprop<float>(FX_SOUND_ROLLOFF),
                inst.getextprop<float>(FX_SOUND_REFDIST),
                inst.getextprop<float>(FX_SOUND_MAXDIST)
            );
        }
        else if(issound(sound))
        {
            soundsources[sound].gain = gain;
            if(!onplayer) soundsources[sound].pos = inst.from;
        }
    }

    static void windfx(instance &inst)
    {
        float speed = inst.getextprop<float>(FX_WIND_SPEED) * getblend(inst);

        addwind(
            inst.from,
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

        vec dir;

        if(inst.to.isnormalized()) dir = inst.to;
        else dir = vec(inst.to).sub(inst.from).safenormalize();

        float radius = inst.getextprop<float>(FX_STAIN_RADIUS) * inst.getscale();

        addstain(
            inst.getextprop<int>(FX_STAIN_TYPE),
            inst.from,
            dir,
            radius,
            getcolor(inst, FX_STAIN_COLOR), 0,
            getcolor(inst, FX_STAIN_ENVCOLOR), inst.getextprop<float>(FX_STAIN_ENVBLEND)
        );
    }

    void instance::emitfx()
    {
        fxdef &def = fxhandle.get();

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
