#include "topology.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::UnorderedElementsAre;

const std::string subchassisPath =
    "/xyz/openbmc_project/inventory/system/chassis/Subchassis";
const std::string superchassisPath =
    "/xyz/openbmc_project/inventory/system/chassis/Superchassis";

const Association subchassisAssoc =
    std::make_tuple("contained_by", "containing", superchassisPath);

const nlohmann::json subchassisExposesItem = nlohmann::json::parse(R"(
    {
        "ConnectsToType": "BackplanePort",
        "Name": "MyDownstreamPort",
        "Type": "DownstreamPort"
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

TEST(Topology, Empty)
{
    Topology topo;

    auto assocs = topo.getAssocs();

    EXPECT_EQ(assocs.size(), 0);
}

TEST(Topology, EmptyExposes)
{
    Topology topo;

    topo.addBoard(subchassisPath, "Chassis", nlohmann::json());
    topo.addBoard(superchassisPath, "Chassis", nlohmann::json());

    auto assocs = topo.getAssocs();

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

    topo.addBoard(subchassisPath, "Chassis", subchassisMissingConnectsTo);
    topo.addBoard(superchassisPath, "Chassis", superchassisExposesItem);

    auto assocs = topo.getAssocs();

    EXPECT_EQ(assocs.size(), 0);
}

TEST(Topology, OtherExposes)
{
    Topology topo;

    topo.addBoard(subchassisPath, "Chassis", otherExposesItem);
    topo.addBoard(superchassisPath, "Chassis", otherExposesItem);

    auto assocs = topo.getAssocs();

    EXPECT_EQ(assocs.size(), 0);
}

TEST(Topology, NoMatchSubchassis)
{
    Topology topo;

    topo.addBoard(subchassisPath, "Chassis", otherExposesItem);
    topo.addBoard(superchassisPath, "Chassis", superchassisExposesItem);

    auto assocs = topo.getAssocs();

    EXPECT_EQ(assocs.size(), 0);
}

TEST(Topology, NoMatchSuperchassis)
{
    Topology topo;

    topo.addBoard(subchassisPath, "Chassis", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", otherExposesItem);

    auto assocs = topo.getAssocs();

    EXPECT_EQ(assocs.size(), 0);
}

TEST(Topology, Basic)
{
    Topology topo;

    topo.addBoard(subchassisPath, "Chassis", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", superchassisExposesItem);

    auto assocs = topo.getAssocs();

    EXPECT_EQ(assocs.size(), 1);
    EXPECT_EQ(assocs[subchassisPath].size(), 1);
    EXPECT_EQ(assocs[subchassisPath][0], subchassisAssoc);
}

TEST(Topology, 2Subchassis)
{
    Topology topo;

    topo.addBoard(subchassisPath, "Chassis", subchassisExposesItem);
    topo.addBoard(subchassisPath + "2", "Chassis", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", superchassisExposesItem);

    auto assocs = topo.getAssocs();

    EXPECT_EQ(assocs.size(), 2);
    EXPECT_EQ(assocs[subchassisPath].size(), 1);
    EXPECT_EQ(assocs[subchassisPath][0], subchassisAssoc);
    EXPECT_EQ(assocs[subchassisPath + "2"].size(), 1);
    EXPECT_EQ(assocs[subchassisPath + "2"][0], subchassisAssoc);
}

TEST(Topology, 2Superchassis)
{
    const Association subchassisAssoc2 =
        std::make_tuple("contained_by", "containing", superchassisPath + "2");

    Topology topo;

    topo.addBoard(subchassisPath, "Chassis", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", superchassisExposesItem);
    topo.addBoard(superchassisPath + "2", "Chassis", superchassisExposesItem);

    auto assocs = topo.getAssocs();

    EXPECT_EQ(assocs.size(), 1);
    EXPECT_EQ(assocs[subchassisPath].size(), 2);

    EXPECT_THAT(assocs[subchassisPath],
                UnorderedElementsAre(subchassisAssoc, subchassisAssoc2));
}

TEST(Topology, 2SuperchassisAnd2Subchassis)
{
    const Association subchassisAssoc2 =
        std::make_tuple("contained_by", "containing", superchassisPath + "2");

    Topology topo;

    topo.addBoard(subchassisPath, "Chassis", subchassisExposesItem);
    topo.addBoard(subchassisPath + "2", "Chassis", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", superchassisExposesItem);
    topo.addBoard(superchassisPath + "2", "Chassis", superchassisExposesItem);

    auto assocs = topo.getAssocs();

    EXPECT_EQ(assocs.size(), 2);
    EXPECT_EQ(assocs[subchassisPath].size(), 2);
    EXPECT_EQ(assocs[subchassisPath + "2"].size(), 2);

    EXPECT_THAT(assocs[subchassisPath],
                UnorderedElementsAre(subchassisAssoc, subchassisAssoc2));
    EXPECT_THAT(assocs[subchassisPath + "2"],
                UnorderedElementsAre(subchassisAssoc, subchassisAssoc2));
}
