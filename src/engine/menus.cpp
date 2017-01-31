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

void addchange(const char *desc, int type)
{
    if(!applydialog) return;
    loopv(needsapply) if(!strcmp(needsapply[i].desc, desc)) return;
    needsapply.add(change(type, desc));
    if(!hidechanges) UI::openui("changes");
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
    if(needsapply.empty()) UI::closeui("changes");
}

void applychanges()
{
    int changetypes = 0;
    loopv(needsapply) changetypes |= needsapply[i].type;
    if(changetypes&CHANGE_GFX) execute("resetgl");
    if(changetypes&CHANGE_SOUND) execute("resetsound");
}

COMMAND(0, applychanges, "");
ICOMMAND(0, pendingchanges, "b", (int *idx), { if(needsapply.inrange(*idx)) result(needsapply[*idx].desc); else if(*idx < 0) intret(needsapply.length()); });

void menuprocess()
{
    hud::gamemenus();
}
