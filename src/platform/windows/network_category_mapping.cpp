#include "platform/windows/network_category_mapping.h"

namespace sysmon::platform
{

std::optional<domain::NetworkCategory> toNetworkCategory(NLM_NETWORK_CATEGORY category) noexcept
{
    switch (category) {
        case NLM_NETWORK_CATEGORY_PUBLIC:
            return domain::NetworkCategory::Public;
        case NLM_NETWORK_CATEGORY_PRIVATE:
            return domain::NetworkCategory::Private;
        case NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED:
            return domain::NetworkCategory::DomainAuthenticated;
        default:
            return std::nullopt;
    }
}

} // namespace sysmon::platform
