// Measures the UI-thread cost of one Processes tab tick: ProcessesView
// applying a snapshot's process list, then, when the tab is shown, a
// synchronous repaint of the view. It is built with the project but is not a
// test and is not registered with CTest. Run it in a Release build with
// .\scripts\build.ps1 -Benchmark; Debug Qt is many times slower. See DC-66 in
// docs/design_decisions.md, and docs/performance.md for the figures.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QLineEdit>
#include <QTreeView>
#include <spdlog/spdlog.h>

#include "benchmark_environment.h"
#include "domain/process_info.h"
#include "domain/system_snapshot.h"
#include "ui/process_model.h"
#include "ui/processes_view.h"

namespace
{

using sysmon::domain::ProcessInfo;
using sysmon::domain::SystemSnapshot;
using sysmon::ui::ProcessesView;
using Column = sysmon::ui::ProcessModel::Column;

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
// Ticks before timing starts, so the table has sorted and laid out once.
constexpr std::size_t kWarmUpTicks = 10;
// Timed ticks, alternating between a new list and a repeat of it, as the
// scheduler sends them: the list refreshes every 2 seconds, snapshots every second.
constexpr std::size_t kTimedTicks = 60;
constexpr std::array kProcessCounts{400, 1000, 2000};
// Share of processes whose CPU usage, memory, or I/O rates change between
// two refreshes, and how many processes exit and start in each.
constexpr double kChangingShare = 0.4;
constexpr int kExitsPerRefresh = 2;

struct Scenario
{
    const char* name;
    bool treeMode;
    Column sortColumn;
    Qt::SortOrder sortOrder;
    const char* filter;
};

constexpr std::array kScenarios{
    Scenario{"Flat, by name", false, Column::Name, Qt::AscendingOrder, ""},
    Scenario{"Flat, by CPU", false, Column::Cpu, Qt::DescendingOrder, ""},
    Scenario{"Tree, by name", true, Column::Name, Qt::AscendingOrder, ""},
    Scenario{"Tree, by CPU", true, Column::Cpu, Qt::DescendingOrder, ""},
    Scenario{"Tree, by CPU, filtered", true, Column::Cpu, Qt::DescendingOrder, "svchost"},
};

constexpr std::array kNames{"svchost.exe",       "svchost.exe", "svchost.exe",  "chrome.exe",  "chrome.exe",
                            "RuntimeBroker.exe", "conhost.exe", "explorer.exe", "dllhost.exe", "app.exe"};

// A deterministic stand-in for the process list: a forest a few levels deep in
// which some processes change their values, exit, and start at each refresh.
class ProcessSimulation
{
public:
    explicit ProcessSimulation(int count) : m_random(static_cast<unsigned>(count))
    {
        for (int index = 0; index < count; ++index) {
            start();
        }
    }

    // Returns the list one refresh later.
    const std::vector<ProcessInfo>& refresh()
    {
        for (int exit = 0; exit < kExitsPerRefresh; ++exit) {
            m_processes.erase(m_processes.begin() + static_cast<std::ptrdiff_t>(m_random() % m_processes.size()));
            start();
        }
        std::uniform_real_distribution<double> chance(0.0, 1.0);
        for (auto& process : m_processes) {
            if (chance(m_random) < kChangingShare) {
                process.cpuUsagePercent = static_cast<float>(m_random() % 300) / 100.0f;
                const uint64_t growth = m_random() % 65'536;
                process.workingSetBytes += growth;
                process.privateWorkingSetBytes += growth;
                process.ioReadBytesPerSec = m_random() % 2 == 0 ? 0 : m_random() % 1'000'000;
                process.ioWriteBytesPerSec = m_random() % 4'096;
            }
            if (chance(m_random) < kChangingShare / 10.0) {
                process.threadCount = 1 + m_random() % 64;
                process.handleCount = 50 + m_random() % 5'000;
            }
        }
        return m_processes;
    }

private:
    void start()
    {
        // A quarter of the processes have no listed parent; the others mostly
        // descend from recent processes, which gives chains a few levels deep.
        uint32_t parentPid = 0;
        if (!m_processes.empty() && m_random() % 4 != 0) {
            const std::size_t recent = std::min<std::size_t>(m_processes.size(), 40);
            parentPid = m_processes[m_processes.size() - 1 - m_random() % recent].pid;
        }
        ++m_clock;
        const uint64_t workingSet = 4'000'000 + m_random() % 200'000'000;
        m_processes.push_back(ProcessInfo{
            .pid = 4 * m_clock,
            .creationTime = std::chrono::system_clock::time_point{std::chrono::seconds{m_clock}},
            .parentPid = parentPid,
            .imageName = kNames[m_random() % kNames.size()],
            .cpuUsagePercent = 0.0f,
            .workingSetBytes = workingSet,
            .privateWorkingSetBytes = workingSet * 3 / 5,
            .ioReadBytesPerSec = 0,
            .ioWriteBytesPerSec = 0,
            .threadCount = 1 + m_random() % 64,
            .handleCount = 50 + m_random() % 5'000,
        });
    }

    std::mt19937 m_random;
    uint32_t m_clock{0};
    std::vector<ProcessInfo> m_processes;
};

struct TickTimes
{
    double refreshMeanMs{0.0};
    double repeatMeanMs{0.0};
    double maxMs{0.0};
};

TickTimes timeTicks(const Scenario& scenario, int processCount, bool isShown)
{
    ProcessesView view;
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(kWindowWidth, kWindowHeight);
    if (isShown) {
        view.show();
    }
    view.setTreeMode(scenario.treeMode);
    view.filterEdit()->setText(scenario.filter);
    view.treeView()->sortByColumn(static_cast<int>(scenario.sortColumn), scenario.sortOrder);

    ProcessSimulation simulation(processCount);
    SystemSnapshot snapshot;
    std::vector<double> refreshes;
    std::vector<double> repeats;
    for (std::size_t tick = 0; tick < kWarmUpTicks + kTimedTicks; ++tick) {
        const bool isRefresh = tick % 2 == 0;
        if (isRefresh) {
            snapshot.processes = simulation.refresh();
        }
        // One tick per second, so highlights end and exited processes leave the table.
        snapshot.timestamp = std::chrono::steady_clock::time_point{std::chrono::seconds{tick}};
        const auto start = std::chrono::steady_clock::now();
        view.updateSnapshot(snapshot);
        if (isShown) {
            static_cast<void>(view.grab());
        }
        const auto end = std::chrono::steady_clock::now();
        if (tick >= kWarmUpTicks) {
            (isRefresh ? refreshes : repeats).push_back(std::chrono::duration<double, std::milli>(end - start).count());
        }
        QCoreApplication::processEvents();
    }

    const auto mean = [](const std::vector<double>& values) {
        double total = 0.0;
        for (const double value : values) {
            total += value;
        }
        return total / static_cast<double>(values.size());
    };
    return TickTimes{
        .refreshMeanMs = mean(refreshes),
        .repeatMeanMs = mean(repeats),
        .maxMs = std::max(*std::ranges::max_element(refreshes), *std::ranges::max_element(repeats)),
    };
}

void printTable(bool isShown)
{
    spdlog::info("{}", isShown ? "Processes tab shown: the list applied, then the view repainted."
                               : "Processes tab hidden: the list applied only.");
    std::string header = "| Scenario |";
    std::string rule = "| --- |";
    for (const int count : kProcessCounts) {
        header += std::format(" {} processes |", count);
        rule += " --- |";
    }
    spdlog::info("{}", header);
    spdlog::info("{}", rule);
    for (const Scenario& scenario : kScenarios) {
        std::string row = std::string("| ") + scenario.name + " |";
        for (const int count : kProcessCounts) {
            const TickTimes times = timeTicks(scenario, count, isShown);
            row += std::format(" {:.2f} / {:.2f} ms ({:.1f}) |", times.refreshMeanMs, times.repeatMeanMs, times.maxMs);
        }
        spdlog::info("{}", row);
    }
    spdlog::info("");
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    spdlog::set_pattern("%v");

#ifndef NDEBUG
    spdlog::warn("Warning: this is a Debug build, whose Qt libraries are many times slower. "
                 "Run .\\scripts\\build.ps1 -Benchmark for representative numbers.\n");
#endif
    sysmon::benchmarks::logBenchmarkEnvironment();

    spdlog::info("Processes tab tick cost in a {}x{} window, as mean time per tick with a new process list",
                 kWindowWidth, kWindowHeight);
    spdlog::info("/ with the same list repeated (max of both), over {} ticks. Each refresh changes the values of",
                 kTimedTicks);
    spdlog::info("{:.0f}% of the processes, and {} processes exit and {} start.\n", kChangingShare * 100.0,
                 kExitsPerRefresh, kExitsPerRefresh);
    printTable(true);
    printTable(false);
    return 0;
}
