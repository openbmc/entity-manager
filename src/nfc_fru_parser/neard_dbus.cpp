#include "neard_dbus.hpp"

#include <sdbusplus/message.hpp>

#include <iostream>
#include <map>
#include <variant>

using Variant =
    std::variant<std::string, std::vector<uint8_t>, uint32_t, bool,
                 std::vector<std::string>, sdbusplus::message::object_path>;

std::vector<uint8_t> getMimePayload(sdbusplus::bus_t& bus,
                                    const std::string& objPath)
{
    auto m = bus.new_method_call(NEARD_SERVICE, objPath.c_str(),
                                 PROPERTY_INTERFACE, "Get");

    m.append(NEARD_RECORD_INTERFACE, "MIMEPayload");
    auto r = bus.call(m);
    std::variant<std::vector<uint8_t>> v;
    r.read(v);
    return std::get<std::vector<uint8_t>>(v);
}

static bool isFruRecord(sdbusplus::bus_t& bus, const std::string& path)
{
    auto m = bus.new_method_call(NEARD_SERVICE, path.c_str(),
                                 PROPERTY_INTERFACE, "GetAll");

    m.append(NEARD_RECORD_INTERFACE);
    auto r = bus.call(m);

    std::map<std::string, Variant> props;
    r.read(props);

    auto t = std::get_if<std::string>(&props["Type"]);
    auto mtype = std::get_if<std::string>(&props["MIME"]);

    return *t == RECORD_TYPE_MIME && mtype;
}

std::optional<std::string> findFruRecordPath(sdbusplus::bus_t& bus)
{
    auto m = bus.new_method_call(NEARD_SERVICE, NEARD_OBJ_PATH,
                                 INTROSPECT_INTERFACE, "Introspect");

    auto r = bus.call(m);
    std::string xml;
    r.read(xml);

    size_t pos = 0;
    while ((pos = xml.find("node name=\"tag", pos)) != std::string::npos)
    {
        auto s = xml.find('"', pos) + 1;
        auto e = xml.find('"', s);
        auto tag = xml.substr(s, e - s);
        pos = e;

        std::string tagPath = std::string(NEARD_OBJ_PATH) + "/" + tag;

        auto m2 = bus.new_method_call(NEARD_SERVICE, tagPath.c_str(),
                                      INTROSPECT_INTERFACE, "Introspect");

        auto r2 = bus.call(m2);
        std::string xml2;
        r2.read(xml2);

        size_t p2 = 0;
        while ((p2 = xml2.find("node name=\"record", p2)) != std::string::npos)
        {
            auto s2 = xml2.find('"', p2) + 1;
            auto e2 = xml2.find('"', s2);
            auto rec = xml2.substr(s2, e2 - s2);
            p2 = e2;

            std::string recPath = tagPath + "/" + rec;
            if (isFruRecord(bus, recPath))
            {
                std::cerr << "Found FRU record. path : " << recPath << "\n";
                return recPath;
            }
        }
    }
    return std::nullopt;
}
