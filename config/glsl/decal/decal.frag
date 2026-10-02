// Decals, drawn into the g-buffer over world geometry (renderdecals,
// src/engine/renderva.cpp). With dual-source blending ($maxdualdrawbufs) a
// decal draws in two passes: pass 0 blends the colour and glossiness, pass 1
// the normal. Without it, one pass writes both.
// Defines, from decalvariantshader in config/glsl/decal.cfg (the type letter
// in brackets):
//   DECAL_REFLECT          envmap reflection (r)
//   DECAL_REFLECT_SPECMAP  reflection scaled by the spec map (R)
//   DECAL_SPEC             spec (s)
//   DECAL_SPECMAP          spec map (S)
//   DECAL_NORMALMAP        normal map (n)
//   DECAL_PARALLAX         parallax (p)
//   DECAL_GLOW             glow (g)
//   DECAL_PULSEGLOW        pulse glow (G)
//   DECAL_KEEPNORMALS      leave the g-buffer normals alone (b)
//   DECAL_DISPLACE         displacement (v)
//   DECAL_PASS0            dual-source pass 0 (0)
//   DECAL_PASS1            dual-source pass 1 (1); neither is the single pass
//   USEPACKNORM            $usepacknorm, 0 or 1
// The output declarations come from the out_*.glsl include the alias picks
// for the pass. Uses GSPEC_PACK*, GGLOW_PACK* from
// config/glsl/shared/gcolor.glsl and GNORMAL_PACK* from
// config/glsl/shared/gnormal.glsl.
uniform sampler2D diffusemap;
uniform vec4 colorparams;
in vec4 texcoord0;
#ifdef DECAL_NORMALMAP
uniform sampler2D normalmap;
in mat3 world;
#else
in vec3 nvec;
#define bumpblend vec4(1.0)
#endif
#if defined(DECAL_PARALLAX) || defined(DECAL_REFLECT)
in vec3 camvec;
#endif
#if defined(DECAL_GLOW) || defined(DECAL_SPECMAP)
uniform sampler2D glowmap;
#endif
#ifdef DECAL_PULSEGLOW
flat in float pulse;
#endif
#ifdef DECAL_REFLECT
uniform samplerCube envmap;
#endif
#ifdef DECAL_DISPLACE
in vec2 dispcoord0, dispcoord1;
uniform sampler2D dispmap;
#endif

// A texture coordinate, offset by the displacement when there is one.
#ifdef DECAL_DISPLACE
#define DECAL_TC(tc) tc + disp.xy
#else
#define DECAL_TC(tc) tc
#endif

void main(void)
{
#ifdef DECAL_DISPLACE
    vec3 disp = (texture(dispmap, dispcoord0).rgb*dispcontrib.x + texture(dispmap, dispcoord1).rgb*dispcontrib.y - (dispcontrib.x+dispcontrib.y)*0.5) * dispcontrib.z;
#endif

#ifdef DECAL_NORMALMAP
#ifdef DECAL_PARALLAX
    float height = texture(normalmap, DECAL_TC(texcoord0.xy)).a;
    vec3 camvecn = normalize(camvec);
    vec2 dtc = texcoord0.xy + (camvecn * world).xy*(height*parallaxscale.x + parallaxscale.y);
#else
#define dtc texcoord0.xy
#endif

#if !defined(DECAL_PASS0) || defined(DECAL_REFLECT)
    vec3 bump = texture(normalmap, DECAL_TC(dtc)).rgb*2.0 - 1.0;
    vec3 bumpw = world * bump;
#define nvec bumpw
#endif
#else
#define dtc texcoord0.xy
#endif

    vec4 diffuse = texture(diffusemap, DECAL_TC(dtc));

#ifdef DECAL_GLOW
    vec4 glowspec = texture(glowmap, DECAL_TC(dtc));
#define glow glowspec.rgb
#define spec glowspec.a
#ifdef DECAL_PULSEGLOW
    glow *= mix(glowcolor.xyz, pulseglowcolor.xyz, pulse);
#else
    glow *= glowcolor.xyz;
#endif
    glow *= diffuse.a;
#endif

#ifdef DECAL_PASS0
#if defined(DECAL_SPECMAP) && !defined(DECAL_GLOW)
#if !defined(DECAL_NORMALMAP) || defined(DECAL_PARALLAX)
    float spec = texture(glowmap, DECAL_TC(dtc)).r;
#else
    float spec = texture(normalmap, DECAL_TC(dtc)).a;
#endif
#endif
#if defined(DECAL_SPEC) && defined(DECAL_SPECMAP)
    GSPEC_PACK_SPEC(gloss.x, spec * specscale.x)
#elif defined(DECAL_SPEC)
    GSPEC_PACK_SPEC(gloss.x, specscale.x)
#else
    GSPEC_PACK(gloss.x)
#endif
#endif

#ifdef DECAL_PASS1
#ifdef DECAL_GLOW
    vec3 gcolor = diffuse.rgb*colorparams.rgb;
#endif
#else
    gcolor.rgb = diffuse.rgb*colorparams.rgb;

#ifdef DECAL_REFLECT
#if !defined(DECAL_NORMALMAP) || !defined(DECAL_PARALLAX)
    vec3 camvecn = normalize(camvec);
#endif
    float invfresnel = dot(camvecn, nvec);
    vec3 rvec = 2.0*nvec*invfresnel - camvecn;
    vec3 reflect = texture(envmap, rvec).rgb * diffuse.a;
#ifdef DECAL_REFLECT_SPECMAP
    vec3 rmod = envscale.xyz*spec;
#else
#define rmod envscale.xyz
#endif
    reflect *= diffuse.a;
    gcolor.rgb = mix(gcolor.rgb, reflect, rmod*clamp(1.0 - invfresnel, 0.0, 1.0));
#endif
#endif

#ifdef DECAL_GLOW
    GGLOW_PACK(glow)
#endif

#ifndef DECAL_PASS0
    vec3 normal = normalize(nvec);
#ifdef DECAL_GLOW
    GNORMAL_PACK_BLEND(normal, GGLOW_PACKNORM)
#else
    GNORMAL_PACK(normal)
#endif
#endif

    float inside = clamp(texcoord0.z, 0.0, 1.0) * clamp(texcoord0.w, 0.0, 1.0);
    float alpha = inside * diffuse.a * colorparams.a;
#if defined(DECAL_PASS0)
    gcolor.rgb *= inside;
    gcolor.a *= alpha;
    gcolorblend = vec4(alpha);
#elif defined(DECAL_PASS1)
#if USEPACKNORM
    gnormal.a = alpha * bumpblend.x;
#else
    gnormalblend = vec4(alpha * bumpblend.x);
#endif
#else
    gcolor.rgb *= inside;
    gcolor.a = alpha;
#ifdef DECAL_KEEPNORMALS
    gnormal = vec4(0.0);
#else
    gnormal.rgb *= alpha * bumpblend.x;
    gnormal.a = alpha * bumpblend.x;
#endif
#endif
}
