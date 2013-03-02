#ifndef Value_h
#define Value_h

#include <rct/String.h>
#include <rct/Log.h>
#include <math.h>

class Value
{
public:
    inline Value() : mType(Type_Invalid) {}
    inline Value(int i) : mType(Type_Integer) { mData.integer = i; }
    inline Value(double d) : mType(Type_Double) { mData.dbl = d; }
    inline Value(bool b) : mType(Type_Boolean) { mData.boolean = b; }
    Value(const String &string) : mType(Type_String) { new (mData.stringBuf) String(string); }
    Value(const Value &other) { copy(other); }
    Value &operator=(const Value &other) { clear(); copy(other); return *this; }
    ~Value() { clear(); }
    inline bool isNull() const { return mType == Type_Invalid; }
    inline bool isValid() const { return mType != Type_Invalid; }
    enum Type {
        Type_Invalid,
        Type_Boolean,
        Type_Integer,
        Type_Double,
        Type_String
    };
    inline static const char *typeToString(Type type);
    inline Type type() const { return mType; }
    inline bool toBool() const;
    inline int toInteger() const;
    inline double toDouble() const;
    inline String toString() const;
    template <typename T> inline T convert() const { invalidType(T()); return T(); }
    template <typename T> static Value create(const T &t) { return Value(t); }
    void clear();
private:
    void copy(const Value &other);
    String *stringPtr() { return reinterpret_cast<String*>(mData.stringBuf); }
    const String *stringPtr() const { return reinterpret_cast<const String*>(mData.stringBuf); }
    Type mType;
    union {
        int integer;
        double dbl;
        bool boolean;
        char stringBuf[sizeof(String)];
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
  }
  return "";
}


template <> inline int Value::convert<int>() const
{
  switch (mType) {
  case Type_Integer: return mData.integer;
  case Type_Double: return static_cast<int>(round(mData.dbl));
  case Type_Boolean: return mData.boolean;
  case Type_String:
    return atoi(stringPtr()->constData());
  case Type_Invalid: return 0;
    break;
  }
  return false;
}

template <> inline bool Value::convert<bool>() const
{
  switch (mType) {
  case Type_Integer: return mData.integer;
  case Type_Double: return mData.dbl;
  case Type_Boolean: return mData.boolean;
  case Type_String:
  {
    const String str = toString();
    return (str == "true" || str == "1");
  }
  case Type_Invalid:
    break;
  }
  return false;
}


template <> inline double Value::convert<double>() const
{
  switch (mType) {
  case Type_Integer: return mData.integer;
  case Type_Double: return mData.dbl;
  case Type_Boolean: return mData.boolean;
  case Type_String: return strtod(stringPtr()->constData(), 0);
  case Type_Invalid:
    break;
  }
  return false;
}

template <> inline String Value::convert<String>() const
{
  switch (mType) {
  case Type_Integer: return String::number(mData.integer);
  case Type_Double: return String::number(mData.dbl);
  case Type_Boolean: return mData.boolean ? "true" : "false";
  case Type_String: return *stringPtr();
  case Type_Invalid:
    break;
  }
  return String();
}

inline bool Value::toBool() const { return convert<bool>(); }
inline int Value::toInteger() const { return convert<int>(); }
inline double Value::toDouble() const { return convert<double>(); }
inline String Value::toString() const { return convert<String>(); }

inline Log operator<<(Log log, const Value &value)
{
  log << String::format<128>("Variant(%s: %s)", Value::typeToString(value.type()),
                             value.toString().constData());
  return log;
}

#endif
