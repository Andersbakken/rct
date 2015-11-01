#include <cppunit/extensions/HelperMacros.h>

class DateTestSuite : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(DateTestSuite);

    CPPUNIT_TEST(testSetTimeUTC);
    CPPUNIT_TEST(testSetTimeLocalTZ);

    CPPUNIT_TEST_SUITE_END();

    public:
        void setUp();
        void tearDown();

    protected:
        void testSetTimeUTC();
        void testSetTimeLocalTZ();

};

CPPUNIT_TEST_SUITE_REGISTRATION(DateTestSuite);

