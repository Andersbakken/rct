#ifndef DB_h
#define DB_h

#ifdef RCT_DB_USE_MAP
#include <rct/Map.h>
#endif
#include <rct/Path.h>
#include <rct/Serializer.h>

template <typename Key, typename Value>
struct DBPrivate;
template <typename Key, typename Value>
class DB
{
public:
    inline DB();
    inline DB(DB<Key, Value> &&other);
    inline ~DB();

    inline DB &operator=(DB<Key, Value> &&other);

    enum Flag {
        None = 0x0,
        Overwrite = 0x1,
        AllowEmpty = 0x2
    };
    inline bool load(const Path &path, uint16_t version, unsigned int flags, String *error = 0);
    inline void unload();
    inline Path path() const { return mPath; }
    inline uint16_t version() const { return mVersion; }
    inline unsigned int flags() const { return mFlags; }
    inline Value value(const Key &key) const;
    class iterator {
    public:
        inline const Key &key() const;
        inline const Value &constValue() const;
        inline Value &value();

        inline void setValue(const Value &value);

        inline std::pair<const Key, Value> *operator->() const;
        inline std::pair<const Key, Value> &operator*() const;
        inline iterator &operator++();
        inline iterator &operator--();
        inline iterator operator++(int);
        inline iterator operator--(int);
        inline bool operator==(const iterator &other) const;
        inline bool operator!=(const iterator &other) const { return !operator==(other); }
    private:
#ifdef RCT_DB_USE_MAP
        typename Map<Key, Value>::iterator mIterator;
        iterator(const typename Map<Key, Value>::iterator it)
            : mIterator(it)
        {
        }
#endif
        friend class DB<Key, Value>;
    };

    inline iterator begin();
    inline iterator end();
    inline iterator lower_bound(const Key &key);
    inline iterator find(const Key &key);

    inline const Value &operator[](const Key &key) const;

    class const_iterator {
    public:
        inline const Key &key() const;
        inline const Value &constValue() const;
        inline const Value &value() const { return constValue(); }

        inline const std::pair<const Key, Value> *operator->() const;
        inline const std::pair<const Key, Value> &operator*() const;
        inline const_iterator &operator++();
        inline const_iterator &operator--();
        inline const_iterator operator++(int);
        inline const_iterator operator--(int);
        inline bool operator==(const const_iterator &other) const;
        inline bool operator!=(const const_iterator &other) const { return !operator==(other); }
    private:
#ifdef RCT_DB_USE_MAP
        typename Map<Key, Value>::const_iterator mIterator;
        const_iterator(const typename Map<Key, Value>::const_iterator it)
            : mIterator(it)
        {
        }
#endif
        friend class DB<Key, Value>;
    };
    inline const_iterator begin() const;
    inline const_iterator end() const;
    inline const_iterator constBegin() const { return begin(); }
    inline const_iterator constEnd() const { return end(); }
    inline const_iterator lower_bound(const Key &key) const;
    inline const_iterator find(const Key &key) const;
    inline bool contains(const Key &key) const { return find(key) != end(); }

    class WriteScope
    {
    public:
        inline ~WriteScope();
        bool flush(String *error = 0);
    private:
        inline WriteScope(DB &db);
        DB *mDB;
        friend class DB<Key, Value>;
    };
    inline std::shared_ptr<WriteScope> createWriteScope();
    inline int size() const;
    inline bool isEmpty() const { return !size(); }
    inline bool write(String *error = 0);
    inline void set(const Key &key, const Value &value);
    inline bool remove(const Key &key);
    inline void erase(const iterator &it);
private:
    DB(const DB &) = delete;
    DB &operator=(const DB &) = delete;

    Path mPath;
    unsigned int mFlags;
    uint16_t mVersion;
#ifdef RCT_DB_USE_MAP
    int mWriteScope;
    Map<Key, Value> mMap;
#endif
};

#ifdef RCT_DB_USE_MAP
#include <rct/DBmap.h>
#endif
#endif

