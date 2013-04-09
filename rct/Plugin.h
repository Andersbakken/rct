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

    void clear() { if (mHandle) { deleteInstance(); Rct::unloadPlugin(mHandle); mHandle = 0; } }
    void deleteInstance() { delete mInstance; mInstance = 0; }

    void setFileName(const Path& fileName) { clear(); mFileName = fileName; }
    Path fileName() const { return mFileName; }

    T* instance();

private:
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
        mInstance = static_cast<T*>(Rct::resolveSymbol(mHandle, "createInstance"));
        if (!mInstance)
            clear();
    }
    return mInstance;
}

#endif
