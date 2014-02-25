#ifndef Rct_h
#define Rct_h

#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <rct/String.h>
#include <rct/Path.h>
#include <rct/List.h>
#include <sys/select.h>

namespace Rct {

enum { Max_USec = 1000000 };

String shortOptions(const option *longOptions);
int readLine(FILE *f, char *buf = 0, int max = -1);
inline int fileSize(FILE *f)
{
    assert(f);
    const int pos = ftell(f);
    fseek(f, 0, SEEK_END);
    const int ret = ftell(f);
    fseek(f, pos, SEEK_SET);
    return ret;
}
template <typename Container, typename Value>
inline bool addTo(Container &container, const Value &value)
{
    const int oldSize = container.size();
    container += value;
    return container.size() != oldSize;
}
bool readFile(const Path& path, String &data);
bool readFile(FILE *f, String &data);
bool writeFile(const Path& path, const String& data);
void removeDirectory(const Path &path);
String unescape(String command);
void findExecutablePath(const char *argv0);
Path executablePath();
String backtrace(int maxFrames = -1);
bool gettime(timeval* time);
uint64_t monoMs();
uint64_t currentTimeMs();
String hostName();

template <typename Node>
void insertLinkedListNode(Node *node, Node *&first, Node *&last, Node *after = 0)
{
    assert(node);
    if (after) {
        assert(first);
        assert(last);
        node->next = after->next;
        if (after->next) {
            after->next->prev = node;
        } else {
            assert(last == after);
            last = node;
        }
        after->next = node;
        node->prev = after;
    } else if (!first) {
        first = last = node;
    } else {
        node->next = first;
        assert(first);
        first->prev = node;
        first = node;
    }
}

template <typename Node>
void removeLinkedListNode(Node *node, Node *&first, Node *&last)
{
    assert(node);
    if (node == first) {
        if (node == last) {
            first = last = 0;
        } else {
            first = node->next;
            first->prev = 0;
        }
    } else if (node == last) {
        assert(node->prev);
        node->prev->next = 0;
        last = node->prev;
    } else {
        assert(node->prev);
        assert(node->next);
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    node->next = node->prev = 0; // ### ???
}

template <typename Node>
void deleteLinkedListNodes(Node *node)
{
    while (node) {
        Node *tmp = node;
        node = node->next;
        delete tmp;
    }
}

enum LookupMode { Auto, IPv4, IPv6 };
String addrLookup(const String& addr, LookupMode mode = Auto, bool *ok = 0);
String nameLookup(const String& name, LookupMode mode = IPv4, bool *ok = 0);

inline bool timevalGreaterEqualThan(const timeval* a, const timeval* b)
{
    return (a->tv_sec > b->tv_sec
            || (a->tv_sec == b->tv_sec && a->tv_usec >= b->tv_usec));
}

inline void timevalAdd(timeval* a, int diff)
{
    a->tv_sec += diff / 1000;
    a->tv_usec += (diff % 1000) * 1000;
    if (a->tv_usec >= Max_USec) {
        ++a->tv_sec;
        a->tv_usec -= Max_USec;
    }
}

inline void timevalSub(timeval* a, timeval* b)
{
    a->tv_sec -= b->tv_sec;
    a->tv_usec -= b->tv_usec;
    if (a->tv_sec < 0) {
        a->tv_sec = a->tv_usec = 0;
    } else if (a->tv_usec < 0) {
        if (--a->tv_sec < 0) {
            a->tv_sec = a->tv_usec = 0;
        } else {
            a->tv_usec += Max_USec;
        }
    }
}

inline uint64_t timevalMs(timeval* a)
{
    return (a->tv_sec * 1000) + (a->tv_usec / 1000);
}

inline int timevalDiff(timeval* a, timeval* b)
{
    const uint64_t ams = timevalMs(a);
    const uint64_t bms = timevalMs(b);
    return ams - bms;
}

}

#define eintrwrap(VAR, BLOCK)                   \
    do {                                        \
        VAR = BLOCK;                            \
    } while (VAR == -1 && errno == EINTR)

#endif
