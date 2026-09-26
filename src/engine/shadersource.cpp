// shadersource.cpp: the shader_new loader, see shadersource.h.

#include "engine.h"
#include "shadersource.h"

namespace shadersource
{
    bool validpath(const char *path)
    {
        static const char prefix[] = "config/glsl/";
        static const int prefixlen = sizeof(prefix) - 1;
        if(strncmp(path, prefix, prefixlen) || strpbrk(path, "\\:")) return false;
        for(const char *seg = path + prefixlen;;)
        {
            const char *end = strchr(seg, '/');
            int len = end ? int(end - seg) : int(strlen(seg));
            if(!len || (len == 1 && seg[0] == '.') || (len == 2 && seg[0] == '.' && seg[1] == '.')) return false;
            if(!end) return true;
            seg = end + 1;
        }
    }

    bool validdefine(const char *name, const char *value)
    {
        if(!isalpha(uchar(name[0])) && name[0] != '_') return false;
        for(const char *c = name + 1; *c; c++) if(!isalnum(uchar(*c)) && *c != '_') return false;
        return !strpbrk(value, "\r\n");
    }

    void appenddefine(vector<char> &out, const char *name, const char *value)
    {
        out.put("#define ", 8);
        out.put(name, strlen(name));
        if(*value)
        {
            out.add(' ');
            out.put(value, strlen(value));
        }
        out.add('\n');
    }

    void appendtext(vector<char> &out, const char *text)
    {
        char last = '\n';
        for(const char *c = text; *c; c++) if(*c != '\r')
        {
            out.add(*c);
            last = *c;
        }
        if(last != '\n') out.add('\n');
    }

    void assemblestage(vector<char> &out, const vector<char> &defines, const vector<const char *> &includes, const char *body)
    {
        out.setsize(0);
        if(body)
        {
            if(defines.length()) out.put(defines.getbuf(), defines.length());
            loopv(includes) appendtext(out, includes[i]);
            appendtext(out, body);
        }
        out.add('\0');
    }
}
