#include <rct/StringTokenizer.h>
#include <rct/String.h>
#include "StringTokenizerTestSuite.h"

static inline std::vector<std::string> toVector(List<String> &&in)
{
    std::vector<std::string> ret;
    ret.reserve(in.size());
    for (String &str : in) {
        ret.push_back(std::move(str));
    }
    return ret;
}

void StringTokenizerTestSuite::breakIdentifierWithUnderscore()
{
    std::vector<std::string> result = toVector(StringTokenizer::break_parts_of_word("my_shiny_identifier"));

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), result.size());
    CPPUNIT_ASSERT_EQUAL(std::string("my"), result[0]);
    CPPUNIT_ASSERT_EQUAL(std::string("shiny"), result[1]);
    CPPUNIT_ASSERT_EQUAL(std::string("identifier"), result[2]);
}

void StringTokenizerTestSuite::breakIdentifierWithCamelCase()
{
    std::vector<std::string> result = toVector(StringTokenizer::break_parts_of_word("MyShinyIdentifier"));

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), result.size());
    CPPUNIT_ASSERT_EQUAL(std::string("my"), result[0]);
    CPPUNIT_ASSERT_EQUAL(std::string("shiny"), result[1]);
    CPPUNIT_ASSERT_EQUAL(std::string("identifier"), result[2]);
}

void StringTokenizerTestSuite::breakIdentifierWithUpperLetters()
{
    std::vector<std::string> result = toVector(StringTokenizer::break_parts_of_word("MyShinyXYZIdentifier"));

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), result.size());
    CPPUNIT_ASSERT_EQUAL(std::string("my"), result[0]);
    CPPUNIT_ASSERT_EQUAL(std::string("shiny"), result[1]);
    CPPUNIT_ASSERT_EQUAL(std::string("xyz"), result[2]);
    CPPUNIT_ASSERT_EQUAL(std::string("identifier"), result[3]);
}

void StringTokenizerTestSuite::breakIdentifierWithDigits()
{
    std::vector<std::string> result = toVector(StringTokenizer::break_parts_of_word("foo12345bar"));

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), result.size());
    CPPUNIT_ASSERT_EQUAL(std::string("foo"), result[0]);
    CPPUNIT_ASSERT_EQUAL(std::string("12345"), result[1]);
    CPPUNIT_ASSERT_EQUAL(std::string("bar"), result[2]);
}

void StringTokenizerTestSuite::breakIdentifierWithDigitsAtBeginning()
{
    std::vector<std::string> result = toVector(StringTokenizer::break_parts_of_word("12345FooBar"));

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), result.size());
    CPPUNIT_ASSERT_EQUAL(std::string("12345"), result[0]);
    CPPUNIT_ASSERT_EQUAL(std::string("foo"), result[1]);
    CPPUNIT_ASSERT_EQUAL(std::string("bar"), result[2]);
}

void StringTokenizerTestSuite::breakVeryComplexIdentifier()
{
    std::vector<std::string> result = toVector(StringTokenizer::break_parts_of_word("XYZ12345XMLDocument"));

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), result.size());
    CPPUNIT_ASSERT_EQUAL(std::string("xyz"), result[0]);
    CPPUNIT_ASSERT_EQUAL(std::string("12345"), result[1]);
    CPPUNIT_ASSERT_EQUAL(std::string("xml"), result[2]);
    CPPUNIT_ASSERT_EQUAL(std::string("document"), result[3]);
}

void StringTokenizerTestSuite::breakVeryComplexIdentifierWithUnderscore()
{
    std::vector<std::string> result = toVector(StringTokenizer::break_parts_of_word("XYZ12345XM_LDocument"));

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(5), result.size());
    CPPUNIT_ASSERT_EQUAL(std::string("xyz"), result[0]);
    CPPUNIT_ASSERT_EQUAL(std::string("12345"), result[1]);
    CPPUNIT_ASSERT_EQUAL(std::string("xm"), result[2]);
    CPPUNIT_ASSERT_EQUAL(std::string("l"), result[3]);
    CPPUNIT_ASSERT_EQUAL(std::string("document"), result[4]);
}

static bool test_word_boundary_match(const String &name, const String &candidate,
                                     List<size_t> &match_result)
{
    List<String> words = StringTokenizer::break_parts_of_word(name);
    return StringTokenizer::is_boundary_match(words, candidate, match_result);
}

void StringTokenizerTestSuite::matchSimpleSearchPattern()
{
    List<size_t> match_result;
    bool r = test_word_boundary_match ("foo_bar_text", "fb", match_result);
    CPPUNIT_ASSERT(r);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), match_result[0]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), match_result[1]);
}

void StringTokenizerTestSuite::commonPrefix()
{
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), StringTokenizer::common_prefix("sparta", "spa"));
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), StringTokenizer::common_prefix("spa", "sparta"));
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), StringTokenizer::common_prefix("", ""));
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), StringTokenizer::common_prefix("xyz", "abc"));
}

void StringTokenizerTestSuite::matchSimpleSearchPattern2()
{
    List<size_t> match_result;
    bool r = test_word_boundary_match ("foo_bar_text", "fbarte", match_result);
    CPPUNIT_ASSERT(r);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), match_result[0]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), match_result[1]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), match_result[2]);
}

void StringTokenizerTestSuite::matchSimpleSearchPatternSkipChunks()
{
    List<size_t> match_result;
    bool r = test_word_boundary_match ("foo_bar_text_sparta", "ftexts", match_result);
    CPPUNIT_ASSERT(r);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), match_result[0]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), match_result[1]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), match_result[2]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), match_result[3]);
}

void StringTokenizerTestSuite::matchSimpleSearchPatternInvalid()
{
    List<size_t> match_result;
    bool r = test_word_boundary_match ("foo_bar_text", "fbx", match_result);
    CPPUNIT_ASSERT(!r);
}

void StringTokenizerTestSuite::matchSimpleSearchPatternWithInvalidCandidateChars()
{
    List<size_t> match_result;
    bool r = test_word_boundary_match ("foo_bar_text", "f_^@ba", match_result);
    CPPUNIT_ASSERT(r);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), match_result[0]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), match_result[1]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), match_result[2]);
}

void StringTokenizerTestSuite::matchSimpleSearchPatternComplex()
{
    List<size_t> match_result;
    bool r = test_word_boundary_match ("ob_obsah_s", "obsas", match_result);
    CPPUNIT_ASSERT(r);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), match_result[0]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), match_result[1]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), match_result[2]);
}

void StringTokenizerTestSuite::matchSimpleSearchPatternComplex2()
{
    List<size_t> match_result;
    bool r = test_word_boundary_match ("spa_pax_paxo_paxon", "spaxon", match_result);
    CPPUNIT_ASSERT(r);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), match_result[0]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), match_result[1]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0), match_result[2]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(5), match_result[3]);
}

void StringTokenizerTestSuite::findMatchInvalid()
{
    std::unique_ptr<MatchResult> r = StringTokenizer::find_match(new CompletionCandidate("foo_bar"), "xyz");
    CPPUNIT_ASSERT(!r);
}

void StringTokenizerTestSuite::findMatchInvalidSmaller()
{
    std::unique_ptr<MatchResult> r = StringTokenizer::find_match(new CompletionCandidate("foo_bar"), "foo_bar_");
    CPPUNIT_ASSERT(!r);
}

void StringTokenizerTestSuite::findMatchExactCaseSensitive()
{
    std::unique_ptr<MatchResult> r = StringTokenizer::find_match(new CompletionCandidate("FooBar"), "FooBar");
    CPPUNIT_ASSERT_EQUAL(EXACT_MATCH_CASE_SENSITIVE, r->type);
}

void StringTokenizerTestSuite::findMatchExactCaseInsensitive()
{
    std::unique_ptr<MatchResult> r = StringTokenizer::find_match(new CompletionCandidate("FooBar"), "Foobar");
    CPPUNIT_ASSERT_EQUAL(EXACT_MATCH_CASE_INSENSITIVE, r->type);
}

void StringTokenizerTestSuite::findMatchPrefixCaseSensitive()
{
    std::unique_ptr<MatchResult> r = StringTokenizer::find_match(new CompletionCandidate("FooBarBaz"), "FooBar");
    CPPUNIT_ASSERT_EQUAL(PREFIX_MATCH_CASE_SENSITIVE, r->type);
}

void StringTokenizerTestSuite::findMatchPrefixCaseInsensitive()
{
    std::unique_ptr<MatchResult> r = StringTokenizer::find_match(new CompletionCandidate("FooBarBaz"), "Foobar");
    CPPUNIT_ASSERT_EQUAL(PREFIX_MATCH_CASE_INSENSITIVE, r->type);
}

void StringTokenizerTestSuite::findMatchWordBoundary()
{
    std::unique_ptr<MatchResult> r = StringTokenizer::find_match(new CompletionCandidate("FooBarBaz"), "fbb");
    CPPUNIT_ASSERT_EQUAL(WORD_BOUNDARY_MATCH, r->type);

    WordBoundaryMatchResult *wbm = static_cast<WordBoundaryMatchResult *> (r.get());
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), wbm->indices[0]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), wbm->indices[1]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), wbm->indices[2]);
}

static List<std::unique_ptr<MatchResult> > test_find_and_sort_matches(const List<String> &candidate_names, const String &query)
{
    List<CompletionCandidate *> candidates;
    for (unsigned i = 0; i < candidate_names.size(); i++)
        candidates.push_back(new CompletionCandidate(candidate_names[i]));

    return StringTokenizer::find_and_sort_matches(candidates, query);
}

void StringTokenizerTestSuite::findAndSortResultsSimple()
{
    List<String> names = {"foo", "bar", "baz"};
    List<std::unique_ptr<MatchResult> > results = test_find_and_sort_matches(names, "fo");

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), results.size());
    CPPUNIT_ASSERT_EQUAL(PREFIX_MATCH_CASE_SENSITIVE, results[0]->type);
    CPPUNIT_ASSERT_EQUAL(std::string("foo"), results[0]->candidate->name.ref());
}

void StringTokenizerTestSuite::findAndSortResultsSimpleMultiple()
{
    List<String> names = {"foo", "fredy", "baz", "f"};
    List<std::unique_ptr<MatchResult> > results = test_find_and_sort_matches(names, "f");

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), results.size());
    CPPUNIT_ASSERT_EQUAL(std::string("f"), results[0]->candidate->name.ref());
    CPPUNIT_ASSERT_EQUAL(EXACT_MATCH_CASE_SENSITIVE, results[0]->type);
    CPPUNIT_ASSERT_EQUAL(std::string("foo"), results[1]->candidate->name.ref());
    CPPUNIT_ASSERT_EQUAL(PREFIX_MATCH_CASE_SENSITIVE, results[1]->type);
    CPPUNIT_ASSERT_EQUAL(std::string("fredy"), results[2]->candidate->name.ref());
    CPPUNIT_ASSERT_EQUAL(PREFIX_MATCH_CASE_SENSITIVE, results[2]->type);
}

void StringTokenizerTestSuite::findAndSortResultsCaseSensitivity()
{
    List<String> names = {"Fr", "Fredy", "Baz", "franko", "fr"};
    List<std::unique_ptr<MatchResult> > results = test_find_and_sort_matches(names, "Fr");

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), results.size());
    CPPUNIT_ASSERT_EQUAL(std::string("Fr"), results[0]->candidate->name.ref());
    CPPUNIT_ASSERT_EQUAL(EXACT_MATCH_CASE_SENSITIVE, results[0]->type);
    CPPUNIT_ASSERT_EQUAL(std::string("fr"), results[1]->candidate->name.ref());
    CPPUNIT_ASSERT_EQUAL(EXACT_MATCH_CASE_INSENSITIVE, results[1]->type);
    CPPUNIT_ASSERT_EQUAL(std::string("Fredy"), results[2]->candidate->name.ref());
    CPPUNIT_ASSERT_EQUAL(PREFIX_MATCH_CASE_SENSITIVE, results[2]->type);
    CPPUNIT_ASSERT_EQUAL(std::string("franko"), results[3]->candidate->name.ref());
    CPPUNIT_ASSERT_EQUAL(PREFIX_MATCH_CASE_INSENSITIVE, results[3]->type);
}

void StringTokenizerTestSuite::findAndSortResultsCaseMixture()
{
    List<String> names = {"fbar", "f_call_bar", "from_bar_and_read", "gnome", "", "ffbar"};
    List<std::unique_ptr<MatchResult> > results = test_find_and_sort_matches(names, "fbar");

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), results.size());
    CPPUNIT_ASSERT_EQUAL(EXACT_MATCH_CASE_SENSITIVE, results[0]->type);
    CPPUNIT_ASSERT_EQUAL(std::string("fbar"), results[0]->candidate->name.ref());
    CPPUNIT_ASSERT_EQUAL(WORD_BOUNDARY_MATCH, results[1]->type);
    CPPUNIT_ASSERT_EQUAL(std::string("from_bar_and_read"), results[1]->candidate->name.ref());
    CPPUNIT_ASSERT_EQUAL(WORD_BOUNDARY_MATCH, results[2]->type);
    CPPUNIT_ASSERT_EQUAL(std::string("f_call_bar"), results[2]->candidate->name.ref());
}

void StringTokenizerTestSuite::findAndSortResultsLongerPrefixAtBeginning()
{
    List<String> names = {"get_long_value_with_very_nice", "gloooveshark", "get_small_and_long", "gl_o"};
    List<std::unique_ptr<MatchResult> > results = test_find_and_sort_matches(names, "glo");

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(4), results.size());
    CPPUNIT_ASSERT_EQUAL(std::string("gloooveshark"), results[0]->candidate->name.ref());
    CPPUNIT_ASSERT_EQUAL(std::string("gl_o"), results[1]->candidate->name.ref());
    CPPUNIT_ASSERT_EQUAL(std::string("get_long_value_with_very_nice"), results[2]->candidate->name.ref());
    CPPUNIT_ASSERT_EQUAL(std::string("get_small_and_long"), results[3]->candidate->name.ref());
}

void StringTokenizerTestSuite::setUp()
{
}

void StringTokenizerTestSuite::tearDown()
{
}
