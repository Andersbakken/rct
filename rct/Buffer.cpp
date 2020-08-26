#include "Buffer.h"

#include <stdio.h>

#include "rct/String.h"

bool Buffer::load(const String& filename)
{
    clear();

    FILE* f = fopen(filename.constData(), "r");
    if (!f)
        return false;

    fseek(f, 0, SEEK_END);
    long int size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size == -1) {
        fclose(f);
        return false;
    }

    resize(size);
    assert(size >= 0);
    assert(bufferSize >= static_cast<unsigned long int>(size));

    if (fread(bufferData, 1, size, f) < 1) {
        fclose(f);
        clear();
        return false;
    }

    fclose(f);
    return true;
}
