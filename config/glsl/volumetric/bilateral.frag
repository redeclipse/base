// Volumetric light filter: one direction (BILATERAL_X, else y) of a
// separable blur of the volumetric buffer that weights each tap by its depth
// difference from the centre.
// volumetricbilateralshader (config/glsl/volumetric.cfg) defines:
//   VOLBILATERAL_TAPS   taps each side of the centre, 1..3 ($volbilateral)
//   BILATERAL_REDUCE    $volreduce, 0..2: the buffer is 2^n times smaller than
//                       the depth buffer
//   BILATERAL_X
// and the engine state: GFETCH_MS ($msaalight), GDEPTH_FORMAT,
// TEXRECT_MINOFFSET and TEXRECT_MAXOFFSET.
// Uses config/glsl/shared/gfetch.glsl, gdepth.glsl and bilateral.glsl.
uniform GFETCH_SAMPLER currentdepth;
GDEPTH_UNPACK_DECLS
uniform sampler2DRect tex0;
uniform vec2 bilateralparams;
#if BILATERAL_REDUCE
varying vec2 texcoord0;
#endif
fragdata(0) vec4 fragcolor;

#define tc gl_FragCoord.xy
#if BILATERAL_REDUCE
#define depthtc texcoord0
#else
#define depthtc gl_FragCoord.xy
#endif
#define BILATERAL_DEPTHTEX currentdepth

// One tap: its colour, depth and weight names, w minus its squared distance,
// texv and depthv its samples.
#define VOLBILATERAL_TAP(tapcolor, tapdepth, tapweight, w, texv, depthv) vec3 tapcolor = texv.rgb; float tapdepth = GDEPTH_UNPACK(depthv); tapdepth -= depth; float tapweight = exp2(w*bilateralparams.x - tapdepth*tapdepth*bilateralparams.y); weights += tapweight; color += tapweight * tapcolor;

// The depth offsets of the taps 2, 4 and 6 texels out, times
// BILATERAL_DEPTHSCALE, spelt as the generator printed them.
#if BILATERAL_REDUCE == 2
#define VOLBILATERAL_DEPTH1 8.0
#define VOLBILATERAL_DEPTH2 16.0
#define VOLBILATERAL_DEPTH3 24.0
#elif BILATERAL_REDUCE == 1
#define VOLBILATERAL_DEPTH1 4.0
#define VOLBILATERAL_DEPTH2 8.0
#define VOLBILATERAL_DEPTH3 12.0
#else
#define VOLBILATERAL_DEPTH1 2.0
#define VOLBILATERAL_DEPTH2 4.0
#define VOLBILATERAL_DEPTH3 6.0
#endif

// The taps at -6..6 texels (VOLBILATERAL_TAPM3..VOLBILATERAL_TAPP3), each
// taking its three names. The depth offset is checked first: it fitting
// implies the tap offset fits (shared/bilateral.glsl).
#if BILATERAL_FITS(-6*BILATERAL_DEPTHSCALE)
#define VOLBILATERAL_TAPM3(c, d, w) VOLBILATERAL_TAP(c, d, w, -9.0, texvaloffset(-6.0), depthvaloffset(-VOLBILATERAL_DEPTH3))
#elif BILATERAL_FITS(-6)
#define VOLBILATERAL_TAPM3(c, d, w) VOLBILATERAL_TAP(c, d, w, -9.0, texvaloffset(-6.0), depthval(-VOLBILATERAL_DEPTH3))
#else
#define VOLBILATERAL_TAPM3(c, d, w) VOLBILATERAL_TAP(c, d, w, -9.0, texval(-6.0), depthval(-VOLBILATERAL_DEPTH3))
#endif
#if BILATERAL_FITS(-4*BILATERAL_DEPTHSCALE)
#define VOLBILATERAL_TAPM2(c, d, w) VOLBILATERAL_TAP(c, d, w, -4.0, texvaloffset(-4.0), depthvaloffset(-VOLBILATERAL_DEPTH2))
#elif BILATERAL_FITS(-4)
#define VOLBILATERAL_TAPM2(c, d, w) VOLBILATERAL_TAP(c, d, w, -4.0, texvaloffset(-4.0), depthval(-VOLBILATERAL_DEPTH2))
#else
#define VOLBILATERAL_TAPM2(c, d, w) VOLBILATERAL_TAP(c, d, w, -4.0, texval(-4.0), depthval(-VOLBILATERAL_DEPTH2))
#endif
#if BILATERAL_FITS(-2*BILATERAL_DEPTHSCALE)
#define VOLBILATERAL_TAPM1(c, d, w) VOLBILATERAL_TAP(c, d, w, -1.0, texvaloffset(-2.0), depthvaloffset(-VOLBILATERAL_DEPTH1))
#elif BILATERAL_FITS(-2)
#define VOLBILATERAL_TAPM1(c, d, w) VOLBILATERAL_TAP(c, d, w, -1.0, texvaloffset(-2.0), depthval(-VOLBILATERAL_DEPTH1))
#else
#define VOLBILATERAL_TAPM1(c, d, w) VOLBILATERAL_TAP(c, d, w, -1.0, texval(-2.0), depthval(-VOLBILATERAL_DEPTH1))
#endif
#if BILATERAL_FITS(2*BILATERAL_DEPTHSCALE)
#define VOLBILATERAL_TAPP1(c, d, w) VOLBILATERAL_TAP(c, d, w, -1.0, texvaloffset(2.0), depthvaloffset(VOLBILATERAL_DEPTH1))
#elif BILATERAL_FITS(2)
#define VOLBILATERAL_TAPP1(c, d, w) VOLBILATERAL_TAP(c, d, w, -1.0, texvaloffset(2.0), depthval(VOLBILATERAL_DEPTH1))
#else
#define VOLBILATERAL_TAPP1(c, d, w) VOLBILATERAL_TAP(c, d, w, -1.0, texval(2.0), depthval(VOLBILATERAL_DEPTH1))
#endif
#if BILATERAL_FITS(4*BILATERAL_DEPTHSCALE)
#define VOLBILATERAL_TAPP2(c, d, w) VOLBILATERAL_TAP(c, d, w, -4.0, texvaloffset(4.0), depthvaloffset(VOLBILATERAL_DEPTH2))
#elif BILATERAL_FITS(4)
#define VOLBILATERAL_TAPP2(c, d, w) VOLBILATERAL_TAP(c, d, w, -4.0, texvaloffset(4.0), depthval(VOLBILATERAL_DEPTH2))
#else
#define VOLBILATERAL_TAPP2(c, d, w) VOLBILATERAL_TAP(c, d, w, -4.0, texval(4.0), depthval(VOLBILATERAL_DEPTH2))
#endif
#if BILATERAL_FITS(6*BILATERAL_DEPTHSCALE)
#define VOLBILATERAL_TAPP3(c, d, w) VOLBILATERAL_TAP(c, d, w, -9.0, texvaloffset(6.0), depthvaloffset(VOLBILATERAL_DEPTH3))
#elif BILATERAL_FITS(6)
#define VOLBILATERAL_TAPP3(c, d, w) VOLBILATERAL_TAP(c, d, w, -9.0, texvaloffset(6.0), depthval(VOLBILATERAL_DEPTH3))
#else
#define VOLBILATERAL_TAPP3(c, d, w) VOLBILATERAL_TAP(c, d, w, -9.0, texval(6.0), depthval(VOLBILATERAL_DEPTH3))
#endif

void main(void)
{
    vec3 color = texture2DRect(tex0, tc).rgb;
    float depth = GDEPTH_UNPACK(gfetch(currentdepth, depthtc));
    float weights = 1.0;
    // Taps run outwards-in on the negative side, then outwards on the
    // positive side, numbered in that order: the weights are summed so.
#if VOLBILATERAL_TAPS >= 3
    VOLBILATERAL_TAPM3(color0, depth0, weight0)
    VOLBILATERAL_TAPM2(color1, depth1, weight1)
    VOLBILATERAL_TAPM1(color2, depth2, weight2)
    VOLBILATERAL_TAPP1(color3, depth3, weight3)
    VOLBILATERAL_TAPP2(color4, depth4, weight4)
    VOLBILATERAL_TAPP3(color5, depth5, weight5)
#elif VOLBILATERAL_TAPS == 2
    VOLBILATERAL_TAPM2(color0, depth0, weight0)
    VOLBILATERAL_TAPM1(color1, depth1, weight1)
    VOLBILATERAL_TAPP1(color2, depth2, weight2)
    VOLBILATERAL_TAPP2(color3, depth3, weight3)
#else
    VOLBILATERAL_TAPM1(color0, depth0, weight0)
    VOLBILATERAL_TAPP1(color1, depth1, weight1)
#endif
    fragcolor = vec4(color / weights, 0.0);
}
