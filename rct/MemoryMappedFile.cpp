#include "MemoryMappedFile.h"

#include "Log.h"

#ifdef _WIN32
#  include <Windows.h>
#else

#endif

MemoryMappedFile::MemoryMappedFile()
    : mpMapped(nullptr)
#ifdef _WIN32
    ,mhFile(INVALID_HANDLE_VALUE), mhFileMapping(INVALID_HANDLE_VALUE),
      mFileSize(0)
#else
#endif
{

}

MemoryMappedFile::MemoryMappedFile(const Path &f_file, AccessType f_access,
                                   LockType f_lock)
    : mpMapped(nullptr)
#ifdef _WIN32
    , mhFile(INVALID_HANDLE_VALUE), mhFileMapping(INVALID_HANDLE_VALUE),
      mFileSize(0)
#else
#endif
{
    open(f_file, f_access, f_lock);
}

MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& f_other)
{
    *this = std::move(f_other);
}

MemoryMappedFile &MemoryMappedFile::operator=(MemoryMappedFile &&f_other)
{
    mpMapped = f_other.mpMapped;
    f_other.mpMapped = nullptr;

    mFilename = f_other.mFilename;
    f_other.mFilename.clear();

#ifdef _WIN32
    mhFile = f_other.mhFile;
    f_other.mhFile = INVALID_HANDLE_VALUE;

    mhFileMapping = f_other.mhFileMapping;
    f_other.mhFileMapping = INVALID_HANDLE_VALUE;

    mFileSize = f_other.mFileSize;
    f_other.mFileSize = 0;
#else

#endif

    return *this;
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

bool MemoryMappedFile::open(const Path &f_filename, AccessType f_access,
                            LockType f_lock)
{
    if(isOpen()) close();
#ifdef _WIN32

    const DWORD access = (f_access == READ_ONLY) ?
        GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);
    const DWORD share = (f_lock == DO_LOCK) ?
        0 : (FILE_SHARE_READ | FILE_SHARE_WRITE);
    const DWORD protect = (f_access == READ_ONLY) ?
        PAGE_READONLY : PAGE_READWRITE;
    const DWORD desiredAccess = (f_access == READ_ONLY) ?
        FILE_MAP_READ : FILE_MAP_WRITE; // FILE_MAP_WRITE includes read access

    // first, we need to open the file:
    mhFile = CreateFile(f_filename.nullTerminated(),
                        access,
                        share,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

    if(mhFile == INVALID_HANDLE_VALUE)
    {
        error() << "Can't open file " << f_filename << ". "
                << "GetLastError(): " << GetLastError();
        return false;
    }

    mFileSize = GetFileSize(mhFile, NULL);

    // now, we can set up a file mapping:
    mhFileMapping = CreateFileMapping(mhFile,   // file to map
                                      NULL,     // security attrs
                                      protect,
                                      0, 0,     // use full file size
                                      NULL);    // name

    if(mhFileMapping == NULL)
    {
        error() << "Can't map file " << f_filename << ". "
                << "GetLastError(): " << GetLastError();
        close();
        return false;
    }

    // now we need to open a so-called "view" into the file mapping:
    mpMapped = MapViewOfFile(mhFileMapping, desiredAccess,
                             0,0,  // offset high and low
                             0);   // size


    if(mpMapped == NULL)
    {
        error() << "Can't map view of file " << f_filename << ". "
                << "GetLastError(): " << GetLastError();
        close();
        return false;
    }

    // everything worked out! We're done.
    mFilename = f_filename;
    return true;

#else
    return false;
#endif
}

void MemoryMappedFile::close()
{
#ifdef _WIN32
    if(mpMapped && !UnmapViewOfFile(mpMapped))
    {
        error() << "Could not UnmapViewOfFile(). GetLastError()="
                << GetLastError();
    }
    closeHandleIfValid(mhFileMapping);
    closeHandleIfValid(mhFile);
    mFileSize = 0;
    mFilename.clear();
    mpMapped = nullptr;
#else
#endif
}

#ifdef _WIN32
/* static */ void MemoryMappedFile::closeHandleIfValid(HANDLE &f_hdl)
{
    if(f_hdl == INVALID_HANDLE_VALUE) return;

    if(!CloseHandle(f_hdl))
    {
        error() << "Could not close handle! GetLastError()=" << GetLastError();
    }
    f_hdl = INVALID_HANDLE_VALUE;
}
#endif
