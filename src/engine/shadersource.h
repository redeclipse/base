// Shader source loader: shader_new and variantshader_new build a shader from
// #defines, include files and .vert/.frag files under config/glsl/, then hand
// the assembled text to shader()/variantshader(). See doc/shader-reference.md,
// "Shader source files".
#ifndef SHADERSOURCE_H
#define SHADERSOURCE_H

namespace shadersource
{
    // A path shader_source and shader_include_* may read: under config/glsl/,
    // forward slashes only, and no empty, "." or ".." component.
    bool validpath(const char *path);
    // A macro name ([A-Za-z_][A-Za-z0-9_]*) and a value that fits on one line.
    bool validdefine(const char *name, const char *value);
    // "#define <name> <value>\n", or "#define <name>\n" when value is empty.
    void appenddefine(vector<char> &out, const char *name, const char *value);
    // Appends text without its '\r's, then '\n' unless it already ends with
    // one, so a CRLF checkout assembles to the same bytes as an LF one.
    void appendtext(vector<char> &out, const char *text);
    // One stage, NUL-terminated: the defines, each include, then the body. A
    // NULL body (no file for this stage) gives "", which variantshader reads
    // as "reuse the parent's stage".
    void assemblestage(vector<char> &out, const vector<char> &defines, const vector<const char *> &includes, const char *body);
}

#endif
