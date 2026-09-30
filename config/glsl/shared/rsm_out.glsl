// Reflective shadow map outputs: the sunlit colour and the normal packed to
// 0..1. The RSM is what the radiance hints (config/glsl/gi/) gather light
// from. Used by rsmsky; world/rsm.frag and the model RSM shaders write the
// same two outputs.
fragdata(0) vec4 gcolor;
fragdata(1) vec4 gnormal;
