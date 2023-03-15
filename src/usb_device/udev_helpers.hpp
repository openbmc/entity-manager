#pragma once

#include <libudev.h>

#include <iostream>
#include <string>

class UDevDevice
{
    friend class UDev;
    friend class UDevMonitor;

  private:
    explicit UDevDevice(struct udev_device* udev) : ptr(udev) {}

  public:
    ~UDevDevice()
    {
        udev_device_unref(ptr);
    }

    std::string getDevnode()
    {
        return {udev_device_get_devnode(ptr)};
    }

    std::string getAction()
    {
        return {udev_device_get_action(ptr)};
    }

    std::string getSysattrValue(const char* sysattr)
    {
        const char* value = udev_device_get_sysattr_value(ptr, sysattr);
        if (value == nullptr)
        {
            std::cerr << "No value found for sysattr: " << sysattr << "\n";
            return {};
        }
        return {value};
    }

    std::string getSysnum()
    {
        const char* value = udev_device_get_sysnum(ptr);
        if (value == nullptr)
        {
            std::cerr << "No value found for sysnum\n";
            return {};
        }
        return {value};
    }

    UDevDevice getParentWithSubsystemDevtype(const char* subsystem,
                                             const char* devtype)
    {
        return UDevDevice(udev_device_get_parent_with_subsystem_devtype(
            ptr, subsystem, devtype));
    }

  private:
    struct udev_device* ptr;
};

class UDevEnumerate
{
    friend class UDev;

  public:
    ~UDevEnumerate()
    {
        udev_enumerate_unref(ptr);
    }

    int addMatchSubsystem(const char* subsystem)
    {
        return udev_enumerate_add_match_subsystem(ptr, subsystem);
    }

    int scanDevices()
    {
        return udev_enumerate_scan_devices(ptr);
    }

    struct udev_list_entry* getListEntry()
    {
        return udev_enumerate_get_list_entry(ptr);
    }

  private:
    explicit UDevEnumerate(struct udev* udev) : ptr(udev_enumerate_new(udev)) {}

    struct udev_enumerate* ptr;
};

class UDevMonitor
{
    friend class UDev;

  public:
    int filterAddMatchSubsystemDevtype(const char* subsystem,
                                       const char* devtype)
    {
        return udev_monitor_filter_add_match_subsystem_devtype(
            ptr, subsystem, devtype);
    }

    int enableReceiving()
    {
        return udev_monitor_enable_receiving(ptr);
    }

    int getFd()
    {
        return udev_monitor_get_fd(ptr);
    }

    UDevDevice receiveDevice()
    {
        return UDevDevice(udev_monitor_receive_device(ptr));
    }

    ~UDevMonitor()
    {
        udev_monitor_unref(ptr);
    }

  private:
    explicit UDevMonitor(struct udev* udev) :
        ptr(udev_monitor_new_from_netlink(udev, "udev"))
    {
        if (ptr == nullptr)
        {
            std::cerr << "Can't create udev- USB Device Probe failed\n";
        }
    }

    struct udev_monitor* ptr;
};

class UDev
{
  private:
    explicit UDev(struct udev* udev) : ptr(udev) {}

  public:
    UDev() : ptr(udev_new())
    {
        if (ptr == nullptr)
        {
            std::cerr << "Can't create udev- USB Device Probe failed\n";
        }
    }
    ~UDev()
    {
        udev_unref(ptr);
    }

    UDevDevice createDeviceFromSyspath(const char* syspath) const
    {
        struct udev_device* newPtr = udev_device_new_from_syspath(ptr, syspath);
        if (newPtr == nullptr)
        {
            std::cerr << "Can't create udev device from syspath: " << syspath
                      << "\n";
        }
        return UDevDevice(newPtr);
    }

    UDevEnumerate createEnumerate() const
    {
        return UDevEnumerate(ptr);
    }

    UDevMonitor createMonitor() const
    {
        return UDevMonitor(ptr);
    }

    struct udev* ptr;
};
