/*
// Copyright (c) 2025 Intel Corporation
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
/// \file fru_dcscm.hpp

#pragma once
#include "fru_utils.hpp"

namespace dcscm_fru
{
/*This table describes HPM FRU MultiRecord fields and sizes
 *as per OCP DC-SCM Rev 2.1 Ver1.1.
 */
constexpr std::array<std::pair<const char*, uint8_t>, 9>
    hpmFruMultiRecordTable = {
        {{"MANUFACTURE_ID", 3},
         {"OEM_RECORD_VERSION", 1},
         {"DC_SCI_REVISION", 2},
         {"DC_SCI_VERSION", 1},
         {"LTPI_REVISION", 2},
         {"LTPI_VERSION", 1},
         {"DC_SCM_TYPE", 1},
         {"HPM_VENDOR_ID", 2},
         {"HPM_DEVICE_ID", 2}}};

void populateMultirecordOCPDCSCM(
    std::span<const uint8_t> device,
    boost::container::flat_map<std::string, std::string>& result);
} // namespace dcscm_fru
