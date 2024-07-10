#ifdef CPP_GAME_SERVER
#define TD_PREFIX sv_
#define TD_DECLARE(a,b) a = { b };
#else // CPP_GAME_SERVER
#define TD_PREFIX
#ifdef CPP_GAME_MAIN
#define TD_DECLARE(a,b) a = { b };
#else // CPP_GAME_MAIN
#define TD_DECLARE(a,b) extern a;
#endif // CPP_GAME_MAIN
#endif // CPP_GAME_SERVER

#define TPVAR(flags, level, name, mn, mx, neutral0, alpha0, omega0, enemy0, environment0) \
    GVAR(flags, level, teamneutral##name, mn, neutral0, mx); \
    GVAR(flags, level, teamalpha##name, mn, alpha0, mx); \
    GVAR(flags, level, teamomega##name, mn, omega0, mx); \
    GVAR(flags, level, teamenemy##name, mn, enemy0, mx); \
    GVAR(flags, level, teamenvironment##name, mn, environment0, mx); \
    TD_DECLARE(int *EXPAND(TD_PREFIX, team_stat_##name[]), \
        &EXPAND(TD_PREFIX, teamneutral##name) NEXTARG \
        &EXPAND(TD_PREFIX, teamalpha##name) NEXTARG \
        &EXPAND(TD_PREFIX, teamomega##name) NEXTARG \
        &EXPAND(TD_PREFIX, teamenemy##name) NEXTARG \
        &EXPAND(TD_PREFIX, teamenvironment##name) \
    );

#define TPFVAR(flags, level, name, mn, mx, neutral0, alpha0, omega0, enemy0, environment0) \
    GFVAR(flags, level, teamneutral##name, mn, neutral0, mx); \
    GFVAR(flags, level, teamalpha##name, mn, alpha0, mx); \
    GFVAR(flags, level, teamomega##name, mn, omega0, mx); \
    GFVAR(flags, level, teamenemy##name, mn, enemy0, mx); \
    GFVAR(flags, level, teamenvironment##name, mn, environment0, mx); \
    TD_DECLARE(float *EXPAND(TD_PREFIX, team_stat_##name[]), \
        &EXPAND(TD_PREFIX, teamneutral##name) NEXTARG \
        &EXPAND(TD_PREFIX, teamalpha##name) NEXTARG \
        &EXPAND(TD_PREFIX, teamomega##name) NEXTARG \
        &EXPAND(TD_PREFIX, teamenemy##name) NEXTARG \
        &EXPAND(TD_PREFIX, teamenvironment##name) \
    );

#define TPSVAR(flags, level, name, neutral0, alpha0, omega0, enemy0, environment0) \
    GSVAR(flags, level, teamneutral##name, neutral0); \
    GSVAR(flags, level, teamalpha##name, alpha0); \
    GSVAR(flags, level, teamomega##name, omega0); \
    GSVAR(flags, level, teamenemy##name, enemy0); \
    GSVAR(flags, level, teamenvironment##name, environment0); \
    TD_DECLARE(char **EXPAND(TD_PREFIX, team_stat_##name[]), \
        &EXPAND(TD_PREFIX, teamneutral##name) NEXTARG \
        &EXPAND(TD_PREFIX, teamalpha##name) NEXTARG \
        &EXPAND(TD_PREFIX, teamomega##name) NEXTARG \
        &EXPAND(TD_PREFIX, teamenemy##name) NEXTARG \
        &EXPAND(TD_PREFIX, teamenvironment##name) \
    );

#define TEAM(team,name) (*EXPAND(TD_PREFIX, team_stat_##name[team]))
