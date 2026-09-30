// Model halos, the outline drawn through walls (halomodelshader): halomodel,
// alphahalomodel (a), windhalomodel (aw) and their shimmer (0) forms, each
// with skeletal variants. See model_defs.glsl for the type defines.
attribute vec4 vvertex;
#ifdef MODEL_SKELETAL
SKELANIM_DECLS
#endif
uniform mat4 modelmatrix;
attribute vec2 vtexcoord0;
uniform vec3 texscroll;
varying vec2 texcoord0;
#ifdef MODEL_WIND
WIND_DECLS(camprojmatrix)
WIND_FUNCS
#endif

ROTATEUV_FUNC

void main(void)
{
#ifdef MODEL_SKELETAL
    SKELANIM_BLEND
    SKELANIM_POS
#else
#define mpos vvertex
#endif

    gl_Position = modelmatrix * mpos;

#ifdef MODEL_WIND
    WIND_ANIM(camprojmatrix)
#endif

    MODEL_TEXCOORD
}
