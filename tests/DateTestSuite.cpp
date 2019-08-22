#include <DateTestSuite.h>
#include <rct/Date.h>

void
DateTestSuite::setUp()
{
}

void
DateTestSuite::tearDown()
{
}

void
DateTestSuite::testSetTimeUTC()
{
    // prepare
    Date d;

    CPPUNIT_ASSERT_EQUAL(static_cast<time_t>(0), d.time());

    const time_t now = time(nullptr);

    // execute
    d.setTime(now, Date::Mode::UTC);

    // verify
    CPPUNIT_ASSERT_EQUAL(now, d.time());
}

void
DateTestSuite::testSetTimeLocalTZ()
{
    // prepare
    Date d;

    CPPUNIT_ASSERT_EQUAL(static_cast<time_t>(0), d.time());

    const time_t now = time(nullptr);

    // execute
    d.setTime(now, Date::Mode::Local);

    // verify
    struct tm ltime;
    localtime_r(&now, &ltime);
    CPPUNIT_ASSERT_EQUAL(now + ltime.tm_gmtoff, d.time());
}
