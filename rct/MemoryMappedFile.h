#include "Path.h"

#ifdef _WIN32
#  include <Windows.h>
#endif

/**
 * Allows to map a file to memory and read/write the file
 * through a standard pointer.
 */
class MemoryMappedFile
{
public:  // ctors + dtors

    MemoryMappedFile();

    MemoryMappedFile(MemoryMappedFile &&) = default;

    MemoryMappedFile(const Path &filename);

    ~MemoryMappedFile();

public:  // operators

    MemoryMappedFile &operator=(MemoryMappedFile &&) = default;

public:  // methods

    bool open(const Path &filename);

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
#else

#endif
};
