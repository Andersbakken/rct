#ifndef Value_h
#define Value_h

#include <rct/String.h>
#include <rct/Log.h>
#include <rct/Serializer.h>
#include <rct/Map.h>
#include <rct/List.h>
#include <math.h>

struct cJSON;
class Value
{
public:
    inline Value() : mType(Type_Invalid) {}
    inline Value(int i) : mType(Type_Integer) { mData.integer = i; }
    inline Value(double d) : mType(Type_Double) { mData.dbl = d; }
    inline Value(bool b) : mType(Type_Boolean) { mData.boolean = b; }
    inline Value(void *ptr) : mType(Type_Pointer) { mData.pointer = ptr; }
    inline Value(const String &string) : mType(Type_String) { new (mData.stringBuf) String(string); }
    inline Value(const Value &other) : mType(Type_Invalid) { copy(other); }
    inline Value(const Map<String, Value> &map) : mType(Type_Map) { new (mData.mapBuf) Map<String, Value>(map); }
    inline Value(const List<Value> &list) : mType(Type_List) { new (mData.listBuf) List<Value>(list); }
    inline Value &operator=(const Value &other) { clear(); copy(other); return *this; }
    ~Value() { clear(); }
    inline bool isNull() const { return mType == Type_Invalid; }
    inline bool isValid() const { return mType != Type_Invalid; }
    enum Type {
        Type_Invalid,
        Type_Boolean,
        Type_Integer,
        Type_Double,
        Type_String,
        Type_Pointer,
        Type_Map,
        Type_List
    };
    inline static const char *typeToString(Type type);
    inline Type type() const { return mType; }
    inline bool toBool() const;
    inline int toInteger() const;
    inline double toDouble() const;
    inline String toString() const;
    inline void *toPointer() const;
    inline Map<String, Value> toMap() const;
    inline List<Value> toList() const;
    Map<String, Value>::const_iterator begin() const;
    Map<String, Value>::const_iterator end() const;
    inline int count() const;
    inline const Value &at(int idx) const;
    const Value &operator[](int idx) const;
    Value &operator[](int idx);
    const Value &operator[](const String &key) const;
    Value &operator[](const String &key);
    inline Value child(int idx, const Value &defaultValue = Value()) const;
    template <typename T> inline T child(int idx, const T &defaultValue = T()) const;
    inline Value child(const String &key, const Value &defaultValue = Value()) const;
    template <typename T> inline T child(const String &name, const T &defaultValue = T()) const;
    template <typename T> inline T convert(bool *ok = 0) const { invalidType(T()); if (ok) *ok = false; return T(); }
    template <typename T> static Value create(const T &t) { return Value(t); }
    void clear();
    static Value fromJSON(const String &json, bool *ok = 0);
    String toJSON(bool pretty = false) const;
private:
    static cJSON *toCJSON(const Value &value);
    void copy(const Value &other);
    String *stringPtr() { return reinterpret_cast<String*>(mData.stringBuf); }
    const String *stringPtr() const { return reinterpret_cast<const String*>(mData.stringBuf); }
    Map<String, Value> *mapPtr() { return reinterpret_cast<Map<String, Value>*>(mData.mapBuf); }
    const Map<String, Value> *mapPtr() const { return reinterpret_cast<const Map<String, Value>*>(mData.mapBuf); }
    List<Value> *listPtr() { return reinterpret_cast<List<Value>*>(mData.listBuf); }
    const List<Value> *listPtr() const { return reinterpret_cast<const List<Value>*>(mData.listBuf); }

    Type mType;
    union {
        int integer;
        double dbl;
        bool boolean;
        char stringBuf[sizeof(String)];
        char mapBuf[sizeof(Map<String, Value>)];
        char listBuf[sizeof(List<Value>)];
        void *pointer;
    } mData;
};

const char *Value::typeToString(Type type)
{
    switch (type) {
    case Type_Invalid: return "invalid";
    case Type_Boolean: return "boolean";
    case Type_Integer: return "integer";
    case Type_Double: return "double";
    case Type_String: return "string";
    case Type_Pointer: return "pointer";
    case Type_Map: return "list";
    case Type_List: return "map";
    }
    return "";
}


template <> inline int Value::convert<int>(bool *ok) const
{
    if (ok)
        *ok = true;
    switch (mType) {
    case Type_Integer: return mData.integer;
    case Type_Double: return static_cast<int>(round(mData.dbl));
    case Type_Boolean: return mData.boolean;
    case Type_String: return atoi(stringPtr()->constData());
    case Type_Invalid: break;
    case Type_Pointer: break;
    case Type_List: break;
    case Type_Map: break;
    }
    if (ok)
        *ok = false;
    return 0;
}

template <> inline void* Value::convert<void*>(bool *ok) const
{
    if (ok)
        *ok = true;
    switch (mType) {
    case Type_Integer: break;
    case Type_Double: break;
    case Type_Boolean: break;
    case Type_String: break;
    case Type_Invalid: break;
    case Type_Pointer: return mData.pointer;
    case Type_List: break;
    case Type_Map: break;
    }
    if (ok)
        *ok = false;
    return 0;
}

template <> inline bool Value::convert<bool>(bool *ok) const
{
    if (ok)
        *ok = true;

    switch (mType) {
    case Type_Integer: return mData.integer;
    case Type_Double: return mData.dbl;
    case Type_Boolean: return mData.boolean;
    case Type_Pointer: return mData.pointer;
    case Type_String: {
        const String str = toString();
        return (str == "true" || str == "1"); }
    case Type_Invalid: break;
    case Type_List: break;
    case Type_Map: break;
    }
    if (ok)
        *ok = false;

    return false;
}


template <> inline double Value::convert<double>(bool *ok) const
{
    if (ok)
        *ok = true;

    switch (mType) {
    case Type_Integer: return mData.integer;
    case Type_Double: return mData.dbl;
    case Type_Boolean: return mData.boolean;
    case Type_String: return strtod(stringPtr()->constData(), 0);
    case Type_Pointer: break;
    case Type_Invalid: break;
    case Type_List: break;
    case Type_Map: break;
    }
    if (ok)
        *ok = false;

    return .0;
}

template <> inline String Value::convert<String>(bool *ok) const
{
    if (ok)
        *ok = true;

    switch (mType) {
    case Type_Integer: return String::number(mData.integer);
    case Type_Double: return String::number(mData.dbl);
    case Type_Boolean: return mData.boolean ? "true" : "false";
    case Type_String: return *stringPtr();
    case Type_Invalid: break;
    case Type_Pointer: break;
    case Type_List: break;
    case Type_Map: break;
    }
    if (ok)
        *ok = false;

    return String();
}

template <> inline List<Value> Value::convert<List<Value> >(bool *ok) const
{
    if (ok)
        *ok = true;

    switch (mType) {
    case Type_Integer: break;
    case Type_Double: break;
    case Type_Boolean: break;
    case Type_String: break;
    case Type_Invalid: break;
    case Type_Pointer: break;
    case Type_List: return *listPtr();
    case Type_Map: break;
    }
    if (ok)
        *ok = false;

    return List<Value>();
}

template <> inline Map<String, Value> Value::convert<Map<String, Value> >(bool *ok) const
{
    if (ok)
        *ok = true;

    switch (mType) {
    case Type_Integer: break;
    case Type_Double: break;
    case Type_Boolean: break;
    case Type_String: break;
    case Type_Invalid: break;
    case Type_Pointer: break;
    case Type_List: break;
    case Type_Map: return *mapPtr();
    }
    if (ok)
        *ok = false;

    return Map<String, Value>();
}


inline bool Value::toBool() const { return convert<bool>(0); }
inline int Value::toInteger() const { return convert<int>(0); }
inline double Value::toDouble() const { return convert<double>(0); }
inline String Value::toString() const { return convert<String>(0); }
inline void *Value::toPointer() const { return convert<void*>(0); }
inline Map<String, Value> Value::toMap() const { return convert<Map<String, Value> >(0); }
inline List<Value> Value::toList() const { return convert<List<Value> >(0); }
inline Value Value::child(int idx, const Value &defaultValue) const
{
    return mType == Type_List ? listPtr()->value(idx, defaultValue) : defaultValue;
}

template <typename T>
inline T Value::child(int idx, const T &defaultValue) const
{
    return child(idx, defaultValue).convert<T>();
}

inline Value Value::child(const String &key, const Value &defaultValue) const
{
    return mType == Type_Map ? mapPtr()->value(key, defaultValue) : defaultValue;
}

template <typename T>
inline T Value::child(const String &key, const T &defaultValue) const
{
    return child(key, defaultValue).convert<T>();
}

inline Map<String, Value>::const_iterator Value::begin() const
{
    if (mType != Type_Map)
        return end();
    return mapPtr()->begin();
}
inline Map<String, Value>::const_iterator Value::end() const
{
    if (mType != Type_Map) {
        const static Map<String, Value> hack;
        return hack.end();
    }
    return mapPtr()->end();
}

inline int Value::count() const
{
    switch (mType) {
    case Type_Map:
        return mapPtr()->size();
    case Type_List:
        return listPtr()->size();
    default:
        break;
    }
    return 0;
}

inline const Value &Value::at(int idx) const
{
    return operator[](idx);
}

inline const Value &Value::operator[](int idx) const
{
    assert(mType == Type_List);
    return (*listPtr())[idx];
}

inline Value &Value::operator[](int idx)
{
    assert(mType == Type_List);
    return (*listPtr())[idx];
}

inline const Value &Value::operator[](const String &key) const
{
    assert(mType == Type_Map);
    return (*mapPtr())[key];
}

inline Value &Value::operator[](const String &key)
{
    assert(mType == Type_Map);
    return (*mapPtr())[key];
}

inline Log operator<<(Log log, const Value &value)
{
    String str;
    {
        Log l(&str);
        switch (value.type()) {
        case Value::Type_Integer: l << value.toInteger(); break;
        case Value::Type_Double: l << value.toDouble(); break;
        case Value::Type_Boolean: l << value.toBool(); break;
        case Value::Type_String: l << value.toString(); break;
        case Value::Type_Invalid: l << "(invalid)"; break;
        case Value::Type_Pointer: l << value.toPointer(); break;
        case Value::Type_List: l << value.toList(); break;
        case Value::Type_Map: l << value.toMap(); break;
        }
    }
    log << String::format<128>("Variant(%s: %s)", Value::typeToString(value.type()),
                               str.constData());
    return log;
}

inline Serializer& operator<<(Serializer& serializer, const Value& value)
{
    serializer << static_cast<int>(value.type());
    switch (value.type()) {
    case Value::Type_Integer: serializer << value.toInteger(); break;
    case Value::Type_Double: serializer << value.toDouble(); break;
    case Value::Type_Boolean: serializer << value.toBool(); break;
    case Value::Type_String: serializer << value.toString(); break;
    case Value::Type_Map: serializer << value.toMap(); break;
    case Value::Type_List: serializer << value.toList(); break;
    case Value::Type_Pointer: error() << "Trying to serialize pointer"; break;
    case Value::Type_Invalid: break;
    }
    return serializer;
}

inline Deserializer& operator>>(Deserializer& deserializer, Value& value)
{
    int t;
    deserializer >> t;
    Value::Type type = static_cast<Value::Type>(t);
    switch (type) {
    case Value::Type_Integer: { int v; deserializer >> v; value = v; break; }
    case Value::Type_Double: { double v; deserializer >> v; value = v; break; }
    case Value::Type_Boolean: { bool v; deserializer >> v; value = v; break; }
    case Value::Type_String: { String v; deserializer >> v; value = v; break; }
    case Value::Type_Map: { Map<String, Value> v; deserializer >> v; value = v; break; }
    case Value::Type_List: { List<Value> v; deserializer >> v; value = v; break; }
    case Value::Type_Pointer: value.clear(); error() << "Trying to deserialize pointer"; break;
    case Value::Type_Invalid: value.clear(); break;
    }
    return deserializer;
}

#endif
