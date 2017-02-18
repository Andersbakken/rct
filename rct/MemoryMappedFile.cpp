#include "MemoryMappedFile.h"

#ifdef _WIN32
#  include <Windows.h>
#else

#endif

MemoryMappedFile::MemoryMappedFile()
    : mpMapped(nullptr),
#ifdef _WIN32
      mhFile(INVALID_HANDLE_VALUE), mhFileMapping(INVALID_HANDLE_VALUE),
      mFileSize(0)
#else
#endif
{

}

MemoryMappedFile::MemoryMappedFile(const Path &f_file)
    : mpMapped(nullptr),
#ifdef _WIN32
      mhFile(INVALID_HANDLE_VALUE), mhFileMapping(INVALID_HANDLE_VALUE),
      mFileSize(0)
#else
#endif
{
    open(f_file);
}

MemoryMappedFile::~MemoryMappedFile()
{
    close();
}

std::size_t MemoryMappedFile::size() const
{
#ifdef _WIN32
    return mFileSize;
#else
#endif
}

bool MemoryMappedFile::open(const Path &f_filename)
{
    if(isOpen()) close();
#ifdef _WIN32
    return false;
#else
    return false;
#endif
}

void MemoryMappedFile::close()
{
    if(!isOpen()) return;

    // TODO
}
