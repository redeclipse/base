// client.cpp, mostly network related client game code

#include "engine.h"

ENetHost *clienthost = NULL;
ENetPeer *curpeer = NULL, *connpeer = NULL;
int connmillis = 0, connattempts = 0, discmillis = 0;

bool multiplayer(bool msg)
{
    // check if we're playing alone
    int n = client::otherclients();
    if(n && msg) conoutft(CON_MESG, "\froperation not available with other clients");
    return n > 0;
}

void setrate(int rate)
{
   if(!curpeer) return;
    enet_host_bandwidth_limit(clienthost, rate*1024, rate*1024);
}

VARF(0, rate, 0, 0, 1024, setrate(rate));

void throttle();

VARF(0, throttle_interval, 0, 5, 30, throttle());
VARF(0, throttle_accel, 0, 2, 32, throttle());
VARF(0, throttle_decel, 0, 2, 32, throttle());

void throttle()
{
    if(!curpeer) return;
    ASSERT(ENET_PEER_PACKET_THROTTLE_SCALE==32);
    enet_peer_throttle_configure(curpeer, throttle_interval*1000, throttle_accel, throttle_decel);
}

bool connected(bool attempt, bool local)
{
    return curpeer || (attempt && connpeer) || (local && haslocalclients());
}

const ENetAddress *connectedpeer()
{
    return curpeer ? &curpeer->address : NULL;
}

ICOMMAND(0, connectedip, "", (),
{
    const ENetAddress *address = connectedpeer();
    string hostname;
    result(address && enet_address_get_host_ip(address, hostname, sizeof(hostname)) >= 0 ? hostname : "");
});

ICOMMAND(0, connectedhost, "", (),
{
    const ENetAddress *address = connectedpeer();
    string hostname;
    result(address && enet_address_get_host(address, hostname, sizeof(hostname)) >= 0 ? hostname : (address && enet_address_get_host_ip(address, hostname, sizeof(hostname)) >= 0 ? hostname : ""));
});

ICOMMAND(0, connectedport, "", (),
{
    const ENetAddress *address = connectedpeer();
    intret(address ? address->port : -1);
});

void abortconnect(bool msg)
{
    if(!connpeer) return;
    client::connectfail();
    if(msg) conoutft(CON_MESG, "\faaborting connection attempt");
    if(connpeer->state!=ENET_PEER_STATE_DISCONNECTED) enet_peer_reset(connpeer);
    connpeer = NULL;
    if(curpeer) return;
    enet_host_destroy(clienthost);
    clienthost = NULL;
}

void connectfail()
{
    abortconnect(false);
    localconnect(false);
}

void trydisconnect()
{
    if(connpeer) abortconnect();
    else if(curpeer || haslocalclients())
    {
        if(verbose) conoutft(CON_MESG, "\faattempting to disconnect...");
        disconnect(0, !discmillis);
    }
    else conoutft(CON_MESG, "\frnot connected");
}

SVAR(0, connectname, "");
VAR(0, connectport, 0, 0, VAR_MAX);
VAR(IDF_PERSIST, connectguidelines, 0, 0, 1);
SVAR(0, guidelinesaction, "");

void connectserv(const char *name, int port, const char *password)
{
    abortconnect();
    if(!port) port = SERVER_PORT;

    ENetAddress address;
    address.port = port;

    setsvar("connectname", "");
    setvar("connectport", 0);
    if(name && *name)
    {
        if(!connectguidelines)
        {
            defformatstring(s)("connect %s %d %s", name, port, password && *password ? password : "");
            setsvar("guidelinesaction", s);
            showgui("guidelines");
            return;
        }
        addserver(name, port);
        conoutft(CON_MESG, "\faattempting to connect to %s:[%d]", name, port);
        if(!resolverwait(name, &address))
        {
            conoutft(CON_MESG, "\frcould not resolve host %s", name);
            connectfail();
            return;
        }
        setsvar("connectname", name);
        setvar("connectport", port);
    }
    else
    {
        conoutft(CON_MESG, "\faattempting to connect to a local server");
        address.host = ENET_HOST_BROADCAST;
    }

    if(!clienthost)
    {
        clienthost = enet_host_create(NULL, 2, server::numchannels(), rate*1024, rate*1024);
        if(!clienthost)
        {
            conoutft(CON_MESG, "\frfailed creating client socket");
            connectfail();
            return;
        }
        clienthost->duplicatePeers = 0;
    }

    connpeer = enet_host_connect(clienthost, &address, server::numchannels(), 0);
    enet_host_flush(clienthost);
    connmillis = totalmillis;
    connattempts = 0;
    client::connectattempt(name ? name : "", port, password ? password : "", address);
    conoutft(CON_MESG, "\fgconnecting to %s:[%d]", name != NULL ? name : "local server", port);
}

void disconnect(int onlyclean, int async)
{
    bool cleanup = onlyclean!=0;
    if(curpeer || haslocalclients())
    {
        if(curpeer)
        {
            if(!discmillis)
            {
                enet_peer_disconnect(curpeer, DISC_NONE);
                if(clienthost) enet_host_flush(clienthost);
                discmillis = totalmillis;
            }
            if(curpeer->state!=ENET_PEER_STATE_DISCONNECTED)
            {
                if(async) return;
                enet_peer_reset(curpeer);
            }
            curpeer = NULL;
        }
        discmillis = 0;
        conoutft(CON_MESG, "\frdisconnected");
        cleanup = true;
    }
    if(!connpeer && clienthost)
    {
        enet_host_destroy(clienthost);
        clienthost = NULL;
    }
    if(cleanup)
    {
        client::gamedisconnect(onlyclean);
        localdisconnect();
    }
    if(!onlyclean) localconnect(false);
}

ICOMMAND(0, connect, "sis", (char *n, int *a, char *pwd), connectserv(*n ? n : servermaster, *n || *a ? *a : SERVER_PORT, pwd));
COMMANDN(0, disconnect, trydisconnect, "");

ICOMMAND(0, lanconnect, "is", (int *a, char *pwd), connectserv(NULL, *a, pwd));
ICOMMAND(0, localconnect, "i", (int *n), localconnect(*n ? false : true));

ICOMMAND(0, isonline, "", (), intret(curpeer ? 1 : 0));
ICOMMAND(0, isconnected, "ii", (int *a, int *b), intret(connected(*a==0, *b==0) ? 1 : 0));

void reconnect(const char *pass)
{
    int port = 0;
    string addr = "";
    if(*connectname) copystring(addr, connectname);
    if(connectport) port = connectport;
    disconnect(1);
    connectserv(*addr ? addr : NULL, port > 0 ? port : SERVER_PORT, pass);
}
COMMAND(0, reconnect, "s");

int lastupdate = -1000;

void sendclientpacket(ENetPacket *packet, int chan)
{
    if(curpeer) enet_peer_send(curpeer, chan, packet);
    else localclienttoserver(chan, packet);
}

void flushclient()
{
    if(clienthost) enet_host_flush(clienthost);
}

void neterr(const char *s)
{
    conoutft(CON_MESG, "\frillegal network message (%s)", s);
    disconnect();
}

void localservertoclient(int chan, ENetPacket *packet)  // processes any updates from the server
{
    packetbuf p(packet);
    client::parsepacketclient(chan, p);
}

void clientkeepalive()
{
    if(clienthost) enet_host_service(clienthost, NULL, 0);
    if(serverhost) enet_host_service(serverhost, NULL, 0);

}

VAR(IDF_PERSIST, connectretry, 0, 5000, VAR_MAX);
VAR(IDF_PERSIST, connectattempts, 0, 3, VAR_MAX);

void gets2c()           // get updates from the server
{
    ENetEvent event;
    if(!clienthost) return;
    if(connpeer && totalmillis >= connmillis+connectretry)
    {
        connmillis = totalmillis;
        if(++connattempts > connectattempts)
        {
            conoutft(CON_MESG, "\frcould not connect to server");
            connectfail();
            return;
        }
        else conoutft(CON_MESG, "\faconnection attempt %d", connattempts);
    }
    while(clienthost && enet_host_service(clienthost, &event, 0) > 0) switch(event.type)
    {
        case ENET_EVENT_TYPE_CONNECT:
            disconnect(1);
            curpeer = connpeer;
            connpeer = NULL;
            conoutft(CON_MESG, "\fgconnected to server");
            throttle();
            if(rate) setrate(rate);
            client::gameconnect(true);
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            if(discmillis) conoutft(CON_MESG, "\faattempting to disconnect...");
            else localservertoclient(event.channelID, event.packet);
            enet_packet_destroy(event.packet);
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            if(event.data >= DISC_NUM) event.data = DISC_NONE;
            if(event.peer == connpeer)
            {
                conoutft(CON_MESG, "\frcould not connect to server");
                connectfail();
            }
            else
            {
                if(!discmillis || event.data) conoutft(CON_MESG, "\frserver disconnected (%s) ...", disc_reasons[event.data]);
                disconnect();
            }
            return;

        default:
            break;
    }
}

