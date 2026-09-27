// Bilateral AO filter: one direction (BILATERAL_X, else y) of a separable
// blur that weights each tap by its depth difference from the centre.
// Options, from bilateralshader in config/glsl/ao.cfg:
//   BILATERAL_LINEAR    depth comes from linearizedepth ("l")
//   BILATERAL_PACKED    AO carries its depth, see AO_PACKED ("p")
//   BILATERAL_UPSCALED  filter from the reduced AO buffer to full size ("u")
//   BILATERAL_REDUCE    aoreduce when the depth is read at full size, else 0
//   BILATERAL_TAPS      taps each side of the centre, 1..10
//   BILATERAL_X         filter along x; the x pass also writes the packed depth
// Engine state, from aoshaderdefines: MSAA_SAMPLES, GDEPTH_FORMAT,
// AO_DEPTH_FORMAT (see linearizedepth.frag), and TEXRECT_MINOFFSET/
// TEXRECT_MAXOFFSET ($mintexrectoffset/$maxtexrectoffset).
//
// The taps are unrolled on purpose, as in ambientobscurance.frag. Each one
// chooses between an offset fetch and a plain one, because offset fetches
// need a constant offset inside the TEXRECT limits. Keep the macros on one
// line each; line continuation needs GLSL 4.20.
#if MSAA_SAMPLES && !defined(BILATERAL_LINEAR)
uniform sampler2DMS tex1;
#define gfetch(sampler, coords) texelFetch(sampler, ivec2(coords), 0)
#define gfetchoffset(sampler, coords, offset) texelFetch(sampler, ivec2(coords) + offset, 0)
#else
uniform sampler2DRect tex1;
#define gfetch(sampler, coords) texture2DRect(sampler, coords)
#define gfetchoffset(sampler, coords, offset) texture2DRectOffset(sampler, coords, offset)
#endif
uniform vec3 gdepthscale;
uniform vec3 gdepthunpackparams;
uniform sampler2DRect tex0;
uniform vec2 bilateralparams;
uniform vec3 gdepthpackparams;
#if BILATERAL_REDUCE
varying vec2 texcoord0;
#endif
#ifdef BILATERAL_UPSCALED
varying vec2 texcoord1;
#endif
fragdata(0) vec4 fragcolor;

#ifdef BILATERAL_UPSCALED
#define tc texcoord1
#else
#define tc gl_FragCoord.xy
#endif
#if BILATERAL_REDUCE
#define depthtc texcoord0
#else
#define depthtc gl_FragCoord.xy
#endif
#ifdef BILATERAL_X
#define tapvec(type, i) type(i, 0.0)
#else
#define tapvec(type, i) type(0.0, i)
#endif
#define texval(i) texture2DRect(tex0, tc + tapvec(vec2, i))
#define texvaloffset(i) texture2DRectOffset(tex0, tc, tapvec(ivec2, i))
#define depthval(i) gfetch(tex1, depthtc + tapvec(vec2, i))
#define depthvaloffset(i) gfetchoffset(tex1, depthtc, tapvec(ivec2, i))

// A g-buffer depth sample to linear depth.
#if GDEPTH_FORMAT > 1
#define BILATERAL_UNPACKDEPTH(val) val.r
#elif GDEPTH_FORMAT == 1
#define BILATERAL_UNPACKDEPTH(val) dot(val.rgb, gdepthunpackparams)
#else
#define BILATERAL_UNPACKDEPTH(val) gdepthscale.x / (val.r*gdepthscale.y + gdepthscale.z)
#endif

// tapcolor and tapdepth of one tap, from its AO sample texv and depth sample depthv.
#if defined(BILATERAL_PACKED) && AO_DEPTH_FORMAT != 0
#define BILATERAL_TAPREAD(texv, depthv) vec2 tapvals = texv.rg;
#define tapcolor tapvals.x
#define tapdepth tapvals.y
#elif defined(BILATERAL_PACKED)
#define BILATERAL_TAPREAD(texv, depthv) vec4 tapvals = texv; float tapdepth = dot(tapvals.rgb, gdepthunpackparams);
#define tapcolor tapvals.a
#elif defined(BILATERAL_LINEAR) && AO_DEPTH_FORMAT != 0
#define BILATERAL_TAPREAD(texv, depthv) float tapcolor = texv.r; float tapdepth = depthv.r;
#elif defined(BILATERAL_LINEAR)
#define BILATERAL_TAPREAD(texv, depthv) float tapcolor = texv.r; float tapdepth = dot(depthv.rgb, gdepthunpackparams);
#else
#define BILATERAL_TAPREAD(texv, depthv) float tapcolor = texv.r; float tapdepth = BILATERAL_UNPACKDEPTH(depthv);
#endif

// One tap: w is minus its squared distance, texv and depthv its samples.
#define BILATERAL_TAP(w, texv, depthv) { BILATERAL_TAPREAD(texv, depthv) tapdepth -= depth; float tapweight = exp2(w*bilateralparams.x - tapdepth*tapdepth*bilateralparams.y); weights += tapweight; color += tapweight * tapcolor; }

// Whether an offset fits an offset fetch.
#define BILATERAL_FITS(o) ((o) >= TEXRECT_MINOFFSET && (o) <= TEXRECT_MAXOFFSET)
// Each tap below checks BILATERAL_FITS on the depth offset first: the depth
// offset is the tap offset times BILATERAL_DEPTHSCALE (2^BILATERAL_REDUCE)
// with the same sign, so it fitting implies the tap offset fits too (the
// limits always contain 0).
// The depth offset's scale, 2^BILATERAL_REDUCE (aoreduce is 0..2). GLSL 1.20
// rejects "<<" in code (no EXT_gpu_shader4), so this is a literal chain.
#if BILATERAL_REDUCE == 2
#define BILATERAL_DEPTHSCALE 4
#elif BILATERAL_REDUCE == 1
#define BILATERAL_DEPTHSCALE 2
#else
#define BILATERAL_DEPTHSCALE 1
#endif

void main(void)
{
#if defined(BILATERAL_PACKED) && AO_DEPTH_FORMAT != 0
    vec2 vals = texture2DRect(tex0, tc).rg;
    #define color vals.x
  #ifdef BILATERAL_UPSCALED
    float depth = BILATERAL_UNPACKDEPTH(gfetch(tex1, depthtc));
  #else
    #define depth vals.y
  #endif
#elif defined(BILATERAL_PACKED)
    vec4 vals = texture2DRect(tex0, tc);
    #define color vals.a
  #ifdef BILATERAL_UPSCALED
    float depth = BILATERAL_UNPACKDEPTH(gfetch(tex1, depthtc));
  #else
    float depth = dot(vals.rgb, gdepthunpackparams);
  #endif
#elif defined(BILATERAL_LINEAR)
    float color = gfetch(tex0, tc).r;
  #if AO_DEPTH_FORMAT != 0
    float depth = gfetch(tex1, depthtc).r;
  #else
    float depth = dot(gfetch(tex1, depthtc).rgb, gdepthunpackparams);
  #endif
#else
    float color = texture2DRect(tex0, tc).r;
    float depth = BILATERAL_UNPACKDEPTH(gfetch(tex1, depthtc));
#endif
    float weights = 1.0;
    // Taps run -BILATERAL_TAPS..-1, then 1..BILATERAL_TAPS: the weights are summed in that order.
#if BILATERAL_TAPS >= 10
  #if BILATERAL_FITS(-20*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-100.0, texvaloffset(-20.0), depthvaloffset(float(-20*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-20)
    BILATERAL_TAP(-100.0, texvaloffset(-20.0), depthval(float(-20*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-100.0, texval(-20.0), depthval(float(-20*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 9
  #if BILATERAL_FITS(-18*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-81.0, texvaloffset(-18.0), depthvaloffset(float(-18*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-18)
    BILATERAL_TAP(-81.0, texvaloffset(-18.0), depthval(float(-18*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-81.0, texval(-18.0), depthval(float(-18*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 8
  #if BILATERAL_FITS(-16*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-64.0, texvaloffset(-16.0), depthvaloffset(float(-16*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-16)
    BILATERAL_TAP(-64.0, texvaloffset(-16.0), depthval(float(-16*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-64.0, texval(-16.0), depthval(float(-16*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 7
  #if BILATERAL_FITS(-14*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-49.0, texvaloffset(-14.0), depthvaloffset(float(-14*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-14)
    BILATERAL_TAP(-49.0, texvaloffset(-14.0), depthval(float(-14*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-49.0, texval(-14.0), depthval(float(-14*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 6
  #if BILATERAL_FITS(-12*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-36.0, texvaloffset(-12.0), depthvaloffset(float(-12*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-12)
    BILATERAL_TAP(-36.0, texvaloffset(-12.0), depthval(float(-12*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-36.0, texval(-12.0), depthval(float(-12*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 5
  #if BILATERAL_FITS(-10*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-25.0, texvaloffset(-10.0), depthvaloffset(float(-10*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-10)
    BILATERAL_TAP(-25.0, texvaloffset(-10.0), depthval(float(-10*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-25.0, texval(-10.0), depthval(float(-10*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 4
  #if BILATERAL_FITS(-8*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-16.0, texvaloffset(-8.0), depthvaloffset(float(-8*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-8)
    BILATERAL_TAP(-16.0, texvaloffset(-8.0), depthval(float(-8*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-16.0, texval(-8.0), depthval(float(-8*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 3
  #if BILATERAL_FITS(-6*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-9.0, texvaloffset(-6.0), depthvaloffset(float(-6*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-6)
    BILATERAL_TAP(-9.0, texvaloffset(-6.0), depthval(float(-6*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-9.0, texval(-6.0), depthval(float(-6*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 2
  #if BILATERAL_FITS(-4*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-4.0, texvaloffset(-4.0), depthvaloffset(float(-4*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-4)
    BILATERAL_TAP(-4.0, texvaloffset(-4.0), depthval(float(-4*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-4.0, texval(-4.0), depthval(float(-4*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 1
  #if BILATERAL_FITS(-2*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-1.0, texvaloffset(-2.0), depthvaloffset(float(-2*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(-2)
    BILATERAL_TAP(-1.0, texvaloffset(-2.0), depthval(float(-2*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-1.0, texval(-2.0), depthval(float(-2*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 1
  #if BILATERAL_FITS(2*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-1.0, texvaloffset(2.0), depthvaloffset(float(2*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(2)
    BILATERAL_TAP(-1.0, texvaloffset(2.0), depthval(float(2*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-1.0, texval(2.0), depthval(float(2*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 2
  #if BILATERAL_FITS(4*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-4.0, texvaloffset(4.0), depthvaloffset(float(4*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(4)
    BILATERAL_TAP(-4.0, texvaloffset(4.0), depthval(float(4*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-4.0, texval(4.0), depthval(float(4*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 3
  #if BILATERAL_FITS(6*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-9.0, texvaloffset(6.0), depthvaloffset(float(6*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(6)
    BILATERAL_TAP(-9.0, texvaloffset(6.0), depthval(float(6*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-9.0, texval(6.0), depthval(float(6*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 4
  #if BILATERAL_FITS(8*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-16.0, texvaloffset(8.0), depthvaloffset(float(8*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(8)
    BILATERAL_TAP(-16.0, texvaloffset(8.0), depthval(float(8*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-16.0, texval(8.0), depthval(float(8*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 5
  #if BILATERAL_FITS(10*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-25.0, texvaloffset(10.0), depthvaloffset(float(10*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(10)
    BILATERAL_TAP(-25.0, texvaloffset(10.0), depthval(float(10*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-25.0, texval(10.0), depthval(float(10*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 6
  #if BILATERAL_FITS(12*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-36.0, texvaloffset(12.0), depthvaloffset(float(12*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(12)
    BILATERAL_TAP(-36.0, texvaloffset(12.0), depthval(float(12*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-36.0, texval(12.0), depthval(float(12*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 7
  #if BILATERAL_FITS(14*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-49.0, texvaloffset(14.0), depthvaloffset(float(14*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(14)
    BILATERAL_TAP(-49.0, texvaloffset(14.0), depthval(float(14*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-49.0, texval(14.0), depthval(float(14*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 8
  #if BILATERAL_FITS(16*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-64.0, texvaloffset(16.0), depthvaloffset(float(16*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(16)
    BILATERAL_TAP(-64.0, texvaloffset(16.0), depthval(float(16*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-64.0, texval(16.0), depthval(float(16*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 9
  #if BILATERAL_FITS(18*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-81.0, texvaloffset(18.0), depthvaloffset(float(18*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(18)
    BILATERAL_TAP(-81.0, texvaloffset(18.0), depthval(float(18*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-81.0, texval(18.0), depthval(float(18*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if BILATERAL_TAPS >= 10
  #if BILATERAL_FITS(20*BILATERAL_DEPTHSCALE)
    BILATERAL_TAP(-100.0, texvaloffset(20.0), depthvaloffset(float(20*BILATERAL_DEPTHSCALE)))
  #elif BILATERAL_FITS(20)
    BILATERAL_TAP(-100.0, texvaloffset(20.0), depthval(float(20*BILATERAL_DEPTHSCALE)))
  #else
    BILATERAL_TAP(-100.0, texval(20.0), depthval(float(20*BILATERAL_DEPTHSCALE)))
  #endif
#endif
#if defined(BILATERAL_X) && defined(BILATERAL_PACKED) && AO_DEPTH_FORMAT != 0
    fragcolor.rg = vec2(color / weights, depth);
#elif defined(BILATERAL_X) && defined(BILATERAL_PACKED)
  #ifdef BILATERAL_UPSCALED
    vec3 packdepth = depth * gdepthpackparams;
    packdepth = vec3(packdepth.x, fract(packdepth.yz));
    packdepth.xy -= packdepth.yz * (1.0/255.0);
  #else
    #define packdepth vals.rgb
  #endif
    fragcolor = vec4(packdepth, color / weights);
#else
    fragcolor = vec4(color / weights, 0.0, 0.0, 1.0);
#endif
}
