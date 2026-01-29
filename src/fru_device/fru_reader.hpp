// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2022 Equinix, Inc.

#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <utility>

extern "C"
{
// For I2C_SMBUS_BLOCK_MAX
#include <linux/i2c.h>
}

// A function to read up to I2C_SMBUS_BLOCK_MAX bytes of FRU data.  Returns
// negative on error, or the number of bytes read otherwise, which may be (but
// is not guaranteed to be) less than len if the read would go beyond the end
// of the FRU.
using ReadBlockFunc =
    std::function<int64_t(off_t offset, std::span<uint8_t> outbuf)>;

// A caching wrapper around a ReadBlockFunc
class FRUReader
{
  public:
    explicit FRUReader(ReadBlockFunc readFunc) : readFunc(std::move(readFunc))
    {}
    // The ::read() operation here is analogous to ReadBlockFunc (with the same
    // return value semantics), but is not subject to SMBus block size
    // limitations; it can read as much data as needed in a single call.
    ssize_t read(off_t start, std::span<uint8_t> outbuf);

  private:
    static constexpr size_t cacheBlockSize = 32;
    static_assert(cacheBlockSize <= I2C_SMBUS_BLOCK_MAX);
    using CacheBlock = std::array<uint8_t, cacheBlockSize>;

    // indexed by block number (byte number / block size)
    using Cache = std::map<uint32_t, CacheBlock>;

    ReadBlockFunc readFunc;
    Cache cache;

    // byte offset of the end of the FRU (if readFunc has reported it)
    std::optional<size_t> eof;
};
