#ifndef STRINGTOKENIZERTESTS_H
#define STRINGTOKENIZERTESTS_H

#include <cppunit/extensions/HelperMacros.h>

class StringTokenizerTestSuite : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(StringTokenizerTestSuite);

    CPPUNIT_TEST(breakIdentifierWithUnderscore);
    CPPUNIT_TEST(breakIdentifierWithCamelCase);
    CPPUNIT_TEST(breakIdentifierWithUpperLetters);
    CPPUNIT_TEST(breakIdentifierWithDigits);
    CPPUNIT_TEST(breakIdentifierWithDigitsAtBeginning);
    CPPUNIT_TEST(breakVeryComplexIdentifier);
    CPPUNIT_TEST(breakVeryComplexIdentifierWithUnderscore);
    CPPUNIT_TEST(matchSimpleSearchPattern);
    CPPUNIT_TEST(commonPrefix);
    CPPUNIT_TEST(matchSimpleSearchPattern2);
    CPPUNIT_TEST(matchSimpleSearchPatternSkipChunks);
    CPPUNIT_TEST(matchSimpleSearchPatternInvalid);
    CPPUNIT_TEST(matchSimpleSearchPatternWithInvalidCandidateChars);
    CPPUNIT_TEST(matchSimpleSearchPatternComplex);
    CPPUNIT_TEST(matchSimpleSearchPatternComplex2);
    CPPUNIT_TEST(findMatchInvalid);
    CPPUNIT_TEST(findMatchInvalidSmaller);
    CPPUNIT_TEST(findMatchExactCaseSensitive);
    CPPUNIT_TEST(findMatchExactCaseInsensitive);
    CPPUNIT_TEST(findMatchPrefixCaseSensitive);
    CPPUNIT_TEST(findMatchPrefixCaseInsensitive);
    CPPUNIT_TEST(findMatchWordBoundary);
    CPPUNIT_TEST(findAndSortResultsSimple);
    CPPUNIT_TEST(findAndSortResultsSimpleMultiple);
    CPPUNIT_TEST(findAndSortResultsCaseSensitivity);
    CPPUNIT_TEST(findAndSortResultsCaseMixture);
    CPPUNIT_TEST(findAndSortResultsLongerPrefixAtBeginning);

    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

protected:
    void breakIdentifierWithUnderscore();
    void breakIdentifierWithCamelCase();
    void breakIdentifierWithUpperLetters();
    void breakIdentifierWithDigits();
    void breakIdentifierWithDigitsAtBeginning();
    void breakVeryComplexIdentifier();
    void breakVeryComplexIdentifierWithUnderscore();
    void matchSimpleSearchPattern();
    void commonPrefix();
    void matchSimpleSearchPattern2();
    void matchSimpleSearchPatternSkipChunks();
    void matchSimpleSearchPatternInvalid();
    void matchSimpleSearchPatternWithInvalidCandidateChars();
    void matchSimpleSearchPatternComplex();
    void matchSimpleSearchPatternComplex2();
    void findMatchInvalid();
    void findMatchInvalidSmaller();
    void findMatchExactCaseSensitive();
    void findMatchExactCaseInsensitive();
    void findMatchPrefixCaseSensitive();
    void findMatchPrefixCaseInsensitive();
    void findMatchWordBoundary();
    void findAndSortResultsSimple();
    void findAndSortResultsSimpleMultiple();
    void findAndSortResultsCaseSensitivity();
    void findAndSortResultsCaseMixture();
    void findAndSortResultsLongerPrefixAtBeginning();
};

CPPUNIT_TEST_SUITE_REGISTRATION(StringTokenizerTestSuite);


#endif /* STRINGTOKENIZERTESTS_H */
