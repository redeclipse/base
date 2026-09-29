// G-buffer normal packing and unpacking, the GLSL counterparts of gnormpack
// and unpacknorm in config/glsl/shared.cfg. Include with shader_include_fs
// after defining USEPACKNORM ($usepacknorm: 1 folds the glow blend weight
// into the normal's length and leaves gnormal.a to the shader, 0 writes it to
// gnormal.a). The shader declares the gnormal output for the packing macros.

#if USEPACKNORM
// Writes the unit normal n to gnormal.rgb.
#define GNORMAL_PACK(n) gnormal.rgb = n * 0.5 + 0.5;
// The same, scaled by the glow blend weight k (GGLOW_PACKNORM).
#define GNORMAL_PACK_BLEND(n, k) gnormal.rgb = n * sqrt(0.25 + 0.5*(k)) * 0.5 + 0.5;
#else
#define GNORMAL_PACK(n) gnormal.rgb = n*0.5 + 0.5; gnormal.a = 1.0;
#define GNORMAL_PACK_BLEND(n, k) gnormal.rgb = n*0.5 + 0.5; gnormal.a = (k);
#endif

// Turns k, the squared length of a normal GNORMAL_PACK_BLEND wrote (after
// n = n*2.0 - 1.0), back into the blend weight in 0..1; unpacknorm.
#define GNORMAL_UNPACK_SCALE(k) k = clamp(k * 2.02 - 0.51, 0.0, 1.0);
