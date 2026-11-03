// JSON values, parsing and writing; strings inside are UTF-8, the writer takes the engine's own encoding

#define JSON_MAXDEPTH 64

enum { JSON_NULL = 0, JSON_FALSE, JSON_TRUE, JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT };

struct jsonvalue
{
    int type;
    double num;
    char *key, *str; // str holds a string, or a number as it was written
    vector<jsonvalue *> items; // an array's values or an object's members

    jsonvalue(int type = JSON_NULL) : type(type), num(0), key(NULL), str(NULL) {}
    ~jsonvalue() { DELETEA(key); DELETEA(str); items.deletecontents(); }

    int length() const { return items.length(); }
    jsonvalue *at(int i) { return items.inrange(i) ? items[i] : NULL; }
    jsonvalue *get(const char *name);
    const char *getstr(const char *name, const char *def = NULL);
    double getnum(const char *name, double def = 0);
    bool getbool(const char *name, bool def = false);
    bool istrue() const { return type == JSON_TRUE || (type == JSON_NUMBER && num != 0) || (type == JSON_STRING && str && *str); }
};

// builds JSON text without a tree, commas and nesting are handled for you
struct jsonwriter
{
    vector<char> &buf;
    vector<bool> first;

    jsonwriter(vector<char> &buf) : buf(buf) {}

    void key(const char *name);
    void beginobject(const char *name = NULL);
    void endobject();
    void beginarray(const char *name = NULL);
    void endarray();
    void str(const char *name, const char *value, int len = -1); // from the engine's encoding
    void utf8(const char *name, const char *value, int len = -1);
    void num(const char *name, int value);
    void num(const char *name, double value);
    void boolean(const char *name, bool value);
    void null(const char *name);
    void raw(const char *name, const char *json, int len = -1);
    void value(const char *name, jsonvalue *v);
};

namespace json
{
    extern jsonvalue *parse(const char *str, int len = -1, char *err = NULL, int errlen = 0);
    extern void write(vector<char> &buf, jsonvalue *v);
    extern void writestring(vector<char> &buf, const char *s, int len = -1, bool cube = false);
    extern jsonvalue *find(jsonvalue *root, const char *path); // dot separated keys and indices, like "servers.3.name"
    extern void tocube(vector<char> &buf, const char *s, int len = -1);
}
