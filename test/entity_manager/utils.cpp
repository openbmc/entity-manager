#include "entity_manager/utils.hpp"

#include "test_em.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include "gtest/gtest.h"

using namespace std::string_literals;

TEST(EntityManager, fwVersionIsSame)
{
    EXPECT_FALSE(em_utils::fwVersionIsSame());
}
