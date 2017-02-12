#include "engine.h"

#ifndef GIT_REVISION_ID
#define GIT_REVISION_ID "unknown-commit"
#endif

#ifdef GIT_REPO_DIRTY
#define DEVELOPMENT_BUILD true
#define VERSION_SUFFIX "-dirty"
#endif

#ifndef VERSION_SUFFIX
#define VERSION_SUFFIX ""
#endif

#ifndef DEVELOPMENT_BUILD
#define DEVELOPMENT_BUILD false
#endif

#ifndef GIT_REPO_DIRTY
#define GIT_REPO_DIRTY false
#endif


VAR(IDF_READONLY, isdevelopmentbuild, 1, DEVELOPMENT_BUILD, 0);
SVAR(IDF_READONLY, gitrevisionid, GIT_REVISION_ID);
SVAR(IDF_READONLY, versionsuffix, VERSION_SUFFIX);
