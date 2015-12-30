#ifndef StackBuffer_h
#define StackBuffer_h

template <size_t Size, typename T = char>
class StackBuffer
{
public:
    StackBuffer(size_t size)
        : mBuffer(0), mSize(size)
    {
        if (size > Size) {
            mBuffer = new T[size];
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
        if (mSize > Size)
            delete[] mBuffer;
        mSize = size;
        if (mSize > Size) {
            mBuffer = new T[mSize];
        } else {
            mBuffer = mStackBuffer;
        }
    }
    const T *buffer() const { return mBuffer; }
    T *buffer() { return mBuffer; }
    size_t size() const { return mSize; }
    T &operator[](size_t idx) { return mBuffer[idx]; }
    const T &operator[](size_t idx) const { return mBuffer[idx]; }
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
