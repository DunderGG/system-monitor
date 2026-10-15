#include "ui/processes_view.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

#include <QAbstractButton>
#include <QAction>
#include <QCheckBox>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

#include "domain/process_identity.h"
#include "ui/dashboard_formatting.h"
#include "ui/process_formatting.h"
#include "ui/process_model.h"
#include "ui/process_sort_filter_proxy_model.h"
#include "ui/view_metrics.h"

namespace sysmon::ui
{

namespace
{

// Room for the padding around a header label and for the sort indicator.
constexpr int kHeaderPadding = 24;
// Wide enough for a long image name; the filter box does not need more.
constexpr int kFilterMaximumWidth = 320;

const QString kEndProcessText = QStringLiteral("End process");
const QString kEndProcessWarning = QStringLiteral(
    "Ending a process stops it immediately. Any unsaved data in it is lost, and programs that depend on it may stop "
    "working.");
const QString kCloseMonitorText = QStringLiteral("Close System Monitor");
const QString kCloseMonitorExplanation = QStringLiteral("It closes the same way as when you close its window.");

// Sizes each value column to fit its widest expected text, so the columns stay
// put as values change. The name column takes the remaining width.
void sizeColumns(QTreeView* treeView)
{
    struct ColumnTemplate
    {
        ProcessModel::Column column;
        QString widestText;
    };
    const std::array templates{
        ColumnTemplate{ProcessModel::Column::Pid, QStringLiteral("9999999")},
        ColumnTemplate{ProcessModel::Column::Cpu, QStringLiteral("100.0%")},
        ColumnTemplate{ProcessModel::Column::Memory, QStringLiteral("1023.9 MiB")},
        ColumnTemplate{ProcessModel::Column::Io, QStringLiteral("1023.9 MiB/s")},
        ColumnTemplate{ProcessModel::Column::Network, kNotAvailable},
        ColumnTemplate{ProcessModel::Column::Threads, QStringLiteral("9,999")},
        ColumnTemplate{ProcessModel::Column::Handles, QStringLiteral("999,999")},
    };

    QHeaderView* header = treeView->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(static_cast<int>(ProcessModel::Column::Name), QHeaderView::Stretch);

    const QFontMetrics metrics = treeView->fontMetrics();
    for (const auto& entry : templates) {
        const int section = static_cast<int>(entry.column);
        const QString title = treeView->model()->headerData(section, Qt::Horizontal).toString();
        const int width = std::max(metrics.horizontalAdvance(entry.widestText), metrics.horizontalAdvance(title));
        header->resizeSection(section, width + kHeaderPadding);
    }
}

// The icon follows the level the terminator logs the result at (DC-87):
// information for a process that had already exited, a warning for a refusal
// or a denial, and critical for any other failure.
QMessageBox::Icon terminationMessageIcon(domain::ProcessTerminationResult result)
{
    switch (result) {
        case domain::ProcessTerminationResult::Terminated:
        case domain::ProcessTerminationResult::AlreadyExited:
            return QMessageBox::Information;
        case domain::ProcessTerminationResult::NotOwnedByCurrentUser:
        case domain::ProcessTerminationResult::Elevated:
        case domain::ProcessTerminationResult::NeverOpened:
        case domain::ProcessTerminationResult::AccessDenied:
            return QMessageBox::Warning;
        case domain::ProcessTerminationResult::Failed:
            return QMessageBox::Critical;
    }
    return QMessageBox::Warning;
}

// Opens a window-modal message that says why the process was not ended, or
// that it had already exited. A process that was ended needs none.
void showTerminationMessage(QWidget* parent, const domain::ProcessInfo& process,
                            domain::ProcessTerminationResult result)
{
    const auto message = processTerminationMessage(process, result);
    if (!message) {
        return;
    }

    auto* box =
        new QMessageBox(terminationMessageIcon(result), kEndProcessText, message->text, QMessageBox::Ok, parent);
    box->setAttribute(Qt::WA_DeleteOnClose);
    // The text names the process; it must not be read as rich text. The
    // informative text takes the same format.
    box->setTextFormat(Qt::PlainText);
    box->setInformativeText(message->informativeText);
    box->open();
}

// Opens a window-modal dialog that asks question, with a button for the
// action and Cancel, which is the default. onConfirmed runs if the action's
// button closed the dialog, once it has closed (buttonClicked comes while it
// is still shown), so a message about the result is not opened over it.
void askToConfirm(QWidget* parent, QMessageBox::Icon icon, const QString& question, const QString& informativeText,
                  const QString& actionText, QMessageBox::ButtonRole actionRole, std::function<void()> onConfirmed)
{
    auto* dialog = new QMessageBox(icon, kEndProcessText, question, QMessageBox::Cancel, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    // The question names a process; it must not be read as rich text.
    dialog->setTextFormat(Qt::PlainText);
    dialog->setInformativeText(informativeText);
    const QPushButton* actionButton = dialog->addButton(actionText, actionRole);
    dialog->setDefaultButton(QMessageBox::Cancel);
    dialog->setEscapeButton(QMessageBox::Cancel);
    QObject::connect(dialog, &QMessageBox::finished, parent,
                     [dialog, actionButton, onConfirmed = std::move(onConfirmed)] {
                         if (dialog->clickedButton() == actionButton) {
                             onConfirmed();
                         }
                     });
    dialog->open();
}

} // namespace

ProcessesView::ProcessesView(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kViewMargin, kViewMargin, kViewMargin, kViewMargin);
    layout->setSpacing(kItemSpacing);

    auto* titleLabel = new QLabel("Processes", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(kTitlePointSize);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(kItemSpacing);
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter by name or PID");
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setMaximumWidth(kFilterMaximumWidth);
    toolbar->addWidget(m_filterEdit, 1);
    m_treeModeCheckBox = new QCheckBox("Show as tree", this);
    m_treeModeCheckBox->setToolTip("Show each process under the process that started it");
    toolbar->addWidget(m_treeModeCheckBox);
    toolbar->addStretch();
    m_summaryLabel = new QLabel(processListSummary(std::nullopt, false), this);
    toolbar->addWidget(m_summaryLabel);
    m_endProcessButton = new QPushButton(kEndProcessText, this);
    m_endProcessButton->setToolTip("End the selected process, after you confirm (Delete)");
    m_endProcessButton->setEnabled(false);
    toolbar->addWidget(m_endProcessButton);
    layout->addLayout(toolbar);

    // One action for the button, the context menu, and the Delete key. The
    // key works only while the table has focus, so Delete in the filter box
    // still edits the text.
    m_endProcessAction = new QAction(kEndProcessText, this);
    m_endProcessAction->setShortcut(QKeySequence::Delete);
    m_endProcessAction->setShortcutContext(Qt::WidgetShortcut);
    m_endProcessAction->setEnabled(false);

    m_model = new ProcessModel(this);
    m_proxyModel = new ProcessSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);

    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_proxyModel);
    m_treeView->setRootIsDecorated(false);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setAllColumnsShowFocus(true);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setSortingEnabled(true);
    m_treeView->sortByColumn(static_cast<int>(ProcessModel::Column::Name), Qt::AscendingOrder);
    m_treeView->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_treeView->addAction(m_endProcessAction);
    sizeColumns(m_treeView);
    layout->addWidget(m_treeView, 1);

    connect(m_endProcessButton, &QPushButton::clicked, m_endProcessAction, &QAction::trigger);
    connect(m_endProcessAction, &QAction::enabledChanged, m_endProcessButton, &QPushButton::setEnabled);
    connect(m_endProcessAction, &QAction::triggered, this, &ProcessesView::confirmEndProcess);
    // The current row also changes when it is removed or filtered out.
    connect(m_treeView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &ProcessesView::updateEndProcessAction);

    connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_proxyModel->setFilterFixedString(text);
        // Matches deep in the tree would otherwise stay inside collapsed branches.
        if (m_model->isTreeMode()) {
            m_treeView->expandAll();
        }
    });
    connect(m_treeModeCheckBox, &QCheckBox::toggled, this, &ProcessesView::applyTreeMode);
    connect(
        m_proxyModel, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex& parent, int first, int last) {
            // A process that gains its first child is expanded. One with
            // children already keeps the state it has, which the user may
            // have chosen.
            if (m_model->isTreeMode() && parent.isValid() && first == 0 && m_proxyModel->rowCount(parent) == last + 1) {
                m_treeView->expand(parent);
            }
        });
}

void ProcessesView::setTreeMode(bool enabled)
{
    m_treeModeCheckBox->setChecked(enabled);
}

bool ProcessesView::isTreeMode() const
{
    return m_model->isTreeMode();
}

void ProcessesView::updateSnapshot(const domain::SystemSnapshot& snapshot)
{
    std::optional<std::size_t> listedCount;
    if (snapshot.processes) {
        m_model->updateProcesses(*snapshot.processes, snapshot.timestamp);
        listedCount = snapshot.processes->size();
    } else {
        m_model->updateTime(snapshot.timestamp);
    }
    m_summaryLabel->setText(processListSummary(listedCount, m_hasProcessList));
    m_hasProcessList = m_hasProcessList || snapshot.processes.has_value();
    // The current row may have been marked as exited, or listed again, without becoming another row.
    updateEndProcessAction();
}

ProcessModel* ProcessesView::model() const
{
    return m_model;
}

ProcessSortFilterProxyModel* ProcessesView::proxyModel() const
{
    return m_proxyModel;
}

QLineEdit* ProcessesView::filterEdit() const
{
    return m_filterEdit;
}

QCheckBox* ProcessesView::treeModeCheckBox() const
{
    return m_treeModeCheckBox;
}

QAction* ProcessesView::endProcessAction() const
{
    return m_endProcessAction;
}

QPushButton* ProcessesView::endProcessButton() const
{
    return m_endProcessButton;
}

void ProcessesView::setProcessTerminator(ProcessTerminator terminator)
{
    m_terminator = std::move(terminator);
    updateEndProcessAction();
}

void ProcessesView::setMonitorIdentity(const domain::ProcessIdentity& identity)
{
    m_monitorIdentity = identity;
}

void ProcessesView::applyTreeMode(bool enabled)
{
    // Switching resets the model, which clears the selection; the selected
    // process is selected again afterwards.
    std::optional<domain::ProcessIdentity> selected;
    if (const auto* process = currentProcess()) {
        selected = process->identity();
    }

    m_model->setTreeMode(enabled);
    m_treeView->setRootIsDecorated(enabled);
    if (enabled) {
        m_treeView->expandAll();
    }

    if (selected) {
        const QModelIndex index = m_proxyModel->mapFromSource(m_model->indexOf(*selected));
        if (index.isValid()) {
            m_treeView->setCurrentIndex(index);
            m_treeView->scrollTo(index);
        }
    }
    // The reset cleared the current row without signalling it.
    updateEndProcessAction();
}

const domain::ProcessInfo* ProcessesView::currentProcess() const
{
    return m_model->process(m_proxyModel->mapToSource(m_treeView->currentIndex()));
}

const domain::ProcessInfo* ProcessesView::currentRunningProcess() const
{
    // An exited process keeps its row for a moment, but there is nothing left to end.
    const QModelIndex current = m_proxyModel->mapToSource(m_treeView->currentIndex());
    return m_model->hasExited(current) ? nullptr : m_model->process(current);
}

void ProcessesView::updateEndProcessAction()
{
    m_endProcessAction->setEnabled(m_terminator && currentRunningProcess() != nullptr);
}

void ProcessesView::confirmEndProcess()
{
    const domain::ProcessInfo* current = currentRunningProcess();
    if (current == nullptr || !m_terminator) {
        return;
    }

    if (current->identity() == m_monitorIdentity) {
        // Ending its own process would stop the monitor before it stops the
        // scheduler and flushes its log. Closing its window shuts it down the
        // usual way.
        askToConfirm(this, QMessageBox::Question, monitorCloseQuestion(*current), kCloseMonitorExplanation,
                     kCloseMonitorText, QMessageBox::AcceptRole, [this] { window()->close(); });
        return;
    }

    // The table keeps refreshing while the dialog is open, and the current
    // row can move to another process, so the answer applies to a copy of
    // the process the dialog names.
    askToConfirm(this, QMessageBox::Warning, processTerminationQuestion(*current), kEndProcessWarning, kEndProcessText,
                 QMessageBox::DestructiveRole, [this, process = *current] {
                     if (m_terminator) {
                         // The terminator also logs the outcome.
                         showTerminationMessage(this, process, m_terminator(process));
                     }
                 });
}

QTreeView* ProcessesView::treeView() const
{
    return m_treeView;
}

QString ProcessesView::summaryText() const
{
    return m_summaryLabel->text();
}

} // namespace sysmon::ui
