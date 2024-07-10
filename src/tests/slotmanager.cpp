#include "cube.h"

void testslotmanager()
{
    Slotmanager<int> slotmanager;

    ASSERT(slotmanager.length() == 0);
    ASSERT(!slotmanager.inrange(0));
    ASSERT(!slotmanager.rename("TEST1", "TEST2"));
    ASSERT(!slotmanager.hasslot("TEST1"));
    ASSERT(!slotmanager.hasslot("TEST2"));

    Slotmanager<int>::Handle handle = slotmanager["TEST1"];
    Slotmanager<int>::Handle handle2 = slotmanager["TEST1"];

    handle.set(1337);
    ASSERT(handle == handle2);
    ASSERT(handle.get() == 1337);
    ASSERT(handle2.get() == 1337);
    ASSERT(handle.getindex() == 0);
    ASSERT(handle.isvalid());
    ASSERT(slotmanager.length() == 1);
    ASSERT(slotmanager.inrange(0));
    ASSERT(slotmanager.hasslot("TEST1"));
    ASSERT(!strcmp(slotmanager.getname(handle), "TEST1"));
    ASSERT(slotmanager.rename("TEST1", "TEST2"));
    ASSERT(!slotmanager.hasslot("TEST1"));
    ASSERT(slotmanager.hasslot("TEST2"));

    handle2 = slotmanager["TEST2"];
    ASSERT(handle == handle2);
    ASSERT(slotmanager.length() == 1);
    ASSERT(handle.getindex() == 0);
    ASSERT(handle.isvalid());
    ASSERT(handle.get() == 1337);
    ASSERT(!strcmp(slotmanager.getname(handle), "TEST2"));

    slotmanager.clear();
    ASSERT(slotmanager.length() == 0);
    ASSERT(!slotmanager.inrange(0));
    ASSERT(handle.getindex() == -1);
    ASSERT(!handle.isvalid());
    ASSERT(slotmanager.rename("TEST2", "TEST1"));
    ASSERT(slotmanager.hasslot("TEST1"));
    ASSERT(!slotmanager.hasslot("TEST2"));
    slotmanager["TEST1"];
    ASSERT(handle.isvalid());

    slotmanager.remove("TEST1");
    ASSERT(slotmanager.length() == 0);
    ASSERT(!slotmanager.inrange(0));
    ASSERT(!slotmanager.rename("TEST1", "TEST2"));
    ASSERT(!slotmanager.hasslot("TEST1"));
    ASSERT(!slotmanager.hasslot("TEST2"));

    handle = slotmanager["TEST1"];
    handle2 = slotmanager["TEST2"];

    ASSERT(slotmanager.length() == 2);
    ASSERT(slotmanager.inrange(0));
    ASSERT(slotmanager.inrange(1));
    ASSERT(slotmanager.hasslot("TEST1"));
    ASSERT(slotmanager.hasslot("TEST2"));

    handle.set(100);
    handle2.set(200);

    ASSERT(handle != handle2);
    ASSERT(handle.isvalid());
    ASSERT(handle2.isvalid());
    ASSERT(handle.get() == 100);
    ASSERT(handle2.get() == 200);
    ASSERT(handle.getindex() == 0);
    ASSERT(handle2.getindex() == 1);

    slotmanager.remove("TEST1");
    ASSERT(handle != handle2);
    ASSERT(!handle.isvalid());
    ASSERT(handle2.isvalid());
    ASSERT(handle2.getindex() == 0);
    ASSERT(handle2.get() == 200);
    ASSERT(slotmanager.length() == 1);
    ASSERT(slotmanager.inrange(0));
    ASSERT(!slotmanager.inrange(1));
}
