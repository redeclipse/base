// Web API network thread: sockets, TLS, HTTP and WebSocket framing
// handlers and callbacks never run here, everything goes back to the main thread as events

#include "engine.h"

#ifdef WIN32
#include <ws2tcpip.h>
#include <wincrypt.h>
#include <bcrypt.h>
#define NET_POLL WSAPoll
typedef WSAPOLLFD netpollfd;
#else
#include <poll.h>
#include <pthread.h>
#include <errno.h>
#define NET_POLL poll
typedef struct pollfd netpollfd;
#endif

#include <mbedtls/ssl.h>
#include <mbedtls/ssl_ticket.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/threading.h>
#include <psa/crypto.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace httpnet
{
    // threads and locks, the dedicated server has no SDL so these are native
#ifdef WIN32
    struct mutex
    {
        SRWLOCK lock;
        mutex() { InitializeSRWLock(&lock); }
        void acquire() { AcquireSRWLockExclusive(&lock); }
        void release() { ReleaseSRWLockExclusive(&lock); }
    };
    struct signal
    {
        HANDLE sem;
        signal() { sem = CreateSemaphore(NULL, 0, 0x7FFFFFFF, NULL); }
        ~signal() { CloseHandle(sem); }
        void post() { ReleaseSemaphore(sem, 1, NULL); }
        void wait() { WaitForSingleObject(sem, INFINITE); }
    };
    typedef HANDLE thread;
    typedef DWORD (WINAPI *threadfun)(void *);
    #define THREADFUN(name) static DWORD WINAPI name(void *arg)
    #define THREADRET return 0
    static bool startthread(thread &t, threadfun fun, void *arg) { t = CreateThread(NULL, 0, fun, arg, 0, NULL); return t != NULL; }
    static void jointhread(thread &t) { WaitForSingleObject(t, INFINITE); CloseHandle(t); }
    static int sockerror() { return WSAGetLastError(); }
    static bool wouldblock(int err) { return err == WSAEWOULDBLOCK || err == WSAEINTR; }
#else
    struct mutex
    {
        pthread_mutex_t lock;
        mutex() { pthread_mutex_init(&lock, NULL); }
        ~mutex() { pthread_mutex_destroy(&lock); }
        void acquire() { pthread_mutex_lock(&lock); }
        void release() { pthread_mutex_unlock(&lock); }
    };
    struct signal
    {
        pthread_mutex_t lock;
        pthread_cond_t cond;
        int count;
        signal() : count(0) { pthread_mutex_init(&lock, NULL); pthread_cond_init(&cond, NULL); }
        ~signal() { pthread_cond_destroy(&cond); pthread_mutex_destroy(&lock); }
        void post() { pthread_mutex_lock(&lock); count++; pthread_cond_signal(&cond); pthread_mutex_unlock(&lock); }
        void wait() { pthread_mutex_lock(&lock); while(count <= 0) pthread_cond_wait(&cond, &lock); count--; pthread_mutex_unlock(&lock); }
    };
    typedef pthread_t thread;
    typedef void *(*threadfun)(void *);
    #define THREADFUN(name) static void *name(void *arg)
    #define THREADRET return NULL
    static bool startthread(thread &t, threadfun fun, void *arg) { return !pthread_create(&t, NULL, fun, arg); }
    static void jointhread(thread &t) { pthread_join(t, NULL); }
    static int sockerror() { return errno; }
    static bool wouldblock(int err) { return err == EAGAIN || err == EWOULDBLOCK || err == EINTR || err == EINPROGRESS; }
#endif

    struct lock
    {
        mutex &m;
        lock(mutex &m) : m(m) { m.acquire(); }
        ~lock() { m.release(); }
    };

    // >0 bytes, 0 would block, -1 closed or failed
    static int sockrecv(ENetSocket fd, void *buf, int len, int flags = 0)
    {
        int r = recv(fd, (char *)buf, len, flags);
        if(r > 0) return r;
        if(r < 0 && wouldblock(sockerror())) return 0;
        return -1;
    }

    static int socksend(ENetSocket fd, const void *buf, int len)
    {
        int r = ::send(fd, (const char *)buf, len, MSG_NOSIGNAL);
        if(r >= 0) return r;
        if(wouldblock(sockerror())) return 0;
        return -1;
    }

    static void socknodelay(ENetSocket fd)
    {
        enet_socket_set_option(fd, ENET_SOCKOPT_NODELAY, 1);
#ifdef SO_NOSIGPIPE
        int on = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
    }

    // a buffer queued to one or many connections, only ever touched here once handed over
    struct netbuf
    {
        char *data;
        int len, refs;

        netbuf(const char *src, int n, int refs = 1) : data(new char[max(n, 1)]), len(n), refs(refs) { if(n) memcpy(data, src, n); }
        ~netbuf() { DELETEA(data); }
    };

    static void unref(netbuf *b) { if(b && --b->refs <= 0) delete b; }

    struct netout
    {
        netbuf *buf;
        int pos;
    };

    enum { ROLE_SERVER = 0, ROLE_CLIENT };
    enum { CS_RESOLVING = 0, CS_CONNECTING, CS_DETECT, CS_HANDSHAKE, CS_OPEN };
    enum { PROTO_HTTP = 0, PROTO_WS };
    enum { P_HEAD = 0, P_BODY, P_CHUNKSIZE, P_CHUNKDATA, P_CHUNKEND, P_TRAILER, P_UNTILCLOSE, P_DONE };

    struct job
    {
        int method, maxsize, redirects;
        bool ws, gzip;
        httpurl url;
        char *file, *pin, *partial;
        vector<char> body;
        httpheaders headers;
        string wskey;
        ENetAddress addrs[4];
        int numaddrs, curaddr;
        FILE *fp;

        job() : method(HTTP_GET), maxsize(HTTP_INMEMORY), redirects(0), ws(false), gzip(false), file(NULL), pin(NULL), partial(NULL), numaddrs(0), curaddr(0), fp(NULL) { wskey[0] = '\0'; }
        ~job()
        {
            if(fp) fclose(fp);
            if(partial) { remove(partial); DELETEA(partial); }
            DELETEA(file);
            DELETEA(pin);
        }
    };

    struct tlsserver
    {
        mbedtls_ssl_config conf;
        mbedtls_x509_crt chain;
        mbedtls_pk_context key;
        int refs;

        tlsserver() : refs(1)
        {
            mbedtls_ssl_config_init(&conf);
            mbedtls_x509_crt_init(&chain);
            mbedtls_pk_init(&key);
        }
        ~tlsserver()
        {
            mbedtls_ssl_config_free(&conf);
            mbedtls_x509_crt_free(&chain);
            mbedtls_pk_free(&key);
        }
    };

    struct conn
    {
        int id, role, state, proto, pstate, maxmessage;
        ENetSocket fd;
        uint ip;
        bool tls, busy, cancelled, wantwrite, waiting, closing, chunked, keepalive, gotclose, sentclose, sentping;
        long long remaining;
        int recvd, total, headbytes, wsop;
        uint lastrecv, lastsend, opened, closestart, msgstart;
        vector<char> in, wsmsg;
        int inpos;
        vector<netout> out;
        int queued;
        httpmessage *msg;
        job *fetch;
        mbedtls_ssl_context *ssl;
        tlsserver *server;

        conn() : id(-1), role(ROLE_SERVER), state(CS_OPEN), proto(PROTO_HTTP), pstate(P_HEAD), maxmessage(0), fd(ENET_SOCKET_NULL), ip(0),
            tls(false), busy(false), cancelled(false), wantwrite(false), waiting(false), closing(false), chunked(false), keepalive(false), gotclose(false), sentclose(false), sentping(false),
            remaining(0), recvd(0), total(-1), headbytes(0), wsop(-1), lastrecv(0), lastsend(0), opened(0), closestart(0), msgstart(0), inpos(0), queued(0), msg(NULL), fetch(NULL)
            , ssl(NULL), server(NULL)
        {}
    };

    enum { CMD_SEND = 0, CMD_SHARED, CMD_CLOSE, CMD_FETCH, CMD_LISTEN, CMD_UNLISTEN, CMD_SERVERCERT };
    struct command
    {
        int type, id, flags, maxmessage;
        netbuf *buf;
        job *fetch;
        vector<int> ids;
        ENetSocket fd;
        tlsserver *server;

        command() : type(-1), id(-1), flags(0), maxmessage(0), buf(NULL), fetch(NULL), fd(ENET_SOCKET_NULL)
            , server(NULL)
        {}
    };

    enum { TASK_RESOLVE = 0, TASK_HANDSHAKE };
    struct task
    {
        int type, result;
        conn *c;
    };

    struct progressinfo
    {
        int recvd, total;
    };

    static mutex cmdlock, evlock, tasklock, donelock;
    static signal tasksignal;
    static vector<command *> commands;
    static vector<event *> events;
    static vector<task> tasks, done;
    static hashtable<int, progressinfo> progresses;
    static config cfg, sharedcfg;
    static bool running = false, quitting = false;
    static thread nettid;
    static vector<thread> workers;
    static ENetSocket wakefd = ENET_SOCKET_NULL, listenfd = ENET_SOCKET_NULL;
    static ENetAddress wakeaddr;

    // network thread only
    static vector<conn *> conns;
    static hashtable<uint, int> peraddr;
    static uint netmillis = 0;

    struct ratewindow
    {
        uint start;
        int count;
    };
    static hashtable<uint, ratewindow> handshakes;

    static uint getmillis()
    {
        return enet_time_get();
    }

    static void wake()
    {
        if(wakefd == ENET_SOCKET_NULL) return;
        ENetBuffer b;
        char c = 0;
        b.data = &c;
        b.dataLength = 1;
        enet_socket_send(wakefd, &wakeaddr, &b, 1);
    }

    static void pushcommand(command *c)
    {
        { lock l(cmdlock); commands.add(c); }
        wake();
    }

    static void pushevent(event *e)
    {
        lock l(evlock);
        events.add(e);
    }

    // the network thread can't print, so what it has to say goes to the main thread
    static void netlog(const char *fmt, ...)
    {
        if(cfg.verbose < 2) return;
        event *e = new event;
        e->type = EV_LOG;
        bigstring msg;
        va_list args;
        va_start(args, fmt);
        vformatstring(msg, fmt, args);
        va_end(args);
        e->error = newstring(msg);
        pushevent(e);
    }

    static const char *addrstr(uint ip, string &buf)
    {
        ENetAddress a;
        a.host = ip;
        a.port = 0;
        if(enet_address_get_host_ip(&a, buf, sizeof(buf)) < 0) copystring(buf, "unknown");
        return buf;
    }

    void getevents(vector<event *> &evs)
    {
        lock l(evlock);
        loopv(events) evs.add(events[i]);
        events.setsize(0);
    }

    void setconfig(config &c)
    {
        lock l(cmdlock);
        sharedcfg = c;
    }

    // SHA-1 and base64, needed for WebSocket handshakes
    static void dosha1(const uchar *data, int len, uchar digest[20])
    {
        mbedtls_sha1(data, len, digest);
    }

    static void dobase64(vector<char> &buf, const uchar *data, int len)
    {
        static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for(int i = 0; i < len; i += 3)
        {
            uint v = uint(data[i])<<16;
            if(i+1 < len) v |= uint(data[i+1])<<8;
            if(i+2 < len) v |= uint(data[i+2]);
            buf.add(chars[(v>>18)&63]);
            buf.add(chars[(v>>12)&63]);
            buf.add(i+1 < len ? chars[(v>>6)&63] : '=');
            buf.add(i+2 < len ? chars[v&63] : '=');
        }
    }

    static void wsaccept(const char *key, string &result)
    {
        defformatstring(s, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
        uchar digest[20];
        dosha1((const uchar *)s, strlen(s), digest);
        vector<char> b;
        dobase64(b, digest, 20);
        b.add('\0');
        copystring(result, b.getbuf());
    }

    // the system's own randomness, as session tokens and keys come from here when the TLS generator isn't running
    static void osrandom(uchar *buf, int len)
    {
        int got = 0;
#ifdef WIN32
        if(BCRYPT_SUCCESS(BCryptGenRandom(NULL, buf, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG))) got = len;
#else
        FILE *f = fopen("/dev/urandom", "rb");
        if(f) { got = int(fread(buf, 1, len, f)); fclose(f); }
#endif
        for(int i = got; i < len; i++) buf[i] = uchar(rnd(256));
    }

    static bool tlsinit = false, clientready = false;
    static mbedtls_entropy_context entropy;
    static mbedtls_ctr_drbg_context drbg;
    // a new set of trusted certificates replaces the config, the old one stays until shutdown as handshakes may still use it
    struct tlsclient
    {
        mbedtls_ssl_config conf;
        mbedtls_x509_crt chain;

        tlsclient()
        {
            mbedtls_x509_crt_init(&chain);
            mbedtls_ssl_config_init(&conf);
        }
        ~tlsclient()
        {
            mbedtls_ssl_config_free(&conf);
            mbedtls_x509_crt_free(&chain);
        }
    };
    static tlsclient *curclient = NULL;
    static vector<tlsclient *> oldclients;
    static mbedtls_ssl_ticket_context tickets;
    static tlsserver *curserver = NULL; // network thread
    static mutex clientlock;

#ifdef WIN32
    static void tlsmutexinit(mbedtls_threading_mutex_t *m) { InitializeSRWLock((PSRWLOCK)&m->lock); m->is_valid = 1; }
    static void tlsmutexfree(mbedtls_threading_mutex_t *m) { m->is_valid = 0; }
    static int tlsmutexlock(mbedtls_threading_mutex_t *m) { if(!m->is_valid) return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA; AcquireSRWLockExclusive((PSRWLOCK)&m->lock); return 0; }
    static int tlsmutexunlock(mbedtls_threading_mutex_t *m) { if(!m->is_valid) return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA; ReleaseSRWLockExclusive((PSRWLOCK)&m->lock); return 0; }
#endif

    static const char *tlserror(int ret, char *buf, int len)
    {
        mbedtls_strerror(ret, buf, len);
        return buf;
    }

    static void tlsrandom(uchar *buf, int len)
    {
        if(tlsinit && !mbedtls_ctr_drbg_random(&drbg, buf, len)) return;
        osrandom(buf, len);
    }

    static bool starttls()
    {
        if(tlsinit) return true;
#ifdef WIN32
        mbedtls_threading_set_alt(tlsmutexinit, tlsmutexfree, tlsmutexlock, tlsmutexunlock);
#endif
        if(psa_crypto_init() != PSA_SUCCESS)
        {
            conoutf(colourred, "TLS: unable to initialise the crypto library");
            return false;
        }
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        static const char pers[] = "redeclipse";
        int ret = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, (const uchar *)pers, sizeof(pers)-1);
        if(ret)
        {
            char err[128];
            conoutf(colourred, "TLS: unable to seed the random generator: %s", tlserror(ret, err, sizeof(err)));
            mbedtls_ctr_drbg_free(&drbg);
            mbedtls_entropy_free(&entropy);
            return false;
        }
        mbedtls_ssl_ticket_init(&tickets);
        mbedtls_ssl_ticket_setup(&tickets, mbedtls_ctr_drbg_random, &drbg, MBEDTLS_CIPHER_AES_256_GCM, 86400);
        tlsinit = true;
        return true;
    }

    static void stoptls()
    {
        if(!tlsinit) return;
        if(curserver) { if(--curserver->refs <= 0) delete curserver; curserver = NULL; }
        DELETEP(curclient);
        oldclients.deletecontents();
        mbedtls_ssl_ticket_free(&tickets);
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_psa_crypto_free();
#ifdef WIN32
        mbedtls_threading_free_alt();
#endif
        tlsinit = clientready = false;
    }

    bool tlsready() { return tlsinit; }

    static int addcafile(mbedtls_x509_crt &chain, const char *file)
    {
        int ret = mbedtls_x509_crt_parse_file(&chain, file);
        if(ret < 0) return 0;
        int count = 0;
        for(mbedtls_x509_crt *c = &chain; c && c->version; c = c->next) count++;
        return count;
    }

    // called from the main thread, before the first outgoing TLS connection uses it
    bool cafile(const char *file)
    {
        if(!tlsinit) return false;
        tlsclient *n = new tlsclient;
        mbedtls_ssl_config_defaults(&n->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        mbedtls_ssl_conf_rng(&n->conf, mbedtls_ctr_drbg_random, &drbg);
        mbedtls_ssl_conf_authmode(&n->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        int count = 0;
        if(file && *file) count = addcafile(n->chain, file);
        else
        {
#ifdef WIN32
            HCERTSTORE store = CertOpenSystemStoreA(0, "ROOT");
            if(store)
            {
                PCCERT_CONTEXT ctx = NULL;
                while((ctx = CertEnumCertificatesInStore(store, ctx)) != NULL)
                    if(!mbedtls_x509_crt_parse_der(&n->chain, ctx->pbCertEncoded, ctx->cbCertEncoded)) count++;
                CertCloseStore(store, 0);
            }
#else
            static const char * const paths[] = {
                "/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt", "/etc/ssl/ca-bundle.pem",
                "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem", "/etc/ssl/cert.pem", "/usr/local/etc/ssl/cert.pem", NULL
            };
            for(int i = 0; paths[i] && !count; i++) count = addcafile(n->chain, paths[i]);
#endif
            if(!count)
            {
                const char *bundle = findfile("config/cacert.pem", "r");
                if(bundle) count = addcafile(n->chain, bundle);
            }
        }
        mbedtls_ssl_conf_ca_chain(&n->conf, &n->chain, NULL);
        {
            lock l(clientlock);
            if(curclient) oldclients.add(curclient);
            curclient = n;
            clientready = count > 0;
        }
        if(clientready) conoutf(colourwhite, "TLS: loaded %d trusted certificates%s%s", count, file && *file ? " from " : "", file && *file ? file : "");
        else conoutf(colourred, "TLS: no trusted certificates found%s%s, https:// and wss:// will fail", file && *file ? " in " : "", file && *file ? file : "");
        return clientready;
    }

    // called from the main thread, the result goes to the network thread
    bool servercert(const char *certfile, const char *keyfile, char *fingerprint, int fplen)
    {
        if(!tlsinit) return false;
        tlsserver *s = new tlsserver;
        char err[128];
        int ret = mbedtls_x509_crt_parse_file(&s->chain, certfile);
        if(ret)
        {
            conoutf(colourred, "TLS: unable to load certificate %s: %s", certfile, ret < 0 ? tlserror(ret, err, sizeof(err)) : "some certificates were invalid");
            delete s;
            return false;
        }
        ret = mbedtls_pk_parse_keyfile(&s->key, keyfile, NULL, mbedtls_ctr_drbg_random, &drbg);
        if(ret)
        {
            conoutf(colourred, "TLS: unable to load key %s: %s", keyfile, tlserror(ret, err, sizeof(err)));
            delete s;
            return false;
        }
        mbedtls_ssl_config_defaults(&s->conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        mbedtls_ssl_conf_rng(&s->conf, mbedtls_ctr_drbg_random, &drbg);
        mbedtls_ssl_conf_session_tickets_cb(&s->conf, mbedtls_ssl_ticket_write, mbedtls_ssl_ticket_parse, &tickets);
        ret = mbedtls_pk_check_pair(&s->chain.pk, &s->key, mbedtls_ctr_drbg_random, &drbg);
        if(!ret) ret = mbedtls_ssl_conf_own_cert(&s->conf, &s->chain, &s->key);
        if(ret)
        {
            conoutf(colourred, "TLS: certificate %s does not match key %s: %s", certfile, keyfile, tlserror(ret, err, sizeof(err)));
            delete s;
            return false;
        }
        if(fingerprint && fplen > 0)
        {
            uchar digest[32];
            mbedtls_sha256(s->chain.raw.p, s->chain.raw.len, digest, 0);
            fingerprint[0] = '\0';
            loopi(32) if(i*2+2 < fplen) { defformatstring(hex, "%02x", digest[i]); concatstring(fingerprint, hex, fplen); }
        }
        command *c = new command;
        c->type = CMD_SERVERCERT;
        c->server = s;
        pushcommand(c);
        return true;
    }

    void clearservercert()
    {
        if(!tlsinit) return;
        command *c = new command;
        c->type = CMD_SERVERCERT;
        pushcommand(c);
    }

    static int tlssend(void *ctx, const uchar *buf, size_t len)
    {
        conn *c = (conn *)ctx;
        int r = socksend(c->fd, buf, int(min(len, size_t(1<<20))));
        if(r > 0) return r;
        if(!r) return MBEDTLS_ERR_SSL_WANT_WRITE;
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }

    static int tlsrecv(void *ctx, uchar *buf, size_t len)
    {
        conn *c = (conn *)ctx;
        int r = sockrecv(c->fd, buf, int(min(len, size_t(1<<20))));
        if(r > 0) return r;
        if(!r) return MBEDTLS_ERR_SSL_WANT_READ;
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }

    static int pinverify(void *ctx, mbedtls_x509_crt *crt, int depth, uint32_t *flags)
    {
        conn *c = (conn *)ctx;
        if(!c->fetch || !c->fetch->pin) return 0;
        if(depth) { *flags = 0; return 0; } // the leaf alone decides, so a chain to an untrusted root still pins
        uchar digest[32];
        mbedtls_sha256(crt->raw.p, crt->raw.len, digest, 0);
        string hex = "";
        loopi(32) { defformatstring(h, "%02x", digest[i]); concatstring(hex, h); }
        if(!strcasecmp(hex, c->fetch->pin)) *flags = 0;
        else *flags |= MBEDTLS_X509_BADCERT_NOT_TRUSTED;
        return 0;
    }

    static bool tlssetup(conn *c, mbedtls_ssl_config *conf)
    {
        c->ssl = new mbedtls_ssl_context;
        mbedtls_ssl_init(c->ssl);
        if(mbedtls_ssl_setup(c->ssl, conf))
        {
            mbedtls_ssl_free(c->ssl);
            DELETEP(c->ssl);
            return false;
        }
        mbedtls_ssl_set_bio(c->ssl, c, tlssend, tlsrecv, NULL);
        c->tls = true;
        return true;
    }

    void wsframe(vector<char> &buf, int opcode, const char *data, int len, bool mask)
    {
        buf.add(char(0x80|(opcode&0x0F)));
        uchar m = mask ? 0x80 : 0;
        if(len < 126) buf.add(char(m|len));
        else if(len < 65536) { buf.add(char(m|126)); buf.add(char((len>>8)&0xFF)); buf.add(char(len&0xFF)); }
        else
        {
            buf.add(char(m|127));
            loopi(8) buf.add(char(i < 4 ? 0 : (uint(len)>>((7-i)*8))&0xFF));
        }
        if(mask)
        {
            uchar key[4];
            tlsrandom(key, 4);
            loopi(4) buf.add(char(key[i]));
            int start = buf.length();
            buf.put(data, len);
            loopi(len) buf[start+i] ^= key[i&3];
        }
        else buf.put(data, len);
    }

    static conn *findconn(int id)
    {
        loopv(conns) if(conns[i]->id == id) return conns[i];
        return NULL;
    }

    static void queue(conn *c, netbuf *b)
    {
        netout &o = c->out.add();
        o.buf = b;
        o.pos = 0;
        c->queued += b->len;
    }

    static void discard(conn *c)
    {
        loopv(c->out) unref(c->out[i].buf);
        c->out.setsize(0);
        c->queued = 0;
    }

    static void queuedata(conn *c, const char *data, int len)
    {
        queue(c, new netbuf(data, len));
    }

    static void queuestr(conn *c, const char *s)
    {
        queuedata(c, s, strlen(s));
    }

    static void fetchfailed(conn *c, const char *fmt, ...);

    static void closeconn(int n, const char *why = NULL)
    {
        conn *c = conns[n];
        if(c->busy) return; // a worker owns it, closed when it comes back
        if(c->ssl)
        {
            if(c->state == CS_OPEN) mbedtls_ssl_close_notify(c->ssl);
            mbedtls_ssl_free(c->ssl);
            DELETEP(c->ssl);
        }
        if(c->server && --c->server->refs <= 0) delete c->server;
        c->server = NULL;
        if(c->fd != ENET_SOCKET_NULL) enet_socket_destroy(c->fd);
        c->fd = ENET_SOCKET_NULL;
        discard(c);
        if(c->role == ROLE_SERVER)
        {
            int *count = peraddr.access(c->ip);
            if(count && --*count <= 0) peraddr.remove(c->ip);
            event *e = new event;
            e->type = c->proto == PROTO_WS ? EV_WSCLOSE : EV_CLOSED;
            e->id = c->id;
            e->code = c->gotclose ? 1000 : 1006;
            pushevent(e);
        }
        else if(c->fetch)
        {
            if(c->proto == PROTO_WS)
            {
                event *e = new event;
                e->type = EV_WSCLOSE;
                e->id = c->id;
                e->code = c->gotclose ? 1000 : 1006;
                pushevent(e);
            }
            else fetchfailed(c, "%s", why ? why : "connection closed");
            DELETEP(c->fetch);
        }
        { lock l(evlock); progresses.remove(c->id); }
        DELETEP(c->msg);
        delete c;
        conns.remove(n);
    }

    static void fetchfailed(conn *c, const char *fmt, ...)
    {
        if(!c->fetch) return;
        event *e = new event;
        e->type = c->fetch->ws ? EV_WSCLOSE : EV_FETCHED;
        e->id = c->id;
        e->code = 1006;
        bigstring msg;
        va_list args;
        va_start(args, fmt);
        vformatstring(msg, fmt, args);
        va_end(args);
        e->error = newstring(msg);
        pushevent(e);
        DELETEP(c->fetch);
    }

    // canned answers the network thread gives on its own, the connection closes after
    static void reject(conn *c, int status)
    {
        defformatstring(s, "HTTP/1.1 %d %s\r\nConnection: close\r\nContent-Length: 0\r\n\r\n", status, http::statusname(status));
        queuestr(c, s);
        c->closing = true;
    }

    static const char *nextline(conn *c, int &len)
    {
        char *start = c->in.getbuf() + c->inpos, *end = c->in.getbuf() + c->in.length();
        char *nl = (char *)memchr(start, '\n', end - start);
        if(!nl) { len = end - start; return NULL; }
        len = nl - start;
        c->inpos += len + 1;
        if(len > 0 && start[len-1] == '\r') len--;
        return start;
    }

    static bool parsestart(conn *c, const char *line, int len)
    {
        httpmessage *m = c->msg;
        const char *end = line + len;
        if(c->role == ROLE_SERVER)
        {
            const char *sp = (const char *)memchr(line, ' ', len);
            if(!sp) return false;
            m->method = http::findmethod(line, sp - line);
            const char *target = sp + 1, *sp2 = (const char *)memchr(target, ' ', end - target);
            if(!sp2 || sp2 == target) return false;
            m->target = newstring(target, sp2 - target);
            const char *ver = sp2 + 1;
            if(end - ver != 8 || strncmp(ver, "HTTP/1.", 7)) return false;
            m->version = ver[7] == '0' ? 10 : 11;
        }
        else
        {
            if(len < 12 || strncmp(line, "HTTP/1.", 7) || line[8] != ' ') return false;
            m->version = line[7] == '0' ? 10 : 11;
            m->status = atoi(line + 9);
            m->reason = newstring(len > 13 ? line + 13 : "", len > 13 ? len - 13 : 0);
            if(m->status < 100 || m->status > 999) return false;
        }
        return true;
    }

    static bool parseheader(conn *c, const char *line, int len)
    {
        if(c->msg->headers.length() >= cfg.maxheaders) return false;
        const char *colon = (const char *)memchr(line, ':', len);
        if(!colon || colon == line) return false;
        // a name is a token and a value has no control characters but tabs, as proxies that read them otherwise see other headers
        for(const char *s = line; s < colon; s++) if(!isalnum(uchar(*s)) && (!*s || !strchr("!#$%&'*+-.^_`|~", *s))) return false;
        for(const char *s = colon + 1; s < line + len; s++) if((uchar(*s) < 0x20 && *s != '\t') || *s == 0x7F) return false;
        const char *val = colon + 1, *end = line + len;
        while(val < end && (*val == ' ' || *val == '\t')) val++;
        while(end > val && (end[-1] == ' ' || end[-1] == '\t')) end--;
        c->msg->headers.add(line, val, colon - line, end - val);
        return true;
    }

    static bool sinkbody(conn *c, const char *data, int len)
    {
        c->recvd += len;
        if(c->fetch && c->fetch->fp)
        {
            if(c->recvd > c->fetch->maxsize) { fetchfailed(c, "response from %s is over %d bytes", c->fetch->url.host, c->fetch->maxsize); return false; }
            if(fwrite(data, 1, len, c->fetch->fp) != size_t(len)) { fetchfailed(c, "unable to write %s", c->fetch->file); return false; }
        }
        else
        {
            int limit = c->role == ROLE_SERVER ? cfg.maxbody : (c->fetch ? c->fetch->maxsize : cfg.maxbody);
            if(c->msg->body.length() + len > limit)
            {
                if(c->role == ROLE_CLIENT && c->fetch) fetchfailed(c, "response from %s is over %d bytes", c->fetch->url.host, limit);
                return false;
            }
            c->msg->body.put(data, len);
        }
        if(c->role == ROLE_CLIENT)
        {
            lock l(evlock);
            progressinfo &p = progresses[c->id];
            p.recvd = c->recvd;
            p.total = c->total;
        }
        return true;
    }

    static bool inflatebody(vector<char> &body, int limit)
    {
        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        if(inflateInit2(&zs, 15+32) != Z_OK) return false;
        vector<char> out;
        zs.next_in = (Bytef *)body.getbuf();
        zs.avail_in = body.length();
        int ret = Z_OK;
        while(ret == Z_OK)
        {
            out.reserve(max(4096, body.length()*2));
            zs.next_out = (Bytef *)out.getbuf() + out.length();
            zs.avail_out = out.capacity() - out.length();
            int before = zs.avail_out;
            ret = inflate(&zs, Z_NO_FLUSH);
            out.advance(before - zs.avail_out);
            if(out.length() > limit) { ret = Z_MEM_ERROR; break; }
        }
        inflateEnd(&zs);
        if(ret != Z_STREAM_END) return false;
        body.setsize(0);
        body.put(out.getbuf(), out.length());
        return true;
    }

    static void startfetch(conn *c);

    // a full request or response has arrived
    static void finished(conn *c)
    {
        httpmessage *m = c->msg;
        c->msg = NULL;
        c->pstate = P_HEAD;
        if(c->role == ROLE_SERVER)
        {
            c->waiting = true;
            c->msgstart = 0;
            event *e = new event;
            e->type = EV_REQUEST;
            e->id = c->id;
            e->ip = c->ip;
            e->secure = c->tls;
            e->msg = m;
            pushevent(e);
            return;
        }
        job *f = c->fetch;
        if(!f) { delete m; return; }
        if(m->status >= 300 && m->status < 400 && m->status != 304 && f->redirects < HTTP_REDIRECTS && !f->ws)
        {
            const char *loc = m->headers.get("Location");
            if(loc && *loc)
            {
                httpurl next;
                bool ok = false;
                if(loc[0] == '/')
                {
                    defformatstring(full, "%s://%s:%d%s", f->url.tls ? "https" : "http", f->url.host, f->url.port, loc);
                    ok = next.parse(full);
                }
                else ok = next.parse(loc);
                if(ok && f->url.tls && !next.tls) ok = false; // never from https down to plain http, what came over checked TLS must stay so
                if(ok)
                {
                    bool samesite = !strcasecmp(next.host, f->url.host) && next.port == f->url.port && next.tls == f->url.tls;
                    job *nf = new job;
                    nf->method = (m->status == 303 || ((m->status == 301 || m->status == 302) && f->method == HTTP_POST)) ? HTTP_GET : f->method;
                    if(nf->method == f->method) nf->body.put(f->body.getbuf(), f->body.length());
                    nf->maxsize = f->maxsize;
                    nf->redirects = f->redirects + 1;
                    nf->headers.copy(f->headers);
                    if(!samesite) // credentials are for the server they were meant for
                    {
                        nf->headers.remove("Authorization");
                        nf->headers.remove("Cookie");
                        nf->headers.remove("Proxy-Authorization");
                    }
                    nf->url.tls = next.tls;
                    nf->url.port = next.port;
                    copystring(nf->url.host, next.host);
                    nf->url.path = newstring(next.path);
                    if(f->file) nf->file = newstring(f->file);
                    if(f->pin && samesite) nf->pin = newstring(f->pin);
                    if(f->fp) { fclose(f->fp); f->fp = NULL; }
                    delete m;
                    conn *n = new conn;
                    n->id = c->id;
                    n->role = ROLE_CLIENT;
                    n->fetch = nf;
                    n->opened = n->lastrecv = netmillis;
                    conns.add(n);
                    c->id = -1;
                    c->closing = true;
                    discard(c);
                    DELETEP(c->fetch);
                    startfetch(n);
                    return;
                }
            }
        }
        if(f->gzip && m->headers.hastoken("Content-Encoding", "gzip") && !inflatebody(m->body, f->maxsize))
        {
            delete m;
            fetchfailed(c, "unable to decompress the response");
            c->closing = true;
            return;
        }
        if(f->fp)
        {
            fclose(f->fp);
            f->fp = NULL;
            m->file = f->partial;
            f->partial = NULL;
        }
        event *e = new event;
        e->type = EV_FETCHED;
        e->id = c->id;
        e->msg = m;
        pushevent(e);
        DELETEP(c->fetch);
        c->closing = true;
    }

    static bool beginbody(conn *c)
    {
        httpmessage *m = c->msg;
        c->recvd = 0;
        c->total = -1;
        const char *te = m->headers.get("Transfer-Encoding"), *cl = m->headers.get("Content-Length");
        if(te && cl) return false; // both is how requests get smuggled past proxies
        loopv(m->headers.list) if(!strcasecmp(m->headers.list[i].name, "Content-Length") && strcmp(m->headers.list[i].value, cl)) return false; // and so are two that disagree
        if(te && m->headers.hastoken("Transfer-Encoding", "chunked")) { c->pstate = P_CHUNKSIZE; return true; }
        if(te) return false;
        if(cl)
        {
            if(!isdigit(uchar(*cl))) return false; // digits alone, where strtoll would take spaces and a sign
            char *end = NULL;
            long long n = strtoll(cl, &end, 10);
            if(n < 0 || !end || *end || end == cl) return false;
            c->remaining = n;
            c->total = int(min(n, (long long)INT_MAX));
            if(c->role == ROLE_SERVER && n > cfg.maxbody) return false;
            if(c->role == ROLE_CLIENT && c->fetch && n > c->fetch->maxsize) { fetchfailed(c, "response from %s is over %d bytes", c->fetch->url.host, c->fetch->maxsize); return false; }
            c->pstate = n ? P_BODY : P_DONE;
            return true;
        }
        if(c->role == ROLE_SERVER) { c->pstate = P_DONE; return true; }
        // responses without a length run until the connection closes, unless they can't have a body
        if(m->status < 200 || m->status == 204 || m->status == 304 || (c->fetch && c->fetch->method == HTTP_HEAD)) c->pstate = P_DONE;
        else c->pstate = P_UNTILCLOSE;
        return true;
    }

    static bool clientupgrade(conn *c)
    {
        httpmessage *m = c->msg;
        if(m->status != 101 || !m->headers.hastoken("Upgrade", "websocket")) return false;
        string expect;
        wsaccept(c->fetch->wskey, expect);
        const char *got = m->headers.get("Sec-WebSocket-Accept");
        if(!got || strcmp(got, expect)) return false;
        DELETEP(c->msg);
        c->proto = PROTO_WS;
        c->maxmessage = cfg.maxmessage;
        event *e = new event;
        e->type = EV_WSOPEN;
        e->id = c->id;
        pushevent(e);
        return true;
    }

    // returns false if the connection should close
    static bool parsehttp(conn *c)
    {
        while(c->inpos < c->in.length() || c->pstate == P_DONE)
        {
            if(c->role == ROLE_SERVER && c->waiting) return true;
            switch(c->pstate)
            {
                case P_HEAD: case P_TRAILER:
                {
                    int len = 0;
                    const char *line = nextline(c, len);
                    if(!line)
                    {
                        if(len > cfg.maxline) { if(c->role == ROLE_SERVER) { reject(c, 431); return true; } return false; }
                        return true;
                    }
                    if(len > cfg.maxline) { if(c->role == ROLE_SERVER) { reject(c, 431); return true; } return false; }
                    c->headbytes += len;
                    if(c->pstate == P_TRAILER)
                    {
                        if(!len) c->pstate = P_DONE;
                        break;
                    }
                    if(!c->msg)
                    {
                        if(!len) break; // stray line breaks between requests
                        c->msg = new httpmessage;
                        c->headbytes = 0;
                        if(!parsestart(c, line, len)) { if(c->role == ROLE_SERVER) { reject(c, 400); return true; } return false; }
                        if(c->role == ROLE_SERVER && c->msg->method == HTTP_NONE) { reject(c, 501); return true; }
                        break;
                    }
                    if(len)
                    {
                        if(!parseheader(c, line, len)) { if(c->role == ROLE_SERVER) { reject(c, c->msg->headers.length() >= cfg.maxheaders ? 431 : 400); return true; } return false; }
                        break;
                    }
                    // end of the head
                    if(c->role == ROLE_CLIENT)
                    {
                        if(c->fetch && c->fetch->ws) return clientupgrade(c);
                        if(c->msg->status >= 100 && c->msg->status < 200) { DELETEP(c->msg); break; }
                    }
                    if(!beginbody(c)) { if(c->role == ROLE_SERVER) { reject(c, c->remaining > cfg.maxbody ? 413 : 400); return true; } return false; }
                    if(c->role == ROLE_SERVER && c->pstate != P_DONE && c->msg->headers.hastoken("Expect", "100-continue"))
                        queuestr(c, "HTTP/1.1 100 Continue\r\n\r\n");
                    break;
                }
                case P_BODY: case P_CHUNKDATA:
                {
                    int avail = c->in.length() - c->inpos, n = int(min((long long)avail, c->remaining));
                    if(!sinkbody(c, c->in.getbuf() + c->inpos, n)) { if(c->role == ROLE_SERVER) { reject(c, 413); return true; } return false; }
                    c->inpos += n;
                    c->remaining -= n;
                    if(!c->remaining) c->pstate = c->pstate == P_BODY ? P_DONE : P_CHUNKEND;
                    break;
                }
                case P_CHUNKSIZE:
                {
                    int len = 0;
                    const char *line = nextline(c, len);
                    if(!line) return len <= 64;
                    if(!len || !isxdigit(uchar(*line)) || (len > 1 && (line[1] == 'x' || line[1] == 'X'))) return false; // hex digits alone, no spaces, sign or 0x
                    char *end = NULL;
                    long long n = strtoll(line, &end, 16);
                    if(n < 0 || end == line || (end < line + len && *end != ';' && *end != ' ' && *end != '\t')) return false;
                    c->remaining = n;
                    c->pstate = n ? P_CHUNKDATA : P_TRAILER;
                    break;
                }
                case P_CHUNKEND:
                {
                    int len = 0;
                    const char *line = nextline(c, len);
                    if(!line) return len <= 2;
                    if(len) return false;
                    c->pstate = P_CHUNKSIZE;
                    break;
                }
                case P_UNTILCLOSE:
                {
                    int n = c->in.length() - c->inpos;
                    if(!sinkbody(c, c->in.getbuf() + c->inpos, n)) return false;
                    c->inpos += n;
                    break;
                }
                case P_DONE:
                {
                    finished(c);
                    if(c->closing || c->role == ROLE_CLIENT) return true;
                    break;
                }
            }
            if(c->closing) return true;
        }
        return true;
    }

    static void wsdeliver(conn *c, int opcode, const char *data, int len)
    {
        event *e = new event;
        e->type = EV_WSMESSAGE;
        e->id = c->id;
        e->opcode = opcode;
        e->data.put(data, len);
        pushevent(e);
    }

    static void wscontrol(conn *c, int opcode, const char *data, int len)
    {
        vector<char> frame;
        wsframe(frame, opcode, data, len, c->role == ROLE_CLIENT);
        queuedata(c, frame.getbuf(), frame.length());
    }

    static bool parsews(conn *c)
    {
        for(;;)
        {
            uchar *p = (uchar *)c->in.getbuf() + c->inpos;
            int avail = c->in.length() - c->inpos;
            if(avail < 2) return true;
            bool fin = (p[0]&0x80) != 0, masked = (p[1]&0x80) != 0;
            int opcode = p[0]&0x0F, hdr = 2;
            if(p[0]&0x70) return false;
            if(masked != (c->role == ROLE_SERVER)) return false;
            long long len = p[1]&0x7F;
            if(len == 126)
            {
                if(avail < 4) return true;
                len = (p[2]<<8)|p[3];
                hdr = 4;
            }
            else if(len == 127)
            {
                if(avail < 10) return true;
                len = 0;
                loopi(8) len = (len<<8)|p[2+i];
                hdr = 10;
            }
            if(len < 0 || len > c->maxmessage) { wscontrol(c, WS_CLOSE, "\x03\xF1", 2); c->sentclose = c->closing = true; return true; }
            if(masked) hdr += 4;
            if(avail < hdr + len) return true;
            char *data = (char *)p + hdr;
            if(masked) { uchar *key = p + hdr - 4; loopi(len) data[i] ^= key[i&3]; }
            c->inpos += hdr + int(len);
            if(opcode >= 8)
            {
                if(!fin || len > 125) return false;
                switch(opcode)
                {
                    case WS_PING: wscontrol(c, WS_PONG, data, int(len)); break;
                    case WS_PONG: c->sentping = false; break;
                    case WS_CLOSE:
                        c->gotclose = true;
                        if(!c->sentclose) { wscontrol(c, WS_CLOSE, data, min(int(len), 2)); c->sentclose = true; }
                        c->closing = true;
                        return true;
                    default: return false;
                }
                continue;
            }
            if(opcode == WS_CONT)
            {
                if(c->wsop < 0) return false;
                if(c->wsmsg.length() + len > c->maxmessage) { wscontrol(c, WS_CLOSE, "\x03\xF1", 2); c->sentclose = c->closing = true; return true; }
                c->wsmsg.put(data, int(len));
                if(fin) { wsdeliver(c, c->wsop, c->wsmsg.getbuf(), c->wsmsg.length()); c->wsmsg.setsize(0); c->wsop = -1; }
            }
            else if(opcode == WS_TEXT || opcode == WS_BINARY)
            {
                if(c->wsop >= 0) return false;
                if(fin) wsdeliver(c, opcode, data, int(len));
                else { c->wsop = opcode; c->wsmsg.setsize(0); c->wsmsg.put(data, int(len)); }
            }
            else return false;
        }
    }

    static bool parse(conn *c)
    {
        bool ok = c->proto == PROTO_WS ? parsews(c) : parsehttp(c);
        if(c->proto == PROTO_HTTP && c->pstate == P_HEAD && !c->msg && c->role == ROLE_SERVER && c->in.length() - c->inpos > cfg.maxline*2 && !c->waiting) ok = false;
        // the upgrade switched protocol part way through the buffer
        if(ok && c->proto == PROTO_WS && c->inpos < c->in.length() && !c->closing) ok = parsews(c);
        if(c->inpos > 0 && (c->inpos >= c->in.length() || c->inpos > 65536))
        {
            int left = c->in.length() - c->inpos;
            if(left > 0) memmove(c->in.getbuf(), c->in.getbuf() + c->inpos, left);
            c->in.setsize(left);
            c->inpos = 0;
        }
        return ok;
    }

    // returns false if the connection should close
    static bool readconn(conn *c)
    {
        char buf[16384];
        loopk(64)
        {
            int r;
            if(c->tls)
            {
                r = mbedtls_ssl_read(c->ssl, (uchar *)buf, sizeof(buf));
                if(r == MBEDTLS_ERR_SSL_WANT_READ) break;
                if(r == MBEDTLS_ERR_SSL_WANT_WRITE) { c->wantwrite = true; break; }
                if(r == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) continue;
                if(r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || r == 0) r = -1;
                else if(r < 0) r = -1;
            }
            else
            {
                r = sockrecv(c->fd, buf, sizeof(buf));
                if(!r) break;
            }
            if(r < 0)
            {
                if(c->role == ROLE_CLIENT && c->pstate == P_UNTILCLOSE && c->msg) { c->pstate = P_DONE; parse(c); }
                return false;
            }
            c->lastrecv = netmillis;
            c->sentping = false;
            if(c->role == ROLE_SERVER && c->proto == PROTO_HTTP && !c->waiting && !c->msgstart) c->msgstart = netmillis; // when the request's first byte came
            c->in.put(buf, r);
            if(!parse(c)) return false;
            if(c->closing || (c->role == ROLE_SERVER && c->waiting && c->in.length() - c->inpos > cfg.maxline + cfg.maxbody)) break;
        }
        return true;
    }

    // returns false if the connection should close
    static bool flushconn(conn *c)
    {
        c->wantwrite = false;
        while(c->out.length())
        {
            netout &o = c->out[0];
            int len = o.buf->len - o.pos, r;
            if(c->tls)
            {
                r = mbedtls_ssl_write(c->ssl, (const uchar *)o.buf->data + o.pos, min(len, 16384));
                if(r == MBEDTLS_ERR_SSL_WANT_WRITE || r == MBEDTLS_ERR_SSL_WANT_READ) { c->wantwrite = true; return true; }
                if(r < 0) return false;
            }
            else
            {
                r = socksend(c->fd, o.buf->data + o.pos, len);
                if(r < 0) return false;
                if(!r) { c->wantwrite = true; return true; }
            }
            o.pos += r;
            c->queued -= r;
            c->lastsend = netmillis;
            if(o.pos >= o.buf->len)
            {
                unref(o.buf);
                c->out.remove(0);
            }
        }
        return true;
    }

    static void sendrequest(conn *c)
    {
        job *f = c->fetch;
        vector<char> req;
        const char *path = f->url.path && *f->url.path ? f->url.path : "/";
        bool defport = f->url.port == (f->url.tls ? HTTPS_PORT : HTTP_PORT);
        defformatstring(head, "%s %s HTTP/1.1\r\nHost: %s", http::methodnames[f->method], path, f->url.host);
        req.put(head, strlen(head));
        if(!defport) { defformatstring(port, ":%d", f->url.port); req.put(port, strlen(port)); }
        req.put("\r\n", 2);
        if(f->ws)
        {
            uchar key[16];
            tlsrandom(key, 16);
            vector<char> b;
            dobase64(b, key, 16);
            b.add('\0');
            copystring(f->wskey, b.getbuf());
            defformatstring(ws, "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: %s\r\n", f->wskey);
            req.put(ws, strlen(ws));
        }
        else
        {
            req.put("Connection: close\r\n", 19);
            if(!f->file && !f->headers.find("Accept-Encoding")) { req.put("Accept-Encoding: gzip\r\n", 23); f->gzip = true; }
        }
        loopv(f->headers.list)
        {
            httppair &h = f->headers.list[i];
            if(!strcasecmp(h.name, "Host") || !strcasecmp(h.name, "Content-Length") || !strcasecmp(h.name, "Connection") || !strcasecmp(h.name, "Transfer-Encoding")) continue;
            if(!*h.name || strpbrk(h.name, "\r\n: \t") || strpbrk(h.value, "\r\n")) continue; // a script's header can't add lines of its own
            req.put(h.name, strlen(h.name));
            req.put(": ", 2);
            req.put(h.value, strlen(h.value));
            req.put("\r\n", 2);
        }
        if(f->body.length() || f->method == HTTP_POST || f->method == HTTP_PUT || f->method == HTTP_PATCH)
        {
            defformatstring(cl, "Content-Length: %d\r\n", f->body.length());
            req.put(cl, strlen(cl));
        }
        req.put("\r\n", 2);
        req.put(f->body.getbuf(), f->body.length());
        queuedata(c, req.getbuf(), req.length());
        c->pstate = P_HEAD;
    }

    static void connected(conn *c)
    {
        c->state = CS_OPEN;
        if(c->role == ROLE_CLIENT && c->fetch)
        {
            if(c->fetch->file)
            {
                defformatstring(part, "%s.part", c->fetch->file);
                c->fetch->fp = fopen(part, "wb");
                if(!c->fetch->fp) { fetchfailed(c, "unable to write %s", part); c->closing = true; return; }
                c->fetch->partial = newstring(part);
            }
            sendrequest(c);
        }
    }

    static void pushtask(conn *c, int type)
    {
        c->busy = true;
        task t;
        t.type = type;
        t.c = c;
        t.result = 0;
        { lock l(tasklock); tasks.add(t); }
        tasksignal.post();
    }

    static void runtask(task &t)
    {
        conn *c = t.c;
        switch(t.type)
        {
            case TASK_RESOLVE:
            {
                job *f = c->fetch;
                f->numaddrs = 0;
                ENetAddress a;
                a.port = f->url.port;
                if(enet_address_set_host(&a, f->url.host) >= 0) f->addrs[f->numaddrs++] = a;
                t.result = f->numaddrs;
                break;
            }
            case TASK_HANDSHAKE:
            {
                t.result = mbedtls_ssl_handshake(c->ssl);
                break;
            }
        }
    }

    THREADFUN(workerloop)
    {
        for(;;)
        {
            tasksignal.wait();
            task t;
            {
                lock l(tasklock);
                if(tasks.empty()) continue;
                t = tasks.remove(0);
            }
            if(!t.c) break;
            runtask(t);
            { lock l(donelock); done.add(t); }
            wake();
        }
        THREADRET;
    }

    static void handshake(conn *c);

    static void tryconnect(conn *c)
    {
        job *f = c->fetch;
        while(f->curaddr < f->numaddrs)
        {
            ENetAddress &a = f->addrs[f->curaddr++];
            c->ip = a.host;
            c->fd = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
            if(c->fd == ENET_SOCKET_NULL) continue;
            enet_socket_set_option(c->fd, ENET_SOCKOPT_NONBLOCK, 1);
            socknodelay(c->fd);
            if(enet_socket_connect(c->fd, &a) < 0) { enet_socket_destroy(c->fd); c->fd = ENET_SOCKET_NULL; continue; }
            c->state = CS_CONNECTING;
            c->opened = netmillis;
            return;
        }
        fetchfailed(c, "unable to connect to %s:%d", f->url.host, f->url.port);
        c->closing = true;
    }

    static void startfetch(conn *c)
    {
        c->state = CS_RESOLVING;
        pushtask(c, TASK_RESOLVE);
    }

    static void handshakedone(conn *c, int ret)
    {
        if(!ret)
        {
            connected(c);
            if(c->closing) return;
            if(c->role == ROLE_SERVER && !readconn(c)) c->closing = true;
            else if(c->out.length() && !flushconn(c)) { if(c->role == ROLE_CLIENT) fetchfailed(c, "connection lost while sending"); c->closing = true; discard(c); }
            return;
        }
        if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE || ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
        {
            c->wantwrite = ret == MBEDTLS_ERR_SSL_WANT_WRITE;
            return;
        }
        if(c->role == ROLE_CLIENT)
        {
            char err[128];
            uint flags = mbedtls_ssl_get_verify_result(c->ssl);
            if(flags && flags != uint(-1))
            {
                char info[256];
                mbedtls_x509_crt_verify_info(info, sizeof(info), "", flags);
                for(char *s = info; *s; s++) if(*s == '\n') *s = ' ';
                if(c->fetch && c->fetch->pin) fetchfailed(c, "certificate for %s doesn't match the pinned fingerprint", c->fetch->url.host);
                else fetchfailed(c, "certificate for %s not trusted: %s", c->fetch ? c->fetch->url.host : "?", info);
            }
            else fetchfailed(c, "TLS handshake with %s failed: %s", c->fetch ? c->fetch->url.host : "?", tlserror(ret, err, sizeof(err)));
        }
        else
        {
            char err[128];
            string addr;
            netlog("TLS handshake from %s failed: %s", addrstr(c->ip, addr), tlserror(ret, err, sizeof(err)));
        }
        c->closing = true;
        discard(c);
    }

    static void handshake(conn *c)
    {
        if(cfg.handshakethreads > 0) { pushtask(c, TASK_HANDSHAKE); return; }
        task t;
        t.type = TASK_HANDSHAKE;
        t.c = c;
        runtask(t);
        handshakedone(c, t.result);
    }

    static void detect(conn *c)
    {
        uchar b = 0;
        int r = sockrecv(c->fd, &b, 1, MSG_PEEK);
        if(!r) return;
        if(r < 0) { c->closing = true; discard(c); return; }
        if(b == 0x16 && curserver)
        {
            ratewindow *w = handshakes.access(c->ip);
            if(!w) { w = &handshakes[c->ip]; w->start = netmillis; w->count = 0; } // new entries aren't zeroed
            else if(netmillis - w->start > 10000) { w->start = netmillis; w->count = 0; }
            string addr;
            if(++w->count > cfg.handshakerate) { netlog("refused %s, over %d TLS handshakes in ten seconds", addrstr(c->ip, addr), cfg.handshakerate); c->closing = true; return; }
            if(!tlssetup(c, &curserver->conf)) { netlog("unable to set up TLS for %s", addrstr(c->ip, addr)); c->closing = true; return; }
            c->server = curserver;
            curserver->refs++;
            c->state = CS_HANDSHAKE;
            handshake(c);
            return;
        }
        if(b == 0x16) { string addr; netlog("refused TLS from %s, no certificate is set", addrstr(c->ip, addr)); c->closing = true; return; }
        c->state = CS_OPEN;
    }

    static void acceptconns()
    {
        if(listenfd == ENET_SOCKET_NULL) return;
        loopk(64)
        {
            ENetAddress addr;
            ENetSocket fd = enet_socket_accept(listenfd, &addr);
            if(fd == ENET_SOCKET_NULL) break;
            int *count = peraddr.access(addr.host);
            if(conns.length() >= cfg.maxconns || (count && *count >= cfg.maxperaddr))
            {
                string a;
                netlog("refused %s, at the %s connection limit", addrstr(addr.host, a), conns.length() >= cfg.maxconns ? "total" : "per address");
                enet_socket_destroy(fd);
                continue;
            }
            enet_socket_set_option(fd, ENET_SOCKOPT_NONBLOCK, 1);
            socknodelay(fd);
            conn *c = new conn;
            static int lastid = 0;
            do lastid = lastid >= 0x3FFFFFFF ? 1 : lastid + 1; while(findconn(lastid | 0x40000000)); // wraps, and never onto one still open
            c->id = lastid | 0x40000000;
            c->fd = fd;
            c->ip = addr.host;
            c->role = ROLE_SERVER;
            c->state = CS_DETECT;
            c->opened = c->lastrecv = netmillis;
            if(count) (*count)++;
            else peraddr[addr.host] = 1;
            conns.add(c);
        }
    }

    static void docommand(command *cmd)
    {
        switch(cmd->type)
        {
            case CMD_SEND:
            {
                conn *c = findconn(cmd->id);
                if(!c || c->closing) { unref(cmd->buf); break; }
                if(cmd->buf->len) queue(c, cmd->buf);
                else unref(cmd->buf);
                if(cmd->flags&SEND_UPGRADE)
                {
                    c->proto = PROTO_WS;
                    c->maxmessage = cmd->maxmessage > 0 ? cmd->maxmessage : cfg.maxmessage;
                }
                if(cmd->flags&SEND_CLOSE) c->closing = true;
                if(cmd->flags&(SEND_DONE|SEND_UPGRADE))
                {
                    c->waiting = false;
                    if(!c->closing && !parse(c)) { c->closing = true; discard(c); }
                }
                break;
            }
            case CMD_SHARED:
            {
                int used = 0;
                loopv(cmd->ids)
                {
                    conn *c = findconn(cmd->ids[i]);
                    if(!c || c->closing || c->proto != PROTO_WS) continue;
                    queue(c, cmd->buf);
                    used++;
                }
                cmd->buf->refs -= cmd->ids.length() - used;
                if(cmd->buf->refs <= 0) delete cmd->buf;
                break;
            }
            case CMD_CLOSE:
            {
                conn *c = findconn(cmd->id);
                if(!c) break;
                if(c->proto == PROTO_WS && !c->sentclose && c->state == CS_OPEN)
                {
                    uchar code[2] = { uchar((cmd->flags>>8)&0xFF), uchar(cmd->flags&0xFF) };
                    wscontrol(c, WS_CLOSE, (const char *)code, 2);
                    c->sentclose = true;
                }
                if(c->fetch && c->proto != PROTO_WS) // cancelled, no callback
                {
                    if(c->busy) c->cancelled = true;
                    else DELETEP(c->fetch);
                }
                c->closing = true;
                break;
            }
            case CMD_FETCH:
            {
                conn *c = new conn;
                c->id = cmd->id;
                c->role = ROLE_CLIENT;
                c->fetch = cmd->fetch;
                c->opened = c->lastrecv = netmillis;
                cmd->fetch = NULL;
                conns.add(c);
                startfetch(c);
                break;
            }
            case CMD_LISTEN:
            {
                if(listenfd != ENET_SOCKET_NULL) enet_socket_destroy(listenfd);
                listenfd = cmd->fd;
                break;
            }
            case CMD_UNLISTEN:
            {
                if(listenfd != ENET_SOCKET_NULL) enet_socket_destroy(listenfd);
                listenfd = ENET_SOCKET_NULL;
                loopvrev(conns) if(conns[i]->role == ROLE_SERVER) { conns[i]->closing = true; closeconn(i); }
                break;
            }
            case CMD_SERVERCERT:
            {
                if(curserver && --curserver->refs <= 0) delete curserver;
                curserver = cmd->server;
                cmd->server = NULL;
                break;
            }
        }
        DELETEP(cmd->fetch);
        delete cmd;
    }

    static void checktimers()
    {
        loopv(conns)
        {
            conn *c = conns[i];
            if(c->busy || c->closing) continue;
            uint idle = netmillis - c->lastrecv;
            switch(c->state)
            {
                case CS_RESOLVING: break;
                case CS_CONNECTING: if(netmillis - c->opened > uint(cfg.idletime)*2) { fetchfailed(c, "timed out connecting to %s", c->fetch ? c->fetch->url.host : "?"); c->closing = true; } break;
                case CS_DETECT: case CS_HANDSHAKE: if(idle > uint(cfg.idletime)) { if(c->role == ROLE_CLIENT) fetchfailed(c, "timed out during the TLS handshake"); c->closing = true; } break;
                case CS_OPEN:
                {
                    if(c->proto == PROTO_WS)
                    {
                        if(idle > uint(cfg.pingtime)*2 + 5000) { c->closing = true; break; }
                        if(idle > uint(cfg.pingtime) && !c->sentping) { wscontrol(c, WS_PING, "", 0); c->sentping = true; }
                    }
                    else if(c->role == ROLE_SERVER)
                    {
                        if(!c->waiting && idle > uint(cfg.idletime)) c->closing = true;
                        // a request trickled in a byte at a time still has to arrive in time
                        else if(!c->waiting && c->msgstart && netmillis - c->msgstart > uint(cfg.idletime)*2) { reject(c, 408); string addr; netlog("dropped %s, too slow sending a request", addrstr(c->ip, addr)); }
                    }
                    else if(idle > uint(cfg.idletime)*2) { fetchfailed(c, "timed out waiting for %s", c->fetch ? c->fetch->url.host : "?"); c->closing = true; }
                    break;
                }
            }
            if(c->queued > cfg.maxmessage + (1<<20)) { discard(c); c->closing = true; } // a reader that can't keep up
        }
    }

    THREADFUN(netloop)
    {
        vector<netpollfd> fds;
        vector<conn *> fdconns;
        uint lasttimers = 0;
        for(;;)
        {
            netmillis = getmillis();
            vector<command *> cmds;
            {
                lock l(cmdlock);
                if(quitting) break;
                cfg = sharedcfg;
                loopv(commands) cmds.add(commands[i]);
                commands.setsize(0);
            }
            loopv(cmds) docommand(cmds[i]);
            vector<task> finished;
            {
                lock l(donelock);
                loopv(done) finished.add(done[i]);
                done.setsize(0);
            }
            loopv(finished)
            {
                task &t = finished[i];
                conn *c = t.c;
                c->busy = false;
                if(c->cancelled) DELETEP(c->fetch);
                if(c->closing) continue;
                if(t.type == TASK_RESOLVE)
                {
                    if(!c->fetch) { c->closing = true; continue; }
                    if(t.result <= 0) { fetchfailed(c, "unable to resolve %s", c->fetch->url.host); c->closing = true; continue; }
                    tryconnect(c);
                }
                else handshakedone(c, t.result);
            }
            // close what is finished, once its output has gone
            loopvrev(conns)
            {
                conn *c = conns[i];
                if(!c->closing || c->busy) continue;
                if(!c->closestart) c->closestart = netmillis;
                if(c->out.length() && c->fd != ENET_SOCKET_NULL && c->state == CS_OPEN && netmillis - c->closestart < 5000)
                {
                    if(!flushconn(c)) { discard(c); closeconn(i); }
                    else if(!c->out.length()) closeconn(i);
                    continue;
                }
                closeconn(i);
            }
            fds.setsize(0);
            fdconns.setsize(0);
            netpollfd p;
            p.fd = wakefd;
            p.events = POLLIN;
            p.revents = 0;
            fds.add(p);
            fdconns.add(NULL);
            if(listenfd != ENET_SOCKET_NULL && conns.length() < cfg.maxconns)
            {
                p.fd = listenfd;
                fds.add(p);
                fdconns.add(NULL);
            }
            loopv(conns)
            {
                conn *c = conns[i];
                if(c->busy || c->fd == ENET_SOCKET_NULL || c->state == CS_RESOLVING) continue;
                p.fd = c->fd;
                p.events = 0;
                if(c->state == CS_CONNECTING) p.events = POLLOUT;
                else
                {
                    if(!(c->role == ROLE_SERVER && c->waiting && c->in.length() - c->inpos > cfg.maxline + cfg.maxbody)) p.events |= POLLIN;
                    if(c->wantwrite || c->out.length()) p.events |= POLLOUT;
                }
                p.revents = 0;
                fds.add(p);
                fdconns.add(c);
            }
            int ready = NET_POLL(fds.getbuf(), fds.length(), 250);
            netmillis = getmillis();
            if(ready > 0)
            {
                if(fds[0].revents&POLLIN)
                {
                    char buf[256];
                    ENetBuffer b;
                    b.data = buf;
                    b.dataLength = sizeof(buf);
                    while(enet_socket_receive(wakefd, NULL, &b, 1) > 0);
                }
                loopv(fds)
                {
                    if(!fds[i].revents) continue;
                    conn *c = fdconns[i];
                    if(!c)
                    {
                        if(fds[i].fd == listenfd) acceptconns();
                        continue;
                    }
                    if(c->closing || c->busy) continue;
                    short rev = fds[i].revents;
                    if(c->state == CS_CONNECTING)
                    {
                        int error = 0;
                        if((rev&(POLLERR|POLLHUP|POLLNVAL)) || enet_socket_get_option(c->fd, ENET_SOCKOPT_ERROR, &error) < 0 || error)
                        {
                            enet_socket_destroy(c->fd);
                            c->fd = ENET_SOCKET_NULL;
                            tryconnect(c);
                            continue;
                        }
                        c->lastrecv = netmillis;
                        if(c->fetch->url.tls)
                        {
                            lock l(clientlock);
                            if(!curclient || (!clientready && !c->fetch->pin)) { fetchfailed(c, "no trusted certificates to check %s against", c->fetch->url.host); c->closing = true; continue; }
                            if(!tlssetup(c, &curclient->conf)) { fetchfailed(c, "unable to set up TLS"); c->closing = true; continue; }
                            mbedtls_ssl_set_hostname(c->ssl, c->fetch->url.host);
                            if(c->fetch->pin) mbedtls_ssl_set_verify(c->ssl, pinverify, c);
                            c->state = CS_HANDSHAKE;
                            handshake(c);
                            continue;
                        }
                        connected(c);
                        if(!flushconn(c)) { c->closing = true; discard(c); }
                        continue;
                    }
                    if(c->state == CS_DETECT) { if(rev&(POLLIN|POLLERR|POLLHUP)) detect(c); if(c->state != CS_OPEN) continue; }
                    if(c->state == CS_HANDSHAKE) { handshake(c); continue; }
                    if((rev&POLLOUT) || c->wantwrite)
                    {
                        if(!flushconn(c)) { if(c->role == ROLE_CLIENT) fetchfailed(c, "connection lost while sending"); c->closing = true; discard(c); continue; }
                    }
                    if(rev&(POLLIN|POLLERR|POLLHUP))
                    {
                        if(!readconn(c))
                        {
                            c->closing = true;
                            if(c->role == ROLE_CLIENT && c->fetch && c->proto == PROTO_HTTP) fetchfailed(c, "connection to %s closed early", c->fetch->url.host);
                            continue;
                        }
                        if(c->out.length() && !flushconn(c)) { c->closing = true; discard(c); }
                    }
                }
            }
            if(netmillis - lasttimers >= 250)
            {
                lasttimers = netmillis;
                checktimers();
                if(handshakes.numelems > 4096) handshakes.clear();
            }
        }
        while(conns.length()) { conns.last()->busy = false; closeconn(conns.length()-1); }
        if(listenfd != ENET_SOCKET_NULL) enet_socket_destroy(listenfd);
        listenfd = ENET_SOCKET_NULL;
        THREADRET;
    }

    bool start(config &c)
    {
        if(running) { setconfig(c); return true; }
        sharedcfg = cfg = c;
        wakefd = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        if(wakefd == ENET_SOCKET_NULL) return false;
        ENetAddress local;
        local.host = ENET_HOST_TO_NET_32(0x7F000001);
        local.port = 0;
        if(enet_socket_bind(wakefd, &local) < 0 || enet_socket_get_address(wakefd, &wakeaddr) < 0)
        {
            enet_socket_destroy(wakefd);
            wakefd = ENET_SOCKET_NULL;
            return false;
        }
        enet_socket_set_option(wakefd, ENET_SOCKOPT_NONBLOCK, 1);
        if(!starttls()) conoutf(colourred, "TLS is unavailable, https:// and wss:// will fail");
        quitting = false;
        if(!startthread(nettid, netloop, NULL))
        {
            enet_socket_destroy(wakefd);
            wakefd = ENET_SOCKET_NULL;
            return false;
        }
        int threads = clamp(c.handshakethreads, 1, 16); // always at least one, for name lookups
        loopi(threads)
        {
            thread t;
            if(startthread(t, workerloop, NULL)) workers.add(t);
        }
        running = true;
        return true;
    }

    void stop()
    {
        if(!running) return;
        loopv(workers)
        {
            { lock l(tasklock); task t; t.type = -1; t.c = NULL; t.result = 0; tasks.add(t); }
            tasksignal.post();
        }
        loopv(workers) jointhread(workers[i]);
        workers.setsize(0);
        { lock l(cmdlock); quitting = true; }
        wake();
        jointhread(nettid);
        { lock l(tasklock); tasks.setsize(0); }
        { lock l(donelock); done.setsize(0); }
        { lock l(cmdlock); commands.deletecontents(); }
        { lock l(evlock); events.deletecontents(); progresses.clear(); }
        conns.setsize(0);
        peraddr.clear();
        handshakes.clear();
        enet_socket_destroy(wakefd);
        wakefd = ENET_SOCKET_NULL;
        stoptls();
        running = false;
    }

    bool listen(const char *ip, int port, int range, int &bound)
    {
        bound = 0;
        ENetAddress address = { ENET_HOST_ANY, 0 };
        if(ip && *ip && enet_address_set_host(&address, ip) < 0)
        {
            conoutf(colourred, "Failed to resolve HTTP address: %s", ip);
            return false;
        }
        for(int i = 0; i <= max(range, 0); i++)
        {
            ENetSocket fd = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
            if(fd == ENET_SOCKET_NULL) return false;
            address.port = port + i;
#ifndef WIN32
            enet_socket_set_option(fd, ENET_SOCKOPT_REUSEADDR, 1); // on Windows this would let two servers share a port
#endif
            if(enet_socket_bind(fd, &address) < 0 || enet_socket_listen(fd, 128) < 0) { enet_socket_destroy(fd); continue; }
            enet_socket_set_option(fd, ENET_SOCKOPT_NONBLOCK, 1);
            command *c = new command;
            c->type = CMD_LISTEN;
            c->fd = fd;
            pushcommand(c);
            bound = port + i;
            return true;
        }
        return false;
    }

    void unlisten()
    {
        command *c = new command;
        c->type = CMD_UNLISTEN;
        pushcommand(c);
    }

    void send(int id, const char *data, int len, int flags, int maxmessage)
    {
        command *c = new command;
        c->type = CMD_SEND;
        c->id = id;
        c->flags = flags;
        c->maxmessage = maxmessage;
        c->buf = new netbuf(data, len);
        pushcommand(c);
    }

    int sendshared(const vector<int> &ids, const char *data, int len)
    {
        if(ids.empty()) return 0;
        command *c = new command;
        c->type = CMD_SHARED;
        loopv(ids) c->ids.add(ids[i]);
        c->buf = new netbuf(data, len, ids.length());
        pushcommand(c);
        return ids.length();
    }

    void close(int id, int code)
    {
        command *c = new command;
        c->type = CMD_CLOSE;
        c->id = id;
        c->flags = code;
        pushcommand(c);
    }

    bool fetch(int id, httpfetch &f, bool ws, int maxsize)
    {
        job *j = new job;
        if(!j->url.parse(f.url) || (ws != j->url.ws && (ws || j->url.ws))) { delete j; return false; }
        j->ws = ws;
        j->method = ws ? HTTP_GET : f.method;
        j->maxsize = maxsize;
        j->headers.copy(f.reqheaders);
        j->body.put(f.reqbody.getbuf(), f.reqbody.length());
        if(f.file) j->file = newstring(f.file);
        if(f.pin) j->pin = newstring(f.pin);
        command *c = new command;
        c->type = CMD_FETCH;
        c->id = id;
        c->fetch = j;
        pushcommand(c);
        return true;
    }

    int progress(int id, int &total)
    {
        lock l(evlock);
        progressinfo *p = progresses.access(id);
        if(!p) { total = -1; return 0; }
        total = p->total;
        return p->recvd;
    }
}

namespace http
{
    void sha1(const uchar *data, int len, uchar digest[20]) { httpnet::dosha1(data, len, digest); }
    void sha256(const uchar *data, int len, uchar digest[32]) { mbedtls_sha256(data, len, digest, 0); }
    void base64(vector<char> &buf, const uchar *data, int len) { httpnet::dobase64(buf, data, len); }
    void randombytes(uchar *buf, int len) { httpnet::tlsrandom(buf, len); }
}
