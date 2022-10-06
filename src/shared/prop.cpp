#include "cube.h"

void property::setdef(const propertydef *newdef)
{
    def = newdef;
    type = def->type;
}

void property::clear()
{
    if(data) switch(type)
    {
        case PROP_IVEC: delete (ivec *)data; break;
        case PROP_FVEC: delete (vec *)data; break;
        case PROP_STRING: delete[] (char*)data; break;
    }

    data = NULL;
}

bool property::checktype(int comptype)
{
    ASSERT(def);

    static const char *typenames[PROP_TYPES] =
    {
        "int", "float", "byte vector", "int vector", "float vector", "string"
    };

    if(type != comptype)
    {
        ASSERT(type >= 0 && comptype >= 0);
        conoutf("\frError: %s, incorrect property assignment type: got %s, expected %s", def->name,
                typenames[comptype], typenames[type]);

        return false;
    }

    return true;
}

void property::set(int value)
{
    ASSERT(def);
    if(checktype(PROP_INT))
        *this = clamp(value, def->minval, def->maxval);
}

void property::set(float value)
{
    ASSERT(def);
    if(checktype(PROP_FLOAT))
        *this = clamp(value, def->minval, def->maxval);
}

void property::set(const bvec &value)
{
    ASSERT(def);
    if(checktype(PROP_COLOR))
        *this = bvec(value).min(def->maxval.get<bvec>()).max(def->minval.get<bvec>());
}

void property::set(const ivec &value)
{
    ASSERT(def);
    if(checktype(PROP_IVEC))
        *this = ivec(value).min(def->maxval.get<ivec>()).max(def->minval.get<ivec>());
}

void property::set(const vec &value)
{
    ASSERT(def);
    if(checktype(PROP_FVEC))
        *this = vec(value).min(def->maxval.get<vec>()).max(def->minval.get<vec>());
}

void property::set(const char *value)
{
    ASSERT(def);
    if(checktype(PROP_STRING))
        *this = value;
}

void property::set(const property &prop)
{
    if(prop.type == PROP_NOTYPE) clear();
    else switch(type)
    {
        case PROP_INT: set(prop.get<int>()); break;
        case PROP_FLOAT: set(prop.get<float>()); break;
        case PROP_COLOR: set(prop.get<bvec>()); break;
        case PROP_IVEC: set(prop.get<ivec>()); break;
        case PROP_FVEC: set(prop.get<vec>()); break;
        case PROP_STRING: set(prop.get<char *>()); break;
        default: ASSERT(0);
    }
}

void property::reset()
{
    ASSERT(def);
    set(def->defaultval);
}

size_t property::size() const
{
    switch(type)
    {
        case PROP_INT:
        case PROP_COLOR:
            return sizeof(int);

        case PROP_FLOAT:
            return sizeof(float);

        case PROP_IVEC:
            return sizeof(ivec);

        case PROP_FVEC:
            return sizeof(vec);

        case PROP_STRING:
            return strlen(*this) + 1; // include terminator
    }

    return 0;
}

void property::pack(vector<uchar> &buf) const
{
    if(!def) return;

    int datasize = size();

    buf.put((uchar *)&datasize, sizeof(datasize));

    switch(type)
    {
        case PROP_INT:
            buf.put((uchar *)&ival, datasize);
            break;

        case PROP_COLOR:
            buf.put((uchar *)&ival, datasize);
            break;

        case PROP_FLOAT:
            buf.put((uchar *)&fval, datasize);
            break;

        case PROP_IVEC:
        case PROP_FVEC:
        case PROP_STRING:
            buf.put((uchar *)data, datasize);
            break;
    }
}

int property::unpack(uchar *buf, int bufsize)
{
    ASSERT(def);

    int bufread = 0;
    int *datasize_packed = 0;

    if(bufsize <= sizeof(*datasize_packed))
    {
        conoutf("Error unpacking prop '%s': not enough data to get the size!", def->name);
        return 0;
    }

    datasize_packed = (int*)buf;
    bufread += sizeof(*datasize_packed);

    switch(type)
    {
        case PROP_INT:
        case PROP_COLOR:
        case PROP_FLOAT:
            if(*datasize_packed != size())
            {
                conoutf("Error unpacking prop '%s': unexpected data size! Wanted: %d, got: %d",
                    def->name, size(), *datasize_packed);

                return 0;
            }
            break;
    }

    switch(type)
    {
        case PROP_INT:
        case PROP_COLOR:
            if(bufsize - bufread <= size())
            {
                conoutf("Error unpacking prop '%s': not enough data!", def->name);
                return 0;
            }

            memcpy(&ival, buf + bufread, size());
            bufread += size();
            break;

        case PROP_FLOAT:
            if(bufsize - bufread <= size())
            {
                conoutf("Error unpacking prop '%s': not enough data!", def->name);
                return 0;
            }

            memcpy(&fval, buf + bufread, size());
            bufread += size();
            break;

        case PROP_IVEC:
        case PROP_FVEC:
        case PROP_STRING:
            if(bufsize - bufread <= *datasize_packed)
            {
                conoutf("Error unpacking prop '%s': not enough data!", def->name);
                return 0;
            }

            clear();
            data = new uchar[*datasize_packed];
            memcpy(data, buf + bufread, *datasize_packed);
            bufread += *datasize_packed;
            break;
    }

    return bufread;
}
