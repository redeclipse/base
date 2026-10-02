// Separable bilateral filter taps, shared by the AO filter (ao/bilateral.frag)
// and the volumetric light filter (volumetric/bilateral.frag). Include with
// shader_include_fs after defining:
//   BILATERAL_X         filter along x, else along y
//   BILATERAL_REDUCE    log2 of the depth buffer's scale over the filtered one
//   TEXRECT_MINOFFSET, TEXRECT_MAXOFFSET  $mintexrectoffset, $maxtexrectoffset
// The shader #defines tc and depthtc (where the filtered buffer tex0 and the
// depth buffer are read) and BILATERAL_DEPTHTEX (the depth sampler), and
// provides gfetch/gfetchoffset for it (config/glsl/shared/gfetch.glsl).
//
// Offset fetches need a constant offset inside the TEXRECT limits, so each tap
// picks texvaloffset/depthvaloffset or texval/depthval with BILATERAL_FITS.
#ifdef BILATERAL_X
#define tapvec(type, i) type(i, 0.0)
#else
#define tapvec(type, i) type(0.0, i)
#endif
#define texval(i) texture(tex0, tc + tapvec(vec2, i))
#define texvaloffset(i) texture2DRectOffset(tex0, tc, tapvec(ivec2, i))
#define depthval(i) gfetch(BILATERAL_DEPTHTEX, depthtc + tapvec(vec2, i))
#define depthvaloffset(i) gfetchoffset(BILATERAL_DEPTHTEX, depthtc, tapvec(ivec2, i))

// Whether an offset fits an offset fetch.
#define BILATERAL_FITS(o) ((o) >= TEXRECT_MINOFFSET && (o) <= TEXRECT_MAXOFFSET)
// A tap's depth offset is its offset times BILATERAL_DEPTHSCALE
// (2^BILATERAL_REDUCE) with the same sign, so the depth offset fitting implies
// the tap offset fits too (the limits always contain 0).
#define BILATERAL_DEPTHSCALE (1 << BILATERAL_REDUCE)
