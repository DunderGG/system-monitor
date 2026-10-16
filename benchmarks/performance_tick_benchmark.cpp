// Measures the UI-thread cost of one Performance tab tick, and of one Network
// tab tick: MainWindow forwarding a snapshot, then a synchronous repaint of the
// whole window. It is built with the project but is not a test and is not
// registered with CTest. Run it in a Release build with
// .\scripts\build.ps1 -Benchmark; Debug Qt paints many times slower. See DC-66
// and DC-98 in docs/design_decisions.md.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QTabWidget>
#include <spdlog/spdlog.h>

#include "benchmark_environment.h"
#include "domain/system_snapshot.h"
#include "ui/dashboard_view.h"
#include "ui/main_window.h"
#include "ui/network_view.h"
#include "ui/performance_page.h"
#include "ui/performance_view.h"

namespace
{

using sysmon::domain::AdapterType;
using sysmon::domain::CpuSample;
using sysmon::domain::DiskSample;
using sysmon::domain::MemorySample;
using sysmon::domain::NetworkSample;
using sysmon::domain::OperationalStatus;
using sysmon::domain::SystemActivitySample;
using sysmon::domain::SystemSnapshot;
using sysmon::ui::MainWindow;
using sysmon::ui::NetworkView;
using sysmon::ui::PerformancePage;
using sysmon::ui::PerformanceView;

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
constexpr std::size_t kTimedTicks = 20;
constexpr std::size_t kVolumeCount = 3;
// Adapters in the Performance tab's snapshots, and the counts the Network tab is timed with.
constexpr std::size_t kAdapterCount = 2;
constexpr std::array<std::size_t, 3> kNetworkTabAdapterCounts{2, 8, 16};
constexpr int kNetworkTabCoreCount = 16;
constexpr uint64_t kGiB = 1024ULL * 1024ULL * 1024ULL;
constexpr double kPeakBytesPerSecond = 100.0 * 1024.0 * 1024.0;

// Offsets that give every series of a snapshot its own swing() index.
constexpr std::size_t kCoreSeries = 1;
constexpr std::size_t kMemorySeries = 1000;
constexpr std::size_t kDiskSeries = 2000;
constexpr std::size_t kNetworkSeries = 3000;

struct Scenario
{
    const char* name;
    PerformanceView::Page page;
    int coreCount;
};

constexpr std::array kScenarios{
    Scenario{.name = "CPU, 16 cores", .page = PerformanceView::Page::Cpu, .coreCount = 16},
    Scenario{.name = "CPU, 64 cores", .page = PerformanceView::Page::Cpu, .coreCount = 64},
    Scenario{.name = "Memory", .page = PerformanceView::Page::Memory, .coreCount = 16},
    Scenario{.name = "Disk, 3 volumes", .page = PerformanceView::Page::Disk, .coreCount = 16},
    Scenario{.name = "Network, 2 adapters", .page = PerformanceView::Page::Network, .coreCount = 16},
};

struct TickTimes
{
    double meanMs{0.0};
    double maxMs{0.0};
};

// The worst case for painting: a fraction between 0.05 and 0.95 that swings
// across most of its range with a period of 20 to 30 samples, different for
// each series, so no line is ever flat and a dense chart keeps two points in
// every pixel column.
double swing(std::size_t tick, std::size_t series)
{
    const double period = 20.0 + static_cast<double>(series % 11);
    const double phase = static_cast<double>(series) * 3.0;
    return 0.5 + 0.45 * std::sin(2.0 * std::numbers::pi * (static_cast<double>(tick) + phase) / period);
}

float swingPercent(std::size_t tick, std::size_t series)
{
    return static_cast<float>(100.0 * swing(tick, series));
}

uint64_t swingBytes(uint64_t total, std::size_t tick, std::size_t series)
{
    return static_cast<uint64_t>(static_cast<double>(total) * swing(tick, series));
}

NetworkSample makeAdapter(std::size_t tick, std::size_t adapter)
{
    const std::string name = adapter == 0 ? "Ethernet" : (adapter == 1 ? "Wi-Fi" : std::format("Ethernet {}", adapter));
    const std::size_t series = kNetworkSeries + 2 * adapter;
    return NetworkSample{
        .interfaceId = adapter + 1,
        .adapterName = name,
        .friendlyName = name,
        .description = name + " Controller",
        .inBytesTotal = static_cast<uint64_t>(tick) * 50'000'000ULL,
        .outBytesTotal = static_cast<uint64_t>(tick) * 5'000'000ULL,
        .inBytesPerSec = static_cast<uint64_t>(kPeakBytesPerSecond * swing(tick, series)),
        .outBytesPerSec = static_cast<uint64_t>(kPeakBytesPerSecond * swing(tick, series + 1)),
        .linkSpeedBps = 2'500'000'000ULL,
        .operationalStatus = OperationalStatus::Up,
        .type = adapter == 1 ? AdapterType::WiFi : AdapterType::Ethernet,
        .ipAddresses = {std::format("192.168.1.{}", 20 + adapter), "fe80::4a1c:9d2e:77b0:1f3a"},
        .dnsServers = {"192.168.1.1"},
        .isHardwareInterface = true,
        .hasIpInterface = true,
    };
}

SystemSnapshot makeSnapshot(std::size_t tick, int coreCount, std::size_t adapterCount)
{
    SystemSnapshot snapshot;
    snapshot.timestamp = std::chrono::steady_clock::time_point{std::chrono::seconds{tick}};

    CpuSample cpu{.totalUsagePercent = swingPercent(tick, 0), .coreCount = coreCount, .baseSpeedMhz = 4700};
    for (int core = 0; core < coreCount; ++core) {
        cpu.coreUsagePercents.push_back(swingPercent(tick, kCoreSeries + static_cast<std::size_t>(core)));
    }
    snapshot.cpu = std::move(cpu);

    const uint64_t totalMemory = 32 * kGiB;
    const uint64_t usedMemory = swingBytes(totalMemory, tick, kMemorySeries);
    snapshot.memory = MemorySample{
        .totalBytes = totalMemory,
        .availableBytes = totalMemory - usedMemory,
        .usagePercent = static_cast<float>(100.0 * static_cast<double>(usedMemory) / static_cast<double>(totalMemory)),
        .commitLimit = 48 * kGiB,
        .commitCurrent = swingBytes(48 * kGiB, tick, kMemorySeries + 1),
        .systemCacheBytes = 8 * kGiB,
        .pagedPoolBytes = kGiB,
        .nonPagedPoolBytes = kGiB / 2,
    };

    std::vector<DiskSample> disks;
    for (std::size_t volume = 0; volume < kVolumeCount; ++volume) {
        const uint64_t total = 1000 * kGiB;
        const uint64_t used = swingBytes(total, tick, kDiskSeries + volume);
        disks.push_back(DiskSample{
            .volumeName = std::string(1, static_cast<char>('C' + volume)) + ":\\",
            .totalBytes = total,
            .freeBytes = total - used,
            .usagePercent = static_cast<float>(100.0 * static_cast<double>(used) / static_cast<double>(total)),
        });
    }
    snapshot.disks = std::move(disks);

    std::vector<NetworkSample> networks;
    for (std::size_t adapter = 0; adapter < adapterCount; ++adapter) {
        networks.push_back(makeAdapter(tick, adapter));
    }
    snapshot.networks = std::move(networks);

    snapshot.activity = SystemActivitySample{.processCount = 350, .threadCount = 5'000, .handleCount = 200'000};
    snapshot.uptime = std::chrono::milliseconds{std::chrono::seconds{tick}};
    return snapshot;
}

// Times kTimedTicks ticks, each a forwarded snapshot followed by a repaint of
// the whole window. Snapshots are built before the clock starts.
TickTimes timeTicks(MainWindow& window, std::size_t& tick, int coreCount, std::size_t adapterCount)
{
    std::vector<double> durations;
    for (std::size_t index = 0; index < kTimedTicks; ++index) {
        const SystemSnapshot snapshot = makeSnapshot(tick++, coreCount, adapterCount);
        const auto start = std::chrono::steady_clock::now();
        window.onSnapshotReady(snapshot);
        static_cast<void>(window.grab());
        const auto end = std::chrono::steady_clock::now();
        durations.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }
    double total = 0.0;
    for (const double duration : durations) {
        total += duration;
    }
    return TickTimes{
        .meanMs = total / static_cast<double>(durations.size()),
        .maxMs = *std::ranges::max_element(durations),
    };
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    spdlog::set_pattern("%v");

#ifndef NDEBUG
    spdlog::warn("Warning: this is a Debug build, whose Qt libraries paint many times slower. "
                 "Run .\\scripts\\build.ps1 -Benchmark for representative numbers.\n");
#endif
    sysmon::benchmarks::logBenchmarkEnvironment();

    spdlog::info("Performance tab tick cost: a forwarded snapshot plus a repaint of a {}x{} window,", kWindowWidth,
                 kWindowHeight);
    spdlog::info("mean (max) of {} ticks after {} recorded snapshots of worst-case data.\n", kTimedTicks,
                 PerformancePage::kHistoryCapacity);
    spdlog::info("| Page | 1 min | 5 min | 30 min |");
    spdlog::info("| --- | --- | --- | --- |");

    for (const Scenario& scenario : kScenarios) {
        MainWindow window;
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(kWindowWidth, kWindowHeight);
        window.show();

        // Record the full history while the Performance tab is hidden: hidden
        // views record without refreshing (DC-54), so this part is quick.
        window.tabWidget()->setCurrentWidget(window.dashboardView());
        std::size_t tick = 0;
        for (; tick < PerformancePage::kHistoryCapacity; ++tick) {
            window.onSnapshotReady(makeSnapshot(tick, scenario.coreCount, kAdapterCount));
        }

        auto* view = window.performanceView();
        window.tabWidget()->setCurrentWidget(view);
        view->setCurrentPage(scenario.page);

        std::string row = std::string("| ") + scenario.name + " |";
        for (const std::size_t historyWindow : PerformanceView::kHistoryWindows) {
            view->setHistoryWindow(historyWindow);
            QCoreApplication::processEvents();
            const TickTimes times = timeTicks(window, tick, scenario.coreCount, kAdapterCount);
            row += std::format(" {:.1f} ms ({:.1f}) |", times.meanMs, times.maxMs);
        }
        spdlog::info("{}", row);
    }

    spdlog::info("\nNetwork tab tick cost: the same, with the Network tab shown and {} samples per adapter chart,",
                 NetworkView::kHistoryCapacity);
    spdlog::info("for adapters that are all listed and Up.\n");
    spdlog::info("| Adapters | Tick |");
    spdlog::info("| --- | --- |");

    for (const std::size_t adapterCount : kNetworkTabAdapterCounts) {
        MainWindow window;
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(kWindowWidth, kWindowHeight);
        window.show();
        window.tabWidget()->setCurrentWidget(window.networkView());

        std::size_t tick = 0;
        for (; tick < NetworkView::kHistoryCapacity; ++tick) {
            window.onSnapshotReady(makeSnapshot(tick, kNetworkTabCoreCount, adapterCount));
        }
        QCoreApplication::processEvents();
        const TickTimes times = timeTicks(window, tick, kNetworkTabCoreCount, adapterCount);
        spdlog::info("| {} | {:.1f} ms ({:.1f}) |", adapterCount, times.meanMs, times.maxMs);
    }
    return 0;
}
