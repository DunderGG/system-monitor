#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QAbstractItemModel>
#include <QObject>
#include <QString>

#include "domain/process_info.h"
#include "ui/process_model.h"
#include "ui/process_sort_filter_proxy_model.h"

using namespace sysmon::domain;
using sysmon::ui::ProcessModel;
using sysmon::ui::ProcessSortFilterProxyModel;

namespace
{

using Column = ProcessModel::Column;

// The monotonic time of each refresh, unless a test moves it on.
const auto kNow = std::chrono::steady_clock::time_point{std::chrono::seconds{100}};

ProcessInfo process(uint32_t pid, std::string name, std::optional<float> cpu = std::nullopt)
{
    return ProcessInfo{
        .pid = pid,
        .creationTime = std::chrono::system_clock::time_point{std::chrono::seconds{pid}},
        .imageName = std::move(name),
        .cpuUsagePercent = cpu,
    };
}

// A ProcessModel shown through a proxy.
struct SortedModel
{
    ProcessModel model;
    ProcessSortFilterProxyModel proxy;

    explicit SortedModel(const std::vector<ProcessInfo>& processes)
    {
        model.updateProcesses(processes, kNow);
        proxy.setSourceModel(&model);
    }

    // Returns the PIDs in the proxy's rows, top to bottom.
    [[nodiscard]] std::vector<uint32_t> pids() const
    {
        std::vector<uint32_t> result;
        for (int row = 0; row < proxy.rowCount(); ++row) {
            result.push_back(proxy.index(row, static_cast<int>(Column::Pid)).data().toUInt());
        }
        return result;
    }

    void sort(Column column, Qt::SortOrder order)
    {
        proxy.sort(static_cast<int>(column), order);
    }
};

} // namespace

TEST(ProcessSortFilterProxyModel, Sort_Cpu_ByValueNotText)
{
    SortedModel sorted({process(1, "a.exe", 10.0f), process(2, "b.exe", 9.0f), process(3, "c.exe", 100.0f)});

    sorted.sort(Column::Cpu, Qt::DescendingOrder);

    // As text, "9.0%" would sort above "10.0%" and "100.0%".
    EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{3, 1, 2}));
}

TEST(ProcessSortFilterProxyModel, Sort_MissingValue_BelowEveryValue)
{
    SortedModel sorted({process(1, "a.exe", std::nullopt), process(2, "b.exe", 0.0f), process(3, "c.exe", 5.0f)});

    sorted.sort(Column::Cpu, Qt::DescendingOrder);
    EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{3, 2, 1}));

    sorted.sort(Column::Cpu, Qt::AscendingOrder);
    EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{1, 2, 3}));
}

TEST(ProcessSortFilterProxyModel, Sort_EqualValues_FallBackToNameThenPid)
{
    SortedModel sorted(
        {process(30, "svchost.exe", 0.0f), process(10, "svchost.exe", 0.0f), process(20, "audiodg.exe", 0.0f)});

    sorted.sort(Column::Cpu, Qt::AscendingOrder);

    EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{20, 10, 30}));
}

TEST(ProcessSortFilterProxyModel, Sort_Name_IgnoresCaseAndNamesTheIdleProcess)
{
    SortedModel sorted({process(1, "zeta.exe"), process(2, "Alpha.exe"), process(0, ""), process(3, "beta.exe"),
                        process(4, "Taskmgr.exe")});

    sorted.sort(Column::Name, Qt::AscendingOrder);

    // The Idle process sorts as "System Idle Process".
    EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{2, 3, 0, 4, 1}));
}

TEST(ProcessSortFilterProxyModel, Sort_CountsAndBytes_ByValue)
{
    ProcessInfo small = process(1, "small.exe");
    small.workingSetBytes = 900;
    small.threadCount = 9;
    small.handleCount = 900;
    small.ioReadBytesPerSec = 900;
    small.ioWriteBytesPerSec = 0;
    ProcessInfo large = process(2, "large.exe");
    large.workingSetBytes = 1000;
    large.threadCount = 10;
    large.handleCount = 1000;
    large.ioReadBytesPerSec = 500;
    large.ioWriteBytesPerSec = 500;
    SortedModel sorted({small, large});

    for (const Column column : {Column::Memory, Column::Io, Column::Threads, Column::Handles}) {
        sorted.sort(column, Qt::DescendingOrder);
        EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{2, 1})) << static_cast<int>(column);
    }
    sorted.sort(Column::Pid, Qt::DescendingOrder);
    EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{2, 1}));
}

TEST(ProcessSortFilterProxyModel, Sort_ValueChanges_Resorts)
{
    SortedModel sorted({process(1, "a.exe", 50.0f), process(2, "b.exe", 10.0f)});
    sorted.sort(Column::Cpu, Qt::DescendingOrder);

    sorted.model.updateProcesses({process(1, "a.exe", 5.0f), process(2, "b.exe", 60.0f)}, kNow);

    EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{2, 1}));
}

TEST(ProcessSortFilterProxyModel, Filter_Name_SubstringIgnoringCase)
{
    SortedModel sorted({process(1, "explorer.exe"), process(2, "svchost.exe"), process(3, "Explorer2.exe")});
    sorted.sort(Column::Pid, Qt::AscendingOrder);

    sorted.proxy.setFilterFixedString("EXPLORER");

    EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{1, 3}));
}

TEST(ProcessSortFilterProxyModel, Filter_Pid_Substring)
{
    SortedModel sorted({process(1234, "a.exe"), process(5123, "b.exe"), process(42, "c.exe")});
    sorted.sort(Column::Pid, Qt::AscendingOrder);

    sorted.proxy.setFilterFixedString("123");

    EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{1234, 5123}));
}

TEST(ProcessSortFilterProxyModel, Filter_TextIsLiteral_NotARegularExpression)
{
    SortedModel sorted({process(1, "a.exe"), process(2, "abexe")});

    sorted.proxy.setFilterFixedString(".exe");

    EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{1}));
}

TEST(ProcessSortFilterProxyModel, Filter_Cleared_ShowsEveryRow)
{
    SortedModel sorted({process(1, "a.exe"), process(2, "b.exe")});
    sorted.proxy.setFilterFixedString("a.exe");

    sorted.proxy.setFilterFixedString("");

    EXPECT_EQ(sorted.proxy.rowCount(), 2);
}

TEST(ProcessSortFilterProxyModel, Filter_NewMatchingProcess_Appears)
{
    SortedModel sorted({process(1, "a.exe")});
    sorted.proxy.setFilterFixedString("notepad");

    sorted.model.updateProcesses({process(1, "a.exe"), process(2, "notepad.exe")}, kNow);

    EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{2}));
}

TEST(ProcessSortFilterProxyModel, Filter_TreeMode_KeepsAncestorsOfMatches)
{
    ProcessModel model;
    model.setTreeMode(true);
    ProcessInfo services = process(400, "services.exe");
    ProcessInfo svchost = process(500, "svchost.exe");
    svchost.parentPid = 400;
    ProcessInfo spoolsv = process(600, "spoolsv.exe");
    spoolsv.parentPid = 400;
    model.updateProcesses({services, svchost, spoolsv, process(700, "other.exe")}, kNow);
    ProcessSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    proxy.setFilterFixedString("svchost");

    ASSERT_EQ(proxy.rowCount(), 1);
    const QModelIndex parent = proxy.index(0, 0);
    EXPECT_EQ(parent.data().toString(), "services.exe");
    ASSERT_EQ(proxy.rowCount(parent), 1);
    EXPECT_EQ(proxy.index(0, 0, parent).data().toString(), "svchost.exe");
}

TEST(ProcessSortFilterProxyModel, Sort_TreeMode_SortsChildrenAmongThemselves)
{
    ProcessModel model;
    model.setTreeMode(true);
    ProcessInfo zeta = process(500, "zeta.exe");
    zeta.parentPid = 400;
    ProcessInfo alpha = process(600, "alpha.exe");
    alpha.parentPid = 400;
    model.updateProcesses({process(400, "services.exe"), zeta, alpha, process(100, "aaa.exe")}, kNow);
    ProcessSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);

    proxy.sort(static_cast<int>(Column::Name), Qt::AscendingOrder);

    ASSERT_EQ(proxy.rowCount(), 2);
    EXPECT_EQ(proxy.index(0, 0).data().toString(), "aaa.exe");
    const QModelIndex services = proxy.index(1, 0);
    EXPECT_EQ(proxy.index(0, 0, services).data().toString(), "alpha.exe");
    EXPECT_EQ(proxy.index(1, 0, services).data().toString(), "zeta.exe");
}

TEST(ProcessSortFilterProxyModel, Filter_TreeMode_ValueChanges_KeepMatchesAndResort)
{
    ProcessModel model;
    model.setTreeMode(true);
    ProcessInfo first = process(500, "svchost.exe", 1.0f);
    first.parentPid = 400;
    ProcessInfo second = process(600, "svchost.exe", 2.0f);
    second.parentPid = 400;
    model.updateProcesses({process(400, "services.exe"), first, second, process(700, "other.exe")}, kNow);
    ProcessSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.sort(static_cast<int>(Column::Cpu), Qt::DescendingOrder);
    proxy.setFilterFixedString("svchost");

    first.cpuUsagePercent = 9.0f;
    model.updateProcesses({process(400, "services.exe"), first, second, process(700, "other.exe")}, kNow);

    ASSERT_EQ(proxy.rowCount(), 1);
    const QModelIndex services = proxy.index(0, 0);
    ASSERT_EQ(proxy.rowCount(services), 2);
    EXPECT_EQ(proxy.index(0, static_cast<int>(Column::Pid), services).data().toString(), "500");
    EXPECT_EQ(proxy.index(1, static_cast<int>(Column::Pid), services).data().toString(), "600");
}

namespace
{

// Two parents with two children each, all at the given CPU usages in order:
// services (400) over 500 and 600, and explorer (700) over 800 and 900.
std::vector<ProcessInfo> twoFamilies(float a, float b, float c, float d)
{
    ProcessInfo first = process(500, "first.exe", a);
    first.parentPid = 400;
    ProcessInfo second = process(600, "second.exe", b);
    second.parentPid = 400;
    ProcessInfo third = process(800, "third.exe", c);
    third.parentPid = 700;
    ProcessInfo fourth = process(900, "fourth.exe", d);
    fourth.parentPid = 700;
    return {process(400, "services.exe", 0.0f), first, second, process(700, "explorer.exe", 0.0f), third, fourth};
}

std::vector<uint32_t> childPids(const QAbstractItemModel& proxy, const QModelIndex& parent)
{
    std::vector<uint32_t> pids;
    for (int row = 0; row < proxy.rowCount(parent); ++row) {
        pids.push_back(proxy.index(row, static_cast<int>(Column::Pid), parent).data().toUInt());
    }
    return pids;
}

} // namespace

TEST(ProcessSortFilterProxyModel, Sort_SortedValuesChangeUnderSeveralParents_SortedOnceAndCorrectly)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses(twoFamilies(1.0f, 2.0f, 3.0f, 4.0f), kNow);
    ProcessSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.sort(static_cast<int>(Column::Cpu), Qt::DescendingOrder);
    int layoutChanges = 0;
    QObject::connect(&proxy, &QAbstractItemModel::layoutChanged, &proxy, [&layoutChanges] { ++layoutChanges; });

    model.updateProcesses(twoFamilies(9.0f, 2.0f, 3.0f, 8.0f), kNow);

    EXPECT_EQ(layoutChanges, 1);
    EXPECT_TRUE(proxy.dynamicSortFilter());
    ASSERT_EQ(proxy.rowCount(), 2);
    const QModelIndex services =
        proxy.index(0, 0).data().toString() == "services.exe" ? proxy.index(0, 0) : proxy.index(1, 0);
    const QModelIndex explorer =
        proxy.index(0, 0).data().toString() == "explorer.exe" ? proxy.index(0, 0) : proxy.index(1, 0);
    EXPECT_EQ(childPids(proxy, services), (std::vector<uint32_t>{500, 600}));
    EXPECT_EQ(childPids(proxy, explorer), (std::vector<uint32_t>{900, 800}));
}

TEST(ProcessSortFilterProxyModel, Sort_OtherColumnsChange_NoResort)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses(twoFamilies(1.0f, 2.0f, 3.0f, 4.0f), kNow);
    ProcessSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.sort(static_cast<int>(Column::Cpu), Qt::DescendingOrder);
    int layoutChanges = 0;
    QObject::connect(&proxy, &QAbstractItemModel::layoutChanged, &proxy, [&layoutChanges] { ++layoutChanges; });

    std::vector<ProcessInfo> processes = twoFamilies(1.0f, 2.0f, 3.0f, 4.0f);
    for (auto& info : processes) {
        info.threadCount = 7;
    }
    model.updateProcesses(processes, kNow);

    EXPECT_EQ(layoutChanges, 0);
    EXPECT_TRUE(proxy.dynamicSortFilter());
}

TEST(ProcessSortFilterProxyModel, Sort_NewProcessesAfterDeferredSort_InsertedInOrder)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses(twoFamilies(1.0f, 2.0f, 3.0f, 4.0f), kNow);
    ProcessSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.sort(static_cast<int>(Column::Cpu), Qt::DescendingOrder);

    std::vector<ProcessInfo> processes = twoFamilies(9.0f, 2.0f, 3.0f, 8.0f);
    ProcessInfo newcomer = process(950, "newcomer.exe", 5.0f);
    newcomer.parentPid = 700;
    processes.push_back(newcomer);
    model.updateProcesses(processes, kNow);

    const QModelIndex explorer =
        proxy.index(0, 0).data().toString() == "explorer.exe" ? proxy.index(0, 0) : proxy.index(1, 0);
    EXPECT_EQ(childPids(proxy, explorer), (std::vector<uint32_t>{900, 950, 800}));
}
