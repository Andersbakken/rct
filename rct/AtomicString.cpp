#include "AtomicString.h"
#include <rct/MutexLocker.h>

Mutex AtomicString::mutex;
unordered_set<String> AtomicString::strings;

unordered_set<String>::iterator AtomicString::add(const String& str)
{
    MutexLocker locker(&mutex);
    std::pair<unordered_set<String>::iterator, bool> i = strings.insert(str);
    return i.first;
}

unordered_set<String>::iterator AtomicString::null()
{
    MutexLocker locker(&mutex);
    return strings.end();
}
