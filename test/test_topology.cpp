#include "topology.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::UnorderedElementsAre;

const std::string subchassisPath =
    "/xyz/openbmc_project/inventory/system/chassis/Subchassis";
const std::string superchassisPath =
    "/xyz/openbmc_project/inventory/system/chassis/Superchassis";
const std::string superpowerPath =
    "/xyz/openbmc_project/inventory/system/chassis/Superpower";

const Association subchassisAssoc =
    std::make_tuple("contained_by", "containing", superchassisPath);
const Association subPowerAssoc =
    std::make_tuple("powered_by", "powering", superpowerPath);

const nlohmann::json subchassisExposesItem = nlohmann::json::parse(R"(
    {
        "ConnectsToType": "BackplanePort",
        "Name": "MyDownstreamPort",
        "Type": "DownstreamPort"
    }
)");

const nlohmann::json subpowerExposesItem = nlohmann::json::parse(R"(
    {
        "PoweredByType": "PowerSupplyPort",
        "Name": "MyPoweredPort",
        "Type": "PoweredPort"
    }
)");

const nlohmann::json superchassisExposesItem = nlohmann::json::parse(R"(
    {
        "Name": "MyBackplanePort",
        "Type": "BackplanePort"
    }
)");

const nlohmann::json superpowerExposesItem = nlohmann::json::parse(R"(
    {
        "Name": "PowerSupplyPort",
        "Type": "PowerSupplyPort"
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

    EXPECT_EQ(assocs.size(), 0);
}

TEST(Topology, EmptyExposes)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", nlohmann::json());
    topo.addBoard(superchassisPath, "Chassis", "BoardB", nlohmann::json());

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0);
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

    EXPECT_EQ(assocs.size(), 0);
}

TEST(Topology, MissingPoweredByType)
{
    const nlohmann::json subchassisMissingPoweredByType = nlohmann::json::parse(R"(
        {
            "Name": "MyDownstreamPort",
            "Type": "DownstreamPort"
        }
    )");

    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA",
                  subchassisMissingPoweredByType);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  subpowerExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0);
}

TEST(Topology, OtherExposes)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", otherExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB", otherExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0);
}

TEST(Topology, NoMatchSubchassis)
{
    Topology topo;

    BoardMap boards{{subchassisPath, "BoardA"},
                    {superchassisPath, "BoardB"},
                    {superpowerPath, "BoardC"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", otherExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  superchassisExposesItem);
    topo.addBoard(superpowerPath, "Chassis", "BoardC",
                  superpowerExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0);
}

TEST(Topology, NoMatchSuperchassis)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"},
                    {subchassisPath, "BoardB"},
                    {superchassisPath, "BoardC"},
                    {superpowerPath, "BoardD"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(subchassisPath, "Chassis", "BoardB", subpowerExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardC", otherExposesItem);
    topo.addBoard(superpowerPath, "Chassis", "BoardD", otherExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0);
}

TEST(Topology, BasicChassis)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  superchassisExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 1);
    EXPECT_EQ(assocs[subchassisPath].size(), 1);
    EXPECT_EQ(assocs[subchassisPath][0], subchassisAssoc);
}

TEST(Topology, BasicPower)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superpowerPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subpowerExposesItem);
    topo.addBoard(superpowerPath, "Chassis", "BoardB",
                  superpowerExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 1);
    EXPECT_EQ(assocs[subchassisPath].size(), 1);
    EXPECT_EQ(assocs[subchassisPath][0], subPowerAssoc);
}

TEST(Topology, NoNewBoards)
{
    Topology topo;
    BoardMap boards;

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(subchassisPath, "Chassis", "BoardB", subpowerExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardC",
                  superchassisExposesItem);
    topo.addBoard(superpowerPath, "Chassis", "BoardD",
                  superpowerExposesItem);

    // Boards A and B aren't new, so no assocs are returned.
    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 0);
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

    EXPECT_EQ(assocs.size(), 2);
    EXPECT_EQ(assocs[subchassisPath].size(), 1);
    EXPECT_EQ(assocs[subchassisPath][0], subchassisAssoc);
    EXPECT_EQ(assocs[subchassisPath + "2"].size(), 1);
    EXPECT_EQ(assocs[subchassisPath + "2"][0], subchassisAssoc);
}

TEST(Topology, 2Subpowerchassis)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"},
                    {subchassisPath + "2", "BoardB"},
                    {superpowerPath, "BoardC"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subpowerExposesItem);
    topo.addBoard(subchassisPath + "2", "Chassis", "BoardB",
                  subpowerExposesItem);
    topo.addBoard(superpowerPath, "Chassis", "BoardC",
                  superpowerExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 2);
    EXPECT_EQ(assocs[subchassisPath].size(), 1);
    EXPECT_EQ(assocs[subchassisPath][0], subPowerAssoc);
    EXPECT_EQ(assocs[subchassisPath + "2"].size(), 1);
    EXPECT_EQ(assocs[subchassisPath + "2"][0], subPowerAssoc);
}

TEST(Topology, OneNewChassisBoard)
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

    EXPECT_EQ(assocs.size(), 1);
    EXPECT_EQ(assocs[subchassisPath].size(), 1);
    EXPECT_EQ(assocs[subchassisPath][0], subchassisAssoc);
}

TEST(Topology, OneNewPoweredBoard)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subpowerExposesItem);
    topo.addBoard(subchassisPath + "2", "Chassis", "BoardB",
                  subpowerExposesItem);
    topo.addBoard(superpowerPath, "Chassis", "BoardC",
                  superpowerExposesItem);

    // Only the assoc for BoardB should be returned
    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 1);
    EXPECT_EQ(assocs[subchassisPath].size(), 1);
    EXPECT_EQ(assocs[subchassisPath][0], subPowerAssoc);
}

TEST(Topology, OneNewPoweredBoardAndOneNewChassisBoard)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"},
                    {subchassisPath + "3", "BoardC"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(subchassisPath + "2", "Chassis", "BoardB",
                  subchassisExposesItem);
    topo.addBoard(subchassisPath + "3", "Chassis", "BoardC",
                  subpowerExposesItem);
    topo.addBoard(subchassisPath + "4", "Chassis", "BoardD",
                  subpowerExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardE",
                  superchassisExposesItem);
    topo.addBoard(superpowerPath, "Chassis", "BoardF",
                  superpowerExposesItem);

    // Only the assoc for BoardA and BoardC should be returned
    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 2);
    EXPECT_EQ(assocs[subchassisPath].size(), 1);
    EXPECT_EQ(assocs[subchassisPath][0], subchassisAssoc);
    EXPECT_EQ(assocs[subchassisPath + "3"].size(), 1);
    EXPECT_EQ(assocs[subchassisPath + "3"][0], subPowerAssoc);
}

TEST(Topology, PoweredAndConteinedByEachOther)
{
    Topology topo;
    BoardMap boards{{superchassisPath, "BoardA"},
                    {superpowerPath, "BoardB"}};

    // BoardA powered_by BoardB and BoardB contained_by BoardA
    topo.addBoard(superchassisPath, "Chassis", "BoardA", subpowerExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardA", superchassisExposesItem);
    topo.addBoard(superpowerPath, "Chassis", "BoardB", subchassisExposesItem);
    topo.addBoard(superpowerPath, "Chassis", "BoardB", superpowerExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 2);
    EXPECT_EQ(assocs[superchassisPath].size(), 1);
    EXPECT_EQ(assocs[superchassisPath][0], subPowerAssoc);
    EXPECT_EQ(assocs[superpowerPath].size(), 1);
    EXPECT_EQ(assocs[superpowerPath][0], subchassisAssoc);
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

    EXPECT_EQ(assocs.size(), 1);
    EXPECT_EQ(assocs[subchassisPath].size(), 2);

    EXPECT_THAT(assocs[subchassisPath],
                UnorderedElementsAre(subchassisAssoc, subchassisAssoc2));
}

TEST(Topology, 2Superpower)
{
    const Association subPowerAssoc2 =
        std::make_tuple("powered_by", "powering", superpowerPath + "2");

    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"},
                    {superpowerPath, "BoardB"},
                    {superpowerPath + "2", "BoardC"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subpowerExposesItem);
    topo.addBoard(superpowerPath, "Chassis", "BoardB",
                  superpowerExposesItem);
    topo.addBoard(superpowerPath + "2", "Chassis", "BoardC",
                  superpowerExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 1);
    EXPECT_EQ(assocs[subchassisPath].size(), 2);

    EXPECT_THAT(assocs[subchassisPath],
                UnorderedElementsAre(subPowerAssoc, subPowerAssoc2));
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

    EXPECT_EQ(assocs.size(), 2);
    EXPECT_EQ(assocs[subchassisPath].size(), 2);
    EXPECT_EQ(assocs[subchassisPath + "2"].size(), 2);

    EXPECT_THAT(assocs[subchassisPath],
                UnorderedElementsAre(subchassisAssoc, subchassisAssoc2));
    EXPECT_THAT(assocs[subchassisPath + "2"],
                UnorderedElementsAre(subchassisAssoc, subchassisAssoc2));
}

TEST(Topology, 2SuperpowerAnd2Subchassis)
{
    const Association subPowerAssoc2 =
        std::make_tuple("powered_by", "powering", superpowerPath + "2");

    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"},
                    {subchassisPath + "2", "BoardB"},
                    {superpowerPath, "BoardC"},
                    {superpowerPath + "2", "BoardD"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subpowerExposesItem);
    topo.addBoard(subchassisPath + "2", "Chassis", "BoardB",
                  subpowerExposesItem);
    topo.addBoard(superpowerPath, "Chassis", "BoardC", superpowerExposesItem);
    topo.addBoard(superpowerPath + "2", "Chassis", "BoardD",
                  superpowerExposesItem);

    auto assocs = topo.getAssocs(boards);

    EXPECT_EQ(assocs.size(), 2);
    EXPECT_EQ(assocs[subchassisPath].size(), 2);
    EXPECT_EQ(assocs[subchassisPath + "2"].size(), 2);

    EXPECT_THAT(assocs[subchassisPath],
                UnorderedElementsAre(subPowerAssoc, subPowerAssoc2));
    EXPECT_THAT(assocs[subchassisPath + "2"],
                UnorderedElementsAre(subPowerAssoc, subPowerAssoc2));
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

        EXPECT_EQ(assocs.size(), 2);
        EXPECT_EQ(assocs[subchassisPath].size(), 1);
        EXPECT_EQ(assocs[subchassisPath][0], subchassisAssoc);
        EXPECT_EQ(assocs[subchassisPath + "2"].size(), 1);
        EXPECT_EQ(assocs[subchassisPath + "2"][0], subchassisAssoc);
    }

    {
        topo.remove("BoardA");
        auto assocs = topo.getAssocs(boards);

        EXPECT_EQ(assocs.size(), 1);
        EXPECT_EQ(assocs[subchassisPath + "2"].size(), 1);
        EXPECT_EQ(assocs[subchassisPath + "2"][0], subchassisAssoc);
    }

    {
        topo.remove("BoardB");
        auto assocs = topo.getAssocs(boards);

        EXPECT_EQ(assocs.size(), 0);
    }
}
