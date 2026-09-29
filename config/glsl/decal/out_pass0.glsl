// Decal outputs, pass 0 of the dual-source path: colour, blended by alpha.
// A separate file per output set because the engine reads these declarations
// from the text, whatever the #if around them; see decal.frag.
fragdata(0) vec4 gcolor;
fragblend(0) vec4 gcolorblend;
