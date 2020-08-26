#ifndef MemoryMappedFile_h
#define MemoryMappedFile_h

#include <iosfwd>

#include "Path.h"
#include "rct/Path.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#else
#  include <sys/types.h>
#endif

/**
 * Allows to map a file to memory and read/write the file
 * through a standard pointer.
 *
 * This class only allows mapping a whole file to memory.
 * Mapping sections of a file is not supported (yet).
 */
class MemoryMappedFile
{
public:
    enum AccessType
    {
        NO_ACCESS,
        READ_ONLY,
        READ_WRITE
    };

    enum LockType
    {
        /**
         * Lock the file upon opening.
         *
         * On unix-like systems, this does not prevent anyone else from
         * opening, reading or writing the file.
         * The lock is more like a mutex.
         * Trying to lock the file again will either block or lead
         * to an error.
         *
         * On unix-like systems, opening the same file twice using this class
         * with the DO_LOCK flag will result in an error on the second time.
         * The call to open() (or the constructor) will not block.
         *
         * On Windows, opening the file with DO_LOCK will prevent other
         * processes from opening the file.
         */
        DO_LOCK,

        /**
         * Don't set up a lock on the file.
         *
         * On windows, opening a file that is already opened by someone else
         * requires an appropriate dwShareMode flag in the call to CreateFile
         * (see documentation on msdn), otherwise opening will fail.
         *
         * That means that opening a MemoryMappedFile again through the
         * standard library (such as fopen()) will fail even if the
         * MemoryMappedFile is opened with the DONT_LOCK flag.
         */
        DONT_LOCK
    };

public:  // ctors + dtors

    MemoryMappedFile();

    MemoryMappedFile(MemoryMappedFile &&other);

    /**
     * @param lock See documentation for LockType.
     *             When you pass DO_LOCK and there already is a lock, this
     *             constructor will fail. The ctor will *not* block until
     *             the lock is released.
     */
    MemoryMappedFile(const Path &filename, AccessType access=READ_ONLY,
                     LockType lock=DONT_LOCK);

    /**
     * Destructor.
     *
     * Closes the associated file.
     */
    ~MemoryMappedFile();

public:  // operators

    MemoryMappedFile &operator=(MemoryMappedFile &&);

public:  // methods

    /**
     * If this object has already opened a file, it will be closed before
     * opening the new file.
     *
     * @param lock See documentation for LockType.
     *             When you pass DO_LOCK and there already is a lock, this
     *             constructor will fail. This method will *not* block until
     *             the lock is released.
     */
    bool open(const Path &filename, AccessType access=READ_ONLY,
              LockType lock=DONT_LOCK);

    /**
     * Closes the file mapping.
     * Calling this method when no file is mapped is a no-op.
     */
    void close();

    bool isOpen() const {return mAccessType != NO_ACCESS;}

    AccessType accessType() const {return mAccessType;}

    /**
     * The size of the mapped portion of the file.
     * If no file is mapped, this returns 0.
     */
    std::size_t size() const {return mFileSize;}

    /**
     * Get a typed pointer to the file's memory region.
     * Bounds checking is not performed, so there is no error if
     * sizeof(T) > size().
     * @return nullptr if no file is mapped, nullptr if the file is empty.
     */
    template<class T=void>
    T *filePtr() {return reinterpret_cast<T*>(mpMapped);}

    /**
     * Get a typed pointer to the file's memory region.
     * Bounds checking is not performed, so there is no error if
     * sizeof(T) > size().
     * @return nullptr if no file is mapped, nullptr if the file is empty.
     */
    template<class T=void>
    const T *filePtr() const {return reinterpret_cast<T*>(mpMapped);}

    /**
     * @return Empty string if no file is mapped.
     */
    const Path filename() const {return mFilename;}

private:
    void *mpMapped;
    Path mFilename;
    AccessType mAccessType;
#ifdef _WIN32
    HANDLE mhFile;
    HANDLE mhFileMapping;
    DWORD mFileSize;

    static void closeHandleIfValid(HANDLE &hdl);
#else
    int mFd;   ///< Descriptor for the file
    off_t mFileSize;
#endif
};

#endif
