#ifdef GAMESERVER
    #define TPVAR(flags, name, mn, mx, w00, w01, w02, w03, w04, w05) \
        GVAR(flags, teamneutral##name, mn, w00, mx); \
        GVAR(flags, teamalpha##name, mn, w01, mx); \
        GVAR(flags, teamomega##name, mn, w02, mx); \
        GVAR(flags, teamkappa##name, mn, w03, mx); \
        GVAR(flags, teamsigma##name, mn, w04, mx); \
        GVAR(flags, teamenemy##name, mn, w05, mx); \
        int *sv_team_stat_##name[] = { \
            &sv_teamneutral##name, \
            &sv_teamalpha##name, \
            &sv_teamomega##name, \
            &sv_teamkappa##name, \
            &sv_teamsigma##name, \
            &sv_teamenemy##name \
        };

    #define TPFVAR(flags, name, mn, mx, w00, w01, w02, w03, w04, w05) \
        GFVAR(flags, teamneutral##name, mn, w00, mx); \
        GFVAR(flags, teamalpha##name, mn, w01, mx); \
        GFVAR(flags, teamomega##name, mn, w02, mx); \
        GFVAR(flags, teamkappa##name, mn, w03, mx); \
        GFVAR(flags, teamsigma##name, mn, w04, mx); \
        GFVAR(flags, teamenemy##name, mn, w05, mx); \
        float *sv_team_stat_##name[] = { \
            &sv_teamneutral##name, \
            &sv_teamalpha##name, \
            &sv_teamomega##name, \
            &sv_teamkappa##name, \
            &sv_teamsigma##name, \
            &sv_teamenemy##name \
        };

    #define TPSVAR(flags, name, w00, w01, w02, w03, w04, w05) \
        GSVAR(flags, teamneutral##name, w00); \
        GSVAR(flags, teamalpha##name, w01); \
        GSVAR(flags, teamomega##name, w02); \
        GSVAR(flags, teamkappa##name, w03); \
        GSVAR(flags, teamsigma##name, w04); \
        GSVAR(flags, teamenemy##name, w05); \
        char **sv_team_stat_##name[] = { \
            &sv_teamneutral##name, \
            &sv_teamalpha##name, \
            &sv_teamomega##name, \
            &sv_teamkappa##name, \
            &sv_teamsigma##name, \
            &sv_teamenemy##name \
        };

    #define TEAM(team,name)         (*sv_team_stat_##name[team])
#else
#ifdef GAMEWORLD
    #define TPVAR(flags, name, mn, mx, w00, w01, w02, w03, w04, w05) \
        GVAR(flags, teamneutral##name, mn, w00, mx); \
        GVAR(flags, teamalpha##name, mn, w01, mx); \
        GVAR(flags, teamomega##name, mn, w02, mx); \
        GVAR(flags, teamkappa##name, mn, w03, mx); \
        GVAR(flags, teamsigma##name, mn, w04, mx); \
        GVAR(flags, teamenemy##name, mn, w05, mx); \
        int *team_stat_##name[] = { \
            &teamneutral##name, \
            &teamalpha##name, \
            &teamomega##name, \
            &teamkappa##name, \
            &teamsigma##name, \
            &teamenemy##name \
        };

    #define TPFVAR(flags, name, mn, mx, w00, w01, w02, w03, w04, w05) \
        GFVAR(flags, teamneutral##name, mn, w00, mx); \
        GFVAR(flags, teamalpha##name, mn, w01, mx); \
        GFVAR(flags, teamomega##name, mn, w02, mx); \
        GFVAR(flags, teamkappa##name, mn, w03, mx); \
        GFVAR(flags, teamsigma##name, mn, w04, mx); \
        GFVAR(flags, teamenemy##name, mn, w05, mx); \
        float *team_stat_##name[] = { \
            &teamneutral##name, \
            &teamalpha##name, \
            &teamomega##name, \
            &teamkappa##name, \
            &teamsigma##name, \
            &teamenemy##name \
        };

    #define TPSVAR(flags, name, w00, w01, w02, w03, w04, w05) \
        GSVAR(flags, teamneutral##name, w00); \
        GSVAR(flags, teamalpha##name, w01); \
        GSVAR(flags, teamomega##name, w02); \
        GSVAR(flags, teamkappa##name, w03); \
        GSVAR(flags, teamsigma##name, w04); \
        GSVAR(flags, teamenemy##name, w05); \
        char **team_stat_##name[] = { \
            &teamneutral##name, \
            &teamalpha##name, \
            &teamomega##name, \
            &teamkappa##name, \
            &teamsigma##name, \
            &teamenemy##name \
        };
#else
    #define TPVAR(flags, name, mn, mx, w00, w01, w02, w03, w04, w05) \
        GVAR(flags, teamneutral##name, mn, w00, mx); \
        GVAR(flags, teamalpha##name, mn, w01, mx); \
        GVAR(flags, teamomega##name, mn, w02, mx); \
        GVAR(flags, teamkappa##name, mn, w03, mx); \
        GVAR(flags, teamsigma##name, mn, w04, mx); \
        GVAR(flags, teamenemy##name, mn, w05, mx); \
        extern int *team_stat_##name[];
    #define TPFVAR(flags, name, mn, mx, w00, w01, w02, w03, w04, w05) \
        GFVAR(flags, teamneutral##name, mn, w00, mx); \
        GFVAR(flags, teamalpha##name, mn, w01, mx); \
        GFVAR(flags, teamomega##name, mn, w02, mx); \
        GFVAR(flags, teamkappa##name, mn, w03, mx); \
        GFVAR(flags, teamsigma##name, mn, w04, mx); \
        GFVAR(flags, teamenemy##name, mn, w05, mx); \
        extern float *team_stat_##name[];
    #define TPSVAR(flags, name, w00, w01, w02, w03, w04, w05) \
        GSVAR(flags, teamneutral##name, w00); \
        GSVAR(flags, teamalpha##name, w01); \
        GSVAR(flags, teamomega##name, w02); \
        GSVAR(flags, teamkappa##name, w03); \
        GSVAR(flags, teamsigma##name, w04); \
        GSVAR(flags, teamenemy##name, w05); \
        extern char **team_stat_##name[];
#endif
    #define TEAM(team,name)         (*team_stat_##name[team])
#endif
