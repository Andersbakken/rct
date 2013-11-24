#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>
#include <assert.h>
#include <rct/String.h>

class Buffer
{
public:
    Buffer()
        : bufferData(0), bufferSize(0), bufferReserved(0)
    {
    }
    Buffer(Buffer&& other)
    {
        bufferData = other.bufferData;
        bufferSize = other.bufferSize;
        bufferReserved = other.bufferReserved;
        other.bufferData = 0;
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
        other.bufferData = 0;
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
            bufferData = 0;
            bufferReserved = 0;
        }
        bufferSize = 0;
    }

    void reserve(unsigned int sz)
    {
        if (sz <= bufferReserved)
            return;
        bufferData = static_cast<unsigned char*>(realloc(bufferData, sz));
        if (!bufferData)
            abort();
        bufferReserved = sz;
    }

    void resize(unsigned int sz)
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

    unsigned int size() const { return bufferSize; }
    unsigned int capacity() const { return bufferReserved; }

    unsigned char* data() { return bufferData; }
    unsigned char* end() { return bufferData + bufferSize; }
    const unsigned char* data() const { return bufferData; }

    bool load(const String& filename);

private:
    unsigned char* bufferData;
    unsigned int bufferSize, bufferReserved;

private:
    Buffer(const Buffer& other) = delete;
    Buffer& operator=(const Buffer& other) = delete;
};

#endif
