#include "MemoryMappedFile.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <utility>

#include "Log.h"

#ifndef _WIN32
#  include <sys/stat.h>
#  include <sys/file.h>
#  include <unistd.h>

#  include "Rct.h"
#endif

MemoryMappedFile::MemoryMappedFile()
    : mpMapped(nullptr), mAccessType(NO_ACCESS),
#ifdef _WIN32
      mhFile(INVALID_HANDLE_VALUE), mhFileMapping(INVALID_HANDLE_VALUE),
      mFileSize(0)
#else
      mFd(-1), mFileSize(0)
#endif
{
}

MemoryMappedFile::MemoryMappedFile(const Path &f_file, AccessType f_access,
                                   LockType f_lock)
    : mpMapped(nullptr), mAccessType(NO_ACCESS),
#ifdef _WIN32
      mhFile(INVALID_HANDLE_VALUE), mhFileMapping(INVALID_HANDLE_VALUE),
      mFileSize(0)
#else
      mFd(-1), mFileSize(0)
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

    mAccessType = f_other.mAccessType;
    f_other.mAccessType = NO_ACCESS;

#ifdef _WIN32
    mhFile = f_other.mhFile;
    f_other.mhFile = INVALID_HANDLE_VALUE;

    mhFileMapping = f_other.mhFileMapping;
    f_other.mhFileMapping = INVALID_HANDLE_VALUE;

    mFileSize = f_other.mFileSize;
    f_other.mFileSize = 0;
#else
    mFd = f_other.mFd;
    f_other.mFd = -1;

    mFileSize = f_other.mFileSize;
    f_other.mFileSize = 0;
#endif

    return *this;
}

MemoryMappedFile::~MemoryMappedFile()
{
    close();
}

bool MemoryMappedFile::open(const Path &f_filename, AccessType f_access,
                            LockType f_lock)
{
    if(isOpen()) close();

    if(f_access == NO_ACCESS)
    {
        error() << "Can't create MemoryMappedFile with access type NO_ACCESS";
        return false;
    }

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
    mhFile = CreateFileW(Utf8To16(f_filename.nullTerminated()),
                         access,
                         share,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);

    if(mhFile == INVALID_HANDLE_VALUE)
    {
        const DWORD errorCode = GetLastError();

        auto errStream = error() << "Can't open file " << f_filename;
        switch(errorCode)
        {
        case ERROR_FILE_NOT_FOUND:
            errStream << " (file not found)";
            break;
        case ERROR_SHARING_VIOLATION:
            errStream << " (file is locked)";
            break;
        default:
            errStream << "GetLastError(): " << errorCode;
        }

        return false;
    }

    mFileSize = GetFileSize(mhFile, NULL);

    if(mFileSize == 0)
    {
        // can't map empty file. We still report it as success (with size() == 0)
        mFilename = f_filename;
        mAccessType = f_access;
        mpMapped = nullptr;
        return true;
    }

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

#else
    // try to open the file
    const int openFlags = (f_access == READ_ONLY) ?
    O_RDONLY : O_RDWR;
    const int protFlags = (f_access == READ_ONLY) ?
    PROT_READ : (PROT_READ | PROT_WRITE);

    mFd = ::open(f_filename.nullTerminated(), openFlags);

    if(mFd == -1)
    {
        auto errStream = error() << "Could not open file " << f_filename;
        switch(errno)
        {
        case ENOENT:
            errStream << " (file does not exist)";
            break;
        default:
            errStream << ". errno=" << errno;
            break;
        }
        return false;
    }

    if(f_lock == DO_LOCK)
    {
        int lockRes;
        eintrwrap(lockRes, flock(mFd, LOCK_EX | LOCK_NB));

        if(lockRes == -1)
        {
            auto errStream = error() << "Could not lock file " << f_filename;
            switch(errno)
            {
            case EWOULDBLOCK:
                errStream << " (file is already locked)";
                break;
            default:
                errStream << ". errno=" << errno;
                break;
            }
            close();
            return false;
        }
    }

    // get file size
    struct stat st;
    if(fstat(mFd, &st) != 0)
    {
        error() << "Could not stat file " << f_filename
                << ". errno=" << errno;
        close();
        return false;
    }

    mFileSize = st.st_size;   // size in byte

    if(mFileSize == 0)
    {
        // can't map empty file. We still report it as success (with size() == 0)
        mFilename = f_filename;
        mAccessType = f_access;
        mpMapped = nullptr;
        return true;
    }

    // now, we can actually map the file
    mpMapped = mmap(nullptr,       // destination hint
                    mFileSize,
                    protFlags,  // mmu page protection
                    MAP_SHARED,
                    mFd,
                    0);           // offset


    if(mpMapped == MAP_FAILED)
    {
        mpMapped = nullptr;
        close();
        error() << "Could not map file " << f_filename
                << ". errno=" << errno;
        return false;
    }

#endif

    // everything worked out! We're done.
    mFilename = f_filename;
    mAccessType = f_access;
    return true;
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

#else  // ifdef _WIN32
    if(mpMapped != nullptr && munmap(mpMapped, mFileSize) != 0)
    {
        error() << "Could not unmap " << mFilename
                << ". errno=" << errno;
    }

    if(mFd != -1)
    {
        int ret;
        eintrwrap(ret, ::close(mFd));

        if(ret == -1)
        {
            error() << "Could not close file " << mFilename
                    << ". errno=" << errno;
        }

        mFd = -1;
    }

#endif

    mFileSize = 0;
    mFilename.clear();
    mpMapped = nullptr;
    mAccessType = NO_ACCESS;
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
