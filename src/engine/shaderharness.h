// Shader equivalence harness, see tools/harness/shaders.ps1 and
// doc/superpowers/specs/2026-09-25-shader-equivalence-harness-design.md.
#ifndef SHADERHARNESS_H
#define SHADERHARNESS_H

#ifdef DEBUG_UTILS
namespace shaderharness
{
    static const ullong FNVBASIS = 0xcbf29ce484222325ULL;

    // 64-bit FNV-1a. Pass the previous result as h to hash several buffers as one.
    ullong fnv1a(const void *data, size_t len, ullong h = FNVBASIS);
    // Not an fnv1a overload: fnv1a(buf, len) would bind len to h.
    ullong fnv1astr(const char *str, ullong h = FNVBASIS);

    // 16 lowercase hex digits and a terminator.
    void hexhash(ullong h, char out[17]);

    // Deterministic bench inputs: the same (name, index, seed) gives the same
    // value on every run, and in both programs of a comparison.
    float benchfloat(const char *name, int index, int seed); // in [0.1, 1.0]
    int benchint(const char *name, int index, int seed);     // in [1, 4]

    // GLSL spelling of a reflected type, or "0x<hex>" for one not listed.
    const char *gltypename(GLenum type);
    // Scalars per element (mat4 is 16); 0 for a type the bench cannot feed.
    int gltypecomponents(GLenum type);
    bool issamplertype(GLenum type);
}

// Implemented in shader.cpp, which owns the shader table.
extern Shader *findstagesource(Shader &s, GLenum type);
extern bool composeglslsource(Shader &s, GLenum type, vector<char> &out);
extern void scanfragdatalocs(Shader &s, vector<FragDataLoc> &out);
extern void forceallshaders();
extern void collectshaders(vector<Shader *> &out);
#endif

#endif
