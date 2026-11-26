// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "blocklist.hpp"

#include "../utils.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <cerrno>
#include <charconv>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

struct AddressBlocklistResult
{
    int rc;
    std::set<size_t> list;
};

AddressBlocklistResult loadAddressBlocklist(const nlohmann::json& data)
{
    auto addrIt = data.find("addresses");
    if (addrIt == data.end())
    {
        return {0, std::set<size_t>()};
    }

    const auto* const addr =
        data["addresses"].get_ptr<const nlohmann::json::array_t*>();

    if (addr == nullptr)
    {
        lg2::error("addresses must be an array");
        return {EINVAL, std::set<size_t>()};
    }

    std::set<size_t> addressBlocklist = {};

    for (const auto& address : *addr)
    {
        const auto* addrS = address.get_ptr<const std::string*>();
        if (addrS == nullptr)
        {
            lg2::error("address must be a string\n");
            return {EINVAL, std::set<size_t>()};
        }

        if (!(addrS->starts_with("0x") || addrS->starts_with("0X")))
        {
            lg2::error("address must start with 0x or 0X\n");
            return {EINVAL, std::set<size_t>()};
        }

        // The alternative offered here relies on undefined behavior
        // of dereferencing iterators given by .end()
        // this pointer access is checked above by the calls to starts_with
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        size_t addressInt = 0;
        auto [ptr, ec] = std::from_chars(
            addrS->data() + 2, addrS->data() + addrS->length(), addressInt, 16);

        const auto erc = std::make_error_condition(ec);
        if (ptr != (addrS->data() + addrS->length()) || erc)
        {
            lg2::error("Invalid address type: {ADDR} {MSG}\n", "ADDR", *addrS,
                       "MSG", erc.message());
            return {EINVAL, std::set<size_t>()};
        }
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (addressInt > 0x77)
        {
            lg2::error("Invalid address {ADDR}\n", "ADDR", *addrS);
            return {EINVAL, std::set<size_t>()};
        }

        addressBlocklist.insert(addressInt);
    }

    return {0, addressBlocklist};
}

std::optional<Blocklist> loadBlocklist(const char* path)
{
    std::ifstream blocklistStream(path);
    if (!blocklistStream.good())
    {
        // File is optional.
        lg2::error("Cannot open blocklist file.\n");
        return Blocklist{};
    }

    nlohmann::json data =
        nlohmann::json::parse(blocklistStream, nullptr, false);
    if (data.is_discarded())
    {
        lg2::error("Illegal blocklist file detected, cannot validate JSON");
        return std::nullopt;
    }

    // It's expected to have at least one field, "buses" that is an array of the
    // buses by integer. Allow for future options to exclude further aspects,
    // such as specific addresses or ranges.
    if (data.type() != nlohmann::json::value_t::object)
    {
        lg2::error("Illegal blocklist, expected to read dictionary");
        return std::nullopt;
    }

    Blocklist blocklist;

    // If buses field is missing, that's fine.
    if (data.count("buses") == 1)
    {
        // Parse the buses array after a little validation.
        auto buses = data.at("buses");
        if (buses.type() != nlohmann::json::value_t::array)
        {
            // Buses field present but invalid, therefore this is an error.
            lg2::error("Invalid contents for blocklist buses field");
            return std::nullopt;
        }

        // Catch exception here for type mismatch.
        try
        {
            for (const auto& busIterator : buses)
            {
                // If bus and addresses field are missing, that's fine.
                if (busIterator.contains("bus") &&
                    busIterator.contains("addresses"))
                {
                    auto busData = busIterator.at("bus");
                    auto bus = busData.get<size_t>();

                    auto addressData = busIterator.at("addresses");
                    auto addresses =
                        addressData.get<std::set<std::string_view>>();

                    auto& block = blocklist.buses[bus].emplace();
                    for (const auto& address : addresses)
                    {
                        size_t addressInt = 0;
                        bool fullMatch = false;
                        fromCharsWrapper(address.substr(2), addressInt,
                                         fullMatch, 16);
                        block.insert(addressInt);
                    }
                }
                else
                {
                    blocklist.buses[busIterator.get<size_t>()] = std::nullopt;
                }
            }
        }
        catch (const nlohmann::detail::type_error& e)
        {
            // Type mismatch is a critical error.
            lg2::error("Invalid bus type: {ERR}", "ERR", e.what());
            return std::nullopt;
        }
    }

    auto [rc, addressBlocklist] = loadAddressBlocklist(data);
    if (rc != 0)
    {
        return std::nullopt;
    }

    blocklist.addresses = std::move(addressBlocklist);
    return blocklist;
}
