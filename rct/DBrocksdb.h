#include <rct/Serializer.h>

namespace DBRocksDBHelpers {

template <typename T, typename std::enable_if<!FixedSize<T>::value>::type * = nullptr>
inline void toString(const T &t, String &string, rocksdb::Slice &slice)
{
    Serializer serializer(string);
    serializer << t;
    slice = rocksdb::Slice(string.constData(), string.size());
}


template <typename T, typename std::enable_if<FixedSize<T>::value>::type * = nullptr>
inline void toString(const T &t, String &, rocksdb::Slice &slice)
{
    slice = rocksdb::Slice(reinterpret_cast<const char*>(&t), FixedSize<T>::value);
}

template <>
inline void toString(const String &t, String &, rocksdb::Slice &slice)
{
    slice = rocksdb::Slice(t.constData(), t.size());
}

template <typename T>
inline void toString(const std::shared_ptr<T> &t, String &string, rocksdb::Slice &slice)
{
    toString(*t.get(), string, slice);
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
const Key &DB<Key, Value>::iterator::key() const
{
    assert(mIterator->Valid());
    // return mIt
    // return mIterator.first;
}

template <typename Key, typename Value>
const Value &DB<Key, Value>::iterator::value() const
{
    // return mIterator.second;
}

template <typename Key, typename Value>
void DB<Key, Value>::iterator::setValue(const Value &value)
{
    // assert(mDB->mWriteScope);
    // mIterator->second = value;
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator &DB<Key, Value>::iterator::operator++()
{
    // ++mIterator;
    // return *this;
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator &DB<Key, Value>::iterator::operator--()
{
    // --mIterator;
    // return *this;
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::iterator::operator++(int)
{
    // assert(mIterator != mDB->end().mIterator);
    // const auto prev = mIterator;
    // ++mIterator;
    // return iterator(mDB, prev);
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::iterator::operator--(int)
{
    // assert(mIterator != mDB->begin().mIterator);
    // const auto prev = mIterator;
    // --mIterator;
    // return iterator(mDB, prev);
}

template <typename Key, typename Value>
const typename std::pair<const Key, Value> *DB<Key, Value>::iterator::operator->() const
{
    // return mIterator.operator->();
}

template <typename Key, typename Value>
const typename std::pair<const Key, Value> &DB<Key, Value>::iterator::operator*() const
{
    // return mIterator.operator*();
}

template <typename Key, typename Value>
bool DB<Key, Value>::iterator::operator==(const iterator &other) const
{
    // return mIterator == other.mIterator;
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::begin()
{
    // return iterator(this, mMap.begin());
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::end()
{
    // return iterator(this, mMap.end());
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::lower_bound(const Key &key)
{
    // return iterator(this, mMap.lower_bound(key));
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::find(const Key &key)
{
    // return iterator(this, mMap.find(key));
}

template <typename Key, typename Value>
Value DB<Key, Value>::operator[](const Key &key) const
{
#warning this probably needs to look at the current writeBatch if there is one.
    assert(mRocksDB);
    std::string value;
    String k;
    rocksdb::Slice slice;
    DBRocksDBHelpers::toString<Key>(key, k, slice);
    rocksdb::Status s = mRocksDB->Get(rocksdb::ReadOptions(), slice, &value);
    Value ret;
    if (s.ok()) {
        DBRocksDBHelpers::fromSlice(value, ret);
    }
    return ret;
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator DB<Key, Value>::begin() const
{
    assert(mRocksDB);
    rocksdb::Iterator *it = mRocksDB->NewIterator(mReadOptions);
    it->SeekToFirst();
    return const_iterator(it, this);
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator DB<Key, Value>::end() const
{
    assert(mRocksDB);
    return const_iterator(mRocksDB->NewIterator(mReadOptions), this);
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator DB<Key, Value>::lower_bound(const Key &key) const
{
    // return mMap.lower_bound(key);
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator DB<Key, Value>::find(const Key &key) const
{
    // return mMap.find(key);
}

template <typename Key, typename Value>
const Key &DB<Key, Value>::const_iterator::key() const
{
    if (!(mCache & CachedKey)) {
        const rocksdb::Slice slice = mIterator->key();
        DBRocksDBHelpers::fromSlice(slice, mCachedKey);
        mCache |= CachedKey;
    }
    return mCachedKey;
}

template <typename Key, typename Value>
const Value &DB<Key, Value>::const_iterator::value() const
{
    if (!(mCache & CachedValue)) {
        const rocksdb::Slice slice = mIterator->value();
        DBRocksDBHelpers::fromSlice(slice, mCachedValue);
        mCache |= CachedValue;
    }
    return mCachedValue;
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator &DB<Key, Value>::const_iterator::operator++()
{
    assert(mIterator);
    mIterator->Next();
    clearCache();
    return *this;
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator &DB<Key, Value>::const_iterator::operator--()
{
    assert(mIterator);
    mIterator->Prev();
    clearCache();
    return *this;
}

template <typename Key, typename Value>
const typename std::pair<const Key, Value> *DB<Key, Value>::const_iterator::operator->() const
{
    // return mIterator.operator->();
}

template <typename Key, typename Value>
const typename std::pair<const Key, Value> &DB<Key, Value>::const_iterator::operator*() const
{
    // return mIterator.operator*();
}

template <typename Key, typename Value>
bool DB<Key, Value>::const_iterator::operator==(const const_iterator &other) const
{
    // return mIterator == other.mIterator;
}

template <typename Key, typename Value>
DB<Key, Value>::WriteScope::WriteScope(DB *db, int reservedSize)
    : mDB(db)
{
    assert((mDB->mWriteBatch == 0) == (mDB->mWriteScope == 0));
    ++mDB->mWriteScope;
    if (!mDB->mWriteBatch) {
        mDB->mWriteBatch = new rocksdb::WriteBatch(reservedSize);
    }
    // ++mDB->mWriteScope;
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
    DBRocksDBHelpers::toString(key, k, ks);
    DBRocksDBHelpers::toString(value, v, vs);
    mWriteBatch->Put(ks, vs);
}

template <typename Key, typename Value>
bool DB<Key, Value>::remove(const Key &key)
{
    assert(mWriteBatch);
    String k;
    rocksdb::Slice slice;
    DBRocksDBHelpers::toString(key, k, slice);
    mWriteBatch->Delete(slice);
}

template <typename Key, typename Value>
void DB<Key, Value>::erase(const typename DB<Key, Value>::iterator &it)
{
    // assert(mWriteScope);
    // mMap.erase(it.mIterator);
}
