#ifndef AtomicString_h
#define AtomicString_h

#include <rct/Tr1.h>
#include <rct/Mutex.h>

class StringWrapper
{
public:
    StringWrapper(const char* s) : cstr(s) { }
    StringWrapper(const String& s) : cstr(0), str(s) { }

    bool operator==(const StringWrapper& other) const
    {
        if (cstr) {
            if (other.cstr)
                return !strcmp(cstr, other.cstr);
            return !strcmp(cstr, other.str.constData());
        }
        if (other.cstr)
            return !strcmp(str.constData(), other.cstr);
        return str == other.str;
    }

    const char* cstr;
    String str;
};

namespace std {
    template<>
    class hash<StringWrapper>
    {
    public:
        size_t operator()(const StringWrapper& str) const
        {
            hash<const char*> h;
            if (str.cstr)
                return h(str.cstr);
            return h(str.str.constData());
        }
    };
}

class AtomicString
{
public:
    AtomicString() { string = null(); }
    explicit AtomicString(const String& str) { string = add(str); }
    AtomicString(const char* str, int len = -1) { string = add(String(str, len)); }
    AtomicString(const AtomicString& str) { string = str.string; }

    // this should be safe with no mutex locked
    operator const String&() const { return string->str; }
    operator const char*() const { return string->str.constData(); }

    const String* operator->() const { return &string->str; }
    const String& operator*() const { return string->str; }

    bool operator<(const AtomicString& other) const { return string->str.operator<(other.string->str); }
    bool operator==(const AtomicString& other) const { return string->str.operator==(other.string->str); }

    AtomicString& operator=(const AtomicString& other) { string = other.string; return *this; }

private:
    static Mutex mutex;
    static unordered_set<StringWrapper> strings;

    static unordered_set<StringWrapper>::iterator add(const String& string);
    static unordered_set<StringWrapper>::iterator add(const char* string);

    static unordered_set<StringWrapper>::iterator null();

    unordered_set<StringWrapper>::iterator string;
};

#endif
