// Geometry template instances (src/engine/geomtemplate.cpp, renderva.cpp):
// world geometry through the rows of a 4x3 transform. Instanced draws feed
// per-instance rows and the inverse of their uniform scale; everything else
// gets the attributes' constant values, identity rows and 1
// (gle::resetinstance), for which INSTANCE_POS and INSTANCE_DIR return their
// input bit for bit. Include with shader_include_vs.
in vec4 vinstance0, vinstance1, vinstance2;
in float vinstancescale;
#define INSTANCE_POS(v) vec4(dot(vinstance0, v), dot(vinstance1, v), dot(vinstance2, v), v.w)
#define INSTANCE_DIR(n) (vec3(dot(vinstance0.xyz, n), dot(vinstance1.xyz, n), dot(vinstance2.xyz, n))*vinstancescale)
