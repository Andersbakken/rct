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
    struct Custom;
    inline Value() : mType(Type_Invalid) {}
    inline Value(int i) : mType(Type_Integer) { mData.integer = i; }
    inline Value(int64_t i) : mType(Type_Integer) { mData.int64 = i; }
    inline Value(uint64_t i) : mType(Type_Integer) { mData.uint64 = i; }
    inline Value(double d) : mType(Type_Double) { mData.dbl = d; }
    inline Value(bool b) : mType(Type_Boolean) { mData.boolean = b; }
    inline Value(const std::shared_ptr<Custom> &custom) : mType(Type_Custom) { new (mData.customBuf) std::shared_ptr<Custom>(custom); }
    inline Value(const String &string) : mType(Type_String) { new (mData.stringBuf) String(string); }

    struct Custom : std::enable_shared_from_this<Custom>
    {
        Custom(int t)
            : type(t)
        {}

        virtual ~Custom() {}
        virtual String toString() const { return String(); }

        const int type;
    };
    inline Value(const char *str, int len = -1) : mType(Type_String)
    {
        if (len == -1)
            len = strlen(str);
        new (mData.stringBuf) String(str, len);
    }
    inline Value(const Value &other) : mType(Type_Invalid) { copy(other); }
    inline Value(const Map<String, Value> &map) : mType(Type_Map) { new (mData.mapBuf) Map<String, Value>(map); }
    template <typename T> inline Value(const List<T> &list)
        : mType(Type_List)
    {
        new (mData.listBuf) List<Value>(list.size());
        int i = 0;
        List<Value> *l = listPtr();

        for (const T &t : list)
            (*l)[i++] = t;
    }
    inline Value(const List<Value> &list) : mType(Type_List) { new (mData.listBuf) List<Value>(list); }
    Value(Value &&other);
    ~Value() { clear(); }

    inline Value &operator=(const Value &other) { clear(); copy(other); return *this; }
    Value & operator=(Value&& other);

    inline bool isNull() const { return mType == Type_Invalid; }
    inline bool isValid() const { return mType != Type_Invalid; }
    enum Type {
        Type_Invalid,
        Type_Undefined,
        Type_Boolean,
        Type_Integer,
        Type_Double,
        Type_String,
        Type_Custom,
        Type_Map,
        Type_List
    };

    inline bool isInvalid() const { return mType == Type_Invalid; }
    inline bool isUndefined() const { return mType == Type_Undefined; }
    inline bool isBoolean() const { return mType == Type_Boolean; }
    inline bool isInteger() const { return mType == Type_Integer; }
    inline bool isDouble() const { return mType == Type_Double; }
    inline bool isString() const { return mType == Type_String; }
    inline bool isCustom() const { return mType == Type_Custom; }
    inline bool isMap() const { return mType == Type_Map; }
    inline bool isList() const { return mType == Type_List; }

    inline static const char *typeToString(Type type);
    inline Type type() const { return mType; }
    inline bool toBool() const;
    inline int toInteger() const;
    inline int64_t toInt64() const;
    inline uint64_t toUInt64() const;
    inline double toDouble() const;
    inline String toString() const;
    inline std::shared_ptr<Custom> toCustom() const;
    inline Map<String, Value> toMap() const;
    template <typename T>
    inline List<T> toList() const;
    inline List<Value> toList() const;
    Map<String, Value>::const_iterator begin() const;
    Map<String, Value>::const_iterator end() const;
    List<Value>::const_iterator listBegin() const;
    List<Value>::const_iterator listEnd() const;
    inline int count() const;
    inline bool contains(const String &key) const;
    inline const Value &at(int idx) const;
    template <typename T> T operator[](int idx) const;
    template <typename T> T operator[](const String &key) const;
    const Value &operator[](int idx) const;
    Value &operator[](int idx);
    void push_back(const Value &value);
    const Value &operator[](const String &key) const;
    Value &operator[](const String &key);
    inline Value value(int idx, const Value &defaultValue = Value()) const;
    template <typename T> inline T value(int idx, const T &defaultValue = T(), bool *ok = 0) const;
    inline Value value(const String &key, const Value &defaultValue = Value()) const;
    template <typename T> inline T value(const String &name, const T &defaultValue = T(), bool *ok = 0) const;
    template <typename T> inline T convert(bool *ok = 0) const { invalidType(T()); if (ok) *ok = false; return T(); }
    inline Value convert(Type type, bool *ok) const;
    template <typename T> static Value create(const T &t) { return Value(t); }
    void clear();
    static Value fromJSON(const String &json, bool *ok = 0) { return fromJSON(json.constData(), ok); }
    static Value fromJSON(const char *json, bool *ok = 0);
    String toJSON(bool pretty = false) const;
    static Value undefined() { return Value(Type_Undefined); }
private:
    explicit Value(Type type) : mType(type) {}

    static cJSON *toCJSON(const Value &value);
    void copy(const Value &other);
    String *stringPtr() { return reinterpret_cast<String*>(mData.stringBuf); }
    const String *stringPtr() const { return reinterpret_cast<const String*>(mData.stringBuf); }
    Map<String, Value> *mapPtr() { return reinterpret_cast<Map<String, Value>*>(mData.mapBuf); }
    const Map<String, Value> *mapPtr() const { return reinterpret_cast<const Map<String, Value>*>(mData.mapBuf); }
    List<Value> *listPtr() { return reinterpret_cast<List<Value>*>(mData.listBuf); }
    const List<Value> *listPtr() const { return reinterpret_cast<const List<Value>*>(mData.listBuf); }
    std::shared_ptr<Custom> *customPtr() { return reinterpret_cast<std::shared_ptr<Custom>*>(mData.customBuf); }
    const std::shared_ptr<Custom> *customPtr() const { return reinterpret_cast<const std::shared_ptr<Custom>*>(mData.customBuf); }

    Type mType;
    union {
        int integer;
        int64_t int64;
        uint64_t uint64;
        double dbl;
        bool boolean;
        char stringBuf[sizeof(String)];
        char mapBuf[sizeof(Map<String, Value>)];
        char listBuf[sizeof(List<Value>)];
        char customBuf[sizeof(std::shared_ptr<Custom>)];
    } mData;
};

inline Value::Value(Value &&other)
    : mType(Type_Invalid)
{
    copy(other);
    memset(&other.mData, '\0', sizeof(mData));
    other.mType = Type_Invalid;
}

inline Value &Value::operator=(Value &&other)
{
    clear();
    copy(other);
    memset(&other.mData, '\0', sizeof(mData));
    other.mType = Type_Invalid;
    return *this;
}

const char *Value::typeToString(Type type)
{
    switch (type) {
    case Type_Invalid: return "invalid";
    case Type_Undefined: return "undefined";
    case Type_Boolean: return "boolean";
    case Type_Integer: return "integer";
    case Type_Double: return "double";
    case Type_String: return "string";
    case Type_Custom: return "custom";
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
    case Type_String: {
        char *end;
        const int ret = strtol(stringPtr()->constData(), &end, 10);
        if (!*end)
            return ret;
        break; }
    case Type_Invalid: break;
    case Type_Undefined: break;
    case Type_Custom: break;
    case Type_List: break;
    case Type_Map: break;
    }
    if (ok)
        *ok = false;
    return 0;
}

template <> inline int64_t Value::convert<int64_t>(bool *ok) const
{
    if (ok)
        *ok = true;
    switch (mType) {
    case Type_Integer: return mData.int64;
    case Type_Double: return static_cast<int64_t>(round(mData.dbl));
    case Type_Boolean: return mData.boolean;
    case Type_String: {
        char *end;
        const int64_t ret = strtoll(stringPtr()->constData(), &end, 10);
        if (!*end)
            return ret;
        break; }
    case Type_Invalid: break;
    case Type_Undefined: break;
    case Type_Custom: break;
    case Type_List: break;
    case Type_Map: break;
    }
    if (ok)
        *ok = false;
    return 0;
}

template <> inline uint64_t Value::convert<uint64_t>(bool *ok) const
{
    if (ok)
        *ok = true;
    switch (mType) {
    case Type_Integer: return mData.uint64;
    case Type_Double: return static_cast<uint64_t>(round(mData.dbl));
    case Type_Boolean: return mData.boolean;
    case Type_String: {
        char *end;
        const uint64_t ret = strtoull(stringPtr()->constData(), &end, 10);
        if (!*end)
            return ret;
        break; }
    case Type_Invalid: break;
    case Type_Undefined: break;
    case Type_Custom: break;
    case Type_List: break;
    case Type_Map: break;
    }
    if (ok)
        *ok = false;
    return 0;
}

template <> inline std::shared_ptr<Value::Custom> Value::convert<std::shared_ptr<Value::Custom> >(bool *ok) const
{
    if (ok)
        *ok = true;
    switch (mType) {
    case Type_Integer: break;
    case Type_Double: break;
    case Type_Boolean: break;
    case Type_String: break;
    case Type_Invalid: break;
    case Type_Undefined: break;
    case Type_Custom: return *customPtr();
    case Type_List: break;
    case Type_Map: break;
    }
    if (ok)
        *ok = false;
    return std::shared_ptr<Custom>();
}

template <> inline bool Value::convert<bool>(bool *ok) const
{
    if (ok)
        *ok = true;

    switch (mType) {
    case Type_Integer: return mData.integer;
    case Type_Double: return mData.dbl;
    case Type_Boolean: return mData.boolean;
    case Type_String: {
        const String str = toString();
        if (str == "true" || str == "1") {
            return true;
        } else if (str == "false" || str == "0") {
            return false;
        }
        break; }
    case Type_Custom:
    case Type_Invalid: break;
    case Type_Undefined: break;
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
    case Type_String: {
        char *end;
        const double ret = strtod(stringPtr()->constData(), &end);
        if (!*end)
            return ret;
        break; }
    case Type_Custom: break;
    case Type_Invalid: break;
    case Type_Undefined: break;
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
    case Type_Undefined: return "undefined";
    case Type_Custom: break;
    case Type_List: break;
    case Type_Map: break;
    }
    return toJSON();
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
    case Type_Undefined: break;
    case Type_Custom: break;
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
    case Type_Undefined: break;
    case Type_Custom: break;
    case Type_List: break;
    case Type_Map: return *mapPtr();
    }
    if (ok)
        *ok = false;

    return Map<String, Value>();
}

inline Value Value::convert(Type type, bool *ok) const
{
    switch (type) {
    case Type_Integer: return convert<int>(ok);
    case Type_Double: return convert<double>(ok);
    case Type_Boolean: return convert<bool>(ok);
    case Type_String: return convert<String>(ok);
    case Type_Invalid: if (ok) *ok = true; return Value();
    case Type_Undefined: if (ok) *ok = true; return Value::undefined();
    case Type_Custom: return convert<std::shared_ptr<Custom> >(ok);
    case Type_List: return convert<List<Value> >(ok);
    case Type_Map: return convert<Map<String, Value> >(ok);
    }
    if (ok)
        *ok = false;
    return Value();
}

inline bool Value::toBool() const { return convert<bool>(0); }
inline int Value::toInteger() const { return convert<int>(0); }
inline int64_t Value::toInt64() const { return convert<int64_t>(0); }
inline uint64_t Value::toUInt64() const { return convert<uint64_t>(0); }
inline double Value::toDouble() const { return convert<double>(0); }
inline String Value::toString() const { return convert<String>(0); }
inline std::shared_ptr<Value::Custom> Value::toCustom() const { return convert<std::shared_ptr<Custom> >(0); }
inline Map<String, Value> Value::toMap() const { return convert<Map<String, Value> >(0); }
inline List<Value> Value::toList() const { return convert<List<Value> >(0); }
template <typename T>
inline List<T> Value::toList() const
{
    List<T> ret;
    if (type() == Type_List) {
        ret.reserve(count());
        for (const Value &val : *listPtr()) {
            ret.append(val.convert<T>());
        }
    }
    return ret;
}

inline Value Value::value(int idx, const Value &defaultValue) const
{
    return mType == Type_List ? listPtr()->value(idx, defaultValue) : defaultValue;
}

template <typename T>
inline T Value::value(int idx, const T &defaultValue, bool *ok) const
{
    return value(idx, Value(defaultValue)).convert<T>(ok);
}

inline Value Value::value(const String &key, const Value &defaultValue) const
{
    return mType == Type_Map ? mapPtr()->value(key, defaultValue) : defaultValue;
}

template <typename T>
inline T Value::value(const String &key, const T &defaultValue, bool *ok) const
{
    return value(key, Value(defaultValue)).convert<T>(ok);
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

inline List<Value>::const_iterator Value::listBegin() const
{
    if (mType != Type_List)
        return listEnd();
    return listPtr()->begin();
}
inline List<Value>::const_iterator Value::listEnd() const
{
    if (mType != Type_List) {
        const static List<Value> hack;
        return hack.end();
    }
    return listPtr()->end();
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

inline bool Value::contains(const String &key) const
{
    if (mType == Type_Invalid) {
        return false;
    }
    assert(mType == Type_Map);
    return mapPtr()->contains(key);
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
    if (mType == Type_Invalid) {
        new (mData.listBuf) List<Value>;
        mType = Type_List;
    } else {
        assert(mType == Type_List);
    }
    return (*listPtr())[idx];
}

inline void Value::push_back(const Value &value)
{
    if (mType == Type_Invalid) {
        new (mData.listBuf) List<Value>;
        mType = Type_List;
    } else {
        assert(mType == Type_List);
    }
    listPtr()->push_back(value);
}

inline const Value &Value::operator[](const String &key) const
{
    assert(mType == Type_Map);
    return (*mapPtr())[key];
}

inline Value &Value::operator[](const String &key)
{
    if (mType == Type_Invalid) {
        new (mData.mapBuf) Map<String, Value>;
        mType = Type_Map;
    } else {
        assert(mType == Type_Map);
    }
    return (*mapPtr())[key];
}
template <typename T>
inline T Value::operator[](int idx) const
{
    assert(mType == Type_List);
    return (*listPtr())[idx].convert<T>();
}

template <typename T>
inline T Value::operator[](const String &key) const
{
    assert(mType == Type_Map);
    return (*mapPtr())[key].convert<T>();
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
        case Value::Type_Undefined: l << "(undefined)"; break;
        case Value::Type_Custom: {
            const std::shared_ptr<Value::Custom> custom = value.toCustom();
            if (custom) {
                l << custom->toString();
            } else {
                l << "Custom(0)";
            }
            break; }
        case Value::Type_List: l << value.toList(); break;
        case Value::Type_Map: l << value.toMap(); break;
        }
    }
    log << String::format<128>("Value(%s: %s)", Value::typeToString(value.type()),
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
    case Value::Type_Custom: error() << "Trying to serialize pointer"; break;
    case Value::Type_Invalid: break;
    case Value::Type_Undefined: break;
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
    case Value::Type_Custom: value.clear(); error() << "Trying to deserialize pointer"; break;
    case Value::Type_Invalid: value.clear(); break;
    case Value::Type_Undefined: value = Value::undefined(); break;
    }
    return deserializer;
}

#endif
