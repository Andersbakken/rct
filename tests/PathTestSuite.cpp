#include "PathTestSuite.h"

#include <rct/Path.h>
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <functional>

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

template <class F>
class OnScopeExit
{
public:
    OnScopeExit(const F &ff) : f(ff) {}
    ~OnScopeExit() {f();}
private:
    F f;
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

void PathTestSuite::mkdirAndRmdir()
{
    CPPUNIT_ASSERT(Path::mkdir("temp_subdir"));

    // test if dir exists
    FILE *subdir_file = fopen("temp_subdir/file.txt", "w");
    CPPUNIT_ASSERT(subdir_file);
    if(subdir_file) fclose(subdir_file);

    CPPUNIT_ASSERT(Path::rmdir("temp_subdir"));

    // dir should not exist anymore
    subdir_file = fopen("temp_subdir/file.txt", "w");
    CPPUNIT_ASSERT(!subdir_file);


    // try to create recursive path without setting recursive mode
    // (should fail)
    CPPUNIT_ASSERT(!Path::mkdir("temp_subdir2/anotherSubDir"));


    // try to create recursive path *with* setting recursive mode (should work)
    CPPUNIT_ASSERT(Path::mkdir("temp_subdir3/anotherSubDir", Path::Recursive));

    // check if dir was created
    subdir_file = fopen("temp_subdir3/anotherSubDir/file.txt", "w");

    CPPUNIT_ASSERT(subdir_file);

    if(subdir_file) fclose(subdir_file);

    CPPUNIT_ASSERT(Path::rmdir("temp_subdir3"));

    //file should not exist anymore
    subdir_file = fopen("temp_subdir3/anotherSubdir/file.txt", "w");
    CPPUNIT_ASSERT(!subdir_file);


    // try to rm a non-existing directory
    CPPUNIT_ASSERT(!Path::rmdir("thisDirDoesNotExist"));

    // try to create an already-existing dir
    CPPUNIT_ASSERT(Path::mkdir("temp_subdir4")); // should work first time...
    CPPUNIT_ASSERT(Path::mkdir("temp_subdir4")); // ... and still return true the second time

    // creating a sub dir to an already existing dir should work though
    CPPUNIT_ASSERT(Path::mkdir("temp_subdir4/sub"));
    subdir_file = fopen("temp_subdir4/sub/subfile", "w");
    CPPUNIT_ASSERT(subdir_file);
    if(subdir_file) fclose(subdir_file);


    // cleanupn
    CPPUNIT_ASSERT(Path::rmdir("temp_subdir4"));
}

void PathTestSuite::testCanonicalize()
{
    CPPUNIT_ASSERT(Path("dir1/dir2/dir3/../../dir4").canonicalized() == Path("dir1/dir4"));
    CPPUNIT_ASSERT(Path("dir1/dir2/dir3/./../../dir4").canonicalized() == Path("dir1/dir4"));
    CPPUNIT_ASSERT(Path("dir1//dir2/").canonicalized() == Path("dir1/dir2"));
    CPPUNIT_ASSERT(Path("dir1/dir2/dir3/../").canonicalized() == Path("dir1/dir2"));
    CPPUNIT_ASSERT(Path("dir1/dir2/dir3/..").canonicalized() == Path("dir1/dir2"));
    CPPUNIT_ASSERT(Path("dir1/dir2/dir3/.").canonicalized() == Path("dir1/dir2/dir3"));
    CPPUNIT_ASSERT(Path("dir1/dir2/dir3/./").canonicalized() == Path("dir1/dir2/dir3"));
    CPPUNIT_ASSERT(Path("dir1/dir2/./dir3/").canonicalized() == Path("dir1/dir2/dir3"));
    CPPUNIT_ASSERT(Path("dir1/dir2/./dir3").canonicalized() == Path("dir1/dir2/dir3"));
}


#if !defined(__APPLE__)
void PathTestSuite::unicode()
{
    static const char unicodePath[] = u8"Äßéמש最終";
    // we try some funky utf8-stuff
    Path p(unicodePath);
    //OnScopeExit<std::function<void()> > deletePath([&]{Path::rmdir(p);});

    CPPUNIT_ASSERT(p.mkdir());
    CPPUNIT_ASSERT(p.isDir());

    bool changed = false;
    CPPUNIT_ASSERT(p.resolve(Path::RealPath, Path(), &changed));
    CPPUNIT_ASSERT(changed);

    Path parent = p + "/..";
    // check if the file exists
    bool exists = false;

    parent.visit([&](const Path &f_p) -> Path::VisitResult
                 {
                     if(f_p.name() == unicodePath)
                     {
                         exists = true;
                         return Path::Abort;
                     }
                     return Path::Continue;
                 });

    CPPUNIT_ASSERT(exists);

    CPPUNIT_ASSERT(Path::rmdir(p));

}
#endif
