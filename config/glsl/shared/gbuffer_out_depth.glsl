// G-buffer outputs with a depth target ($gdepthformat 1 or more): colour,
// normal, depth and glow. See gbuffer_out.glsl.
fragdata(0) vec4 gcolor;
fragdata(1) vec4 gnormal;
fragdata(2) vec4 gdepth;
fragdata(3) vec4 gglow;
