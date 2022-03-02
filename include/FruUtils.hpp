/*
// Copyright (c) 2018 Intel Corporation
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
/// \file FruUtils.hpp

#pragma once
#include <boost/container/flat_map.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>
extern "C"
{
// Include for I2C_SMBUS_BLOCK_MAX
#include <linux/i2c.h>
}

constexpr size_t fruBlockSize = 8;

enum class DecodeState
{
    ok,
    end,
    err,
};

enum class resCodes
{
    resOK,
    resWarn,
    resErr
};

enum class fruAreas
{
    fruAreaInternal = 0,
    fruAreaChassis,
    fruAreaBoard,
    fruAreaProduct,
    fruAreaMultirecord
};

const std::vector<std::string> fruAreaNames = {"INTERNAL", "CHASSIS", "BOARD",
                                               "PRODUCT", "MULTIRECORD"};
const std::regex nonAsciiRegex("[^\x01-\x7f]");

const std::vector<std::string> chassisFruAreas = {"PART_NUMBER",
                                                  "SERIAL_NUMBER"};

const std::vector<std::string> boardFruAreas = {"MANUFACTURER", "PRODUCT_NAME",
                                                "SERIAL_NUMBER", "PART_NUMBER",
                                                "FRU_VERSION_ID"};

const std::vector<std::string> productFruAreas = {
    "MANUFACTURER",  "PRODUCT_NAME", "PART_NUMBER",   "VERSION",
    "SERIAL_NUMBER", "ASSET_TAG",    "FRU_VERSION_ID"};

const std::string fruCustomFieldName = "INFO_AM";

inline fruAreas operator++(fruAreas& x)
{
    return x = static_cast<fruAreas>(std::underlying_type<fruAreas>::type(x) +
                                     1);
}

// A function to read up to I2C_SMBUS_BLOCK_MAX bytes of FRU data.  Returns
// negative on error, or the number of bytes read otherwise, which may be (but
// is not guaranteed to be) less than len if the read would go beyond the end
// of the FRU.
using ReadBlockFunc =
    std::function<int64_t(uint16_t offset, uint8_t len, uint8_t* outbuf)>;

class FRUReader
{
public:
  // The ::read() operation here is analogous to ReadBlockFunc (with the same
  // return value semantics), but is not subject to SMBus block size
  // limitations; it can read as much data as needed in a single call.
  virtual int64_t read(uint16_t start, uint8_t len, uint8_t* outbuf) = 0;
  virtual ~FRUReader() = default;
};

// A caching wrapper around a ReadBlockFunc
class CachingFRUReader : public FRUReader
{
public:
  CachingFRUReader(ReadBlockFunc readFunc) : readFunc(std::move(readFunc))
  {}
  int64_t read(uint16_t start, uint8_t len, uint8_t* outbuf) override;
  static constexpr size_t cacheBlockSize = 32;
  static_assert(cacheBlockSize <= I2C_SMBUS_BLOCK_MAX);

private:
  using CacheBlock = std::array<uint8_t, cacheBlockSize>;

  ReadBlockFunc readFunc;

  // byte offset of the end of the FRU (if readFunc has reported it)
  std::optional<size_t> eof;

  // indexed by block number (byte number / block size)
  boost::container::flat_map<uint32_t, CacheBlock> cache;
};

// A FRUReader wrapper that adds a fixed offset to all reads
class OffsetFRUReader : public FRUReader
{
public:
  OffsetFRUReader(FRUReader& inner, int16_t offset) : inner(inner), offset(offset)
  {}

  int64_t read(uint16_t start, uint8_t len, uint8_t* outbuf) override {
      return inner.read(start + offset, len, outbuf);
  }

private:
  FRUReader& inner;
  int16_t offset;
};

inline const std::string& getFruAreaName(fruAreas area)
{
    return fruAreaNames[static_cast<unsigned int>(area)];
}

std::tm intelEpoch(void);

char sixBitToChar(uint8_t val);

/* 0xd - 0xf are reserved values, but not fatal; use a placeholder char. */
const char bcdHighChars[] = {
    ' ', '-', '.', 'X', 'X', 'X',
};

char bcdPlusToChar(uint8_t val);

bool verifyOffset(const std::vector<uint8_t>& fruBytes, fruAreas currentArea,
                  uint8_t len);

std::pair<DecodeState, std::string>
    decodeFRUData(std::vector<uint8_t>::const_iterator& iter,
                  const std::vector<uint8_t>::const_iterator& end,
                  bool isLangEng);

bool checkLangEng(uint8_t lang);

resCodes
    formatFRU(const std::vector<uint8_t>& fruBytes,
              boost::container::flat_map<std::string, std::string>& result);

std::vector<uint8_t>& getFRUInfo(const uint8_t& bus, const uint8_t& address);

uint8_t calculateChecksum(std::vector<uint8_t>::const_iterator iter,
                          std::vector<uint8_t>::const_iterator end);

uint8_t calculateChecksum(std::vector<uint8_t>& fruAreaData);

unsigned int updateFRUAreaLenAndChecksum(std::vector<uint8_t>& fruData,
                                         size_t fruAreaStart,
                                         size_t fruAreaEndOfFieldsOffset,
                                         size_t fruAreaEndOffset);

ssize_t getFieldLength(uint8_t fruFieldTypeLenValue);

/// \brief Read and validate FRU contents.
/// \param reader the FRUReader to read via
/// \param errorHelp and a helper string for failures
/// \return the FRU contents from the file
std::vector<uint8_t> readFRUContents(FRUReader& reader,
                                     const std::string& errorHelp);

/// \brief Validate an IPMI FRU common header
/// \param hdr the bytes comprising the common header
/// \return true if valid
bool validateIPMIHeader(const std::vector<uint8_t>& hdr);

/// \brief Get offset for a common header area
/// \param area - the area
/// \return the field offset
unsigned int getHeaderAreaFieldOffset(fruAreas area);
