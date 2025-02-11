#include "demangle.h"

#include <cxxabi.h>

std::string demangle(const char *mangled)
{
    int status;
    char *result = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    if (status == 0) {
        std::string demangled(result);
        free(result);
        return demangled;
    }
    return std::string();
}
