// Volumetric light: ray marches the view ray through one point or spot
// light's sphere or cone, summing its attenuated (and shadowed) light, into
// the reduced volumetric buffer. Assembled after shared/gfetch.glsl,
// shared/gdepth.glsl, shared/smfilter.glsl and volumetric_steps.glsl.
//
// volumetricvariantshader (config/glsl/volumetric.cfg) defines:
//   VOL_ROW     variant row: -1 the parent (point light, no shadow), 0 shadowed
//               point light, 1 colour-shadowed point light, 2 spot light,
//               3 shadowed spot light, 4 colour-shadowed spot light
//   VOL_STEPS   ray march steps, 1..64 ($volsteps)
//   VOL_SHADOW  the row reads the shadow atlas (type letter p)
//   one define per filter letter, which picks the atlas the lights were
//   rendered to: SMFILTER_GATHER5 G, SMFILTER_GATHER3 g, SMFILTER_BILINEAR5 E,
//   SMFILTER_BILINEAR3 F, SMFILTER_ROTATED f, none for N
//   and SMFILTER_SINGLE and SMFILTER_COLOR for shared/smfilter.glsl, and the
//   engine state: GFETCH_MS ($msaalight), GDEPTH_FORMAT, USETEXGATHER.
#if VOL_ROW >= 2
#define VOL_SPOTLIGHT 1
#else
#define VOL_SPOTLIGHT 0
#endif
#if VOL_ROW == 1 || VOL_ROW == 4
#define VOL_COLORSHADOW 1
#else
#define VOL_COLORSHADOW 0
#endif

uniform GFETCH_SAMPLER currentdepth;
GDEPTH_UNPACK_DECLS
#ifdef VOL_SHADOW
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
#if VOL_COLORSHADOW
uniform sampler2DRect tex11;
#define lightshadowtype vec3
#else
#define lightshadowtype float
#endif
uniform vec4 lightpos;
uniform vec3 lightcolor;
#if VOL_SPOTLIGHT
uniform vec4 spotparams;
#endif
#ifdef VOL_SHADOW
uniform vec4 shadowparams;
uniform vec2 shadowoffset;
#endif
uniform vec3 camera;
uniform mat4 worldmatrix;
uniform vec4 fogdir;
uniform vec3 fogcolor;
uniform vec2 fogdensity;
uniform vec4 radialfogscale;
uniform vec2 shadowatlasscale;
uniform vec4 volscale;
uniform float volminstep;
uniform float voldistclamp;
uniform float volprefilter;
fragdata(0) vec4 fragcolor;

// Shadow atlas coordinates for the light at dir from the sample.
#ifdef VOL_SHADOW
#if VOL_SPOTLIGHT
vec3 getspottc(vec3 dir, float spotdist)
{
    vec2 mparams = shadowparams.xy / max(spotdist, 1e-5);
    return vec3((dir.xy - spotparams.xy*(spotdist + (spotparams.z < 0.0 ? -1.0 : 1.0)*dir.z)*shadowparams.z) * mparams.x + shadowoffset, mparams.y + shadowparams.w);
}
#else
vec3 getshadowtc(vec3 dir)
{
    vec3 adir = abs(dir);
    float m = max(adir.x, adir.y), mz = max(adir.z, m);
    vec2 mparams = shadowparams.xy / max(mz, 1e-5);
    vec4 proj;
    if(adir.x > adir.y) proj = vec4(dir.zyx, 0.0); else proj = vec4(dir.xzy, 1.0);
    if(adir.z > m) proj = vec4(dir, 2.0);
    return vec3(proj.xy * mparams.x + vec2(proj.w, step(proj.z, 0.0)) * shadowparams.z + shadowoffset, mparams.y + shadowparams.w);
}
#endif
#endif

void main(void)
{
    vec2 tc = gl_FragCoord.xy * volscale.xy;
    GDEPTH_UNPACK_POS(depth, pos, gfetch(currentdepth, tc), tc)
    vec3 ray = pos.xyz - camera;
    float dist2 = dot(ray, ray), invdist = inversesqrt(dist2), radialdist = min(dist2 * invdist, voldistclamp);
    ray *= invdist;
    vec3 camlight = lightpos.xyz - camera * lightpos.w;
    float camlight2 = dot(camlight, camlight), v = dot(camlight, ray), d = v*v + 1.0 - camlight2;
    lightshadowtype light = lightshadowtype(0.0);
    if(d > 0)
    {
        d = sqrt(d);
        float front = v - d, back = v + d;
#if VOL_SPOTLIGHT
        // Clip the ray to the cone.
        float spotangle = 1.0 - 1.0/spotparams.w, spotangle2 = spotangle*spotangle,
              rayspot = dot(ray, spotparams.xyz), camspot = dot(camlight, spotparams.xyz),
              qa = spotangle2 - rayspot*rayspot,
              qb = rayspot*camspot - spotangle2*v,
              qc = spotangle2*camlight2 - camspot*camspot,
              disc = qb*qb - qa*qc;
        if(disc > 0)
        {
            disc = abs(sqrt(disc)/qa);
            float t = -qb/qa, t0 = t - disc, t1 = t + disc;
            if(t0*rayspot < camspot) front = max(front, t1);
            else if(t1*rayspot < camspot) back = min(back, t0);
            else { front = max(front, t0); back = min(back, t1); }
#endif
        float maxspace = back - front, stepdist = maxspace * VOL_STEPSCALE;
        front = max(front, 0.0);
        back = min(back, radialdist * lightpos.w);
        float space = back - front;
        if(space > volminstep*stepdist)
        {
            float dither = dot(fract((gl_FragCoord.xy - 0.5).xyxy*vec4(0.5, 0.5, 0.25, 0.25)), vec4(0.375, 0.9375, 0.25, 0.125));
            vec3 lightdir = ray * (back + stepdist*dither) - camlight;
            vec3 raystep = ray * -stepdist;
            for(int i = 0; i < VOL_STEPS; i++)
            {
                lightdir += raystep;
#if VOL_SPOTLIGHT
                float lightdist2 = dot(lightdir, lightdir);
                float lightinvdist = inversesqrt(lightdist2);
                float spotdist = dot(lightdir, spotparams.xyz);
                float spotatten = 1.0 - (1.0 - lightinvdist * spotdist) * spotparams.w;
                if(spotatten > 0.0)
                {
                    float lightatten = clamp(1.0 - lightdist2 * lightinvdist, 0.0, 1.0) * spotatten;
#ifdef VOL_SHADOW
                    vec3 spottc = getspottc(lightdir, spotdist);
                    lightatten *= filtershadow(spottc);
#endif
#if VOL_COLORSHADOW
                    light += lightatten * filtercolorshadow(tex11, spottc);
#else
                    light += lightatten;
#endif
                }
#else
                float lightatten = clamp(1.0 - length(lightdir), 0.0, 1.0);
#ifdef VOL_SHADOW
                vec3 shadowtc = getshadowtc(lightdir);
                lightatten *= filtershadow(shadowtc);
#endif
#if VOL_COLORSHADOW
                light += lightatten * filtercolorshadow(tex11, shadowtc);
#else
                light += lightatten;
#endif
#endif
                space -= stepdist;
                if(space <= 0) break;
            }
            float fogcoord = front/lightpos.w;
            float foglerp = clamp(exp2(fogcoord*fogdensity.x)*fogdensity.y, 0.0, 1.0);
            light *= foglerp * stepdist;
#if VOL_SPOTLIGHT
            light /= min(maxspace, 1.0);
#endif
        }
#if VOL_SPOTLIGHT
        }
#endif
    }
    // Prefilter: where the distance varies less than volprefilter, average
    // each 2x2 quad to hide the dither pattern.
    vec2 weights = step(fwidth(radialdist), volprefilter) * (2.0*fract((gl_FragCoord.xy - 0.5)*0.5) - 0.5);
    light -= dFdx(light) * weights.x;
    light -= dFdy(light) * weights.y;
    fragcolor.rgb = light * lightcolor;
    fragcolor.a = 0.0;
}
