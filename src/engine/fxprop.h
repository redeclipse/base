#include "cube.h"

namespace fx
{
    static propertydef propdeflerp[FX_MOD_LERP_PROPS] =
    {
        propertydef(
            "lerptime",
            PROP_INT,
            1, 1000, INT_MAX
        ),
        propertydef(
            "lerpmode",
            PROP_INT,
            (int)FX_MOD_LERP_ACTIVE, (int)FX_MOD_LERP_ACTIVE, (int)FX_MOD_LERP_MODES - 1
        ),
        propertydef(
            "lerpparam",
            PROP_INT,
            0, 0, FX_PARAMS
        ),
        propertydef(
            "lerpshape",
            PROP_INT,
            FX_MOD_LERP_SHAPE_LINEAR, FX_MOD_LERP_SHAPE_LINEAR, FX_MOD_LERP_SHAPES
        ),
        propertydef(
            "lerpscalemin",
            PROP_FLOAT,
            -FLT_MAX, 0.0f, FLT_MAX
        ),
        propertydef(
            "lerpscalemax",
            PROP_FLOAT,
            -FLT_MAX, 0.0f, FLT_MAX
        )
    };

    static propertydef paramdef = propertydef(
        "param",
        PROP_INT,
        0, 0, FX_PARAMS
    );

    static propertydef propdefparam[FX_MOD_PARAM_PROPS] =
    {
        propertydef(
            "parammode",
            PROP_INT,
            FX_MOD_PARAM_ADD, FX_MOD_PARAM_ADD, FX_MOD_PARAM_MODES
        ),
        propertydef(
            "paramscale",
            PROP_FVEC,
            vec(-FLT_MAX, -FLT_MAX, -FLT_MAX), vec(1, 1, 1), vec(FLT_MAX, FLT_MAX, FLT_MAX)
        )
    };

    static fxpropertydef propdefstd[FX_STD_PROPS] =
    {
        fxpropertydef(
            "activelen",
            PROP_INT,
            1, 1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "emitlen",
            PROP_INT,
            1, 1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "emitinterval",
            PROP_INT,
            1, 1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "emitdelay",
            PROP_INT,
            0, 0, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "emitparent",
            PROP_INT,
            0, 0, 1,
            0
        ),
        fxpropertydef(
            "emittimeliness",
            PROP_INT,
            0, 1, 1,
            0
        ),
        fxpropertydef(
            "emitsingle",
            PROP_INT,
            0, 0, 1,
            0
        ),
        fxpropertydef(
            "emitrestart",
            PROP_INT,
            0, 0, 1,
            0
        ),
        fxpropertydef(
            "emitmove",
            PROP_FLOAT,
            0.0f, 0.0f, FLT_MAX,
            0
        ),
        fxpropertydef(
            "emitparam",
            PROP_INT,
            -1, -1, FX_PARAMS,
            0
        ),
        fxpropertydef(
            "emitdist",
            PROP_FLOAT,
            0.0f, 0.0f, FLT_MAX,
            0
        ),
        fxpropertydef(
            "emitcull",
            PROP_FLOAT,
            0.0f, 0.0f, FLT_MAX,
            0
        ),
        fxpropertydef(
            "fadein",
            PROP_INT,
            0, 0, INT_MAX,
            0
        ),
        fxpropertydef(
            "fadeout",
            PROP_INT,
            0, 0, INT_MAX,
            0
        ),
        fxpropertydef(
            "blend",
            PROP_FLOAT,
            0.0f, 1.0f, 1.0f,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "scale",
            PROP_FLOAT,
            0.0f, 1.0f, 1.0f,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "colorized",
            PROP_INT,
            FX_COLORIZE_DISABLED, FX_COLORIZE_DISABLED, FX_COLORIZE_PALETTE,
            0
        ),
        fxpropertydef(
            "palette",
            PROP_INT,
            0, 0, (int)PULSE_MAX,
            0
        ),
        fxpropertydef(
            "reloffset",
            PROP_INT,
            0, 1, 1,
            0
        ),
        fxpropertydef(
            "posoffset",
            PROP_FVEC,
            vec(-FLT_MAX), vec(0.0f), vec(FLT_MAX),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "endoffset",
            PROP_FVEC,
            vec(-FLT_MAX), vec(0.0f), vec(FLT_MAX),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "endfrompos",
            PROP_FVEC,
            vec(-FLT_MAX), vec(0.0f), vec(FLT_MAX),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "posfromend",
            PROP_FVEC,
            vec(-FLT_MAX), vec(0.0f), vec(FLT_MAX),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "posflip",
            PROP_INT,
            0, 0, 1,
            0
        ),
        fxpropertydef(
            "endfromprev",
            PROP_INT,
            0, 0, 1,
            0
        ),
        fxpropertydef(
            "posfromenttag",
            PROP_INT,
            -1, -1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "endfromenttag",
            PROP_INT,
            -1, -1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "posfroment",
            PROP_INT,
            game::ENT_POS_NONE, game::ENT_POS_NONE, game::ENT_POS_MUZZLE,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "endfroment",
            PROP_INT,
            game::ENT_POS_NONE, game::ENT_POS_NONE, game::ENT_POS_MUZZLE,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "iter",
            PROP_INT,
            1, 1, FX_ITER_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "iteroffset",
            PROP_FVEC,
            vec(-FLT_MAX), vec(0.0f), vec(FLT_MAX),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "weapon",
            PROP_INT,
            -1, -1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        )
    };

    static fxpropertydef propdefpart[FX_PART_PROPS] =
    {
        fxpropertydef(
            "parttype",
            PROP_INT,
            0, (int)FX_PART_TYPE_SINGLE, (int)FX_PART_TYPES - 1,
            0
        ),
        fxpropertydef(
            "part",
            PROP_INT,
            (int)PART_FIREBALL_LERP, (int)PART_FIREBALL_LERP, (int)PART_LIGHTZAP,
            0
        ),
        fxpropertydef(
            "num",
            PROP_INT,
            1, 1, 100,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "shape",
            PROP_INT,
            0, 0, 58,
            0
        ),
        fxpropertydef(
            "colour",
            PROP_COLOR,
            bvec(0, 0, 0), bvec(255, 255, 255), bvec(255, 255, 255),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "envcolour",
            PROP_COLOR,
            bvec(0, 0, 0), bvec(255, 255, 255), bvec(255, 255, 255),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "envblend",
            PROP_FLOAT,
            0.0f, 0.5f, 1.0f,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "fade",
            PROP_INT,
            1, 1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "collide",
            PROP_INT,
            -2, -1, (int)STAIN_MAX - 1,
            0
        ),
        fxpropertydef(
            "regdelay",
            PROP_INT,
            0, 0, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "shapesize",
            PROP_FLOAT,
            0.0f, 4.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "partsize",
            PROP_FLOAT,
            FLT_MIN, 4.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "partsizechange",
            PROP_FLOAT,
            -FLT_MAX, 0.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "maxpartsize",
            PROP_FLOAT,
            FLT_MIN, 16.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "vel",
            PROP_FLOAT,
            0.0f, 50.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "gravity",
            PROP_FLOAT,
            -FLT_MAX, 0.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "text",
            PROP_STRING,
            "", "", "",
            0
        ),
        fxpropertydef(
            "parttrack",
            PROP_INT,
            0, 1, 1,
            0
        ),
        fxpropertydef(
            "hintcolour",
            PROP_COLOR,
            bvec(0, 0, 0), bvec(0, 0, 255), bvec(255, 255, 255),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "hintblend",
            PROP_FLOAT,
            0.0f, 0.0f, 1.0f,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        )
    };

    static fxpropertydef propdeflight[FX_LIGHT_PROPS] =
    {
        fxpropertydef(
            "radius",
            PROP_FLOAT,
            FLT_MIN, 32.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "colour",
            PROP_COLOR,
            bvec(0, 0, 0), bvec(255, 255, 255), bvec(255, 255, 255),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "flags",
            PROP_INT,
            0, 0, INT_MAX,
            0
        ),
        fxpropertydef(
            "spot",
            PROP_INT,
            0, 0, 180,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "gain",
            PROP_FLOAT,
            0.0f, 1.0f, FVAR_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        )
    };

    static fxpropertydef propdefsound[FX_SOUND_PROPS] =
    {
        fxpropertydef(
            "sound",
            PROP_STRING,
            "", "", "",
            0
        ),
        fxpropertydef(
            "weaponsound",
            PROP_INT,
            -1, -1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "gain",
            PROP_FLOAT,
            0.0f, 1.0f, 100.0f,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "pitch",
            PROP_FLOAT,
            0.0f, 1.0f, 100.0f,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "rolloff",
            PROP_FLOAT,
            -1.0f, -1.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "refdist",
            PROP_FLOAT,
            -1.0f, -1.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "maxdist",
            PROP_FLOAT,
            -1.0f, -1.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "flags",
            PROP_INT,
            0, 0, INT_MAX,
            0
        ),
        fxpropertydef(
            "onplayer",
            PROP_INT,
            0, 0, 1,
            0
        )
    };

    static fxpropertydef propdefwind[FX_WIND_PROPS] =
    {
        fxpropertydef(
            "mode",
            PROP_INT,
            0, 1, INT_MAX,
            0
        ),
        fxpropertydef(
            "speed",
            PROP_FLOAT,
            0.0f, 1.0f, 2.0f,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "interval",
            PROP_INT,
            0, 0, INT_MAX,
            0
        ),
        fxpropertydef(
            "cyclelen",
            PROP_INT,
            0, 4000, INT_MAX,
            0
        ),
        fxpropertydef(
            "radius",
            PROP_INT,
            0, 0, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "atten",
            PROP_FLOAT,
            0.0f, 0.1f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "yaw",
            PROP_INT,
            0, 0, 360,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_FLAG_LERP360) | BIT(FX_MOD_PARAM)
        )
    };

    static fxpropertydef propdefstain[FX_STAIN_PROPS] =
    {
        fxpropertydef(
            "staintype",
            PROP_INT,
            0, (int)STAIN_SMOKE, (int)STAIN_MAX - 1,
            0
        ),
        fxpropertydef(
            "radius",
            PROP_FLOAT,
            FLT_MIN, 16.0f, 256.0f,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "colour",
            PROP_COLOR,
            bvec(0, 0, 0), bvec(255, 255, 255), bvec(255, 255, 255),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "envcolour",
            PROP_COLOR,
            bvec(0, 0, 0), bvec(255, 255, 255), bvec(255, 255, 255),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        ),
        fxpropertydef(
            "envblend",
            PROP_FLOAT,
            0.0f, 0.5f, 1.0f,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_PARAM)
        )
    };
}
