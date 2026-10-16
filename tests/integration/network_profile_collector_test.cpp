#include <chrono>
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

// Waits for the collector's first read, which runs on the collector's own thread.
bool waitForFirstRead(const NetworkProfileCollector& collector)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    while (collector.readCount() == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return collector.readCount() > 0;
}

// Creates a collector on a new thread, which may have a single-threaded
// apartment as a UI thread has, and returns its first read.
std::optional<std::vector<NetworkProfile>> collectFromCollectorCreatedOnNewThread(bool withSingleThreadedApartment)
{
    std::optional<std::vector<NetworkProfile>> profiles;
    std::jthread thread([&profiles, withSingleThreadedApartment] {
        std::optional<ScopedSingleThreadedApartment> apartment;
        if (withSingleThreadedApartment) {
            apartment.emplace();
        }
        NetworkProfileCollector collector;
        if (waitForFirstRead(collector)) {
            profiles = collector.collect();
        }
    });
    thread.join();
    return profiles;
}

} // namespace

TEST(NetworkProfileCollectorIntegration, RealHost_ConnectedNetworksNamedAndMatchedToListedAdapters)
{
    NetworkProfileCollector collector;
    ASSERT_TRUE(waitForFirstRead(collector));
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
    ASSERT_TRUE(waitForFirstRead(collector));

    const auto first = collector.collect();
    const auto second = collector.collect();

    EXPECT_EQ(first, second);
}

TEST(NetworkProfileCollectorIntegration, CreatedOnThreadWithSingleThreadedApartment_ReadsLikeAnyOtherCollector)
{
    // The collector reads on its own thread, in its own apartment, whatever
    // the apartment of the thread that creates it.
    const auto multithreaded = collectFromCollectorCreatedOnNewThread(false);
    const auto singleThreaded = collectFromCollectorCreatedOnNewThread(true);

    // The networks could change between the reads, but whether they can be read cannot.
    EXPECT_EQ(singleThreaded.has_value(), multithreaded.has_value());
}

TEST(NetworkProfileCollectorIntegration, RepeatedConstructionAndDestruction_StopsTheWorkerAndNotificationsCleanly)
{
    // Half of the collectors are destroyed while their first read may still be running.
    for (int i = 0; i < 20; ++i) {
        NetworkProfileCollector collector;
        if (i % 2 == 0) {
            EXPECT_TRUE(waitForFirstRead(collector));
        }
    }
    SUCCEED();
}
