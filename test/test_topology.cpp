#include "entity_manager/topology.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::UnorderedElementsAre;

const std::string subchassisPath =
    "/xyz/openbmc_project/inventory/system/chassis/Subchassis";
const std::string superchassisPath =
    "/xyz/openbmc_project/inventory/system/chassis/Superchassis";

const Association subchassisAssoc =
    std::make_tuple("contained_by", "containing", superchassisPath);
const Association powerAssoc =
    std::make_tuple("powered_by", "powering", subchassisPath);

const nlohmann::json subchassisExposesItem = nlohmann::json::parse(R"(
    {
        "ConnectsToType": "BackplanePort",
        "Name": "MyDownstreamPort",
        "Type": "DownstreamPort"
    }
)");

const nlohmann::json powerExposesItem = nlohmann::json::parse(R"(
    {
        "ConnectsToType": "BackplanePort",
        "Name": "MyDownstreamPort",
        "Type": "DownstreamPort",
        "PowerPort": true
    }
)");

const nlohmann::json superchassisExposesItem = nlohmann::json::parse(R"(
    {
        "Name": "MyBackplanePort",
        "Type": "BackplanePort"
    }
)");

const nlohmann::json otherExposesItem = nlohmann::json::parse(R"(
    {
        "Name": "MyExposes",
        "Type": "OtherType"
    }
)");

using BoardMap = std::map<std::string, std::string>;

TEST(Topology, Empty)
{
    Topology topo;
    BoardMap boards;

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0U);
}

TEST(Topology, EmptyExposes)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", nlohmann::json());
    topo.addBoard(superchassisPath, "Chassis", "BoardB", nlohmann::json());

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0U);
}

TEST(Topology, MissingConnectsTo)
{
    const nlohmann::json subchassisMissingConnectsTo = nlohmann::json::parse(R"(
        {
            "Name": "MyDownstreamPort",
            "Type": "DownstreamPort"
        }
    )");

    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA",
                  subchassisMissingConnectsTo);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  superchassisExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0U);
}

TEST(Topology, OtherExposes)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", otherExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB", otherExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0U);
}

TEST(Topology, NoMatchSubchassis)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", otherExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  superchassisExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0U);
}

TEST(Topology, NoMatchSuperchassis)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB", otherExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0U);
}

TEST(Topology, Basic)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  superchassisExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 1U);
    EXPECT_EQ(assocs[subchassisPath].size(), 1U);
    EXPECT_EQ(assocs[subchassisPath][0], subchassisAssoc);
}

TEST(Topology, BasicPower)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", powerExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  superchassisExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 2U);
    EXPECT_EQ(assocs[subchassisPath].size(), 1U);
    EXPECT_EQ(assocs[subchassisPath][0], subchassisAssoc);
    EXPECT_EQ(assocs[superchassisPath].size(), 1U);
    EXPECT_EQ(assocs[superchassisPath][0], powerAssoc);
}

TEST(Topology, NoNewBoards)
{
    Topology topo;
    BoardMap boards;

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  superchassisExposesItem);

    // Boards A and B aren't new, so no assocs are returned.
    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0U);
}

TEST(Topology, 2Subchassis)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"},
                    {subchassisPath + "2", "BoardB"},
                    {superchassisPath, "BoardC"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(subchassisPath + "2", "Chassis", "BoardB",
                  subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardC",
                  superchassisExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 2U);
    EXPECT_EQ(assocs[subchassisPath].size(), 1U);
    EXPECT_EQ(assocs[subchassisPath][0], subchassisAssoc);
    EXPECT_EQ(assocs[subchassisPath + "2"].size(), 1U);
    EXPECT_EQ(assocs[subchassisPath + "2"][0], subchassisAssoc);
}

TEST(Topology, OneNewBoard)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(subchassisPath + "2", "Chassis", "BoardB",
                  subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardC",
                  superchassisExposesItem);

    // Only the assoc for BoardA should be returned
    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 1U);
    EXPECT_EQ(assocs[subchassisPath].size(), 1U);
    EXPECT_EQ(assocs[subchassisPath][0], subchassisAssoc);
}

TEST(Topology, 2Superchassis)
{
    const Association subchassisAssoc2 =
        std::make_tuple("contained_by", "containing", superchassisPath + "2");

    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"},
                    {superchassisPath, "BoardB"},
                    {superchassisPath + "2", "BoardC"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  superchassisExposesItem);
    topo.addBoard(superchassisPath + "2", "Chassis", "BoardC",
                  superchassisExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 1U);
    EXPECT_EQ(assocs[subchassisPath].size(), 2U);

    EXPECT_THAT(assocs[subchassisPath],
                UnorderedElementsAre(subchassisAssoc, subchassisAssoc2));
}

TEST(Topology, 2SuperchassisAnd2Subchassis)
{
    const Association subchassisAssoc2 =
        std::make_tuple("contained_by", "containing", superchassisPath + "2");

    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"},
                    {subchassisPath + "2", "BoardB"},
                    {superchassisPath, "BoardC"},
                    {superchassisPath + "2", "BoardD"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(subchassisPath + "2", "Chassis", "BoardB",
                  subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardC",
                  superchassisExposesItem);
    topo.addBoard(superchassisPath + "2", "Chassis", "BoardD",
                  superchassisExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 2U);
    EXPECT_EQ(assocs[subchassisPath].size(), 2U);
    EXPECT_EQ(assocs[subchassisPath + "2"].size(), 2U);

    EXPECT_THAT(assocs[subchassisPath],
                UnorderedElementsAre(subchassisAssoc, subchassisAssoc2));
    EXPECT_THAT(assocs[subchassisPath + "2"],
                UnorderedElementsAre(subchassisAssoc, subchassisAssoc2));
}

TEST(Topology, Remove)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"},
                    {subchassisPath + "2", "BoardB"},
                    {superchassisPath, "BoardC"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(subchassisPath + "2", "Chassis", "BoardB",
                  subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardC",
                  superchassisExposesItem);

    {
        auto assocs = topo.getAssocs(boards);

        EXPECT_EQ(assocs.size(), 2U);
        EXPECT_EQ(assocs[subchassisPath].size(), 1U);
        EXPECT_EQ(assocs[subchassisPath][0], subchassisAssoc);
        EXPECT_EQ(assocs[subchassisPath + "2"].size(), 1U);
        EXPECT_EQ(assocs[subchassisPath + "2"][0], subchassisAssoc);
    }

    {
        topo.remove("BoardA");
        auto assocs = topo.getAssocs(boards);

        EXPECT_EQ(assocs.size(), 1U);
        EXPECT_EQ(assocs[subchassisPath + "2"].size(), 1U);
        EXPECT_EQ(assocs[subchassisPath + "2"][0], subchassisAssoc);
    }

    {
        topo.remove("BoardB");
        auto assocs = topo.getAssocs(boards);

        EXPECT_EQ(assocs.size(), 0U);
    }
}
