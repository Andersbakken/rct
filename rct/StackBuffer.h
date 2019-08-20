#ifndef StackBuffer_h
#define StackBuffer_h

template <size_t Size, typename T = char>
class StackBuffer
{
public:
    StackBuffer(size_t len)
        : mBuffer(nullptr), mSize(len)
    {
        if (len > Size) {
            mBuffer = new T[len];
        } else {
            mBuffer = mStackBuffer;
        }
    }
    ~StackBuffer()
    {
        if (mSize > Size)
            delete[] mBuffer;
    }
    void resize(size_t size)
    {
        T *old = nullptr;
        if (mSize > Size) {
            old = mBuffer;
        } else {
            old = mStackBuffer;
        }
        const size_t oldSize = mSize;
        mSize = size;
        if (mSize > Size) {
            mBuffer = new T[mSize];
        } else {
            mBuffer = mStackBuffer;
        }
        for (size_t i=0; i<oldSize; ++i) {
            mBuffer[i] = old[i];
        }
        if (old != mStackBuffer)
            delete[] old;
    }
    const T *buffer() const { return mBuffer; }
    T *buffer() { return mBuffer; }
    size_t size() const { return mSize; }
    T &operator[](int idx) { return mBuffer[idx]; }
    const T &operator[](int idx) const { return mBuffer[idx]; }
    operator T *() { return mBuffer; }
    operator const T *() const { return mBuffer; }
private:
    StackBuffer(const StackBuffer &) = delete;
    StackBuffer &operator=(const StackBuffer &) = delete;
    T mStackBuffer[Size];
    T *mBuffer;
    size_t mSize;
};

#endif
