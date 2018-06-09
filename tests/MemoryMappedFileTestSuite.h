#include <cppunit/extensions/HelperMacros.h>

class MemoryMappedFileTestSuite : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(MemoryMappedFileTestSuite);
    CPPUNIT_TEST(mapSimpleFile);
    CPPUNIT_TEST(nonExistingFile);
    CPPUNIT_TEST(emptyFile);
    CPPUNIT_TEST(closing);
    CPPUNIT_TEST(moving);
#if !defined(__APPLE__)
    CPPUNIT_TEST(specialChars);
#endif
    CPPUNIT_TEST(dont_lock);
    CPPUNIT_TEST(do_lock);
    CPPUNIT_TEST(writing);
    CPPUNIT_TEST_SUITE_END();

protected:
    void mapSimpleFile();

    void nonExistingFile();

    void emptyFile();

    void closing();

    void moving();
#if !defined(__APPLE__)
    void specialChars();
#endif
    void dont_lock();

    void do_lock();

    void writing();
};

CPPUNIT_TEST_SUITE_REGISTRATION(MemoryMappedFileTestSuite);
