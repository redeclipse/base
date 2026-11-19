#include "engine.h"
#include "master.h"

#define LINK_STATETIME 600 // seconds a link in progress lasts
#define LINK_MAXPENDING 1024
#define OPENID_NS "http://specs.openid.net/auth/2.0"
#define OPENID_SELECT "http://specs.openid.net/auth/2.0/identifier_select"
#define STEAM_CLAIMED "https://steamcommunity.com/openid/id/"
#define DISCORD_IDMAX 20

namespace master
{
    // linking an account to Steam and Discord, which its owner proves through each one's own sign-in

    SVAR(0, mastersteamopenid, "https://steamcommunity.com/openid/login");


    enum { LINK_STEAM = 0, LINK_DISCORD };

    struct linkpending
    {
        int kind, deferred, fetch;
        string state, handle, extra; // extra: Steam's claimed ID, or Discord's member while its name is fetched
        string addr; // where its owner finished it from, for the Admin notice
        time_t started;
    };

    static vector<linkpending *> linkpendings;

    static linkpending *newlinkpending(int kind, const char *handle)
    {
        time_t now = time(NULL);
        loopvrev(linkpendings) if(linkpendings[i]->fetch < 0 && (now - linkpendings[i]->started > LINK_STATETIME || (linkpendings[i]->kind == kind && !strcmp(linkpendings[i]->handle, handle))))
            delete linkpendings.remove(i);
        if(linkpendings.length() >= LINK_MAXPENDING) return NULL;
        linkpending *l = new linkpending;
        l->kind = kind;
        l->deferred = l->fetch = -1;
        uchar bytes[16];
        http::randombytes(bytes, 16);
        hexbytes(l->state, bytes, 16);
        copystring(l->handle, handle);
        l->extra[0] = l->addr[0] = '\0';
        l->started = now;
        linkpendings.add(l);
        return l;
    }

    static linkpending *findlinkpending(int kind, const char *handle, const char *state)
    {
        if(!state) return NULL;
        loopv(linkpendings)
        {
            linkpending *l = linkpendings[i];
            if(l->kind == kind && l->fetch < 0 && !strcmp(l->handle, handle) && !strcmp(l->state, state) && time(NULL) - l->started <= LINK_STATETIME) return l;
        }
        return NULL;
    }

    void cleanuplinks()
    {
        loopv(linkpendings) if(linkpendings[i]->fetch >= 0) http::cancel(linkpendings[i]->fetch);
        linkpendings.deletecontents();
    }

    static void linkreturn(string &url, const char *page, const char *state)
    {
        formatstring(url, "%s/%s?state=%s", masteraccounturl, page, state);
    }

    // the scheme and host the return address is under, as Steam wants for its realm
    static void linkrealm(string &realm)
    {
        copystring(realm, masteraccounturl);
        char *host = strstr(realm, "://");
        if(host) { char *path = strchr(host + 3, '/'); if(path) *path = '\0'; }
    }

    static void urlparam(vector<char> &buf, const char *name, const char *value)
    {
        buf.add(buf.empty() || !memchr(buf.getbuf(), '?', buf.length()) ? '?' : '&');
        buf.put(name, strlen(name));
        buf.add('=');
        http::urlencode(buf, value);
    }

    // GET /signin/account/steam: where to send the owner to prove their Steam account
    static void routesteamstart(httprequest &req, httpresponse &res)
    {
        authuser *u = NULL;
        if(!ownersignin(req, res, u)) return;
        linkpending *l = newlinkpending(LINK_STEAM, u->name);
        if(!l) { res.error(503, "too many links in progress, try again soon"); return; }
        string back, realm;
        linkreturn(back, "steam", l->state);
        linkrealm(realm);
        vector<char> url;
        url.put(mastersteamopenid, strlen(mastersteamopenid));
        urlparam(url, "openid.ns", OPENID_NS);
        urlparam(url, "openid.mode", "checkid_setup");
        urlparam(url, "openid.return_to", back);
        urlparam(url, "openid.realm", realm);
        urlparam(url, "openid.identity", OPENID_SELECT);
        urlparam(url, "openid.claimed_id", OPENID_SELECT);
        url.add('\0');
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.utf8("url", url.getbuf());
        w.utf8("state", l->state);
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }

    static bool steamtaken(const char *steamid, const char *handle)
    {
        bool taken = false;
        enumerate(authusers, authuser, o, if(strcmp(o.name, handle) && o.steamid && !strcmp(o.steamid, steamid)) taken = true);
        return taken;
    }

    static void steamchecked(httpfetch &f)
    {
        linkpending *l = (linkpending *)f.ctx;
        if(linkpendings.find(l) < 0) return;
        linkpendings.removeobj(l);
        httpresponse res;
        authuser *u = authusers.access(l->handle);
        bool valid = false;
        if(f.status == 200) // key:value lines, of which is_valid:true says Steam made the answer
        {
            f.body.add('\0');
            for(const char *line = f.body.getbuf(); *line; )
            {
                int len = strcspn(line, "\n");
                if(len >= 13 && !strncmp(line, "is_valid:true", 13)) valid = true;
                line += len;
                if(*line) line++;
            }
        }
        if(!u) res.error(404, "your account no longer exists");
        else if(!valid) res.error(403, "Steam didn't vouch for that sign-in, try again");
        else if(steamtaken(l->extra, u->name)) res.error(409, "that Steam account is linked to another account");
        else
        {
            delete[] u->steamid;
            u->steamid = newstring(l->extra);
            dirtymasterfile(MFILE_USER);
            noticef("account", NOTICE_INFO, "Account '%s' linked the Steam account %s from %s", u->name, u->steamid, l->addr);
            vector<char> buf;
            jsonwriter w(buf);
            writeowner(w, *u);
            res.json(buf.getbuf(), buf.length());
        }
        http::reply(l->deferred, res);
        delete l;
    }

    // POST /signin/account/steam with every openid. field Steam sent back
    static void routesteamfinish(httprequest &req, httpresponse &res)
    {
        authuser *u = NULL;
        if(!ownersignin(req, res, u)) return;
        jsonvalue *v = signinbody(req);
        jsonvalue *params = v ? v->get("openid") : NULL;
        const char *back = params ? params->getstr("openid.return_to") : NULL, *claimed = params ? params->getstr("openid.claimed_id") : NULL;
        const char *mode = params ? params->getstr("openid.mode") : NULL, *state = NULL;
        string expected = "";
        // the answer has to come back to our page with the state we gave this account
        if(back)
        {
            const char *q = strstr(back, "?state=");
            if(q) state = q + 7;
        }
        linkpending *l = state ? findlinkpending(LINK_STEAM, u->name, state) : NULL;
        if(l) linkreturn(expected, "steam", l->state);
        const char *id = claimed && !strncmp(claimed, STEAM_CLAIMED, strlen(STEAM_CLAIMED)) ? claimed + strlen(STEAM_CLAIMED) : NULL;
        if(!params || params->type != JSON_OBJECT || !mode || strcmp(mode, "id_res")) res.error(400, "send the openid fields Steam sent back");
        else if(!l || strcmp(back, expected)) res.error(403, "that sign-in wasn't started here, or took too long, start again");
        else if(!validsteamid(id)) res.error(400, "Steam's answer has no Steam ID");
        else if(steamtaken(id, u->name)) res.error(409, "that Steam account is linked to another account");
        else
        {
            copystring(l->extra, id);
            copystring(l->addr, req.addr);
            httpfetch *f = http::newfetch(HTTP_POST, mastersteamopenid, steamchecked, l);
            f->reqheaders.add("Content-Type", "application/x-www-form-urlencoded");
            bool first = true;
            loopv(params->items)
            {
                jsonvalue *p = params->items[i];
                if(!p->key || strncmp(p->key, "openid.", 7) || p->type != JSON_STRING || !p->str) continue;
                if(!first) f->reqbody.add('&');
                first = false;
                http::urlencode(f->reqbody, p->key);
                f->reqbody.add('=');
                http::urlencode(f->reqbody, strcmp(p->key, "openid.mode") ? p->str : "check_authentication");
            }
            l->deferred = http::deferred(req);
            res.defer();
            int id = http::submit(f);
            if(linkpendings.find(l) >= 0) l->fetch = id;
        }
        DELETEP(v);
    }

    // DELETE /signin/account/steam
    static void routesteamunlink(httprequest &req, httpresponse &res)
    {
        authuser *u = NULL;
        if(!ownersignin(req, res, u)) return;
        if(!u->steamid || !*u->steamid) { res.error(404, "no Steam account is linked"); return; }
        noticef("account", NOTICE_INFO, "Account '%s' unlinked the Steam account %s from %s", u->name, u->steamid, req.addr);
        delete[] u->steamid;
        u->steamid = newstring("");
        dirtymasterfile(MFILE_USER);
        res.status = 204;
    }

    SVAR(0, masterdiscordclient, ""); // the Discord application's client ID, for linking through Discord's own sign-in
    SVAR(0, masterdiscordapi, "https://discord.com/api");
    SVAR(0, masterdiscordauthorize, "https://discord.com/oauth2/authorize");
    VAR(0, masterdiscordlock, 0, PRIV_DEVELOPER, PRIV_MAX); // Privilege that may look up Discord members, the bot's



    static bool validdiscordid(const char *id) { int len = strlen(id); return len >= 15 && len <= DISCORD_IDMAX && (int)strspn(id, "0123456789") == len; }

    authuser *discordaccount(const char *id)
    {
        authuser *found = NULL;
        enumerate(authusers, authuser, u, if(!found && u.discord && *u.discord && !strcmp(u.discord, id)) found = &u);
        return found;
    }

    // links the account, or says why not, filling an empty display name from Discord's
    static bool linkdiscord(httpresponse &res, authuser &u, const char *id, const char *name, const char *addr)
    {
        authuser *other = discordaccount(id);
        if(other && other != &u) { res.error(409, "that Discord account is linked to another account"); return false; }
        delete[] u.discord;
        u.discord = newstring(id);
        if((!u.display || !*u.display) && name && *name)
        {
            string shown;
            displayfromutf8(shown, name);
            if(shown[0]) setdisplay(u, shown);
        }
        dirtymasterfile(MFILE_USER);
        noticef("account", NOTICE_INFO, "Account '%s' linked the Discord account %s from %s", u.name, id, addr);
        return true;
    }

    // GET /signin/account/discord: where to send the owner to prove their Discord account
    static void routediscordstart(httprequest &req, httpresponse &res)
    {
        authuser *u = NULL;
        if(!ownersignin(req, res, u)) return;
        if(!*masterdiscordclient || !*mastersecrets.discord) { res.error(503, "linking through Discord's sign-in isn't set up"); return; }
        linkpending *l = newlinkpending(LINK_DISCORD, u->name);
        if(!l) { res.error(503, "too many links in progress, try again soon"); return; }
        defformatstring(back, "%s/discord", masteraccounturl);
        vector<char> url;
        url.put(masterdiscordauthorize, strlen(masterdiscordauthorize));
        urlparam(url, "client_id", masterdiscordclient);
        urlparam(url, "response_type", "code");
        urlparam(url, "redirect_uri", back);
        urlparam(url, "scope", "identify");
        urlparam(url, "state", l->state);
        urlparam(url, "prompt", "none");
        url.add('\0');
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.utf8("url", url.getbuf());
        w.utf8("state", l->state);
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }

    static void discordfail(linkpending *l, int status, const char *msg)
    {
        linkpendings.removeobj(l);
        httpresponse res;
        res.error(status, msg);
        http::reply(l->deferred, res);
        delete l;
    }

    static void discorduser(httpfetch &f)
    {
        linkpending *l = (linkpending *)f.ctx;
        if(linkpendings.find(l) < 0) return;
        jsonvalue *v = f.status == 200 && f.body.length() ? json::parse(f.body.getbuf(), f.body.length()) : NULL;
        const char *id = v ? v->getstr("id") : NULL, *name = v ? v->getstr("global_name") : NULL;
        if(v && (!name || !*name)) name = v->getstr("username");
        authuser *u = authusers.access(l->handle);
        if(!id || !validdiscordid(id)) { DELETEP(v); discordfail(l, 502, "Discord didn't say who you are, try again"); return; }
        if(!u) { DELETEP(v); discordfail(l, 404, "your account no longer exists"); return; }
        linkpendings.removeobj(l);
        httpresponse res;
        if(linkdiscord(res, *u, id, name, l->addr))
        {
            vector<char> buf;
            jsonwriter w(buf);
            writeowner(w, *u);
            res.json(buf.getbuf(), buf.length());
        }
        http::reply(l->deferred, res);
        DELETEP(v);
        delete l;
    }

    static void discordtoken(httpfetch &f)
    {
        linkpending *l = (linkpending *)f.ctx;
        if(linkpendings.find(l) < 0) return;
        jsonvalue *v = f.status == 200 && f.body.length() ? json::parse(f.body.getbuf(), f.body.length()) : NULL;
        const char *token = v ? v->getstr("access_token") : NULL;
        if(!token || !*token || strlen(token) > 512 || strpbrk(token, "\r\n")) { DELETEP(v); discordfail(l, 403, "Discord didn't accept that sign-in, try again"); return; }
        defformatstring(url, "%s/users/@me", masterdiscordapi);
        httpfetch *g = http::newfetch(HTTP_GET, url, discorduser, l);
        defformatstring(auth, "Bearer %s", token);
        g->reqheaders.add("Authorization", auth);
        DELETEP(v);
        l->fetch = -1;
        int id = http::submit(g);
        if(linkpendings.find(l) >= 0) l->fetch = id;
    }

    // POST /signin/account/discord with the code and state Discord sent back to the account page
    static void routediscordfinish(httprequest &req, httpresponse &res)
    {
        authuser *u = NULL;
        if(!ownersignin(req, res, u)) return;
        jsonvalue *v = signinbody(req);
        const char *code = signinfield(req, v, "code"), *state = signinfield(req, v, "state");
        linkpending *l = findlinkpending(LINK_DISCORD, u->name, state);
        if(!*masterdiscordclient || !*mastersecrets.discord) res.error(503, "linking through Discord's sign-in isn't set up");
        else if(!code || !*code || strlen(code) > 256) res.error(400, "send the code Discord sent back");
        else if(!l) res.error(403, "that sign-in wasn't started here, or took too long, start again");
        else
        {
            defformatstring(url, "%s/oauth2/token", masterdiscordapi);
            defformatstring(back, "%s/discord", masteraccounturl);
            copystring(l->addr, req.addr);
            httpfetch *f = http::newfetch(HTTP_POST, url, discordtoken, l);
            f->reqheaders.add("Content-Type", "application/x-www-form-urlencoded");
            const char *fields[5][2] = { { "client_id", masterdiscordclient }, { "client_secret", mastersecrets.discord }, { "grant_type", "authorization_code" }, { "code", code }, { "redirect_uri", back } };
            loopi(5)
            {
                if(i) f->reqbody.add('&');
                f->reqbody.put(fields[i][0], strlen(fields[i][0]));
                f->reqbody.add('=');
                http::urlencode(f->reqbody, fields[i][1]);
            }
            l->deferred = http::deferred(req);
            res.defer();
            int id = http::submit(f);
            if(linkpendings.find(l) >= 0) l->fetch = id;
        }
        DELETEP(v);
    }

    // DELETE /signin/account/discord
    static void routediscordunlink(httprequest &req, httpresponse &res)
    {
        authuser *u = NULL;
        if(!ownersignin(req, res, u)) return;
        if(!u->discord || !*u->discord) { res.error(404, "no Discord account is linked"); return; }
        noticef("account", NOTICE_INFO, "Account '%s' unlinked the Discord account %s from %s", u->name, u->discord, req.addr);
        delete[] u->discord;
        u->discord = newstring("");
        dirtymasterfile(MFILE_USER);
        res.status = 204;
    }

    static bool discordbot(httprequest &req, httpresponse &res)
    {
        res.headers.set("Cache-Control", "no-store");
        if(req.privilege < masterdiscordlock) { res.error(req.privilege ? 403 : 401, "only the Discord bot can do that"); return false; }
        return true;
    }

    // GET /accounts/discord/<id>: which account a member is, for the bot to show their handle
    void routediscordlookup(httprequest &req, httpresponse &res)
    {
        if(!discordbot(req, res)) return;
        const char *id = req.param("id");
        authuser *u = id && validdiscordid(id) ? discordaccount(id) : NULL;
        if(!u) { res.error(404, "that member hasn't linked an account"); return; }
        string priv;
        copystring(priv, PRIV_STR[clamp(accountpriv(u->flags), 0, PRIV_MAX-1)]);
        for(char *c = priv; *c; c++) *c = tolower(uchar(*c));
        vector<char> buf;
        jsonwriter w(buf);
        w.beginobject();
        w.utf8("handle", u->name);
        w.str("displayname", u->display ? u->display : "");
        w.utf8("privilege", priv); // the account's own standing, for the bot to show by the member's bridged chat
        w.endobject();
        res.json(buf.getbuf(), buf.length());
    }

    void setuplinks()
    {
        http::addroute(HTTP_GET, SIGNIN_PATH "/account/steam", HTTP_PUBLIC, routesteamstart);
        http::addroute(HTTP_POST, SIGNIN_PATH "/account/steam", HTTP_PUBLIC, routesteamfinish);
        http::addroute(HTTP_DELETE, SIGNIN_PATH "/account/steam", HTTP_PUBLIC, routesteamunlink);
        http::addroute(HTTP_GET, SIGNIN_PATH "/account/discord", HTTP_PUBLIC, routediscordstart);
        http::addroute(HTTP_POST, SIGNIN_PATH "/account/discord", HTTP_PUBLIC, routediscordfinish);
        http::addroute(HTTP_DELETE, SIGNIN_PATH "/account/discord", HTTP_PUBLIC, routediscordunlink);
    }
}
