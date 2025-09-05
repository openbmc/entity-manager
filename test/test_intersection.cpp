#include "entity_manager.hpp"

#include <gtest/gtest.h>

namespace
{

DBusDeviceDescriptor makeDevice(const std::string& path,
                                const DBusInterface& interface = {})
{
    return {interface, path};
}

const DBusInterface iface1{{"property1", "value1"}};
const DBusInterface iface2{{"property2", "value2"}};
const DBusInterface iface3{{"property3", "value3"}};
const DBusInterface iface4{{"property4", "value4"}};
const DBusInterface iface5{{"property5", "value5"}};

TEST(IntersectDevicesTest, BothEmpty)
{
    FoundDevices set1;
    FoundDevices set2;
    intersectDevices(set1, set2);
    EXPECT_TRUE(set1.empty());
}

TEST(IntersectDevicesTest, Set1Empty)
{
    FoundDevices set1;
    FoundDevices set2 = {makeDevice("/path/1", iface2), makeDevice("/path/2", iface2)};
    intersectDevices(set1, set2);
    EXPECT_TRUE(set1.empty());
}

TEST(IntersectDevicesTest, Set2Empty)
{
    FoundDevices set1 = {makeDevice("/path/1"), makeDevice("/path/2")};
    FoundDevices set2; // iface2 is not used here
    intersectDevices(set1, set2);
    EXPECT_TRUE(set1.empty());
}

TEST(IntersectDevicesTest, NoIntersection)
{
    FoundDevices set1 = {makeDevice("/path/1", iface1), makeDevice("/path/2", iface2)};
    FoundDevices set2 = {makeDevice("/path/3", iface3), makeDevice("/path/4", iface4)};
    intersectDevices(set1, set2);
    EXPECT_TRUE(set1.empty());
}

TEST(IntersectDevicesTest, FullIntersection)
{
    FoundDevices set1 = {makeDevice("/path/1", iface1),
                         makeDevice("/path/2", iface2)};
    FoundDevices set2 = {makeDevice("/path/1", iface3),
                         makeDevice("/path/2", iface4)};
    intersectDevices(set1, set2);
    ASSERT_EQ(set1.size(), 4);
    EXPECT_EQ(set1[0].path, "/path/1");
    EXPECT_EQ(set1[0].interface, iface1);
    EXPECT_EQ(set1[1].path, "/path/2");
    EXPECT_EQ(set1[1].interface, iface2);
    EXPECT_EQ(set1[2].path, "/path/1");
    EXPECT_EQ(set1[2].interface, iface3);
    EXPECT_EQ(set1[3].path, "/path/2");
    EXPECT_EQ(set1[3].interface, iface4);
}

TEST(IntersectDevicesTest, PartialIntersectionSet1Larger)
{
    FoundDevices set1 = {makeDevice("/path/1", iface1),
                         makeDevice("/path/2", iface2),
                         makeDevice("/path/3", iface3)};
    FoundDevices set2 = {makeDevice("/path/2", iface4),
                         makeDevice("/path/3", iface5)};
    intersectDevices(set1, set2);
    ASSERT_EQ(set1.size(), 4);
    EXPECT_EQ(set1[0].path, "/path/2");
    EXPECT_EQ(set1[0].interface, iface2);
    EXPECT_EQ(set1[1].path, "/path/3");
    EXPECT_EQ(set1[1].interface, iface3);
    EXPECT_EQ(set1[2].path, "/path/2");
    EXPECT_EQ(set1[2].interface, iface4);
    EXPECT_EQ(set1[3].path, "/path/3");
    EXPECT_EQ(set1[3].interface, iface5);
}

TEST(IntersectDevicesTest, PartialIntersectionSet2Larger)
{
    FoundDevices set1 = {makeDevice("/path/2", iface1),
                         makeDevice("/path/3", iface2)};
    FoundDevices set2 = {makeDevice("/path/1", iface3),
                         makeDevice("/path/2", iface4),
                         makeDevice("/path/3", iface5)};
    intersectDevices(set1, set2);
    ASSERT_EQ(set1.size(), 4);
    EXPECT_EQ(set1[0].path, "/path/2");
    EXPECT_EQ(set1[0].interface, iface1);
    EXPECT_EQ(set1[1].path, "/path/3");
    EXPECT_EQ(set1[1].interface, iface2);
    EXPECT_EQ(set1[2].path, "/path/2");
    EXPECT_EQ(set1[2].interface, iface4);
    EXPECT_EQ(set1[3].path, "/path/3");
    EXPECT_EQ(set1[3].interface, iface5);
}

TEST(IntersectDevicesTest, OneElementIntersection)
{
    FoundDevices set1 = {makeDevice("/path/1", iface1),
                         makeDevice("/path/common", iface2)};
    FoundDevices set2 = {makeDevice("/path/common", iface3),
                         makeDevice("/path/3", iface4)};
    intersectDevices(set1, set2);
    ASSERT_EQ(set1.size(), 2);
    EXPECT_EQ(set1[0].path, "/path/common");
    EXPECT_EQ(set1[0].interface, iface2);
    EXPECT_EQ(set1[1].path, "/path/common");
    EXPECT_EQ(set1[1].interface, iface3);
}

TEST(IntersectDevicesTest, DuplicatesInSet1)
{
    FoundDevices set1 = {makeDevice("/path/1", iface1),
                         makeDevice("/path/1", iface1),
                         makeDevice("/path/2", iface1)};
    FoundDevices set2 = {makeDevice("/path/1", iface2),
                         makeDevice("/path/2", iface2)};
    intersectDevices(set1, set2);
    // The current implementation will keep duplicates if they are in the
    // intersection.
    ASSERT_EQ(set1.size(), 5);
    EXPECT_EQ(set1[0].path, "/path/1");
    EXPECT_EQ(set1[0].interface, iface1);
    EXPECT_EQ(set1[1].path, "/path/1");
    EXPECT_EQ(set1[1].interface, iface1);
    EXPECT_EQ(set1[2].path, "/path/2");
    EXPECT_EQ(set1[2].interface, iface1);   
    EXPECT_EQ(set1[3].path, "/path/1");
    EXPECT_EQ(set1[3].interface, iface2);
    EXPECT_EQ(set1[4].path, "/path/2");
    EXPECT_EQ(set1[4].interface, iface2);
}

TEST(IntersectDevicesTest, DuplicatesInSet2)
{
    FoundDevices set1 = {makeDevice("/path/1", iface1),
                         makeDevice("/path/2", iface1)};
    FoundDevices set2 = {makeDevice("/path/1", iface2),
                         makeDevice("/path/1", iface2),
                         makeDevice("/path/2", iface2)};
    intersectDevices(set1, set2);

    ASSERT_EQ(set1.size(), 5);
    EXPECT_EQ(set1[0].path, "/path/1");
    EXPECT_EQ(set1[0].interface, iface1);
    EXPECT_EQ(set1[1].path, "/path/2");
    EXPECT_EQ(set1[1].interface, iface1);
    EXPECT_EQ(set1[2].path, "/path/1");
    EXPECT_EQ(set1[2].interface, iface2);   
    EXPECT_EQ(set1[3].path, "/path/1");
    EXPECT_EQ(set1[3].interface, iface2);
    EXPECT_EQ(set1[4].path, "/path/2"); 
    EXPECT_EQ(set1[4].interface, iface2);
}

TEST(IntersectDevicesTest, DuplicatesInBoth)
{
    FoundDevices set1 = {makeDevice("/path/1", iface1),
                         makeDevice("/path/1", iface1),
                         makeDevice("/path/2", iface2)};
    FoundDevices set2 = {makeDevice("/path/1", iface1),
                         makeDevice("/path/2", iface2),
                         makeDevice("/path/2", iface2)};
    intersectDevices(set1, set2);

    ASSERT_EQ(set1.size(), 6);
    EXPECT_EQ(set1[0].path, "/path/1");
    EXPECT_EQ(set1[0].interface, iface1);
    EXPECT_EQ(set1[1].path, "/path/1");
    EXPECT_EQ(set1[1].interface, iface1);
    EXPECT_EQ(set1[2].path, "/path/2");
    EXPECT_EQ(set1[2].interface, iface2);
    EXPECT_EQ(set1[3].path, "/path/1");
    EXPECT_EQ(set1[3].interface, iface1);
    EXPECT_EQ(set1[4].path, "/path/2");
    EXPECT_EQ(set1[4].interface, iface2);
    EXPECT_EQ(set1[5].path, "/path/2");
    EXPECT_EQ(set1[5].interface, iface2);
}

} // namespace
