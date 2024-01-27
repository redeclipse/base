#ifdef CPP_GAME_SERVER
#define PD_PREFIX sv_
#define PD_DECLARE(a,b) a = { b };
#else // CPP_GAME_SERVER
#define PD_PREFIX
#ifdef CPP_GAME_MAIN
#define PD_DECLARE(a,b) a = { b };
#else // CPP_GAME_MAIN
#define PD_DECLARE(a,b) extern a;
#endif // CPP_GAME_MAIN
#endif // CPP_GAME_SERVER

#define APVAR(flags, level, name, mn, mx, player0, bot0, turret0, grunt0, drone0, roller0, hazard0, janitor0) \
    GVAR(flags, level, player##name, mn, player0, mx); \
    GVAR(flags, level, bot##name, mn, bot0, mx); \
    GVAR(flags, level, turret##name, mn, turret0, mx); \
    GVAR(flags, level, grunt##name, mn, grunt0, mx); \
    GVAR(flags, level, drone##name, mn, drone0, mx); \
    GVAR(flags, level, roller##name, mn, roller0, mx); \
    GVAR(flags, level, hazard##name, mn, hazard0, mx); \
    GVAR(flags, level, janitor##name, mn, janitor0, mx); \
    PD_DECLARE(int *EXPAND(PD_PREFIX, actor_stat_##name)[], \
        &EXPAND(PD_PREFIX, player##name) NEXTARG \
        &EXPAND(PD_PREFIX, bot##name) NEXTARG \
        &EXPAND(PD_PREFIX, turret##name) NEXTARG \
        &EXPAND(PD_PREFIX, grunt##name) NEXTARG \
        &EXPAND(PD_PREFIX, drone##name) NEXTARG \
        &EXPAND(PD_PREFIX, roller##name) NEXTARG \
        &EXPAND(PD_PREFIX, hazard##name) NEXTARG \
        &EXPAND(PD_PREFIX, janitor##name) \
    );

#define APFVAR(flags, level, name, mn, mx, player0, bot0, turret0, grunt0, drone0, roller0, hazard0, janitor0) \
    GFVAR(flags, level, player##name, mn, player0, mx); \
    GFVAR(flags, level, bot##name, mn, bot0, mx); \
    GFVAR(flags, level, turret##name, mn, turret0, mx); \
    GFVAR(flags, level, grunt##name, mn, grunt0, mx); \
    GFVAR(flags, level, drone##name, mn, drone0, mx); \
    GFVAR(flags, level, roller##name, mn, roller0, mx); \
    GFVAR(flags, level, hazard##name, mn, hazard0, mx); \
    GFVAR(flags, level, janitor##name, mn, janitor0, mx); \
    PD_DECLARE(float *EXPAND(PD_PREFIX, actor_stat_##name)[], \
        &EXPAND(PD_PREFIX, player##name) NEXTARG \
        &EXPAND(PD_PREFIX, bot##name) NEXTARG \
        &EXPAND(PD_PREFIX, turret##name) NEXTARG \
        &EXPAND(PD_PREFIX, grunt##name) NEXTARG \
        &EXPAND(PD_PREFIX, drone##name) NEXTARG \
        &EXPAND(PD_PREFIX, roller##name) NEXTARG \
        &EXPAND(PD_PREFIX, hazard##name) NEXTARG \
        &EXPAND(PD_PREFIX, janitor##name) \
    );

#define APSVAR(flags, level, name, player0, bot0, turret0, grunt0, drone0, roller0, hazard0, janitor0) \
    GSVAR(flags, level, player##name, player0); \
    GSVAR(flags, level, bot##name, bot0); \
    GSVAR(flags, level, turret##name, turret0); \
    GSVAR(flags, level, grunt##name, grunt0); \
    GSVAR(flags, level, drone##name, drone0); \
    GSVAR(flags, level, roller##name, roller0); \
    GSVAR(flags, level, hazard##name, hazard0); \
    GSVAR(flags, level, janitor##name, janitor0); \
    PD_DECLARE(char **EXPAND(PD_PREFIX, actor_stat_##name)[], \
        &EXPAND(PD_PREFIX, player##name) NEXTARG \
        &EXPAND(PD_PREFIX, bot##name) NEXTARG \
        &EXPAND(PD_PREFIX, turret##name) NEXTARG \
        &EXPAND(PD_PREFIX, grunt##name) NEXTARG \
        &EXPAND(PD_PREFIX, drone##name) NEXTARG \
        &EXPAND(PD_PREFIX, roller##name) NEXTARG \
        &EXPAND(PD_PREFIX, hazard##name) NEXTARG \
        &EXPAND(PD_PREFIX, janitor##name) \
    );

#define A(atyp,name) (*EXPAND(PD_PREFIX, actor_stat_##name[clamp(atyp, 0, int(A_MAX)-1)]))
