// nasty macros to create cubescript accessible enumeratives

#ifdef ENUM_INIT
#undef VARD
#undef FVARD
#undef SVARD
#undef VARE
#undef VARL
#undef VARS
#endif

#ifdef ENUM_DEFINE
#define VARD(name, val) int name = variable(#name, 1, val, -1, &name, NULL, IDF_READONLY, 0);
#define FVARD(name, val) float name = fvariable(#name, 1, val, -1, &name, NULL, IDF_READONLY, 0);
#define SVARD(name, val) char *name = svariable(#name, val, &name, NULL, IDF_READONLY, 0);
#define VARE(name, val) int _enum_##name = variable(#name, 1, int(name), -1, &_enum_##name, NULL, IDF_READONLY, 0);
#define VARL(name, val) char *name = svariable(#name, &val[1], &name, NULL, IDF_READONLY, 0);
#define VARS(name, val) const char *name[] = { val "" };
#else
#define VARD(name, val) extern int name;
#define FVARD(name, val) extern float name;
#define SVARD(name, val) extern char *name;
#define VARE(name, val)
#define VARL(name, val) extern char *name;
#define VARS(name, val) extern const char *name[];
#endif

#ifndef ENUM_INIT
#define ENUM_INIT
#define ENUMDSX(prefix, name, assign) prefix##_##name = assign,
#define ENUMDSV(prefix, name, assign) VARE(prefix##_##name, prefix##_##name)
#define ENUMDSL(prefix, name, assign) " " #prefix "_" #name

// enum with assigned values
#define ENUMDI(prefix, definition) enum { definition(prefix, ENUMDSX) }; definition(prefix, ENUMDSV)

// enum with assigned values and list variable
#define ENUMLI(prefix, definition) enum { definition(prefix, ENUMDSX) }; definition(prefix, ENUMDSV) VARL(prefix##_LIST, definition(prefix, ENUMDSL))

#define ENUMVSX(prefix, name) prefix##_##name,
#define ENUMVSV(prefix, name) VARE(prefix##_##name, prefix##_##name)
#define ENUMVSL(prefix, name) " " #prefix "_" #name

// enum with default values
#define ENUMDV(prefix, definition) enum { definition(prefix, ENUMVSX) }; definition(prefix, ENUMVSV)

// enum with default values and list variable
#define ENUMLV(prefix, definition) enum { definition(prefix, ENUMVSX) }; definition(prefix, ENUMVSV) VARL(prefix##_LIST, definition(prefix, ENUMVSL))

#define ENUMNIX(prefix, pretty, name, assign) prefix##_##name = assign,
#define ENUMNIV(prefix, pretty, name, assign) VARE(prefix##_##name, prefix##_##name)
#define ENUMNIL(prefix, pretty, name, assign) " " #prefix "_" #name
#define ENUMNIN(prefix, pretty, name, assign) " [" #pretty "]"
#define ENUMNIS(prefix, pretty, name, assign) #pretty,

// enum with assigned values and list/name variables
#define ENUMNI(prefix, definition) enum { definition(prefix, ENUMNIX) }; definition(prefix, ENUMNIV) VARL(prefix##_LIST, definition(prefix, ENUMNIL)) VARL(prefix##_NAMES, definition(prefix, ENUMNIN)) VARS(prefix##_STR, definition(prefix, ENUMNIS))

#define ENUMNVX(prefix, pretty, name) prefix##_##name,
#define ENUMNVV(prefix, pretty, name) VARE(prefix##_##name, prefix##_##name)
#define ENUMNVL(prefix, pretty, name) " " #prefix "_" #name
#define ENUMNVN(prefix, pretty, name) " [" #pretty "]"
#define ENUMNVS(prefix, pretty, name) #pretty,

// enum with default values and list/name variables
#define ENUMNV(prefix, definition) enum { definition(prefix, ENUMNVX) }; definition(prefix, ENUMNVV) VARL(prefix##_LIST, definition(prefix, ENUMNVL)) VARL(prefix##_NAMES, definition(prefix, ENUMNVN)) VARS(prefix##_STR, definition(prefix, ENUMNVS))
#endif