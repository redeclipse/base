#include "engine.h"

// Prefab path validation, see validprefabpath() in engine/octaedit.cpp. It
// guards removeprefab and renameprefab, which move files, so the rejections
// matter more than the acceptances.
void testprefab()
{
    ASSERT(validprefabpath("prefab/oak"));
    ASSERT(validprefabpath("prefab/trees/oak"));
    ASSERT(validprefabpath("prefab/a.b-c_d"));
    ASSERT(validprefabpath("prefab/Trees/Oak2"));

    ASSERT(!validprefabpath(""));
    ASSERT(!validprefabpath("oak"));
    ASSERT(!validprefabpath("prefab"));
    ASSERT(!validprefabpath("prefab/"));
    ASSERT(!validprefabpath("prefabx/y"));
    ASSERT(!validprefabpath("/prefab/x"));
    ASSERT(!validprefabpath("prefab//x"));
    ASSERT(!validprefabpath("prefab/x/"));
    ASSERT(!validprefabpath("prefab/../x"));
    ASSERT(!validprefabpath("prefab/x/.."));
    ASSERT(!validprefabpath("prefab/./x"));
    ASSERT(!validprefabpath("prefab\\x"));
    ASSERT(!validprefabpath("C:/x"));
    ASSERT(!validprefabpath("prefab/sp ace"));
    // Windows strips a trailing '.', so "st." and "st" would name one file
    ASSERT(!validprefabpath("prefab/st./x"));
    ASSERT(!validprefabpath("prefab/x."));
    ASSERT(!validprefabpath("prefab/..."));
    ASSERT(!validprefabpath("prefab/\xe9"));

    // The name plus ".obr" must still fit in a string.
    string name;
    copystring(name, "prefab/");
    size_t len = strlen(name);
    while(len < MAXSTRLEN-5) name[len++] = 'a';
    name[len] = '\0';
    ASSERT(validprefabpath(name)); // len+4 == MAXSTRLEN-1
    name[len++] = 'a';
    name[len] = '\0';
    ASSERT(!validprefabpath(name)); // len+4 == MAXSTRLEN

    conoutf(colourwhite, "testprefab: ok");
}
