// Separable Gaussian blur: one direction (BLUR_X, else y) of the passes
// setblurshader (src/engine/shader.cpp) runs. Tap i adds weights[i] times the
// samples at +/-offsets[i]; setupblurkernel places them between texel pairs so
// bilinear filtering reads two texels per sample.
// Defines, from blurshader in config/glsl/blur.cfg:
//   BLUR_RADIUS  taps each side of the centre, 1..MAXBLURRADIUS (7)
//   BLUR_X       blur along x, else y
//   BLUR_RECT    tex0 is a sampler2DRect, else a sampler2D
//   BLUR_ALPHA   weight each sample by its alpha, and keep the result no
//                brighter than the brightest sample ("bluralpha" shaders)
// Uses BLUR_SIZE and BLUR_AXIS from blur_defs.glsl, LUMWEIGHTS from
// config/glsl/shared/luma.glsl.
//
// The taps are unrolled on purpose, one macro line each; keep them on one
// line, since line continuation needs GLSL 4.20.
uniform float weights[BLUR_SIZE];
uniform float offsets[BLUR_SIZE];
#ifdef BLUR_RECT
uniform sampler2DRect tex0;
#define texval(sampler, coords) texture2DRect(sampler, (coords))
#else
uniform sampler2D tex0;
#define texval(sampler, coords) texture2D(sampler, (coords))
#endif
varying vec2 texcoord0, texcoordp1, texcoordn1;
#if BLUR_RADIUS >= 2
varying vec2 texcoordp2, texcoordn2;
#endif
#if BLUR_RADIUS >= 3
varying vec2 texcoordp3, texcoordn3;
#endif
fragdata(0) vec4 fragcolor;

// The +/- texcoords of tap i beyond the interpolated three.
#ifdef BLUR_X
#define BLUR_TCP(i) vec2(texcoord0.x + offsets[i], texcoord0.y)
#define BLUR_TCN(i) vec2(texcoord0.x - offsets[i], texcoord0.y)
#else
#define BLUR_TCP(i) vec2(texcoord0.x, texcoord0.y + offsets[i])
#define BLUR_TCN(i) vec2(texcoord0.x, texcoord0.y - offsets[i])
#endif

// Tap i, sampled at texcoords p and n.
#ifdef BLUR_ALPHA
#define BLUR_ALPHASAMPLE(s, i) if(s.a > 0.0) { val.rgb += s.rgb * weights[i] * s.a; val.a += s.a * weights[i]; total += weights[i] * s.a; maxsource = max(maxsource, dot(s.rgb, LUMWEIGHTS)); }
#define BLUR_TAP(i, p, n) samplep = texval(tex0, p); samplen = texval(tex0, n); BLUR_ALPHASAMPLE(samplep, i) BLUR_ALPHASAMPLE(samplen, i)
#else
#define BLUR_TAP(i, p, n) val += weights[i] * (texval(tex0, p) + texval(tex0, n));
#endif

void main(void)
{
    vec4 srccolor = texval(tex0, texcoord0);
#ifdef BLUR_ALPHA
    vec4 samplep, samplen;
    vec4 val = srccolor * weights[0] * srccolor.a;
    float total = weights[0] * srccolor.a, maxsource = dot(srccolor.rgb, LUMWEIGHTS);
#else
    vec4 val = srccolor * weights[0];
#endif
    BLUR_TAP(1, texcoordp1, texcoordn1)
#if BLUR_RADIUS >= 2
    BLUR_TAP(2, texcoordp2, texcoordn2)
#endif
#if BLUR_RADIUS >= 3
    BLUR_TAP(3, texcoordp3, texcoordn3)
#endif
#if BLUR_RADIUS >= 4
    BLUR_TAP(4, BLUR_TCP(4), BLUR_TCN(4))
#endif
#if BLUR_RADIUS >= 5
    BLUR_TAP(5, BLUR_TCP(5), BLUR_TCN(5))
#endif
#if BLUR_RADIUS >= 6
    BLUR_TAP(6, BLUR_TCP(6), BLUR_TCN(6))
#endif
#if BLUR_RADIUS >= 7
    BLUR_TAP(7, BLUR_TCP(7), BLUR_TCN(7))
#endif
#ifdef BLUR_ALPHA
    if(total > 0.0) val.rgb /= total;
    float outputbright = dot(val.rgb, LUMWEIGHTS);
    if(outputbright > maxsource && outputbright > 0.0) val.rgb *= maxsource / outputbright;
#endif

    fragcolor = val;
}
