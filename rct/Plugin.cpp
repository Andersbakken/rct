#include "Plugin.h"

#include "rct/Path.h"

#ifdef _WIN32
// TODO: Imlement on windows
#else
#include <dlfcn.h>

namespace Rct {

void* loadPlugin(const Path& fileName)
{
    if (!fileName.isFile())
        return nullptr;
    return dlopen(fileName.nullTerminated(), RTLD_LAZY);
}

void unloadPlugin(void* handle)
{
    if (handle)
        dlclose(handle);
}

void* resolveSymbol(void* handle, const char* symbol)
{
    return dlsym(handle, symbol);
}

char* pluginError()
{
    return dlerror();
}

} // namespace RctPlugin

#endif
