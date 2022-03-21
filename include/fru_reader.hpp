/*
// Copyright (c) 2022 Equinix, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
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
    std::function<int64_t(off_t offset, size_t len, uint8_t* outbuf)>;

class BaseFRUReader
{
  public:
    // The ::read() operation here is analogous to ReadBlockFunc (with the same
    // return value semantics), but is not subject to SMBus block size
    // limitations; it can read as much data as needed in a single call.
    virtual ssize_t read(off_t start, size_t len, uint8_t* outbuf) = 0;
    virtual ~BaseFRUReader() = default;
};

// A caching wrapper around a ReadBlockFunc
class FRUReader : public BaseFRUReader
{
  public:
    FRUReader(ReadBlockFunc readFunc) :
        readFunc(std::move(readFunc)), cache(Cache()), eof(std::nullopt)
    {}

    static constexpr size_t cacheBlockSize = 32;
    static_assert(cacheBlockSize <= I2C_SMBUS_BLOCK_MAX);
    using CacheBlock = std::array<uint8_t, cacheBlockSize>;

    // indexed by block number (byte number / block size)
    using Cache = std::unordered_map<size_t, CacheBlock>;

    ReadBlockFunc readFunc;
    Cache cache;

    // byte offset of the end of the FRU (if readFunc has reported it)
    std::optional<size_t> eof;

    ssize_t read(off_t start, size_t len, uint8_t* outbuf) override;
};

// wraps an existing BaseFRUReader and applies a fixed offset to all reads
class OffsetFRUReader : public BaseFRUReader
{
  public:
    OffsetFRUReader(BaseFRUReader& inner, off_t offset) :
        inner(inner), offset(offset)
    {}

    ssize_t read(off_t start, size_t len, uint8_t* outbuf) override;

  private:
    BaseFRUReader& inner;
    off_t offset;
};
