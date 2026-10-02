// The blendmap lookup, the shader half of blendslice in octa.h and the
// textures built in blend.cpp; the GLSL counterpart of blendmapdefs in
// config/glsl/shared.cfg, keep the two in step. A blended face's fragments
// carry their world position and the face's orientation in blendpos (the
// vertex stage writes vec4(vvertex.xyz, vtexcoord0.z)); the face reads the
// slice just in front of it along its own axis. blendlookup takes the atlas
// sampler, which the shader declares.
uniform vec4 blendmapparams, blendindexparams, blendatlasparams;
uniform sampler2D blendindex;

// An entry's two halves, high then low: a high half of 65535 holds a value,
// otherwise the entry is an index.
vec2 blendindexfetch(float i)
{
    float row = floor((i + 0.5) * blendindexparams.y);
    float col = i - row * blendindexparams.x;
    return floor(texture(blendindex, vec2((col + 0.5) * blendindexparams.y, (row + 0.5) * blendindexparams.z)).rg * 65535.0 + 0.5);
}

float blendlookup(sampler2D atlas, vec4 bpos)
{
    float orient = floor(bpos.w + 0.5), dim = floor(orient * 0.5);
    float depth;
    vec2 across;
    if(dim < 0.5) { depth = bpos.x; across = bpos.yz; }
    else if(dim < 1.5) { depth = bpos.y; across = bpos.zx; }
    else { depth = bpos.z; across = bpos.xy; }
    float slice = orient - dim * 2.0 > 0.5 ? floor((depth + 4.0 - 0.0625) * 0.25) : floor((depth + 0.0625) * 0.25) - 1.0;
    float pagedepth = floor(slice / blendmapparams.w);
    vec2 pageacross = floor(across * 0.015625);
    vec3 page = dim < 0.5 ? vec3(pagedepth, pageacross.x, pageacross.y) : (dim < 1.5 ? vec3(pageacross.y, pagedepth, pageacross.x) : vec3(pageacross.x, pageacross.y, pagedepth));
    vec3 region = floor(page / blendmapparams.y);
    float rootsize = blendmapparams.x;
    if(min(page.x, min(page.y, page.z)) < 0.0 || max(region.x, max(region.y, region.z)) >= rootsize) return 1.0;
    vec3 inregion = page - region * blendmapparams.y, chunkpages = vec3(blendmapparams.y / blendmapparams.z);
    vec3 chunk = floor(inregion / chunkpages), inchunk = inregion - chunk * chunkpages;
    float tablesize = blendmapparams.z;
    vec2 entry = blendindexfetch(((dim * rootsize + region.z) * rootsize + region.y) * rootsize + region.x);
    if(entry.x >= 65535.0) return entry.y * (1.0/255.0);
    entry = blendindexfetch(entry.x * 65536.0 + entry.y + (chunk.z * tablesize + chunk.y) * tablesize + chunk.x);
    if(entry.x >= 65535.0) return entry.y * (1.0/255.0);
    entry = blendindexfetch(entry.x * 65536.0 + entry.y + (inchunk.z * tablesize + inchunk.y) * tablesize + inchunk.x);
    if(entry.x >= 65535.0) return entry.y * (1.0/255.0);
    entry = blendindexfetch(entry.x * 65536.0 + entry.y + slice - pagedepth * blendmapparams.w);
    if(entry.x >= 65535.0) return entry.y * (1.0/255.0);
    float tile = entry.x * 65536.0 + entry.y, row = floor((tile + 0.5) / blendatlasparams.x);
    vec2 texel = vec2(tile - row * blendatlasparams.x, row) * 18.0 + 1.0 + (across - pageacross * 64.0) * 0.25;
    return texture(atlas, texel * blendatlasparams.yz).r;
}
