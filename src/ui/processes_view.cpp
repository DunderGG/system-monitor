#include "ui/processes_view.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>

#include <QCheckBox>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
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
    layout->addLayout(toolbar);

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
    sizeColumns(m_treeView);
    layout->addWidget(m_treeView, 1);

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

void ProcessesView::applyTreeMode(bool enabled)
{
    // Switching resets the model, which clears the selection; the selected
    // process is selected again afterwards.
    std::optional<domain::ProcessIdentity> selected;
    if (const auto* process = m_model->process(m_proxyModel->mapToSource(m_treeView->currentIndex()))) {
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
