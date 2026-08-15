#include "entity_manager/probe_type.hpp"

#include <string>

#include <gtest/gtest.h>

using probe::findProbeType;
using probe::probe_type_codes;

// The bare boolean/logical keywords are classified as probe-type commands.
TEST(FindProbeType, ExactKeywordsAreProbeTypes)
{
    EXPECT_EQ(findProbeType("TRUE"), probe_type_codes::TRUE_T);
    EXPECT_EQ(findProbeType("FALSE"), probe_type_codes::FALSE_T);
    EXPECT_EQ(findProbeType("AND"), probe_type_codes::AND);
    EXPECT_EQ(findProbeType("OR"), probe_type_codes::OR);
    EXPECT_EQ(findProbeType("MATCH_ONE"), probe_type_codes::MATCH_ONE);
}

// Incidental surrounding whitespace is tolerated.
TEST(FindProbeType, KeywordsTolerateSurroundingWhitespace)
{
    EXPECT_EQ(findProbeType("  OR"), probe_type_codes::OR);
    EXPECT_EQ(findProbeType("AND  "), probe_type_codes::AND);
    EXPECT_EQ(findProbeType("\tTRUE\t"), probe_type_codes::TRUE_T);
}

// FOUND is a function-style command: FOUND('<name>').
TEST(FindProbeType, FoundFunctionIsProbeType)
{
    EXPECT_EQ(findProbeType("FOUND('Board Name')"), probe_type_codes::FOUND);
    EXPECT_EQ(findProbeType("  FOUND('Board')"), probe_type_codes::FOUND);
}

// The regression this change fixes: a D-Bus probe whose contents contain a
// keyword as a substring must NOT be treated as a probe-type command.
TEST(FindProbeType, DbusProbeWithKeywordSubstringIsNotProbeType)
{
    EXPECT_EQ(findProbeType("xyz.openbmc_project.FruDevice("
                            "{'BOARD_PRODUCT_NAME' : 'HORIZON'})"),
              std::nullopt);
}

// A plain token that merely contains a keyword as a substring is not a match.
TEST(FindProbeType, KeywordSubstringInBareTokenIsNotProbeType)
{
    EXPECT_EQ(findProbeType("HORIZON"), std::nullopt); // contains "OR"
    EXPECT_EQ(findProbeType("STANDARD"), std::nullopt); // contains "AND"
    EXPECT_EQ(findProbeType("TRUENORTH"), std::nullopt); // starts "TRUE"
}

// A well-formed D-Bus interface probe is not a probe-type command.
TEST(FindProbeType, DbusInterfaceProbeIsNotProbeType)
{
    EXPECT_EQ(findProbeType("xyz.openbmc_project.FruDevice("
                            "{'BOARD_PRODUCT_NAME' : 'Board'})"),
              std::nullopt);
}

// Empty and whitespace-only inputs are not probe-type commands.
TEST(FindProbeType, EmptyOrWhitespaceIsNotProbeType)
{
    EXPECT_EQ(findProbeType(""), std::nullopt);
    EXPECT_EQ(findProbeType("   "), std::nullopt);
}
