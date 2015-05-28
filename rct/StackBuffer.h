#ifndef StackBuffer_h
#define StackBuffer_h

template <int Size, typename T = char>
class StackBuffer
{
public:
    StackBuffer(int size)
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
    void resize(int size)
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
    int size() const { return mSize; }
    T &operator[](int idx) { return mBuffer[idx]; }
    const T &operator[](int idx) const { return mBuffer[idx]; }
    operator T *() { return mBuffer; }
    operator const T *() const { return mBuffer; }
private:
    StackBuffer(const StackBuffer &) = delete;
    StackBuffer &operator=(const StackBuffer &) = delete;
    T mStackBuffer[Size];
    T *mBuffer;
    int mSize;
};

#endif
