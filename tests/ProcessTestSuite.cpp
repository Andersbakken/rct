#include "ProcessTestSuite.h"

#include <rct/Process.h>

void ProcessTestSuite::setUp() {}

void ProcessTestSuite::tearDown() {}

void ProcessTestSuite::returnCode()
{
    Process p;
    p.exec("ChildProcess");

    // todo: tell the child process to exit a code

    CPPUNIT_ASSERT(p.returnCode() == 12);
}
