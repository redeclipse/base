#include "engine.h"

struct change
{
    int type;
    const char *desc;

    change() {}
    change(int type, const char *desc) : type(type), desc(desc) {}
};
static vector<change> needsapply;

VAR(IDF_PERSIST, applydialog, 0, 1, 1);
VAR(0, hidechanges, 0, 0, 1);

static const char *changenames[CHANGE_MAX] = { "graphics", "sound", "shader" };

void addchange(const char *desc, int type)
{
    if(!applydialog) return;
    loopv(needsapply) if(!strcmp(needsapply[i].desc, desc)) return;
    string name;
    name[0]= 0;
    loopi(CHANGE_MAX) if(type&(1<<i)) concformatstring(name, "%s%s", name[0] ? "/" : "", changenames[i]);
    if(engineready) conoutf(colouryellow, "Pending %s change: %s", name, desc);
    needsapply.add(change(type, desc));
}

void clearchanges(int type)
{
    loopvrev(needsapply)
    {
        change &c = needsapply[i];
        if(c.type&type)
        {
            c.type &= ~type;
            if(!c.type) needsapply.remove(i);
        }
    }
}

void applychanges()
{
    int changetypes = 0;
    loopv(needsapply) changetypes |= needsapply[i].type;
    if(changetypes&CHANGE_GFX) execident("resetgl");
    else if(changetypes&CHANGE_SHADERS) execident("resetshaders");
    if(changetypes&CHANGE_SOUND) execident("resetsound");

}

COMMAND(0, applychanges, "");
ICOMMAND(0, pendingchanges, "b", (int *idx),
{
    if(needsapply.inrange(*idx)) result(needsapply[*idx].desc);
    else if(*idx < 0) intret(needsapply.length());
});
