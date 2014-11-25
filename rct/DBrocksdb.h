#include <rct/Serializer.h>

namespace DBRocksDBHelpers {

template <typename T, typename std::enable_if<!FixedSize<T>::value>::type * = nullptr>
inline void toSlice(const T &t, rocksdb::Slice &slice, String &string)
{
    Serializer serializer(string);
    serializer << t;
    slice = rocksdb::Slice(string.constData(), string.size());
}


template <typename T, typename std::enable_if<FixedSize<T>::value>::type * = nullptr>
inline void toSlice(const T &t, rocksdb::Slice &slice, String &)
{
    slice = rocksdb::Slice(reinterpret_cast<const char*>(&t), FixedSize<T>::value);
}

template <>
inline void toSlice(const String &t, rocksdb::Slice &slice, String &)
{
    slice = rocksdb::Slice(t.constData(), t.size());
}

template <typename T>
inline void toSlice(const std::shared_ptr<T> &t, rocksdb::Slice &slice, String &string)
{
    toSlice(*t.get(), slice, string);
}

template <typename T, typename std::enable_if<!FixedSize<T>::value>::type * = nullptr>
static inline void fromSlice(const rocksdb::Slice &slice, T &value)
{
    Deserializer deserializer(slice.data(), slice.size());
    deserializer >> value;
}

template <typename T, typename std::enable_if<FixedSize<T>::value>::type * = nullptr>
static inline void fromSlice(const rocksdb::Slice &slice, T &value)
{
    memcpy(reinterpret_cast<char*>(&value), slice.data(), FixedSize<T>::value);
}

template <>
inline void fromSlice(const rocksdb::Slice &s, String &string)
{
    string.assign(s.data(), s.size());
}

template <typename Value>
static inline void fromSlice(const std::string &string, std::shared_ptr<Value> &value)
{
    value = std::make_shared<Value>();
    fromSlice(string, *value.get());
}
}

template <typename Key, typename Value>
DB<Key, Value>::DB()
    : mRocksDB(0), mWriteScope(0), mWriteBatch(0), mReadOptions(rocksdb::ReadOptions())
{
}

template <typename Key, typename Value>
DB<Key, Value>::DB(DB<Key, Value> &&other)
    : mRocksDB(other.mDB), mWriteScope(other.mWriteScope),
      mWriteBatch(other.mWriteBatch), mReadOptions(rocksdb::ReadOptions())
{
    other.mDB = 0;
    other.mWriteBatch = 0;
    other.mWriteScope = 0;
}

template <typename Key, typename Value>
DB<Key, Value>::~DB()
{
    close();
}

template <typename Key, typename Value>
DB<Key, Value> &DB<Key, Value>::operator=(DB<Key, Value> &&other)
{
    close();
    mRocksDB = other.mDB;
    mWriteScope = other.mWriteScope;
    mWriteBatch = other.mWriteBatch;

    other.mDB = 0;
    other.mWriteBatch = 0;
    other.mWriteScope = 0;
    return *this;
}


template <typename Key, typename Value>
bool DB<Key, Value>::open(const Path &path, uint16_t version, unsigned int flags)
{
    assert(!mRocksDB);
    rocksdb::Options options;
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();
    options.create_if_missing = true;

    if (flags & Overwrite)
        Rct::removeDirectory(path);

    rocksdb::Status s = rocksdb::DB::Open(options, path.ref(), &mRocksDB);
    if (!s.ok())
        return false;
    return true;
}

template <typename Key, typename Value>
void DB<Key, Value>::close()
{
    assert(!mWriteBatch);
    assert(!mWriteScope);
    mPath.clear();
    delete mRocksDB;
    mRocksDB = 0;
}

template <typename Key, typename Value>
const Key &DB<Key, Value>::Iterator::key() const
{
    assert(mIterator);
    assert(mIterator->Valid());
    if (!(mCache & CachedKey)) {
        const rocksdb::Slice slice = mIterator->key();
        DBRocksDBHelpers::fromSlice(slice, mCachedKey);
        mCache |= CachedKey;
    }
    return mCachedKey;
}

template <typename Key, typename Value>
const Value &DB<Key, Value>::Iterator::value() const
{
    assert(mIterator);
    assert(mIterator->Valid());
    if (!(mCache & CachedValue)) {
        const rocksdb::Slice slice = mIterator->value();
        DBRocksDBHelpers::fromSlice(slice, mCachedValue);
        mCache |= CachedValue;
    }
    return mCachedValue;
}

template <typename Key, typename Value>
void DB<Key, Value>::Iterator::setValue(const Value &value)
{
    assert(mIterator);
    assert(mIterator->Valid());
    const Key k = key();
    mCachedValue = value;
    mCache |= CachedValue;
    assert(mDB);
    mDB->set(k, value);
}

template <typename Key, typename Value>
void DB<Key, Value>::Iterator::erase()
{
    assert(mIterator);
    assert(mIterator->Valid());
    const Key k = key();
    assert(mDB);
    next();
    mDB->remove(k);
}

template <typename Key, typename Value>
void DB<Key, Value>::Iterator::next()
{
    assert(mIterator);
    assert(mIterator->Valid());
    clearCache();
    mIterator->Next();
}

template <typename Key, typename Value>
void DB<Key, Value>::Iterator::prev()
{
    assert(mIterator);
    assert(mIterator->Valid());
    clearCache();
    mIterator->Prev();
}

template <typename Key, typename Value>
void DB<Key, Value>::Iterator::seekToFront()
{
    assert(mIterator);
    clearCache();
    mIterator->SeekToFirst();
}

template <typename Key, typename Value>
void DB<Key, Value>::Iterator::seekToEnd()
{
    assert(mIterator);
    clearCache();
    mIterator->SeekToLast();
}

template <typename Key, typename Value>
bool DB<Key, Value>::Iterator::isValid() const
{
    assert(mIterator);
    return mIterator->Valid();
}

template <typename Key, typename Value>
std::unique_ptr<typename DB<Key, Value>::Iterator> DB<Key, Value>::createIterator()
{
    rocksdb::Iterator *it = mRocksDB->NewIterator(mReadOptions);
    it->SeekToFirst();
    return std::unique_ptr<typename DB<Key, Value>::Iterator>(new Iterator(this, it));
}

template <typename Key, typename Value>
std::unique_ptr<typename DB<Key, Value>::Iterator> DB<Key, Value>::lower_bound(const Key &key)
{
    rocksdb::Iterator *it = mRocksDB->NewIterator(mReadOptions);
    String k;
    rocksdb::Slice slice;
    DBRocksDBHelpers::toSlice<Key>(key, slice, k);
    it->Seek(slice);
    return std::unique_ptr<typename DB<Key, Value>::Iterator>(new Iterator(this, it));
}

template <typename Key, typename Value>
std::unique_ptr<typename DB<Key, Value>::Iterator> DB<Key, Value>::find(const Key &key)
{
    rocksdb::Iterator *it = mRocksDB->NewIterator(mReadOptions);
    String k;
    rocksdb::Slice slice;
    DBRocksDBHelpers::toSlice<Key>(key, slice, k);
    it->Seek(slice);
    if (it->key() != key) {
        delete it;
        it = mRocksDB->NewIterator(mReadOptions);
    }
    return std::unique_ptr<typename DB<Key, Value>::Iterator>(new Iterator(this, it));
}

template <typename Key, typename Value>
Value DB<Key, Value>::operator[](const Key &key) const
{
#warning this probably needs to look at the current writeBatch if there is one.
    assert(mRocksDB);
    std::string value;
    String k;
    rocksdb::Slice slice;
    DBRocksDBHelpers::toSlice<Key>(key, slice, k);
    rocksdb::Status s = mRocksDB->Get(rocksdb::ReadOptions(), slice, &value);
    Value ret;
    if (s.ok()) {
        DBRocksDBHelpers::fromSlice(value, ret);
    }
    return ret;
}

template <typename Key, typename Value>
DB<Key, Value>::WriteScope::WriteScope(DB *db, int reservedSize)
    : mDB(db)
{
    assert((mDB->mWriteBatch == 0) == (mDB->mWriteScope == 0));
    ++mDB->mWriteScope;
}

template <typename Key, typename Value>
DB<Key, Value>::WriteScope::~WriteScope()
{
    if (mDB) {
        if (mDB->mWriteScope == 1) {
            flush();
        } else {
            --mDB->mWriteScope;
        }
    }
}

template <typename Key, typename Value>
bool DB<Key, Value>::WriteScope::flush(String *error)
{
    assert(mDB);
    assert(mDB->mWriteScope == 1);
    assert(mDB->mWriteBatch);
    --mDB->mWriteScope;
    const rocksdb::Status status = mDB->mRocksDB->Write(rocksdb::WriteOptions(), mDB->mWriteBatch);
    delete mDB->mWriteBatch;
    mDB->mWriteBatch = 0;
    mDB = 0;
    return status.ok();
}

template <typename Key, typename Value>
int DB<Key, Value>::size() const
{
    assert(mRocksDB);
    uint64_t count = 0;
    mRocksDB->GetIntProperty("rocksdb.estimate-num-keys", &count);
    return static_cast<int>(count);
}

template <typename Key, typename Value>
bool DB<Key, Value>::isEmpty() const
{
    return !size();
}

template <typename Key, typename Value>
void DB<Key, Value>::set(const Key &key, const Value &value)
{
    assert(mWriteBatch);
    String k, v;
    rocksdb::Slice ks, vs;
    DBRocksDBHelpers::toSlice(key, ks, k);
    DBRocksDBHelpers::toSlice(value, vs, v);
    mWriteBatch->Put(ks, vs);
}

template <typename Key, typename Value>
bool DB<Key, Value>::remove(const Key &key)
{
    assert(mWriteBatch);
    String k;
    rocksdb::Slice slice;
    DBRocksDBHelpers::toSlice(key, slice, k);
    mWriteBatch->Delete(slice);
}
