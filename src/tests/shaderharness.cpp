#include "engine.h"
#include "shaderharness.h"

// Shader equivalence harness helpers, see tools/harness/shaders.ps1.
void testshaderharness()
{
    using namespace shaderharness;

    // Published FNV-1a 64-bit test vectors.
    ASSERT(fnv1astr("") == 0xcbf29ce484222325ULL);
    ASSERT(fnv1astr("a") == 0xaf63dc4c8601ec8cULL);
    ASSERT(fnv1astr("foobar") == 0x85944171f73967e8ULL);

    // Chaining hashes the concatenation, which is how a blob's files are hashed as one.
    ASSERT(fnv1astr("bar", fnv1astr("foo")) == fnv1astr("foobar"));
    // A (buffer, length) call must hash the bytes, not treat the length as a basis.
    const char *buf = "foobar";
    int len = 6;
    ASSERT(fnv1a(buf, len) == fnv1astr("foobar"));

    char hex[17];
    hexhash(0x85944171f73967e8ULL, hex);
    ASSERT(!strcmp(hex, "85944171f73967e8"));
    hexhash(1, hex);
    ASSERT(!strcmp(hex, "0000000000000001"));

    // Bench inputs are a pure function of (name, index, seed), and stay in range.
    ASSERT(benchfloat("gloss", 0, 1) == benchfloat("gloss", 0, 1));
    ASSERT(benchfloat("gloss", 0, 1) != benchfloat("gloss", 0, 2));
    ASSERT(benchfloat("gloss", 0, 1) != benchfloat("gloss", 1, 1));
    loopi(256)
    {
        float f = benchfloat("x", i, 3);
        ASSERT(f >= 0.1f && f <= 1.0f);
        int n = benchint("x", i, 3);
        ASSERT(n >= 1 && n <= 4);
    }

    ASSERT(!strcmp(gltypename(GL_FLOAT_VEC4), "vec4"));
    ASSERT(!strcmp(gltypename(GL_SAMPLER_2D_RECT), "sampler2DRect"));
    ASSERT(!strcmp(gltypename(0x1234), "0x1234"));
    ASSERT(gltypecomponents(GL_FLOAT_VEC3) == 3);
    ASSERT(gltypecomponents(GL_FLOAT_MAT4) == 16);
    ASSERT(gltypecomponents(GL_INT) == 1);
    ASSERT(gltypecomponents(0x1234) == 0);
    ASSERT(issamplertype(GL_SAMPLER_2D_SHADOW));
    ASSERT(!issamplertype(GL_FLOAT_VEC4));

    conoutf(colourwhite, "testshaderharness: ok");
}
