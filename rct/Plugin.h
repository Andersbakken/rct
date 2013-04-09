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
    Plugin() { }
    Plugin(const Path& fileName) : mFileName(fileName) { }
    ~Plugin() { clear(); }

    void clear() { if (mHandle) { Rct::unloadPlugin(mHandle); mHandle = mInstance = 0; } }

    void setFileName(const Path& fileName) { clear(); mFileName = fileName; }
    Path fileName() const { return mFileName; }

    T* instance();

private:
    Path mFileName;
    void* mHandle;
    void* mInstance;
};

template<typename T>
inline T* Plugin<T>::instance()
{
    if (!mHandle) {
        mHandle = Rct::loadPlugin(mFileName);
        if (!mHandle)
            return 0;
        mInstance = Rct::resolveSymbol(mHandle, "createInstance");
        if (!mInstance)
            return 0;
    }
    assert(mInstance);
    return reinterpret_cast<T*>(mInstance);
}

#endif
