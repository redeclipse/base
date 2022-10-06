#include "cube.h"

enum
{
    SOUNDENV_PROP_RVB_DENSITY = 0,
    SOUNDENV_PROP_RVB_DIFFUSION,
    SOUNDENV_PROP_RVB_GAIN,
    SOUNDENV_PROP_RVB_GAINHF,
    SOUNDENV_PROP_RVB_DECAY_TIME,
    SOUNDENV_PROP_RVB_DECAY_HFRATIO,
    SOUNDENV_PROP_RVB_REFL_GAIN,
    SOUNDENV_PROP_RVB_REFL_DELAY,
    SOUNDENV_PROP_RVB_LATE_GAIN,
    SOUNDENV_PROP_RVB_LATE_DELAY,
    SOUNDENV_PROP_RVB_ABSORBTION_GAINHF,
    SOUNDENV_PROP_RVB_ROOM_ROLLOFF,
    SOUNDENV_PROP_RVB_DECAY_HFLIMIT,

    SOUNDENV_PROP_ECHO_DELAY,
    SOUNDENV_PROP_ECHO_LRDELAY,
    SOUNDENV_PROP_ECHO_DAMPING,
    SOUNDENV_PROP_ECHO_FEEDBACK,
    SOUNDENV_PROP_ECHO_SPREAD,

    SOUNDENV_PROPS
};

static propertydef soundenvprops[SOUNDENV_PROPS] =
{
    propertydef(
        "reverb_density",
        PROP_FLOAT,
        0.0f, 1.0f, 1.0f
    ),
    propertydef(
        "reverb_diffusion",
        PROP_FLOAT,
        0.0f, 1.0f, 1.0f
    ),
    propertydef(
        "reverb_gain",
        PROP_FLOAT,
        0.0f, 0.32f, 1.0f
    ),
    propertydef(
        "reverb_gainhf",
        PROP_FLOAT,
        0.0f, 0.89f, 1.0f
    ),
    propertydef(
        "reverb_decay_time",
        PROP_FLOAT,
        0.1f, 1.49f, 20.0f
    ),
    propertydef(
        "reverb_decay_hfratio",
        PROP_FLOAT,
        0.1f, 0.83f, 2.0f
    ),
    propertydef(
        "reverb_reflections_gain",
        PROP_FLOAT,
        0.0f, 0.05f, 3.16f
    ),
    propertydef(
        "reverb_reflections_delay",
        PROP_FLOAT,
        0.0f, 0.007f, 0.3f
    ),
    propertydef(
        "reverb_late_gain",
        PROP_FLOAT,
        0.0f, 1.26f, 10.0f
    ),
    propertydef(
        "reverb_late_delay",
        PROP_FLOAT,
        0.0f, 0.011f, 0.1f
    ),
    propertydef(
        "reverb_air_absorbtion_gainhf",
        PROP_FLOAT,
        0.892f, 0.994f, 1.0f
    ),
    propertydef(
        "reverb_room_rolloff_factor",
        PROP_FLOAT,
        0.0f, 0.0f, 10.0f
    ),
    propertydef(
        "reverb_decay_hflimit",
        PROP_INT,
        0, 1, 1
    ),

    propertydef(
        "echo_delay",
        PROP_FLOAT,
        0.0f, 0.1f, 0.207f
    ),
    propertydef(
        "echo_lrdelay",
        PROP_FLOAT,
        0.0f, 0.1f, 0.404f
    ),
    propertydef(
        "echo_damping",
        PROP_FLOAT,
        0.0f, 0.5f, 0.99f
    ),
    propertydef(
        "echo_feedback",
        PROP_FLOAT,
        0.0f, 0.5f, 1.0f
    ),
    propertydef(
        "echo_spread",
        PROP_FLOAT,
        -1.0f, -1.0f, 1.0f
    )
};
