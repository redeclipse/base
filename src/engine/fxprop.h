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
            "lerpsquare",
            PROP_INT,
            0, 0, 1
        )
    };

    static fxpropertydef propdefstd[FX_STD_PROPS] =
    {
        fxpropertydef(
            "activelen",
            PROP_INT,
            1,
            1,
            INT_MAX,
            BIT(FX_MOD_RAND)
        ),
        fxpropertydef(
            "emitlen",
            PROP_INT,
            1, 1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "emitinterval",
            PROP_INT,
            1, 1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "emitdelay",
            PROP_INT,
            0, 0, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
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
            0, 0, 1,
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
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "scale",
            PROP_FLOAT,
            0.0f, 1.0f, 1.0f,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "colorized",
            PROP_INT,
            0, 0, 1,
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
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "endoffset",
            PROP_FVEC,
            vec(-FLT_MAX), vec(0.0f), vec(FLT_MAX),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "endfrompos",
            PROP_FVEC,
            vec(-FLT_MAX), vec(0.0f), vec(FLT_MAX),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "posfromend",
            PROP_FVEC,
            vec(-FLT_MAX), vec(0.0f), vec(FLT_MAX),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
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
            (int)PART_FIREBALL_LERP, (int)PART_FIREBALL_LERP, (int)PART_MAX - 1,
            0
        ),
        fxpropertydef(
            "num",
            PROP_INT,
            1, 1, 100,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
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
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "fade",
            PROP_INT,
            1, 1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "collide",
            PROP_INT,
            0, 0, (int)STAIN_MAX - 1,
            0
        ),
        fxpropertydef(
            "regdelay",
            PROP_INT,
            0, 0, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "shapesize",
            PROP_FLOAT,
            0.0f, 4.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "partsize",
            PROP_FLOAT,
            FLT_MIN, 4.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "maxpartsize",
            PROP_FLOAT,
            FLT_MIN, 16.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "vel",
            PROP_FLOAT,
            0.0f, 50.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "gravity",
            PROP_FLOAT,
            -FLT_MAX, 0.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
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
        )
    };

    static fxpropertydef propdeflight[FX_LIGHT_PROPS] =
    {
        fxpropertydef(
            "radius",
            PROP_FLOAT,
            FLT_MIN, 32.0f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "colour",
            PROP_COLOR,
            bvec(0, 0, 0), bvec(255, 255, 255), bvec(255, 255, 255),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
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
            "volume",
            PROP_INT,
            0, 255, 255,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "flags",
            PROP_INT,
            0, 0, INT_MAX,
            0
        ),
        fxpropertydef(
            "minrad",
            PROP_INT,
            -1, -1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "maxrad",
            PROP_INT,
            -1, -1, INT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
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
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
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
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "atten",
            PROP_FLOAT,
            0.0f, 0.1f, FLT_MAX,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "yaw",
            PROP_INT,
            0, 0, 360,
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP) | BIT(FX_MOD_FLAG_LERP360)
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
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        ),
        fxpropertydef(
            "colour",
            PROP_COLOR,
            bvec(0, 0, 0), bvec(255, 255, 255), bvec(255, 255, 255),
            BIT(FX_MOD_RAND) | BIT(FX_MOD_LERP)
        )
    };
}
