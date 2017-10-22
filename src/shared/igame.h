// the interface the engine uses to run the gameplay module
#ifndef STANDALONE
namespace entities
{
    extern int numattrs(int type);
    extern int triggertime(extentity &e);
    extern void editent(int i, bool local);
    extern void readent(stream *g, int mtype, int mver, char *gid, int gver, int id);
    extern void writeent(stream *g, int id);
    extern void remapents(vector<int> &idxs);
    extern void initents(int mtype, int mver, char *gid, int gver);
    extern float dropheight(extentity &e);
    extern void fixentity(int n, bool recurse = true, bool create = false);
    extern bool cansee(int n);
    extern const char *entinfo(int type, attrvector &attr, bool full = false, bool icon = false);
    extern const char *entinfo(entity &e, bool full = false, bool icon = false);
    extern const char *entmdlname(int type, attrvector &attr);
    extern const char *findname(int type);
    extern int findtype(char *type);
    extern bool maylink(int type, int ver = 0);
    extern bool canlink(int index, int node, bool msg = false);
    extern bool linkents(int index, int node, bool add = true, bool local = true, bool toggle = true);
    extern extentity *newent();
    extern void deleteent(extentity *e);
    extern void clearents();
    extern vector<extentity *> &getents();
    extern int firstent(int type);
    extern int firstuse(int type);
    extern int lastent(int type);
    extern int lastuse(int type);
    extern void drawparticles();
}

namespace client
{
    extern int getcn(physent *d);
    extern int parseplayer(const char *arg);
    extern int maxmsglen();
    extern void gamedisconnect(int clean);
    extern void parsepacketclient(int chan, packetbuf &p);
    extern void gameconnect(bool _remote);
    extern bool allowedittoggle(bool edit);
    extern void edittoggled(bool edit);
    extern void toserver(int flags, const char *text, const char *target = NULL);
    extern void editvar(ident *id, bool local);
    extern void edittrigger(const selinfo &sel, int op, int arg1 = 0, int arg2 = 0, int arg3 = 0, const VSlot *vs = NULL);
    extern void changemap(const char *name);
    extern int waiting(bool state = true);
    extern void connectattempt(const char *name, int port, const char *password, const ENetAddress &address);
    extern void connectfail();
    extern int state();
    extern int otherclients(bool self = false, bool nospec = false);
    extern int numplayers();
    extern int servercompare(serverinfo *a, serverinfo *b);
    extern const char *getname();
    extern bool sendcmd(int nargs, const char *cmd, const char *arg);
}

namespace hud
{
    extern int statrate;
    extern char *progresstex, *progringtex;
    extern float radarlimit(float dist = -1);
    extern bool radarlimited(float dist);
    extern bool hasinput(bool pass = false, bool focus = true);
    extern bool textinput(const char *str, int len);
    extern bool keypress(int code, bool isdown);
    extern void drawhud(bool noview = false);
    extern void drawlast();
    extern float motionblur();
    extern bool getcolour(vec &colour);
    extern void processmenu();
    extern void update(int w, int h);
    extern bool needminimap();
    extern void drawquad(float x, float y, float w, float h, float tx1 = 0, float ty1 = 0, float tx2 = 1, float ty2 = 1, bool flipx = false, bool flipy = false);
    extern void drawcoord(float x, float y, float w, float h, float tx = 0, float ty = 0, float tw = 1, float th = 1, bool flipx = false, bool flipy = false);
    extern void drawtexture(float x, float y, float w, float h, bool flipx = false, bool flipy = false);
    extern void drawsized(float x, float y, float s, bool flipx = false, bool flipy = false);
    extern void drawblend(int x, int y, int w, int h, float r, float g, float b, bool blend = false);
    extern void colourskew(float &r, float &g, float &b, float skew = 1);
}

namespace physics
{
    extern int physsteps, physframetime, physinterp;
    extern float floatspeed, stairheight, floorz, slopez, wallz, stepspeed;
    extern float liquidmerge(physent *d, float from, float to);
    extern bool liquidcheck(physent *d);
    extern float gravityvel(physent *d);
    extern bool isfloating(physent *d);
    extern float movevelocity(physent *d, bool floating = false);
    extern bool issolid(physent *d, physent *e = NULL, bool esc = true, bool impact = true, bool reverse = false);
    extern bool move(physent *d, vec &dir);
    extern void move(physent *d, int moveres = 10, bool local = true);
    extern bool entinmap(physent *d, bool avoidplayers);
    extern void updatephysstate(physent *d);
    extern bool droptofloor(vec &o, int type = ENT_CAMERA, float radius = 1, float height = 1);
    extern bool moveplayer(physent *pl, int moveres, bool local, int millis);
    extern void interppos(physent *d);
    extern void updatematerial(physent *pl, const vec &center, const vec &bottom, bool local = false);
    extern bool xcollide(physent *d, const vec &dir, physent *o);
    extern bool xtracecollide(physent *d, const vec &from, const vec &to, float x1, float x2, float y1, float y2, float maxdist, float &dist, physent *o);
    extern void complexboundbox(physent *d);
}

namespace game
{
    extern void start();
    extern bool clientoption(char *arg);
    extern void preload();
    extern void updateworld();
    extern void newmap(int size, const char *mname = "");
    extern void resetmap(bool empty);
    extern void savemap(bool force = false, const char *mname = "");
    extern void startmap(bool empty = false);
    extern bool allowmove(physent *d);
    extern dynent *iterdynents(int i, bool all = false);
    extern dynent *focusedent(bool force = false);
    extern int numdynents(bool all = false);
    extern vec getpalette(int palette, int index);
    extern void adddynlights();
    extern void particletrack(particle *p, uint type, int &ts, bool step);
    extern void dynlighttrack(physent *owner, vec &o, vec &hud);
    extern bool mousemove(int dx, int dy, int x, int y, int w, int h);
    extern void project();
    extern void recomputecamera();
    extern const char *gametitle();
    extern const char *gametext();
    extern int numanims();
    extern void findanims(const char *pattern, vector<int> &anims);
    extern void render();
    extern void renderavatar();
    extern void renderplayerpreview(int model = 0, int color = 0xA0A0A0, int team = 0, int weap = 0, const char *vanity = "", float scale = 1, float blend = 1, const vec &lightcolor = vec(1, 1, 1), const vec &lightdir = vec(0, -1, 2));
    extern bool thirdpersonview(bool viewonly = false, physent *d = NULL);
    extern vec thirdpos(const vec &pos, float yaw, float pitch, float dist = 1, float side = 0);
    extern vec camerapos(physent *d, bool hasfoc = false, bool hasyp = false, float yaw = 0, float pitch = 0);
    extern void start();
}
#endif
namespace server
{
    extern bool updatecontrols;
    extern void start();
    extern void reload();
    extern void shutdown();
    extern void ancmsgft(int cn, int snd, int conlevel, const char *s, ...);
    extern void srvmsgft(int cn, int conlevel, const char *s, ...);
    extern void srvmsgf(int cn, const char *s, ...);
    extern void srvoutf(int r, const char *s, ...);
    extern bool serveroption(char *arg);
    extern void *newinfo();
    extern void deleteinfo(void *ci);
    extern int numclients(int exclude = -1, bool nospec = false, int actortype = -1);
    extern int reserveclients();
    extern int numchannels();
    extern int dupclients();
    extern void clientdisconnect(int n, bool local = false, int reason = DISC_NONE);
    extern int clientconnect(int n, uint ip, bool local = false);
    extern bool allowbroadcast(int n);
    extern int peerowner(int n);
    extern void recordpacket(int chan, void *data, int len);
    extern void parsepacket(int sender, int chan, packetbuf &p);
    extern bool sendpackets(bool force = false);
    extern void queryreply(ucharbuf &req, ucharbuf &p);
    extern void serverupdate();
    extern const char *gameid();
    extern int getver(int n = 0);
    extern const char *pickmap(const char *suggest = NULL, int mode = -1, int muts = -1, bool notry = false);
    extern const char *choosemap(const char *suggest = NULL, int mode = -1, int muts = -1, int force = 0, bool notry = false);
    extern void changemap(const char *name = NULL, int mode = -1, int muts = -1);
    extern bool canload(const char *type);
    extern bool rewritecommand(ident *id, tagval *args, int numargs);
    extern void processmasterinput(const char *cmd, int cmdlen, const char *args);
    extern void masterconnected();
    extern void masterdisconnected();
}
