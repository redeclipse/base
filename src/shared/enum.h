// nasty macros to create cubescript accessible enumeratives

#ifdef ENUM_INIT
#undef VARQ
#undef VARD
#undef FVARD
#undef SVARD
#undef VARE
#undef VARL
#undef VARS
#endif

#ifdef ENUM_DEFINE
#define VARQ(name, val) enum { name = val }; int _enum_##name = variable(#name, 1, val, -1, &_enum_##name, NULL, IDF_READONLY, 0);
#define VARD(name, val) int name = variable(#name, 1, val, -1, &name, NULL, IDF_READONLY, 0);
#define FVARD(name, val) float name = fvariable(#name, 1, val, -1, &name, NULL, IDF_READONLY, 0);
#define SVARD(name, val) char *name = svariable(#name, val, &name, NULL, IDF_READONLY, 0);
#define VARE(name, val) int _enum_##name = variable(#name, 1, int(name), -1, &_enum_##name, NULL, IDF_READONLY, 0);
#define VARL(name, val) char *name = svariable(#name, &val[1], &name, NULL, IDF_READONLY, 0);
#define VARS(name, val) const char *name[] = { val "" };
#else
#define VARQ(name, val) enum { name = val };
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

#define ENUMVSX(prefix, name) prefix##_##name,
#define ENUMVSV(prefix, name) VARE(prefix##_##name, prefix##_##name)
#define ENUMVSL(prefix, name) " " #prefix "_" #name

// enum with assigned values
#define ENUMA(prefix) enum { prefix##_ENUM(ENUMDSX, prefix) }; prefix##_ENUM(ENUMDSV, prefix)

// enum with default values
#define ENUMD(prefix) enum { prefix##_ENUM(ENUMVSX, prefix) }; prefix##_ENUM(ENUMVSV, prefix)

// enum with assigned values and list variable
#define ENUMAL(prefix) enum { prefix##_ENUM(ENUMDSX, prefix) }; prefix##_ENUM(ENUMDSV, prefix) VARL(prefix##_LIST, prefix##_ENUM(ENUMDSL, prefix))

// enum with default values and list variable
#define ENUMDL(prefix) enum { prefix##_ENUM(ENUMVSX, prefix) }; prefix##_ENUM(ENUMVSV, prefix) VARL(prefix##_LIST, prefix##_ENUM(ENUMVSL, prefix))

#define ENUMALNX(prefix, pretty, name, assign) prefix##_##name = assign,
#define ENUMALNV(prefix, pretty, name, assign) VARE(prefix##_##name, prefix##_##name)
#define ENUMALNL(prefix, pretty, name, assign) " " #prefix "_" #name
#define ENUMALNN(prefix, pretty, name, assign) " [" #pretty "]"
#define ENUMALNS(prefix, pretty, name, assign) #pretty,

// enum with assigned values and list/name/str variables
#define ENUMALN(prefix) enum { prefix##_ENUM(ENUMALNX, prefix) }; prefix##_ENUM(ENUMALNV, prefix) VARL(prefix##_LIST, prefix##_ENUM(ENUMALNL, prefix)) VARL(prefix##_NAMES, prefix##_ENUM(ENUMALNN, prefix)) VARS(prefix##_STR, prefix##_ENUM(ENUMALNS, prefix))

#define ENUMDLNX(prefix, pretty, name) prefix##_##name,
#define ENUMDLNV(prefix, pretty, name) VARE(prefix##_##name, prefix##_##name)
#define ENUMDLNL(prefix, pretty, name) " " #prefix "_" #name
#define ENUMDLNN(prefix, pretty, name) " [" #pretty "]"
#define ENUMDLNS(prefix, pretty, name) #pretty,

// enum with default values and list/name/str variables
#define ENUMDLN(prefix) enum { prefix##_ENUM(ENUMDLNX, prefix) }; prefix##_ENUM(ENUMDLNV, prefix) VARL(prefix##_LIST, prefix##_ENUM(ENUMDLNL, prefix)) VARL(prefix##_NAMES, prefix##_ENUM(ENUMDLNN, prefix)) VARS(prefix##_STR, prefix##_ENUM(ENUMDLNS, prefix))
#endif