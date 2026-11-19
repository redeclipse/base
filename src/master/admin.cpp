#include "engine.h"
#include "master.h"

#define ADMIN_LISTMAX 1000

namespace master
{
    // the account management the master offers its administrators over the Web API, as the master has no console

    VAR(0, masteradminlock, 0, PRIV_ADMINISTRATOR, PRIV_MAX); // Privilege that may manage accounts, server accounts and control entries, PRIV_MAX for nobody


    // an account's Privilege, as the game reads its flags
    int accountpriv(const char *flags)
    {
        int best = PRIV_NONE;
        for(const char *c = flags ? flags : ""; *c; c++)
        {
            int n = PRIV_NONE;
            switch(*c)
            {
                case 'c': case 'C': n = PRIV_FOUNDER; break;
                case 'd': case 'D': n = PRIV_DEVELOPER; break;
                case 'a': case 'A': n = PRIV_ADMINISTRATOR; break;
                case 'o': case 'O': case 'm': case 'M': n = PRIV_MODERATOR; break;
                case 's': case 'S': n = PRIV_SUPPORTER; break;
                case 'u': case 'U': n = PRIV_PLAYER; break;
            }
            best = max(best, n);
        }
        return best;
    }

    // the administrator a request is from, or NULL with the response already set
    signin *adminsignin(httprequest &req, httpresponse &res)
    {
        if(!signinready(req, res)) return NULL;
        bool tried = false;
        signin *s = findsignin(req, tried);
        if(signinrefused(req, res, s, tried)) return NULL;
        if(accountpriv(s->flags) < masteradminlock) { res.error(403, "your account can't manage the master"); return NULL; }
        return s;
    }

    // whether an administrator at one Privilege may change an account at another, or give it one
    bool adminoutranks(int priv, int other) { return priv >= PRIV_LAST || other < priv; }

    bool removeauth(hashnameset<authuser> &users, const char *name)
    {
        authuser *u = users.access(name);
        if(!u) return false;
        authuser copy = *u;
        users.remove(name);
        freeauth(copy);
        return true;
    }

    authuser *findowner(const char *name)
    {
        if(!name || !*name) return NULL;
        authuser *u = authusers.access(name);
        if(!u) enumerate(authusers, authuser, a, if(!u && !strcasecmp(a.name, name)) u = &a);
        return u;
    }

    void writeauth(jsonwriter &w, authuser &u, bool server, time_t newkey)
    {
        w.beginobject();
        w.utf8("handle", u.name);
        w.utf8("flags", u.flags);
        if(server)
        {
            w.str("desc", u.desc ? u.desc : "");
            w.utf8("owner", u.owner ? u.owner : "");
            if(u.owner && *u.owner && !findowner(u.owner)) w.boolean("ownergone", true);
            // only to administrators, as owners get theirs from writeowner
            if(u.addrs)
            {
                vector<knownaddr> addrs;
                readknownaddrs(u.addrs, addrs);
                w.beginarray("addresses");
                loopv(addrs)
                {
                    w.beginobject();
                    w.str("host", addrs[i].host);
                    w.num("seen", double(addrs[i].seen));
                    w.endobject();
                }
                w.endarray();
            }
        }
        else
        {
            w.utf8("email", u.email ? u.email : "");
            w.utf8("steamid", u.steamid ? u.steamid : "");
            w.utf8("discord", u.discord ? u.discord : "");
            w.str("displayname", u.display ? u.display : "");
        }
        w.utf8("pubkey", u.key);
        if(newkey) w.num("newkey", double(newkey));
        w.endobject();
    }

    static bool adminmatch(authuser &u, const char *q)
    {
        return !q || !*q || strstr(u.name, q) || (u.email && strstr(u.email, q)) || (u.desc && strstr(u.desc, q)) || (u.owner && strstr(u.owner, q));
    }

    // GET /signin/admin/accounts or /signin/admin/servers, ?q= for handles or emails holding it
    static void routeadminlist(httprequest &req, httpresponse &res, bool server)
    {
        if(!adminsignin(req, res)) return;
        const char *q = req.var("q");
        int limit = clamp(atoi(req.var("limit", "100")), 1, ADMIN_LISTMAX), total = 0;
        hashnameset<authuser> &users = server ? serverauthusers : authusers;
        vector<authuser *> found;
        enumerate(users, authuser, u, if(adminmatch(u, q)) { total++; found.add(&u); });
        found.sort([](const authuser *a, const authuser *b) { return strcmp(a->name, b->name) < 0; });
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.num("total", total);
        w.beginarray(server ? "servers" : "accounts");
        loopv(found) { if(i >= limit) break; writeauth(w, *found[i], server); }
        w.endarray();
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }
    static void routeadminaccounts(httprequest &req, httpresponse &res) { routeadminlist(req, res, false); }

    VAR(0, masterdisplaylock, 0, PRIV_MODERATOR, PRIV_MAX); // Privilege that may clear or change accounts' display names

    void setdisplay(authuser &u, const char *shown)
    {
        delete[] u.display;
        u.display = newstring(shown);
        dirtymasterfile(MFILE_USER);
    }

    // PATCH /signin/admin/accounts/<handle> with new flags, or a displayname to put in place of the owner's ("" clears it)
    static void routeadminaccountset(httprequest &req, httpresponse &res)
    {
        if(!signinready(req, res)) return;
        bool tried = false;
        signin *s = findsignin(req, tried);
        if(signinrefused(req, res, s, tried)) return;
        int priv = accountpriv(s->flags);
        const char *name = req.param("handle");
        authuser *u = name ? authusers.access(name) : NULL;
        jsonvalue *v = signinbody(req);
        const char *flags = signinfield(req, v, "flags"), *display = signinfield(req, v, "displayname");
        if(priv < min(masteradminlock, masterdisplaylock) || (flags && priv < masteradminlock) || (display && priv < masterdisplaylock)) res.error(403, "your account can't manage the master");
        else if(!u) res.error(404, "no such account");
        else if(!flags && !display) res.error(400, "send the account's new flags or displayname");
        else if(!strcmp(u->name, s->name)) res.error(403, "you can't change your own account here");
        else if(!adminoutranks(priv, accountpriv(u->flags))) res.error(403, "that account's standing is at or above yours");
        else if(flags && (!*flags || strspn(flags, "uUsSmMoOaAdDcC") != strlen(flags) || strlen(flags) > 16)) res.error(400, "flags are letters from u, s, m, o, a, d and c");
        else if(flags && !adminoutranks(priv, accountpriv(flags))) res.error(403, "you can't give a standing at or above your own");
        else
        {
            if(flags)
            {
                char flagbuf[2] = { userflag(flags), '\0' };
                noticef("account", NOTICE_INFO, "Account '%s'%s had its flags changed from '%s' to '%s' by '%s'", u->name, accountlinks(*u), u->flags, flagbuf, s->name);
                delete[] u->flags;
                u->flags = newstring(flagbuf);
                dirtymasterfile(MFILE_USER);
                signincheck = true; // its sign-ins carry the old flags
            }
            if(display)
            {
                string shown;
                displayfromutf8(shown, display);
                noticef("account", NOTICE_INFO, "Account '%s' had its display name changed from '%s' to '%s' by '%s'", u->name, u->display, shown, s->name);
                setdisplay(*u, shown);
            }
            vector<char> buf;
            jsonwriter w(buf);
            writeauth(w, *u, false);
            res.json(buf.getbuf(), buf.length());
        }
        DELETEP(v);
    }

    // DELETE /signin/admin/accounts/<handle>
    static void routeadminaccountdel(httprequest &req, httpresponse &res)
    {
        signin *s = adminsignin(req, res);
        if(!s) return;
        const char *name = req.param("handle");
        authuser *u = name ? authusers.access(name) : NULL;
        if(!u) { res.error(404, "no such account"); return; }
        if(!strcmp(u->name, s->name)) { res.error(403, "you can't remove your own account here"); return; }
        if(!adminoutranks(accountpriv(s->flags), accountpriv(u->flags))) { res.error(403, "that account's standing is at or above yours"); return; }
        string handle, by, links, flags;
        copystring(handle, u->name);
        copystring(by, s->name);
        copystring(links, accountlinks(*u));
        copystring(flags, u->flags);
        removeauth(authusers, handle);
        dirtymasterfile(MFILE_USER);
        signincheck = true;
        noticef("account", NOTICE_INFO, "Account '%s' [%s]%s was removed by '%s'", handle, flags, links, by);
        res.status = 204;
    }


    void controladdr(const ipinfo &c, string &addr)
    {
        union { uchar b[sizeof(enet_uint32)]; enet_uint32 i; } ip, mask;
        ip.i = c.ip;
        mask.i = c.mask;
        addr[0] = '\0';
        loopj(4)
        {
            if(mask.b[j] != 0xFF) break;
            concformatstring(addr, "%s%d", j ? "." : "", ip.b[j]);
        }
    }

    static void writecontrol(jsonwriter &w, const ipinfo &c)
    {
        string addr;
        controladdr(c, addr);
        w.beginobject();
        w.num("id", c.id);
        w.str("type", ipinfotypes[clamp(c.type, 0, int(ipinfo::MAXTYPES)-1)]);
        w.str("address", addr);
        w.str("reason", c.reason ? c.reason : "");
        if(c.source >= 0 && c.source < ipinfo::SRC_MAX) w.str("source", controlsources[c.source]);
        if(c.addhandle[0]) w.str("by", c.addhandle);
        if(c.added > 0) w.num("added", c.added);
        w.endobject();
    }

    // GET /signin/admin/controls: the master's own entries, which it sends to every linked server
    static void routeadmincontrols(httprequest &req, httpresponse &res)
    {
        if(!adminsignin(req, res)) return;
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.beginarray("controls");
        loopv(control) if(control[i].flag == ipinfo::LOCAL) writecontrol(w, control[i]);
        w.endarray();
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }

    // POST /signin/admin/controls with a type, address and reason
    static void routeadmincontroladd(httprequest &req, httpresponse &res)
    {
        signin *s = adminsignin(req, res);
        if(!s) return;
        jsonvalue *v = signinbody(req);
        const char *type = signinfield(req, v, "type"), *addr = signinfield(req, v, "address"), *reason = signinfield(req, v, "reason");
        int ctype = -1;
        if(type) loopi(ipinfo::MAXTYPES) if(!strcmp(type, ipinfotypes[i])) { ctype = i; break; }
        enet_uint32 ip = 0, mask = 0;
        if(ctype < 0) res.error(400, "the type is one of allow, ban, mute, limit, except or trust");
        else if(!addr || !*addr || !parsecontroladdr(addr, ip, mask)) res.error(400, "the address is up to four numbers with dots, such as 1.2.3.4 or 1.2 for a range");
        else
        {
            int found = -1;
            loopv(control) if(control[i].flag == ipinfo::LOCAL && control[i].type == ctype && control[i].ip == ip && control[i].mask == mask) { found = i; break; }
            if(found >= 0) res.error(409, "that entry is already there");
            else
            {
                string why;
                filterstring(why, reason ? reason : "", true, true, true, true, false, MAXSTRLEN-1);
                addipinfo(control, ctype, addr, why);
                ipinfo &c = control.last();
                c.source = ipinfo::SRC_PLAYER;
                copystring(c.addhandle, s->name);
                noticef("control", NOTICE_INFO, "Control entry %s %s was added by '%s': %s", ipinfotypes[ctype], addr, s->name, why[0] ? why : "no reason given");
                vector<char> buf;
                jsonwriter w(buf);
                writecontrol(w, c);
                res.status = 201;
                res.json(buf.getbuf(), buf.length());
            }
        }
        DELETEP(v);
    }

    // DELETE /signin/admin/controls/<id>
    static void routeadmincontroldel(httprequest &req, httpresponse &res)
    {
        signin *s = adminsignin(req, res);
        if(!s) return;
        int id = atoi(req.param("id", "0"));
        loopv(control) if(control[i].flag == ipinfo::LOCAL && control[i].id == id)
        {
            ipinfo &c = control[i];
            string addr, origin = "";
            controladdr(c, addr);
            if(c.source == ipinfo::SRC_PLAYER && c.addhandle[0]) formatstring(origin, ", added by '%s'", c.addhandle);
            else if(c.source == ipinfo::SRC_AUTO) copystring(origin, ", added by Master shield");
            else if(c.source >= 0 && c.source < ipinfo::SRC_MAX) formatstring(origin, ", added by the %s", controlsources[c.source]);
            if(c.added > 0) concformatstring(origin, " on %s", noticedate(c.added));
            noticef("control", NOTICE_INFO, "Control entry %s %s was removed by '%s'%s: %s", ipinfotypes[clamp(c.type, 0, int(ipinfo::MAXTYPES)-1)], addr, s->name, origin, c.reason && *c.reason ? c.reason : "no reason given");
            control.remove(i);
            nextcontrolversion(); // linked servers get the list again, and the file is saved
            res.status = 204;
            return;
        }
        res.error(404, "no such entry");
    }

    // Master shield on the website, at mastershieldlock: its settings and modulators, and a test of a body against the list
    static signin *shieldsignin(httprequest &req, httpresponse &res)
    {
        if(!signinready(req, res)) return NULL;
        bool tried = false;
        signin *s = findsignin(req, tried);
        if(signinrefused(req, res, s, tried)) return NULL;
        if(accountpriv(s->flags) < mastershieldlock) { res.error(403, "your account can't manage Master shield"); return NULL; }
        return s;
    }

    void writeshieldmodulator(jsonwriter &w, shieldmodulator &m)
    {
        w.beginobject();
        w.num("id", m.id);
        w.str("name", m.name);
        w.str("link", m.link);
        w.boolean("enabled", m.enabled);
        w.str("body", m.body ? m.body : "");
        w.endobject();
    }

    void shieldchanged()
    {
        dirtymasterfile(MFILE_SHIELD);
        shield.rescan = true;
    }

    // GET /signin/admin/shield
    static void routeadminshield(httprequest &req, httpresponse &res)
    {
        if(!shieldsignin(req, res)) return;
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.beginobject("settings");
        loopi(numshieldsettings) w.num(shieldsettings[i], getvar(shieldsettings[i]));
        w.endobject();
        w.beginarray("modulators");
        loopv(shield.modulators) writeshieldmodulator(w, *shield.modulators[i]);
        w.endarray();
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }

    // PATCH /signin/admin/shield with any of the settings
    static void routeadminshieldsettings(httprequest &req, httpresponse &res)
    {
        signin *s = shieldsignin(req, res);
        if(!s) return;
        jsonvalue *v = signinbody(req);
        if(!v || v->type != JSON_OBJECT) { DELETEP(v); res.error(400, "send the settings to change as JSON"); return; }
        string changes = "";
        loopi(numshieldsettings)
        {
            jsonvalue *n = v->get(shieldsettings[i]);
            if(!n) continue;
            if(n->type != JSON_NUMBER && n->type != JSON_TRUE && n->type != JSON_FALSE) { DELETEP(v); res.error(400, tempformatstring("%s is a number", shieldsettings[i])); return; }
            int val = n->type == JSON_NUMBER ? int(n->num) : (n->istrue() ? 1 : 0);
            if(val == getvar(shieldsettings[i])) continue;
            setvar(shieldsettings[i], val, true, false, true);
            concformatstring(changes, "%s%s %d", changes[0] ? ", " : "", shieldsettings[i], getvar(shieldsettings[i]));
        }
        DELETEP(v);
        if(changes[0])
        {
            shieldchanged();
            noticef("shield", NOTICE_INFO, "Master shield's settings were changed by '%s': %s", s->name, changes);
        }
        routeadminshield(req, res);
    }

    // what a modulator's fields from the website say, checked; NULL when they're good, otherwise why not
    static const char *shieldcheckfields(jsonvalue *v, shieldmodulator *m, char *name, const char *&link, const char *&body)
    {
        const char *n = v->getstr("name");
        link = v->getstr("link");
        body = v->getstr("body");
        if(n || !m)
        {
            if(!n || !shieldname(name, n)) return "a modulator's name is up to 64 characters of plain text";
            if(shield.find(name, m)) return "another modulator has that name";
        }
        else copystring(name, m->name, MAXSTRLEN);
        if(link && !shieldlink(link)) return "a link is to a GitHub issue or security advisory, such as https://github.com/redeclipse/base/issues/1";
        if(!body && !m) return "send its body";
        if(body && strlen(body) > SHIELD_BODYLEN) return "its body is too long";
        return NULL;
    }

    static void shieldmodulatorjson(httpresponse &res, shieldmodulator &m, int status)
    {
        vector<char> buf;
        jsonwriter w(buf);
        writeshieldmodulator(w, m);
        res.status = status;
        res.json(buf.getbuf(), buf.length());
    }

    // POST /signin/admin/shield/modulators with its name, link, body and whether it's on
    static void routeadminshieldadd(httprequest &req, httpresponse &res)
    {
        signin *s = shieldsignin(req, res);
        if(!s) return;
        jsonvalue *v = signinbody(req);
        string name;
        const char *link = NULL, *body = NULL, *why = v && v->type == JSON_OBJECT ? shieldcheckfields(v, NULL, name, link, body) : "send the modulator as JSON";
        if(why) res.error(400, why);
        else if(shield.modulators.length() >= SHIELD_MODULATORS) res.error(409, "there are as many modulators as there can be");
        else
        {
            bool enabled = signinflag(req, v, "enabled", true);
            shieldmodulator *m = addshieldmodulator(name, link ? link : "", enabled, body);
            noticef("shield", NOTICE_INFO, "Shield modulator '%s' was added%s by '%s'", m->name, enabled ? "" : ", turned off,", s->name);
            shieldchanged();
            shieldmodulatorjson(res, *m, 201);
        }
        DELETEP(v);
    }

    // PATCH /signin/admin/shield/modulators/<id> with any of its name, link, body and whether it's on
    static void routeadminshieldset(httprequest &req, httpresponse &res)
    {
        signin *s = shieldsignin(req, res);
        if(!s) return;
        shieldmodulator *m = shield.find(atoi(req.param("id", "0")));
        if(!m) { res.error(404, "no such modulator"); return; }
        jsonvalue *v = signinbody(req);
        string name;
        const char *link = NULL, *body = NULL, *why = v && v->type == JSON_OBJECT ? shieldcheckfields(v, m, name, link, body) : "send the changes as JSON";
        if(why) { DELETEP(v); res.error(400, why); return; }
        jsonvalue *on = v->get("enabled");
        string changes = "";
        if(strcmp(name, m->name)) { concformatstring(changes, "renamed from '%s'", m->name); copystring(m->name, name); }
        if(link && strcmp(link, m->link)) { concatstring(changes, changes[0] ? ", link changed" : "link changed"); copystring(m->link, link); }
        if(body && strcmp(body, m->body ? m->body : "")) { concatstring(changes, changes[0] ? ", changed" : "changed"); DELETEA(m->body); m->body = newstring(body); }
        if(on && on->istrue() != m->enabled) { m->enabled = on->istrue(); concatstring(changes, m->enabled ? (changes[0] ? ", turned on" : "turned on") : (changes[0] ? ", turned off" : "turned off")); }
        DELETEP(v);
        if(changes[0])
        {
            noticef("shield", NOTICE_INFO, "Shield modulator '%s' was %s by '%s'", m->name, changes, s->name);
            shieldchanged();
        }
        shieldmodulatorjson(res, *m, 200);
    }

    // DELETE /signin/admin/shield/modulators/<id>
    static void routeadminshielddel(httprequest &req, httpresponse &res)
    {
        signin *s = shieldsignin(req, res);
        if(!s) return;
        int id = atoi(req.param("id", "0"));
        loopv(shield.modulators) if(shield.modulators[i]->id == id)
        {
            noticef("shield", NOTICE_INFO, "Shield modulator '%s' was removed by '%s'", shield.modulators[i]->name, s->name);
            delete shield.modulators.remove(i);
            shieldchanged();
            res.status = 204;
            return;
        }
        res.error(404, "no such modulator");
    }

    // POST /signin/admin/shield/test with a body: what it would do to each listed server, doing nothing
    static void routeadminshieldtest(httprequest &req, httpresponse &res)
    {
        if(!shieldsignin(req, res)) return;
        jsonvalue *v = signinbody(req);
        const char *body = v ? v->getstr("body") : NULL;
        if(!body) { DELETEP(v); res.error(400, "send the body to test"); return; }
        if(strlen(body) > SHIELD_BODYLEN) { DELETEP(v); res.error(400, "its body is too long"); return; }
        char *code = newstring(body);
        DELETEP(v);
        static const char * const actions[] = { "", "ban", "flag", "bad" };
        clearsandboxdenials();
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        int tested = 0;
        w.beginarray("results");
        loopv(masterclients)
        {
            masterclient &c = *masterclients[i];
            if(!c.listserver) continue;
            tested++;
            string text;
            int action = shieldrun(code, c, text);
            if(action == SHIELD_NONE) continue;
            w.beginobject();
            w.str("host", c.name);
            w.num("port", c.port);
            w.str("desc", c.desc);
            w.str("map", c.lastmap);
            w.str("action", actions[clamp(action, 0, 3)]);
            w.str("text", text);
            w.endobject();
        }
        w.endarray();
        w.num("tested", tested);
        w.endobject();
        delete[] code;
        clearsandboxdenials();
        res.json(buf.getbuf(), buf.length());
    }

    // POST /signin/admin/mail with an address, to check the master can send mail
    static void routeadminmail(httprequest &req, httpresponse &res)
    {
        signin *s = adminsignin(req, res);
        if(!s) return;
        jsonvalue *v = signinbody(req);
        const char *to = signinfield(req, v, "to");
        defformatstring(text, "This is a test from the Red Eclipse master server, asked for by %s.\n", s->name);
        tagval args[1];
        args[0].setcstr(s->name);
        if(!to || !validemail(to)) res.error(400, "send an address to mail");
        else if(!queuemaildef("test", to, "Red Eclipse master server test", text, args, 1)) res.error(503, "the mail queue is full");
        else res.status = 202;
        DELETEP(v);
    }

    void routeadminservers(httprequest &req, httpresponse &res)
    {
        if(!adminsignin(req, res)) return;
        const char *q = req.var("q");
        int limit = clamp(atoi(req.var("limit", "100")), 1, ADMIN_LISTMAX), total = 0, shown = 0;
        vector<authuser *> found;
        enumerate(serverauthusers, authuser, u, if(adminmatch(u, q)) { total++; found.add(&u); });
        found.sort([](const authuser *a, const authuser *b) { return strcmp(a->name, b->name) < 0; });
        vector<ticket *> waiting;
        vector<ticket *> &tickets = accounttickets();
        loopv(tickets)
        {
            ticket &t = *tickets[i];
            if(t.kind == TICKET_SERVER && t.level[0] && (!q || !*q || strstr(t.handle, q) || strstr(t.owner, q) || strstr(t.desc, q))) { total++; waiting.add(&t); }
        }
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.num("total", total);
        w.beginarray("servers");
        loopv(waiting) { if(shown++ >= limit) break; writependingserver(w, *waiting[i]); }
        loopv(found)
        {
            if(shown++ >= limit) break;
            ticket *t = serverticket(found[i]->name);
            writeauth(w, *found[i], true, t && !t->level[0] ? t->expires : 0); // with when a new key's link runs out
        }
        w.endarray();
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }

    // POST /signin/admin/servers with a handle, its level as flags, the owner's handle, and an optional desc: the owner is mailed a link for its key
    void routeadminserveradd(httprequest &req, httpresponse &res)
    {
        signin *s = adminsignin(req, res);
        if(!s) return;
        jsonvalue *v = signinbody(req);
        const char *name = signinfield(req, v, "handle"), *flags = signinfield(req, v, "flags"), *ownername = signinfield(req, v, "owner"), *desc = signinfield(req, v, "desc");
        string handle, clean;
        descfromutf8(clean, desc);
        char level[2] = { serverflag(flags), '\0' };
        authuser *owner = findowner(ownername);
        if(!name || !parsehandle(name, handle)) res.error(400, "a server's handle is 3 to 24 letters, numbers, - or _");
        else if(!adminoutranks(accountpriv(s->flags), accountpriv(level))) res.error(403, "you can't give a level at or above your own standing");
        else if(serverauthusers.access(handle) || serverticket(handle)) res.error(409, "that handle already has a server account");
        else if(!owner) res.error(404, "there's no account with the owner's handle");
        else if(!owner->email || !validemail(owner->email)) res.error(409, "the owner's account has no email address to send the link to");
        else
        {
            ticket *t = serverlink(handle, owner->name, owner->email, level, clean, s->name);
            noticef("serveraccount", NOTICE_INFO, "Server account '%s' [%s] was made for '%s' by '%s', waiting for them to activate it", handle, level, owner->name, s->name);
            vector<char> buf;
            jsonwriter w(buf);
            writependingserver(w, *t);
            res.status = 202;
            res.json(buf.getbuf(), buf.length());
        }
        DELETEP(v);
    }

    // PATCH /signin/admin/servers/<handle> with its level as flags, which servers signed in with it get straight away, its owner's handle, and/or desc:
    // one still waiting has its link sent to the new owner instead, and a new key's link to the old owner stops working
    void routeadminserverset(httprequest &req, httpresponse &res)
    {
        signin *s = adminsignin(req, res);
        if(!s) return;
        const char *name = req.param("handle");
        authuser *u = name ? serverauthusers.access(name) : NULL;
        ticket *t = name ? serverticket(name) : NULL;
        jsonvalue *v = signinbody(req);
        const char *flags = signinfield(req, v, "flags"), *ownername = signinfield(req, v, "owner"), *desc = signinfield(req, v, "desc");
        authuser *owner = ownername ? findowner(ownername) : NULL;
        int priv = accountpriv(s->flags);
        const char *now = u ? u->flags : (t ? t->level : "");
        char level[2] = { serverflag(flags ? flags : now), '\0' };
        string clean;
        if(desc) descfromutf8(clean, desc);
        if(!u && !(t && t->level[0])) res.error(404, "no such server account");
        else if(!flags && !ownername && !desc) res.error(400, "send the server account's level as flags, b, u or m, its owner's handle, or desc");
        else if(!adminoutranks(priv, accountpriv(now)) || !adminoutranks(priv, accountpriv(level))) res.error(403, "you can't change a level at or above your own standing");
        else if(ownername && !owner) res.error(404, "there's no account with the owner's handle");
        else if(ownername && !u && (!owner->email || !validemail(owner->email))) res.error(409, "the owner's account has no email address to send the link to");
        else if(!u)
        {
            if(owner && strcmp(owner->name, t->owner))
            {
                noticef("serveraccount", NOTICE_INFO, "The waiting server account '%s' was given to '%s' by '%s', its link sent to them instead of '%s'", t->handle, owner->name, s->name, t->owner);
                string handle, shown;
                copystring(handle, t->handle);
                copystring(shown, desc ? clean : t->desc);
                t = serverlink(handle, owner->name, owner->email, level, shown, s->name);
            }
            else
            {
                if(flags) copystring(t->level, level);
                if(desc) copystring(t->desc, clean);
                dirtyaccounts();
            }
            vector<char> buf;
            jsonwriter w(buf);
            writependingserver(w, *t);
            res.json(buf.getbuf(), buf.length());
        }
        else
        {
            bool changed = false;
            if(strcmp(u->flags, level))
            {
                noticef("serveraccount", NOTICE_INFO, "Server account '%s'%s had its level changed from '%s' to '%s' by '%s'", u->name, knownlast(*u), u->flags, level, s->name);
                delete[] u->flags;
                u->flags = newstring(level);
                loopv(masterclients) if(masterclients[i]->authhandle[0] && !strcmp(masterclients[i]->authhandle, u->name)) copystring(masterclients[i]->flags, level);
                changed = true;
            }
            if(desc && strcmp(u->desc ? u->desc : "", clean))
            {
                noticef("serveraccount", NOTICE_INFO, "Server account '%s'%s had its description changed from '%s' to '%s' by '%s'", u->name, knownlast(*u), u->desc ? u->desc : "", clean, s->name);
                delete[] u->desc;
                u->desc = newstring(clean);
                changed = true;
            }
            if(owner && strcmp(owner->name, u->owner ? u->owner : ""))
            {
                noticef("serveraccount", NOTICE_INFO, "Server account '%s'%s was given to '%s' by '%s'%s", u->name, knownlast(*u), owner->name, s->name, u->owner && *u->owner ? tempformatstring(", from '%s'", u->owner) : "");
                delete[] u->owner;
                u->owner = newstring(owner->name);
                if(t) { dropticket(t); t = NULL; } // a new key's link went to the old owner
                changed = true;
            }
            if(changed) dirtymasterfile(MFILE_SERV);
            vector<char> buf;
            jsonwriter w(buf);
            writeauth(w, *u, true, t && !t->level[0] ? t->expires : 0);
            res.json(buf.getbuf(), buf.length());
        }
        DELETEP(v);
    }

    // POST /signin/admin/servers/<handle>/key: a fresh link to the owner, for a new key, or for one still waiting
    void routeadminserverkey(httprequest &req, httpresponse &res)
    {
        signin *s = adminsignin(req, res);
        if(!s) return;
        const char *name = req.param("handle");
        authuser *u = name ? serverauthusers.access(name) : NULL;
        ticket *t = name && !u ? serverticket(name) : NULL;
        if(!u && !(t && t->level[0])) { res.error(404, "no such server account"); return; }
        if(!adminoutranks(accountpriv(s->flags), accountpriv(u ? u->flags : t->level))) { res.error(403, "that server account's level is at or above your standing"); return; }
        string handle, owner, email, level, desc;
        copystring(handle, u ? u->name : t->handle);
        copystring(level, u ? "" : t->level);
        copystring(desc, u ? "" : t->desc);
        copystring(owner, u ? (u->owner ? u->owner : "") : t->owner);
        if(!owner[0]) { res.error(409, "it has no owner yet, so give it one first"); return; }
        authuser *o = findowner(owner);
        if(!o) { res.error(409, "its owner's account is gone, so give it a new owner first"); return; }
        copystring(email, o->email ? o->email : "");
        if(!validemail(email)) { res.error(409, "there's no email address to send the link to"); return; }
        ticket *n = serverlink(handle, o->name, email, level, desc, s->name);
        noticef("serveraccount", NOTICE_INFO, "A new link for the server account '%s'%s was sent to '%s' by '%s'", handle, u ? knownlast(*u) : "", o->name, s->name);
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.utf8("handle", handle);
        w.num("expires", double(n->expires));
        w.endobject();
        res.status = 202;
        res.json(buf.getbuf(), buf.length());
    }

    // DELETE /signin/admin/servers/<handle>: removes a server account, or withdraws one still waiting for its owner
    void routeadminserverdel(httprequest &req, httpresponse &res)
    {
        signin *s = adminsignin(req, res);
        if(!s) return;
        const char *name = req.param("handle");
        string handle;
        copystring(handle, name ? name : "");
        authuser *u = serverauthusers.access(handle);
        ticket *t = serverticket(handle);
        if(!u && !(t && t->level[0])) { res.error(404, "no such server account"); return; }
        if(!adminoutranks(accountpriv(s->flags), accountpriv(u ? u->flags : t->level))) { res.error(403, "that server account's level is at or above your standing"); return; }
        if(t) dropticket(t);
        if(u)
        {
            string last;
            copystring(last, knownlast(*u));
            removeauth(serverauthusers, handle);
            dirtymasterfile(MFILE_SERV);
            noticef("serveraccount", NOTICE_INFO, "Server account '%s'%s was removed by '%s'", handle, last, s->name);
        }
        else noticef("serveraccount", NOTICE_INFO, "The waiting server account '%s' was withdrawn by '%s'", handle, s->name);
        res.status = 204;
    }


    void setupadmin()
    {
        http::addroute(HTTP_POST, ADMIN_PATH "/mail", HTTP_PUBLIC, routeadminmail);
        http::addroute(HTTP_GET, ADMIN_PATH "/accounts", HTTP_PUBLIC, routeadminaccounts);
        http::addroute(HTTP_PATCH, ADMIN_PATH "/accounts/:handle", HTTP_PUBLIC, routeadminaccountset);
        http::addroute(HTTP_DELETE, ADMIN_PATH "/accounts/:handle", HTTP_PUBLIC, routeadminaccountdel);
        http::addroute(HTTP_GET, ADMIN_PATH "/servers", HTTP_PUBLIC, routeadminservers);
        http::addroute(HTTP_POST, ADMIN_PATH "/servers", HTTP_PUBLIC, routeadminserveradd);
        http::addroute(HTTP_PATCH, ADMIN_PATH "/servers/:handle", HTTP_PUBLIC, routeadminserverset);
        http::addroute(HTTP_POST, ADMIN_PATH "/servers/:handle/key", HTTP_PUBLIC, routeadminserverkey);
        http::addroute(HTTP_DELETE, ADMIN_PATH "/servers/:handle", HTTP_PUBLIC, routeadminserverdel);
        http::addroute(HTTP_GET, ADMIN_PATH "/controls", HTTP_PUBLIC, routeadmincontrols);
        http::addroute(HTTP_POST, ADMIN_PATH "/controls", HTTP_PUBLIC, routeadmincontroladd);
        http::addroute(HTTP_DELETE, ADMIN_PATH "/controls/:id", HTTP_PUBLIC, routeadmincontroldel);
        http::addroute(HTTP_GET, ADMIN_PATH "/shield", HTTP_PUBLIC, routeadminshield);
        http::addroute(HTTP_PATCH, ADMIN_PATH "/shield", HTTP_PUBLIC, routeadminshieldsettings);
        http::addroute(HTTP_POST, ADMIN_PATH "/shield/test", HTTP_PUBLIC, routeadminshieldtest);
        http::addroute(HTTP_POST, ADMIN_PATH "/shield/modulators", HTTP_PUBLIC, routeadminshieldadd);
        http::addroute(HTTP_PATCH, ADMIN_PATH "/shield/modulators/:id", HTTP_PUBLIC, routeadminshieldset);
        http::addroute(HTTP_DELETE, ADMIN_PATH "/shield/modulators/:id", HTTP_PUBLIC, routeadminshielddel);
    }
}
