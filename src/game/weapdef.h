#ifdef GAMESERVER
    #define WPVAR(flags, name, mn, mx, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12) \
        GVAR(flags, claw##name, mn, w00, mx); \
        GVAR(flags, pistol##name, mn, w01, mx); \
        GVAR(flags, sword##name, mn, w02, mx); \
        GVAR(flags, shotgun##name, mn, w03, mx); \
        GVAR(flags, smg##name, mn, w04, mx); \
        GVAR(flags, flamer##name, mn, w05, mx); \
        GVAR(flags, plasma##name, mn, w06, mx); \
        GVAR(flags, zapper##name, mn, w07, mx); \
        GVAR(flags, rifle##name, mn, w08, mx); \
        GVAR(flags, grenade##name, mn, w09, mx); \
        GVAR(flags, mine##name, mn, w10, mx); \
        GVAR(flags, rocket##name, mn, w11, mx); \
        GVAR(flags, melee##name, mn, w12, mx); \
        int *sv_weap_stat_##name[] = { \
            &sv_claw##name, \
            &sv_pistol##name, \
            &sv_sword##name, \
            &sv_shotgun##name, \
            &sv_smg##name, \
            &sv_flamer##name, \
            &sv_plasma##name, \
            &sv_zapper##name, \
            &sv_rifle##name, \
            &sv_grenade##name, \
            &sv_mine##name, \
            &sv_rocket##name, \
            &sv_melee##name \
        };

    #define WPVARM(flags, name, mn, mx, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212) \
        GVAR(flags, claw##name##1, mn, w100, mx);    GVAR(flags, claw##name##2, mn, w200, mx); \
        GVAR(flags, pistol##name##1, mn, w101, mx);  GVAR(flags, pistol##name##2, mn, w201, mx); \
        GVAR(flags, sword##name##1, mn, w102, mx);   GVAR(flags, sword##name##2, mn, w202, mx); \
        GVAR(flags, shotgun##name##1, mn, w103, mx); GVAR(flags, shotgun##name##2, mn, w203, mx); \
        GVAR(flags, smg##name##1, mn, w104, mx);     GVAR(flags, smg##name##2, mn, w204, mx); \
        GVAR(flags, flamer##name##1, mn, w105, mx);  GVAR(flags, flamer##name##2, mn, w205, mx); \
        GVAR(flags, plasma##name##1, mn, w106, mx);  GVAR(flags, plasma##name##2, mn, w206, mx); \
        GVAR(flags, zapper##name##1, mn, w107, mx);  GVAR(flags, zapper##name##2, mn, w207, mx); \
        GVAR(flags, rifle##name##1, mn, w108, mx);   GVAR(flags, rifle##name##2, mn, w208, mx);\
        GVAR(flags, grenade##name##1, mn, w109, mx); GVAR(flags, grenade##name##2, mn, w209, mx); \
        GVAR(flags, mine##name##1, mn, w110, mx);    GVAR(flags, mine##name##2, mn, w210, mx); \
        GVAR(flags, rocket##name##1, mn, w111, mx);  GVAR(flags, rocket##name##2, mn, w211, mx); \
        GVAR(flags, melee##name##1, mn, w112, mx);   GVAR(flags, melee##name##2, mn, w212, mx); \
        int *sv_weap_stat_##name[][2] = { \
            { &sv_claw##name##1,     &sv_claw##name##2 }, \
            { &sv_pistol##name##1,   &sv_pistol##name##2 }, \
            { &sv_sword##name##1,    &sv_sword##name##2 }, \
            { &sv_shotgun##name##1,  &sv_shotgun##name##2 }, \
            { &sv_smg##name##1,      &sv_smg##name##2 }, \
            { &sv_flamer##name##1,   &sv_flamer##name##2 }, \
            { &sv_plasma##name##1,   &sv_plasma##name##2 }, \
            { &sv_zapper##name##1,   &sv_zapper##name##2 }, \
            { &sv_rifle##name##1,    &sv_rifle##name##2 }, \
            { &sv_grenade##name##1,  &sv_grenade##name##2 }, \
            { &sv_mine##name##1,     &sv_mine##name##2 }, \
            { &sv_rocket##name##1,   &sv_rocket##name##2 }, \
            { &sv_melee##name##1,    &sv_melee##name##2 } \
        };

    #define WPFVAR(flags, name, mn, mx, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12) \
        GFVAR(flags, claw##name, mn, w00, mx); \
        GFVAR(flags, pistol##name, mn, w01, mx); \
        GFVAR(flags, sword##name, mn, w02, mx); \
        GFVAR(flags, shotgun##name, mn, w03, mx); \
        GFVAR(flags, smg##name, mn, w04, mx); \
        GFVAR(flags, flamer##name, mn, w05, mx); \
        GFVAR(flags, plasma##name, mn, w06, mx); \
        GFVAR(flags, zapper##name, mn, w07, mx); \
        GFVAR(flags, rifle##name, mn, w08, mx); \
        GFVAR(flags, grenade##name, mn, w09, mx); \
        GFVAR(flags, mine##name, mn, w10, mx); \
        GFVAR(flags, rocket##name, mn, w11, mx); \
        GFVAR(flags, melee##name, mn, w12, mx); \
        float *sv_weap_stat_##name[] = { \
            &sv_claw##name, \
            &sv_pistol##name, \
            &sv_sword##name, \
            &sv_shotgun##name, \
            &sv_smg##name, \
            &sv_flamer##name, \
            &sv_plasma##name, \
            &sv_zapper##name, \
            &sv_rifle##name, \
            &sv_grenade##name, \
            &sv_mine##name, \
            &sv_rocket##name, \
            &sv_melee##name \
        };

    #define WPFVARM(flags, name, mn, mx, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212) \
        GFVAR(flags, claw##name##1, mn, w100, mx);    GFVAR(flags, claw##name##2, mn, w200, mx); \
        GFVAR(flags, pistol##name##1, mn, w101, mx);  GFVAR(flags, pistol##name##2, mn, w201, mx); \
        GFVAR(flags, sword##name##1, mn, w102, mx);   GFVAR(flags, sword##name##2, mn, w202, mx); \
        GFVAR(flags, shotgun##name##1, mn, w103, mx); GFVAR(flags, shotgun##name##2, mn, w203, mx); \
        GFVAR(flags, smg##name##1, mn, w104, mx);     GFVAR(flags, smg##name##2, mn, w204, mx); \
        GFVAR(flags, flamer##name##1, mn, w105, mx);  GFVAR(flags, flamer##name##2, mn, w205, mx); \
        GFVAR(flags, plasma##name##1, mn, w106, mx);  GFVAR(flags, plasma##name##2, mn, w206, mx); \
        GFVAR(flags, zapper##name##1, mn, w107, mx);  GFVAR(flags, zapper##name##2, mn, w207, mx); \
        GFVAR(flags, rifle##name##1, mn, w108, mx);   GFVAR(flags, rifle##name##2, mn, w208, mx);\
        GFVAR(flags, grenade##name##1, mn, w109, mx); GFVAR(flags, grenade##name##2, mn, w209, mx); \
        GFVAR(flags, mine##name##1, mn, w110, mx);    GFVAR(flags, mine##name##2, mn, w210, mx); \
        GFVAR(flags, rocket##name##1, mn, w111, mx);  GFVAR(flags, rocket##name##2, mn, w211, mx); \
        GFVAR(flags, melee##name##1, mn, w112, mx);   GFVAR(flags, melee##name##2, mn, w212, mx); \
        float *sv_weap_stat_##name[][2] = { \
            { &sv_claw##name##1,     &sv_claw##name##2 }, \
            { &sv_pistol##name##1,   &sv_pistol##name##2 }, \
            { &sv_sword##name##1,    &sv_sword##name##2 }, \
            { &sv_shotgun##name##1,  &sv_shotgun##name##2 }, \
            { &sv_smg##name##1,      &sv_smg##name##2 }, \
            { &sv_flamer##name##1,   &sv_flamer##name##2 }, \
            { &sv_plasma##name##1,   &sv_plasma##name##2 }, \
            { &sv_zapper##name##1,   &sv_zapper##name##2 }, \
            { &sv_rifle##name##1,    &sv_rifle##name##2 }, \
            { &sv_grenade##name##1,  &sv_grenade##name##2 }, \
            { &sv_mine##name##1,     &sv_mine##name##2 }, \
            { &sv_rocket##name##1,   &sv_rocket##name##2 }, \
            { &sv_melee##name##1,    &sv_melee##name##2 } \
        };

    #define WPSVAR(flags, name, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12) \
        GSVAR(flags, claw##name, w00); \
        GSVAR(flags, pistol##name, w01); \
        GSVAR(flags, sword##name, w02); \
        GSVAR(flags, shotgun##name, w03); \
        GSVAR(flags, smg##name, w04); \
        GSVAR(flags, flamer##name, w05); \
        GSVAR(flags, plasma##name, w06); \
        GSVAR(flags, zapper##name, w07); \
        GSVAR(flags, rifle##name, w08); \
        GSVAR(flags, grenade##name, w09); \
        GSVAR(flags, mine##name, w10); \
        GSVAR(flags, rocket##name, w11); \
        GSVAR(flags, melee##name, w12); \
        char **sv_weap_stat_##name[] = { \
            &sv_claw##name, \
            &sv_pistol##name, \
            &sv_sword##name, \
            &sv_shotgun##name, \
            &sv_smg##name, \
            &sv_flamer##name, \
            &sv_plasma##name, \
            &sv_zapper##name, \
            &sv_rifle##name, \
            &sv_grenade##name, \
            &sv_mine##name, \
            &sv_rocket##name, \
            &sv_rocket##name \
        };

    #define WPSVARM(flags, name, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212) \
        GSVAR(flags, claw##name##1, w100);    GSVAR(flags, claw##name##2, w200); \
        GSVAR(flags, pistol##name##1, w101);  GSVAR(flags, pistol##name##2, w201); \
        GSVAR(flags, sword##name##1, w102);   GSVAR(flags, sword##name##2, w202); \
        GSVAR(flags, shotgun##name##1, w103); GSVAR(flags, shotgun##name##2, w203); \
        GSVAR(flags, smg##name##1, w104);     GSVAR(flags, smg##name##2, w204); \
        GSVAR(flags, flamer##name##1, w105);  GSVAR(flags, flamer##name##2, w205); \
        GSVAR(flags, plasma##name##1, w106);  GSVAR(flags, plasma##name##2, w206); \
        GSVAR(flags, zapper##name##1, w107);  GSVAR(flags, zapper##name##2, w207); \
        GSVAR(flags, rifle##name##1, w108);   GSVAR(flags, rifle##name##2, w208);\
        GSVAR(flags, grenade##name##1, w109); GSVAR(flags, grenade##name##2, w209); \
        GSVAR(flags, mine##name##1, w110);    GSVAR(flags, mine##name##2, w210); \
        GSVAR(flags, rocket##name##1, w111);  GSVAR(flags, rocket##name##2, w211); \
        GSVAR(flags, melee##name##1, w112);   GSVAR(flags, melee##name##2, w212); \
        char **sv_weap_stat_##name[][2] = { \
            { &sv_claw##name##1,     &sv_claw##name##2 }, \
            { &sv_pistol##name##1,   &sv_pistol##name##2 }, \
            { &sv_sword##name##1,    &sv_sword##name##2 }, \
            { &sv_shotgun##name##1,  &sv_shotgun##name##2 }, \
            { &sv_smg##name##1,      &sv_smg##name##2 }, \
            { &sv_flamer##name##1,   &sv_flamer##name##2 }, \
            { &sv_plasma##name##1,   &sv_plasma##name##2 }, \
            { &sv_zapper##name##1,   &sv_zapper##name##2 }, \
            { &sv_rifle##name##1,    &sv_rifle##name##2 }, \
            { &sv_grenade##name##1,  &sv_grenade##name##2 }, \
            { &sv_mine##name##1,     &sv_mine##name##2 }, \
            { &sv_rocket##name##1,   &sv_rocket##name##2 }, \
            { &sv_melee##name##1,    &sv_melee##name##2 } \
        };

    #define W(weap,name)         (*sv_weap_stat_##name[weap])
    #define W2(weap,name,second) (*sv_weap_stat_##name[weap][second?1:0])
#else
#ifdef GAMEWORLD
    #define WPVAR(flags, name, mn, mx, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12) \
        GVAR(flags, claw##name, mn, w00, mx); \
        GVAR(flags, pistol##name, mn, w01, mx); \
        GVAR(flags, sword##name, mn, w02, mx); \
        GVAR(flags, shotgun##name, mn, w03, mx); \
        GVAR(flags, smg##name, mn, w04, mx); \
        GVAR(flags, flamer##name, mn, w05, mx); \
        GVAR(flags, plasma##name, mn, w06, mx); \
        GVAR(flags, zapper##name, mn, w07, mx); \
        GVAR(flags, rifle##name, mn, w08, mx); \
        GVAR(flags, grenade##name, mn, w09, mx); \
        GVAR(flags, mine##name, mn, w10, mx); \
        GVAR(flags, rocket##name, mn, w11, mx); \
        GVAR(flags, melee##name, mn, w12, mx); \
        int *weap_stat_##name[] = { \
            &claw##name, \
            &pistol##name, \
            &sword##name, \
            &shotgun##name, \
            &smg##name, \
            &flamer##name, \
            &plasma##name, \
            &zapper##name, \
            &rifle##name, \
            &grenade##name, \
            &mine##name, \
            &rocket##name, \
            &melee##name \
        };

    #define WPVARM(flags, name, mn, mx, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212) \
        GVAR(flags, claw##name##1, mn, w100, mx);    GVAR(flags, claw##name##2, mn, w200, mx); \
        GVAR(flags, pistol##name##1, mn, w101, mx);  GVAR(flags, pistol##name##2, mn, w201, mx); \
        GVAR(flags, sword##name##1, mn, w102, mx);   GVAR(flags, sword##name##2, mn, w202, mx); \
        GVAR(flags, shotgun##name##1, mn, w103, mx); GVAR(flags, shotgun##name##2, mn, w203, mx); \
        GVAR(flags, smg##name##1, mn, w104, mx);     GVAR(flags, smg##name##2, mn, w204, mx); \
        GVAR(flags, flamer##name##1, mn, w105, mx);  GVAR(flags, flamer##name##2, mn, w205, mx); \
        GVAR(flags, plasma##name##1, mn, w106, mx);  GVAR(flags, plasma##name##2, mn, w206, mx); \
        GVAR(flags, zapper##name##1, mn, w107, mx);  GVAR(flags, zapper##name##2, mn, w207, mx); \
        GVAR(flags, rifle##name##1, mn, w108, mx);   GVAR(flags, rifle##name##2, mn, w208, mx);\
        GVAR(flags, grenade##name##1, mn, w109, mx); GVAR(flags, grenade##name##2, mn, w209, mx); \
        GVAR(flags, mine##name##1, mn, w110, mx);    GVAR(flags, mine##name##2, mn, w210, mx); \
        GVAR(flags, rocket##name##1, mn, w111, mx);  GVAR(flags, rocket##name##2, mn, w211, mx); \
        GVAR(flags, melee##name##1, mn, w112, mx);   GVAR(flags, melee##name##2, mn, w212, mx); \
        int *weap_stat_##name[][2] = { \
            { &claw##name##1,     &claw##name##2 }, \
            { &pistol##name##1,   &pistol##name##2 }, \
            { &sword##name##1,    &sword##name##2 }, \
            { &shotgun##name##1,  &shotgun##name##2 }, \
            { &smg##name##1,      &smg##name##2 }, \
            { &flamer##name##1,   &flamer##name##2 }, \
            { &plasma##name##1,   &plasma##name##2 }, \
            { &zapper##name##1,   &zapper##name##2 }, \
            { &rifle##name##1,    &rifle##name##2 }, \
            { &grenade##name##1,  &grenade##name##2 }, \
            { &mine##name##1,     &mine##name##2 }, \
            { &rocket##name##1,   &rocket##name##2 }, \
            { &melee##name##1,    &melee##name##2 } \
        };

    #define WPFVAR(flags, name, mn, mx, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12) \
        GFVAR(flags, claw##name, mn, w00, mx); \
        GFVAR(flags, pistol##name, mn, w01, mx); \
        GFVAR(flags, sword##name, mn, w02, mx); \
        GFVAR(flags, shotgun##name, mn, w03, mx); \
        GFVAR(flags, smg##name, mn, w04, mx); \
        GFVAR(flags, flamer##name, mn, w05, mx); \
        GFVAR(flags, plasma##name, mn, w06, mx); \
        GFVAR(flags, zapper##name, mn, w07, mx); \
        GFVAR(flags, rifle##name, mn, w08, mx); \
        GFVAR(flags, grenade##name, mn, w09, mx); \
        GFVAR(flags, mine##name, mn, w10, mx); \
        GFVAR(flags, rocket##name, mn, w11, mx); \
        GFVAR(flags, melee##name, mn, w12, mx); \
        float *weap_stat_##name[] = { \
            &claw##name, \
            &pistol##name, \
            &sword##name, \
            &shotgun##name, \
            &smg##name, \
            &flamer##name, \
            &plasma##name, \
            &zapper##name, \
            &rifle##name, \
            &grenade##name, \
            &mine##name, \
            &rocket##name, \
            &melee##name \
        };

    #define WPFVARM(flags, name, mn, mx, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212) \
        GFVAR(flags, claw##name##1, mn, w100, mx);    GFVAR(flags, claw##name##2, mn, w200, mx); \
        GFVAR(flags, pistol##name##1, mn, w101, mx);  GFVAR(flags, pistol##name##2, mn, w201, mx); \
        GFVAR(flags, sword##name##1, mn, w102, mx);   GFVAR(flags, sword##name##2, mn, w202, mx); \
        GFVAR(flags, shotgun##name##1, mn, w103, mx); GFVAR(flags, shotgun##name##2, mn, w203, mx); \
        GFVAR(flags, smg##name##1, mn, w104, mx);     GFVAR(flags, smg##name##2, mn, w204, mx); \
        GFVAR(flags, flamer##name##1, mn, w105, mx);  GFVAR(flags, flamer##name##2, mn, w205, mx); \
        GFVAR(flags, plasma##name##1, mn, w106, mx);  GFVAR(flags, plasma##name##2, mn, w206, mx); \
        GFVAR(flags, zapper##name##1, mn, w107, mx);  GFVAR(flags, zapper##name##2, mn, w207, mx); \
        GFVAR(flags, rifle##name##1, mn, w108, mx);   GFVAR(flags, rifle##name##2, mn, w208, mx);\
        GFVAR(flags, grenade##name##1, mn, w109, mx); GFVAR(flags, grenade##name##2, mn, w209, mx); \
        GFVAR(flags, mine##name##1, mn, w110, mx);    GFVAR(flags, mine##name##2, mn, w210, mx); \
        GFVAR(flags, rocket##name##1, mn, w111, mx);  GFVAR(flags, rocket##name##2, mn, w211, mx); \
        GFVAR(flags, melee##name##1, mn, w112, mx);   GFVAR(flags, melee##name##2, mn, w212, mx); \
        float *weap_stat_##name[][2] = { \
            { &claw##name##1,     &claw##name##2 }, \
            { &pistol##name##1,   &pistol##name##2 }, \
            { &sword##name##1,    &sword##name##2 }, \
            { &shotgun##name##1,  &shotgun##name##2 }, \
            { &smg##name##1,      &smg##name##2 }, \
            { &flamer##name##1,   &flamer##name##2 }, \
            { &plasma##name##1,   &plasma##name##2 }, \
            { &zapper##name##1,   &zapper##name##2 }, \
            { &rifle##name##1,    &rifle##name##2 }, \
            { &grenade##name##1,  &grenade##name##2 }, \
            { &mine##name##1,     &mine##name##2 }, \
            { &rocket##name##1,   &rocket##name##2 }, \
            { &melee##name##1,    &melee##name##2 } \
        };

    #define WPSVAR(flags, name, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12) \
        GSVAR(flags, claw##name, w00); \
        GSVAR(flags, pistol##name, w01); \
        GSVAR(flags, sword##name, w02); \
        GSVAR(flags, shotgun##name, w03); \
        GSVAR(flags, smg##name, w04); \
        GSVAR(flags, flamer##name, w05); \
        GSVAR(flags, plasma##name, w06); \
        GSVAR(flags, zapper##name, w07); \
        GSVAR(flags, rifle##name, w08); \
        GSVAR(flags, grenade##name, w09); \
        GSVAR(flags, mine##name, w10); \
        GSVAR(flags, rocket##name, w11); \
        GSVAR(flags, melee##name, w12); \
        char **weap_stat_##name[] = { \
            &claw##name, \
            &pistol##name, \
            &sword##name, \
            &shotgun##name, \
            &smg##name, \
            &flamer##name, \
            &plasma##name, \
            &zapper##name, \
            &rifle##name, \
            &grenade##name, \
            &mine##name, \
            &rocket##name, \
            &melee##name \
        };

    #define WPSVARM(flags, name, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212) \
        GSVAR(flags, claw##name##1, w100);    GSVAR(flags, claw##name##2, w200); \
        GSVAR(flags, pistol##name##1, w101);  GSVAR(flags, pistol##name##2, w201); \
        GSVAR(flags, sword##name##1, w102);   GSVAR(flags, sword##name##2, w202); \
        GSVAR(flags, shotgun##name##1, w103); GSVAR(flags, shotgun##name##2, w203); \
        GSVAR(flags, smg##name##1, w104);     GSVAR(flags, smg##name##2, w204); \
        GSVAR(flags, flamer##name##1, w105);  GSVAR(flags, flamer##name##2, w205); \
        GSVAR(flags, plasma##name##1, w106);  GSVAR(flags, plasma##name##2, w206); \
        GSVAR(flags, zapper##name##1, w107);  GSVAR(flags, zapper##name##2, w207); \
        GSVAR(flags, rifle##name##1, w108);   GSVAR(flags, rifle##name##2, w208);\
        GSVAR(flags, grenade##name##1, w109); GSVAR(flags, grenade##name##2, w209); \
        GSVAR(flags, mine##name##1, w110);    GSVAR(flags, mine##name##2, w210); \
        GSVAR(flags, rocket##name##1, w111);  GSVAR(flags, rocket##name##2, w211); \
        GSVAR(flags, melee##name##1, w112);   GSVAR(flags, melee##name##2, w212); \
        char **weap_stat_##name[][2] = { \
            { &claw##name##1,     &claw##name##2 }, \
            { &pistol##name##1,   &pistol##name##2 }, \
            { &sword##name##1,    &sword##name##2 }, \
            { &shotgun##name##1,  &shotgun##name##2 }, \
            { &smg##name##1,      &smg##name##2 }, \
            { &flamer##name##1,   &flamer##name##2 }, \
            { &plasma##name##1,   &plasma##name##2 }, \
            { &zapper##name##1,   &zapper##name##2 }, \
            { &rifle##name##1,    &rifle##name##2 }, \
            { &grenade##name##1,  &grenade##name##2 }, \
            { &mine##name##1,     &mine##name##2 }, \
            { &rocket##name##1,   &rocket##name##2 }, \
            { &melee##name##1,    &melee##name##2 } \
        };
#else
    #define WPVAR(flags, name, mn, mx, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12) \
        GVAR(flags, claw##name, mn, w00, mx); \
        GVAR(flags, pistol##name, mn, w01, mx); \
        GVAR(flags, sword##name, mn, w02, mx); \
        GVAR(flags, shotgun##name, mn, w03, mx); \
        GVAR(flags, smg##name, mn, w04, mx); \
        GVAR(flags, flamer##name, mn, w05, mx); \
        GVAR(flags, plasma##name, mn, w06, mx); \
        GVAR(flags, zapper##name, mn, w07, mx); \
        GVAR(flags, rifle##name, mn, w08, mx); \
        GVAR(flags, grenade##name, mn, w09, mx); \
        GVAR(flags, mine##name, mn, w10, mx); \
        GVAR(flags, rocket##name, mn, w11, mx); \
        GVAR(flags, melee##name, mn, w12, mx); \
        extern int *weap_stat_##name[];
    #define WPVARM(flags, name, mn, mx, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212) \
        GVAR(flags, claw##name##1, mn, w100, mx);   GVAR(flags, claw##name##2, mn, w200, mx); \
        GVAR(flags, pistol##name##1, mn, w101, mx);  GVAR(flags, pistol##name##2, mn, w201, mx); \
        GVAR(flags, sword##name##1, mn, w102, mx);   GVAR(flags, sword##name##2, mn, w202, mx); \
        GVAR(flags, shotgun##name##1, mn, w103, mx); GVAR(flags, shotgun##name##2, mn, w203, mx); \
        GVAR(flags, smg##name##1, mn, w104, mx);     GVAR(flags, smg##name##2, mn, w204, mx); \
        GVAR(flags, flamer##name##1, mn, w105, mx);  GVAR(flags, flamer##name##2, mn, w205, mx); \
        GVAR(flags, plasma##name##1, mn, w106, mx);  GVAR(flags, plasma##name##2, mn, w206, mx); \
        GVAR(flags, zapper##name##1, mn, w107, mx);  GVAR(flags, zapper##name##2, mn, w207, mx); \
        GVAR(flags, rifle##name##1, mn, w108, mx);   GVAR(flags, rifle##name##2, mn, w208, mx);\
        GVAR(flags, grenade##name##1, mn, w109, mx); GVAR(flags, grenade##name##2, mn, w209, mx); \
        GVAR(flags, mine##name##1, mn, w110, mx);    GVAR(flags, mine##name##2, mn, w210, mx); \
        GVAR(flags, rocket##name##1, mn, w111, mx);  GVAR(flags, rocket##name##2, mn, w211, mx); \
        GVAR(flags, melee##name##1, mn, w112, mx);   GVAR(flags, melee##name##2, mn, w212, mx); \
        extern int *weap_stat_##name[][2];
    #define WPFVAR(flags, name, mn, mx, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12) \
        GFVAR(flags, claw##name, mn, w00, mx); \
        GFVAR(flags, pistol##name, mn, w01, mx); \
        GFVAR(flags, sword##name, mn, w02, mx); \
        GFVAR(flags, shotgun##name, mn, w03, mx); \
        GFVAR(flags, smg##name, mn, w04, mx); \
        GFVAR(flags, flamer##name, mn, w05, mx); \
        GFVAR(flags, plasma##name, mn, w06, mx); \
        GFVAR(flags, zapper##name, mn, w07, mx); \
        GFVAR(flags, rifle##name, mn, w08, mx); \
        GFVAR(flags, grenade##name, mn, w09, mx); \
        GFVAR(flags, mine##name, mn, w10, mx); \
        GFVAR(flags, rocket##name, mn, w11, mx); \
        GFVAR(flags, melee##name, mn, w12, mx); \
        extern float *weap_stat_##name[];
    #define WPFVARM(flags, name, mn, mx, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212) \
        GFVAR(flags, claw##name##1, mn, w100, mx);    GFVAR(flags, claw##name##2, mn, w200, mx); \
        GFVAR(flags, pistol##name##1, mn, w101, mx);  GFVAR(flags, pistol##name##2, mn, w201, mx); \
        GFVAR(flags, sword##name##1, mn, w102, mx);   GFVAR(flags, sword##name##2, mn, w202, mx); \
        GFVAR(flags, shotgun##name##1, mn, w103, mx); GFVAR(flags, shotgun##name##2, mn, w203, mx); \
        GFVAR(flags, smg##name##1, mn, w104, mx);     GFVAR(flags, smg##name##2, mn, w204, mx); \
        GFVAR(flags, flamer##name##1, mn, w105, mx);  GFVAR(flags, flamer##name##2, mn, w205, mx); \
        GFVAR(flags, plasma##name##1, mn, w106, mx);  GFVAR(flags, plasma##name##2, mn, w206, mx); \
        GFVAR(flags, zapper##name##1, mn, w107, mx);  GFVAR(flags, zapper##name##2, mn, w207, mx); \
        GFVAR(flags, rifle##name##1, mn, w108, mx);   GFVAR(flags, rifle##name##2, mn, w208, mx);\
        GFVAR(flags, grenade##name##1, mn, w109, mx); GFVAR(flags, grenade##name##2, mn, w209, mx); \
        GFVAR(flags, mine##name##1, mn, w110, mx);    GFVAR(flags, mine##name##2, mn, w210, mx); \
        GFVAR(flags, rocket##name##1, mn, w111, mx);  GFVAR(flags, rocket##name##2, mn, w211, mx); \
        GFVAR(flags, melee##name##1, mn, w112, mx);   GFVAR(flags, melee##name##2, mn, w212, mx); \
        extern float *weap_stat_##name[][2];
    #define WPSVAR(flags, name, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12) \
        GSVAR(flags, claw##name, w00); \
        GSVAR(flags, pistol##name, w01); \
        GSVAR(flags, sword##name, w02); \
        GSVAR(flags, shotgun##name, w03); \
        GSVAR(flags, smg##name, w04); \
        GSVAR(flags, flamer##name, w05); \
        GSVAR(flags, plasma##name, w06); \
        GSVAR(flags, zapper##name, w07); \
        GSVAR(flags, rifle##name, w08); \
        GSVAR(flags, grenade##name, w09); \
        GSVAR(flags, mine##name, w10); \
        GSVAR(flags, rocket##name, w11); \
        GSVAR(flags, melee##name, w12); \
        extern char **weap_stat_##name[];
    #define WPSVARM(flags, name, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212) \
        GSVAR(flags, claw##name##1, w100);    GSVAR(flags, claw##name##2, w200); \
        GSVAR(flags, pistol##name##1, w101);  GSVAR(flags, pistol##name##2, w201); \
        GSVAR(flags, sword##name##1, w102);   GSVAR(flags, sword##name##2, w202); \
        GSVAR(flags, shotgun##name##1, w103); GSVAR(flags, shotgun##name##2, w203); \
        GSVAR(flags, smg##name##1, w104);     GSVAR(flags, smg##name##2, w204); \
        GSVAR(flags, flamer##name##1, w105);  GSVAR(flags, flamer##name##2, w205); \
        GSVAR(flags, plasma##name##1, w106);  GSVAR(flags, plasma##name##2, w206); \
        GSVAR(flags, zapper##name##1, w107);  GSVAR(flags, zapper##name##2, w207); \
        GSVAR(flags, rifle##name##1, w108);   GSVAR(flags, rifle##name##2, w208);\
        GSVAR(flags, grenade##name##1, w109); GSVAR(flags, grenade##name##2, w209); \
        GSVAR(flags, mine##name##1, w110);    GSVAR(flags, mine##name##2, w210); \
        GSVAR(flags, rocket##name##1, w111);  GSVAR(flags, rocket##name##2, w211); \
        GSVAR(flags, melee##name##1, w112);   GSVAR(flags, melee##name##2, w212); \
        extern char **weap_stat_##name[][2];
#endif
    #define W(weap,name)         (*weap_stat_##name[weap])
    #define W2(weap,name,second) (*weap_stat_##name[weap][second ? 1 : 0])
#endif
#define WPVARK(flags, name, mn, mx, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212, w300, w301, w302, w303, w304, w305, w306, w307, w308, w309, w310, w311, w312, w400, w401, w402, w403, w404, w405, w406, w407, w408, w409, w410, w411, w412) \
    WPVARM(flags, name, mn, mx, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212); \
    WPVARM(flags, flak##name, mn, mx, w300, w301, w302, w303, w304, w305, w306, w307, w308, w309, w310, w311, w312, w400, w401, w402, w403, w404, w405, w406, w407, w408, w409, w410, w411, w412);
#define WPFVARK(flags, name, mn, mx, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212, w300, w301, w302, w303, w304, w305, w306, w307, w308, w309, w310, w311, w312, w400, w401, w402, w403, w404, w405, w406, w407, w408, w409, w410, w411, w412) \
    WPFVARM(flags, name, mn, mx, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212); \
    WPFVARM(flags, flak##name, mn, mx, w300, w301, w302, w303, w304, w305, w306, w307, w308, w309, w310, w311, w312, w400, w401, w402, w403, w404, w405, w406, w407, w408, w409, w410, w411, w412);
#define WPSVARK(flags, name, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212, w300, w301, w302, w303, w304, w305, w306, w307, w308, w309, w310, w311, w312, w400, w401, w402, w403, w404, w405, w406, w407, w408, w409, w410, w411, w412) \
    WPSVARM(flags, name, w100, w101, w102, w103, w104, w105, w106, w107, w108, w109, w110, w111, w112, w200, w201, w202, w203, w204, w205, w206, w207, w208, w209, w210, w211, w212); \
    WPSVARM(flags, flak##name, w300, w301, w302, w303, w304, w305, w306, w307, w308, w309, w310, w311, w312, w400, w401, w402, w403, w404, w405, w406, w407, w408, w409, w410, w411, w412);
#define WPSVARR(flags, name, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12) \
    WPSVARM(flags, name, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12); \
    WPSVARM(flags, flak##name, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12, w00, w01, w02, w03, w04, w05, w06, w07, w08, w09, w10, w11, w12);
#define WF(c,w,v,s) (c ? W2(w, flak##v, s) : W2(w, v, s))
#define WS(flags)  (flags&HIT_ALT)
#define WK(flags)  (flags&HIT_FLAK)
