#include "MemoryMappedFileTestSuite.h"

#include <rct/MemoryMappedFile.h>

#include <fstream>

void MemoryMappedFileTestSuite::mapSimpleFile()
{
    {
        // create a file
        std::ofstream file("testfile.txt");
        file << "Hello world\n";
    }

    MemoryMappedFile mmf("testfile.txt");

    CPPUNIT_ASSERT(mmf.isOpen());
}

void MemoryMappedFileTestSuite::nonExistingFile()
{
    CPPUNIT_ASSERT(false);  // todo
}
