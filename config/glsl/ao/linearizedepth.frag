// Linearizes the g-buffer depth into the reduced AO depth buffer (renderao).
// Engine state, from aoshaderdefines in config/glsl/ao.cfg:
//   MSAA_SAMPLES     $msaasamples: nonzero reads a multisampled g-buffer
//   GDEPTH_FORMAT    $gdepthformat: 0 hyperbolic, 1 packed RGB8, >1 linear float
//   AO_DEPTH_FORMAT  $aodepthformat: 0 packs into RGB8, nonzero writes a float
#if MSAA_SAMPLES
uniform sampler2DMS tex0;
#define gfetch(sampler, coords) texelFetch(sampler, ivec2(coords), 0)
#else
uniform sampler2DRect tex0;
#define gfetch(sampler, coords) texture2DRect(sampler, coords)
#endif
uniform vec3 gdepthscale;
uniform vec3 gdepthunpackparams;
uniform vec3 gdepthpackparams;
varying vec2 texcoord0;
fragdata(0) vec4 fragcolor;
void main(void)
{
#if AO_DEPTH_FORMAT == 0 && GDEPTH_FORMAT == 1
    fragcolor = gfetch(tex0, texcoord0);
#else
  #if GDEPTH_FORMAT > 1
    float depth = gfetch(tex0, texcoord0).r;
  #elif GDEPTH_FORMAT == 1
    float depth = dot(gfetch(tex0, texcoord0).rgb, gdepthunpackparams);
  #else
    float depth = gdepthscale.x / (gfetch(tex0, texcoord0).r*gdepthscale.y + gdepthscale.z);
  #endif
  #if AO_DEPTH_FORMAT == 0
    vec3 packdepth = depth * gdepthpackparams;
    packdepth = vec3(packdepth.x, fract(packdepth.yz));
    packdepth.xy -= packdepth.yz * (1.0/255.0);
    fragcolor = vec4(packdepth, 1.0);
  #else
    fragcolor.r = depth;
  #endif
#endif
}
