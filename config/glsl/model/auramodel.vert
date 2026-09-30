// Model auras, the silhouette edges drawn around models (auramodelshader):
// auramodel, alphaauramodel (a), windauramodel (aw) and windsolidauramodel
// (w), each with skeletal variants. The transparent model depth
// (transdepthmodelshader, transdepthmodel.frag) draws with it too. See
// model_defs.glsl for the type defines.
attribute vec4 vvertex;
#ifdef MODEL_SKELETAL
SKELANIM_DECLS
#endif
uniform mat4 modelmatrix;
uniform vec2 lineardepthscale;
attribute vec2 vtexcoord0;
uniform vec3 texscroll;
varying vec2 texcoord0;
varying float lineardepth;
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

    lineardepth = -dot(lineardepthscale, gl_Position.zw);
    MODEL_TEXCOORD
}
