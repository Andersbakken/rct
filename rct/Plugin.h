#ifndef PLUGIN_H
#define PLUGIN_H

#include <rct/Path.h>
#include <assert.h>

namespace Rct {
void* loadPlugin(const Path& fileName);
void  unloadPlugin(void* handle);
void* resolveSymbol(void* handle, const char* symbol);
};

template<typename T>
class Plugin
{
public:
    Plugin() : mHandle(0), mInstance(0) { }
    Plugin(const Path& fileName) : mFileName(fileName), mHandle(0), mInstance(0) { }
    ~Plugin() { clear(); }

    void clear() { if (mHandle) { deleteInstance(); Rct::unloadPlugin(mHandle); mHandle = 0; } }
    void deleteInstance() { delete mInstance; mInstance = 0; }

    void setFileName(const Path& fileName) { clear(); mFileName = fileName; }
    Path fileName() const { return mFileName; }

    T* instance();

private:
    Plugin(const Plugin &);
    Plugin &operator=(const Plugin &);

    Path mFileName;
    void* mHandle;
    T* mInstance;
};

template<typename T>
inline T* Plugin<T>::instance()
{
    if (!mHandle) {
        mHandle = Rct::loadPlugin(mFileName);
        if (!mHandle)
            return 0;
        typedef T *(*CreateInstance)();
        CreateInstance create = reinterpret_cast<CreateInstance>(Rct::resolveSymbol(mHandle, "createInstance"));
        if (create)
            mInstance = create();
        if (!mInstance)
            clear();
    }
    return mInstance;
}

#endif
