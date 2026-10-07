#ifdef CPP_GAME_SERVER
#define WD_PREFIX sv_
#define WD_DECLARE(a,b) a = { b };
#else // CPP_GAME_SERVER
#define WD_PREFIX
#ifdef CPP_GAME_MAIN
#define WD_DECLARE(a,b) a = { b };
#else // CPP_GAME_MAIN
#define WD_DECLARE(a,b) extern a;
#endif // CPP_GAME_MAIN
#endif // CPP_GAME_SERVER

#define WPVAR(flags, level, name, mn, mx, claw0, pistol0, sword0, shotgun0, smg0, flamer0, plasma0, zapper0, rifle0, corroder0, grenade0, mine0, rocket0, minigun0, jetsaw0, eclipse0, melee0) \
    GVAR(flags, level, claw##name, mn, claw0, mx); \
    GVAR(flags, level, pistol##name, mn, pistol0, mx); \
    GVAR(flags, level, sword##name, mn, sword0, mx); \
    GVAR(flags, level, shotgun##name, mn, shotgun0, mx); \
    GVAR(flags, level, smg##name, mn, smg0, mx); \
    GVAR(flags, level, flamer##name, mn, flamer0, mx); \
    GVAR(flags, level, plasma##name, mn, plasma0, mx); \
    GVAR(flags, level, zapper##name, mn, zapper0, mx); \
    GVAR(flags, level, rifle##name, mn, rifle0, mx); \
    GVAR(flags, level, corroder##name, mn, corroder0, mx); \
    GVAR(flags, level, grenade##name, mn, grenade0, mx); \
    GVAR(flags, level, mine##name, mn, mine0, mx); \
    GVAR(flags, level, rocket##name, mn, rocket0, mx); \
    GVAR(flags, level, minigun##name, mn, minigun0, mx); \
    GVAR(flags, level, jetsaw##name, mn, jetsaw0, mx); \
    GVAR(flags, level, eclipse##name, mn, eclipse0, mx); \
    GVAR(flags, level, melee##name, mn, melee0, mx); \
    WD_DECLARE(int *EXPAND(WD_PREFIX, weap_stat_##name)[], \
        &EXPAND(WD_PREFIX, claw##name) NEXTARG \
        &EXPAND(WD_PREFIX, pistol##name) NEXTARG \
        &EXPAND(WD_PREFIX, sword##name) NEXTARG \
        &EXPAND(WD_PREFIX, shotgun##name) NEXTARG \
        &EXPAND(WD_PREFIX, smg##name) NEXTARG \
        &EXPAND(WD_PREFIX, flamer##name) NEXTARG \
        &EXPAND(WD_PREFIX, plasma##name) NEXTARG \
        &EXPAND(WD_PREFIX, zapper##name) NEXTARG \
        &EXPAND(WD_PREFIX, rifle##name) NEXTARG \
        &EXPAND(WD_PREFIX, corroder##name) NEXTARG \
        &EXPAND(WD_PREFIX, grenade##name) NEXTARG \
        &EXPAND(WD_PREFIX, mine##name) NEXTARG \
        &EXPAND(WD_PREFIX, rocket##name) NEXTARG \
        &EXPAND(WD_PREFIX, minigun##name) NEXTARG \
        &EXPAND(WD_PREFIX, jetsaw##name) NEXTARG \
        &EXPAND(WD_PREFIX, eclipse##name) NEXTARG \
        &EXPAND(WD_PREFIX, melee##name) \
    );

#define WPVARM(flags, level, name, mn, mx, claw1, pistol1, sword1, shotgun1, smg1, flamer1, plasma1, zapper1, rifle1, corroder1, grenade1, mine1, rocket1, minigun1, jetsaw1, eclipse1, melee1, claw2, pistol2, sword2, shotgun2, smg2, flamer2, plasma2, zapper2, rifle2, corroder2, grenade2, mine2, rocket2, minigun2, jetsaw2, eclipse2, melee2) \
    GVAR(flags, level, claw##name##1, mn, claw1, mx);           GVAR(flags, level, claw##name##2, mn, claw2, mx); \
    GVAR(flags, level, pistol##name##1, mn, pistol1, mx);       GVAR(flags, level, pistol##name##2, mn, pistol2, mx); \
    GVAR(flags, level, sword##name##1, mn, sword1, mx);         GVAR(flags, level, sword##name##2, mn, sword2, mx); \
    GVAR(flags, level, shotgun##name##1, mn, shotgun1, mx);     GVAR(flags, level, shotgun##name##2, mn, shotgun2, mx); \
    GVAR(flags, level, smg##name##1, mn, smg1, mx);             GVAR(flags, level, smg##name##2, mn, smg2, mx); \
    GVAR(flags, level, flamer##name##1, mn, flamer1, mx);       GVAR(flags, level, flamer##name##2, mn, flamer2, mx); \
    GVAR(flags, level, plasma##name##1, mn, plasma1, mx);       GVAR(flags, level, plasma##name##2, mn, plasma2, mx); \
    GVAR(flags, level, zapper##name##1, mn, zapper1, mx);       GVAR(flags, level, zapper##name##2, mn, zapper2, mx); \
    GVAR(flags, level, rifle##name##1, mn, rifle1, mx);         GVAR(flags, level, rifle##name##2, mn, rifle2, mx);\
    GVAR(flags, level, corroder##name##1, mn, corroder1, mx);   GVAR(flags, level, corroder##name##2, mn, corroder2, mx);\
    GVAR(flags, level, grenade##name##1, mn, grenade1, mx);     GVAR(flags, level, grenade##name##2, mn, grenade2, mx); \
    GVAR(flags, level, mine##name##1, mn, mine1, mx);           GVAR(flags, level, mine##name##2, mn, mine2, mx); \
    GVAR(flags, level, rocket##name##1, mn, rocket1, mx);       GVAR(flags, level, rocket##name##2, mn, rocket2, mx); \
    GVAR(flags, level, minigun##name##1, mn, minigun1, mx);     GVAR(flags, level, minigun##name##2, mn, minigun2, mx); \
    GVAR(flags, level, jetsaw##name##1, mn, jetsaw1, mx);       GVAR(flags, level, jetsaw##name##2, mn, jetsaw2, mx); \
    GVAR(flags, level, eclipse##name##1, mn, eclipse1, mx);     GVAR(flags, level, eclipse##name##2, mn, eclipse2, mx); \
    GVAR(flags, level, melee##name##1, mn, melee1, mx);         GVAR(flags, level, melee##name##2, mn, melee2, mx); \
    WD_DECLARE(int *EXPAND(WD_PREFIX, weap_stat_##name)[][2], \
        { &EXPAND(WD_PREFIX, claw##name##1) NEXTARG       &EXPAND(WD_PREFIX, claw##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, pistol##name##1) NEXTARG     &EXPAND(WD_PREFIX, pistol##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, sword##name##1) NEXTARG      &EXPAND(WD_PREFIX, sword##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, shotgun##name##1) NEXTARG    &EXPAND(WD_PREFIX, shotgun##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, smg##name##1) NEXTARG        &EXPAND(WD_PREFIX, smg##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, flamer##name##1) NEXTARG     &EXPAND(WD_PREFIX, flamer##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, plasma##name##1) NEXTARG     &EXPAND(WD_PREFIX, plasma##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, zapper##name##1) NEXTARG     &EXPAND(WD_PREFIX, zapper##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, rifle##name##1) NEXTARG      &EXPAND(WD_PREFIX, rifle##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, corroder##name##1) NEXTARG   &EXPAND(WD_PREFIX, corroder##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, grenade##name##1) NEXTARG    &EXPAND(WD_PREFIX, grenade##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, mine##name##1) NEXTARG       &EXPAND(WD_PREFIX, mine##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, rocket##name##1) NEXTARG     &EXPAND(WD_PREFIX, rocket##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, minigun##name##1) NEXTARG    &EXPAND(WD_PREFIX, minigun##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, jetsaw##name##1) NEXTARG     &EXPAND(WD_PREFIX, jetsaw##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, eclipse##name##1) NEXTARG    &EXPAND(WD_PREFIX, eclipse##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, melee##name##1) NEXTARG      &EXPAND(WD_PREFIX, melee##name##2) } \
    );

#define WPFVAR(flags, level, name, mn, mx, claw0, pistol0, sword0, shotgun0, smg0, flamer0, plasma0, zapper0, rifle0, corroder0, grenade0, mine0, rocket0, minigun0, jetsaw0, eclipse0, melee0) \
    GFVAR(flags, level, claw##name, mn, claw0, mx); \
    GFVAR(flags, level, pistol##name, mn, pistol0, mx); \
    GFVAR(flags, level, sword##name, mn, sword0, mx); \
    GFVAR(flags, level, shotgun##name, mn, shotgun0, mx); \
    GFVAR(flags, level, smg##name, mn, smg0, mx); \
    GFVAR(flags, level, flamer##name, mn, flamer0, mx); \
    GFVAR(flags, level, plasma##name, mn, plasma0, mx); \
    GFVAR(flags, level, zapper##name, mn, zapper0, mx); \
    GFVAR(flags, level, rifle##name, mn, rifle0, mx); \
    GFVAR(flags, level, corroder##name, mn, corroder0, mx); \
    GFVAR(flags, level, grenade##name, mn, grenade0, mx); \
    GFVAR(flags, level, mine##name, mn, mine0, mx); \
    GFVAR(flags, level, rocket##name, mn, rocket0, mx); \
    GFVAR(flags, level, minigun##name, mn, minigun0, mx); \
    GFVAR(flags, level, jetsaw##name, mn, jetsaw0, mx); \
    GFVAR(flags, level, eclipse##name, mn, eclipse0, mx); \
    GFVAR(flags, level, melee##name, mn, melee0, mx); \
    WD_DECLARE(float *EXPAND(WD_PREFIX, weap_stat_##name)[], \
        &EXPAND(WD_PREFIX, claw##name) NEXTARG \
        &EXPAND(WD_PREFIX, pistol##name) NEXTARG \
        &EXPAND(WD_PREFIX, sword##name) NEXTARG \
        &EXPAND(WD_PREFIX, shotgun##name) NEXTARG \
        &EXPAND(WD_PREFIX, smg##name) NEXTARG \
        &EXPAND(WD_PREFIX, flamer##name) NEXTARG \
        &EXPAND(WD_PREFIX, plasma##name) NEXTARG \
        &EXPAND(WD_PREFIX, zapper##name) NEXTARG \
        &EXPAND(WD_PREFIX, rifle##name) NEXTARG \
        &EXPAND(WD_PREFIX, corroder##name) NEXTARG \
        &EXPAND(WD_PREFIX, grenade##name) NEXTARG \
        &EXPAND(WD_PREFIX, mine##name) NEXTARG \
        &EXPAND(WD_PREFIX, rocket##name) NEXTARG \
        &EXPAND(WD_PREFIX, minigun##name) NEXTARG \
        &EXPAND(WD_PREFIX, jetsaw##name) NEXTARG \
        &EXPAND(WD_PREFIX, eclipse##name) NEXTARG \
        &EXPAND(WD_PREFIX, melee##name) \
    );

#define WPFVARM(flags, level, name, mn, mx, claw1, pistol1, sword1, shotgun1, smg1, flamer1, plasma1, zapper1, rifle1, corroder1, grenade1, mine1, rocket1, minigun1, jetsaw1, eclipse1, melee1, claw2, pistol2, sword2, shotgun2, smg2, flamer2, plasma2, zapper2, rifle2, corroder2, grenade2, mine2, rocket2, minigun2, jetsaw2, eclipse2, melee2) \
    GFVAR(flags, level, claw##name##1, mn, claw1, mx);          GFVAR(flags, level, claw##name##2, mn, claw2, mx); \
    GFVAR(flags, level, pistol##name##1, mn, pistol1, mx);      GFVAR(flags, level, pistol##name##2, mn, pistol2, mx); \
    GFVAR(flags, level, sword##name##1, mn, sword1, mx);        GFVAR(flags, level, sword##name##2, mn, sword2, mx); \
    GFVAR(flags, level, shotgun##name##1, mn, shotgun1, mx);    GFVAR(flags, level, shotgun##name##2, mn, shotgun2, mx); \
    GFVAR(flags, level, smg##name##1, mn, smg1, mx);            GFVAR(flags, level, smg##name##2, mn, smg2, mx); \
    GFVAR(flags, level, flamer##name##1, mn, flamer1, mx);      GFVAR(flags, level, flamer##name##2, mn, flamer2, mx); \
    GFVAR(flags, level, plasma##name##1, mn, plasma1, mx);      GFVAR(flags, level, plasma##name##2, mn, plasma2, mx); \
    GFVAR(flags, level, zapper##name##1, mn, zapper1, mx);      GFVAR(flags, level, zapper##name##2, mn, zapper2, mx); \
    GFVAR(flags, level, rifle##name##1, mn, rifle1, mx);        GFVAR(flags, level, rifle##name##2, mn, rifle2, mx);\
    GFVAR(flags, level, corroder##name##1, mn, corroder1, mx);  GFVAR(flags, level, corroder##name##2, mn, corroder2, mx);\
    GFVAR(flags, level, grenade##name##1, mn, grenade1, mx);    GFVAR(flags, level, grenade##name##2, mn, grenade2, mx); \
    GFVAR(flags, level, mine##name##1, mn, mine1, mx);          GFVAR(flags, level, mine##name##2, mn, mine2, mx); \
    GFVAR(flags, level, rocket##name##1, mn, rocket1, mx);      GFVAR(flags, level, rocket##name##2, mn, rocket2, mx); \
    GFVAR(flags, level, minigun##name##1, mn, minigun1, mx);    GFVAR(flags, level, minigun##name##2, mn, minigun2, mx); \
    GFVAR(flags, level, jetsaw##name##1, mn, jetsaw1, mx);      GFVAR(flags, level, jetsaw##name##2, mn, jetsaw2, mx); \
    GFVAR(flags, level, eclipse##name##1, mn, eclipse1, mx);    GFVAR(flags, level, eclipse##name##2, mn, eclipse2, mx); \
    GFVAR(flags, level, melee##name##1, mn, melee1, mx);        GFVAR(flags, level, melee##name##2, mn, melee2, mx); \
    WD_DECLARE(float *EXPAND(WD_PREFIX, weap_stat_##name)[][2], \
        { &EXPAND(WD_PREFIX, claw##name##1) NEXTARG       &EXPAND(WD_PREFIX, claw##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, pistol##name##1) NEXTARG     &EXPAND(WD_PREFIX, pistol##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, sword##name##1) NEXTARG      &EXPAND(WD_PREFIX, sword##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, shotgun##name##1) NEXTARG    &EXPAND(WD_PREFIX, shotgun##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, smg##name##1) NEXTARG        &EXPAND(WD_PREFIX, smg##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, flamer##name##1) NEXTARG     &EXPAND(WD_PREFIX, flamer##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, plasma##name##1) NEXTARG     &EXPAND(WD_PREFIX, plasma##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, zapper##name##1) NEXTARG     &EXPAND(WD_PREFIX, zapper##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, rifle##name##1) NEXTARG      &EXPAND(WD_PREFIX, rifle##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, corroder##name##1) NEXTARG   &EXPAND(WD_PREFIX, corroder##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, grenade##name##1) NEXTARG    &EXPAND(WD_PREFIX, grenade##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, mine##name##1) NEXTARG       &EXPAND(WD_PREFIX, mine##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, rocket##name##1) NEXTARG     &EXPAND(WD_PREFIX, rocket##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, minigun##name##1) NEXTARG    &EXPAND(WD_PREFIX, minigun##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, jetsaw##name##1) NEXTARG     &EXPAND(WD_PREFIX, jetsaw##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, eclipse##name##1) NEXTARG    &EXPAND(WD_PREFIX, eclipse##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, melee##name##1) NEXTARG      &EXPAND(WD_PREFIX, melee##name##2) } \
    );

#define WPSVAR(flags, level, name, claw0, pistol0, sword0, shotgun0, smg0, flamer0, plasma0, zapper0, rifle0, corroder0, grenade0, mine0, rocket0, minigun0, jetsaw0, eclipse0, melee0) \
    GSVAR(flags, level, claw##name, claw0); \
    GSVAR(flags, level, pistol##name, pistol0); \
    GSVAR(flags, level, sword##name, sword0); \
    GSVAR(flags, level, shotgun##name, shotgun0); \
    GSVAR(flags, level, smg##name, smg0); \
    GSVAR(flags, level, flamer##name, flamer0); \
    GSVAR(flags, level, plasma##name, plasma0); \
    GSVAR(flags, level, zapper##name, zapper0); \
    GSVAR(flags, level, rifle##name, rifle0); \
    GSVAR(flags, level, corroder##name, corroder0); \
    GSVAR(flags, level, grenade##name, grenade0); \
    GSVAR(flags, level, mine##name, mine0); \
    GSVAR(flags, level, rocket##name, rocket0); \
    GSVAR(flags, level, minigun##name, minigun0); \
    GSVAR(flags, level, jetsaw##name, jetsaw0); \
    GSVAR(flags, level, eclipse##name, eclipse0); \
    GSVAR(flags, level, melee##name, melee0); \
    WD_DECLARE(char **EXPAND(WD_PREFIX, weap_stat_##name)[], \
        &EXPAND(WD_PREFIX, claw##name) NEXTARG \
        &EXPAND(WD_PREFIX, pistol##name) NEXTARG \
        &EXPAND(WD_PREFIX, sword##name) NEXTARG \
        &EXPAND(WD_PREFIX, shotgun##name) NEXTARG \
        &EXPAND(WD_PREFIX, smg##name) NEXTARG \
        &EXPAND(WD_PREFIX, flamer##name) NEXTARG \
        &EXPAND(WD_PREFIX, plasma##name) NEXTARG \
        &EXPAND(WD_PREFIX, zapper##name) NEXTARG \
        &EXPAND(WD_PREFIX, rifle##name) NEXTARG \
        &EXPAND(WD_PREFIX, corroder##name) NEXTARG \
        &EXPAND(WD_PREFIX, grenade##name) NEXTARG \
        &EXPAND(WD_PREFIX, mine##name) NEXTARG \
        &EXPAND(WD_PREFIX, rocket##name) NEXTARG \
        &EXPAND(WD_PREFIX, minigun##name) NEXTARG \
        &EXPAND(WD_PREFIX, jetsaw##name) NEXTARG \
        &EXPAND(WD_PREFIX, eclipse##name) NEXTARG \
        &EXPAND(WD_PREFIX, melee##name) \
    );

#define WPSVARM(flags, level, name, claw1, pistol1, sword1, shotgun1, smg1, flamer1, plasma1, zapper1, rifle1, corroder1, grenade1, mine1, rocket1, minigun1, jetsaw1, eclipse1, melee1, claw2, pistol2, sword2, shotgun2, smg2, flamer2, plasma2, zapper2, rifle2, corroder2, grenade2, mine2, rocket2, minigun2, jetsaw2, eclipse2, melee2) \
    GSVAR(flags, level, claw##name##1, claw1);          GSVAR(flags, level, claw##name##2, claw2); \
    GSVAR(flags, level, pistol##name##1, pistol1);      GSVAR(flags, level, pistol##name##2, pistol2); \
    GSVAR(flags, level, sword##name##1, sword1);        GSVAR(flags, level, sword##name##2, sword2); \
    GSVAR(flags, level, shotgun##name##1, shotgun1);    GSVAR(flags, level, shotgun##name##2, shotgun2); \
    GSVAR(flags, level, smg##name##1, smg1);            GSVAR(flags, level, smg##name##2, smg2); \
    GSVAR(flags, level, flamer##name##1, flamer1);      GSVAR(flags, level, flamer##name##2, flamer2); \
    GSVAR(flags, level, plasma##name##1, plasma1);      GSVAR(flags, level, plasma##name##2, plasma2); \
    GSVAR(flags, level, zapper##name##1, zapper1);      GSVAR(flags, level, zapper##name##2, zapper2); \
    GSVAR(flags, level, rifle##name##1, rifle1);        GSVAR(flags, level, rifle##name##2, rifle2);\
    GSVAR(flags, level, corroder##name##1, corroder1);  GSVAR(flags, level, corroder##name##2, corroder2);\
    GSVAR(flags, level, grenade##name##1, grenade1);    GSVAR(flags, level, grenade##name##2, grenade2); \
    GSVAR(flags, level, mine##name##1, mine1);          GSVAR(flags, level, mine##name##2, mine2); \
    GSVAR(flags, level, rocket##name##1, rocket1);      GSVAR(flags, level, rocket##name##2, rocket2); \
    GSVAR(flags, level, minigun##name##1, minigun1);    GSVAR(flags, level, minigun##name##2, minigun2); \
    GSVAR(flags, level, jetsaw##name##1, jetsaw1);      GSVAR(flags, level, jetsaw##name##2, jetsaw2); \
    GSVAR(flags, level, eclipse##name##1, eclipse1);    GSVAR(flags, level, eclipse##name##2, eclipse2); \
    GSVAR(flags, level, melee##name##1, melee1);        GSVAR(flags, level, melee##name##2, melee2); \
    WD_DECLARE(char **EXPAND(WD_PREFIX, weap_stat_##name)[][2], \
        { &EXPAND(WD_PREFIX, claw##name##1) NEXTARG       &EXPAND(WD_PREFIX, claw##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, pistol##name##1) NEXTARG     &EXPAND(WD_PREFIX, pistol##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, sword##name##1) NEXTARG      &EXPAND(WD_PREFIX, sword##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, shotgun##name##1) NEXTARG    &EXPAND(WD_PREFIX, shotgun##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, smg##name##1) NEXTARG        &EXPAND(WD_PREFIX, smg##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, flamer##name##1) NEXTARG     &EXPAND(WD_PREFIX, flamer##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, plasma##name##1) NEXTARG     &EXPAND(WD_PREFIX, plasma##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, zapper##name##1) NEXTARG     &EXPAND(WD_PREFIX, zapper##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, rifle##name##1) NEXTARG      &EXPAND(WD_PREFIX, rifle##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, corroder##name##1) NEXTARG   &EXPAND(WD_PREFIX, corroder##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, grenade##name##1) NEXTARG    &EXPAND(WD_PREFIX, grenade##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, mine##name##1) NEXTARG       &EXPAND(WD_PREFIX, mine##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, rocket##name##1) NEXTARG     &EXPAND(WD_PREFIX, rocket##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, minigun##name##1) NEXTARG    &EXPAND(WD_PREFIX, minigun##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, jetsaw##name##1) NEXTARG     &EXPAND(WD_PREFIX, jetsaw##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, eclipse##name##1) NEXTARG    &EXPAND(WD_PREFIX, eclipse##name##2) } NEXTARG \
        { &EXPAND(WD_PREFIX, melee##name##1) NEXTARG      &EXPAND(WD_PREFIX, melee##name##2) } \
    );

#define WPVARK(flags, level, name, mn, mx, \
    claw1, pistol1, sword1, shotgun1, smg1, flamer1, plasma1, zapper1, rifle1, corroder1, grenade1, mine1, rocket1, minigun1, jetsaw1, eclipse1, melee1, \
    claw2, pistol2, sword2, shotgun2, smg2, flamer2, plasma2, zapper2, rifle2, corroder2, grenade2, mine2, rocket2, minigun2, jetsaw2, eclipse2, melee2, \
    claw3, pistol3, sword3, shotgun3, smg3, flamer3, plasma3, zapper3, rifle3, corroder3, grenade3, mine3, rocket3, minigun3, jetsaw3, eclipse3, melee3, \
    claw4, pistol4, sword4, shotgun4, smg4, flamer4, plasma4, zapper4, rifle4, corroder4, grenade4, mine4, rocket4, minigun4, jetsaw4, eclipse4, melee4) \
    WPVARM(flags, level, name, mn, mx, \
        claw1, pistol1, sword1, shotgun1, smg1, flamer1, plasma1, zapper1, rifle1, corroder1, grenade1, mine1, rocket1, minigun1, jetsaw1, eclipse1, melee1, \
        claw2, pistol2, sword2, shotgun2, smg2, flamer2, plasma2, zapper2, rifle2, corroder2, grenade2, mine2, rocket2, minigun2, jetsaw2, eclipse2, melee2); \
    WPVARM(flags, level, flak##name, mn, mx, \
        claw3, pistol3, sword3, shotgun3, smg3, flamer3, plasma3, zapper3, rifle3, corroder3, grenade3, mine3, rocket3, minigun3, jetsaw3, eclipse3, melee3, \
        claw4, pistol4, sword4, shotgun4, smg4, flamer4, plasma4, zapper4, rifle4, corroder4, grenade4, mine4, rocket4, minigun4, jetsaw4, eclipse4, melee4);

#define WPFVARK(flags, level, name, mn, mx, \
    claw1, pistol1, sword1, shotgun1, smg1, flamer1, plasma1, zapper1, rifle1, corroder1, grenade1, mine1, rocket1, minigun1, jetsaw1, eclipse1, melee1, \
    claw2, pistol2, sword2, shotgun2, smg2, flamer2, plasma2, zapper2, rifle2, corroder2, grenade2, mine2, rocket2, minigun2, jetsaw2, eclipse2, melee2, \
    claw3, pistol3, sword3, shotgun3, smg3, flamer3, plasma3, zapper3, rifle3, corroder3, grenade3, mine3, rocket3, minigun3, jetsaw3, eclipse3, melee3, \
    claw4, pistol4, sword4, shotgun4, smg4, flamer4, plasma4, zapper4, rifle4, corroder4, grenade4, mine4, rocket4, minigun4, jetsaw4, eclipse4, melee4) \
    WPFVARM(flags, level, name, mn, mx, \
        claw1, pistol1, sword1, shotgun1, smg1, flamer1, plasma1, zapper1, rifle1, corroder1, grenade1, mine1, rocket1, minigun1, jetsaw1, eclipse1, melee1, \
        claw2, pistol2, sword2, shotgun2, smg2, flamer2, plasma2, zapper2, rifle2, corroder2, grenade2, mine2, rocket2, minigun2, jetsaw2, eclipse2, melee2); \
    WPFVARM(flags, level, flak##name, mn, mx, \
        claw3, pistol3, sword3, shotgun3, smg3, flamer3, plasma3, zapper3, rifle3, corroder3, grenade3, mine3, rocket3, minigun3, jetsaw3, eclipse3, melee3, \
        claw4, pistol4, sword4, shotgun4, smg4, flamer4, plasma4, zapper4, rifle4, corroder4, grenade4, mine4, rocket4, minigun4, jetsaw4, eclipse4, melee4);

#define WPSVARK(flags, level, name, \
    claw1, pistol1, sword1, shotgun1, smg1, flamer1, plasma1, zapper1, rifle1, corroder1, grenade1, mine1, rocket1, minigun1, jetsaw1, eclipse1, melee1, \
    claw2, pistol2, sword2, shotgun2, smg2, flamer2, plasma2, zapper2, rifle2, corroder2, grenade2, mine2, rocket2, minigun2, jetsaw2, eclipse2, melee2, \
    claw3, pistol3, sword3, shotgun3, smg3, flamer3, plasma3, zapper3, rifle3, corroder3, grenade3, mine3, rocket3, minigun3, jetsaw3, eclipse3, melee3, \
    claw4, pistol4, sword4, shotgun4, smg4, flamer4, plasma4, zapper4, rifle4, corroder4, grenade4, mine4, rocket4, minigun4, jetsaw4, eclipse4, melee4) \
    WPSVARM(flags, level, name, \
        claw1, pistol1, sword1, shotgun1, smg1, flamer1, plasma1, zapper1, rifle1, corroder1, grenade1, mine1, rocket1, minigun1, jetsaw1, eclipse1, melee1, \
        claw2, pistol2, sword2, shotgun2, smg2, flamer2, plasma2, zapper2, rifle2, corroder2, grenade2, mine2, rocket2, minigun2, jetsaw2, eclipse2, melee2); \
    WPSVARM(flags, level, flak##name, \
        claw3, pistol3, sword3, shotgun3, smg3, flamer3, plasma3, zapper3, rifle3, corroder3, grenade3, mine3, rocket3, minigun3, jetsaw3, eclipse3, melee3, \
        claw4, pistol4, sword4, shotgun4, smg4, flamer4, plasma4, zapper4, rifle4, corroder4, grenade4, mine4, rocket4, minigun4, jetsaw4, eclipse4, melee4);

#define WPSVARR(flags, level, name, \
    claw0, pistol0, sword0, shotgun0, smg0, flamer0, plasma0, zapper0, rifle0, corroder0, grenade0, mine0, rocket0, minigun0, jetsaw0, eclipse0, melee0) \
    WPSVARM(flags, level, name, \
        claw0, pistol0, sword0, shotgun0, smg0, flamer0, plasma0, zapper0, rifle0, corroder0, grenade0, mine0, rocket0, minigun0, jetsaw0, eclipse0, melee0, \
        claw0, pistol0, sword0, shotgun0, smg0, flamer0, plasma0, zapper0, rifle0, corroder0, grenade0, mine0, rocket0, minigun0, jetsaw0, eclipse0, melee0); \
    WPSVARM(flags, level, flak##name, \
        claw0, pistol0, sword0, shotgun0, smg0, flamer0, plasma0, zapper0, rifle0, corroder0, grenade0, mine0, rocket0, minigun0, jetsaw0, eclipse0, melee0, \
        claw0, pistol0, sword0, shotgun0, smg0, flamer0, plasma0, zapper0, rifle0, corroder0, grenade0, mine0, rocket0, minigun0, jetsaw0, eclipse0, melee0);

#define W(weap,name)            (*EXPAND(WD_PREFIX, weap_stat_##name[clamp(weap, 0, int(W_MAX)-1)]))
#define W2(weap,name,second)    (*EXPAND(WD_PREFIX, weap_stat_##name[clamp(weap, 0, int(W_MAX)-1)][second ? 1 : 0]))
#define WF(c,w,v,s)             (c ? W2(w, flak##v, s) : W2(w, v, s))
#define WS(flags)               (flags&HIT_ALT)
#define WK(flags)               (flags&HIT_FLAK)
