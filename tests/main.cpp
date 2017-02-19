#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestFailure.h>
#include <cppunit/CompilerOutputter.h>

#include <algorithm>
#include <iostream>

class CustomOutputter : public CppUnit::CompilerOutputter
{
    public:
        CustomOutputter(CppUnit::TestResultCollector* iResults,
                        CppUnit::OStream& iStream,
                        const std::string& iLocationFormat = CPPUNIT_COMPILER_LOCATION_FORMAT)
            : CppUnit::CompilerOutputter(iResults, iStream, iLocationFormat),
              m_result(iResults),
              m_stream(iStream)
        {}

        virtual void write()
        {
            CppUnit::CompilerOutputter::write();

            using namespace CppUnit;
            // use because TestFailures holds TestFailure* instead of Test*;
            // \fixme Find better solution to detect whether the test case failed or not
            std::set<const Test*> failed;
            TestResultCollector::TestFailures::const_iterator fIter = m_result->failures().begin();
            TestResultCollector::TestFailures::const_iterator fEnd = m_result->failures().end();

            for (; fIter != fEnd; ++fIter) {
                failed.insert((*fIter)->failedTest());
            }

            std::string pass = "[32mPASS[0m";
            std::string fail = "[31mFAIL[0m";

            TestResultCollector::Tests::const_iterator iter = m_result->tests().begin();
            TestResultCollector::Tests::const_iterator end = m_result->tests().end();

            std::string testName;
            std::string suiteName;
            std::string caseName;
            std::string lastTestSuiteName;
            for (; iter != end; ++iter) {
                // split test name into suite name and case name
                testName = (*iter)->getName();
                suiteName = testName.substr(0, testName.find("::"));
                caseName = testName.substr(testName.find("::") + 2);

                // if new test suite is observed, print its name
                if (suiteName != lastTestSuiteName) {
                    m_stream << std::endl << suiteName << std::endl;
                    lastTestSuiteName = suiteName;
                }

                m_stream << "[ ";
                if (failed.end() != failed.find(*iter)) {
                    m_stream << fail;
                } else {
                    m_stream << pass;
                }



                m_stream << " ] " << caseName << std::endl;
            }
            m_stream << std::endl;

            /** STATS **/
            int numErrors = m_result->testErrors();
            int numFailures = m_result->testFailures();
            int numFailuresTotal = m_result->testFailuresTotal();
            int numTests = m_result->runTests();

            m_stream << "============================================================" << std::endl;
            m_stream << " STATISTICS" << std::endl;
            m_stream << "============================================================" << std::endl;
            m_stream << "Number of errors (uncaught exception):  " << numErrors << std::endl;
            m_stream << "Number of failures (failed assertions): " << numFailures << std::endl;
            m_stream << "Total number of detected failures:      " << numFailuresTotal << std::endl;
            m_stream << "Total number of tests:                  " << numTests << std::endl;
            m_stream << "============================================================" << std::endl;
        }

    private:
        CppUnit::TestResultCollector* m_result;
        CppUnit::OStream& m_stream;

};


int main()
{
    CppUnit::TestRunner runner;

    CppUnit::TestFactoryRegistry &registry = CppUnit::TestFactoryRegistry::getRegistry();

    runner.addTest( registry.makeTest() );

    CppUnit::TestResult controller;
    CppUnit::TestResultCollector result;
    controller.addListener(&result);

    runner.run(controller);

    CustomOutputter outputter(&result, std::cerr);

    outputter.write();

    return result.wasSuccessful() ? 0 : 1;
}

