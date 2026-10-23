#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
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
#include <QWidget>

#include "domain/process_identity.h"
#include "domain/process_info.h"
#include "domain/process_termination.h"
#include "domain/settings.h"
#include "domain/system_snapshot.h"
#include "support/process_samples.h"
#include "support/snapshots.h"
#include "ui/process_formatting.h"
#include "ui/process_model.h"
#include "ui/process_sort_filter_proxy_model.h"
#include "ui/processes_view.h"

using namespace verdandi::domain;
using namespace verdandi::ui;
using namespace verdandi::tests;

TEST(ProcessesView, Construction_TreeViewShowsTheModelThroughTheProxyAsAFlatTable)
{
    ProcessesView view;

    ASSERT_NE(view.treeView(), nullptr);
    EXPECT_EQ(view.treeView()->model(), view.proxyModel());
    EXPECT_EQ(view.proxyModel()->sourceModel(), view.model());
    EXPECT_FALSE(view.treeView()->rootIsDecorated());
    EXPECT_EQ(view.summaryText(), processListSummary(std::nullopt, false));
}

TEST(ProcessesView, Construction_SortableAndSortedByName)
{
    ProcessesView view;
    view.updateSnapshot(
        snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe"), process(12, "smss.exe")}));

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
    view.updateSnapshot(
        snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe"), process(12, "smss.exe")}));

    view.filterEdit()->setText("EXPLO");

    ASSERT_EQ(view.proxyModel()->rowCount(), 1);
    EXPECT_EQ(view.proxyModel()->index(0, 0).data().toString(), "explorer.exe");
    EXPECT_EQ(view.summaryText(), processListSummary(3, true));

    view.filterEdit()->clear();
    EXPECT_EQ(view.proxyModel()->rowCount(), 3);
}

TEST(ProcessesView, UpdateSnapshot_WithProcessList_ShowsRowsAndCount)
{
    ProcessesView view;

    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}));

    EXPECT_EQ(view.model()->rowCount(), 2);
    EXPECT_EQ(view.summaryText(), processListSummary(2, true));
}

TEST(ProcessesView, SetDisplayUnits_GivesTheModelTheByteUnits)
{
    ProcessesView view;
    constexpr DisplayUnits kUnits{.bytes = ByteUnits::Decimal, .networkRates = NetworkRateUnits::BitsPerSecond};

    view.setDisplayUnits(kUnits);

    EXPECT_EQ(view.displayUnits(), kUnits);
    EXPECT_EQ(view.model()->byteUnits(), ByteUnits::Decimal);
}

TEST(ProcessesView, UpdateSnapshot_WithoutProcessList_KeepsLastRows)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}));

    view.updateSnapshot(SystemSnapshot{});

    EXPECT_EQ(view.model()->rowCount(), 2);
    EXPECT_EQ(view.summaryText(), processListSummary(std::nullopt, true));
}

TEST(ProcessesView, UpdateSnapshot_WithoutProcessList_StillEndsHighlights)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}, 0));
    view.updateSnapshot(snapshotWithProcesses({process(4, "System")}, 1));

    SystemSnapshot withoutList;
    withoutList.timestamp = std::chrono::steady_clock::time_point{std::chrono::seconds{10}};
    view.updateSnapshot(withoutList);

    EXPECT_EQ(view.model()->rowCount(), 1);
}

TEST(ProcessesView, UpdateSnapshot_EmptyList_RemovesRowsWhenHighlightsEnd)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWithProcesses({process(4, "System")}, 0));

    view.updateSnapshot(snapshotWithProcesses({}, 1));
    EXPECT_EQ(view.model()->rowCount(), 1);
    view.updateSnapshot(snapshotWithProcesses({}, 10));

    EXPECT_EQ(view.model()->rowCount(), 0);
    EXPECT_EQ(view.summaryText(), processListSummary(0, true));
}

TEST(ProcessesView, SetTreeMode_ShowsBranchesExpanded)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), child(100, "smss.exe", 4)}));

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
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), child(100, "smss.exe", 4)}));
    view.treeModeCheckBox()->setChecked(true);

    view.treeModeCheckBox()->setChecked(false);

    EXPECT_FALSE(view.isTreeMode());
    EXPECT_FALSE(view.treeView()->rootIsDecorated());
    EXPECT_EQ(view.proxyModel()->rowCount(), 2);
}

TEST(ProcessesView, SetTreeMode_SelectedProcessStaysSelected)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), child(100, "smss.exe", 4)}));
    const auto smssIdentity = child(100, "smss.exe", 4).identity();
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
    view.updateSnapshot(snapshotWithProcesses({process(400, "services.exe")}));

    view.updateSnapshot(snapshotWithProcesses({process(400, "services.exe"), child(500, "svchost.exe", 400)}));

    EXPECT_TRUE(view.treeView()->isExpanded(view.proxyModel()->index(0, 0)));
}

TEST(ProcessesView, TreeMode_CollapsedBranch_StaysCollapsedWhenChildrenArrive)
{
    ProcessesView view;
    view.setTreeMode(true);
    view.updateSnapshot(snapshotWithProcesses({process(400, "services.exe"), child(500, "svchost.exe", 400)}));
    view.treeView()->collapse(view.proxyModel()->index(0, 0));

    view.updateSnapshot(snapshotWithProcesses(
        {process(400, "services.exe"), child(500, "svchost.exe", 400), child(600, "spoolsv.exe", 400)}));

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

// A terminator that ends nothing and returns `result`.
ProcessTerminator terminatorReturning(ProcessTerminationResult result)
{
    return [result](const ProcessInfo&) { return result; };
}

// The message boxes the view shows. A dialog that has closed stays a hidden
// child until its deferred deletion runs, which these tests never do.
std::vector<QMessageBox*> shownMessageBoxes(const ProcessesView& view)
{
    std::vector<QMessageBox*> shown;
    for (auto* box : view.findChildren<QMessageBox*>()) {
        if (box->isVisible()) {
            shown.push_back(box);
        }
    }
    return shown;
}

// Lists explorer.exe (PID 800), selects it, and confirms ending it. Returns
// the confirmation dialog, or nullptr if it did not open.
QMessageBox* confirmEndingExplorer(ProcessesView& view)
{
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}));
    selectProcess(view, process(800, "explorer.exe").identity());
    view.endProcessAction()->trigger();

    auto* dialog = view.findChild<QMessageBox*>();
    if (dialog == nullptr || endButton(*dialog) == nullptr) {
        return nullptr;
    }
    endButton(*dialog)->click();
    return dialog;
}

} // namespace

TEST(ProcessesView, EndProcessAction_NoProcessSelected_Disabled)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}));

    EXPECT_FALSE(view.endProcessAction()->isEnabled());
    EXPECT_FALSE(view.endProcessButton()->isEnabled());
}

TEST(ProcessesView, EndProcessAction_NoTerminator_Disabled)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}));

    selectProcess(view, process(800, "explorer.exe").identity());

    EXPECT_FALSE(view.endProcessAction()->isEnabled());
    EXPECT_FALSE(view.endProcessButton()->isEnabled());
}

TEST(ProcessesView, EndProcessAction_ProcessSelected_EnabledWithTheButton)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}));

    selectProcess(view, process(800, "explorer.exe").identity());

    EXPECT_TRUE(view.endProcessAction()->isEnabled());
    EXPECT_TRUE(view.endProcessButton()->isEnabled());
}

TEST(ProcessesView, EndProcessAction_TerminatorSetAfterSelection_Enabled)
{
    ProcessesView view;
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}));
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
    view.updateSnapshot(snapshotWithProcesses({process(800, "explorer.exe")}, 0));
    selectProcess(view, process(800, "explorer.exe").identity());

    view.updateSnapshot(snapshotWithProcesses({}, 1));
    view.updateSnapshot(snapshotWithProcesses({}, 10));

    ASSERT_EQ(view.model()->rowCount(), 0);
    EXPECT_FALSE(view.endProcessAction()->isEnabled());
}

TEST(ProcessesView, EndProcessAction_SelectedProcessKeptAcrossTreeMode_StaysEnabled)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), child(100, "smss.exe", 4)}));
    selectProcess(view, child(100, "smss.exe", 4).identity());

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
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}));
    selectProcess(view, process(800, "explorer.exe").identity());

    view.endProcessAction()->trigger();

    const auto* dialog = view.findChild<QMessageBox*>();
    ASSERT_NE(dialog, nullptr);
    EXPECT_TRUE(dialog->isVisible());
    EXPECT_EQ(dialog->icon(), QMessageBox::Warning);
    EXPECT_EQ(dialog->text(), processTerminationQuestion(process(800, "explorer.exe")));
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
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}));
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
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}));
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
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe"), notepad}, 0));
    selectProcess(view, notepad.identity());
    view.endProcessAction()->trigger();
    auto* dialog = view.findChild<QMessageBox*>();
    ASSERT_NE(dialog, nullptr);

    // Notepad exits and its row is removed; another process becomes current.
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}, 1));
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}, 10));
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
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}));
    selectProcess(view, process(800, "explorer.exe").identity());

    view.endProcessButton()->click();

    const auto* dialog = view.findChild<QMessageBox*>();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->text(), processTerminationQuestion(process(800, "explorer.exe")));
}

TEST(ProcessesView, EndProcessAction_DialogClosed_DoesNotEndTheProcess)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(800, "explorer.exe")}));
    selectProcess(view, process(800, "explorer.exe").identity());
    view.endProcessAction()->trigger();
    auto* dialog = view.findChild<QMessageBox*>();
    ASSERT_NE(dialog, nullptr);

    // As the title bar's close button does.
    dialog->close();

    EXPECT_TRUE(ended.empty());
    EXPECT_FALSE(dialog->isVisible());
    EXPECT_TRUE(shownMessageBoxes(view).empty());
}

TEST(ProcessesView, EndProcessAction_Confirmed_EndsTheProcessOnceTheDialogHasClosed)
{
    ProcessesView view;
    std::vector<std::size_t> shownWhileEnding;
    view.setProcessTerminator([&view, &shownWhileEnding](const ProcessInfo&) {
        shownWhileEnding.push_back(shownMessageBoxes(view).size());
        return ProcessTerminationResult::AccessDenied;
    });

    ASSERT_NE(confirmEndingExplorer(view), nullptr);

    EXPECT_EQ(shownWhileEnding, std::vector<std::size_t>{0});
}

TEST(ProcessesView, EndProcessAction_Ended_ShowsNoMessage)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));

    ASSERT_NE(confirmEndingExplorer(view), nullptr);

    EXPECT_EQ(ended.size(), 1u);
    EXPECT_TRUE(shownMessageBoxes(view).empty());
}

TEST(ProcessesView, EndProcessAction_AccessDenied_SaysWhyInAMessage)
{
    ProcessesView view;
    view.setProcessTerminator(terminatorReturning(ProcessTerminationResult::AccessDenied));

    const auto* dialog = confirmEndingExplorer(view);

    ASSERT_NE(dialog, nullptr);
    const auto shown = shownMessageBoxes(view);
    ASSERT_EQ(shown.size(), 1u);
    const auto* message = shown.front();
    EXPECT_NE(message, dialog);
    EXPECT_FALSE(dialog->isVisible());
    const auto expected =
        processTerminationMessage(process(800, "explorer.exe"), ProcessTerminationResult::AccessDenied);
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(message->windowTitle(), "End process");
    EXPECT_EQ(message->icon(), QMessageBox::Warning);
    EXPECT_EQ(message->text(), expected->text);
    EXPECT_EQ(message->textFormat(), Qt::PlainText);
    EXPECT_EQ(message->informativeText(), expected->informativeText);
    EXPECT_EQ(message->standardButtons(), QMessageBox::Ok);
    EXPECT_EQ(message->windowModality(), Qt::WindowModal);
}

TEST(ProcessesView, EndProcessAction_NotEnded_ShowsTheResultsMessageWithAnIconByTheLogLevel)
{
    struct Case
    {
        ProcessTerminationResult result;
        QMessageBox::Icon icon;
    };
    const std::array cases{
        Case{ProcessTerminationResult::AlreadyExited, QMessageBox::Information},
        Case{ProcessTerminationResult::NotOwnedByCurrentUser, QMessageBox::Warning},
        Case{ProcessTerminationResult::Elevated, QMessageBox::Warning},
        Case{ProcessTerminationResult::NeverOpened, QMessageBox::Warning},
        Case{ProcessTerminationResult::AccessDenied, QMessageBox::Warning},
        Case{ProcessTerminationResult::Failed, QMessageBox::Critical},
    };

    for (const auto& [result, icon] : cases) {
        SCOPED_TRACE(static_cast<int>(result));
        ProcessesView view;
        view.setProcessTerminator(terminatorReturning(result));

        ASSERT_NE(confirmEndingExplorer(view), nullptr);

        const auto shown = shownMessageBoxes(view);
        ASSERT_EQ(shown.size(), 1u);
        EXPECT_EQ(shown.front()->icon(), icon);
        const auto expected = processTerminationMessage(process(800, "explorer.exe"), result);
        ASSERT_TRUE(expected.has_value());
        EXPECT_EQ(shown.front()->text(), expected->text);
        EXPECT_EQ(shown.front()->informativeText(), expected->informativeText);
    }
}

TEST(ProcessesView, EndProcessAction_MessageAnswered_ClosesAndLeavesTheActionEnabled)
{
    ProcessesView view;
    view.setProcessTerminator(terminatorReturning(ProcessTerminationResult::AlreadyExited));
    ASSERT_NE(confirmEndingExplorer(view), nullptr);
    auto shown = shownMessageBoxes(view);
    ASSERT_EQ(shown.size(), 1u);

    shown.front()->button(QMessageBox::Ok)->click();

    EXPECT_TRUE(shownMessageBoxes(view).empty());
    EXPECT_TRUE(view.endProcessAction()->isEnabled());
}

TEST(ProcessesView, EndProcessAction_SelectedProcessExits_DisabledUntilListedAgain)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(900, "notepad.exe")}, 0));
    selectProcess(view, process(900, "notepad.exe").identity());

    // Notepad's row stays, marked as exited, and stays the current row.
    view.updateSnapshot(snapshotWithProcesses({process(4, "System")}, 1));

    EXPECT_FALSE(view.endProcessAction()->isEnabled());
    EXPECT_FALSE(view.endProcessButton()->isEnabled());
    view.endProcessAction()->trigger();
    EXPECT_EQ(view.findChild<QMessageBox*>(), nullptr);

    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(900, "notepad.exe")}, 2));
    EXPECT_TRUE(view.endProcessAction()->isEnabled());
}

TEST(ProcessesView, EndProcessAction_ExitedRowSelected_Disabled)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;
    view.setProcessTerminator(recordingTerminator(ended));
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), process(900, "notepad.exe")}, 0));
    view.updateSnapshot(snapshotWithProcesses({process(4, "System")}, 1));
    selectProcess(view, process(4, "System").identity());
    ASSERT_TRUE(view.endProcessAction()->isEnabled());

    selectProcess(view, process(900, "notepad.exe").identity());

    EXPECT_FALSE(view.endProcessAction()->isEnabled());
}

namespace
{

// The monitor's own process, as main.cpp tells the view.
ProcessInfo monitorProcess()
{
    return process(1234, "verdandi.exe");
}

QAbstractButton* closeMonitorButton(const QMessageBox& dialog)
{
    for (auto* button : dialog.buttons()) {
        if (dialog.buttonRole(button) == QMessageBox::AcceptRole) {
            return button;
        }
    }
    return nullptr;
}

// Lists the monitor's own row in view, which knows the monitor's identity,
// selects the row, and triggers End process. Returns the dialog that opened,
// or nullptr if none did.
QMessageBox* endMonitorProcess(ProcessesView& view, std::vector<ProcessIdentity>& ended)
{
    view.setProcessTerminator(recordingTerminator(ended));
    view.setMonitorIdentity(monitorProcess().identity());
    view.updateSnapshot(snapshotWithProcesses({process(4, "System"), monitorProcess()}));
    selectProcess(view, monitorProcess().identity());
    view.endProcessAction()->trigger();
    return view.findChild<QMessageBox*>();
}

} // namespace

TEST(ProcessesView, EndProcessAction_MonitorsOwnRow_AsksToCloseTheMonitorInstead)
{
    ProcessesView view;
    std::vector<ProcessIdentity> ended;

    const auto* dialog = endMonitorProcess(view, ended);

    ASSERT_NE(dialog, nullptr);
    EXPECT_TRUE(dialog->isVisible());
    EXPECT_EQ(dialog->icon(), QMessageBox::Question);
    EXPECT_EQ(dialog->text(), monitorCloseQuestion(monitorProcess()));
    EXPECT_EQ(dialog->textFormat(), Qt::PlainText);
    EXPECT_TRUE(dialog->informativeText().contains("close its window"));
    ASSERT_NE(closeMonitorButton(*dialog), nullptr);
    EXPECT_EQ(closeMonitorButton(*dialog)->text(), "Close Verdandi");
    EXPECT_EQ(endButton(*dialog), nullptr);
    EXPECT_EQ(dialog->defaultButton(), dialog->button(QMessageBox::Cancel));
    EXPECT_EQ(dialog->escapeButton(), dialog->button(QMessageBox::Cancel));
}

TEST(ProcessesView, EndProcessAction_MonitorsOwnRowConfirmed_ClosesTheWindowWithoutEndingTheProcess)
{
    QWidget window;
    auto* view = new ProcessesView(&window);
    window.show();
    std::vector<ProcessIdentity> ended;
    auto* dialog = endMonitorProcess(*view, ended);
    ASSERT_NE(dialog, nullptr);
    ASSERT_NE(closeMonitorButton(*dialog), nullptr);

    closeMonitorButton(*dialog)->click();

    EXPECT_FALSE(window.isVisible());
    EXPECT_TRUE(ended.empty());
    EXPECT_TRUE(shownMessageBoxes(*view).empty());
}

TEST(ProcessesView, EndProcessAction_MonitorsOwnRowCancelled_KeepsTheWindowOpen)
{
    QWidget window;
    auto* view = new ProcessesView(&window);
    window.show();
    std::vector<ProcessIdentity> ended;
    auto* dialog = endMonitorProcess(*view, ended);
    ASSERT_NE(dialog, nullptr);

    dialog->button(QMessageBox::Cancel)->click();

    EXPECT_TRUE(window.isVisible());
    EXPECT_TRUE(ended.empty());
}
