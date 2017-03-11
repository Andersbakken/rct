#include <cppunit/extensions/HelperMacros.h>

#include <string>

class SHA256TestSuite : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(SHA256TestSuite);
    CPPUNIT_TEST(nonExistingFile);
    CPPUNIT_TEST(someFiles);
    CPPUNIT_TEST_SUITE_END();

protected:
    void someFiles();
    void nonExistingFile();

private:
    void createFile(const std::string &filename,
                    const std::string &content);
};

CPPUNIT_TEST_SUITE_REGISTRATION(SHA256TestSuite);
