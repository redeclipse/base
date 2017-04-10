#include "game.h"
namespace entities
{
    vector<extentity *> ents;
    int firstenttype[MAXENTTYPES], firstusetype[EU_MAX], lastenttype[MAXENTTYPES], lastusetype[EU_MAX],
        numactors = 0, lastroutenode = -1, lastroutefloor = -1, lastroutetime = 0;
    vector<int> airnodes;

    VAR(IDF_PERSIST, showlighting, 0, 0, 1);
    VAR(IDF_PERSIST, showentmodels, 0, 1, 2);
    VAR(IDF_PERSIST, showentdescs, 0, 2, 3);
    VAR(IDF_PERSIST, showentinfo, 0, 21, 127);
    VAR(IDF_PERSIST, showentattrinfo, 0, 7, 7);

    VAR(IDF_PERSIST, showentdir, 0, 1, 3); // 0 = off, 1 = only selected, 2 = always when editing, 3 = always in editmode
    VAR(IDF_PERSIST, showentradius, 0, 1, 3);
    VAR(IDF_PERSIST, showentlinks, 0, 1, 3);
    VAR(IDF_PERSIST, showentinterval, 0, 32, VAR_MAX);
    VAR(IDF_PERSIST, showentdist, 0, 512, VAR_MAX);
    FVAR(IDF_PERSIST, showentsize, 0, 3, 10);

    VAR(IDF_PERSIST, simpleitems, 0, 0, 2); // 0 = items are models, 1 = items are icons, 2 = items are off and only halos appear
    FVAR(IDF_PERSIST, simpleitemsize, 0, 6, 8);
    FVAR(IDF_PERSIST, simpleitemblend, 0, 1, 1);
    FVAR(IDF_PERSIST, simpleitemhalo, 0, 0.5f, 1);

    FVAR(IDF_PERSIST, haloitemsize, 0, 1, 8);
    FVAR(IDF_PERSIST, haloitemblend, 0, 0.75f, 1);

    VARF(0, routeid, -1, -1, VAR_MAX, lastroutenode = -1; lastroutetime = 0; airnodes.setsize(0)); // selected route in race
    VARF(0, droproute, 0, 0, 1, lastroutenode = -1; lastroutetime = 0; airnodes.setsize(0); if(routeid < 0) routeid = 0);
    VAR(IDF_HEX, routecolour, 0, 0xFF22FF, 0xFFFFFF);
    VAR(0, droproutedist, 1, 16, VAR_MAX);
    VAR(0, routemaxdist, 0, 64, VAR_MAX);

    vector<extentity *> &getents() { return ents; }
    int firstent(int type) { return type >= 0 && type < MAXENTTYPES ? clamp(firstenttype[type], 0, ents.length()-1) : 0; }
    int firstuse(int type) { return type >= 0 && type < EU_MAX ? clamp(firstusetype[type], 0, ents.length()-1) : 0; }
    int lastent(int type) { return type >= 0 && type < MAXENTTYPES ? clamp(lastenttype[type], 0, ents.length()) : 0; }
    int lastuse(int type) { return type >= 0 && type < EU_MAX ? clamp(lastusetype[type], 0, ents.length()) : 0; }

    int numattrs(int type) { return type >= 0 && type < MAXENTTYPES ? enttype[type].numattrs : 0; }
    ICOMMAND(0, entityattrs, "b", (int *n), intret(numattrs(*n)));

    ICOMMAND(0, getentinfo, "b", (int *n), {
        if(*n < 0) intret(MAXENTTYPES);
        else if(*n < MAXENTTYPES) result(enttype[*n].name);
    });

    ICOMMAND(0, getentattr, "bbb", (int *n, int *p, int *a), {
        if(*n < 0) intret(MAXENTTYPES);
        else if(*n < MAXENTTYPES)
        {
            if(*p < 0) intret(enttype[*n].numattrs);
            else if(*p < enttype[*n].numattrs)
            {
                const char *attrname = enttype[*n].attrs[*p];
                if(*n == PARTICLES) switch(*a)
                {
                    case -1: break; // not given
                    case 0: switch(*p) { case 0: attrname = "fire-plume"; break; case 1: attrname = "length"; break; case 2: attrname = "height"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "palette"; break; case 6: attrname = "palindex"; break; case 7: attrname = "size"; break; case 8: attrname = "blend"; break; case 9: attrname = "gravity"; break; case 10: attrname = "velocity"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 1: switch(*p) { case 0: attrname = "smoke-vent"; break; case 1: attrname = "dir"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 2: switch(*p) { case 0: attrname = "water-fountain"; break;case 1: attrname = "dir"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 3: switch(*p) { case 0: attrname = "fireball"; break; case 1: attrname = "size"; break; case 2: attrname = "colour"; break; case 3: attrname = "palette"; break; case 4: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 4: switch(*p) { case 0: attrname = "tape"; break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "palette"; break; case 7: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; break; } break;
                    case 5: switch(*p) { case 0: attrname = "progress"; break; case 1: attrname = "amt"; break; case 2: attrname = "colour"; break; case 3: attrname = "palette"; break; case 4: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 6: switch(*p) { case 0: attrname = "progress-vs"; break; case 1: attrname = "amt"; break; case 2: attrname = "colour"; break; case 3: attrname = "colour2"; break; case 4: attrname = "palette1"; break; case 5: attrname = "palindex1"; break; case 6: attrname = "palette2"; break; case 7: attrname = "palindex2"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 7: switch(*p) { case 0: attrname = "lightning"; break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "palette"; break; case 7: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; break; } break;
                    case 8: switch(*p) { case 0: attrname = "fire"; break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; break; } break;
                    case 9: switch(*p) { case 0: attrname = "smoke"; break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; break; } break;
                    case 10: switch(*p) {case 0: attrname = "water"; break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; break; } break;
                    case 11: switch(*p) {case 0: attrname = "plasma"; break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; break; } break;
                    case 12: switch(*p) {case 0: attrname = "snow"; break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; break; } break;
                    case 13: switch(*p) {case 0: attrname = "sparks"; break; case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; break; } break;
                    case 14: switch(*p) {case 0: attrname = "flames"; break; case 1: attrname = "radius"; break; case 2: attrname = "height"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "gravity"; break; case 7: attrname = "velocity"; break; case 8: attrname = "palette"; break; case 9: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 15: switch(*p) {case 0: attrname = "smoke-plume"; break; case 1: attrname = "radius"; break; case 2: attrname = "height"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "gravity"; break; case 7: attrname = "velocity"; break; case 8: attrname = "palette"; break; case 9: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 32: case 33: case 34: case 35: switch(*p) { case 1: attrname = "red"; break; case 2: attrname = "green"; break; case 3: attrname = "blue"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    default: attrname = ""; break;
                }
                result(attrname);
            }
        }
    });

    int triggertime(extentity &e)
    {
        switch(e.type)
        {
            case TRIGGER: case MAPMODEL: case PARTICLES: case MAPSOUND: case LIGHTFX: case TELEPORT: case PUSHER: return 1000; break;
            default: break;
        }
        return 0;
    }

    int triggertime(int n)
    {
        if(ents.inrange(n)) return triggertime(*ents[n]);
        return 0;
    }
    ICOMMAND(0, entitytriggertime, "b", (int *n), intret(triggertime(*n)));

    void getentity(int id, int val, int ex)
    {
        if(id < 0) intret(ents.length());
        else if(ents.inrange(id))
        {
            if(val < 0) intret(3);
            else switch(val)
            {
                case 0: intret(ents[id]->type); break; // type
                case 1: // attrs
                {
                    if(ex < 0) intret(ents[id]->attrs.length());
                    else if(ents[id]->attrs.inrange(ex)) intret(ents[id]->attrs[ex]);
                    break;
                }
                case 2: // links
                {
                    if(ex < 0) intret(ents[id]->links.length());
                    else if(ents[id]->links.inrange(ex)) intret(ents[id]->links[ex]);
                    break;
                }
            }
        }
    }
    ICOMMAND(0, getentity, "bbb", (int *id, int *val, int *ex), getentity(*id, *val, *ex));

    const char *entinfo(int type, attrvector &attr, bool full, bool icon)
    {
        static string entinfostr; entinfostr[0] = 0;
        #define addentinfo(s) if(*(s)) \
        { \
            if(entinfostr[0]) concatstring(entinfostr, ", "); \
            concatstring(entinfostr, s); \
        }
        #define addmodeinfo(a,b) \
        { \
            if(a) \
            { \
                int mode = a < 0 ? 0-a : a; \
                loopi(G_MAX-G_PLAY) if(mode&(1<<i)) \
                { \
                    string ds; \
                    if(a<0) formatstring(ds, "not %s", gametype[i+G_PLAY].name); \
                    else formatstring(ds, "%s", gametype[i+G_PLAY].name); \
                    addentinfo(ds); \
                } \
            } \
            if(b) \
            { \
                int muts = b < 0 ? 0-b : b; \
                loopi(G_M_NUM) if(muts&(1<<i)) \
                { \
                    string ds; \
                    if(b<0) formatstring(ds, "not %s", mutstype[i].name); \
                    else formatstring(ds, "%s", mutstype[i].name); \
                    addentinfo(ds); \
                } \
            } \
        }
        switch(type)
        {
            case PARTICLES:
            {
                switch(attr[0])
                {
                    case 0: addentinfo("fire-plume"); break;
                    case 1: addentinfo("smoke-vent"); break;
                    case 2: addentinfo("water-fountain"); break;
                    case 3: addentinfo("fireball"); break;
                    case 4: addentinfo("tape"); break;
                    case 7: addentinfo("lightning"); break;
                    case 8: addentinfo("fire"); break;
                    case 9: addentinfo("smoke"); break;
                    case 10: addentinfo("water"); break;
                    case 11: addentinfo("plasma"); break;
                    case 12: addentinfo("snow"); break;
                    case 13: addentinfo("sparks"); break;
                    case 14: addentinfo("flames"); break;
                    case 15: addentinfo("smoke-plume"); break;
                    case 6: addentinfo("progress-vs"); break;
                    case 5: addentinfo("progress"); break;
                    case 32: addentinfo("lensflare-plain"); break;
                    case 33: addentinfo("lensflare-sparkle"); break;
                    case 34: addentinfo("lensflare-sun"); break;
                    case 35: addentinfo("lensflare-sparklesun"); break;
                    default: break;
                }
                switch(attr[0])
                {
                    case 4: case 7: case 8: case 9: case 10: case 11: case 12: case 13:
                    {
                        if(attr[1] >= 256)
                        {
                            bool hasval = true;
                            int val = attr[1]-256;
                            switch(val%32)
                            {
                                case 0: case 1: case 2: addentinfo("circle"); break;
                                case 3: case 4: case 5: addentinfo("cylinder-shell"); break;
                                case 6: case 7: case 8: case 9: case 10: case 11: addentinfo("cone-shell"); break;
                                case 12: case 13: case 14: addentinfo("plane-volume"); break;
                                case 15: case 16: case 17: case 18: case 19: case 20: addentinfo("line-volume"); break;
                                case 21: case 22: case 23: hasval = false; addentinfo("sphere"); break;
                                case 24: case 25: case 26: addentinfo("plane-flat"); break;
                                default: hasval = false; addentinfo("default"); break;
                            }
                            if(hasval) switch((val%32)%3)
                            {
                                case 0: addentinfo("x-axis"); break;
                                case 1: addentinfo("y-axis"); break;
                                case 2: addentinfo("z-axis"); break;
                                default: break;
                            }
                            if(val%64 >= 32) addentinfo("inverted");
                            break;
                        }
                        // fall through
                    }
                    case 1: case 2:
                    {
                        switch(attr[1]%3)
                        {
                            case 0: addentinfo("x-axis"); break;
                            case 1: addentinfo("y-axis"); break;
                            case 2: addentinfo("z-axis"); break;
                        }
                        if(attr[1]%6 >= 3) addentinfo("inverted");
                        break;
                    }
                    default: break;
                }
                break;
            }
            case PLAYERSTART: case AFFINITY: case CHECKPOINT:
            {
                if(type != CHECKPOINT)
                {
                    if(valteam(attr[0], T_FIRST))
                    {
                        defformatstring(str, "team %s", game::colourteam(attr[0]));
                        addentinfo(str);
                    }
                }
                else
                {
                    const char *cpnames[CP_MAX+1] = { "respawn", "start", "finish", "last", "" };
                    addentinfo(cpnames[attr[6] < 0 || attr[6] >= CP_MAX ? CP_MAX : attr[6]]);
                }
                addmodeinfo(attr[3], attr[4]);
                break;
            }
            case LIGHTFX:
            {
                if(full)
                {
                    const char *lfxnames[LFX_MAX+1] = { "spotlight", "dynlight", "flicker", "pulse", "glow", "" };
                    addentinfo(lfxnames[attr[0] < 0 || attr[0] >= LFX_MAX ? LFX_MAX : attr[0]]);
                    loopi(LFX_MAX-1) if(attr[4]&(1<<(LFX_S_MAX+i))) { defformatstring(ds, "+%s", lfxnames[i+1]); addentinfo(ds); break; }
                    if(attr[4]&LFX_S_RAND1) addentinfo("rnd-min");
                    if(attr[4]&LFX_S_RAND2) addentinfo("rnd-max");
                }
                break;
            }
            case ACTOR:
            {
                if(full && attr[0] >= 0 && attr[0] < A_TOTAL)
                {
                    addentinfo(actor[attr[0]+A_ENEMY].name);
                    addmodeinfo(attr[3], attr[4]);
                    addentinfo(W(attr[6] > 0 && attr[6] <= W_ALL ? attr[6]-1 : AA(attr[0]+A_ENEMY, weaponspawn), name));
                }
                break;
            }
            case WEAPON:
            {
                int sweap = m_weapon(game::focus->actortype, game::gamemode, game::mutators), attr1 = w_attr(game::gamemode, game::mutators, type, attr[0], sweap);
                if(isweap(attr1))
                {
                    defformatstring(str, "\fs\f[%d]%s%s%s%s\fS", W(attr1, colour), icon ? "\f(" : "", icon ? hud::itemtex(type, attr1) : W(attr1, name), icon ? ")" : "", icon ? W(attr1, longname) : "");
                    addentinfo(str);
                    if(full)
                    {
                        addmodeinfo(attr[2], attr[3]);
                        if(attr[1]&W_F_FORCED) addentinfo("forced");
                    }
                }
                break;
            }
            case MAPMODEL:
            {
                mapmodelinfo *mmi = getmminfo(attr[0]);
                if(!mmi) break;
                addentinfo(mmi->name);
                if(full)
                {
                    if(attr[6]&MMT_HIDE) addentinfo("hide");
                    if(attr[6]&MMT_NOCLIP) addentinfo("no-clip");
                    if(attr[6]&MMT_NOSHADOW) addentinfo("no-shadow");
                    if(attr[6]&MMT_NODYNSHADOW) addentinfo("no-dynshadow");
                }
                break;
            }
            case MAPSOUND:
            {
                if(mapsounds.inrange(attr[0]))
                {
                    int samples = mapsounds[attr[0]].samples.length();
                    defformatstring(ds, "%s (%d %s)", mapsounds[attr[0]].name, samples, samples == 1 ? "sample" : "samples");
                    addentinfo(ds);
                }
                if(full)
                {
                    if(attr[4]&SND_NOATTEN) addentinfo("no-atten");
                    if(attr[4]&SND_NODELAY) addentinfo("no-delay");
                    if(attr[4]&SND_NOCULL) addentinfo("no-cull");
                    if(attr[4]&SND_NOPAN) addentinfo("no-pan");
                    if(attr[4]&SND_NODIST) addentinfo("no-dist");
                    if(attr[4]&SND_NOQUIET) addentinfo("no-quiet");
                    if(attr[4]&SND_CLAMPED) addentinfo("clamped");
                }
                break;
            }
            case TRIGGER:
            {
                if(full)
                {
                    const char *trgnames[TR_MAX+1] = { "toggle", "link", "script", "once", "exit", "" }, *actnames[TA_MAX+1] = { "manual", "proximity", "action", "" };
                    addentinfo(trgnames[attr[1] < 0 || attr[1] >= TR_MAX ? TR_MAX : attr[1]]);
                    addentinfo(actnames[attr[2] < 0 || attr[2] >= TA_MAX ? TA_MAX : attr[2]]);
                    if(attr[4] >= 2) addentinfo(attr[4] ? "routed" : "inert");
                    addentinfo(attr[4]%2 ? "on" : "off");
                    addmodeinfo(attr[5], attr[6]);
                }
                break;
            }
            case PUSHER:
            {
                if(full) switch(attr[5])
                {
                    case 0: addentinfo("conditional-dir"); break;
                    case 1: addentinfo("add-to-dir"); break;
                    case 2: addentinfo("redirect-dir"); break;
                    case 3: addentinfo("absolute-dir"); break;
                    default: break;
                }
                break;
            }
            case TELEPORT:
            {
                if(full)
                {
                    if(attr[5] >= 3) addentinfo("pos-offset");
                    if(attr[5] >= 6) addentinfo("mod-velocity");
                    switch(attr[5]%3)
                    {
                        case 0: addentinfo("absolute-dir"); break;
                        case 1: addentinfo("relative-dir"); break;
                        case 2: addentinfo("keep-dir"); break;
                        default: break;
                    }
                    const char *telenames[TELE_MAX] = { "no-affinity" };
                    loopj(TELE_MAX) if(attr[8]&(1<<j)) { addentinfo(telenames[j]); }
                }
                break;
            }
            default: break;
        }
        return entinfostr[0] ? entinfostr : "";
    }

    const char *entmdlname(int type, attrvector &attr)
    {
        switch(type)
        {
            case AFFINITY: return "props/flag";
            case PLAYERSTART: return playertypes[0][1];
            case WEAPON:
            {
                int weap = w_attr(game::gamemode, game::mutators, type, attr[0], m_weapon(game::focus->actortype, game::gamemode, game::mutators));
                return isweap(weap) && *weaptype[weap].item ? weaptype[weap].item : "projectiles/cartridge";
            }
            case ACTOR: return actor[clamp(attr[0]+A_ENEMY, int(A_ENEMY), int(A_MAX-1))].playermodel[1];
            default: break;
        }
        return "";
    }

    void checkspawns(int n)
    {
        gameentity &e = *(gameentity *)ents[n];
        if(enttype[e.type].usetype == EU_ITEM) loopv(projs::projs)
        {
            projent &proj = *projs::projs[i];
            if(proj.projtype != PRJ_ENT || proj.id != n) continue;
            proj.beenused = 2;
            proj.lifetime = min(proj.lifetime, proj.fadetime);
        }
    }

    void useeffects(gameent *d, int ent, int ammoamt, bool spawn, int weap, int drop, int ammo)
    {
        gameentity &e = *(gameentity *)ents[ent];
        int sweap = m_weapon(d->actortype, game::gamemode, game::mutators), attr = w_attr(game::gamemode, game::mutators, e.type, e.attrs[0], sweap),
            colour = e.type == WEAPON && isweap(attr) ? W(attr, colour) : colourwhite;
        if(e.type == WEAPON && isweap(attr)) d->addicon(eventicon::WEAPON, lastmillis, game::eventiconshort, attr);
        if(isweap(weap))
        {
            d->setweapstate(weap, W_S_SWITCH, weaponswitchdelay, lastmillis);
            d->ammo[weap] = -1;
            if(d->weapselect != weap && weap < W_ALL)
            {
                d->addlastweap(d->weapselect);
                d->weapselect = weap;
            }
        }
        d->useitem(ent, e.type, attr, ammoamt, sweap, lastmillis, weaponswitchdelay);
        playsound(e.type == WEAPON && attr >= W_OFFSET && attr < W_ALL ? WSND(attr, S_W_USE) : S_ITEMUSE, d->o, d, 0, -1, -1, -1, &d->wschan);
        if(game::dynlighteffects) adddynlight(d->center(), enttype[e.type].radius*2, vec::hexcolor(colour).mul(2.f), 250, 250);
        if(ents.inrange(drop) && ents[drop]->type == WEAPON)
        {
            gameentity &f = *(gameentity *)ents[drop];
            attr = w_attr(game::gamemode, game::mutators, f.type, f.attrs[0], sweap);
            if(isweap(attr)) projs::drop(d, attr, drop, ammo, d == game::player1 || d->ai, 0, weap);
        }
        if(e.spawned() != spawn)
        {
            e.setspawned(spawn);
            e.lastemit = lastmillis;
        }
        checkspawns(ent);
    }

    /*
    static inline void collateents(octaentities &oe, const vec &pos, float xyrad, float zrad, bool alive, vector<actitem> &actitems)
    {
        vector<extentity *> &ents = entities::getents();
        loopv(oe.other)
        {
            int n = oe.other[i];
            extentity &e = *ents[n];
            if(enttype[e.type].usetype != EU_NONE && (enttype[e.type].usetype != EU_ITEM || (alive && e.spawned())))
            {
                float radius = enttype[e.type].radius;
                switch(e.type)
                {
                    case TRIGGER: case TELEPORT: case PUSHER: if(e.attrs[3] > 0) radius = e.attrs[3]; break;
                    case CHECKPOINT: if(e.attrs[0] > 0) radius = e.attrs[0]; break;
                }
                if(overlapsbox(pos, zrad, xyrad, e.o, radius, radius))
                {
                    actitem &t = actitems.add();
                    t.type = actitem::ENT;
                    t.target = n;
                    t.score = pos.squaredist(e.o);
                }
            }
        }
    }

    static inline void collateents(cube *c, const ivec &o, int size, const ivec &bo, const ivec &br, const vec &pos, float xyrad, float zrad, bool alive, vector<actitem> &actitems)
    {
        loopoctabox(o, size, bo, br)
        {
            if(c[i].ext && c[i].ext->ents) collateents(*c[i].ext->ents, pos, xyrad, zrad, alive, actitems);
            if(c[i].children && size > octaentsize)
            {
                ivec co(i, o.x, o.y, o.z, size);
                collateents(c[i].children, co, size>>1, bo, br, pos, xyrad, zrad, alive, actitems);
            }
        }
    }

    void collateents(const vec &pos, float xyrad, float zrad, bool alive, vector<actitem> &actitems)
    {
        ivec bo = vec(pos).sub(vec(xyrad, xyrad, zrad)),
             br = vec(pos).add(vec(xyrad, xyrad, zrad)).add(1);
        int diff = (bo.x^br.x) | (bo.y^br.y) | (bo.z^br.z) | octaentsize,
            scale = worldscale-1;
        if(diff&~((1<<scale)-1) || uint(bo.x|bo.y|bo.z|br.x|br.y|br.z) >= uint(hdr.worldsize))
        {
            collateents(worldroot, ivec(0, 0, 0), 1<<scale, bo, br, pos, xyrad, zrad, alive, actitems);
            return;
        }
        cube *c = &worldroot[octastep(bo.x, bo.y, bo.z, scale)];
        if(c->ext && c->ext->ents) collateents(*c->ext->ents, pos, xyrad, zrad, alive, actitems);
        scale--;
        while(c->children && !(diff&(1<<scale)))
        {
            c = &c->children[octastep(bo.x, bo.y, bo.z, scale)];
            if(c->ext && c->ext->ents) collateents(*c->ext->ents, pos, xyrad, zrad, alive, actitems);
            scale--;
        }
        if(c->children && 1<<scale >= octaentsize) collateents(c->children, ivec(bo).mask(~((2<<scale)-1)), 1<<scale, bo, br, pos, xyrad, zrad, alive, actitems);
    }
    */

    static inline bool sortitems(const actitem &a, const actitem &b)
    {
        return a.score > b.score;
    }

    bool collateitems(dynent *d, vec &pos, float radius, vector<actitem> &actitems)
    {
        loopv(ents)
        {
            extentity &e = *ents[i];
            if(enttype[e.type].usetype != EU_NONE && (enttype[e.type].usetype != EU_ITEM || (d->state == CS_ALIVE && e.spawned())))
            {
                float eradius = enttype[e.type].radius, edist = pos.dist(e.o);
                switch(e.type)
                {
                    case TRIGGER: case TELEPORT: case PUSHER: if(e.attrs[3] > 0) eradius = e.attrs[3]; break;
                    case CHECKPOINT: if(e.attrs[0] > 0) eradius = e.attrs[0]; break;
                }
                float diff = edist-radius;
                if(diff > eradius) continue;
                actitem &t = actitems.add();
                t.type = actitem::ENT;
                t.target = i;
                t.score = diff;
            }
        }
        if(d->state == CS_ALIVE) loopv(projs::projs)
        {
            projent &proj = *projs::projs[i];
            if(proj.projtype != PRJ_ENT || !proj.ready()) continue;
            if(!ents.inrange(proj.id) || enttype[ents[proj.id]->type].usetype != EU_ITEM) continue;
            if(!(enttype[ents[proj.id]->type].canuse&(1<<d->type))) continue;
            //if(!overlapsbox(m, eye, d->radius, proj.o, enttype[ents[proj.id]->type].radius, enttype[ents[proj.id]->type].radius))
            //    continue;
            float eradius = enttype[ents[proj.id]->type].radius, edist = pos.dist(proj.o);
            switch(ents[proj.id]->type)
            {
                case TRIGGER: case TELEPORT: case PUSHER: if(ents[proj.id]->attrs[3] > 0) eradius = ents[proj.id]->attrs[3]; break;
                case CHECKPOINT: if(ents[proj.id]->attrs[0] > 0) eradius = ents[proj.id]->attrs[0]; break;
            }
            float diff = edist-radius;
            if(diff > eradius) continue;
            actitem &t = actitems.add();
            t.type = actitem::PROJ;
            t.target = i;
            t.score = diff;
        }
        if(!actitems.empty())
        {
            actitems.sort(sortitems); // sort items so last is closest
            return true;
        }
        return false;
    }

    gameent *trigger = NULL;
    ICOMMAND(0, triggerclientnum, "", (), intret(trigger ? trigger->clientnum : -1));

    bool cantrigger(int n, gameent *d = NULL)
    {
        gameentity &e = *(gameentity *)ents[n];
        switch(e.type)
        {
            case TRIGGER:
            {
                if(!m_check(e.attrs[5], e.attrs[6], game::gamemode, game::mutators)) return false;
                if(d)
                {
                    int millis = d->lastused(n, true);
                    if(millis && lastmillis-millis < triggertime(e)/2) return false;
                }
                return true;
                break;
            }
            default: return true; break;
        }
        return false;
    }

    void runtrigger(int n, gameent *d, bool act = true)
    {
        gameentity &e = *(gameentity *)ents[n];
        if(cantrigger(n, d))
        {
            e.lastemit = lastmillis;
            d->setused(n, lastmillis);
            switch(e.attrs[1])
            {
                case TR_EXIT: if(d->actortype >= A_BOT) break;
                case TR_TOGGLE: case TR_LINK: case TR_ONCE:
                {
                    client::addmsg(N_TRIGGER, "ri2", d->clientnum, n);
                    if(!e.spawned() || e.attrs[1] == TR_TOGGLE) setspawn(n, e.spawned() ? 0 : 1);
                    break;
                }
                case TR_SCRIPT:
                {
                    if(d == game::player1)
                    {
                        defformatstring(s, "on_trigger_%d", e.attrs[0]);
                        trigger = d; RUNWORLD(s); trigger = NULL;
                    }
                    break;
                }
                default: break;
            }
            if(act && e.attrs[2] == TA_ACTION) d->action[AC_USE] = false;
        }
    }

    void runtriggers(int n, gameent *d)
    {
        loopenti(TRIGGER) if(ents[i]->type == TRIGGER && ents[i]->attrs[0] == n && ents[i]->attrs[2] == TA_MANUAL) runtrigger(i, d, false);
    }
    ICOMMAND(0, exectrigger, "i", (int *n), if(identflags&IDF_WORLD) runtriggers(*n, trigger ? trigger : game::player1));

    bool execitem(int n, dynent *d, vec &pos, float dist)
    {
        gameentity &e = *(gameentity *)ents[n];
        switch(enttype[e.type].usetype)
        {
            case EU_ITEM:
            {
                if(gameent::is(d) && (e.type != WEAPON || ((gameent *)d)->action[AC_USE]))
                {
                    gameent *f = (gameent *)d;
                    if(game::allowmove(f))
                    {
                        int sweap = m_weapon(f->actortype, game::gamemode, game::mutators), attr = w_attr(game::gamemode, game::mutators, e.type, e.attrs[0], sweap);
                        if(!isweap(attr)) return false;
                        if(f == game::player1 && !weapons::canuse(attr)) return true;
                        if(!f->canuse(e.type, attr, e.attrs, sweap, lastmillis, (1<<W_S_SWITCH)))
                        {
                            if(e.type != WEAPON) return false;
                            else if(!f->canuse(e.type, attr, e.attrs, sweap, lastmillis, (1<<W_S_SWITCH)|(1<<W_S_RELOAD))) return true;
                            else if(!isweap(f->weapselect) || f->weapload[f->weapselect] <= 0) return true;
                            else
                            {
                                int offset = f->weapload[f->weapselect];
                                f->ammo[f->weapselect] = max(f->ammo[f->weapselect]-offset, 0);
                                f->weapload[f->weapselect] = -f->weapload[f->weapselect];
                            }
                        }
                        client::addmsg(N_ITEMUSE, "ri3", f->clientnum, lastmillis-game::maptime, n);
                        if(e.type == WEAPON)
                        {
                            f->setweapstate(f->weapselect, W_S_WAIT, weaponswitchdelay, lastmillis);
                            f->action[AC_USE] = false;
                        }
                        return false;
                    }
                    return true;
                }
                break;
            }
            case EU_AUTO:
            {
                if(e.type == TELEPORT)
                {
                    if(e.attrs[8]&(1<<TELE_NOAFFIN))
                    {
                        if(gameent::is(d) && physics::carryaffinity((gameent *)d)) break;
                        if(projent::is(d) && ((projent *)d)->type == PRJ_AFFINITY) break;
                    }
                    int millis = d->lastused(n, true);
                    if(millis && lastmillis-millis < triggertime(e)) break;
                    e.lastemit = lastmillis;
                    static vector<int> teleports;
                    teleports.shrink(0);
                    loopv(e.links)
                        if(e.links[i] != n && ents.inrange(e.links[i]) && ents[e.links[i]]->type == e.type)
                            teleports.add(e.links[i]);
                    if(teleports.empty()) break;
                    vec orig = d->o, ovel = d->vel;
                    float oyaw = d->yaw, opitch = d->pitch;
                    while(!teleports.empty())
                    {
                        int r = rnd(teleports.length()), q = teleports[r];
                        gameentity &f = *(gameentity *)ents[q];
                        d->o = vec(f.o).add(f.attrs[5] >= 3 ? vec(orig).sub(e.o) : vec(0, 0, d->height*0.5f));
                        float mag = f.attrs[2] < 0 ? 0.f : max(vec(d->vel).add(d->falling).magnitude(), f.attrs[2] ? float(f.attrs[2]) : 50.f),
                              yaw = f.attrs[0] < 0 ? (lastmillis/5)%360 : f.attrs[0], pitch = f.attrs[1];
                        game::fixrange(yaw, pitch);
                        if(mag > 0 && f.attrs[5] < 6) d->vel = vec(yaw*RAD, pitch*RAD).mul(mag);
                        switch(f.attrs[5]%3)
                        {
                            case 2: break; // keep
                            case 1: // relative
                            {
                                float relyaw = (e.attrs[0] < 0 ? (lastmillis/5)%360 : e.attrs[0])-180, relpitch = e.attrs[1];
                                game::fixrange(relyaw, relpitch);
                                d->yaw = yaw+(d->yaw-relyaw);
                                d->pitch = pitch+(d->pitch-relpitch);
                                break;
                            }
                            case 0: default: // absolute
                            {
                                d->yaw = yaw;
                                d->pitch = pitch;
                                break;
                            }
                        }
                        game::fixrange(d->yaw, d->pitch);
                        if(mag <= 0) d->vel = vec(0, 0, 0);
                        else if(f.attrs[5] >= 6) d->vel = vec(d->yaw*RAD, d->pitch*RAD).mul(mag);
                        if(physics::entinmap(d, gameent::is(d))) // entinmap first for getting position
                        {
                            f.lastemit = lastmillis;
                            d->setused(n, lastmillis);
                            d->setused(q, lastmillis);
                            d->resetinterp(true);
                            if(d->state == CS_ALIVE)
                            {
                                if(gameent::is(d))
                                {
                                    gameent *g = (gameent *)d;
                                    if(g == game::focus) game::resetcamera();
                                    execlink(g, n, true);
                                    execlink(g, q, true);
                                    g->resetair();
                                    ai::inferwaypoints(g, e.o, f.o, float(e.attrs[3] ? e.attrs[3] : enttype[e.type].radius)+ai::CLOSEDIST);
                                }
                                else if(projent::is(d))
                                {
                                    projent *g = (projent *)d;
                                    g->lastbounce = lastmillis;
                                    g->movement = 0;
                                    g->from = g->deltapos = g->o;
                                    g->to = g->dest = vec(g->o).add(g->vel);
                                }
                            }
                            else if(gameent::is(d)) warpragdoll(d, d->vel, vec(f.o).sub(e.o));
                            return false;
                        }
                        d->o = orig;
                        d->vel = ovel;
                        d->yaw = oyaw;
                        d->pitch = opitch;
                        teleports.remove(r); // must've really sucked, try another one
                    }
                    if(d->state == CS_ALIVE)
                    {
                        if(gameent::is(d)) game::suicide((gameent *)d, HIT_SPAWN);
                        else if(projent::is(d))
                        {
                            projent *g = (projent *)d;
                            switch(g->projtype)
                            {
                                case PRJ_ENT: case PRJ_AFFINITY:
                                {
                                    if(!g->beenused)
                                    {
                                        g->beenused = 1;
                                        g->lifetime = min(g->lifetime, g->fadetime);
                                    }
                                    if(g->lifetime > 0) break;
                                }
                                default: g->state = CS_DEAD; g->escaped = true; break;
                            }
                        }
                        else d->state = CS_DEAD;
                    }
                }
                else if(e.type == PUSHER)
                {
                    int millis = d->lastused(n, true);
                    if(millis && lastmillis-millis < triggertime(e)) break;
                    e.lastemit = lastmillis;
                    d->setused(n, lastmillis);
                    float mag = max(e.attrs[2], 1), maxrad = e.attrs[3] ? e.attrs[3] : enttype[PUSHER].radius, minrad = e.attrs[4];
                    if(dist > 0 && minrad > 0 && maxrad > minrad && dist > minrad && maxrad >= dist)
                        mag *= 1.f-clamp((dist-minrad)/float(maxrad-minrad), 0.f, 1.f);
                    vec dir(e.attrs[0]*RAD, e.attrs[1]*RAD), rel = vec(dir).mul(mag);
                    switch(e.attrs[5])
                    {
                        case 0:
                        {
                            loopk(3)
                            {
                                if((d->vel.v[k] > 1e-1f && rel.v[k] < -1e-1f) || (d->vel.v[k] < -1e-1f && rel.v[k] > 1e-1f) || (fabs(rel.v[k]) > fabs(d->vel.v[k])))
                                    d->vel.v[k] = rel.v[k];
                            }
                            break;
                        }
                        case 1: d->vel.add(rel); break;
                        case 2: rel.add(vec(dir).mul(vec(d->vel).add(d->falling).magnitude())); // fall through
                        case 3: d->vel = rel; break;
                        default: break;
                    }
                    if(d->state == CS_ALIVE)
                    {
                        if(gameent::is(d))
                        {
                            gameent *g = (gameent *)d;
                            execlink(g, n, true);
                            g->resetair();
                        }
                        else if(projent::is(d))
                        {
                            projent *g = (projent *)d;
                            g->lastbounce = lastmillis;
                            g->movement = 0;
                            g->from = g->deltapos = g->o;
                            g->to = g->dest = vec(g->o).add(g->vel);
                        }
                    }
                    else if(gameent::is(d)) warpragdoll(d, d->vel);
                }
                else if(e.type == TRIGGER)
                {
                    if(d->state != CS_ALIVE || !gameent::is(d)) break;
                    gameent *g = (gameent *)d;
                    if((e.attrs[2] == TA_ACTION && g->action[AC_USE] && g == game::player1) || e.attrs[2] == TA_AUTO) runtrigger(n, g);
                }
                else if(e.type == CHECKPOINT)
                {
                    if(d->state != CS_ALIVE || !gameent::is(d) || !m_race(game::gamemode)) break;
                    if(!m_check(e.attrs[3], e.attrs[4], game::gamemode, game::mutators)) break;
                    gameent *g = (gameent *)d;
                    if(g->checkpoint == n || (m_ra_gauntlet(game::gamemode, game::mutators) && g->team != T_ALPHA)) break;
                    if(e.attrs[6] == CP_START)
                    {
                        if(g->cpmillis || (d->vel.iszero() && !d->move && !d->strafe)) break;
                        g->cpmillis = lastmillis;
                    }
                    else if(!g->cpmillis) break;
                    client::addmsg(N_TRIGGER, "ri2", g->clientnum, n);
                    g->checkpoint = n;
                }
                break;
            }
        }
        return false;
    }

    void checkitems(dynent *d)
    {
        static vector<actitem> actitems;
        actitems.setsize(0);
        vec pos = d->center();
        float radius = max(d->xradius, d->yradius);
        if(gameent::is(d)) radius = max(d->height*0.5f, radius);
        if(collateitems(d, pos, radius, actitems))
        {
            bool tried = false;
            while(!actitems.empty())
            {
                actitem &t = actitems.last();
                int ent = -1;
                float dist = 0;
                switch(t.type)
                {
                    case actitem::ENT:
                    {
                        if(!ents.inrange(t.target)) break;
                        ent = t.target;
                        dist = t.score;
                        break;
                    }
                    case actitem::PROJ:
                    {
                        if(!projs::projs.inrange(t.target)) break;
                        projent &proj = *projs::projs[t.target];
                        ent = proj.id;
                        dist = t.score;
                        break;
                    }
                    default: break;
                }
                if(ents.inrange(ent) && execitem(ent, d, pos, dist)) tried = true;
                actitems.pop();
            }
            if(tried && gameent::is(d))
            {
                gameent *e = (gameent *)d;
                if(e->action[AC_USE])
                {
                    game::errorsnd(e);
                    e->action[AC_USE] = false;
                }
            }
        }
    }

    void putitems(packetbuf &p)
    {
        loopv(ents)
        {
            if(i >= MAXENTS) break;
            if(enttype[ents[i]->type].syncs)
            {
                gameentity &e = *(gameentity *)ents[i];
                putint(p, i);
                putint(p, int(e.type));
                putint(p, min(e.attrs.length(), MAXENTATTRS));
                loopvj(e.attrs)
                {
                    if(j >= MAXENTATTRS) break;
                    putint(p, e.attrs[j]);
                }
                if(enttype[e.type].syncpos) loopj(3) putint(p, int(e.o[j]*DMF));
                if(enttype[e.type].synckin)
                {
                    putint(p, min(e.kin.length(), MAXENTKIN));
                    loopvj(e.kin)
                    {
                        if(j >= MAXENTKIN) break;
                        putint(p, e.kin[j]);
                    }
                }
            }
        }
    }

    void setspawn(int n, int m)
    {
        if(ents.inrange(n))
        {
            gameentity &e = *(gameentity *)ents[n];
            bool on = m%2, spawned = e.spawned();
            e.setspawned(on);
            if(on) e.lastspawn = lastmillis;
            if(e.type == TRIGGER && cantrigger(n) && (e.attrs[1] == TR_TOGGLE || e.attrs[1] == TR_LINK || e.attrs[1] == TR_ONCE))
            {
                if(m >= 2 || e.lastemit <= 0 || e.spawned() != spawned)
                {
                    if(m >= 2) e.lastemit = -1;
                    else if(e.lastemit > 0)
                    {
                        int last = lastmillis-e.lastemit, trig = triggertime(e);
                        if(last > 0 && last < trig) e.lastemit = lastmillis-(trig-last);
                        else e.lastemit = lastmillis;
                    }
                    else e.lastemit = lastmillis;
                    execlink(NULL, n, false);
                    loopv(e.kin) if(ents.inrange(e.kin[i]))
                    {
                        gameentity &f = *(gameentity *)ents[e.kin[i]];
                        if(!cantrigger(e.kin[i])) continue;
                        f.setspawned(e.spawned());
                        f.lastemit = e.lastemit;
                        execlink(NULL, e.kin[i], false, n);
                    }
                }
            }
            checkspawns(n);
        }
    }

    extentity *newent() { return new gameentity; }
    void deleteent(extentity *e) { delete (gameentity *)e; }

    void clearents()
    {
        while(ents.length()) deleteent(ents.pop());
        memset(firstenttype, 0, sizeof(firstenttype));
        memset(firstusetype, 0, sizeof(firstusetype));
        memset(lastenttype, 0, sizeof(lastenttype));
        memset(lastusetype, 0, sizeof(lastusetype));
    }

    bool cansee(int n)
    {
        if(game::player1->state != CS_EDITING && !(showentinfo&64)) return false;
        if(!ents.inrange(n)) return false;
        if(ents[n]->type == NOTUSED && (n != enthover && entgroup.find(n) < 0)) return false;
        return true;
    }

    void fixentity(int n, bool recurse, bool create)
    {
        gameentity &e = *(gameentity *)ents[n];
        int num = max(5, enttype[e.type].numattrs);
        if(e.attrs.length() < num) e.attrs.add(0, num - e.attrs.length());
        else if(e.attrs.length() > num) e.attrs.setsize(num);
        loopvrev(e.links)
        {
            int ent = e.links[i];
            if(!canlink(n, ent, verbose >= 2)) e.links.remove(i);
            else if(ents.inrange(ent))
            {
                gameentity &f = *(gameentity *)ents[ent];
                if(((enttype[e.type].reclink&(1<<f.type)) || (enttype[f.type].reclink&(1<<e.type))) && f.links.find(n) < 0)
                {
                    f.links.add(n);
                    if(verbose) conoutf("\frWARNING: automatic reciprocal link between %d and %d added", n, ent);
                }
                else continue;
                if(recurse || ent < n) fixentity(ent, false);
            }
            else continue;
        }
        if(issound(e.schan))
        {
            removesound(e.schan); e.schan = -1; // prevent clipping when moving around
            if(e.type == MAPSOUND) e.lastemit = lastmillis+1000;
        }
        switch(e.type)
        {
            case MAPMODEL:
                while(e.attrs[1] < 0) e.attrs[1] += 360;
                while(e.attrs[1] >= 360) e.attrs[1] -= 360; // yaw
                while(e.attrs[2] < -180) e.attrs[2] += 360;
                while(e.attrs[2] > 180) e.attrs[2] -= 360; // pitch
                while(e.attrs[3] < -180) e.attrs[3] += 360;
                while(e.attrs[3] > 180) e.attrs[3] -= 360; // roll
                while(e.attrs[4] < 0) e.attrs[4] += 101;
                while(e.attrs[4] > 100) e.attrs[4] -= 101; // blend
                if(e.attrs[5] < 0) e.attrs[5] = 0;
            case PARTICLES:
            case MAPSOUND:
            case LIGHTFX:
            {
                e.nextemit = 0;
                loopv(e.links) if(ents.inrange(e.links[i]))
                {
                    gameentity &f = *(gameentity *)ents[e.links[i]];
                    if(f.type != TRIGGER || !cantrigger(e.links[i])) continue;
                    e.lastemit = f.lastemit;
                    e.setspawned(TRIGSTATE(f.spawned(), f.attrs[4]));
                    break;
                }
                break;
            }
            case LIGHT:
            {
                if(e.attrs[0] < 0) e.attrs[0] = 0;
                //if(e.attrs[1] < 0) e.attrs[1] = 0;
                //if(e.attrs[2] < 0) e.attrs[2] = 0;
                //if(e.attrs[3] < 0) e.attrs[3] = 0;
                if(e.attrs[4] < 0) e.attrs[4] = 3;
                if(e.attrs[4] > 3) e.attrs[4] = 0;
                break;
            }
            case SUNLIGHT:
            {
                while(e.attrs[0] < 0) e.attrs[0] += 360;
                while(e.attrs[0] >= 360) e.attrs[0] -= 360;
                while(e.attrs[1] < -180) e.attrs[1] += 360;
                while(e.attrs[1] >= 180) e.attrs[1] -= 360;
                if(e.attrs[5] < -1) e.attrs[5] = 0;
                if(e.attrs[6] < 0) e.attrs[6] = 3;
                if(e.attrs[6] > 3) e.attrs[6] = 0;
                break;
            }
            case PUSHER:
            {
                while(e.attrs[0] < 0) e.attrs[0] += 360;
                while(e.attrs[0] >= 360) e.attrs[0] -= 360;
                while(e.attrs[1] < -90) e.attrs[1] += 181;
                while(e.attrs[1] > 90) e.attrs[1] -= 181;
                if(e.attrs[2] < 1) e.attrs[2] = 1;
                if(e.attrs[3] < 0) e.attrs[3] = 0;
                if(e.attrs[4] < 0 || e.attrs[4] >= (e.attrs[3] ? e.attrs[3] : enttype[PUSHER].radius)) e.attrs[4] = 0;
                if(e.attrs[5] < 0) e.attrs[5] += 4;
                if(e.attrs[5] >= 4) e.attrs[5] -= 4;
                break;
            }
            case TRIGGER:
            {
                while(e.attrs[1] < 0) e.attrs[1] += TR_MAX;
                while(e.attrs[1] >= TR_MAX) e.attrs[1] -= TR_MAX;
                while(e.attrs[2] < 0) e.attrs[2] += TA_MAX;
                while(e.attrs[2] >= TA_MAX) e.attrs[2] -= TA_MAX;
                while(e.attrs[4] < 0) e.attrs[4] += 4;
                while(e.attrs[4] >= 4) e.attrs[4] -= 4;
                if(cantrigger(n)) loopv(e.links) if(ents.inrange(e.links[i]) && (ents[e.links[i]]->type == MAPMODEL || ents[e.links[i]]->type == PARTICLES || ents[e.links[i]]->type == MAPSOUND || ents[e.links[i]]->type == LIGHTFX))
                {
                    ents[e.links[i]]->lastemit = e.lastemit;
                    ents[e.links[i]]->setspawned(TRIGSTATE(e.spawned(), e.attrs[4]));
                }
                break;
            }
            case WEAPON:
                if(create && (e.attrs[0] < W_OFFSET || e.attrs[0] >= W_ALL)) e.attrs[0] = W_OFFSET; // don't be stupid when creating the entity
                while(e.attrs[0] < W_OFFSET) e.attrs[0] += W_ALL-W_OFFSET; // don't allow superimposed weaps
                while(e.attrs[0] >= W_ALL) e.attrs[0] -= W_ALL-W_OFFSET;
                break;
            case PLAYERSTART:
                while(e.attrs[0] < 0) e.attrs[0] += T_ALL;
                while(e.attrs[0] >= T_ALL) e.attrs[0] -= T_ALL;
            case CHECKPOINT:
                while(e.attrs[1] < 0) e.attrs[1] += 360;
                while(e.attrs[1] >= 360) e.attrs[1] -= 360;
                while(e.attrs[2] < -90) e.attrs[2] += 180;
                while(e.attrs[2] > 90) e.attrs[2] -= 180;
                if(e.type == CHECKPOINT)
                {
                    while(e.attrs[6] < 0) e.attrs[6] += CP_MAX;
                    while(e.attrs[6] >= CP_MAX) e.attrs[6] -= CP_MAX;
                }
                break;
            case ACTOR:
                while(e.attrs[0] < 0) e.attrs[0] += A_TOTAL;
                while(e.attrs[0] >= A_TOTAL) e.attrs[0] -= A_TOTAL;
                while(e.attrs[1] < 0) e.attrs[1] += 360;
                while(e.attrs[1] >= 360) e.attrs[1] -= 360;
                while(e.attrs[2] < -90) e.attrs[2] += 180;
                while(e.attrs[2] > 90) e.attrs[2] -= 180;
                while(e.attrs[6] < 0) e.attrs[6] += W_ALL+1; // allow any weapon
                while(e.attrs[6] > W_ALL) e.attrs[6] -= W_ALL+1;
                if(e.attrs[7] < 0) e.attrs[7] = 0;
                if(e.attrs[8] < 0) e.attrs[8] = 0;
                if(e.attrs[9] < 0) e.attrs[9] = 0;
                if(create) numactors++;
                break;
            case AFFINITY:
                while(e.attrs[0] < 0) e.attrs[0] += T_ALL;
                while(e.attrs[0] >= T_ALL) e.attrs[0] -= T_ALL;
                while(e.attrs[1] < 0) e.attrs[1] += 360;
                while(e.attrs[1] >= 360) e.attrs[1] -= 360;
                while(e.attrs[2] < -90) e.attrs[2] += 180;
                while(e.attrs[2] > 90) e.attrs[2] -= 180;
                break;
            case TELEPORT:
                while(e.attrs[0] < -1) e.attrs[0] += 361;
                while(e.attrs[0] >= 360) e.attrs[0] -= 360;
                while(e.attrs[1] < -90) e.attrs[1] += 180;
                while(e.attrs[1] > 90) e.attrs[1] -= 180;
                while(e.attrs[5] < 0) e.attrs[5] += 6;
                while(e.attrs[5] >= 6) e.attrs[5] -= 6;
                if(e.attrs[6] < 0) e.attrs[6] = 0;
                if(e.attrs[7] < 0) e.attrs[7] = 0;
                break;
            default: break;
        }
    }

    const char *findname(int type)
    {
        if(type >= NOTUSED && type < MAXENTTYPES) return enttype[type].name;
        return "";
    }

    int findtype(char *type)
    {
        loopi(MAXENTTYPES) if(!strcmp(type, enttype[i].name)) return i;
        return NOTUSED;
    }

    // these functions are called when the client touches the item
    void execlink(gameent *d, int index, bool local, int ignore)
    {
        if(ents.inrange(index) && maylink(ents[index]->type))
        {
            gameentity &e = *(gameentity *)ents[index];
            if(e.type == TRIGGER && !cantrigger(index)) return;
            bool commit = false;
            int frstent = min(firstent(MAPMODEL), min(firstent(LIGHTFX), min(firstent(PARTICLES), firstent(MAPSOUND)))),
                flstent = max(lastent(MAPMODEL), max(lastent(LIGHTFX), max(lastent(PARTICLES), lastent(MAPSOUND))));
            for(int i = frstent; i < flstent; ++i) if(ents[i]->links.find(index) >= 0)
            {
                gameentity &f = *(gameentity *)ents[i];
                if(ents.inrange(ignore) && ents[ignore]->links.find(index) >= 0) continue;
                bool both = e.links.find(i) >= 0;
                switch(f.type)
                {
                    case MAPMODEL:
                    {
                        f.lastemit = e.lastemit;
                        if(e.type == TRIGGER) f.setspawned(TRIGSTATE(e.spawned(), e.attrs[4]));
                        break;
                    }
                    case LIGHTFX:
                    case PARTICLES:
                    {
                        f.lastemit = e.lastemit;
                        if(e.type == TRIGGER) f.setspawned(TRIGSTATE(e.spawned(), e.attrs[4]));
                        else if(local) commit = true;
                        break;
                    }
                    case MAPSOUND:
                    {
                        f.lastemit = e.lastemit;
                        if(e.type == TRIGGER) f.setspawned(TRIGSTATE(e.spawned(), e.attrs[4]));
                        else if(local) commit = true;
                        if(mapsounds.inrange(f.attrs[0]) && !issound(f.schan))
                        {
                            int flags = SND_MAP;
                            loopk(SND_LAST) if(f.attrs[4]&(1<<k)) flags |= 1<<k;
                            playsound(f.attrs[0], both ? f.o : e.o, NULL, flags, f.attrs[3] ? f.attrs[3] : -1, f.attrs[1] || f.attrs[2] ? f.attrs[1] : -1, f.attrs[2] ? f.attrs[2] : -1, &f.schan);
                        }
                        break;
                    }
                    default: break;
                }
            }
            if(d && commit) client::addmsg(N_EXECLINK, "ri2", d->clientnum, index);
        }
    }

    bool tryspawn(dynent *d, const vec &o, float yaw, float pitch)
    {
        game::fixfullrange(d->yaw = yaw, d->pitch = pitch, d->roll = 0);
        (d->o = o).z += d->height+d->aboveeye;
        return physics::entinmap(d, true);
    }

    void spawnplayer(gameent *d, int ent, bool suicide)
    {
        if(ent >= 0 && ents.inrange(ent))
        {
            vec pos = ents[ent]->o;
            switch(ents[ent]->type)
            {
                case PLAYERSTART: case ACTOR:
                    if(tryspawn(d, pos, ents[ent]->attrs[1], ents[ent]->attrs[2])) return;
                    break;
                case CHECKPOINT:
                {
                    float yaw = ents[ent]->attrs[1], pitch = ents[ent]->attrs[2];
                    if(m_ra_gauntlet(game::gamemode, game::mutators) && d->team != T_ALPHA)
                    {
                        yaw -= 180;
                        pitch = -pitch;
                    }
                    physics::droptofloor(pos, ENT_DUMMY);
                    if(tryspawn(d, pos, yaw, pitch)) return;
                    break;
                }
                default:
                    physics::droptofloor(pos, ENT_DUMMY);
                    if(tryspawn(d, pos, rnd(360), 0)) return;
                    break;
            }
        }
        else
        {
            vector<int> spawns;
            loopk(4)
            {
                spawns.shrink(0);
                switch(k)
                {
                    case 0:
                        if(m_play(game::gamemode) && m_team(game::gamemode, game::mutators))
                            loopenti(PLAYERSTART) if(ents[i]->type == PLAYERSTART && ents[i]->attrs[0] == d->team && m_check(ents[i]->attrs[3], ents[i]->attrs[4], game::gamemode, game::mutators))
                                spawns.add(i);
                        break;
                    case 1: case 2:
                        loopenti(PLAYERSTART) if(ents[i]->type == PLAYERSTART && (k == 2 || m_check(ents[i]->attrs[3], ents[i]->attrs[4], game::gamemode, game::mutators))) spawns.add(i);
                        break;
                    case 3:
                        loopenti(PLAYERSTART) if(ents[i]->type == WEAPON && m_check(ents[i]->attrs[2], ents[i]->attrs[3], game::gamemode, game::mutators)) spawns.add(i);
                        break;
                    default: break;
                }
                while(!spawns.empty())
                {
                    int r = rnd(spawns.length());
                    gameentity &e = *(gameentity *)ents[spawns[r]];
                    if(tryspawn(d, e.o, e.type == PLAYERSTART ? e.attrs[1] : rnd(360), e.type == PLAYERSTART ? e.attrs[2] : 0))
                        return;
                    spawns.remove(r); // must've really sucked, try another one
                }
            }
            d->yaw = d->pitch = d->roll = 0;
            d->o.x = d->o.y = d->o.z = getworldsize();
            d->o.x *= 0.5f; d->o.y *= 0.5f;
            if(physics::entinmap(d, true)) return;
        }
        if(!m_edit(game::gamemode) && suicide) game::suicide(d, HIT_SPAWN);
    }

    void editent(int i)
    {
        extentity &e = *ents[i];
        fixentity(i, true);
        if(m_edit(game::gamemode) && game::player1->state == CS_EDITING)
            client::addmsg(N_EDITENT, "ri5iv", i, (int)(e.o.x*DMF), (int)(e.o.y*DMF), (int)(e.o.z*DMF), e.type, e.attrs.length(), e.attrs.length(), e.attrs.getbuf()); // FIXME
        if(e.type < MAXENTTYPES)
        {
            firstenttype[e.type] = min(firstenttype[e.type], i);
            firstusetype[enttype[e.type].usetype] = min(firstusetype[enttype[e.type].usetype], i);
            lastenttype[e.type] = max(lastenttype[e.type], i+1);
            lastusetype[enttype[e.type].usetype] = max(lastusetype[enttype[e.type].usetype], i+1);
        }
    }

    float dropheight(extentity &e)
    {
        if(e.type==MAPMODEL || e.type==AFFINITY) return 0.0f;
        return 4.0f;
    }

    bool maylink(int type, int ver)
    {
        if(enttype[type].links && enttype[type].links <= (ver ? ver : VERSION_GAME))
                return true;
        return false;
    }

    bool canlink(int index, int node, bool msg)
    {
        if(ents.inrange(index) && ents.inrange(node))
        {
            if(index != node && maylink(ents[index]->type) && maylink(ents[node]->type) &&
                    (enttype[ents[index]->type].canlink&(1<<ents[node]->type)))
                        return true;
            if(msg)
                conoutf("\frEntity %s (%d) and %s (%d) are not linkable", enttype[ents[index]->type].name, index, enttype[ents[node]->type].name, node);

            return false;
        }
        if(msg) conoutf("\frEntity %d and %d are unable to be linked as one does not seem to exist", index, node);
        return false;
    }

    bool linkents(int index, int node, bool add, bool local, bool toggle)
    {
        if(ents.inrange(index) && ents.inrange(node) && index != node && canlink(index, node, local && verbose))
        {
            gameentity &e = *(gameentity *)ents[index], &f = *(gameentity *)ents[node];
            bool recip = (enttype[e.type].reclink&(1<<f.type)) || (enttype[f.type].reclink&(1<<e.type));
            int g = -1, h = -1;
            if((toggle || !add) && (g = e.links.find(node)) >= 0)
            {
                h = f.links.find(index);
                if(!add || !canlink(node, index) || (toggle && h >= 0))
                {
                    e.links.remove(g);
                    if(recip && h >= 0) f.links.remove(h);
                    fixentity(index, true);
                    if(local && m_edit(game::gamemode)) client::addmsg(N_EDITLINK, "ri3", 0, index, node);
                    if(verbose > 2) conoutf("\faEntity %s (%d) and %s (%d) delinked", enttype[ents[index]->type].name, index, enttype[ents[node]->type].name, node);
                    return true;
                }
                else if(toggle && canlink(node, index))
                {
                    f.links.add(index);
                    if(recip && (h = e.links.find(node)) < 0) e.links.add(node);
                    fixentity(node, true);
                    if(local && m_edit(game::gamemode)) client::addmsg(N_EDITLINK, "ri3", 1, node, index);
                    if(verbose > 2) conoutf("\faEntity %s (%d) and %s (%d) linked", enttype[ents[node]->type].name, node, enttype[ents[index]->type].name, index);
                    return true;
                }
            }
            else if(toggle && canlink(node, index) && (g = f.links.find(index)) >= 0)
            {
                f.links.remove(g);
                if(recip && (h = e.links.find(node)) >= 0) e.links.remove(h);
                fixentity(node, true);
                if(local && m_edit(game::gamemode)) client::addmsg(N_EDITLINK, "ri3", 0, node, index);
                if(verbose > 2) conoutf("\faEntity %s (%d) and %s (%d) delinked", enttype[ents[node]->type].name, node, enttype[ents[index]->type].name, index);
                return true;
            }
            else if(toggle || add)
            {
                e.links.add(node);
                if(recip && (h = f.links.find(index)) < 0) f.links.add(index);
                fixentity(index, true);
                if(local && m_edit(game::gamemode)) client::addmsg(N_EDITLINK, "ri3", 1, index, node);
                if(verbose > 2) conoutf("\faEntity %s (%d) and %s (%d) linked", enttype[ents[index]->type].name, index, enttype[ents[node]->type].name, node);
                return true;
            }
        }
        if(verbose > 2)
            conoutf("\frEntity %s (%d) and %s (%d) failed linking", enttype[ents[index]->type].name, index, enttype[ents[node]->type].name, node);
        return false;
    }

    void entitylink(int index, int node, bool both = true)
    {
        if(ents.inrange(index) && ents.inrange(node))
        {
            gameentity &e = *(gameentity *)ents[index], &f = *(gameentity *)ents[node];
            if(e.links.find(node) < 0) linkents(index, node, true, true, false);
            if(both && f.links.find(index) < 0) linkents(node, index, true, true, false);
        }
    }

    struct octatele
    {
        int ent, tag;
    };
    vector<octatele> octateles;

    void readent(stream *g, int mtype, int mver, char *gid, int gver, int id)
    {
        gameentity &f = *(gameentity *)ents[id];
        if(mtype == MAP_OCTA)
        {
            // translate into our format
            switch(f.type)
            {
                // LIGHT            -   LIGHT
                // MAPMODEL         -   MAPMODEL
                // PLAYERSTART      -   PLAYERSTART
                // ENVMAP           -   ENVMAP
                // PARTICLES        -   PARTICLES
                // MAPSOUND         -   MAPSOUND
                // SPOTLIGHT        -   LIGHTFX
                //                  -   SUNLIGHT
                case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: break;

                // I_SHELLS         -   WEAPON      W_SHOTGUN
                // I_BULLETS        -   WEAPON      W_SMG
                // I_ROCKETS        -   WEAPON      W_FLAMER
                // I_ROUNDS         -   WEAPON      W_RIFLE
                // I_GL             -   WEAPON      W_GRENADE
                // I_CARTRIDGES     -   WEAPON      W_PLASMA
                case 9: case 10: case 11: case 12: case 13: case 14:
                {
                    int weap = f.type-8;
                    if(weap >= 0 && weap <= 5)
                    {
                        const int weapmap[6] = { W_SHOTGUN, W_SMG, W_FLAMER, W_RIFLE, W_GRENADE, W_PLASMA };
                        f.type = WEAPON;
                        f.attrs[0] = weapmap[weap];
                        f.attrs[1] = 0;
                    }
                    else f.type = NOTUSED;
                    break;
                }
                // I_QUAD           -   WEAPON      W_ROCKET
                case 19:
                {
                    f.type = WEAPON;
                    f.attrs[0] = W_ROCKET;
                    f.attrs[1] = 0;
                    break;
                }

                // TELEPORT         -   TELEPORT
                // TELEDEST         -   TELEPORT (linked)
                case 20: case 21:
                {
                    octatele &t = octateles.add();
                    t.ent = id;
                    if(f.type == 21)
                    {
                        t.tag = f.attrs[1]+1; // needs translating later
                        f.attrs[1] = -1;
                    }
                    else
                    {
                        t.tag = -(f.attrs[0]+1);
                        f.attrs[0] = -1;
                    }
                    f.attrs[2] = f.attrs[3] = f.attrs[4] = 0;
                    f.type = TELEPORT;
                    break;
                }
                // MONSTER          -   NOTUSED
                case 22:
                {
                    f.type = NOTUSED;
                    break;
                }
                // CARROT           -   TRIGGER     0
                case 23:
                {
                    f.type = NOTUSED;
                    f.attrs[0] = f.attrs[1] = f.attrs[2] = f.attrs[3] = f.attrs[4] = 0;
                    break;
                }
                // JUMPPAD          -   PUSHER
                case 24:
                {
                    f.type = PUSHER;
                    break;
                }
                // BASE             -   AFFINITY    1:idx       T_NEUTRAL
                case 25:
                {
                    f.type = AFFINITY;
                    if(f.attrs[0] < 0) f.attrs[0] = 0;
                    f.attrs[1] = T_NEUTRAL; // spawn as neutrals
                    break;
                }
                // RESPAWNPOINT     -   CHECKPOINT
                case 26:
                {
                    f.type = CHECKPOINT;
                    break;
                }
                // FLAG             -   AFFINITY        #           2:team
                case 31:
                {
                    f.type = AFFINITY;
                    f.attrs[0] = 0;
                    if(f.attrs[1] <= 0) f.attrs[1] = -1; // needs a team
                    break;
                }

                // I_HEALTH         -   NOTUSED
                // I_BOOST          -   NOTUSED
                // I_GREENARMOUR    -   NOTUSED
                // I_YELLOWARMOUR   -   NOTUSED
                // BOX              -   NOTUSED
                // BARREL           -   NOTUSED
                // PLATFORM         -   NOTUSED
                // ELEVATOR         -   NOTUSED
                default:
                {
                    if(verbose) conoutf("\frWARNING: ignoring entity %d type %d", id, f.type);
                    f.type = NOTUSED;
                    break;
                }
            }
        }
    }

    void writeent(stream *g, int id)
    {
    }

    void remapents(vector<int> &idxs)
    {
        int numents[MAXENTTYPES], numinvalid = 0;
        memset(numents, 0, sizeof(numents));
        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.type < MAXENTTYPES) numents[e.type]++;
            else numinvalid++;
        }
        int offsets[MAXENTTYPES];
        memset(offsets, -1, sizeof(offsets));
        int priority = INT_MIN, nextpriority = INT_MIN;
        loopi(MAXENTTYPES) nextpriority = max(nextpriority, enttype[i].priority);
        int offset = 0;
        do
        {
            priority = nextpriority;
            nextpriority = INT_MIN;
            loopi(MAXENTTYPES) if(offsets[i] < 0)
            {
                if(enttype[i].priority >= priority) { offsets[i] = offset; offset += numents[i]; }
                else nextpriority = max(nextpriority, enttype[i].priority);
            }
        } while(nextpriority < priority);
        idxs.setsize(0);
        idxs.reserve(offset + numinvalid);
        while(idxs.length() < offset + numinvalid) idxs.add(-1);
        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            idxs[e.type < MAXENTTYPES ? offsets[e.type]++ : offset++] = i;
        }
    }

    void importentities(int mtype, int mver, int gver)
    {
        int flag = 0, teams[T_TOTAL] = {0};
        progress(0, "Importing entities...");
        loopv(octateles) // translate teledest to teleport and link them appropriately
        {
            octatele &t = octateles[i];
            if(t.tag <= 0) continue;
            gameentity &e = *(gameentity *)ents[t.ent];
            if(e.type != TELEPORT) continue;
            loopvj(octateles)
            {
                octatele &p = octateles[j];
                if(p.tag != -t.tag) continue;
                gameentity &f = *(gameentity *)ents[p.ent];
                if(f.type != TELEPORT) continue;
                if(verbose) conoutf("\frWARNING: teledest %d and teleport %d linked automatically", t.ent, p.ent);
                f.links.add(t.ent);
            }
        }
        loopv(octateles) // second pass teledest translation
        {
            octatele &t = octateles[i];
            if(t.tag <= 0) continue;
            gameentity &e = *(gameentity *)ents[t.ent];
            if(e.type != TELEPORT) continue;
            int dest = -1;
            float bestdist = enttype[TELEPORT].radius*4.f;
            loopvj(octateles)
            {
                octatele &p = octateles[j];
                if(p.tag >= 0) continue;
                gameentity &f = *(gameentity *)ents[p.ent];
                if(f.type != TELEPORT) continue;
                float dist = e.o.dist(f.o);
                if(dist > bestdist) continue;
                dest = p.ent;
                bestdist = dist;
            }
            if(ents.inrange(dest))
            {
                gameentity &f = *(gameentity *)ents[dest];
                if(verbose) conoutf("\frWARNING: replaced teledest %d with closest teleport %d", t.ent, dest);
                f.attrs[0] = e.attrs[0]; // copy the yaw
                loopvk(e.links) if(f.links.find(e.links[k]) < 0) f.links.add(e.links[k]);
                loopvj(ents) if(j != t.ent && j != dest)
                {
                    gameentity &g = *(gameentity *)ents[j];
                    if(g.type == TELEPORT)
                    {
                        int link = g.links.find(t.ent);
                        if(link >= 0)
                        {
                            g.links.remove(link);
                            if(g.links.find(dest) < 0) g.links.add(dest);
                            if(verbose) conoutf("\frWARNING: imported link to teledest %d to teleport %d", t.ent, j);
                        }
                    }
                }
                e.type = NOTUSED; // get rid of ye olde teledest
                e.links.shrink(0);
            }
            else if(verbose) conoutf("\frWARNING: teledest %d has become a teleport", t.ent);
        }
        octateles.setsize(0);
        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            progress(i/float(ents.length()), "Importing entities...");
            switch(e.type)
            {
                case WEAPON:
                {
                    float mindist = float((enttype[WEAPON].radius*4)*(enttype[WEAPON].radius*4));
                    int weaps[W_MAX];
                    loopj(W_MAX) weaps[j] = j != e.attrs[0] ? 0 : 1;
                    loopvj(ents) if(j != i)
                    {
                        gameentity &f = *(gameentity *)ents[j];
                        if(f.type == WEAPON && e.o.squaredist(f.o) <= mindist && isweap(f.attrs[0]))
                        {
                            weaps[f.attrs[0]]++;
                            f.type = NOTUSED;
                            if(verbose) conoutf("\frWARNING: culled tightly packed weapon %d [%d]", j, f.attrs[0]);
                        }
                    }
                    int best = e.attrs[0];
                    loopj(W_MAX) if(weaps[j] > weaps[best]) best = j;
                    e.attrs[0] = best;
                    break;
                }
                case AFFINITY: // replace bases/neutral flags near team flags
                {
                    if(valteam(e.attrs[1], T_FIRST)) teams[e.attrs[1]-T_FIRST]++;
                    else if(e.attrs[1] == T_NEUTRAL)
                    {
                        int dest = -1;

                        loopvj(ents) if(j != i)
                        {
                            gameentity &f = *(gameentity *)ents[j];

                            if(f.type == AFFINITY && f.attrs[1] != T_NEUTRAL &&
                                (!ents.inrange(dest) || e.o.dist(f.o) < ents[dest]->o.dist(f.o)) &&
                                    e.o.dist(f.o) <= enttype[AFFINITY].radius*4.f)
                                        dest = j;
                        }

                        if(ents.inrange(dest))
                        {
                            gameentity &f = *(gameentity *)ents[dest];
                            if(verbose) conoutf("\frWARNING: old base %d (%d, %d) replaced with flag %d (%d, %d)", i, e.attrs[0], e.attrs[1], dest, f.attrs[0], f.attrs[1]);
                            if(!f.attrs[0]) f.attrs[0] = e.attrs[0]; // give it the old base idx
                            e.type = NOTUSED;
                        }
                        else if(e.attrs[0] > flag) flag = e.attrs[0]; // find the highest idx
                    }
                    break;
                }
            }
        }

        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            switch(e.type)
            {
                case AFFINITY:
                {
                    if(!e.attrs[0]) e.attrs[0] = ++flag; // assign a sane idx
                    if(!valteam(e.attrs[1], T_NEUTRAL)) // assign a team
                    {
                        int lowest = -1;
                        loopk(T_TOTAL) if(lowest<0 || teams[k] < teams[lowest]) lowest = i;
                        e.attrs[1] = lowest+T_FIRST;
                        teams[lowest]++;
                    }
                    break;
                }
            }
        }
    }

    void checkyawmode(gameentity &e, int mtype, int mver, int gver, int y, int m)
    {
        if((y >= 0) && ((mtype == MAP_OCTA && mver <= 30) || (mtype == MAP_MAPZ && mver <= 39)))
            e.attrs[y] = (e.attrs[y] + 180)%360;
        if(m >= 0 && mtype == MAP_MAPZ && gver <= 201)
        {
            if(e.attrs[m] > 3)
            {
                if(e.attrs[m] != 5) e.attrs[m]++;
                else e.attrs[m]--;
            }
            else if(e.attrs[m] < -3)
            {
                if(e.attrs[m] != -5) e.attrs[m]--;
                else e.attrs[m]++;
            }
        }
    }

    void checkmodes(int &attr)
    {
        if(attr > 0) attr = (1<<(attr-G_PLAY));
        else if(attr < 0) attr = 0-(1<<((0-attr)-G_PLAY));
    }

    void updateoldentities(int mtype, int mver, int gver)
    {
        loopvj(ents)
        {
            gameentity &e = *(gameentity *)ents[j];
            progress(j/float(ents.length()), "Updating old entities...");
            switch(e.type)
            {
                case LIGHTFX:
                {
                    if(mtype == MAP_OCTA || (mtype == MAP_MAPZ && gver <= 159))
                    {
                        e.attrs[1] = e.attrs[0];
                        e.attrs[0] = LFX_SPOTLIGHT;
                        e.attrs[2] = e.attrs[3] = e.attrs[4] = 0;
                    }
                    break;
                }
                case PLAYERSTART:
                {
                    if(mtype == MAP_OCTA || (mtype == MAP_MAPZ && gver <= 158))
                    {
                        short yaw = e.attrs[0];
                        e.attrs[0] = e.attrs[1];
                        e.attrs[1] = yaw;
                        e.attrs[2] = e.attrs[3] = e.attrs[4] = 0;
                    }
                    if(mtype == MAP_MAPZ && gver <= 164 && e.attrs[0] > T_MULTI) e.attrs[0] = T_NEUTRAL;
                    if(mtype == MAP_MAPZ && gver <= 201)
                    {
                        if(e.attrs[3] > 3)
                        {
                            if(e.attrs[3] != 5) e.attrs[3]++;
                            else e.attrs[3]--;
                        }
                        else if(e.attrs[3] < -3)
                        {
                            if(e.attrs[3] != -5) e.attrs[3]--;
                            else e.attrs[3]++;
                        }
                    }
                    if(mtype == MAP_MAPZ && gver <= 213)
                    {
                        e.attrs[5] = e.attrs[4];
                        e.attrs[4] = 0;
                        checkmodes(e.attrs[3]);
                    }
                    break;
                }
                case PARTICLES:
                {
                    if(mtype == MAP_OCTA || (mtype == MAP_MAPZ && mver <= 36))
                    {
                        switch(e.attrs[0])
                        {
                            case 0: if(e.attrs[3] <= 0) break;
                            case 4: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 5: case 6:
                                e.attrs[3] = (((e.attrs[3]&0xF)<<4)|((e.attrs[3]&0xF0)<<8)|((e.attrs[3]&0xF00)<<12))+0x0F0F0F;
                                if(e.attrs[0] != 5 && e.attrs[0] != 6) break;
                            case 3:
                                e.attrs[2] = (((e.attrs[2]&0xF)<<4)|((e.attrs[2]&0xF0)<<8)|((e.attrs[2]&0xF00)<<12))+0x0F0F0F; break;
                            default: break;
                        }
                    }
                    break;
                }
                case TELEPORT:
                {
                    if(mtype == MAP_OCTA)
                    {
                        e.attrs[2] = 100; // give a push
                        e.attrs[4] = e.attrs[1] >= 0 ? 0x2CE : 0;
                        e.attrs[1] = e.attrs[3] = 0;
                        e.o.z += 8; // teleport here is at middle
                        if(e.attrs[0] >= 0)
                        {
                            int material = lookupmaterial(e.o), clipmat = material&MATF_CLIP;
                            if(clipmat == MAT_CLIP || (material&MAT_DEATH) || (material&MATF_VOLUME) == MAT_LAVA)
                                e.o.add(vec(e.attrs[0]*RAD, e.attrs[1]*RAD));
                        }
                    }
                    if(e.attrs[0] >= 0) checkyawmode(e, mtype, mver, gver, 0, -1);
                    break;
                }
                case WEAPON:
                {
                    if(mtype == MAP_MAPZ && gver <= 90)
                    { // move grenade to the end of the weapon array
                        if(e.attrs[0] >= 4) e.attrs[0]--;
                        else if(e.attrs[0] == 3) e.attrs[0] = 7;
                    }
                    if(mtype == MAP_MAPZ && gver <= 97) e.attrs[0]++; // add in pistol
                    if(mtype != MAP_MAPZ || gver <= 112) e.attrs[1] = 0;
                    if(mtype == MAP_MAPZ && gver <= 160)
                    {
                        e.attrs[0]++; // add in claw
                        if(e.attrs[0] < W_OFFSET) e.attrs[0] = 8; // cleanup for fixentity
                    }
                    if(mtype == MAP_MAPZ && gver <= 163) e.attrs[0]++; // add in sword
                    if(mtype == MAP_MAPZ && gver <= 213)
                    {
                        e.attrs[4] = e.attrs[3];
                        e.attrs[3] = 0;
                        checkmodes(e.attrs[2]);
                    }
                    if(mtype == MAP_MAPZ && gver <= 218)
                    { // insert mine before rockets (9 -> 10) after grenades (8)
                        if(e.attrs[0] >= 9) e.attrs[0]++;
                    }
                    if(mtype == MAP_MAPZ && gver <= 221)
                    { // insert zapper before rifle (7 -> 8) after plasma (6)
                        if(e.attrs[0] >= 7) e.attrs[0]++;
                    }
                    break;
                }
                case TRIGGER:
                {
                    if(mtype == MAP_MAPZ && gver <= 158) e.attrs[4] = 0;
                    if(mtype == MAP_MAPZ && gver <= 213) checkmodes(e.attrs[5]);
                    break;
                }
                case PUSHER:
                {
                    if(mtype == MAP_OCTA || (mtype == MAP_MAPZ && gver <= 95)) e.attrs[0] = int(e.attrs[0]*1.25f);
                    if(mtype == MAP_OCTA || (mtype == MAP_MAPZ && gver <= 162))
                    {
                        vec dir = vec(e.attrs[2], e.attrs[1], e.attrs[0]).mul(10);
                        float yaw = 0, pitch = 0;
                        vectoyawpitch(vec(dir).normalize(), yaw, pitch);
                        e.attrs[0] = int(yaw);
                        e.attrs[1] = int(pitch);
                        e.attrs[2] = int(dir.magnitude());
                        e.attrs[5] = 0;
                    }
                    else checkyawmode(e, mtype, mver, gver, 0, -1);
                    break;
                }
                case AFFINITY:
                {
                    if(mtype == MAP_OCTA || (mtype == MAP_MAPZ && gver <= 158))
                    {
                        e.attrs[0] = e.attrs[1];
                        e.attrs[1] = e.attrs[2];
                        e.attrs[2] = e.attrs[3];
                        e.attrs[3] = e.attrs[4] = 0;
                    }
                    if(mtype == MAP_MAPZ && gver <= 164 && e.attrs[0] > T_MULTI) e.attrs[0] = T_NEUTRAL;
                    checkyawmode(e, mtype, mver, gver, 1, 3);
                    if(mtype == MAP_MAPZ && gver <= 213)
                    {
                        e.attrs[5] = e.attrs[4];
                        e.attrs[4] = 0;
                        checkmodes(e.attrs[3]);
                    }
                    break;
                }
                case ACTOR:
                {
                    if(mtype == MAP_MAPZ && gver <= 200) e.attrs[0] -= 1; // remoe A_BOT from array
                    if(mtype == MAP_MAPZ && gver <= 207 && e.attrs[0] > 1) // combine into one type
                    {
                        switch(e.attrs[0])
                        {
                            case 5: default: e.attrs[5] = 8; e.attrs[6] = 100; e.attrs[7] = 40; break;
                            case 4: e.attrs[5] = 6; e.attrs[6] = 150; e.attrs[7] = 40; break;
                            case 3: e.attrs[5] = 4; e.attrs[6] = 200; e.attrs[7] = 35; break;
                            case 2: e.attrs[5] = 2; e.attrs[6] = 50; e.attrs[7] = 50; break;
                        }
                    }
                    checkyawmode(e, mtype, mver, gver, 1, 3);
                    if(mtype == MAP_MAPZ && gver <= 213)
                    {
                        e.attrs[9] = e.attrs[8];
                        e.attrs[8] = e.attrs[7];
                        e.attrs[7] = e.attrs[6];
                        e.attrs[6] = e.attrs[5];
                        e.attrs[5] = e.attrs[4];
                        e.attrs[4] = 0;
                        checkmodes(e.attrs[4]);
                    }
                    break;
                }
                case CHECKPOINT:
                {
                    checkyawmode(e, mtype, mver, gver, 1, 3);
                    if(mtype == MAP_MAPZ && gver <= 213)
                    {
                        e.attrs[6] = e.attrs[5];
                        e.attrs[5] = e.attrs[4];
                        e.attrs[4] = 0;
                        checkmodes(e.attrs[3]);
                    }
                    break;
                }
                default: break;
            }
            if(gver <= 216 && enttype[e.type].modesattr > 0) // mode/mutator array updates
            {
                int attr = enttype[e.type].modesattr;
                if(e.attrs[attr])
                {
                    bool neg = e.attrs[attr] < 0;
                    int value = neg ? 0-e.attrs[attr] : e.attrs[attr];
                    value &= ~(1<<2);
                    loopi(5)
                    {
                        int mode = i+3, bit = 1<<mode, next = 1<<(mode-1);
                        if(value&bit) { value &= ~bit; value |= next; }
                    }
                    e.attrs[attr] = neg ? 0-value : value;
                }
                attr++; // mutators
                if(e.attrs[attr])
                {
                    bool neg = e.attrs[attr] < 0;
                    int value = neg ? 0-e.attrs[attr] : e.attrs[attr];
                    loopirev(14)
                    {
                        int mut = i+2, bit = 1<<mut, next = 1<<(mut+1);
                        if(value&bit) { value &= ~bit; value |= next; }
                    }
                    e.attrs[attr] = neg ? 0-value : value;
                }
            }
        }
    }

    void initents(int mtype, int mver, char *gid, int gver)
    {
        lastroutenode = routeid = -1;
        numactors = lastroutetime = droproute = 0;
        airnodes.setsize(0);
        ai::oldwaypoints.setsize(0);
        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            progress(i/float(ents.length()), "Setting entity attributes...");
            int num = max(5, enttype[e.type].numattrs);
            if(e.attrs.length() < num) e.attrs.add(0, num - e.attrs.length());
            else if(e.attrs.length() > num) e.attrs.setsize(num);
        }
        if(mtype == MAP_OCTA || (mtype == MAP_MAPZ && gver <= 49)) importentities(mtype, mver, gver);
        if(mtype == MAP_OCTA || (mtype == MAP_MAPZ && gver < VERSION_GAME)) updateoldentities(mtype, mver, gver);
        loopv(ents)
        {
            progress(i/float(ents.length()), "Fixing entities...");
            fixentity(i, false);
            switch(ents[i]->type)
            {
                case ACTOR: numactors++; break;
                default: break;
            }
        }
        memset(firstenttype, 0, sizeof(firstenttype));
        memset(firstusetype, 0, sizeof(firstusetype));
        memset(lastenttype, 0, sizeof(lastenttype));
        memset(lastusetype, 0, sizeof(lastusetype));
        if(m_onslaught(game::gamemode, game::mutators) && !numactors)
        {
            loopv(ents) if(ents[i]->type == PLAYERSTART || ents[i]->type == WEAPON)
            {
                extentity &e = *newent();
                ents.add(&e);
                e.type = ACTOR;
                e.o = ents[i]->o;
                e.attrs.add(0, max(5, enttype[ACTOR].numattrs));
                e.attrs[0] = (i%5 != 4 || ents[i]->type == WEAPON ? A_GRUNT : A_TURRET)-1;
                switch(ents[i]->type)
                {
                    case PLAYERSTART:
                        loopj(5) e.attrs[j+1] = ents[i]->attrs[j+1]; // yaw, pitch, mode, muts, id
                        break;
                    case WEAPON:
                        loopj(3) e.attrs[j+3] = ents[i]->attrs[j+2]; // mode, muts, id
                    default:
                        e.attrs[1] = (i%8)*45;
                        break;
                }
                numactors++;
            }
        }
        loopv(ents)
        {
            gameentity &e = *(gameentity *)ents[i];
            progress(i/float(ents.length()), "Updating entities...");
            if(mtype == MAP_MAPZ && gver <= 212)
            {
                if(e.type == ROUTE) e.type = NOTUSED;
                else if(e.type == UNUSEDENT)
                {
                    ai::oldwaypoint &o = ai::oldwaypoints.add();
                    o.o = e.o;
                    o.ent = i;
                    loopvj(e.links) o.links.add(e.links[j]);
                    e.type = NOTUSED;
                }
            }
            if(mtype == MAP_MAPZ && gver <= 221 && (e.type == ROUTE || e.type == UNUSEDENT)) e.type = NOTUSED;
            if(e.type < MAXENTTYPES)
            {
                firstenttype[e.type] = min(firstenttype[e.type], i);
                firstusetype[enttype[e.type].usetype] = min(firstusetype[enttype[e.type].usetype], i);
                lastenttype[e.type] = max(lastenttype[e.type], i+1);
                lastusetype[enttype[e.type].usetype] = max(lastusetype[enttype[e.type].usetype], i+1);
            }
            if(enttype[e.type].usetype == EU_ITEM || e.type == TRIGGER)
            {
                setspawn(i, 0);
                if(enttype[e.type].synckin) // find shared kin
                {
                    loopvj(e.links) if(ents.inrange(e.links[j]))
                    {
                        loopvk(ents) if(ents[k]->type == e.type && ents[k]->links.find(e.links[j]) >= 0)
                        {
                            gameentity &f = *(gameentity *)ents[k];
                            if(e.kin.find(k) < 0) e.kin.add(k);
                            if(f.kin.find(i) < 0) f.kin.add(i);
                        }
                    }
                }
            }
        }
    }

    #define renderfocus(i,f) { gameentity &e = *(gameentity *)ents[i]; f; }
    void renderlinked(gameentity &e, int idx)
    {
        loopv(e.links)
        {
            int index = e.links[i];
            if(ents.inrange(index))
            {
                gameentity &f = *(gameentity *)ents[index];
                bool both = false;
                loopvj(f.links) if(f.links[j] == idx)
                {
                    both = true;
                    break;
                }
                part_trace(e.o, f.o, showentsize, 1, 1, both ? colourviolet : colourdarkviolet, showentinterval);
            }
        }
    }

    bool shouldshowents(int level)
    {
        return max(showentradius, max(showentdir, showentlinks)) >= level;
    }

    void renderentshow(gameentity &e, int idx, int level)
    {
        if(e.o.squaredist(camera1->o) > showentdist*showentdist) return;
        #define entdirpart(o,yaw,pitch,length,fade,colour) part_dir(o, yaw, pitch, length, showentsize, 1, fade, colour, showentinterval);
        if(showentradius >= level)
        {
            switch(e.type)
            {
                case PLAYERSTART:
                {
                    part_radius(vec(e.o).add(vec(0, 0, game::player1->zradius/2)), vec(game::player1->xradius, game::player1->yradius, game::player1->zradius/2), showentsize, 1, 1, TEAM(e.attrs[0], colour));
                    break;
                }
                case ACTOR:
                {
                    part_radius(vec(e.o).add(vec(0, 0, actor[e.attrs[0]].height/2)), vec(actor[e.attrs[0]].xradius, actor[e.attrs[0]].height/2), showentsize, 1, 1, TEAM(T_ENEMY, colour));
                    part_radius(e.o, vec(ai::ALERTMAX, ai::ALERTMAX, ai::ALERTMAX), showentsize, 1, 1, TEAM(T_ENEMY, colour));
                    break;
                }
                case MAPSOUND:
                {
                    part_radius(e.o, vec(e.attrs[1], e.attrs[1], e.attrs[1]), showentsize, 1, 1, colourcyan);
                    part_radius(e.o, vec(e.attrs[2], e.attrs[2], e.attrs[2]), showentsize, 1, 1, colourcyan);
                    break;
                }
                case ENVMAP:
                {
                    int s = e.attrs[0] ? clamp(e.attrs[0], 0, 10000) : envmapradius;
                    part_radius(e.o, vec(s, s, s), showentsize, 1, 1, colourcyan);
                    break;
                }
                case LIGHT:
                {
                    int s = e.attrs[0] ? e.attrs[0] : hdr.worldsize,
                        colour = ((e.attrs[1])<<16)|((e.attrs[2])<<8)|(e.attrs[3]);
                    part_radius(e.o, vec(s, s, s), showentsize, 1, 1, colour);
                    break;
                }
                case LIGHTFX:
                {
                    if(e.attrs[0] == LFX_SPOTLIGHT) loopv(e.links) if(ents.inrange(e.links[i]) && ents[e.links[i]]->type == LIGHT)
                    {
                        gameentity &f = *(gameentity *)ents[e.links[i]];
                        float radius = f.attrs[0];
                        if(!radius) radius = 2*e.o.dist(f.o);
                        vec dir = vec(e.o).sub(f.o).normalize();
                        float angle = clamp(int(e.attrs[1]), 1, 89);
                        int colour = ((f.attrs[1]/2)<<16)|((f.attrs[2]/2)<<8)|(f.attrs[3]/2);
                        part_cone(f.o, dir, radius, angle, showentsize, 1, 1, colour);
                        break;
                    }
                    break;
                }
                case SUNLIGHT:
                {
                    int colour = ((e.attrs[2]/2)<<16)|((e.attrs[3]/2)<<8)|(e.attrs[4]/2),
                        offset = e.attrs[5] ? (e.attrs[5] > 0 ? e.attrs[5] : 0) : 10, yaw = e.attrs[0], pitch = e.attrs[1]+90;
                    vec dir(yaw*RAD, pitch*RAD);
                    static const float offsets[9][2] = { { 0, 0 }, { 0, 1 }, { 90, 1 }, { 180, 1 }, { 270, 1 }, { 45, 0.5f }, { 135, 0.5f }, { 225, 0.5f }, { 315, 0.5f } };
                    loopk(offset ? 9 : 1)
                    {
                        vec spoke(yaw*RAD, (pitch + offset*offsets[k][1])*RAD);
                        spoke.rotate(offsets[k][0]*RAD, dir);
                        float syaw, spitch;
                        vectoyawpitch(spoke, syaw, spitch);
                        entdirpart(e.o, syaw, spitch, getworldsize()*2, 1, colour);
                    }
                    break;
                }
                case AFFINITY:
                {
                    float radius = enttype[e.type].radius;
                    part_radius(e.o, vec(radius, radius, radius), showentsize, 1, 1, TEAM(e.attrs[0], colour));
                    radius = radius*2/3; // capture pickup dist
                    part_radius(e.o, vec(radius, radius, radius), showentsize, 1, 1, TEAM(e.attrs[0], colour));
                    break;
                }
                default:
                {
                    float radius = enttype[e.type].radius;
                    if((e.type == TRIGGER || e.type == TELEPORT || e.type == PUSHER || e.type == CHECKPOINT) && e.attrs[e.type == CHECKPOINT ? 0 : 3])
                        radius = e.attrs[e.type == CHECKPOINT ? 0 : 3];
                    if(radius > 0) part_radius(e.o, vec(radius, radius, radius), showentsize, 1, 1, colourcyan);
                    if(e.type == PUSHER && e.attrs[4] > 0 && e.attrs[4] < radius)
                        part_radius(e.o, vec(e.attrs[4], e.attrs[4], e.attrs[4]), showentsize, 1, 1, colourcyan);
                    break;
                }
            }
        }

        if(showentdir >= level)
        {
            switch(e.type)
            {
                case PLAYERSTART: case CHECKPOINT:
                {
                    entdirpart(e.o, e.attrs[1], e.attrs[2], 4.f, 1, TEAM(e.type == PLAYERSTART ? e.attrs[0] : T_NEUTRAL, colour));
                    break;
                }
                //case MAPMODEL:
                //{
                //    entdirpart(e.o, e.attrs[1], 360-e.attrs[3], 4.f, 1, colourcyan);
                //    break;
                //}
                case ACTOR:
                {
                    entdirpart(e.o, e.attrs[1], e.attrs[2], 4.f, 1, TEAM(T_ENEMY, colour));
                    break;
                }
                case TELEPORT:
                {
                    if(e.attrs[0] < 0) { entdirpart(e.o, (lastmillis/5)%360, e.attrs[1], 4.f, 1, colourcyan); }
                    else { entdirpart(e.o, e.attrs[0], e.attrs[1], 8.f, 1, colourcyan); }
                    break;
                }
                case PUSHER:
                {
                    entdirpart(e.o, e.attrs[0], e.attrs[1], 4.f+e.attrs[2], 1, colourcyan);
                    break;
                }
                default: break;
            }
        }
        if(enttype[e.type].links && showentlinks >= level) renderlinked(e, idx);
    }

    void renderentlight(gameentity &e)
    {
        if(e.o.squaredist(camera1->o) > showentdist*showentdist) return;
        adddynlight(vec(e.o), float(e.attrs[0] ? e.attrs[0] : hdr.worldsize)*0.75f, vec(e.attrs[1], e.attrs[2], e.attrs[3]).div(383.f), 0, 0, DL_KEEP);
    }

    void adddynlights()
    {
        if(game::player1->state == CS_EDITING && showlighting)
        {
            #define islightable(q) ((q)->type == LIGHT && (q)->attrs[0] > 0 && !(q)->links.length())
            loopv(entgroup)
            {
                int n = entgroup[i];
                if(ents.inrange(n) && islightable(ents[n]) && n != enthover)
                    renderfocus(n, renderentlight(e));
            }
            if(ents.inrange(enthover) && islightable(ents[enthover]))
                renderfocus(enthover, renderentlight(e));
        }
        loopenti(LIGHTFX) if(ents[i]->type == LIGHTFX && ents[i]->attrs[0] != LFX_SPOTLIGHT)
        {
            if(ents[i]->spawned() || ents[i]->lastemit)
            {
                if(!ents[i]->spawned() && ents[i]->lastemit > 0 && lastmillis-ents[i]->lastemit > triggertime(*ents[i])/2)
                    continue;
            }
            else
            {
                bool lonely = true;
                loopvk(ents[i]->links) if(ents.inrange(ents[i]->links[k]) && ents[ents[i]->links[k]]->type != LIGHT) { lonely = false; break; }
                if(!lonely) continue;
            }
            loopvk(ents[i]->links) if(ents.inrange(ents[i]->links[k]) && ents[ents[i]->links[k]]->type == LIGHT)
                makelightfx(*ents[i], *ents[ents[i]->links[k]]);
        }
    }

    void update()
    {
        loopenti(MAPSOUND)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.type == MAPSOUND && e.links.empty() && mapsounds.inrange(e.attrs[0]) && !issound(e.schan))
            {
                int flags = SND_MAP|SND_LOOP; // ambient sounds loop
                loopk(SND_LAST)  if(e.attrs[4]&(1<<k)) flags |= 1<<k;
                playsound(e.attrs[0], e.o, NULL, flags, e.attrs[3] ? e.attrs[3] : 255, e.attrs[1] || e.attrs[2] ? e.attrs[1] : -1, e.attrs[2] ? e.attrs[2] : -1, &e.schan);
            }
        }
        if((m_edit(game::gamemode) || m_race(game::gamemode)) && routeid >= 0 && droproute)
        {
            if(game::player1->state == CS_ALIVE)
            {   // don't start until the player begins moving
                if(lastroutenode >= 0 || game::player1->move || game::player1->strafe)
                {
                    const vec o = game::player1->feetpos();
                    int curnode = lastroutenode;
                    if(!ents.inrange(curnode) || ents[curnode]->o.dist(o) >= droproutedist)
                    {
                        curnode = -1;
                        loopenti(ROUTE) if(ents[i]->type == ROUTE && ents[i]->attrs[0] == routeid)
                        {
                            float dist = ents[i]->o.dist(o);
                            if(dist < droproutedist && (!ents.inrange(curnode) || dist < ents[curnode]->o.dist(o)))
                                curnode = i;
                        }
                    }
                    if(!ents.inrange(curnode))
                    {
                        attrvector attrs;
                        attrs.add(routeid);
                        attrs.add(int(game::player1->yaw));
                        attrs.add(int(game::player1->pitch));
                        attrs.add(game::player1->move);
                        attrs.add(game::player1->strafe);
                        attrs.add(0);
                        loopi(AC_MAX) if(game::player1->action[i] || (abs(game::player1->actiontime[i]) > lastroutetime))
                            attrs[5] |= (1<<i);
                        int n = newentity(o, int(ROUTE), attrs);
                        if(ents.inrange(lastroutenode)) ents[lastroutenode]->links.add(n);
                        curnode = n;
                        firstenttype[ROUTE] = min(firstenttype[ROUTE], n);
                        lastenttype[ROUTE] = max(lastenttype[ROUTE], n);
                        if(game::player1->airmillis) airnodes.add(n);
                    }
                    if(!game::player1->airmillis && !airnodes.empty()) airnodes.setsize(0);
                    if(lastroutenode != curnode) lastroutetime = lastmillis;
                    lastroutenode = curnode;
                }
            }
            else if(lastroutenode >= 0)
            {
                lastroutenode = -1;
                lastroutetime = 0;
                if(game::player1->state == CS_DEAD) loopv(airnodes) if(ents.inrange(airnodes[i])) ents[airnodes[i]]->type = ET_EMPTY;
                airnodes.setsize(0);
            }
        }
    }

    void render()
    {
        if(rendermainview && shouldshowents(game::player1->state == CS_EDITING ? 1 : (!entgroup.empty() || ents.inrange(enthover) ? 2 : 3))) loopv(ents) // important, don't render lines and stuff otherwise!
            renderfocus(i, renderentshow(e, i, game::player1->state == CS_EDITING ? ((entgroup.find(i) >= 0 || enthover == i) ? 1 : 2) : 3));
        if(!drawtex)
        {
            int frstent = m_edit(game::gamemode) ? 0 : firstuse(EU_ITEM),
                flstent = m_edit(game::gamemode) ? ents.length() : lastuse(EU_ITEM);
            for(int i = frstent; i < flstent; i++)
            {
                gameentity &e = *(gameentity *)ents[i];
                if(e.type <= NOTUSED || e.type >= MAXENTTYPES || (enttype[e.type].usetype == EU_ITEM && simpleitems)) continue;
                bool active = enttype[e.type].usetype == EU_ITEM && (e.spawned() || (e.lastemit && lastmillis-e.lastemit < 500));
                if((m_edit(game::gamemode) && rendermainview) || active)
                {
                    const char *mdlname = entmdlname(e.type, e.attrs);
                    vec pos = e.o;
                    if(mdlname && *mdlname)
                    {
                        int flags = MDL_SHADOW|MDL_CULL_VFC|MDL_CULL_DIST|MDL_CULL_OCCLUDED,
                            colour = -1;
                        float fade = 1, yaw = 0, pitch = 0, size = 1;
                        if(!active)
                        {
                            if(showentmodels <= (e.type == PLAYERSTART || e.type == ACTOR ? 1 : 0)) continue;
                            if(e.type == AFFINITY || e.type == PLAYERSTART)
                            {
                                yaw = e.attrs[1]+(e.type == PLAYERSTART ? 90 : 0);
                                colour = TEAM(e.attrs[0], colour);
                                pitch = e.attrs[2];
                            }
                            else if(e.type == ACTOR)
                            {
                                yaw = e.attrs[1]+90;
                                pitch = e.attrs[2];
                                int weap = e.attrs[6] > 0 ? e.attrs[6]-1 : AA(e.attrs[0], weaponspawn);
                                if(isweap(weap)) colour = W(weap, colour);
                                size = e.attrs[9] > 0 ? e.attrs[9]/100.f : AA(e.attrs[0], scale);
                            }
                        }
                        else if(e.spawned())
                        {
                            int millis = lastmillis-e.lastspawn;
                            if(millis < 500) size = fade = float(millis)/500.f;
                        }
                        else if(e.lastemit)
                        {
                            int millis = lastmillis-e.lastemit;
                            if(millis < 500) size = fade = 1.f-(float(millis)/500.f);
                        }
                        if(e.type == WEAPON)
                        {
                            int attr = w_attr(game::gamemode, game::mutators, e.type, e.attrs[0], m_weapon(game::focus->actortype, game::gamemode, game::mutators));
                            if(isweap(attr))
                            {
                                flags |= MDL_LIGHTFX;
                                int col = W(attr, colour), interval = lastmillis%1000;
                                e.light.effect = vec::hexcolor(col).mul(interval >= 500 ? (1000-interval)/500.f : interval/500.f);
                                if(attr == W_GRENADE) colour = col;
                            }
                            else continue;
                        }
                        else e.light.effect = vec(0, 0, 0);
                        e.light.material[0] = colour >= 0 ? bvec(colour) : bvec(colourwhite);
                        rendermodel(&e.light, mdlname, ANIM_MAPMODEL|ANIM_LOOP, pos, yaw, pitch, 0.f, flags, NULL, NULL, 0, 0, fade, size);
                    }
                }
            }
        }
    }

    void maketeleport(gameentity &e)
    {
        float yaw = e.attrs[0] < 0 ? (lastmillis/5)%360 : e.attrs[0], radius = float(e.attrs[3] ? e.attrs[3] : enttype[e.type].radius);
        int attr = int(e.attrs[4]), colour = (((attr&0xF)<<4)|((attr&0xF0)<<8)|((attr&0xF00)<<12))+0x0F0F0F;
        if(e.attrs[6] || e.attrs[7])
        {
            vec r = vec::hexcolor(colour).mul(game::getpalette(e.attrs[6], e.attrs[7]));
            colour = (int(r.x*255)<<16)|(int(r.y*255)<<8)|(int(r.z*255));
        }
        part_portal(e.o, radius, 1, yaw, e.attrs[1], PART_TELEPORT, 0, colour);
    }

    void drawparticle(gameentity &e, const vec &o, int idx, bool spawned, bool active, float skew)
    {
        switch(e.type)
        {
            case PARTICLES:
                if(idx >= 0 && e.attrs[11])
                {
                    if((e.nextemit -= curtime) <= 0) e.nextemit = 0;
                    if(e.nextemit) break;
                }
                if(idx < 0 || e.links.empty() || e.spawned() || (e.lastemit > 0 && lastmillis-e.lastemit <= triggertime(e)/2)) makeparticle(o, e.attrs);
                if(idx >= 0 && e.attrs[11]) e.nextemit += e.attrs[11];
                break;
            case TELEPORT:
                if(e.attrs[4]) maketeleport(e);
                break;
            case ROUTE:
            {
                if(e.attrs[0] != routeid || (!m_edit(game::gamemode) && !m_race(game::gamemode))) break;
                loopv(e.links) if(ents.inrange(e.links[i]) && ents[e.links[i]]->type == ROUTE && (!routemaxdist || e.o.dist(ents[e.links[i]]->o) <= routemaxdist))
                    part_flare(e.o, ents[e.links[i]]->o, 1, PART_LIGHTNING_FLARE, routecolour);
            }
            default: break;
        }

        vec off(0, 0, 2.f), pos(o);
        if(enttype[e.type].usetype == EU_ITEM) pos.add(off);
        bool edit = m_edit(game::gamemode) && cansee(idx), isedit = edit && game::player1->state == CS_EDITING,
             hasent = isedit && idx >= 0 && (enthover == idx || entgroup.find(idx) >= 0),
             hastop = hasent && e.o.squaredist(camera1->o) <= showentdist*showentdist;
        int sweap = m_weapon(game::focus->actortype, game::gamemode, game::mutators),
            attr = w_attr(game::gamemode, game::mutators, e.type, e.attrs[0], sweap),
            colour = e.type == WEAPON && isweap(attr) ? W(attr, colour) : colourwhite, interval = lastmillis%1000;
        float fluc = interval >= 500 ? (1500-interval)/1000.f : (500+interval)/1000.f;
        if(enttype[e.type].usetype == EU_ITEM && (active || isedit))
        {
            float blend = fluc*skew, radius = max(((e.type == WEAPON ? weaptype[attr].halo : enttype[e.type].radius*0.5f)+(fluc*0.5f))*skew, 0.125f);
            if(simpleitems == 1)
            {
                part_icon(o, textureload(hud::itemtex(e.type, attr), 3), simpleitemsize*skew, simpleitemblend*skew, 0, 0, 1, colour);
                if(radius < simpleitemsize*skew) radius = simpleitemsize*skew;
                blend *= simpleitemhalo;
            }
            else
            {
                radius *= haloitemsize;
                blend *= haloitemblend;
            }
            vec offset = vec(o).sub(camera1->o).rescale(radius/2);
            offset.z = max(offset.z, -1.0f);
            part_create(PART_HINT_BOLD_SOFT, 1, offset.add(o), colour, radius, blend);
        }
        if(isedit ? (showentinfo&(hasent ? 1 : 2)) : (enttype[e.type].usetype == EU_ITEM && active && showentdescs >= 3))
        {
            const char *itxt = entinfo(e.type, e.attrs, isedit);
            if(itxt && *itxt)
            {
                defformatstring(ds, "<emphasis>%s", itxt);
                part_textcopy(pos.add(off), ds, hastop ? PART_TEXT_ONTOP : PART_TEXT, 1, colourwhite);
            }
        }
        if(edit)
        {
            part_create(hastop ? PART_EDIT_ONTOP : PART_EDIT, 1, o, hastop ? colourviolet : colourdarkviolet, hastop ? 2.f : 1.f);
            if(showentinfo&(hasent ? 4 : 8))
            {
                defformatstring(s, "<super>%s%s (%d)", hastop ? "\fp" : "\fv", enttype[e.type].name, idx >= 0 ? idx : 0);
                part_textcopy(pos.add(off), s, hastop ? PART_TEXT_ONTOP : PART_TEXT);
            }
            if(showentinfo&(hasent ? 16 : 32)) loopk(enttype[e.type].numattrs)
            {
                const char *attrname = enttype[e.type].attrs[k];
                if(e.type == PARTICLES && k) switch(e.attrs[0])
                {
                    case 0: switch(k) { case 1: attrname = "length"; break; case 2: attrname = "height"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "palette"; break; case 6: attrname = "palindex"; break; case 7: attrname = "size"; break; case 8: attrname = "blend"; break; case 9: attrname = "gravity"; break; case 10: attrname = "velocity"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 1: switch(k) { case 1: attrname = "dir"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 2: switch(k) { case 1: attrname = "dir"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 3: switch(k) { case 1: attrname = "size"; break; case 2: attrname = "colour"; break; case 3: attrname = "palette"; break; case 4: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 4: case 7: switch(k) { case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "palette"; break; case 7: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; break; } break;
                    case 8: case 9: case 10: case 11: case 12: case 13: switch(k) { case 1: attrname = "dir"; break; case 2: attrname = "length"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "decal"; break; case 7: attrname = "gravity"; break; case 8: attrname = "velocity"; break; case 9: attrname = "palette"; break; case 10: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; break; } break;
                    case 14: case 15: switch(k) { case 1: attrname = "radius"; break; case 2: attrname = "height"; break; case 3: attrname = "colour"; break; case 4: attrname = "fade"; break; case 5: attrname = "size"; break; case 6: attrname = "gravity"; break; case 7: attrname = "velocity"; break; case 8: attrname = "palette"; break; case 9: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 6: switch(k) { case 1: attrname = "amt"; break; case 2: attrname = "colour"; break; case 3: attrname = "colour2"; break; case 4: attrname = "palette1"; break; case 5: attrname = "palindex1"; break; case 6: attrname = "palette2"; break; case 7: attrname = "palindex2"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 5: switch(k) { case 1: attrname = "amt"; break; case 2: attrname = "colour"; break; case 3: attrname = "palette"; break; case 4: attrname = "palindex"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    case 32: case 33: case 34: case 35: switch(k) { case 1: attrname = "red"; break; case 2: attrname = "green"; break; case 3: attrname = "blue"; break; case 11: attrname = "millis"; break; default: attrname = ""; } break;
                    default: attrname = ""; break;
                }
                if(attrname && *attrname)
                {
                    string attrval; attrval[0] = 0;
                    if(showentattrinfo&1)
                    {
                        defformatstring(s, "\fs\fy%d\fS:", k+1);
                        concatstring(attrval, s);
                    }
                    if(showentattrinfo&2)
                    {
                        if(*attrval) concatstring(attrval, " ");
                        concatstring(attrval, attrname);
                    }
                    if(showentattrinfo&4)
                    {
                        if(*attrval) concatstring(attrval, " = ");
                        defformatstring(s, "\fs\fc%d\fS", e.attrs[k]);
                        concatstring(attrval, s);
                    }
                    defformatstring(s, "%s%s", hastop ? "\fw" : "\fa", attrval);
                    part_textcopy(pos.add(off), s, hastop ? PART_TEXT_ONTOP : PART_TEXT);
                }
            }
        }
    }

    void drawparticles()
    {
        float maxdist = float(maxparticledistance)*float(maxparticledistance);
        int frstent = m_edit(game::gamemode) ? 0 : min(firstuse(EU_ITEM), min(firstent(PARTICLES), firstent(TELEPORT))),
            flstent = m_edit(game::gamemode) ? ents.length() : max(lastuse(EU_ITEM), max(lastent(PARTICLES), lastent(TELEPORT)));
        bool hasroute = (m_edit(game::gamemode) || m_race(game::gamemode)) && routeid >= 0;
        if(hasroute)
        {
            frstent = min(frstent, firstent(ROUTE));
            flstent = max(flstent, lastent(ROUTE));
        }
        for(int i = frstent; i < flstent; ++i)
        {
            gameentity &e = *(gameentity *)ents[i];
            if(e.type == NOTUSED || e.attrs.empty()) continue;
            if(e.type != PARTICLES && e.type != TELEPORT && e.type != ROUTE && !m_edit(game::gamemode) && enttype[e.type].usetype != EU_ITEM) continue;
            else if(e.o.squaredist(camera1->o) > maxdist) continue;
            float skew = 1;
            bool active = false;
            if(e.spawned())
            {
                int millis = lastmillis-e.lastspawn;
                if(millis < 500) skew = float(millis)/500.f;
                active = true;
            }
            else if(e.lastemit)
            {
                int millis = lastmillis-e.lastemit;
                if(millis < 500)
                {
                    skew = 1.f-(float(millis)/500.f);
                    active = true;
                }
            }
            drawparticle(e, e.o, i, e.spawned(), active, skew);
        }
        loopv(projs::projs)
        {
            projent &proj = *projs::projs[i];
            if(proj.projtype != PRJ_ENT || !ents.inrange(proj.id) || !proj.ready()) continue;
            gameentity &e = *(gameentity *)ents[proj.id];
            if(e.type == NOTUSED || e.attrs.empty()) continue;
            float skew = 1;
            if(proj.fadetime && proj.lifemillis)
            {
                int interval = min(proj.lifemillis, proj.fadetime);
                if(proj.lifetime < interval) skew = float(proj.lifetime)/float(interval);
                else if(proj.lifemillis > interval)
                {
                    interval = min(proj.lifemillis-interval, proj.fadetime);
                    if(proj.lifemillis-proj.lifetime < interval) skew = float(proj.lifemillis-proj.lifetime)/float(interval);
                }
            }
            drawparticle(e, proj.o, -1, true, true, skew);
        }
    }
}
