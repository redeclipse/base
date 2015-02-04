#ifdef GAMESERVER
    #define PPVAR(flags, name, mn, mx, w00) \
        GVAR(flags, player##name, mn, w00, mx); \
        int *sv_player_stat_##name[] = { \
            &sv_player##name, \
            &sv_player##name \
        };

    #define PPFVAR(flags, name, mn, mx, w00) \
        GFVAR(flags, player##name, mn, w00, mx); \
        float *sv_player_stat_##name[] = { \
            &sv_player##name, \
            &sv_player##name \
        };

    #define PPSVAR(flags, name, w00) \
        GSVAR(flags, player##name, w00); \
        char **sv_player_stat_##name[] = { \
            &sv_player##name, \
            &sv_player##name \
        };

    #define PLAYER(t,name)         (*sv_player_stat_##name[t])
#else
#ifdef GAMEWORLD
    #define PPVAR(flags, name, mn, mx, w00) \
        GVAR(flags, player##name, mn, w00, mx); \
        int *player_stat_##name[] = { \
            &player##name, \
            &player##name \
        };

    #define PPFVAR(flags, name, mn, mx, w00) \
        GFVAR(flags, player##name, mn, w00, mx); \
        float *player_stat_##name[] = { \
            &player##name, \
            &player##name \
        };

    #define PPSVAR(flags, name, w00) \
        GSVAR(flags, player##name, w00); \
        char **player_stat_##name[] = { \
            &player##name, \
            &player##name \
        };
#else
    #define PPVAR(flags, name, mn, mx, w00) \
        GVAR(flags, player##name, mn, w00, mx); \
        extern int *player_stat_##name[];
    #define PPFVAR(flags, name, mn, mx, w00) \
        GFVAR(flags, player##name, mn, w00, mx); \
        extern float *player_stat_##name[];
    #define PPSVAR(flags, name, w00) \
        GSVAR(flags, player##name, w00); \
        extern char **player_stat_##name[];
#endif
    #define PLAYER(t,name)         (*player_stat_##name[t%PLAYERTYPES])
#endif
