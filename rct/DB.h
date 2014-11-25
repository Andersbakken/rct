#ifndef DB_h
#define DB_h

#ifdef RCT_DB_USE_MAP
#include <rct/Map.h>
#elif defined(RCT_DB_USE_ROCKSDB)
#include <rocksdb/db.h>
#include <rocksdb/slice.h>
#include <rocksdb/options.h>
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
        Overwrite = 0x1
    };
    inline bool open(const Path &path, uint16_t version, unsigned int flags = None);
    inline void close();
    inline Path path() const { return mPath; }
    inline uint16_t version() const { return mVersion; }
    inline Value value(const Key &key) const { return operator[](key); }
    class Iterator {
    public:
#ifdef RCT_DB_USE_ROCKSDB
        ~Iterator()
        {
            delete mIterator;
        }
#endif

        inline const Key &key() const;
        inline const Value &value() const;

        inline void setValue(const Value &value);

        inline void next();
        inline void prev();
        inline bool isValid() const;
        inline void seekToFront();
        inline void seekToEnd();
        inline void erase();
    private:
        Iterator(const Iterator &it) = delete;
        Iterator(Iterator &&it) = delete;
        Iterator &operator=(const Iterator &it) = delete;
#ifdef RCT_DB_USE_MAP
        Iterator(DB<Key, Value> *db, const typename Map<Key, Value>::iterator &it)
            : mIterator(it), mDB(db)
        {
        }
        typename Map<Key, Value>::iterator mIterator;
#elif defined(RCT_DB_USE_ROCKSDB)
        Iterator(DB<Key, Value> *db, rocksdb::Iterator *it)
            : mIterator(it), mCache(0), mDB(db)
        {
        }

        rocksdb::Iterator *mIterator;
        mutable Key mCachedKey;
        mutable Value mCachedValue;
        mutable uint8_t mCache;
        enum {
            CachedKey = 1,
            CachedValue = 2
        };
        inline void clearCache()
        {
            if (mCache & CachedKey)
                mCachedKey = Key();
            if (mCache & CachedValue)
                mCachedValue = Value();
            mCache = 0;
        }
#endif
        DB<Key, Value> *mDB;
        friend class DB<Key, Value>;
    };

    inline std::unique_ptr<Iterator> createIterator(); // seeks to begin
    inline std::unique_ptr<Iterator> lower_bound(const Key &key);
    inline std::unique_ptr<Iterator> find(const Key &key);
    inline Value operator[](const Key &key) const;

    inline bool contains(const Key &key) { return find(key)->isValid(); }
    // ### need a const_iterator

    class WriteScope
    {
    public:
        inline ~WriteScope();
        bool flush(String *error = 0);
    private:
        inline WriteScope(DB *db, int reservedSize);
        DB *mDB;
        friend class DB<Key, Value>;
    };

    inline std::shared_ptr<WriteScope> createWriteScope(int reservedSize)
    {
        return std::shared_ptr<WriteScope>(new WriteScope(this, reservedSize));
    }
    inline int size() const;
    inline bool isEmpty() const;
    inline void set(const Key &key, const Value &value);
    inline bool remove(const Key &key);
private:
    DB(const DB &) = delete;
    DB &operator=(const DB &) = delete;

    Path mPath;
    uint16_t mVersion;
#ifdef RCT_DB_USE_MAP
    inline bool write(String *error = 0);
    int mWriteScope;
    Map<Key, Value> mMap;
#elif defined(RCT_DB_USE_ROCKSDB)
    rocksdb::DB *mRocksDB;
    int mWriteScope;
    rocksdb::WriteBatch *mWriteBatch;
    const rocksdb::ReadOptions mReadOptions;
#endif
};

#ifdef RCT_DB_USE_MAP
#include <rct/DBmap.h>
#elif defined(RCT_DB_USE_ROCKSDB)
#include <rct/DBrocksdb.h>
#endif

#endif
