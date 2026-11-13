// Join links: redeclipse://host:port opens the game and joins that server, or hands it to the game already open
// The most recently started client registers the scheme with the desktop and listens on a per-user local endpoint,
// so a link clicked while playing goes to it instead of starting another game
#include "engine.h"

#ifndef WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

extern string homedir;
extern char *workingdir;

#define JOINLINK_MSGLEN 300 // longest message over the endpoint, a host and port with room to spare
#define JOINLINK_HOSTLEN 255 // longest host name DNS allows
#define JOINLINK_SENDWAIT 10000 // ms a link-started process waits for a running client, which may be loading a map
#define JOINLINK_TAKEWAIT 2000 // ms a starting client waits for the current owner to give up the endpoint
#define JOINLINK_RECLAIM 1000 // ms between a waiting client's tries to take the endpoint back
#define JOINLINK_TIMEOUT 2000 // ms the owner gives a connection to say what it wants
#define JOINLINK_TICKETLEN 32 // a Ready queue Ticket, in hex

namespace joinlink
{
    struct state
    {
#ifdef WIN32
        HANDLE pipe = INVALID_HANDLE_VALUE;
        bool connected = false, answered = false, release = false;
        int since = 0, msglen = 0;
        char msg[JOINLINK_MSGLEN];
#else
        int listener = -1;
#endif
        bool owner = false, waiting = false;
        int lastclaim = 0;
        string pendinghost, pendingticket;
        int pendingport = 0;
    } st;

    VAR(IDF_READONLY, joinlinkport, 0, 0, 65535);
    SVAR(IDF_READONLY, joinlinkhost, "");
    SVAR(IDF_READONLY, joinlinkticket, "");

    static void claim();
    static void release();

    VARF(IDF_PERSIST, joinlinks, 0, 1, 1,
    {
        if(initing) return;
        if(joinlinks) start();
        else
        {
            release();
            st.waiting = false;
        }
    });

    // A Ready queue Ticket from a link's ?ticket=, only ever hex, so a link can't put anything else into the connect
    static void parseticket(const char *query, char *ticket)
    {
        ticket[0] = '\0';
        for(const char *s = query; s && *s; )
        {
            const char *end = s + strcspn(s, "&#");
            if(!strncmp(s, "ticket=", 7))
            {
                const char *v = s + 7;
                int len = end - v;
                if(len > 0 && len <= JOINLINK_TICKETLEN && int(strspn(v, "0123456789abcdefABCDEF")) >= len)
                {
                    memcpy(ticket, v, len);
                    ticket[len] = '\0';
                }
            }
            if(*end != '&') break;
            s = end + 1;
        }
    }

    // Reads a Join link into a host and port, and a Ticket from its ?ticket= when asked; a password from an older link is dropped,
    // and anything else after the port is ignored, so later links can carry more
    bool parse(const char *link, char *host, int &port, char *ticket)
    {
        const char prefix[] = VERSION_UNAME "://";
        const int prefixlen = sizeof(prefix) - 1;
        loopi(prefixlen) if(tolower(uchar(link[i])) != prefix[i]) return false;
        const char *s = link + prefixlen, *end = s + strcspn(s, "/?#");
        const char *at = (const char *)memchr(s, '@', end - s);
        if(at) s = at + 1;
        const char *colon = (const char *)memchr(s, ':', end - s), *hostend = colon ? colon : end;
        int len = hostend - s;
        if(len <= 0 || len > JOINLINK_HOSTLEN) return false;
        // only what a host name or IPv4 address can hold, as the host ends up in a command for the guidelines prompt
        for(const char *c = s; c < hostend; c++) if(!isalnum(uchar(*c)) && *c != '.' && *c != '-' && *c != '_') return false;
        port = SERVER_PORT;
        if(colon && colon + 1 < end)
        {
            int n = 0;
            for(const char *c = colon + 1; c < end; c++)
            {
                if(!isdigit(uchar(*c))) return false;
                n = n*10 + (*c - '0');
                if(n > 65535) return false;
            }
            if(n > 0) port = n;
        }
        memcpy(host, s, len);
        host[len] = '\0';
        if(ticket) parseticket(strchr(end, '?') ? strchr(end, '?') + 1 : NULL, ticket);
        return true;
    }

    // Joins the server from a link, asking first when that would leave a game on a server or edits that can be undone
    void open(const char *host, int port, const char *ticket)
    {
        if(screen)
        {
            SDL_RestoreWindow(screen);
            SDL_RaiseWindow(screen);
        }
        // a link to the server this game is already on only needs the window brought up
        if(connected(false, false) && connectport == port && !strcasecmp(connectname, host)) return;
        if(connected(false, false) || (editmode && hasedits()))
        {
            setsvar("joinlinkhost", host);
            setvar("joinlinkport", port);
            setsvar("joinlinkticket", ticket);
            triggereventcallbacks(CMD_EVENT_GAME_JOINLINK);
            return;
        }
        setqueuelinkticket(ticket);
        connectserv(host, port, NULL);
    }

    // A link that arrives before the game has loaded waits to be opened once it has
    void queue(const char *host, int port, const char *ticket)
    {
        if(!engineready)
        {
            copystring(st.pendinghost, host);
            copystring(st.pendingticket, ticket);
            st.pendingport = port;
            return;
        }
        open(host, port, ticket);
    }

    static bool handle(const char *msg)
    {
        if(!strcmp(msg, "takeover"))
        {
            conoutf(colourgrey, "Join links now go to a newer copy of the game");
            st.waiting = true;
            return true;
        }
        if(strncmp(msg, "join ", 5)) return false;
        string host, ticket = "";
        int port = 0;
        // an older game hands over no Ticket
        if(sscanf(msg + 5, "%255s %d %32s", host, &port, ticket) < 2) return false;
        defformatstring(link, "%s%s:%d?ticket=%s", VERSION_UNAME "://", host, port, ticket);
        string phost, pticket;
        int pport;
        if(!parse(link, phost, pport, pticket)) return false;
        queue(phost, pport, pticket);
        return true;
    }

#ifdef WIN32
    static void endpointname(string &name)
    {
        char user[256];
        DWORD len = sizeof(user);
        if(!GetUserNameA(user, &len)) copystring(user, "user");
        for(char *c = user; *c; c++) if(!isalnum(uchar(*c))) *c = '_';
        formatstring(name, "\\\\.\\pipe\\%s-joinlink-%s", VERSION_UNAME, user);
    }

    // Sends one message to the client holding the endpoint and waits for it to say it took it
    static bool send(const char *msg, int timeout)
    {
        string name;
        endpointname(name);
        HANDLE h = CreateFileA(name, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY && WaitNamedPipeA(name, JOINLINK_TIMEOUT))
            h = CreateFileA(name, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if(h == INVALID_HANDLE_VALUE) return false;
        defformatstring(line, "%s\n", msg);
        DWORD wrote = 0;
        bool ok = false;
        if(WriteFile(h, line, strlen(line), &wrote, NULL) && wrote == strlen(line))
        {
            for(DWORD start = GetTickCount(); int(GetTickCount() - start) < timeout; )
            {
                DWORD avail = 0;
                if(!PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL)) break;
                if(avail)
                {
                    char reply[8] = "";
                    DWORD got = 0;
                    ok = ReadFile(h, reply, sizeof(reply)-1, &got, NULL) && got >= 2 && !strncmp(reply, "ok", 2);
                    break;
                }
                Sleep(10);
            }
        }
        CloseHandle(h);
        return ok;
    }

    static bool bind()
    {
        string name;
        endpointname(name);
        st.pipe = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_NOWAIT|PIPE_REJECT_REMOTE_CLIENTS, 1, JOINLINK_MSGLEN, JOINLINK_MSGLEN, 0, NULL);
        if(st.pipe == INVALID_HANDLE_VALUE) return false;
        st.connected = st.answered = st.release = false;
        st.msglen = 0;
        return true;
    }

    static void unbind()
    {
        if(st.pipe != INVALID_HANDLE_VALUE) CloseHandle(st.pipe);
        st.pipe = INVALID_HANDLE_VALUE;
        st.connected = st.answered = st.release = false;
        st.msglen = 0;
    }

    static void reset()
    {
        DisconnectNamedPipe(st.pipe);
        st.connected = st.answered = false;
        st.msglen = 0;
        if(st.release)
        {
            unbind();
            st.owner = false;
        }
    }

    // The pipe is in no-wait mode, so each frame looks for a connection, then its message, then for it to close
    static void poll()
    {
        if(st.pipe == INVALID_HANDLE_VALUE) return;
        if(!st.connected)
        {
            if(ConnectNamedPipe(st.pipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED)
            {
                st.connected = true;
                st.since = totalmillis;
            }
            else if(GetLastError() == ERROR_NO_DATA) DisconnectNamedPipe(st.pipe);
            return;
        }
        if(st.answered)
        {
            // the reply stays readable until the other end closes, so the pipe is only let go of then
            DWORD avail = 0;
            if(!PeekNamedPipe(st.pipe, NULL, 0, NULL, &avail, NULL) || totalmillis - st.since > JOINLINK_TIMEOUT) reset();
            return;
        }
        DWORD got = 0;
        if(ReadFile(st.pipe, st.msg + st.msglen, sizeof(st.msg) - 1 - st.msglen, &got, NULL) && got)
        {
            st.msglen += got;
            st.msg[st.msglen] = '\0';
            char *nl = strchr(st.msg, '\n');
            if(nl || st.msglen >= int(sizeof(st.msg)) - 1)
            {
                if(nl) *nl = '\0';
                bool ok = handle(st.msg);
                if(ok)
                {
                    DWORD wrote = 0;
                    WriteFile(st.pipe, "ok\n", 3, &wrote, NULL);
                    if(st.waiting) st.release = true;
                }
                st.answered = true;
                st.since = totalmillis;
                if(!ok) reset();
            }
        }
        else if(GetLastError() == ERROR_BROKEN_PIPE || totalmillis - st.since > JOINLINK_TIMEOUT) reset();
    }

    static bool writekey(HKEY root, const char *path, const char *name, const char *value)
    {
        HKEY key;
        if(RegCreateKeyExA(root, path, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) return false;
        bool ok = RegSetValueExA(key, name, 0, REG_SZ, (const BYTE *)value, strlen(value) + 1) == ERROR_SUCCESS;
        RegCloseKey(key);
        return ok;
    }

    // Points the per-user redeclipse scheme at this copy of the game, which needs no administrator rights
    static void registerscheme()
    {
        char exe[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, exe, sizeof(exe));
        if(!len || len >= sizeof(exe)) return;
        string home;
        copystring(home, homedir);
        size_t hlen = strlen(home);
        if(hlen > 1 && (home[hlen-1] == '\\' || home[hlen-1] == '/')) home[hlen-1] = '\0'; // a trailing backslash would escape the quote
        defformatstring(base, "Software\\Classes\\%s", VERSION_UNAME);
        defformatstring(iconkey, "%s\\DefaultIcon", base);
        defformatstring(commandkey, "%s\\shell\\open\\command", base);
        defformatstring(icon, "\"%s\",0", exe);
        defformatstring(command, "\"%s\" \"-h%s\" \"%%1\"", exe, home);
        defformatstring(desc, "URL:%s", VERSION_NAME);
        if(!writekey(HKEY_CURRENT_USER, base, NULL, desc) || !writekey(HKEY_CURRENT_USER, base, "URL Protocol", "") ||
           !writekey(HKEY_CURRENT_USER, iconkey, NULL, icon) || !writekey(HKEY_CURRENT_USER, commandkey, NULL, command))
            conoutf(colouryellow, "Warning: could not register %s:// links", VERSION_UNAME);
    }
#else
    static socklen_t endpointname(sockaddr_un &addr)
    {
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        // an abstract socket, so nothing is left on disk and it goes away with the process
        int len = snprintf(addr.sun_path + 1, sizeof(addr.sun_path) - 1, "%s-joinlink-%u", VERSION_UNAME, uint(getuid()));
        return socklen_t(offsetof(sockaddr_un, sun_path) + 1 + len);
    }

    static bool send(const char *msg, int timeout)
    {
        sockaddr_un addr;
        socklen_t addrlen = endpointname(addr);
        int fd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0);
        if(fd < 0) return false;
        bool ok = false;
        if(!connect(fd, (sockaddr *)&addr, addrlen))
        {
            defformatstring(line, "%s\n", msg);
            size_t len = strlen(line);
            if(write(fd, line, len) == ssize_t(len))
            {
                pollfd p = { fd, POLLIN, 0 };
                if(::poll(&p, 1, timeout) > 0)
                {
                    char reply[8] = "";
                    ok = read(fd, reply, sizeof(reply)-1) >= 2 && !strncmp(reply, "ok", 2);
                }
            }
        }
        close(fd);
        return ok;
    }

    static bool bind()
    {
        sockaddr_un addr;
        socklen_t addrlen = endpointname(addr);
        int fd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, 0);
        if(fd < 0) return false;
        if(::bind(fd, (sockaddr *)&addr, addrlen) || listen(fd, 4))
        {
            close(fd);
            return false;
        }
        st.listener = fd;
        return true;
    }

    static void unbind()
    {
        if(st.listener >= 0) close(st.listener);
        st.listener = -1;
    }

    static void poll()
    {
        if(st.listener < 0) return;
        for(;;)
        {
            int fd = accept4(st.listener, NULL, NULL, SOCK_CLOEXEC);
            if(fd < 0) break;
            // abstract sockets have no file permissions, so only the same user may talk to it
            ucred cred;
            socklen_t credlen = sizeof(cred);
            if(!getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &credlen) && cred.uid == getuid())
            {
                char msg[JOINLINK_MSGLEN];
                int len = 0;
                pollfd p = { fd, POLLIN, 0 };
                while(len < int(sizeof(msg)) - 1 && ::poll(&p, 1, JOINLINK_TIMEOUT / 10) > 0)
                {
                    ssize_t got = read(fd, msg + len, sizeof(msg) - 1 - len);
                    if(got <= 0) break;
                    len += got;
                    msg[len] = '\0';
                    if(strchr(msg, '\n')) break;
                }
                msg[len] = '\0';
                char *nl = strchr(msg, '\n');
                if(nl) *nl = '\0';
                if(handle(msg))
                {
                    if(write(fd, "ok\n", 3) < 0) {}
                    if(st.waiting)
                    {
                        close(fd);
                        unbind();
                        st.owner = false;
                        return;
                    }
                }
            }
            close(fd);
        }
    }

    // Escapes an argument for a desktop entry's Exec key: quoted for the command line, then backslashes doubled for the file
    static void execarg(vector<char> &buf, const char *arg)
    {
        buf.add('"');
        for(const char *c = arg; *c; c++)
        {
            if(*c == '"' || *c == '`' || *c == '$') { buf.add('\\'); buf.add('\\'); }
            else if(*c == '\\') { buf.add('\\'); buf.add('\\'); buf.add('\\'); }
            if(*c == '%') buf.add('%');
            buf.add(*c);
        }
        buf.add('"');
    }

    static void spawn(const char *cmd, const char *a, const char *b, const char *c)
    {
        pid_t pid = fork();
        if(!pid)
        {
            if(!fork())
            {
                int devnull = ::open("/dev/null", O_RDWR);
                if(devnull >= 0) { dup2(devnull, 1); dup2(devnull, 2); }
                execlp(cmd, cmd, a, b, c, (char *)NULL);
                _exit(1);
            }
            _exit(0);
        }
        else if(pid > 0) waitpid(pid, NULL, 0);
    }

    // Writes a hidden desktop entry for this copy of the game and makes it the handler for the scheme
    static void registerscheme()
    {
        string dir;
        const char *data = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
        if(data && *data) copystring(dir, data);
        else if(home && *home) formatstring(dir, "%s/.local/share", home);
        else return;
        concatstring(dir, "/applications");
        for(char *c = dir + 1; *c; c++) if(*c == '/') { *c = '\0'; mkdir(dir, 0755); *c = '/'; }
        mkdir(dir, 0755);
        if(access(dir, W_OK)) return;

        string exe = "", script, cwd = "";
        ssize_t len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        if(len <= 0) return;
        exe[len] = '\0';
        if(!getcwd(cwd, sizeof(cwd))) return;
        string hdir;
        if(homedir[0] == '/') copystring(hdir, homedir);
        else formatstring(hdir, "%s/%s", cwd, homedir);
        size_t hlen = strlen(hdir);
        if(hlen > 1 && hdir[hlen-1] == '/') hdir[hlen-1] = '\0';
        defformatstring(homearg, "-h%s", hdir);

        vector<char> exec;
        // a portable copy goes through its script, which finds its libraries; a system install's binary uses the system's
        formatstring(script, "%s/%s.sh", cwd, VERSION_UNAME);
        if(!access(script, X_OK)) execarg(exec, script);
        else execarg(exec, exe);
        exec.add(' ');
        execarg(exec, homearg);
        exec.put(" %u", 3);
        exec.add('\0');

        defformatstring(path, "%s/%s-url.desktop", dir, VERSION_UNAME);
        defformatstring(entry, "[Desktop Entry]\nType=Application\nName=%s\nNoDisplay=true\nMimeType=x-scheme-handler/%s;\nPath=%s\nExec=%s\n",
            VERSION_NAME, VERSION_UNAME, cwd, exec.getbuf());

        // only a change needs the desktop told again, so starting the same copy twice costs nothing
        FILE *f = fopen(path, "r");
        if(f)
        {
            char old[sizeof(entry)];
            size_t got = fread(old, 1, sizeof(old) - 1, f);
            old[got] = '\0';
            fclose(f);
            if(!strcmp(old, entry)) return;
        }
        f = fopen(path, "w");
        if(!f) return;
        bool ok = fputs(entry, f) >= 0;
        if(fclose(f) || !ok) return;
        defformatstring(desktop, "%s-url.desktop", VERSION_UNAME);
        defformatstring(mime, "x-scheme-handler/%s", VERSION_UNAME);
        spawn("xdg-mime", "default", desktop, mime);
    }
#endif

    static void release()
    {
        unbind();
        st.owner = false;
    }

    static void claim()
    {
        st.lastclaim = totalmillis;
        if(!bind()) return;
        st.owner = true;
        st.waiting = false;
        registerscheme();
    }

    // Takes the endpoint from the client holding it, if any, and registers this copy as the scheme's handler
    void start()
    {
        if(!joinlinks || st.owner) return;
        if(bind())
        {
            st.owner = true;
            st.waiting = false;
            registerscheme();
            return;
        }
        // the owner lets go once it has replied, so the endpoint frees up shortly after
        if(send("takeover", JOINLINK_TAKEWAIT)) for(int start = SDL_GetTicks(); int(SDL_GetTicks() - start) < JOINLINK_TAKEWAIT; )
        {
            if(bind())
            {
                st.owner = true;
                st.waiting = false;
                registerscheme();
                return;
            }
            SDL_Delay(20);
        }
        st.waiting = true;
        st.lastclaim = totalmillis;
    }

    // Hands a link to the running client, returning true when it took it
    bool handoff(const char *host, int port, const char *ticket)
    {
        defformatstring(msg, "join %s %d %s", host, port, ticket);
        return send(msg, JOINLINK_SENDWAIT);
    }

    void update()
    {
        if(st.pendingport && engineready)
        {
            int port = st.pendingport;
            st.pendingport = 0;
            open(st.pendinghost, port, st.pendingticket);
        }
        if(!joinlinks) return;
        if(st.owner) poll();
        else if(st.waiting && totalmillis - st.lastclaim >= JOINLINK_RECLAIM) claim();
    }

    // A Steam build started outside Steam would be started again by Steam without its arguments, so the link goes
    // through Steam instead
    bool steamlaunch()
    {
        if(!st.pendingport) return false;
        const char *host = st.pendinghost;
        int port = st.pendingport;
        if(!versionsteamid || strncmp(versionbranch, "steam", 5) || getenv("SteamGameId") || getenv("SteamAppId")) return false;
        defformatstring(url, "steam://run/%d//%s%%3A%%2F%%2F%s%%3A%d", versionsteamid, VERSION_UNAME, host, port);
        if(st.pendingticket[0]) concformatstring(url, "%%3Fticket%%3D%s", st.pendingticket);
        concatstring(url, "/");
#if SDL_VERSION_ATLEAST(2, 0, 14)
        return SDL_OpenURL(url) == 0;
#elif !defined(WIN32)
        spawn("xdg-open", url, NULL, NULL);
        return true;
#else
        return false;
#endif
    }
}

ICOMMAND(0, joinlinkopen, "", (), { setqueuelinkticket(joinlink::joinlinkticket); connectserv(joinlink::joinlinkhost, joinlink::joinlinkport, NULL); });
