#include "engine.h"
#include "shadersource.h"

// out holds exactly expected and its terminator.
static bool assembled(const vector<char> &out, const char *expected)
{
    return out.length() == int(strlen(expected)) + 1 && !strcmp(out.getbuf(), expected);
}

// shader_new loader helpers, see engine/shadersource.h.
void testshadersource()
{
    using namespace shadersource;

    ASSERT(validpath("config/glsl/ao/ao.frag"));
    ASSERT(validpath("config/glsl/x.vert"));
    ASSERT(validpath("config/glsl/..x.frag")); // a dotted name, not a ".." component
    ASSERT(!validpath("config/glsl/"));
    ASSERT(!validpath("config/glsl"));
    ASSERT(!validpath("config/glslx/a.frag"));
    ASSERT(!validpath("data/x.frag"));
    ASSERT(!validpath("/config/glsl/x.frag"));
    ASSERT(!validpath("C:/config/glsl/x.frag"));
    ASSERT(!validpath("config/glsl/../autoexec.cfg"));
    ASSERT(!validpath("config/glsl/ao/../../x.frag"));
    ASSERT(!validpath("config/glsl/./x.frag"));
    ASSERT(!validpath("config/glsl//x.frag"));
    ASSERT(!validpath("config/glsl/ao/"));
    ASSERT(!validpath("config/glsl/ao\\x.frag"));
    ASSERT(!validpath("config/glsl/c:x.frag"));

    ASSERT(validdefine("AO_TAPS", "12"));
    ASSERT(validdefine("_x1", ""));
    ASSERT(validdefine("TAPVEC", "vec2(i, 0.0)"));
    ASSERT(!validdefine("", "1"));
    ASSERT(!validdefine("1X", "1"));
    ASSERT(!validdefine("A-B", "1"));
    ASSERT(!validdefine("A(x)", "x"));
    ASSERT(!validdefine("A", "1\n#define B 2"));
    ASSERT(!validdefine("A", "1\r"));
    ASSERT(!validdefine("A", "1\\"));
    ASSERT(validdefine("A", "a\\b"));

    vector<char> defs;
    appenddefine(defs, "AO_TAPS", "12");
    appenddefine(defs, "AO_FLAG", "");
    vector<char> shown;
    shown.put(defs.getbuf(), defs.length());
    shown.add('\0');
    ASSERT(!strcmp(shown.getbuf(), "#define AO_TAPS 12\n#define AO_FLAG\n"));

    vector<char> text;
    appendtext(text, "a\r\nb\r\n");
    text.add('\0');
    ASSERT(!strcmp(text.getbuf(), "a\nb\n"));
    text.setsize(0);
    appendtext(text, "no newline");
    text.add('\0');
    ASSERT(!strcmp(text.getbuf(), "no newline\n"));
    text.setsize(0);
    appendtext(text, "");
    ASSERT(text.empty());

    vector<const char *> includes;
    includes.add("inc1\r\n");
    includes.add("inc2");
    vector<char> out;
    assemblestage(out, defs, includes, "body\n");
    ASSERT(assembled(out, "#define AO_TAPS 12\n#define AO_FLAG\ninc1\ninc2\nbody\n"));
    assemblestage(out, defs, includes, NULL);
    ASSERT(assembled(out, ""));
    vector<char> nodefs;
    vector<const char *> noincludes;
    assemblestage(out, nodefs, noincludes, "body");
    ASSERT(assembled(out, "body\n"));

    conoutf(colourwhite, "testshadersource: ok");
}
