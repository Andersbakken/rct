#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>
#include <assert.h>
#include <rct/LinkedList.h>
#include <rct/String.h>
#include <string.h>
#include <utility>

class Buffer
{
public:
    Buffer()
        : bufferData(nullptr), bufferSize(0), bufferReserved(0)
    {
    }
    Buffer(Buffer&& other)
    {
        bufferData = other.bufferData;
        bufferSize = other.bufferSize;
        bufferReserved = other.bufferReserved;
        other.bufferData = nullptr;
        other.bufferSize = 0;
        other.bufferReserved = 0;
    }
    ~Buffer()
    {
        if (bufferData)
            free(bufferData);
    }

    Buffer& operator=(Buffer&& other)
    {
        bufferData = other.bufferData;
        bufferSize = other.bufferSize;
        bufferReserved = other.bufferReserved;
        other.bufferData = nullptr;
        other.bufferSize = 0;
        other.bufferReserved = 0;
        return *this;
    }

    bool isEmpty() const { return !bufferSize; }

    void clear()
    {
        enum { ClearThreshold = 1024 * 512 };
        if (bufferSize >= ClearThreshold) {
            free(bufferData);
            bufferData = nullptr;
            bufferReserved = 0;
        }
        bufferSize = 0;
    }

    void reserve(size_t sz)
    {
        if (sz <= bufferReserved)
            return;
        bufferData = static_cast<unsigned char*>(realloc(bufferData, sz));
        if (!bufferData)
            abort();
        bufferReserved = sz;
    }

    void resize(size_t sz)
    {
        if (!sz) {
            clear();
            return;
        }
        if (sz >= bufferSize && sz <= bufferReserved) {
            bufferSize = sz;
            return;
        }
        bufferData = static_cast<unsigned char*>(realloc(bufferData, sz));
        if (!bufferData)
            abort();
        bufferSize = bufferReserved = sz;
    }

    size_t size() const { return bufferSize; }
    size_t capacity() const { return bufferReserved; }

    unsigned char* data() { return bufferData; }
    unsigned char* end() { return bufferData + bufferSize; }
    const unsigned char* data() const { return bufferData; }

    bool load(const String& filename);

private:
    unsigned char* bufferData;
    size_t bufferSize, bufferReserved;

private:
    Buffer(const Buffer& other) = delete;
    Buffer& operator=(const Buffer& other) = delete;
};

class Buffers
{
public:
    Buffers()
        : mBufferOffset(0)
    {}
    void push(Buffer &&buf)
    {
        mBuffers.append(std::forward<Buffer>(buf));
    }
    size_t size() const
    {
        size_t ret = 0;
        for (const auto &buf : mBuffers)
            ret += buf.size();
        return ret - mBufferOffset;
    }
    size_t read(void *outPtr, size_t size)
    {
        if (!size)
            return 0;

        unsigned char *out = static_cast<unsigned char *>(outPtr);
        size_t read = 0, remaining = size;
        while (!mBuffers.empty()) {
            const auto &buf = mBuffers.front();
            const size_t bufferSize = buf.size() - mBufferOffset;

            if (remaining <= bufferSize) {
                memcpy(out + read, buf.data() + mBufferOffset, remaining);
                if (remaining == bufferSize) {
                    mBufferOffset = 0;
                    mBuffers.pop_front();
                } else {
                    mBufferOffset = remaining + mBufferOffset;
                }
                read += remaining;
                break;
            }
            memcpy(out + read, buf.data() + mBufferOffset, bufferSize);
            read += bufferSize;
            mBufferOffset = 0;
            remaining -= bufferSize;
            assert(!mBuffers.isEmpty());
            mBuffers.pop_front();
        }
        return read;
    }
private:
    Buffers(const Buffers &) = delete;
    Buffers &operator=(const Buffers &) = delete;

    LinkedList<Buffer> mBuffers;
    size_t mBufferOffset;
};

#endif
