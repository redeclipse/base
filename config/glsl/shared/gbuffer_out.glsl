// G-buffer outputs without a depth target ($gdepthformat 0): colour, normal
// and glow, as ginterpfrag in config/glsl/shared.cfg declared them. The
// engine and the harness read output declarations from the text whatever
// #if surrounds them, so the two sets are separate files; the gbufferoutputs
// alias (shared.cfg) picks one. See gbuffer.glsl.
fragdata(0) vec4 gcolor;
fragdata(1) vec4 gnormal;
fragdata(2) vec4 gglow;
