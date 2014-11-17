#ifndef DB_h
#define DB_h

#include <rct/Path.h>
#include <memory>

template <typename Key, typename Value>
struct DBPrivate;
template <typename Key, typename Value>
class DB
{
public:
    DB();
    ~DB();

    bool load(const Path &path) = 0;
    std::shared_ptr<Value> value(const Key &key) const = 0;
    class iterator {
    public:
        Key key() const;
        std::shared_ptr<Value> value() const;
        bool atEnd() const;
        iterator &operator++();
        iterator &operator--();
        bool operator==(const iterator &other) const;
        bool operator!=(const iterator &other) const { return !operator==(other); }
    };
    iterator begin() const;
    iterator end() const;
    iterator lowerBound(const Key &key) const;
    iterator find(const Key &key) const;

    class WriteScope
    {
    public:
        WriteScope(DB &db);
        ~WriteScope();
    };
    int size() const;
private:
    DB(const DB &) = delete;
    DB &operator=(const DB &) = delete;

    DBPrivate<Key, Value> *mPrivate;
};
#ifdef DB_STLMAP
#include <rct/DBstlmap.h>
#endif
#endif
