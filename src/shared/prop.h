enum
{
    PROP_NOTYPE = -1,
    PROP_INT,
    PROP_FLOAT,
    PROP_COLOR,
    PROP_IVEC,
    PROP_FVEC,
    PROP_STRING,

    PROP_TYPES
};

struct propertydef;

struct property
{
    union
    {
        int ival;
        float fval;
        void *data;
    };

    int type;
    const propertydef *def;

    operator int() const { return ival; }
    operator float() const { return fval; }
    operator bvec() const { return bvec(ival); }
    operator ivec() const { return *(ivec *)data; }
    operator vec() const { return *(vec *)data; }
    operator char *() const { return (char *)data; }

    void setdef(const propertydef *newdef);
    void clear();
    bool checktype(int type);
    void set(int value);
    void set(float value);
    void set(const bvec &value);
    void set(const ivec &value);
    void set(const vec &value);
    void set(const char *value);
    void set(const property &prop);
    template<class T> T get() const { return *this; }
    void reset();
    size_t size();

    void operator=(int value) { ival = value; }
    void operator=(float value) { fval = value; }
    void operator=(const bvec &value) { ival = value.tohexcolor(); }
    void operator=(const ivec &value)
    {
        if(!data) data = new ivec(value);
        else *(ivec *)data = value;
    }

    void operator=(const vec &value)
    {
        if(!data) data = new vec(value);
        else *(vec *)data = value;
    }

    void operator=(const char *value)
    {
        if(data) free(data);
        if(value) data = newstring(value);
        else data = NULL;
    }

    void operator=(const property &prop) { set(prop); }

    property() : data(NULL), type(PROP_NOTYPE), def(NULL) {}
    property(int value)         : data(NULL), type(PROP_INT),    def(NULL) { *this = value; }
    property(float value)       : data(NULL), type(PROP_FLOAT),  def(NULL) { *this = value; }
    property(const bvec &value) : data(NULL), type(PROP_COLOR),  def(NULL) { *this = value; }
    property(const ivec &value) : data(NULL), type(PROP_IVEC),   def(NULL) { *this = value; }
    property(const vec &value)  : data(NULL), type(PROP_FVEC),   def(NULL) { *this = value; }
    property(const char *value) : data(NULL), type(PROP_STRING), def(NULL) { *this = value; }
    virtual ~property() { clear(); }
};

struct propertydef
{
    const char *name;
    int type;
    property minval, defaultval, maxval;

    template<class T> propertydef(const char *_name, int _type, T _minval, T _defval,
                                  T _maxval) : name(_name), type(_type),
                                  minval(_minval), defaultval(_defval), maxval(_maxval) {}
};

template<class P, class PD>
static inline void initprops(P *props, const PD *defs, int num, P *copy = NULL)
{
    loopi(num)
    {
        props[i].setdef(&defs[i]);
        if(copy) props[i].set(copy[i]);
        else props[i].reset();
    }
}

template<class P>
static inline P *findprop(const char *name, P *props, int num)
{
    loopi(num) if(!strcmp(name, props[i].def->name)) return &props[i];
    return NULL;
}

template<class PD>
static inline PD *findpropdef(const char *name, PD *propdefs, int num)
{
    loopi(num) if(!strcmp(name, propdefs[i].name)) return &propdefs[i];
    return NULL;
}
