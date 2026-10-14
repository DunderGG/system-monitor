#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QTreeView>

#include "domain/process_info.h"
#include "domain/system_snapshot.h"
#include "ui/process_model.h"
#include "ui/processes_view.h"

using namespace sysmon::domain;
using namespace sysmon::ui;

namespace
{

ProcessInfo process(uint32_t pid, std::string name)
{
    return ProcessInfo{
        .pid = pid,
        .creationTime = std::chrono::system_clock::time_point{std::chrono::seconds{pid}},
        .imageName = std::move(name),
    };
}

SystemSnapshot snapshotWith(std::vector<ProcessInfo> processes)
{
    SystemSnapshot snapshot;
    snapshot.processes = std::move(processes);
    return snapshot;
}

} // namespace

TEST(ProcessesView, Construction_TreeViewShowsTheModelAsAFlatTable)
{
    ProcessesView view;

    ASSERT_NE(view.treeView(), nullptr);
    EXPECT_EQ(view.treeView()->model(), view.model());
    EXPECT_FALSE(view.treeView()->rootIsDecorated());
    EXPECT_EQ(view.summaryText(), "Processes: --");
}

TEST(ProcessesView, UpdateSnapshot_WithProcessList_ShowsRowsAndCount)
{
    ProcessesView view;

    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}));

    EXPECT_EQ(view.model()->rowCount(), 2);
    EXPECT_EQ(view.summaryText(), "Processes: 2");
}

TEST(ProcessesView, UpdateSnapshot_WithoutProcessList_KeepsLastRows)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}));

    view.updateSnapshot(SystemSnapshot{});

    EXPECT_EQ(view.model()->rowCount(), 2);
    EXPECT_EQ(view.summaryText(), "Processes: N/A (showing the last list read)");
}

TEST(ProcessesView, UpdateSnapshot_EmptyList_RemovesRows)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWith({process(4, "System")}));

    view.updateSnapshot(snapshotWith({}));

    EXPECT_EQ(view.model()->rowCount(), 0);
    EXPECT_EQ(view.summaryText(), "Processes: 0");
}
