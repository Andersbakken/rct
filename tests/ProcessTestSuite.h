#include <cppunit/extensions/HelperMacros.h>

class ProcessTestSuite : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(ProcessTestSuite);

    CPPUNIT_TEST(returnCode);

    CPPUNIT_TEST_SUITE_END();

    public:
        void setUp();
        void tearDown();

protected:
    // start a process and examine its return code
    void returnCode();
};

CPPUNIT_TEST_SUITE_REGISTRATION(ProcessTestSuite);
