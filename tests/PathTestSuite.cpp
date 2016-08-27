#include "PathTestSuite.h"

#include <rct/Path.h>
#include <stdlib.h>
#include <iostream>

#ifdef _WIN32
#  include <Windows.h>
#endif

void PathTestSuite::setUp() {}

void PathTestSuite::tearDown() {}

#ifdef _WIN32

class CwdChanger
{
public:
    CwdChanger(const char *newCwd)
    {
        getcwd(&oldCwd[0], _MAX_PATH);
        chdir(newCwd);
    }

    ~CwdChanger() { chdir(oldCwd); }
private:
    char oldCwd[_MAX_PATH];
};

void PathTestSuite::testPathConstructionWindows()
{
    {
        Path p;
        CPPUNIT_ASSERT_EQUAL(p.size(), (size_t)0);
        CPPUNIT_ASSERT(!p.isAbsolute());
    }

    {
        Path p("file.txt");
        CPPUNIT_ASSERT(p == "file.txt");
        CPPUNIT_ASSERT(!p.isAbsolute());
    }

    {
        Path p("C:\\temp\\test.doc");
        CPPUNIT_ASSERT(p == "C:/temp/test.doc");
        CPPUNIT_ASSERT(p.isAbsolute());
    }

    {
        Path p("\\\\127.0.0.1\\share");
        CPPUNIT_ASSERT(p == "\\\\127.0.0.1/share");
        CPPUNIT_ASSERT(p.isAbsolute());
    }
}

void PathTestSuite::testPathStatusWindows()
{
    // This test checks file exists, check if path is dir or regular file, etc.

    {
        Path p("C:\\windows");
        CPPUNIT_ASSERT(p.exists());
        CPPUNIT_ASSERT(p.isDir());
    }

    {
        Path p("this_file_does_not_exist.not_there");
        CPPUNIT_ASSERT(!p.exists());
    }
}

void PathTestSuite::testRelativeToAbsPath_windows()
{
    CwdChanger toSys32("C:\\windows\\system32");

    Path notepad("notepad.exe");

    CPPUNIT_ASSERT(notepad.exists());
    CPPUNIT_ASSERT(notepad.isFile());

    CPPUNIT_ASSERT(!notepad.isAbsolute());
    bool changed;
    CPPUNIT_ASSERT(notepad.resolve(Path::RealPath, Path(), &changed));
    CPPUNIT_ASSERT(changed);
    CPPUNIT_ASSERT(notepad == "C:/windows/system32/notepad.exe");
    CPPUNIT_ASSERT(notepad.parentDir() == "C:/windows/system32/");
}

#endif  // _WIN32
