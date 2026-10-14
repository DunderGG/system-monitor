#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QCheckBox>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QTreeView>

#include "domain/process_info.h"
#include "domain/system_snapshot.h"
#include "ui/process_model.h"
#include "ui/process_sort_filter_proxy_model.h"
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

TEST(ProcessesView, Construction_TreeViewShowsTheModelThroughTheProxyAsAFlatTable)
{
    ProcessesView view;

    ASSERT_NE(view.treeView(), nullptr);
    EXPECT_EQ(view.treeView()->model(), view.proxyModel());
    EXPECT_EQ(view.proxyModel()->sourceModel(), view.model());
    EXPECT_FALSE(view.treeView()->rootIsDecorated());
    EXPECT_EQ(view.summaryText(), "Processes: --");
}

TEST(ProcessesView, Construction_SortableAndSortedByName)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe"), process(12, "smss.exe")}));

    const auto* proxy = view.proxyModel();

    EXPECT_TRUE(view.treeView()->isSortingEnabled());
    EXPECT_EQ(proxy->sortColumn(), static_cast<int>(ProcessModel::Column::Name));
    EXPECT_EQ(proxy->sortOrder(), Qt::AscendingOrder);
    EXPECT_EQ(proxy->index(0, 0).data().toString(), "explorer.exe");
    EXPECT_EQ(proxy->index(2, 0).data().toString(), "System");
}

TEST(ProcessesView, FilterEdit_Text_FiltersRowsButNotTheCount)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe"), process(12, "smss.exe")}));

    view.filterEdit()->setText("EXPLO");

    ASSERT_EQ(view.proxyModel()->rowCount(), 1);
    EXPECT_EQ(view.proxyModel()->index(0, 0).data().toString(), "explorer.exe");
    EXPECT_EQ(view.summaryText(), "Processes: 3");

    view.filterEdit()->clear();
    EXPECT_EQ(view.proxyModel()->rowCount(), 3);
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

namespace
{

ProcessInfo childOf(uint32_t parentPid, uint32_t pid, std::string name)
{
    ProcessInfo info = process(pid, std::move(name));
    info.parentPid = parentPid;
    return info;
}

} // namespace

TEST(ProcessesView, SetTreeMode_ShowsBranchesExpanded)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWith({process(4, "System"), childOf(4, 100, "smss.exe")}));

    view.setTreeMode(true);

    EXPECT_TRUE(view.isTreeMode());
    EXPECT_TRUE(view.model()->isTreeMode());
    EXPECT_TRUE(view.treeModeCheckBox()->isChecked());
    EXPECT_TRUE(view.treeView()->rootIsDecorated());
    ASSERT_EQ(view.proxyModel()->rowCount(), 1);
    const QModelIndex system = view.proxyModel()->index(0, 0);
    EXPECT_EQ(view.proxyModel()->rowCount(system), 1);
    EXPECT_TRUE(view.treeView()->isExpanded(system));
}

TEST(ProcessesView, TreeModeCheckBox_Unchecked_ShowsFlatListAgain)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWith({process(4, "System"), childOf(4, 100, "smss.exe")}));
    view.treeModeCheckBox()->setChecked(true);

    view.treeModeCheckBox()->setChecked(false);

    EXPECT_FALSE(view.isTreeMode());
    EXPECT_FALSE(view.treeView()->rootIsDecorated());
    EXPECT_EQ(view.proxyModel()->rowCount(), 2);
}

TEST(ProcessesView, SetTreeMode_SelectedProcessStaysSelected)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWith({process(4, "System"), childOf(4, 100, "smss.exe")}));
    const auto smssIdentity = childOf(4, 100, "smss.exe").identity();
    view.treeView()->setCurrentIndex(view.proxyModel()->mapFromSource(view.model()->indexOf(smssIdentity)));

    view.setTreeMode(true);

    const QModelIndex current = view.proxyModel()->mapToSource(view.treeView()->currentIndex());
    ASSERT_NE(view.model()->process(current), nullptr);
    EXPECT_EQ(view.model()->process(current)->identity(), smssIdentity);
    EXPECT_TRUE(view.treeView()->selectionModel()->isSelected(view.treeView()->currentIndex()));
}

TEST(ProcessesView, TreeMode_ProcessGainsFirstChild_Expanded)
{
    ProcessesView view;
    view.setTreeMode(true);
    view.updateSnapshot(snapshotWith({process(400, "services.exe")}));

    view.updateSnapshot(snapshotWith({process(400, "services.exe"), childOf(400, 500, "svchost.exe")}));

    EXPECT_TRUE(view.treeView()->isExpanded(view.proxyModel()->index(0, 0)));
}

TEST(ProcessesView, TreeMode_CollapsedBranch_StaysCollapsedWhenChildrenArrive)
{
    ProcessesView view;
    view.setTreeMode(true);
    view.updateSnapshot(snapshotWith({process(400, "services.exe"), childOf(400, 500, "svchost.exe")}));
    view.treeView()->collapse(view.proxyModel()->index(0, 0));

    view.updateSnapshot(snapshotWith(
        {process(400, "services.exe"), childOf(400, 500, "svchost.exe"), childOf(400, 600, "spoolsv.exe")}));

    EXPECT_FALSE(view.treeView()->isExpanded(view.proxyModel()->index(0, 0)));
}
