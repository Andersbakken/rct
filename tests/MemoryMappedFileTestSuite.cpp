#include "MemoryMappedFileTestSuite.h"

#include <rct/MemoryMappedFile.h>

#include <fstream>

void MemoryMappedFileTestSuite::mapSimpleFile()
{
    const std::string dataToWrite = "Hello world";
    {
        // create a file
        std::ofstream file("testfile.txt");
        file << dataToWrite;
    }

    MemoryMappedFile mmf("testfile.txt");

    CPPUNIT_ASSERT(mmf.isOpen());
    CPPUNIT_ASSERT(mmf.size() == dataToWrite.size());

    std::string readData(static_cast<char*>(mmf.filePtr()), mmf.size());
    CPPUNIT_ASSERT(readData == dataToWrite);
}

void MemoryMappedFileTestSuite::nonExistingFile()
{
    MemoryMappedFile mmf1("fileDoesNotExist.txt");
    CPPUNIT_ASSERT(!mmf1.isOpen());

    MemoryMappedFile mmf2;
    CPPUNIT_ASSERT(!mmf2.isOpen());
    CPPUNIT_ASSERT(!mmf2.open("fileDoesNotExist.txt"));
    CPPUNIT_ASSERT(!mmf2.isOpen());
}
