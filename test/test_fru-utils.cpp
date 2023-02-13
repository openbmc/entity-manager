#include "fru_utils.hpp"

#include <algorithm>
#include <array>
#include <iterator>

#include "gtest/gtest.h"

extern "C"
{
// Include for I2C_SMBUS_BLOCK_MAX
#include <linux/i2c.h>
}
