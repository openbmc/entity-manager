#pragma once

class Topology
{
  public:
    using Association = std::tuple<std::string, std::string, std::string>;

    Topology(const sdbusplus::message::object_path& root_)
        : root(root_) {}

    std::vector<Association> dbusProp()
    {
        std::vector<Association> prop;

        if (root == "/xyz/openbmc_project/inventory/system/chassis/Subchassis") {
            prop.emplace_back("chassisContainedBy", "chassisContains",
                              "/xyz/openbmc_project/inventory/system/chassis/Superchassis");
        }

        return prop;
    }

  private:
    sdbusplus::message::object_path root;
};
