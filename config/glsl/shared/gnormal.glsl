// G-buffer normal packing, the GLSL counterpart of gnormpack in
// config/glsl/shared.cfg. Include with shader_include_fs after defining
// USEPACKNORM ($usepacknorm: 1 folds the glow blend weight into the normal's
// length and leaves gnormal.a to the shader, 0 writes it to gnormal.a). The
// shader declares the gnormal output.

#if USEPACKNORM
// Writes the unit normal n to gnormal.rgb.
#define GNORMAL_PACK(n) gnormal.rgb = n * 0.5 + 0.5;
// The same, scaled by the glow blend weight k (GGLOW_PACKNORM).
#define GNORMAL_PACK_BLEND(n, k) gnormal.rgb = n * sqrt(0.25 + 0.5*(k)) * 0.5 + 0.5;
#else
#define GNORMAL_PACK(n) gnormal.rgb = n*0.5 + 0.5; gnormal.a = 1.0;
#define GNORMAL_PACK_BLEND(n, k) gnormal.rgb = n*0.5 + 0.5; gnormal.a = (k);
#endif
