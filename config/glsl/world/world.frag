// World geometry into the g-buffer, without a normal map (worldshader);
// bump.vert/.frag are the normal-mapped counterpart (bumpshader). Both take
// these defines from worldvariantshader/bumpvariantshader in
// config/glsl/world.cfg (the type letter in brackets):
//   WORLD_REFLECT          envmap reflection (r)
//   WORLD_REFLECT_SPECMAP  reflection scaled by the spec map (R)
//   WORLD_SPEC             spec (s)
//   WORLD_SPECMAP          spec map (S)
//   WORLD_GLOW             glow (g)
//   WORLD_PULSEGLOW        pulse glow (G)
//   WORLD_BLEND            blend-map layer (b), variant row 0
//   WORLD_ALPHA            transparent (a), variant row 1
//   WORLD_REFRACT          refractive (A), with WORLD_ALPHA
//   WORLD_ALPHAMASK        alpha mask (m)
//   WORLD_TRIPLANAR        triplanar texturing (T)
//   WORLD_DETAIL           detail textures on the triplanar z axis (d)
//   WORLD_DISPLACE         displacement (v)
//   WORLD_PARALLAX         parallax (p), bump only
//   MSAA_LIGHT             $msaalight
//   MSAA_SAMPLES           $msaasamples
//   GDEPTH_FORMAT          $gdepthformat
//   USEPACKNORM            $usepacknorm
// world_defs.glsl derives the rest. The outputs come first, from the
// include the gbufferoutputs alias picks (config/glsl/shared/).
#ifdef WORLD_REFRACT
uniform GFETCH_SAMPLER currentlight;
GDEPTH_UNPACK_DECLS
uniform vec4 refractparams;
#endif
uniform vec4 colorparams;
uniform sampler2D diffusemap;
#if WORLD_MSAADEPTH
uniform float hashid;
#endif
varying vec3 nvec;
#if GDEPTH_FORMAT || WORLD_MSAADEPTH
GBUFFER_DEPTH_DECLS
#endif
#ifdef WORLD_TRIPLANAR
varying vec2 texcoordx, texcoordy, texcoordz;
#ifdef WORLD_DISPLACE
varying vec2 dispcoordx0, dispcoordy0, dispcoordz0, dispcoordx1, dispcoordy1, dispcoordz1;
#endif
#ifdef WORLD_DETAIL
uniform sampler2D diffusedetail;
#endif
#else
varying vec2 texcoord0;
#ifdef WORLD_DISPLACE
varying vec2 dispcoord0, dispcoord1;
#endif
#endif
#ifdef WORLD_GLOW
uniform sampler2D glowmap;
#endif
#ifdef WORLD_PULSEGLOW
flat varying float pulse;
#endif
#ifdef WORLD_REFLECT
uniform samplerCube envmap; varying vec3 camvec;
#endif
#ifdef WORLD_BLEND
uniform float blendlayer;
uniform sampler2D blendmap;
varying vec2 texcoord1;
#endif
#ifdef WORLD_DISPLACE
uniform sampler2D dispmap;
#endif

void main(void)
{
    vec3 normal = normalize(nvec);

#ifdef WORLD_TRIPLANAR
    vec3 triblend = max(abs(normal) - triplanarbias.xyz, 0.001);
    triblend *= triblend;
    triblend /= triblend.x + triblend.y + triblend.z;

#ifdef WORLD_DISPLACE
    vec3 dispx = WORLD_DISP(dispcoordx0, dispcoordx1);
    vec3 dispy = WORLD_DISP(dispcoordy0, dispcoordy1);
    vec3 dispz = WORLD_DISP(dispcoordz0, dispcoordz1);
#endif
    vec4 diffusex = texture2D(diffusemap, WORLD_TC(texcoordx, dispx));
    vec4 diffusey = texture2D(diffusemap, WORLD_TC(texcoordy, dispy));
    vec4 diffusez = texture2D(WORLD_DIFFUSEZ, WORLD_TC(texcoordz, dispz));
    vec4 diffuse = diffusex*triblend.x + diffusey*triblend.y + diffusez*triblend.z;
#else
#ifdef WORLD_DISPLACE
    vec3 disp = WORLD_DISP(dispcoord0, dispcoord1);
#endif
    vec4 diffuse = texture2D(diffusemap, WORLD_TC(texcoord0, disp));
#endif

    gcolor.rgb = diffuse.rgb*colorparams.rgb;

#ifdef WORLD_REFLECT
    vec3 camvecn = normalize(camvec);
    float invfresnel = dot(camvecn, normal);
    vec3 rvec = 2.0*normal*invfresnel - camvecn;
    vec3 reflect = textureCube(envmap, rvec).rgb;
#ifdef WORLD_REFLECT_SPECMAP
    vec3 rmod = envscale.xyz*diffuse.a;
#else
#define rmod envscale.xyz
#endif
    gcolor.rgb = mix(gcolor.rgb, reflect, envmin.xyz + rmod * clamp(1.0 - invfresnel, 0.0, 1.0));
#endif

#ifdef WORLD_GLOW
#ifdef WORLD_TRIPLANAR
    vec3 glowx = texture2D(glowmap, WORLD_TC(texcoordx, dispx)).rgb;
    vec3 glowy = texture2D(glowmap, WORLD_TC(texcoordy, dispy)).rgb;
    vec3 glowz = texture2D(glowmap, WORLD_TC(texcoordz, dispz)).rgb;
    vec3 glow = glowx*triblend.x + glowy*triblend.y + glowz*triblend.z;
#else
    vec3 glow = texture2D(glowmap, WORLD_TC(texcoord0, disp)).rgb;
#endif
#ifdef WORLD_PULSEGLOW
    glow *= mix(glowcolor.xyz, pulseglowcolor.xyz, pulse);
#else
    glow *= glowcolor.xyz;
#endif
#ifdef WORLD_ALPHAMASK
    glow *= diffuse.a * colorparams.a;
#endif
#ifdef WORLD_ALPHA
    gglow.rgb = glow;
#else
    GGLOW_PACK(glow)
#define packnorm GGLOW_PACKNORM
#endif
#elif defined(WORLD_ALPHA)
    gglow.rgb = vec3(0.0);
#endif

#ifdef WORLD_ALPHA
#ifdef WORLD_REFRACT
    vec3 rlight = gfetch(currentlight, gl_FragCoord.xy).rgb;
#ifdef WORLD_ALPHAMASK
    gcolor.rgb *= diffuse.a;
    gglow.rgb += rlight * refractparams.xyz * (1.0 - colorparams.a * diffuse.a);
#else
    gglow.rgb += rlight * refractparams.xyz * (1.0 - colorparams.a);
#endif
#elif defined(WORLD_ALPHAMASK)
    gcolor.rgb *= diffuse.a;
#define packnorm diffuse.a * colorparams.a
#else
#define packnorm colorparams.a
#endif
#endif

#ifdef packnorm
    GNORMAL_PACK_BLEND(normal, packnorm)
#else
    GNORMAL_PACK(normal)
#endif

#if WORLD_MSAADEPTH
    GBUFFER_PACK_DEPTH_HASH(hashid)
#else
    GBUFFER_PACK_DEPTH
#endif

#ifdef WORLD_BLEND
    float blend = abs(texture2D(blendmap, texcoord1).r - blendlayer);
    gcolor.rgb *= blend;
    gnormal.rgb *= blend;
    gnormal.a *= blendlayer;
#endif

#if defined(WORLD_SPEC) && defined(WORLD_BLEND)
    GSPEC_PACK_SPEC_BLEND(gloss.x, WORLD_SPECSCALE, blendlayer, blend)
#elif defined(WORLD_SPEC)
    GSPEC_PACK_SPEC(gloss.x, WORLD_SPECSCALE)
#elif defined(WORLD_BLEND)
    GSPEC_PACK_BLEND(gloss.x, blendlayer)
#else
    GSPEC_PACK(gloss.x)
#endif
}
