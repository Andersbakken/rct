#include "demangle.h"

#include <cxxabi.h>

String demangle(const char *mangled)
{
    int status;
    char *result = abi::__cxa_demangle(mangled, 0, 0, &status);
    if (status == 0) {
        String demangled(result);
        free(result);
        return demangled;
    }
    return String();
}
