#include "entity_manager/board_presence.hpp"

#include <nlohmann/json.hpp>

#include <vector>

#include <gtest/gtest.h>

using board_presence::ArrayObject;
using board_presence::boardFound;
using board_presence::foundNames;
using board_presence::Names;
using board_presence::recordChange;
using board_presence::RecordChange;

TEST(BoardPresence, FoundNamesAreTheConfigurationNames)
{
    const nlohmann::json systemConfiguration = nlohmann::json::parse(R"({
        "1234": {"Name": "Switch Board", "Type": "Board"},
        "5678": {"Name": "GPU Board 0", "Type": "Board"},
        "9abc": {"Type": "Board"}
    })");

    EXPECT_EQ(foundNames(systemConfiguration),
              (Names{"GPU Board 0", "Switch Board"}));
}

TEST(BoardPresence, RecordWaitsForTheBoardItNames)
{
    const Names found{"Switch Board"};
    const auto onSwitch =
        nlohmann::json::parse(R"({"Name": "SMA_0", "Board": "Switch Board"})");
    const auto onGpu =
        nlohmann::json::parse(R"({"Name": "GPU_0", "Board": "GPU Board 0"})");

    EXPECT_TRUE(boardFound(onSwitch.get<nlohmann::json::object_t>(), found));
    EXPECT_FALSE(boardFound(onGpu.get<nlohmann::json::object_t>(), found));
}

TEST(BoardPresence, RecordNamingNoBoardDoesNotWait)
{
    const auto record = nlohmann::json::parse(R"({"Name": "Fan_0"})");

    EXPECT_TRUE(boardFound(record.get<nlohmann::json::object_t>(), Names{}));
}

namespace
{

// A bridge on the switch board with one device on a board of its own.
nlohmann::json::object_t bridge()
{
    return nlohmann::json::parse(R"({
    "Name": "SMA_0",
    "Type": "MCTPUSBDevice",
    "Board": "Switch Board",
    "Port": [1, 2, 1],
    "BridgedEndpoints": [
        {"Name": "CX8_0", "Board": "Late Board"},
        {"Name": "CX8_1"}
    ]
})")
        .get<nlohmann::json::object_t>();
}

} // namespace

TEST(BoardPresence, RecordIsPublishedWhenItsBoardIsFound)
{
    const RecordChange change =
        recordChange(bridge(), Names{}, Names{"Switch Board"});

    EXPECT_EQ(change.action, RecordChange::Action::publish);
}

TEST(BoardPresence, RecordIsWithdrawnWhenItsBoardIsLost)
{
    const RecordChange change =
        recordChange(bridge(), Names{"Switch Board", "Late Board"}, Names{});

    EXPECT_EQ(change.action, RecordChange::Action::withdraw);
}

// Only the array object naming the board comes and goes, so the record and
// its other objects are left alone and the object keeps its index.
TEST(BoardPresence, OnlyTheArrayObjectFollowsItsBoard)
{
    const RecordChange found = recordChange(
        bridge(), Names{"Switch Board"}, Names{"Switch Board", "Late Board"});
    EXPECT_EQ(found.action, RecordChange::Action::update);
    EXPECT_EQ(found.added, (std::vector<ArrayObject>{{"BridgedEndpoints", 0}}));
    EXPECT_TRUE(found.removed.empty());

    const RecordChange lost = recordChange(
        bridge(), Names{"Switch Board", "Late Board"}, Names{"Switch Board"});
    EXPECT_EQ(lost.action, RecordChange::Action::update);
    EXPECT_TRUE(lost.added.empty());
    EXPECT_EQ(lost.removed,
              (std::vector<ArrayObject>{{"BridgedEndpoints", 0}}));
}

TEST(BoardPresence, UnrelatedBoardChangesNothing)
{
    const RecordChange change = recordChange(
        bridge(), Names{"Switch Board"}, Names{"Switch Board", "Other Board"});

    EXPECT_EQ(change.action, RecordChange::Action::none);
}

TEST(BoardPresence, RecordWaitingForItsBoardIgnoresItsArrays)
{
    const RecordChange change =
        recordChange(bridge(), Names{}, Names{"Late Board"});

    EXPECT_EQ(change.action, RecordChange::Action::none);
}

// An array that is not all objects is not published one object at a time,
// so a board it names changes nothing.
TEST(BoardPresence, ArrayThatIsNotAllObjectsIsIgnored)
{
    const auto record = nlohmann::json::parse(R"({
    "Name": "SMA_0",
    "Type": "MCTPUSBDevice",
    "Mixed": [1, {"Name": "CX8_0", "Board": "Late Board"}]
})")
                            .get<nlohmann::json::object_t>();

    const RecordChange change =
        recordChange(record, Names{}, Names{"Late Board"});

    EXPECT_EQ(change.action, RecordChange::Action::none);
}
