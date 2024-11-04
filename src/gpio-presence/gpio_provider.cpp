
#include "gpio_provider.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string>

using namespace gpio_presence;

GPIOProvider::GPIOProvider(sdbusplus::async::context& io) : io(io) {}

bool GPIOProvider::addInputLine(const std::string& lineLabel)
{
    if (gpioLines.find(lineLabel) != gpioLines.end())
    {
        return true;
    }

    gpiod::line line = findLine(lineLabel);

    if (!line)
    {
        lg2::error("Failed to find line {GPIO_NAME}", "GPIO_NAME", lineLabel);
        return false;
    }
    std::string service = "gpio-presence";
    lineRequest(line, {service, ::gpiod::line_request::DIRECTION_INPUT, 0});

    gpioLines[lineLabel] = line;

    return true;
}

int GPIOProvider::readLine(const std::string& lineLabel)
{
    if (gpioLines.find(lineLabel) == gpioLines.end())
    {
        bool success = addInputLine(lineLabel);
        if (!success)
        {
            return -1;
        }
    }
    const int result = gpioLines[lineLabel].get_value();

    releaseLine(lineLabel);

    return result;
}

void GPIOProvider::releaseLine(const std::string& lineLabel)
{
    if (gpioLines.find(lineLabel) == gpioLines.end())
    {
        return;
    }

    ::gpiod::line line = findLine(lineLabel);

    line.release();
    gpioLines.erase(lineLabel);
}

void GPIOProvider::lineRequest(const gpiod::line& line,
                               const gpiod::line_request& config)
{
    line.request(config);
}

gpiod::line GPIOProvider::findLine(const std::string& label)
{
    return ::gpiod::find_line(label);
}
