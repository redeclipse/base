#ifdef CPP_GAME_SERVER
    #define TPVAR(flags, level, name, mn, mx, w00, w01, w02, w03) \
        GVAR(flags, level, teamneutral##name, mn, w00, mx); \
        GVAR(flags, level, teamalpha##name, mn, w01, mx); \
        GVAR(flags, level, teamomega##name, mn, w02, mx); \
        GVAR(flags, level, teamenemy##name, mn, w03, mx); \
        int *sv_team_stat_##name[] = { \
            &sv_teamneutral##name, \
            &sv_teamalpha##name, \
            &sv_teamomega##name, \
            &sv_teamenemy##name \
        };

    #define TPFVAR(flags, level, name, mn, mx, w00, w01, w02, w03) \
        GFVAR(flags, level, teamneutral##name, mn, w00, mx); \
        GFVAR(flags, level, teamalpha##name, mn, w01, mx); \
        GFVAR(flags, level, teamomega##name, mn, w02, mx); \
        GFVAR(flags, level, teamenemy##name, mn, w03, mx); \
        float *sv_team_stat_##name[] = { \
            &sv_teamneutral##name, \
            &sv_teamalpha##name, \
            &sv_teamomega##name, \
            &sv_teamenemy##name \
        };

    #define TPSVAR(flags, level, name, w00, w01, w02, w03) \
        GSVAR(flags, level, teamneutral##name, w00); \
        GSVAR(flags, level, teamalpha##name, w01); \
        GSVAR(flags, level, teamomega##name, w02); \
        GSVAR(flags, level, teamenemy##name, w03); \
        char **sv_team_stat_##name[] = { \
            &sv_teamneutral##name, \
            &sv_teamalpha##name, \
            &sv_teamomega##name, \
            &sv_teamenemy##name \
        };

    #define TEAM(team,name)         (*sv_team_stat_##name[team])
#else
#ifdef CPP_GAME_MAIN
    #define TPVAR(flags, level, name, mn, mx, w00, w01, w02, w03) \
        GVAR(flags, level, teamneutral##name, mn, w00, mx); \
        GVAR(flags, level, teamalpha##name, mn, w01, mx); \
        GVAR(flags, level, teamomega##name, mn, w02, mx); \
        GVAR(flags, level, teamenemy##name, mn, w03, mx); \
        int *team_stat_##name[] = { \
            &teamneutral##name, \
            &teamalpha##name, \
            &teamomega##name, \
            &teamenemy##name \
        };

    #define TPFVAR(flags, level, name, mn, mx, w00, w01, w02, w03) \
        GFVAR(flags, level, teamneutral##name, mn, w00, mx); \
        GFVAR(flags, level, teamalpha##name, mn, w01, mx); \
        GFVAR(flags, level, teamomega##name, mn, w02, mx); \
        GFVAR(flags, level, teamenemy##name, mn, w03, mx); \
        float *team_stat_##name[] = { \
            &teamneutral##name, \
            &teamalpha##name, \
            &teamomega##name, \
            &teamenemy##name \
        };

    #define TPSVAR(flags, level, name, w00, w01, w02, w03) \
        GSVAR(flags, level, teamneutral##name, w00); \
        GSVAR(flags, level, teamalpha##name, w01); \
        GSVAR(flags, level, teamomega##name, w02); \
        GSVAR(flags, level, teamenemy##name, w03); \
        char **team_stat_##name[] = { \
            &teamneutral##name, \
            &teamalpha##name, \
            &teamomega##name, \
            &teamenemy##name \
        };
#else
    #define TPVAR(flags, level, name, mn, mx, w00, w01, w02, w03) \
        GVAR(flags, level, teamneutral##name, mn, w00, mx); \
        GVAR(flags, level, teamalpha##name, mn, w01, mx); \
        GVAR(flags, level, teamomega##name, mn, w02, mx); \
        GVAR(flags, level, teamenemy##name, mn, w03, mx); \
        extern int *team_stat_##name[];
    #define TPFVAR(flags, level, name, mn, mx, w00, w01, w02, w03) \
        GFVAR(flags, level, teamneutral##name, mn, w00, mx); \
        GFVAR(flags, level, teamalpha##name, mn, w01, mx); \
        GFVAR(flags, level, teamomega##name, mn, w02, mx); \
        GFVAR(flags, level, teamenemy##name, mn, w03, mx); \
        extern float *team_stat_##name[];
    #define TPSVAR(flags, level, name, w00, w01, w02, w03) \
        GSVAR(flags, level, teamneutral##name, w00); \
        GSVAR(flags, level, teamalpha##name, w01); \
        GSVAR(flags, level, teamomega##name, w02); \
        GSVAR(flags, level, teamenemy##name, w03); \
        extern char **team_stat_##name[];
#endif
    #define TEAM(team,name)         (*team_stat_##name[team])
#endif
