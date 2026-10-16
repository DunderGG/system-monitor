#include <cstdint>
#include <optional>
#include <set>
#include <thread>
#include <vector>

#include <windows.h>
#include <objbase.h>

#include <gtest/gtest.h>

#include "domain/network_profile.h"
#include "platform/windows/network_collector.h"
#include "platform/windows/network_profile_collector.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

// These tests tolerate any host state: the host may be connected to any number
// of networks, including none, and the Network List Manager may be unavailable.

namespace
{

// Gives the current thread a single-threaded apartment, as a UI thread has.
class ScopedSingleThreadedApartment
{
public:
    ScopedSingleThreadedApartment() : m_result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}

    ~ScopedSingleThreadedApartment()
    {
        if (SUCCEEDED(m_result)) {
            CoUninitialize();
        }
    }

    ScopedSingleThreadedApartment(const ScopedSingleThreadedApartment&) = delete;
    ScopedSingleThreadedApartment& operator=(const ScopedSingleThreadedApartment&) = delete;

private:
    HRESULT m_result;
};

std::optional<std::vector<NetworkProfile>> collectOnNewThread(bool withSingleThreadedApartment)
{
    std::optional<std::vector<NetworkProfile>> profiles;
    std::jthread thread([&profiles, withSingleThreadedApartment] {
        std::optional<ScopedSingleThreadedApartment> apartment;
        if (withSingleThreadedApartment) {
            apartment.emplace();
        }
        NetworkProfileCollector collector;
        profiles = collector.collect();
    });
    thread.join();
    return profiles;
}

} // namespace

TEST(NetworkProfileCollectorIntegration, RealHost_ConnectedNetworksNamedAndMatchedToListedAdapters)
{
    NetworkProfileCollector collector;
    const auto profiles = collector.collect();
    if (!profiles) {
        GTEST_SKIP() << "The Network List Manager is unavailable on this host";
    }
    NetworkCollector networkCollector;
    const auto adapters = networkCollector.collect();
    ASSERT_TRUE(adapters.has_value());
    std::set<uint64_t> adapterIds;
    for (const auto& adapter : *adapters) {
        adapterIds.insert(adapter.interfaceId);
    }

    for (const auto& profile : *profiles) {
        EXPECT_FALSE(profile.name.empty());
        // A connected network is reached through at least one adapter.
        EXPECT_FALSE(profile.interfaceIds.empty()) << profile.name;
        for (const uint64_t interfaceId : profile.interfaceIds) {
            EXPECT_TRUE(adapterIds.contains(interfaceId)) << profile.name << " on interface " << interfaceId;
        }
    }
}

TEST(NetworkProfileCollectorIntegration, ConsecutiveCollects_ReturnTheCachedProfiles)
{
    NetworkProfileCollector collector;

    const auto first = collector.collect();
    const auto second = collector.collect();

    EXPECT_EQ(first, second);
}

TEST(NetworkProfileCollectorIntegration, ThreadWithSingleThreadedApartment_ReadsLikeAnyOtherThread)
{
    const auto multithreaded = collectOnNewThread(false);
    const auto singleThreaded = collectOnNewThread(true);

    // The networks could change between the reads, but whether they can be read cannot.
    EXPECT_EQ(singleThreaded.has_value(), multithreaded.has_value());
}

TEST(NetworkProfileCollectorIntegration, RepeatedConstructionAndDestruction_CancelsNotificationsCleanly)
{
    for (int i = 0; i < 20; ++i) {
        NetworkProfileCollector collector;
        static_cast<void>(collector.collect());
    }
    SUCCEED();
}
