#include "nvme_device/nvme_device_utils.hpp"

#include <array>

#include <gtest/gtest.h>

using namespace nvme;

TEST(NvmeConfig, ValidAndDefaultName)
{
    auto r = parseConfig(R"({"config":[
        {"Index":4,"Name":"E1S_BOOT","BusID":24},
        {"Index":0,"BusID":25}]})");
    EXPECT_TRUE(r.errors.empty());
    ASSERT_EQ(r.entries.size(), 2U);
    EXPECT_EQ(r.entries[0].name, "E1S_BOOT");
    EXPECT_EQ(r.entries[1].name, "nvme0");
    EXPECT_EQ(r.entries[1].bus, 25);
}

TEST(NvmeConfig, InvalidItemsSkipped)
{
    auto r = parseConfig(R"({"config":[
        {"Index":0,"BusID":25},
        {"Index":1},
        {"BusID":26},
        {"Index":"2","BusID":27},
        {"Index":3,"Name":"bad/name","BusID":28},
        {"Index":0,"BusID":29},
        {"Index":5,"Name":"nvme0","BusID":30},
        {"Index":6,"BusID":25},
        {"Index":7,"BusID":70000}]})");
    EXPECT_EQ(r.entries.size(), 1U);
    EXPECT_EQ(r.errors.size(), 8U);
}

TEST(NvmeConfig, BadDocument)
{
    EXPECT_EQ(parseConfig("{not json").errors.size(), 1U);
    EXPECT_EQ(parseConfig("{}").errors.size(), 1U);
    EXPECT_EQ(loadConfig("/nonexistent/file.json").errors.size(), 1U);
}

TEST(NvmeDecode, Vendor)
{
    EXPECT_EQ(decodeVendorId(0x961b), 0x1b96);
    EXPECT_EQ(vendorName(0x1b96), "Western Digital");
    EXPECT_EQ(vendorName(0x8086), "Intel");
    EXPECT_EQ(vendorName(0x1e0f), "Kioxia");
    EXPECT_EQ(vendorName(0x1344), "Micron");
    EXPECT_EQ(vendorName(0x144d), "Samsung");
    EXPECT_EQ(vendorName(0x1234), "");
}

TEST(NvmeDecode, SerialPadding)
{
    std::array<uint8_t, 8> raw = {'S', '1', ' ', 'X', ' ', ' ', 0, 0};
    EXPECT_EQ(trimSerial(raw), "S1 X");

    std::array<uint8_t, 4> bad = {'1', 0x16, 0x16, 0x16};
    EXPECT_EQ(trimSerial(bad), "");
}

TEST(NvmeDecode, OcpResponsePec)
{
    std::array<uint8_t, 58> response = {
        0x38, 0x52, 0x47, 0x31, 0x30, 0x39, 0x57, 0x30, 0x42, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x57, 0x55, 0x53, 0x36, 0x41, 0x37, 0x36,
        0x32, 0x54, 0x50, 0x39, 0x50, 0x38, 0x58, 0x37, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xb8};
    EXPECT_TRUE(validOcpResponse(0x60, response, 56));
    auto properties = decodeOcpResponse(0x60, response);
    ASSERT_TRUE(properties);
    EXPECT_EQ(std::get<std::string>(properties->at("FirmwareVersion")),
              "RG109W0B");
    EXPECT_EQ(std::get<uint64_t>(properties->at("SecurityVersion")), 1U);
    EXPECT_EQ(std::get<std::string>(properties->at("ModelNumber")),
              "WUS6A762TP9P8X7");
    EXPECT_FALSE(validOcpResponse(0x61, response, 56));
    EXPECT_FALSE(validOcpResponse(0x60, response, 55));
    response[1] ^= 1;
    EXPECT_FALSE(validOcpResponse(0x60, response, 56));
}

TEST(NvmeDecode, OcpInventoryCommands)
{
    std::array<uint8_t, 18> guid = {0x10, 0x73, 0x89, 0x20, 0xe5, 0x6b,
                                    0xee, 0x42, 0x58, 0x9a, 0x7a, 0xce,
                                    0xbd, 0xb3, 0x5f, 0x00, 0x85, 0xd0};
    auto properties = decodeOcpResponse(0x20, guid);
    ASSERT_TRUE(properties);
    const auto& guidValue =
        std::get<std::vector<uint8_t>>(properties->at("OcpDataFormatGuid"));
    ASSERT_EQ(guidValue.size(), 16U);
    EXPECT_EQ(guidValue.front(), 0x73);
    EXPECT_EQ(guidValue.back(), 0x85);

    std::array<uint8_t, 40> power{};
    power[0] = 38;
    power[1] = 0x7f;
    power[2] = 25;
    power[37] = 8;
    power[39] = 0x05;
    properties = decodeOcpResponse(0x32, power);
    ASSERT_TRUE(properties);
    EXPECT_EQ(std::get<uint8_t>(properties->at("MaxPowerWatts")), 25);
    EXPECT_EQ(std::get<uint32_t>(properties->at("TotalNvmCapacityGB")), 2048U);

    std::array<uint8_t, 8> version{6, 0, 4, 0, 0, 0, 0, 0xd0};
    properties = decodeOcpResponse(0xf8, version);
    ASSERT_TRUE(properties);
    EXPECT_EQ(std::get<uint16_t>(properties->at("DataFormatVersion")), 4);
    version[0] = 5;
    EXPECT_FALSE(decodeOcpResponse(0xf8, version));
    EXPECT_FALSE(decodeOcpResponse(0xff, version));
}

TEST(NvmeDecode, OcpSecurityVersion64Bit)
{
    std::array<uint8_t, 58> response = {
        0x38, 0x4c, 0x44, 0x44, 0x36, 0x32, 0x46, 0x32, 0x51, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x4d, 0x5a, 0x54, 0x4c, 0x36, 0x31, 0x54,
        0x39, 0x48, 0x46, 0x4a, 0x41, 0x2d, 0x30, 0x30, 0x41, 0x46, 0x42, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xa7};
    auto properties = decodeOcpResponse(0x60, response);
    ASSERT_TRUE(properties);
    EXPECT_EQ(std::get<uint64_t>(properties->at("SecurityVersion")),
              0x100000000ULL);
    EXPECT_EQ(std::get<std::string>(properties->at("ModelNumber")),
              "MZTL61T9HFJA-00AFB");
}

TEST(NvmeState, IdentityAndVpdLifecycle)
{
    std::optional<DeviceState> st;
    Identity a{0x1b96, "AAA"};
    VpdMap vpd{{"PRODUCT_SERIAL_NUMBER", "X"},
               {"BOARD_INFO_AM5", "extra"},
               {"MULTIRECORD_UUID", "uuid"}};

    applyScan(st, a, [&] { return VpdResult(vpd); });
    ASSERT_TRUE(st);
    EXPECT_TRUE(st->supportVpd());
    EXPECT_EQ(st->vpd, vpd);
    st->ocp[0x60]["FirmwareVersion"] = std::string("version");

    // VPD failure keeps the old data.
    applyScan(st, a, [] { return VpdResult(); });
    EXPECT_TRUE(st->supportVpd());
    EXPECT_EQ(st->vpd, vpd);
    EXPECT_EQ(st->ocp.size(), 1U);

    // Identity change drops old VPD even if the new read fails.
    applyScan(st, Identity{0x1b96, "BBB"}, [] { return VpdResult(); });
    ASSERT_TRUE(st);
    EXPECT_FALSE(st->supportVpd());
    EXPECT_TRUE(st->ocp.empty());

    applyScan(st, Identity{0x1b96, "BBB"}, [&] { return VpdResult(vpd); });
    EXPECT_TRUE(st->supportVpd());
    applyScan(st, Identity{0x1b96, "BBB"}, [] { return VpdResult(VpdMap{}); });
    EXPECT_FALSE(st->supportVpd());

    // Basic read failure removes the device and never reads VPD.
    bool called = false;
    applyScan(st, std::nullopt, [&] {
        called = true;
        return VpdResult();
    });
    EXPECT_FALSE(st);
    EXPECT_FALSE(called);
}
