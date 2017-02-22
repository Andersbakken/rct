#include <cppunit/extensions/HelperMacros.h>

class MemoryMappedFileTestSuite : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(MemoryMappedFileTestSuite);
    CPPUNIT_TEST(mapSimpleFile);
    CPPUNIT_TEST(nonExistingFile);
    CPPUNIT_TEST(closing);
    CPPUNIT_TEST(moving);
    CPPUNIT_TEST_SUITE_END();

protected:
    void mapSimpleFile();

    void nonExistingFile();

    void closing();

    void moving();
};

CPPUNIT_TEST_SUITE_REGISTRATION(MemoryMappedFileTestSuite);
