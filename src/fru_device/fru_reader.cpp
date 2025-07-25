// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2022 Equinix, Inc.

#include "fru_reader.hpp"

#include <cstring>

ssize_t FRUReader::read(off_t start, size_t len, uint8_t* outbuf)
{
    size_t done = 0;
    size_t remaining = len;
    size_t cursor = start;
    while (done < len)
    {
        if (eof.has_value() && cursor >= eof.value())
        {
            break;
        }

        const uint8_t* blkData = nullptr;
        size_t available = 0;
        size_t blk = cursor / cacheBlockSize;
        size_t blkOffset = cursor % cacheBlockSize;
        auto findBlk = cache.find(blk);
        if (findBlk == cache.end())
        {
            // miss, populate cache
            uint8_t* newData = cache[blk].data();
            int64_t ret =
                readFunc(blk * cacheBlockSize, cacheBlockSize, newData);

            // if we've reached the end of the eeprom, record its size
            if (ret >= 0 && static_cast<size_t>(ret) < cacheBlockSize)
            {
                eof = (blk * cacheBlockSize) + ret;
            }

            if (ret <= 0)
            {
                // don't leave empty blocks in the cache
                cache.erase(blk);
                return done != 0U ? done : ret;
            }

            blkData = newData;
            available = ret;
        }
        else
        {
            // hit, use cached data
            blkData = findBlk->second.data();

            // if the hit is to the block containing the (previously
            // discovered on the miss that populated it) end of the eeprom,
            // don't copy spurious bytes past the end
            if (eof.has_value() && (eof.value() / cacheBlockSize == blk))
            {
                available = eof.value() % cacheBlockSize;
            }
            else
            {
                available = cacheBlockSize;
            }
        }

        size_t toCopy = (blkOffset >= available)
                            ? 0
                            : std::min(available - blkOffset, remaining);

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        memcpy(outbuf + done, blkData + blkOffset, toCopy);
        cursor += toCopy;
        done += toCopy;
        remaining -= toCopy;
    }

    return done;
}
