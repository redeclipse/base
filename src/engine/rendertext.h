#ifndef CPP_RENDERTEXT_HEADER
#define CPP_RENDERTEXT_HEADER

#include "engine.h"

struct textkey
{
    char *name, *file;
    Texture *tex;
    textkey() : name(NULL), file(NULL), tex(NULL) {}
    textkey(char *n, char *f, Texture *t) : name(newstring(n)), file(newstring(f)), tex(t) {}
    ~textkey()
    {
        DELETEA(name);
        DELETEA(file);
    }
};

textkey *findtextkey_common(const char *str, vector<textkey *> textkeycache, const char *filename = NULL);
#endif
