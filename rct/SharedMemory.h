#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

#include <sys/types.h>

class Path;
class SharedMemory
{
public:
    enum CreateMode { None, Create, Recreate };
    enum AttachFlag { Read = 0x0, Write = 0x1, ReadWrite = Write };

    SharedMemory(key_t key, int size, CreateMode = None);
    SharedMemory(const Path& filename, int size, CreateMode = None);
    ~SharedMemory();

    void* attach(AttachFlag flag, void* address = nullptr);
    void detach();

    bool isValid() const { return mShm != -1; }
    key_t key() const { return mKey; }
    void *address() const { return mAddr; }
    int size() const { return mSize; }

    void cleanup();
private:
    bool init(key_t key, int size, CreateMode mode);

    int mShm;
    bool mOwner;
    void* mAddr;
    key_t mKey;
    int mSize;
};

#endif
