// Touch textures, the shader half of touchtexstate in renderva.cpp. A face
// with a touch texture shows it over its own texture on the tiles that have
// been touched: the face is drawn first, faded out by how lit its tile is,
// then its touch texture adds itself back in by the same amount. A tile is
// one repeat of the touch texture on one plane, kept in a table hashed by
// where it lies on the touch texture and which texture it is, so fragments
// carry their world position in touchpos (the vertex stage writes
// vvertex.xyz) to find their tile and tell which plane they lie on.
// Include with shader_include_fs after defining USEPACKNORM ($usepacknorm).
uniform vec4 touchtexparams; // 1 drawing a face under its touch texture or 2 drawing the touch texture, else 0; its index; the time now; how far to probe
uniform vec4 touchtexgen[2]; // where a position lies on the touch texture, in repeats
uniform vec2 touchtexmatch; // how far from a lit tile's plane a fragment may be and how closely it must face the same way
uniform sampler2D touchtexmap;
in vec3 touchpos;

float touchtexweight()
{
    if(touchtexparams.x < 0.5) return 1.0;
    vec3 n = cross(dFdx(touchpos), dFdy(touchpos));
    vec2 tile = floor(vec2(dot(touchtexgen[0].xyz, touchpos) + touchtexgen[0].w, dot(touchtexgen[1].xyz, touchpos) + touchtexgen[1].w));
    ivec2 size = textureSize(touchtexmap, 0);
    uint rowslots = uint(size.x / 3), mask = rowslots * uint(size.y) - 1u;
    uint h = (uint(int(tile.x)) * 73856093u) ^ (uint(int(tile.y)) * 19349663u) ^ (uint(int(touchtexparams.y)) * 83492791u);
    float lit = 0.0;
    for(int i = 0; i < int(touchtexparams.w); i++)
    {
        uint slot = (h + uint(i)) & mask;
        ivec2 at = ivec2(int(slot % rowslots) * 3, int(slot / rowslots));
        vec4 t = texelFetch(touchtexmap, at, 0);
        if(t.z < 0.0) break; // no tile lies past an empty slot
        if(t.z != touchtexparams.y || t.xy != tile) continue;
        vec4 p = texelFetch(touchtexmap, at + ivec2(1, 0), 0);
        if(abs(dot(p.xyz, touchpos) - t.w) < touchtexmatch.x && abs(dot(p.xyz, n)) > touchtexmatch.y*length(n))
        {
            lit = clamp((p.w - touchtexparams.z) / texelFetch(touchtexmap, at + ivec2(2, 0), 0).x, 0.0, 1.0);
            break;
        }
    }
    return touchtexparams.x > 1.5 ? lit : 1.0 - lit;
}

// The start of a world shader, so a touch texture leaves its unlit tiles
// before it is shaded.
#define TOUCHTEX_BEGIN \
    float touchw = touchtexweight(); \
    if(touchtexparams.x > 1.5 && touchw <= 0.0) discard;

// The end of a world shader: fades the g-buffer outputs by the tile. A
// see-through surface keeps its glow apart from its colour, so it also does
// gglow *= touchw.
#if USEPACKNORM
#define TOUCHTEX_APPLY \
    gcolor *= touchw; \
    gnormal.rgb *= touchw; \
    if(touchtexparams.x > 1.5) gnormal.a = 0.0;
#else
#define TOUCHTEX_APPLY \
    gcolor *= touchw; \
    gnormal *= touchw;
#endif
