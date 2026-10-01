// Deferred lighting: extensions, uniforms, the output and the point/spot
// shadow lookups. Included after deferredlight_defs.glsl and before
// shared/smfilter.glsl, whose filters read tex4 and shadowatlasscale.
#if defined(DL_SAMPLESHADING) && __VERSION__ < 400
#extension GL_ARB_sample_shading : enable
#endif
#if defined(DL_EDGEDETECT) && GLEXT_SAMPLES_IDENTICAL
#extension GL_EXT_shader_samples_identical : enable
#endif

// The g-buffer: colour, normal, depth, and glow for transparent rows. This is
// the first declaration, so it can't sit inside an #if (see "Porting a
// generator" in doc/shader-reference.md).
#ifdef DL_MSAA
#define DL_GSAMPLER sampler2DMS
#else
#define DL_GSAMPLER sampler2DRect
#endif
#if DL_TRANSPARENT
#define DL_GGLOW , tex2
#else
#define DL_GGLOW
#endif
uniform DL_GSAMPLER tex0, tex1, tex3 DL_GGLOW;

#ifdef SMFILTER
#if defined(SMFILTER_GATHER5) || defined(SMFILTER_GATHER3)
#if USETEXGATHER > 1
uniform sampler2DShadow tex4;
#else
uniform sampler2D tex4;
#endif
#else
uniform sampler2DRectShadow tex4;
#endif
#endif
#if DL_NUMLIGHTS
uniform vec4 lightpos[DL_NUMLIGHTS];
uniform vec4 lightcolor[DL_NUMLIGHTS];
#if DL_SPOTLIGHT
uniform vec4 spotparams[DL_NUMLIGHTS];
#endif
#ifdef DL_LIGHTSHADOW
uniform vec4 shadowparams[DL_NUMLIGHTS];
uniform vec2 shadowoffset[DL_NUMLIGHTS];
#endif
#endif
#if DL_NUMSPLITS
uniform vec4 csmtc[DL_NUMSPLITS];
uniform vec3 csmoffset[DL_NUMSPLITS];
uniform vec2 csmz;
#endif
#ifdef DL_CSM
uniform mat3 csmmatrix;
uniform vec3 sunlightdir;
uniform vec3 sunlightcolor;
#ifdef DL_RH
uniform vec3 skylightcolor;
uniform float giscale, rhnudge, rhbounds;
uniform vec4 rhtc[DL_NUMRH];
#ifdef DL_RHBLEND
uniform vec4 rhblendtc[DL_NUMRH];
uniform float rhblendedge;
#endif
uniform sampler3D tex6, tex7, tex8, tex9;
#endif
#endif
#if DL_COLORSHADOW
uniform sampler2DRect tex10;
#define sunshadowtype vec3
#define filtersunshadow(tc) (filtershadow(tc) * filtercolorshadow(tex10, tc))
#else
#define sunshadowtype float
#define filtersunshadow filtershadow
#endif
#if DL_COLORSHADOW > 1
uniform sampler2DRect tex11;
#define lightshadowtype vec3
#define filterlightshadow(tc) (filtershadow(tc) * filtercolorshadow(tex11, tc))
#else
#define lightshadowtype float
#define filterlightshadow filtershadow
#endif
uniform vec3 camera;
uniform mat4 worldmatrix;
uniform vec4 fogdir;
uniform vec3 fogcolor;
uniform vec2 fogdensity;
uniform vec4 radialfogscale;
uniform vec2 shadowatlasscale;
uniform vec4 lightscale;
uniform vec4 darknessenv;
#if DL_USEAO
uniform sampler2DRect tex5; uniform vec2 aoscale; uniform vec4 aoparams;
#endif
uniform vec3 gdepthscale;
uniform vec3 gdepthunpackparams;
fragdata(0) vec4 fragcolor;

// Shadow atlas coordinates for a light at dir from the surface.
#ifdef DL_LIGHTSHADOW
#if DL_SPOTLIGHT
vec3 getspottc(vec3 dir, float spotdist, vec4 spotparams, vec4 shadowparams, vec2 shadowoffset, float distbias)
{
    vec2 mparams = shadowparams.xy / max(spotdist + distbias, 1e-5);
    return vec3((dir.xy - spotparams.xy*(spotdist + (spotparams.z > 0.0 ? 1.0 : -1.0)*dir.z)*shadowparams.z) * mparams.x + shadowoffset, mparams.y + shadowparams.w);
}
#else
vec3 getshadowtc(vec3 dir, vec4 shadowparams, vec2 shadowoffset, float distbias)
{
    vec3 adir = abs(dir);
    float m = max(adir.x, adir.y), mz = max(adir.z, m);
    vec2 mparams = shadowparams.xy / max(mz + distbias, 1e-5);
    vec4 proj;
    if(adir.x > adir.y) proj = vec4(dir.zyx, 0.0); else proj = vec4(dir.xzy, 1.0);
    if(adir.z > m) proj = vec4(dir, 2.0);
    return vec3(proj.xy * mparams.x + vec2(proj.w, step(0.0, proj.z)) * shadowparams.z + shadowoffset, mparams.y + shadowparams.w);
}
#endif
#endif
