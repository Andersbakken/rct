#include <cppunit/extensions/HelperMacros.h>

class PathTestSuite : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(PathTestSuite);

#ifdef _WIN32
    CPPUNIT_TEST(testPathConstructionWindows);
    CPPUNIT_TEST(testPathStatusWindows);
    CPPUNIT_TEST(testRelativeToAbsPath_windows);
#endif
    CPPUNIT_TEST(mkdirAndRmdir);
    CPPUNIT_TEST(testCanonicalize);
#if !defined(__APPLE__)
    CPPUNIT_TEST(unicode);
#endif
    CPPUNIT_TEST_SUITE_END();

    public:
        void setUp();
        void tearDown();

protected:
#ifdef _WIN32
    void testPathConstructionWindows();
    void testPathStatusWindows();
    void testRelativeToAbsPath_windows();
#endif
    void mkdirAndRmdir();
    void testCanonicalize();
#if !defined(__APPLE__)
    void unicode();
#endif
};

CPPUNIT_TEST_SUITE_REGISTRATION(PathTestSuite);
