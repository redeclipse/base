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
        conoutf(colourred, "Error: %s, incorrect property assignment type: got %s, expected %s", def->name,
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

void property::commandret() const
{
    switch(type)
    {
        case PROP_INT:
        case PROP_COLOR:
            intret(ival);
            break;

        case PROP_FLOAT:
            floatret(fval);
            break;

        case PROP_IVEC:
        {
            const ivec &v = get<ivec>();
            defformatstring(str, "%d %d %d", v.x, v.y, v.z);
            result(str);
            break;
        }

        case PROP_FVEC:
        {
            const vec &v = get<vec>();
            defformatstring(str, "%f %f %f", v.x, v.y, v.z);
            result(str);
            break;
        }
    }
}

void property::commandretmin() const
{
    ASSERT(def);
    def->minval.commandret();
}

void property::commandretmax() const
{
    ASSERT(def);
    def->maxval.commandret();
}

void property::commandretdefault() const
{
    ASSERT(def);
    def->defaultval.commandret();
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
        case PROP_COLOR:
        {
            int _ival = *this;
            lilswap(&_ival, 1);
            buf.put((uchar *)&_ival, datasize);
            break;
        }

        case PROP_FLOAT:
        {
            float _fval = *this;
            lilswap(&_fval, 1);
            buf.put((uchar *)&fval, datasize);
            break;
        }

        case PROP_IVEC:
        {
            ivec _ivval = *this;
            lilswap(_ivval.v, 3);
            buf.put((uchar *)_ivval.v, datasize);
            break;
        }

        case PROP_FVEC:
        {
            vec _fvval = *this;
            lilswap(_fvval.v, 3);
            buf.put((uchar *)_fvval.v, datasize);
            break;
        }

        case PROP_STRING:
            buf.put((uchar *)data, datasize);
            break;
    }
}

int property::unpack(uchar *buf, size_t bufsize)
{
    ASSERT(def);

    int bufread = 0;
    uint *datasize_packed = 0;

    if(bufsize <= sizeof(*datasize_packed))
    {
        conoutf(colourred, "Error unpacking prop '%s': not enough data to get the size!", def->name);
        return 0;
    }

    datasize_packed = (uint*)buf;
    lilswap(datasize_packed, 1);

    bufread += sizeof(*datasize_packed);

    switch(type)
    {
        case PROP_INT:
        case PROP_COLOR:
        case PROP_FLOAT:
            if(*datasize_packed != size())
            {
                conoutf(colourred, "Error unpacking prop '%s': unexpected data size! Wanted: %u, got: %u",
                    def->name, uint(size()), *datasize_packed);

                return 0;
            }
            break;
    }

    switch(type)
    {
        case PROP_INT:
        case PROP_COLOR:
            if(bufsize - bufread < size())
            {
                conoutf(colourred, "Error unpacking prop '%s': not enough data!", def->name);
                return 0;
            }

            memcpy(&ival, buf + bufread, size());
            lilswap(&ival, 1);
            bufread += size();
            break;

        case PROP_FLOAT:
            if(bufsize - bufread < size())
            {
                conoutf(colourred, "Error unpacking prop '%s': not enough data!", def->name);
                return 0;
            }

            memcpy(&fval, buf + bufread, size());
            lilswap(&fval, 1);
            bufread += size();
            break;

        case PROP_IVEC:
        case PROP_FVEC:
        case PROP_STRING:
            if(bufsize - bufread < *datasize_packed)
            {
                conoutf(colourred, "Error unpacking prop '%s': not enough data!", def->name);
                return 0;
            }

            clear();
            data = new uchar[*datasize_packed];
            memcpy(data, buf + bufread, *datasize_packed);

            if(type == PROP_IVEC) lilswap((int*)data, 3);
            else if(type == PROP_FVEC) lilswap((float*)data, 3);

            bufread += *datasize_packed;
            break;
    }

    return bufread;
}
