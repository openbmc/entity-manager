#include "entity_manager/perform_scan.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

#include <gtest/gtest.h>

using json = nlohmann::json;

// Removes from missingConfigurations any config whose "Name" is in names.
TEST(PruneMissingByName, RemovesConfigsWhoseNameIsInList)
{
    json missing = {
        {"a", {{"Name", "A"}}}, {"b", {{"Name", "B"}}}, {"c", {{"Name", "C"}}}};
    std::vector<std::string> names = {"A", "C"};
    scan::detail::pruneMissingByName(missing, names);
    EXPECT_EQ(missing.size(), 1);
    EXPECT_EQ(missing["b"]["Name"], "B");
}
