// Reflective shadow map outputs: the sunlit colour and the normal packed to
// 0..1. The RSM is what the radiance hints (config/glsl/gi/) gather light
// from. Used by rsmsky and the model RSM shaders (model/rsmmodel.frag);
// world/rsm.frag declares the same two outputs itself.
fragdata(0) vec4 gcolor;
fragdata(1) vec4 gnormal;
