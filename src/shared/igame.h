// the interface the engine uses to run the gameplay module
#ifndef STANDALONE
namespace entities
{
    extern int numattrs(int type);
    extern int triggertime(extentity &e, bool delay = false);
    extern void editent(int i, bool local);
    extern void readent(stream *g, int mver, char *gid, int gver, int id);
    extern void writeent(stream *g, int id);
    extern void remapents(vector<int> &idxs);
    extern void initents(int mver, char *gid, int gver);
    extern float dropheight(extentity &e);
    extern void fixentity(int n, bool recurse = true, bool create = false, bool alter = false);
    extern bool cansee(int n);
    extern const char *entinfo(int type, attrvector &attr, bool full = false, bool icon = false);
    extern const char *entinfo(entity &e, bool full = false, bool icon = false);
    extern const char *entmdlname(int type, attrvector &attr);
    extern const char *findname(int type);
    extern int findtype(char *type);
    extern void execlink(dynent *d, int index, bool local, int ignore = -1);
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
    extern bool checkparticle(extentity &e);
    extern void drawparticles();
    extern void allchanged(bool load = false);
    extern void mapshot(vec &pos, float &yaw, float &pitch, float &fov);
}

namespace client
{
    extern int getcn(physent *d);
    extern int parseplayer(const char *arg);
    extern int maxmsglen();
    extern void rehash();
    extern void writecfg();
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
    extern void completeplayers(const char **nextcomplete, const char *start, int commandsize, const char *lastcomplete, bool reverse);
}

namespace hud
{
    extern int statrate, hudsize, hudwidth, hudheight;
    extern char *progresstex, *progringtex;
    extern float cursorsize;
    extern const char *modeimage();
    extern float radarlimit(float dist = -1);
    extern bool radarlimited(float dist);
    extern int hasinput(bool pass = false, bool focus = true);
    extern bool textinput(const char *str, int len);
    extern bool keypress(int code, bool isdown);
    extern void render(bool noview = false);
    extern bool getcolour(vec &colour);
    extern void checkui();
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
    extern int physsteps, physframetime;
    extern float floorz, slopez, wallz;
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
    extern bool checkcollide(physent *d, const vec &dir, physent *o);
    extern bool checktracecollide(physent *d, const vec &from, const vec &to, float &dist, physent *o, float x1, float x2, float y1, float y2);
    extern void collided(physent *d, const vec &dir, physent *o, bool inside = false);
    extern void renderboundboxes(physent *d, const vec &rad, const vec &o);
}

namespace game
{
    extern void mapgamesounds();
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
    extern int hexpalette(int palette, int index);
    extern vec getpalette(int palette, int index);
    extern void fixpalette(int &palette, int &index, int gver);
    extern void adddynlights();
    extern void particletrack(particle *p, uint type, int &ts, bool step);
    extern void dynlighttrack(physent *owner, vec &o, vec &hud);
    extern bool mousemove(int dx, int dy, int x, int y, int w, int h);
    extern void project();
    extern void recomputecamera();
    extern int gametimeremain();
    extern int gametimeelapsed(bool force = false);
    extern const char *gamestatename(int type = 0);
    extern const char *gametitle();
    extern const char *gametext();
    extern int numanims();
    extern void findanims(const char *pattern, vector<int> &anims);
    extern void render();
    extern void renderpost();
    extern void renderavatar();
    extern void renderplayerpreview(float scale = 1, const vec4 &mcolor = vec4(1, 1, 1, 1), const char *actions = NULL);
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
    extern void changemapvariant(int variant);
    extern void ancmsgft(int cn, int conlevel, const char *snd, const char *s, ...);
    extern void srvmsgft(int cn, int conlevel, const char *s, ...);
    extern void srvmsgf(int cn, const char *s, ...);
    extern void srvoutf(int r, const char *s, ...);
    extern bool serveroption(char *arg);
    extern void *newinfo();
    extern void deleteinfo(void *ci);
    extern int numchannels();
    extern int spectatorslots();
    extern int maxslots();
    extern int reserveclients();
    extern int dupclients();
    extern int numclients(int exclude = -1, bool nospec = false, int actortype = -1);
    extern int numspectators(int exclude = -1);
    extern void clientdisconnect(int n, bool local = false, int reason = DISC_NONE);
    extern int clientconnect(int n, uint ip, bool local = false);
    extern void clientsteamticket(const char *id, bool result);
    extern bool allowbroadcast(int n);
    extern int peerowner(int n);
    extern void recordpacket(int chan, void *data, int len);
    extern void parsepacket(int sender, int chan, packetbuf &p);
    extern bool sendpackets(bool force = false);
    extern void queryreply(ucharbuf &req, ucharbuf &p);
    extern void serverupdate();
    extern const char *gameid();
    extern int getver(int n = 0);
    extern const char *gamename(int mode, int muts, int compact = 0, int limit = 0, char separator = ' ');
    extern const char *getgamename(int compact = 0, int limit = 0, char separator = ' ');
    extern const char *pickmap(const char *suggest = NULL, int mode = -1, int muts = -1, bool notry = false);
    extern const char *choosemap(const char *suggest = NULL, int mode = -1, int muts = -1, int force = 0, bool notry = false);
    extern const char *getmapname();
    extern const char *getserverdesc();
    extern void changemap(const char *name = NULL, int mode = -1, int muts = -1);
    extern bool canload(const char *type);
    extern bool rewritecommand(ident *id, tagval *args, int numargs);
    extern void processmasterinput(const char *cmd, int cmdlen, const char *args);
    extern void masterconnected();
    extern void masterdisconnected();
}
