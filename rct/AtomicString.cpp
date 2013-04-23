#include "AtomicString.h"
#include <rct/MutexLocker.h>

Mutex AtomicString::mutex;
unordered_set<StringWrapper> AtomicString::strings;

unordered_set<StringWrapper>::iterator AtomicString::add(const String& str)
{
    MutexLocker locker(&mutex);
    StringWrapper wrapper(str);
    std::pair<unordered_set<StringWrapper>::iterator, bool> i = strings.insert(wrapper);
    return i.first;
}

unordered_set<StringWrapper>::iterator AtomicString::add(const char* str)
{
    MutexLocker locker(&mutex);
    StringWrapper wrapper(str);
    std::pair<unordered_set<StringWrapper>::iterator, bool> i = strings.insert(wrapper);
    if (i.second) {
        strings.erase(i.first);
        wrapper.cstr = 0;
        wrapper.str = str;
        i = strings.insert(wrapper);
        assert(i.second);
    }
    return i.first;
}

unordered_set<StringWrapper>::iterator AtomicString::null()
{
    MutexLocker locker(&mutex);
    return strings.end();
}
