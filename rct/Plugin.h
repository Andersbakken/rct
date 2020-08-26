#ifndef PLUGIN_H
#define PLUGIN_H

#include <assert.h>
#include <rct/Path.h>
#include <rct/String.h>

namespace Rct {
void* loadPlugin(const Path& fileName);
void unloadPlugin(void* handle);
void* resolveSymbol(void* handle, const char* symbol);
char* pluginError();
}

template<typename T>
class Plugin
{
public:
    Plugin() : mHandle(nullptr), mInstance(0) { }
    Plugin(const Path& fileName) : mFileName(fileName), mHandle(nullptr), mInstance(nullptr) { }
    ~Plugin() { clear(); }

    void clear() { if (mHandle) { deleteInstance(); Rct::unloadPlugin(mHandle); mHandle = nullptr; } }
    void deleteInstance() { delete mInstance; mInstance = nullptr; }

    void setFileName(const Path& fileName) { clear(); mFileName = fileName; }
    Path fileName() const { return mFileName; }

    T* instance();

    String error() const { return mError; }

private:
    Plugin(const Plugin &);
    Plugin &operator=(const Plugin &);

    String mError;
    Path mFileName;
    void* mHandle;
    T* mInstance;
};

template<typename T>
inline T* Plugin<T>::instance()
{
    if (!mHandle) {
        mHandle = Rct::loadPlugin(mFileName);
        if (!mHandle) {
            mError = Rct::pluginError();
            return 0;
        }
        typedef T *(*CreateInstance)();
        CreateInstance create = reinterpret_cast<CreateInstance>(Rct::resolveSymbol(mHandle, "createInstance"));
        if (create)
            mInstance = create();
        if (!mInstance) {
            mError = Rct::pluginError();
            clear();
        }
    }
    return mInstance;
}

#endif
