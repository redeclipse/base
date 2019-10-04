#ifdef GAMESERVER
    #define APVAR(flags, level, name, mn, mx, w00, w01, w02, w03, w04, w05) \
        GVAR(flags, level, player##name, mn, w00, mx); \
        GVAR(flags, level, bot##name, mn, w01, mx); \
        GVAR(flags, level, turret##name, mn, w02, mx); \
        GVAR(flags, level, grunt##name, mn, w03, mx); \
        GVAR(flags, level, drone##name, mn, w04, mx); \
        GVAR(flags, level, roller##name, mn, w05, mx); \
        int *sv_actor_stat_##name[] = { \
            &sv_player##name, \
            &sv_bot##name, \
            &sv_turret##name, \
            &sv_grunt##name, \
            &sv_drone##name, \
            &sv_roller##name, \
        };

    #define APFVAR(flags, level, name, mn, mx, w00, w01, w02, w03, w04, w05) \
        GFVAR(flags, level, player##name, mn, w00, mx); \
        GFVAR(flags, level, bot##name, mn, w01, mx); \
        GFVAR(flags, level, turret##name, mn, w02, mx); \
        GFVAR(flags, level, grunt##name, mn, w03, mx); \
        GFVAR(flags, level, drone##name, mn, w04, mx); \
        GFVAR(flags, level, roller##name, mn, w05, mx); \
        float *sv_actor_stat_##name[] = { \
            &sv_player##name, \
            &sv_bot##name, \
            &sv_turret##name, \
            &sv_grunt##name, \
            &sv_drone##name, \
            &sv_roller##name, \
        };

    #define APSVAR(flags, level, name, w00, w01, w02, w03, w04, w05) \
        GSVAR(flags, level, player##name, w00); \
        GSVAR(flags, level, bot##name, w01); \
        GSVAR(flags, level, turret##name, w02); \
        GSVAR(flags, level, grunt##name, w03); \
        GSVAR(flags, level, drone##name, w04); \
        GSVAR(flags, level, roller##name, w05); \
        char **sv_actor_stat_##name[] = { \
            &sv_player##name, \
            &sv_bot##name, \
            &sv_turret##name, \
            &sv_grunt##name, \
            &sv_drone##name, \
            &sv_roller##name, \
        };

    #define AA(t,name)         (*sv_actor_stat_##name[t])
#else
#ifdef GAMEWORLD
    #define APVAR(flags, level, name, mn, mx, w00, w01, w02, w03, w04, w05) \
        GVAR(flags, level, player##name, mn, w00, mx); \
        GVAR(flags, level, bot##name, mn, w01, mx); \
        GVAR(flags, level, turret##name, mn, w02, mx); \
        GVAR(flags, level, grunt##name, mn, w03, mx); \
        GVAR(flags, level, drone##name, mn, w04, mx); \
        GVAR(flags, level, roller##name, mn, w05, mx); \
        int *actor_stat_##name[] = { \
            &player##name, \
            &bot##name, \
            &turret##name, \
            &grunt##name, \
            &drone##name, \
            &roller##name, \
        };

    #define APFVAR(flags, level, name, mn, mx, w00, w01, w02, w03, w04, w05) \
        GFVAR(flags, level, player##name, mn, w00, mx); \
        GFVAR(flags, level, bot##name, mn, w01, mx); \
        GFVAR(flags, level, turret##name, mn, w02, mx); \
        GFVAR(flags, level, grunt##name, mn, w03, mx); \
        GFVAR(flags, level, drone##name, mn, w04, mx); \
        GFVAR(flags, level, roller##name, mn, w05, mx); \
        float *actor_stat_##name[] = { \
            &player##name, \
            &bot##name, \
            &turret##name, \
            &grunt##name, \
            &drone##name, \
            &roller##name, \
        };

    #define APSVAR(flags, level, name, w00, w01, w02, w03, w04, w05) \
        GSVAR(flags, level, player##name, w00); \
        GSVAR(flags, level, bot##name, w01); \
        GSVAR(flags, level, turret##name, w02); \
        GSVAR(flags, level, grunt##name, w03); \
        GSVAR(flags, level, drone##name, w04); \
        GSVAR(flags, level, roller##name, w05); \
        char **actor_stat_##name[] = { \
            &player##name, \
            &bot##name, \
            &turret##name, \
            &grunt##name, \
            &drone##name, \
            &roller##name, \
        };
#else
    #define APVAR(flags, level, name, mn, mx, w00, w01, w02, w03, w04, w05) \
        GVAR(flags, level, player##name, mn, w00, mx); \
        GVAR(flags, level, bot##name, mn, w01, mx); \
        GVAR(flags, level, turret##name, mn, w02, mx); \
        GVAR(flags, level, grunt##name, mn, w03, mx); \
        GVAR(flags, level, drone##name, mn, w04, mx); \
        GVAR(flags, level, roller##name, mn, w05, mx); \
        extern int *actor_stat_##name[];
    #define APFVAR(flags, level, name, mn, mx, w00, w01, w02, w03, w04, w05) \
        GFVAR(flags, level, player##name, mn, w00, mx); \
        GFVAR(flags, level, bot##name, mn, w01, mx); \
        GFVAR(flags, level, turret##name, mn, w02, mx); \
        GFVAR(flags, level, grunt##name, mn, w03, mx); \
        GFVAR(flags, level, drone##name, mn, w04, mx); \
        GFVAR(flags, level, roller##name, mn, w05, mx); \
        extern float *actor_stat_##name[];
    #define APSVAR(flags, level, name, w00, w01, w02, w03, w04, w05) \
        GSVAR(flags, level, player##name, w00); \
        GSVAR(flags, level, bot##name, w01); \
        GSVAR(flags, level, turret##name, w02); \
        GSVAR(flags, level, grunt##name, w03); \
        GSVAR(flags, level, drone##name, w04); \
        GSVAR(flags, level, roller##name, w05); \
        extern char **actor_stat_##name[];
#endif
    #define AA(t,name)         (*actor_stat_##name[t])
#endif
