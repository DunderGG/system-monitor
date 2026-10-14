#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
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
        model.updateProcesses(processes);
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

    sorted.model.updateProcesses({process(1, "a.exe", 5.0f), process(2, "b.exe", 60.0f)});

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

    sorted.model.updateProcesses({process(1, "a.exe"), process(2, "notepad.exe")});

    EXPECT_EQ(sorted.pids(), (std::vector<uint32_t>{2}));
}
