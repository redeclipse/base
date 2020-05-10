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
        case PROP_STRING: free(data); break;
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

size_t property::size()
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

// vector<uchar> packprops(property *props, int num)
// {
//     // TODO: implement in property struct
//     vector<uchar> buf;

//     loopi(num)
//     {
//         property &p = props[i];

//         buf.put((uchar *)p.def->name, strlen(p.def->name) + 1); // include terminator

//         switch(p.def->type)
//         {
//             case PROP_INT:
//             case PROP_COLOR:
//                 buf.put((uchar *)&p, p.size());
//                 break;

//             case PROP_FLOAT:
//                 buf.put((uchar *)&p, p.size());
//                 break;

//             case PROP_IVEC:
//                 buf.put((uchar *)p.other, p.size());
//                 break;

//             case PROP_FVEC:
//                 buf.put((uchar *)p.other, p.size());
//                 break;

//             case PROP_STRING:
//                 buf.put((uchar *)p.other, p.size());
//                 break;
//         }
//     }
// }
