// Screen-quad texcoords, the GLSL counterpart of screentexcoord in
// config/glsl/shared.cfg. Include with shader_include_vs. The shader declares
// vvertex and the screentexcoord<n> uniforms the macros it uses read; the
// engine sets them with setscreentexcoord (rendergl.cpp).
#define vtexcoord0 (vvertex.xy * screentexcoord0.xy + screentexcoord0.zw)
#define vtexcoord1 (vvertex.xy * screentexcoord1.xy + screentexcoord1.zw)
