// Radiance hint outputs, one 3D texture slice each: the spherical harmonics
// of the red, green and blue light (rhr, rhg, rhb) and of the sky occlusion
// (rha). The coefficients are stored biased by 0.5 with the weight in alpha,
// so RH_ZERO is a hint that holds nothing. Shared by every radiancehints*
// shader (config/glsl/gi.cfg).
#define RH_ZERO vec4(0.5, 0.5, 0.5, 0.0)
fragdata(0) vec4 rhr;
fragdata(1) vec4 rhg;
fragdata(2) vec4 rhb;
fragdata(3) vec4 rha;
