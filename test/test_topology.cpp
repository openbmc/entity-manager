#include "entity_manager/topology.hpp"

#include <ranges>

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
    std::make_tuple("powering", "powered_by", superchassisPath);

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

    auto assocs = topo.getAssocs(std::views::keys(boards));

    EXPECT_EQ(assocs.size(), 0U);
}

TEST(Topology, EmptyExposes)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", nlohmann::json());
    topo.addBoard(superchassisPath, "Chassis", "BoardB", nlohmann::json());

    auto assocs = topo.getAssocs(std::views::keys(boards));

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

    auto assocs = topo.getAssocs(std::views::keys(boards));

    EXPECT_EQ(assocs.size(), 0U);
}

TEST(Topology, OtherExposes)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", otherExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB", otherExposesItem);

    auto assocs = topo.getAssocs(std::views::keys(boards));

    EXPECT_EQ(assocs.size(), 0U);
}

TEST(Topology, NoMatchSubchassis)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", otherExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  superchassisExposesItem);

    auto assocs = topo.getAssocs(std::views::keys(boards));

    EXPECT_EQ(assocs.size(), 0U);
}

TEST(Topology, NoMatchSuperchassis)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB", otherExposesItem);

    auto assocs = topo.getAssocs(std::views::keys(boards));

    EXPECT_EQ(assocs.size(), 0U);
}

TEST(Topology, Basic)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  superchassisExposesItem);

    auto assocs = topo.getAssocs(std::views::keys(boards));

    EXPECT_EQ(assocs.size(), 1U);
    EXPECT_EQ(assocs[subchassisPath].size(), 1U);
    EXPECT_TRUE(assocs[subchassisPath].contains(subchassisAssoc));
}

TEST(Topology, BasicPower)
{
    Topology topo;
    BoardMap boards{{subchassisPath, "BoardA"}, {superchassisPath, "BoardB"}};

    topo.addBoard(subchassisPath, "Chassis", "BoardA", powerExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  superchassisExposesItem);

    auto assocs = topo.getAssocs(std::views::keys(boards));

    EXPECT_EQ(assocs.size(), 1U);
    EXPECT_EQ(assocs[subchassisPath].size(), 2U);
    EXPECT_TRUE(assocs[subchassisPath].contains(subchassisAssoc));
    EXPECT_TRUE(assocs[subchassisPath].contains(powerAssoc));
}

TEST(Topology, NoNewBoards)
{
    Topology topo;
    BoardMap boards;

    topo.addBoard(subchassisPath, "Chassis", "BoardA", subchassisExposesItem);
    topo.addBoard(superchassisPath, "Chassis", "BoardB",
                  superchassisExposesItem);

    // Boards A and B aren't new, so no assocs are returned.
    auto assocs = topo.getAssocs(std::views::keys(boards));

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

    auto assocs = topo.getAssocs(std::views::keys(boards));

    EXPECT_EQ(assocs.size(), 2U);
    EXPECT_EQ(assocs[subchassisPath].size(), 1U);
    EXPECT_TRUE(assocs[subchassisPath].contains(subchassisAssoc));
    EXPECT_EQ(assocs[subchassisPath + "2"].size(), 1U);
    EXPECT_TRUE(assocs[subchassisPath + "2"].contains(subchassisAssoc));
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
    auto assocs = topo.getAssocs(std::views::keys(boards));

    EXPECT_EQ(assocs.size(), 1U);
    EXPECT_EQ(assocs[subchassisPath].size(), 1U);
    EXPECT_TRUE(assocs[subchassisPath].contains(subchassisAssoc));
}

TEST(Topology, 2Superchassis)
{
    Association subchassisAssoc2 =
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

    auto assocs = topo.getAssocs(std::views::keys(boards));

    EXPECT_EQ(assocs.size(), 1U);
    EXPECT_EQ(assocs[subchassisPath].size(), 2U);

    EXPECT_THAT(assocs[subchassisPath],
                UnorderedElementsAre(subchassisAssoc, subchassisAssoc2));
}

TEST(Topology, 2SuperchassisAnd2Subchassis)
{
    Association subchassisAssoc2 =
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

    auto assocs = topo.getAssocs(std::views::keys(boards));

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
        auto assocs = topo.getAssocs(std::views::keys(boards));

        EXPECT_EQ(assocs.size(), 2U);
        EXPECT_EQ(assocs[subchassisPath].size(), 1U);
        EXPECT_TRUE(assocs[subchassisPath].contains(subchassisAssoc));
        EXPECT_EQ(assocs[subchassisPath + "2"].size(), 1U);
        EXPECT_TRUE(assocs[subchassisPath + "2"].contains(subchassisAssoc));
    }

    {
        topo.remove("BoardA");
        auto assocs = topo.getAssocs(std::views::keys(boards));

        EXPECT_EQ(assocs.size(), 1U);
        EXPECT_EQ(assocs[subchassisPath + "2"].size(), 1U);
        EXPECT_TRUE(assocs[subchassisPath + "2"].contains(subchassisAssoc));
    }

    {
        topo.remove("BoardB");
        auto assocs = topo.getAssocs(std::views::keys(boards));

        EXPECT_EQ(assocs.size(), 0U);
    }
}

TEST(Topology, SimilarToTyanS8030)
{
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/ChassisA";
    const std::string boardPath =
        "/xyz/openbmc_project/inventory/system/board/BoardA";
    const std::string psu0Path =
        "/xyz/openbmc_project/inventory/system/powersupply/PSU0";
    const std::string psu1Path =
        "/xyz/openbmc_project/inventory/system/powersupply/PSU1";

    // example 3 fan setup in a 2U chassis
    const std::string fan1Path =
        "/xyz/openbmc_project/inventory/system/fan/SYS_FAN_1";
    const std::string fan2Path =
        "/xyz/openbmc_project/inventory/system/fan/SYS_FAN_2";
    const std::string fan3Path =
        "/xyz/openbmc_project/inventory/system/fan/SYS_FAN_3";

    const std::vector<std::string> fanPaths = {fan1Path, fan2Path, fan3Path};
    const std::vector<std::string> psuPaths = {psu0Path, psu1Path};

    const nlohmann::json chassisContainExposesItem = nlohmann::json::parse(R"(
    {
        "Name": "GenericContainPort",
        "PortType": "containing",
        "Type": "Port"
    }
)");
    const nlohmann::json containedByExposesItem = nlohmann::json::parse(R"(
    {
        "Name": "GenericContainPort",
        "PortType": "contained_by",
        "Type": "Port"
    }
)");
    const nlohmann::json boardPowerExposesItem = nlohmann::json::parse(R"(
    {
        "Name": "GenericPowerPort",
        "PortType": "powered_by",
        "Type": "Port"
    }
)");
    const nlohmann::json psuPowerExposesItem = nlohmann::json::parse(R"(
    {
        "Name": "GenericPowerPort",
        "PortType": "powering",
        "Type": "Port"
    }
)");
    const nlohmann::json coolingExposesItem = nlohmann::json::parse(R"(
    {
        "Name": "GenericCoolingPort",
        "PortType": "cooling",
        "Type": "Port"
    }
)");
    const nlohmann::json cooledByExposesItem = nlohmann::json::parse(R"(
    {
        "Name": "GenericCoolingPort",
        "PortType": "cooled_by",
        "Type": "Port"
    }
)");
    Topology topo;
    BoardMap boards{
        {chassisPath, "ChassisA"}, {boardPath, "BoardA"},
        {psu0Path, "PSU0"},        {psu1Path, "PSU1"},
        {fan1Path, "SYS_FAN_1"},   {fan2Path, "SYS_FAN_2"},
        {fan3Path, "SYS_FAN_3"},
    };

    // configure the chassis to be containing something
    topo.addBoard(chassisPath, "Chassis", "ChassisA",
                  chassisContainExposesItem);

    // configure the chassis to be cooled_by something
    topo.addBoard(chassisPath, "Chassis", "ChassisA", cooledByExposesItem);

    // configure board to be contained by something
    topo.addBoard(boardPath, "Board", "BoardA", containedByExposesItem);

    // configure the board to be powered by something
    topo.addBoard(boardPath, "Board", "BoardA", boardPowerExposesItem);

    // configure the board to be cooled_by something
    topo.addBoard(boardPath, "Board", "BoardA", cooledByExposesItem);

    // configure the PSUs to be powering something
    topo.addBoard(psu0Path, "PowerSupply", "PSU0", psuPowerExposesItem);
    topo.addBoard(psu1Path, "PowerSupply", "PSU1", psuPowerExposesItem);

    // configured PSUs to be contained by something
    topo.addBoard(psu0Path, "PowerSupply", "PSU0", containedByExposesItem);
    topo.addBoard(psu1Path, "PowerSupply", "PSU1", containedByExposesItem);

    // configure Fans to be contained by something
    topo.addBoard(fan1Path, "Fan", "SYS_FAN_1", containedByExposesItem);
    topo.addBoard(fan2Path, "Fan", "SYS_FAN_2", containedByExposesItem);
    topo.addBoard(fan3Path, "Fan", "SYS_FAN_3", containedByExposesItem);

    // configure Fans to be cooling something
    topo.addBoard(fan1Path, "Fan", "SYS_FAN_1", coolingExposesItem);
    topo.addBoard(fan2Path, "Fan", "SYS_FAN_2", coolingExposesItem);
    topo.addBoard(fan3Path, "Fan", "SYS_FAN_3", coolingExposesItem);

    auto assocs = topo.getAssocs(std::views::keys(boards));

    EXPECT_TRUE(assocs.contains(boardPath));
    EXPECT_TRUE(assocs.contains(psu0Path));
    EXPECT_TRUE(assocs.contains(psu1Path));
    EXPECT_TRUE(assocs.contains(fan1Path));
    EXPECT_TRUE(assocs.contains(fan2Path));
    EXPECT_TRUE(assocs.contains(fan3Path));

    // expect chassis to contain board
    EXPECT_EQ(assocs[boardPath].size(), 4);
    EXPECT_TRUE(assocs[boardPath].contains(
        {"contained_by", "containing", chassisPath}));

    // expect board to be cooled_by all the fans
    for (const auto& fanPath : fanPaths)
    {
        EXPECT_TRUE(
            assocs[boardPath].contains({"cooled_by", "cooling", fanPath}));
    }

    // expect powering association from each PSU to the board
    // and expect each PSU to be contained by the chassis
    for (const auto& psuPath : psuPaths)
    {
        EXPECT_EQ(assocs[psuPath].size(), 2);
        EXPECT_TRUE(
            assocs[psuPath].contains({"powering", "powered_by", boardPath}));
        EXPECT_TRUE(assocs[psuPath].contains(
            {"contained_by", "containing", chassisPath}));
    }

    // expect chassis to be cooled_by all fans
    for (const auto& fanPath : fanPaths)
    {
        EXPECT_TRUE(
            assocs[chassisPath].contains({"cooled_by", "cooling", fanPath}));
    }

    // expectations for fan inventory object's associations
    for (const auto& fanPath : fanPaths)
    {
        EXPECT_EQ(assocs[fanPath].size(), 3);
        // expect fan to be cooling board
        EXPECT_TRUE(
            assocs[fanPath].contains({"cooling", "cooled_by", boardPath}));
        // expect fan to be cooling chassis
        EXPECT_TRUE(
            assocs[fanPath].contains({"cooling", "cooled_by", chassisPath}));
        // expect fan to be contained_by chassis
        EXPECT_TRUE(assocs[fanPath].contains(
            {"contained_by", "containing", chassisPath}));
    }
}

static nlohmann::json makeExposesItem(const std::string& name,
                                      const std::string& assocName)
{
    nlohmann::json exposesItem = nlohmann::json::parse(R"(
    {
        "Name": "REPLACE",
        "PortType": "REPLACE",
        "Type": "Port"
    }
)");

    exposesItem["Name"] = name;
    exposesItem["PortType"] = assocName;

    return exposesItem;
}

static nlohmann::json makeContainedPortExposesItem(const std::string& name)
{
    return makeExposesItem(name, "contained_by");
}

static nlohmann::json makeContainingPortExposesItem(const std::string& name)
{
    return makeExposesItem(name, "containing");
}

TEST(Topology, SimilarToYosemiteV3)
{
    const std::string blade1Path =
        "/xyz/openbmc_project/inventory/system/board/Blade1";
    const std::string blade2Path =
        "/xyz/openbmc_project/inventory/system/board/Blade2";
    const std::string blade3Path =
        "/xyz/openbmc_project/inventory/system/board/Blade3";
    const std::string blade4Path =
        "/xyz/openbmc_project/inventory/system/board/Blade4";
    const std::string chassis1Path =
        "/xyz/openbmc_project/inventory/system/chassis/Blade1Chassis";
    const std::string chassis2Path =
        "/xyz/openbmc_project/inventory/system/chassis/Blade2Chassis";
    const std::string chassis3Path =
        "/xyz/openbmc_project/inventory/system/chassis/Blade3Chassis";
    const std::string chassis4Path =
        "/xyz/openbmc_project/inventory/system/chassis/Blade4Chassis";
    const std::string superChassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/SuperChassis";

    const nlohmann::json blade1ExposesItem =
        makeContainedPortExposesItem("Blade1Port");
    const nlohmann::json blade2ExposesItem =
        makeContainedPortExposesItem("Blade2Port");
    const nlohmann::json blade3ExposesItem =
        makeContainedPortExposesItem("Blade3Port");
    const nlohmann::json blade4ExposesItem =
        makeContainedPortExposesItem("Blade4Port");

    const nlohmann::json chassis1ExposesItem =
        makeContainingPortExposesItem("Blade1Port");
    const nlohmann::json chassis2ExposesItem =
        makeContainingPortExposesItem("Blade2Port");
    const nlohmann::json chassis3ExposesItem =
        makeContainingPortExposesItem("Blade3Port");
    const nlohmann::json chassis4ExposesItem =
        makeContainingPortExposesItem("Blade4Port");

    const nlohmann::json chassis1ExposesItem2 =
        makeContainedPortExposesItem("SuperChassisPort");
    const nlohmann::json chassis2ExposesItem2 =
        makeContainedPortExposesItem("SuperChassisPort");
    const nlohmann::json chassis3ExposesItem2 =
        makeContainedPortExposesItem("SuperChassisPort");
    const nlohmann::json chassis4ExposesItem2 =
        makeContainedPortExposesItem("SuperChassisPort");

    const nlohmann::json superChassisExposesItem =
        makeContainingPortExposesItem("SuperChassisPort");

    Topology topo;
    BoardMap boards{
        {blade1Path, "Blade1"},
        {blade2Path, "Blade2"},
        {blade3Path, "Blade3"},
        {blade4Path, "Blade4"},
        {chassis1Path, "Chassis1"},
        {chassis2Path, "Chassis2"},
        {chassis3Path, "Chassis3"},
        {chassis4Path, "Chassis4"},
        {superChassisPath, "SuperChassis"},
    };

    topo.addBoard(blade1Path, "Board", "Blade1", blade1ExposesItem);
    topo.addBoard(blade2Path, "Board", "Blade2", blade2ExposesItem);
    topo.addBoard(blade3Path, "Board", "Blade3", blade3ExposesItem);
    topo.addBoard(blade4Path, "Board", "Blade4", blade4ExposesItem);

    topo.addBoard(chassis1Path, "Chassis", "Chassis1", chassis1ExposesItem);
    topo.addBoard(chassis2Path, "Chassis", "Chassis2", chassis2ExposesItem);
    topo.addBoard(chassis3Path, "Chassis", "Chassis3", chassis3ExposesItem);
    topo.addBoard(chassis4Path, "Chassis", "Chassis4", chassis4ExposesItem);

    topo.addBoard(chassis1Path, "Chassis", "Chassis1", chassis1ExposesItem2);
    topo.addBoard(chassis2Path, "Chassis", "Chassis2", chassis2ExposesItem2);
    topo.addBoard(chassis3Path, "Chassis", "Chassis3", chassis3ExposesItem2);
    topo.addBoard(chassis4Path, "Chassis", "Chassis4", chassis4ExposesItem2);

    topo.addBoard(superChassisPath, "Chassis", "SuperChassis",
                  superChassisExposesItem);

    auto assocs = topo.getAssocs(std::views::keys(boards));

    // all blades are contained by their respective chassis
    EXPECT_EQ(assocs[blade1Path].size(), 1);
    EXPECT_TRUE(assocs[blade1Path].contains(
        {"contained_by", "containing", chassis1Path}));

    EXPECT_EQ(assocs[blade2Path].size(), 1);
    EXPECT_TRUE(assocs[blade2Path].contains(
        {"contained_by", "containing", chassis2Path}));

    EXPECT_EQ(assocs[blade3Path].size(), 1);
    EXPECT_TRUE(assocs[blade3Path].contains(
        {"contained_by", "containing", chassis3Path}));

    EXPECT_EQ(assocs[blade4Path].size(), 1);
    EXPECT_TRUE(assocs[blade4Path].contains(
        {"contained_by", "containing", chassis4Path}));

    // all chassis are contained by the superchassis
    EXPECT_TRUE(assocs[chassis1Path].contains(
        {"contained_by", "containing", superChassisPath}));

    EXPECT_TRUE(assocs[chassis2Path].contains(
        {"contained_by", "containing", superChassisPath}));

    EXPECT_TRUE(assocs[chassis3Path].contains(
        {"contained_by", "containing", superChassisPath}));

    EXPECT_TRUE(assocs[chassis4Path].contains(
        {"contained_by", "containing", superChassisPath}));
}
