#ifndef Filter_h
#define Filter_h

#include "Path.h"
#include "List.h"
#include "String.h"
#include <fnmatch.h>

namespace Filter {
enum Result {
    Filtered,
    File,
    Source,
    Directory
};

static inline Result filter(const Path &path, const List<String> &filters = List<String>())
{
    const int size = filters.size();
    for (int i=0; i<size; ++i) {
        const String &filter = filters.at(i);
        if (!fnmatch(filter.constData(), path.constData(), 0) || path.contains(filter))
            return Filtered;
    }

    if (path.isDir())
        return Directory;

    const char *ext = path.extension();
    if (ext && (Path::isSource(ext) || Path::isHeader(ext)))
        return Source;
    return File;
}
}

#endif
