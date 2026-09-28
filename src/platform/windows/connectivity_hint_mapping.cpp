#include "platform/windows/connectivity_hint_mapping.h"

namespace sysmon::platform
{

domain::ConnectivityLevel toConnectivityLevel(NL_NETWORK_CONNECTIVITY_LEVEL_HINT level) noexcept
{
    switch (level) {
        case NetworkConnectivityLevelHintNone:
            return domain::ConnectivityLevel::None;
        case NetworkConnectivityLevelHintLocalAccess:
            return domain::ConnectivityLevel::LocalAccess;
        case NetworkConnectivityLevelHintInternetAccess:
            return domain::ConnectivityLevel::InternetAccess;
        case NetworkConnectivityLevelHintConstrainedInternetAccess:
            return domain::ConnectivityLevel::ConstrainedInternetAccess;
        case NetworkConnectivityLevelHintUnknown:
        case NetworkConnectivityLevelHintHidden:
        default:
            return domain::ConnectivityLevel::Unknown;
    }
}

std::optional<bool> toIsMetered(NL_NETWORK_CONNECTIVITY_COST_HINT cost) noexcept
{
    switch (cost) {
        case NetworkConnectivityCostHintUnrestricted:
            return false;
        case NetworkConnectivityCostHintFixed:
        case NetworkConnectivityCostHintVariable:
            return true;
        case NetworkConnectivityCostHintUnknown:
        default:
            return std::nullopt;
    }
}

domain::ConnectivityStatus toConnectivityStatus(const NL_NETWORK_CONNECTIVITY_HINT &hint) noexcept
{
    return domain::ConnectivityStatus{
        .level = toConnectivityLevel(hint.ConnectivityLevel),
        .isMetered = toIsMetered(hint.ConnectivityCost),
    };
}

} // namespace sysmon::platform
