#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QAbstractButton>
#include <QAction>
#include <QCheckBox>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeView>

#include "domain/process_identity.h"
#include "domain/process_info.h"
#include "domain/process_termination.h"
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

SystemSnapshot snapshotWith(std::vector<ProcessInfo> processes, int atSecond = 0)
{
    SystemSnapshot snapshot;
    snapshot.timestamp = std::chrono::steady_clock::time_point{std::chrono::seconds{atSecond}};
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

TEST(ProcessesView, UpdateSnapshot_WithoutProcessList_StillEndsHighlights)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}, 0));
    view.updateSnapshot(snapshotWith({process(4, "System")}, 1));

    SystemSnapshot withoutList;
    withoutList.timestamp = std::chrono::steady_clock::time_point{std::chrono::seconds{10}};
    view.updateSnapshot(withoutList);

    EXPECT_EQ(view.model()->rowCount(), 1);
}

TEST(ProcessesView, UpdateSnapshot_EmptyList_RemovesRowsWhenHighlightsEnd)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWith({process(4, "System")}, 0));

    view.updateSnapshot(snapshotWith({}, 1));
    EXPECT_EQ(view.model()->rowCount(), 1);
    view.updateSnapshot(snapshotWith({}, 10));

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

namespace
{

// A terminator that records the identity of each process it is asked to end.
ProcessTerminator recordingTerminator(std::vector<ProcessIdentity>& ended)
{
    return [&ended](const ProcessInfo& process) {
        ended.push_back(process.identity());
        return ProcessTerminationResult::Terminated;
    };
}

void selectProcess(ProcessesView& view, const ProcessIdentity& identity)
{
    view.treeView()->setCurrentIndex(view.proxyModel()->mapFromSource(view.model()->indexOf(identity)));
}

QAbstractButton* endButton(const QMessageBox& dialog)
{
    for (auto* button : dialog.buttons()) {
        if (dialog.buttonRole(button) == QMessageBox::DestructiveRole) {
            return button;
        }
    }
    return nullptr;
}

} // namespace

TEST(ProcessesView, EndProcessAction_NoProcessSelected_Disabled)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}));

    EXPECT_FALSE(view.endProcessAction()->isEnabled());
    EXPECT_FALSE(view.endProcessButton()->isEnabled());
}

TEST(ProcessesView, EndProcessAction_NoTerminator_Disabled)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}));

    selectProcess(view, process(800, "explorer.exe").identity());

    EXPECT_FALSE(view.endProcessAction()->isEnabled());
    EXPECT_FALSE(view.endProcessButton()->isEnabled());
}

TEST(ProcessesView, EndProcessAction_ProcessSelected_EnabledWithTheButton)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}));

    selectProcess(view, process(800, "explorer.exe").identity());

    EXPECT_TRUE(view.endProcessAction()->isEnabled());
    EXPECT_TRUE(view.endProcessButton()->isEnabled());
}

TEST(ProcessesView, EndProcessAction_TerminatorSetAfterSelection_Enabled)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}));
    selectProcess(view, process(800, "explorer.exe").identity());

    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));

    EXPECT_TRUE(view.endProcessAction()->isEnabled());
}

TEST(ProcessesView, EndProcessAction_SelectedRowRemoved_DisabledWhenNoRowIsLeft)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWith({process(800, "explorer.exe")}, 0));
    selectProcess(view, process(800, "explorer.exe").identity());

    view.updateSnapshot(snapshotWith({}, 1));
    view.updateSnapshot(snapshotWith({}, 10));

    ASSERT_EQ(view.model()->rowCount(), 0);
    EXPECT_FALSE(view.endProcessAction()->isEnabled());
}

TEST(ProcessesView, EndProcessAction_SelectedProcessKeptAcrossTreeMode_StaysEnabled)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWith({process(4, "System"), childOf(4, 100, "smss.exe")}));
    selectProcess(view, childOf(4, 100, "smss.exe").identity());

    view.setTreeMode(true);

    EXPECT_TRUE(view.endProcessAction()->isEnabled());
}

TEST(ProcessesView, EndProcessAction_OfferedByContextMenuAndDeleteKeyOnTheTable)
{
    ProcessesView view;

    EXPECT_EQ(view.treeView()->contextMenuPolicy(), Qt::ActionsContextMenu);
    EXPECT_TRUE(view.treeView()->actions().contains(view.endProcessAction()));
    EXPECT_EQ(view.endProcessAction()->shortcut(), QKeySequence(QKeySequence::Delete));
    EXPECT_EQ(view.endProcessAction()->shortcutContext(), Qt::WidgetShortcut);
}

TEST(ProcessesView, EndProcessAction_Triggered_AsksWithNameAndPidAndWarning)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}));
    selectProcess(view, process(800, "explorer.exe").identity());

    view.endProcessAction()->trigger();

    const auto* dialog = view.findChild<QMessageBox*>();
    ASSERT_NE(dialog, nullptr);
    EXPECT_TRUE(dialog->isVisible());
    EXPECT_EQ(dialog->icon(), QMessageBox::Warning);
    EXPECT_EQ(dialog->text(), "Do you want to end explorer.exe (PID 800)?");
    EXPECT_EQ(dialog->textFormat(), Qt::PlainText);
    EXPECT_TRUE(dialog->informativeText().contains("unsaved data"));
    EXPECT_NE(endButton(*dialog), nullptr);
    EXPECT_EQ(dialog->defaultButton(), dialog->button(QMessageBox::Cancel));
    EXPECT_EQ(dialog->escapeButton(), dialog->button(QMessageBox::Cancel));
    EXPECT_TRUE(ended.empty());
}

TEST(ProcessesView, EndProcessAction_Confirmed_EndsTheSelectedProcess)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}));
    selectProcess(view, process(800, "explorer.exe").identity());
    view.endProcessAction()->trigger();
    auto* dialog = view.findChild<QMessageBox*>();
    ASSERT_NE(dialog, nullptr);
    ASSERT_NE(endButton(*dialog), nullptr);

    endButton(*dialog)->click();

    EXPECT_EQ(ended, std::vector{process(800, "explorer.exe").identity()});
    EXPECT_FALSE(dialog->isVisible());
}

TEST(ProcessesView, EndProcessAction_Cancelled_DoesNotEndTheProcess)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}));
    selectProcess(view, process(800, "explorer.exe").identity());
    view.endProcessAction()->trigger();
    auto* dialog = view.findChild<QMessageBox*>();
    ASSERT_NE(dialog, nullptr);

    dialog->button(QMessageBox::Cancel)->click();

    EXPECT_TRUE(ended.empty());
    EXPECT_FALSE(dialog->isVisible());
}

TEST(ProcessesView, EndProcessAction_ProcessLeavesTheListWhileAsking_EndsTheProcessAskedAbout)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    const ProcessInfo notepad = process(900, "notepad.exe");
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe"), notepad}, 0));
    selectProcess(view, notepad.identity());
    view.endProcessAction()->trigger();
    auto* dialog = view.findChild<QMessageBox*>();
    ASSERT_NE(dialog, nullptr);

    // Notepad exits and its row is removed; another process becomes current.
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}, 1));
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}, 10));
    ASSERT_FALSE(view.model()->indexOf(notepad.identity()).isValid());
    const auto* current = view.model()->process(view.proxyModel()->mapToSource(view.treeView()->currentIndex()));
    ASSERT_NE(current, nullptr);
    ASSERT_NE(current->identity(), notepad.identity());
    ASSERT_NE(endButton(*dialog), nullptr);
    endButton(*dialog)->click();

    EXPECT_EQ(ended, std::vector{notepad.identity()});
}

TEST(ProcessesView, EndProcessButton_Clicked_Asks)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWith({process(4, "System"), process(800, "explorer.exe")}));
    selectProcess(view, process(800, "explorer.exe").identity());

    view.endProcessButton()->click();

    const auto* dialog = view.findChild<QMessageBox*>();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->text(), "Do you want to end explorer.exe (PID 800)?");
}
