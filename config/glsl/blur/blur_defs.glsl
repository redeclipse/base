// Blur macros both stages share; see blur.frag. Include with shader_include_vs
// and shader_include_fs after the BLUR_* defines.

// Length of the weights and offsets uniforms: the centre plus BLUR_RADIUS taps.
// A literal, not BLUR_RADIUS + 1, so the declarations keep their exact tokens.
#if BLUR_RADIUS >= 7
#define BLUR_SIZE 8
#elif BLUR_RADIUS == 6
#define BLUR_SIZE 7
#elif BLUR_RADIUS == 5
#define BLUR_SIZE 6
#elif BLUR_RADIUS == 4
#define BLUR_SIZE 5
#elif BLUR_RADIUS == 3
#define BLUR_SIZE 4
#elif BLUR_RADIUS == 2
#define BLUR_SIZE 3
#else
#define BLUR_SIZE 2
#endif

// The texcoord component the pass moves along.
#ifdef BLUR_X
#define BLUR_AXIS x
#else
#define BLUR_AXIS y
#endif
