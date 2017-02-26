#include <cppunit/extensions/HelperMacros.h>

class MemoryMappedFileTestSuite : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(MemoryMappedFileTestSuite);
    CPPUNIT_TEST(mapSimpleFile);
    CPPUNIT_TEST(nonExistingFile);
    CPPUNIT_TEST(closing);
    CPPUNIT_TEST(moving);
    CPPUNIT_TEST(specialChars);
    CPPUNIT_TEST(dont_lock);
    CPPUNIT_TEST(do_lock);
    CPPUNIT_TEST(writing);
    CPPUNIT_TEST_SUITE_END();

protected:
    void mapSimpleFile();

    void nonExistingFile();

    void closing();

    void moving();

    void specialChars();

    void dont_lock();

    void do_lock();

    void writing();
};

CPPUNIT_TEST_SUITE_REGISTRATION(MemoryMappedFileTestSuite);
