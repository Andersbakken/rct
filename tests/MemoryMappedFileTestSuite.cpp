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
    CPPUNIT_ASSERT(mmf.filename() == "testfile.txt");

    std::string readData(static_cast<char*>(mmf.filePtr()), mmf.size());
    CPPUNIT_ASSERT(readData == dataToWrite);
}

void MemoryMappedFileTestSuite::nonExistingFile()
{
    MemoryMappedFile mmf1("fileDoesNotExist.txt");
    CPPUNIT_ASSERT(!mmf1.isOpen());
    CPPUNIT_ASSERT(mmf1.size() == 0);
    CPPUNIT_ASSERT(mmf1.filename().isEmpty());
    mmf1.close();  // should be no-op

    MemoryMappedFile mmf2;
    CPPUNIT_ASSERT(!mmf2.isOpen());
    CPPUNIT_ASSERT(mmf2.size() == 0);
    CPPUNIT_ASSERT(!mmf2.open("fileDoesNotExist.txt"));
    CPPUNIT_ASSERT(!mmf2.isOpen());
    CPPUNIT_ASSERT(mmf2.size() == 0);
}

void MemoryMappedFileTestSuite::closing()
{
    // Test whether MemoryMappedFile properly closes an opened file

    const std::string dataToWrite = "some data";
    {
        // create a file
        std::ofstream file("testfile.txt");
        file << dataToWrite << std::flush;
    }

    MemoryMappedFile mmf1("testfile.txt");
    CPPUNIT_ASSERT(mmf1.isOpen());

#ifdef _WIN32   // opening the same file multiple times is allowed on linux.
                // on windows, it's only allowed when it's explictly set up
                // like this in the CreateFile call.
                // Linux/Posix only has advisory, but no real mandatory locks.
    {
        // opening the file for writing should fail because the file is open
        std::ofstream fileWrite("testfile.txt");
        CPPUNIT_ASSERT(!fileWrite);
    }
#endif

    mmf1.close();

    CPPUNIT_ASSERT(!mmf1.isOpen());
    CPPUNIT_ASSERT(mmf1.size() == 0);
    CPPUNIT_ASSERT(mmf1.filename().isEmpty());

#ifdef _WIN32
    {
        // opening the file for writing should work now, because the file
        // should have been closed.
        std::ofstream file("testfile.txt");
        CPPUNIT_ASSERT(file);
    }
#endif
}

void MemoryMappedFileTestSuite::moving()
{
    // try moving an empty file mapping
    MemoryMappedFile mmf1;
    CPPUNIT_ASSERT(!mmf1.isOpen());
    MemoryMappedFile mmf2(std::move(mmf1));
    CPPUNIT_ASSERT(!mmf1.isOpen());
    CPPUNIT_ASSERT(!mmf2.isOpen());



    // try moving an open file mapping
    const std::string dataToWrite = "some data";
    {
        // create a file
        std::ofstream file("testfile.txt");
        file << dataToWrite << std::flush;
    }

    MemoryMappedFile mmf3("testfile.txt");
    CPPUNIT_ASSERT(mmf3.isOpen());
    CPPUNIT_ASSERT(mmf3.size() == dataToWrite.size());
    MemoryMappedFile mmf4;
    CPPUNIT_ASSERT(!mmf4.isOpen());

    mmf4 = std::move(mmf3);

    // mmf4 should now have the data of mmf3
    CPPUNIT_ASSERT(mmf4.isOpen());
    CPPUNIT_ASSERT(mmf4.size() == dataToWrite.size());
    CPPUNIT_ASSERT(mmf4.filename() == "testfile.txt");

    // mmf3 should now be empty.
    CPPUNIT_ASSERT(!mmf3.isOpen());
    CPPUNIT_ASSERT(mmf3.size() == 0);
    CPPUNIT_ASSERT(mmf3.filename().isEmpty());
}
