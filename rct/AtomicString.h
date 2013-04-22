#ifndef AtomicString_h
#define AtomicString_h

#include <rct/Tr1.h>
#include <rct/Mutex.h>

namespace std {
    template<>
    class hash<String> {
    public:
        size_t operator()(const String& str) const
        {
            hash<std::string> h;
            return h(str.str());
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
    operator const String&() const { return *string; }
    operator const char*() const { return string->constData(); }

    const String* operator->() const { return &*string; }
    const String& operator*() const { return *string; }

    bool operator<(const AtomicString& other) { return string->operator<(*other.string); }

private:
    static Mutex mutex;
    static unordered_set<String> strings;

    static unordered_set<String>::iterator add(const String& string);
    static unordered_set<String>::iterator null();

    unordered_set<String>::iterator string;
};

#endif
