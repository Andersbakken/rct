#ifndef DBstlmap_h
#define DBstlmap_h

#include <rct/Serializer.h>

template <typename Key, typename Value>
DB<Key, Value>::DB()
    : mFlags(0), mVersion(0), mWriteScope(0)
{
}

template <typename Key, typename Value>
DB<Key, Value>::DB(DB<Key, Value> &&other)
    : mFlags(other.mFlags), mVersion(other.mVersion), mWriteScope(other.mWriteScope), mMap(std::move(other.mMap))
{
    other.mFlags = 0;
    other.mVersion = 0;
    other.mWriteScope = 0;
}

template <typename Key, typename Value>
DB<Key, Value>::~DB()
{
    assert(!mWriteScope);
}

template <typename Key, typename Value>
DB<Key, Value> &DB<Key, Value>::operator=(DB<Key, Value> &&other)
{
    mWriteScope = other.mWriteScope;
    other.mWriteScope = 0;
    mMap = std::move(other.mMap);
    return *this;
}

template <typename T>
static inline void deserializeValue(Deserializer &deserializer, std::shared_ptr<T> &value)
{
    assert(!value.get());
    value = std::make_shared<T>();
    deserializer >> *value.get();
}

template <typename T>
static inline void deserializeValue(Deserializer &deserializer, T &value)
{
    deserializer >> value;
}

template <typename Key, typename Value>
bool DB<Key, Value>::load(const Path &path, uint16_t version, unsigned int flags, String *err)
{
    bool error = false;
    FILE *f = 0;
    if (flags & Overwrite) {
        f = fopen(path.constData(), "w");
        error = !f;
        if (error && err) {
            *err = String::format<64>("Couldn't open file %s for writing: %d", path.constData(), errno);
        }
        fclose(f);
    } else {
        f = fopen(path.constData(), "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            const int size = ftell(f);
            if (size) {
                fseek(f, 0, SEEK_SET);
                char *buf = new char[size + 1];
                const int ret = fread(buf, sizeof(char), size, f);
                if (ret != size) {
                    delete[] buf;
                    error = true;
                } else {
                    buf[size] = '\0';
                    Deserializer deserializer(buf, size);
                    uint16_t ver;
                    deserializer >> ver;
                    if (ver != version) {
                        if (err)
                            *err = String::format<64>("Invalid version, got %d, expected %d", ver, version);
                        return false;
                    }
                    int size;
                    deserializer >> size;
                    Key key;
                    while (size--) {
                        deserializer >> key;
                        deserializeValue(deserializer, mMap[key]);
                    }
                }
            }
            fclose(f);
        } else if (!(flags & AllowEmpty)) {
            if (err)
                *err = String::format<64>("Couldn't open file %s for reading: %d", path.constData(), errno);
            error = true;
        }
    }
    if (!error) {
        mFlags = flags;
        mPath = path;
        mVersion = version;
    }
    return error;
}

template <typename Key, typename Value>
void DB<Key, Value>::unload()
{
    mPath.clear();
    mFlags = 0;
    mMap.clear();
}

template <typename Key, typename Value>
Value DB<Key, Value>::value(const Key &key) const
{
    return mMap.value(key);
}

template <typename Key, typename Value>
const Key &DB<Key, Value>::iterator::key() const
{
    return mIterator.first;
}

template <typename Key, typename Value>
const Value &DB<Key, Value>::iterator::constValue() const
{
    return mIterator.second;
}

template <typename Key, typename Value>
Value &DB<Key, Value>::iterator::value()
{
    return mIterator.second;
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator &DB<Key, Value>::iterator::operator++()
{
    ++mIterator;
    return *this;
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator &DB<Key, Value>::iterator::operator--()
{
    --mIterator;
    return *this;
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::iterator::operator++(int)
{
    const auto iterator = mIterator;
    ++mIterator;
    return iterator;
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::iterator::operator--(int)
{
    const auto iterator = mIterator;
    --mIterator;
    return iterator;
}

template <typename Key, typename Value>
typename std::pair<const Key, Value> *DB<Key, Value>::iterator::operator->() const
{
    return mIterator.operator->();
}

template <typename Key, typename Value>
typename std::pair<const Key, Value> &DB<Key, Value>::iterator::operator*() const
{
    return mIterator.operator*();
}

template <typename Key, typename Value>
bool DB<Key, Value>::iterator::operator==(const iterator &other) const
{
    return mIterator == other.mIterator;
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::begin()
{
    iterator it = { mMap.begin() };
    return it;
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::end()
{
    return mMap.end();
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::lower_bound(const Key &key)
{
    return mMap.lower_bound(key);
}

template <typename Key, typename Value>
typename DB<Key, Value>::iterator DB<Key, Value>::find(const Key &key)
{
    return mMap.find(key);
}

template <typename Key, typename Value>
Value &DB<Key, Value>::operator[](const Key &key)
{
    assert(mWriteScope);
    return mMap[key];
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator DB<Key, Value>::begin() const
{
    return mMap.begin();
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator DB<Key, Value>::end() const
{
    return mMap.end();
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator DB<Key, Value>::lower_bound(const Key &key) const
{
    return mMap.lower_bound(key);
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator DB<Key, Value>::find(const Key &key) const
{
    return mMap.find(key);
}

template <typename Key, typename Value>
const Key &DB<Key, Value>::const_iterator::key() const
{
    return mIterator.first;
}

template <typename Key, typename Value>
const Value &DB<Key, Value>::const_iterator::constValue() const
{
    return mIterator.second;
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator &DB<Key, Value>::const_iterator::operator++()
{
    ++mIterator;
    return *this;
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator &DB<Key, Value>::const_iterator::operator--()
{
    --mIterator;
    return *this;
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator DB<Key, Value>::const_iterator::operator++(int)
{
    const auto const_iterator = mIterator;
    ++mIterator;
    return const_iterator;
}

template <typename Key, typename Value>
typename DB<Key, Value>::const_iterator DB<Key, Value>::const_iterator::operator--(int)
{
    const auto const_iterator = mIterator;
    --mIterator;
    return const_iterator;
}

template <typename Key, typename Value>
const typename std::pair<const Key, Value> *DB<Key, Value>::const_iterator::operator->() const
{
    return mIterator.operator->();
}

template <typename Key, typename Value>
const typename std::pair<const Key, Value> &DB<Key, Value>::const_iterator::operator*() const
{
    return mIterator.operator*();
}

template <typename Key, typename Value>
bool DB<Key, Value>::const_iterator::operator==(const const_iterator &other) const
{
    return mIterator == other.mIterator;
}

template <typename Key, typename Value>
DB<Key, Value>::WriteScope::WriteScope(DB &db)
    : mDB(&db)
{
    ++mDB->mWriteScope;
}

template <typename Key, typename Value>
DB<Key, Value>::WriteScope::~WriteScope()
{
    if (mDB && !--mDB->mWriteScope) {
        mDB->write();
    }
}

template <typename Key, typename Value>
bool DB<Key, Value>::WriteScope::flush()
{
    assert(mDB);
    assert(mDB->mWriteScope == 1);
    --mDB->mWriteScope;
    const bool ret = mDB->write();
    mDB = 0;
    return ret;
}

template <typename Key, typename Value>
int DB<Key, Value>::size() const
{
    return mMap.size();
}

template <typename Key, typename Value>
std::shared_ptr<typename DB<Key, Value>::WriteScope> DB<Key, Value>::createWriteScope()
{
    return std::shared_ptr<WriteScope>(new WriteScope(*this));
}

template <typename T>
static inline void serializeValue(Serializer &serializer, const std::shared_ptr<T> &value)
{
    assert(value.get());
    serializer << *value.get();
}

template <typename T>
static inline void serializeValue(Serializer &serializer, const T &value)
{
    serializer << value;
}

template <typename Key, typename Value>
bool DB<Key, Value>::write()
{
    assert(!mWriteScope);
    FILE *f = fopen(mPath.constData(), "w");
    if (!f)
        return false;

    Serializer serializer(f);
    serializer << size();
    for (const auto &it : mMap) {
        serializer << it.first;
        serializeValue(serializer, it.second);
    }

    fclose(f);
    return true;
}

template <typename Key, typename Value>
void DB<Key, Value>::insert(const Key &key, const Value &value)
{
    assert(mWriteScope);
    mMap[key] = value;
}

template <typename Key, typename Value>
bool DB<Key, Value>::remove(const Key &key)
{
    assert(mWriteScope);
    return mMap.remove(key);
}

template <typename Key, typename Value>
void DB<Key, Value>::erase(const typename DB<Key, Value>::iterator &it)
{
    assert(mWriteScope);
    mMap.erase(it.mIterator);
}


#endif
