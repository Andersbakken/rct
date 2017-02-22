#include "Path.h"

#ifdef _WIN32
#  include <Windows.h>
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
        READ_ONLY,
        READ_WRITE
    };

    enum LockType
    {
        DO_LOCK,
        DONT_LOCK
    };

public:  // ctors + dtors

    MemoryMappedFile();

    MemoryMappedFile(MemoryMappedFile &&other);

    MemoryMappedFile(const Path &filename, AccessType access=READ_ONLY,
                     LockType lock=DONT_LOCK);

    ~MemoryMappedFile();

public:  // operators

    MemoryMappedFile &operator=(MemoryMappedFile &&);

public:  // methods

    bool open(const Path &filename, AccessType access=READ_ONLY,
              LockType lock=DONT_LOCK);

    /**
     * Closes the file mapping.
     * Calling this method when no file is mapped is a no-op.
     */
    void close();

    bool isOpen() const {return mpMapped;}

    /**
     * The size of the mapped portion of the file.
     * If no file is mapped, this returns 0.
     */
    std::size_t size() const;

    /**
     * @return nullptr if no file is mapped.
     */
    void *filePtr() {return mpMapped;}

    /**
     * @return nullptr if no file is mapped.
     */
    const void *filePtr() const {return mpMapped;}

    /**
     * @return Empty string if no file is mapped.
     */
    const Path filename() const {return mFilename;}

private:
    void *mpMapped;
    Path mFilename;
#ifdef _WIN32
    HANDLE mhFile;
    HANDLE mhFileMapping;
    DWORD mFileSize;

    static void closeHandleIfValid(HANDLE &hdl);
#else

#endif
};
