// nasty macros to create cubescript accessible enumeratives

#ifdef __ENUM_INIT__
#undef ENUM_VAR
#undef ENUM_VARD
#undef ENUM_FVARD
#undef ENUM_SVARD
#undef ENUM_VARX
#undef ENUM_VARL
#undef ENUM_VARA
#endif

#ifdef ENUM_DEFINE
#define ENUM_VAR(name, val) enum { name = val }; int _enum_##name = variable(#name, 1, val, -1, &_enum_##name, NULL, IDF_READONLY, 0);
#define ENUM_VARD(name, val) int name = variable(#name, 1, val, -1, &name, NULL, IDF_READONLY, 0);
#define ENUM_FVARD(name, val) float name = fvariable(#name, 1, val, -1, &name, NULL, IDF_READONLY, 0);
#define ENUM_SVARD(name, val) char *name = svariable(#name, val, &name, NULL, IDF_READONLY, 0);
#define ENUM_VARX(name, val) int _enum_##name = variable(#name, 1, int(name), -1, &_enum_##name, NULL, IDF_READONLY, 0);
#define ENUM_VARL(name, val) char *name = svariable(#name, &val[1], &name, NULL, IDF_READONLY, 0);
#define ENUM_VARA(name, val) const char *name[] = { val "" };
#else
#define ENUM_VAR(name, val) enum { name = val };
#define ENUM_VARD(name, val) extern int name;
#define ENUM_FVARD(name, val) extern float name;
#define ENUM_SVARD(name, val) extern char *name;
#define ENUM_VARX(name, val)
#define ENUM_VARL(name, val) extern char *name;
#define ENUM_VARA(name, val) extern const char *name[];
#endif

#ifndef __ENUM_INIT__
#define __ENUM_INIT__
#define _ENUM_DSX(prefix, name, assign) prefix##_##name = assign,
#define _ENUM_DSV(prefix, name, assign) ENUM_VARX(prefix##_##name, prefix##_##name)
#define _ENUM_DSL(prefix, name, assign) " " #prefix "_" #name

#define _ENUM_VSX(prefix, name) prefix##_##name,
#define _ENUM_VSV(prefix, name) ENUM_VARX(prefix##_##name, prefix##_##name)
#define _ENUM_VSL(prefix, name) " " #prefix "_" #name

// enum with assigned values
#define ENUM_A(prefix) enum { prefix##_ENUM(_ENUM_DSX, prefix) }; prefix##_ENUM(_ENUM_DSV, prefix)

// enum with default values
#define ENUM_D(prefix) enum { prefix##_ENUM(_ENUM_VSX, prefix) }; prefix##_ENUM(_ENUM_VSV, prefix)

// enum with assigned values and list variable
#define ENUM_AL(prefix) enum { prefix##_ENUM(_ENUM_DSX, prefix) }; prefix##_ENUM(_ENUM_DSV, prefix) ENUM_VARL(prefix##_LIST, prefix##_ENUM(_ENUM_DSL, prefix))

// enum with default values and list variable
#define ENUM_DL(prefix) enum { prefix##_ENUM(_ENUM_VSX, prefix) }; prefix##_ENUM(_ENUM_VSV, prefix) ENUM_VARL(prefix##_LIST, prefix##_ENUM(_ENUM_VSL, prefix))

#define _ENUM_ALNX(prefix, pretty, name, assign) prefix##_##name = assign,
#define _ENUM_ALNV(prefix, pretty, name, assign) ENUM_VARX(prefix##_##name, prefix##_##name)
#define _ENUM_ALNL(prefix, pretty, name, assign) " " #prefix "_" #name
#define _ENUM_ALNN(prefix, pretty, name, assign) " [" #pretty "]"
#define _ENUM_ALNS(prefix, pretty, name, assign) #pretty,

// enum with assigned values and list/name/str variables
#define ENUM_ALN(prefix) enum { prefix##_ENUM(_ENUM_ALNX, prefix) }; prefix##_ENUM(_ENUM_ALNV, prefix) ENUM_VARL(prefix##_LIST, prefix##_ENUM(_ENUM_ALNL, prefix)) ENUM_VARL(prefix##_NAMES, prefix##_ENUM(_ENUM_ALNN, prefix)) ENUM_VARA(prefix##_STR, prefix##_ENUM(_ENUM_ALNS, prefix))

#define _ENUM_DLNX(prefix, pretty, name) prefix##_##name,
#define _ENUM_DLNV(prefix, pretty, name) ENUM_VARX(prefix##_##name, prefix##_##name)
#define _ENUM_DLNL(prefix, pretty, name) " " #prefix "_" #name
#define _ENUM_DLNN(prefix, pretty, name) " [" #pretty "]"
#define _ENUM_DLNS(prefix, pretty, name) #pretty,

// enum with default values and list/name/str variables
#define ENUM_DLN(prefix) enum { prefix##_ENUM(_ENUM_DLNX, prefix) }; prefix##_ENUM(_ENUM_DLNV, prefix) ENUM_VARL(prefix##_LIST, prefix##_ENUM(_ENUM_DLNL, prefix)) ENUM_VARL(prefix##_NAMES, prefix##_ENUM(_ENUM_DLNN, prefix)) ENUM_VARA(prefix##_STR, prefix##_ENUM(_ENUM_DLNS, prefix))
#endif