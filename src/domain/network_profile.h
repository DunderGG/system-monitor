#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sysmon::domain
{

/**
 * The network location category Windows gives a network, which selects its
 * firewall profile. Maps to NLM_NETWORK_CATEGORY.
 */
enum class NetworkCategory
{
    Public,
    Private,
    DomainAuthenticated,
};

/**
 * A network the system is connected to, as the Network List Manager knows it.
 *
 * name is the network's profile name as Windows Settings shows it, e.g.
 * "Home-Wi-Fi" or "Network 2". category is std::nullopt when it could not be
 * read. interfaceIds holds the NetworkSample::interfaceId of each adapter
 * connected to this network; it is empty when none could be matched.
 */
struct NetworkProfile
{
    std::string name{};
    std::optional<NetworkCategory> category{};
    std::vector<uint64_t> interfaceIds{};

    bool operator==(const NetworkProfile&) const = default;
};

} // namespace sysmon::domain
