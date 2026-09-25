// Shader equivalence harness: corpus dump (shaderdumpall) and pixel bench
// (shaderbench). See tools/harness/shaders.ps1. Test-only, like ui.cpp's
// uidumptree: nothing here runs unless the harness asks for it.
#include "engine.h"
#include "shaderharness.h"

#ifdef DEBUG_UTILS
namespace shaderharness
{
    ullong fnv1a(const void *data, size_t len, ullong h)
    {
        const uchar *p = (const uchar *)data;
        for(size_t i = 0; i < len; i++) { h ^= p[i]; h *= 0x100000001b3ULL; }
        return h;
    }

    ullong fnv1astr(const char *str, ullong h) { return fnv1a(str, strlen(str), h); }

    void hexhash(ullong h, char out[17])
    {
        static const char digits[] = "0123456789abcdef";
        for(int i = 15; i >= 0; i--) { out[i] = digits[h&0xF]; h >>= 4; }
        out[16] = '\0';
    }

    static ullong benchhash(const char *name, int index, int seed)
    {
        ullong h = fnv1astr(name);
        h = fnv1a(&index, sizeof(index), h);
        return fnv1a(&seed, sizeof(seed), h);
    }

    float benchfloat(const char *name, int index, int seed)
    {
        return 0.1f + 0.9f*float((benchhash(name, index, seed)>>40)&0xFFFFFF)/float(0xFFFFFF);
    }

    int benchint(const char *name, int index, int seed)
    {
        return 1 + int((benchhash(name, index, seed)>>32)&3);
    }

    static const struct { GLenum type; const char *name; int components; bool sampler; } gltypes[] =
    {
        { GL_FLOAT, "float", 1, false }, { GL_FLOAT_VEC2, "vec2", 2, false }, { GL_FLOAT_VEC3, "vec3", 3, false }, { GL_FLOAT_VEC4, "vec4", 4, false },
        { GL_INT, "int", 1, false }, { GL_INT_VEC2, "ivec2", 2, false }, { GL_INT_VEC3, "ivec3", 3, false }, { GL_INT_VEC4, "ivec4", 4, false },
        { GL_UNSIGNED_INT, "uint", 1, false }, { GL_UNSIGNED_INT_VEC2, "uvec2", 2, false }, { GL_UNSIGNED_INT_VEC3, "uvec3", 3, false }, { GL_UNSIGNED_INT_VEC4, "uvec4", 4, false },
        { GL_BOOL, "bool", 1, false }, { GL_BOOL_VEC2, "bvec2", 2, false }, { GL_BOOL_VEC3, "bvec3", 3, false }, { GL_BOOL_VEC4, "bvec4", 4, false },
        { GL_FLOAT_MAT2, "mat2", 4, false }, { GL_FLOAT_MAT3, "mat3", 9, false }, { GL_FLOAT_MAT4, "mat4", 16, false },
        { GL_SAMPLER_2D, "sampler2D", 1, true }, { GL_SAMPLER_3D, "sampler3D", 1, true }, { GL_SAMPLER_CUBE, "samplerCube", 1, true },
        { GL_SAMPLER_2D_SHADOW, "sampler2DShadow", 1, true }, { GL_SAMPLER_2D_RECT, "sampler2DRect", 1, true },
        { GL_SAMPLER_2D_RECT_SHADOW, "sampler2DRectShadow", 1, true }, { GL_SAMPLER_2D_ARRAY, "sampler2DArray", 1, true },
        { GL_SAMPLER_2D_ARRAY_SHADOW, "sampler2DArrayShadow", 1, true }, { GL_SAMPLER_CUBE_SHADOW, "samplerCubeShadow", 1, true },
        { GL_SAMPLER_2D_MULTISAMPLE, "sampler2DMS", 1, true }, { GL_SAMPLER_2D_MULTISAMPLE_ARRAY, "sampler2DMSArray", 1, true },
        { GL_SAMPLER_BUFFER, "samplerBuffer", 1, true },
        { GL_INT_SAMPLER_2D, "isampler2D", 1, true }, { GL_UNSIGNED_INT_SAMPLER_2D, "usampler2D", 1, true },
        { GL_INT_SAMPLER_2D_RECT, "isampler2DRect", 1, true }, { GL_UNSIGNED_INT_SAMPLER_2D_RECT, "usampler2DRect", 1, true }
    };

    const char *gltypename(GLenum type)
    {
        loopi(sizeof(gltypes)/sizeof(gltypes[0])) if(gltypes[i].type == type) return gltypes[i].name;
        static string unknown;
        formatstring(unknown, "0x%X", type);
        return unknown;
    }

    int gltypecomponents(GLenum type)
    {
        loopi(sizeof(gltypes)/sizeof(gltypes[0])) if(gltypes[i].type == type) return gltypes[i].components;
        return 0;
    }

    bool issamplertype(GLenum type)
    {
        loopi(sizeof(gltypes)/sizeof(gltypes[0])) if(gltypes[i].type == type) return gltypes[i].sampler;
        return false;
    }
}
#endif
