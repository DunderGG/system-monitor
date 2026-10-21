#include "ui/performance_view.h"

#include <cassert>
#include <chrono>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVariant>
#include <QVBoxLayout>

#include "domain/settings.h"
#include "ui/charts/sparkline_widget.h"
#include "ui/cpu_performance_page.h"
#include "ui/disk_performance_page.h"
#include "ui/memory_performance_page.h"
#include "ui/network_performance_page.h"
#include "ui/view_metrics.h"

namespace verdandi::ui
{

namespace
{

constexpr int kSidebarWidth = 200;
constexpr int kSidebarItemSpacing = 4;
// Space between the sidebar list and the chart-history selector below it.
constexpr int kSidebarColumnSpacing = 4;

QString sidebarEntry(const PerformancePage& page)
{
    return page.title() + "\n" + page.summary();
}

// The selector's text for a window of samples: the time they cover, as the charts' X-axis labels tell it.
QString historyWindowText(std::size_t samples, std::chrono::milliseconds sampleInterval)
{
    return charts::formatHistoryWindow(std::chrono::duration_cast<std::chrono::seconds>(
        sampleInterval * static_cast<std::chrono::milliseconds::rep>(samples)));
}

} // namespace

PerformanceView::PerformanceView(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(kViewMargin, kViewMargin, kViewMargin, kViewMargin);
    layout->setSpacing(kSectionSpacing);

    auto* sidebarColumn = new QVBoxLayout();
    sidebarColumn->setSpacing(kSidebarColumnSpacing);
    m_sidebar = new QListWidget(this);
    m_sidebar->setFixedWidth(kSidebarWidth);
    m_sidebar->setSpacing(kSidebarItemSpacing);
    sidebarColumn->addWidget(m_sidebar, 1);

    auto* windowLabel = new QLabel("Chart history", this);
    sidebarColumn->addWidget(windowLabel);
    m_windowSelector = new QComboBox(this);
    m_windowSelector->setFixedWidth(kSidebarWidth);
    for (const std::size_t samples : kHistoryWindows) {
        m_windowSelector->addItem(historyWindowText(samples, domain::kDefaultRefreshInterval),
                                  QVariant::fromValue(samples));
    }
    windowLabel->setBuddy(m_windowSelector);
    sidebarColumn->addWidget(m_windowSelector);
    layout->addLayout(sidebarColumn);

    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack, 1);

    m_cpuPage = new CpuPerformancePage(this);
    m_memoryPage = new MemoryPerformancePage(this);
    m_diskPage = new DiskPerformancePage(this);
    m_networkPage = new NetworkPerformancePage(this);
    m_pages = {m_cpuPage, m_memoryPage, m_diskPage, m_networkPage};

    for (auto* page : m_pages) {
        m_stack->addWidget(page);
        m_sidebar->addItem(sidebarEntry(*page));
    }

    connect(m_sidebar, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) {
            return;
        }
        m_stack->setCurrentIndex(row);
        if (isVisible()) {
            refreshVisible();
        }
    });
    m_sidebar->setCurrentRow(0);

    connect(m_windowSelector, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) {
            return;
        }
        const auto samples = m_windowSelector->itemData(index).value<std::size_t>();
        for (auto* page : m_pages) {
            page->setVisibleWindow(samples);
        }
        if (isVisible()) {
            refreshVisible();
        }
    });
    setHistoryWindow(PerformancePage::kDefaultVisibleWindow);
}

void PerformanceView::setHistoryWindow(std::size_t samples)
{
    const int index = m_windowSelector->findData(QVariant::fromValue(samples));
    assert(index >= 0 && "history window must be one of kHistoryWindows");
    m_windowSelector->setCurrentIndex(index);
}

std::size_t PerformanceView::historyWindow() const
{
    return m_windowSelector->currentData().value<std::size_t>();
}

QComboBox* PerformanceView::historyWindowSelector() const
{
    return m_windowSelector;
}

void PerformanceView::setSampleInterval(std::chrono::milliseconds interval)
{
    for (auto* page : m_pages) {
        page->setSampleInterval(interval);
    }
    for (int index = 0; index < m_windowSelector->count(); ++index) {
        const auto samples = m_windowSelector->itemData(index).value<std::size_t>();
        m_windowSelector->setItemText(index, historyWindowText(samples, interval));
    }
    if (isVisible()) {
        refreshVisible();
    }
}

std::chrono::milliseconds PerformanceView::sampleInterval() const
{
    return m_cpuPage->sampleInterval();
}

void PerformanceView::setDisplayUnits(domain::DisplayUnits units)
{
    for (auto* page : m_pages) {
        page->setDisplayUnits(units);
    }
    if (isVisible()) {
        refreshVisible();
    }
}

domain::DisplayUnits PerformanceView::displayUnits() const
{
    return m_cpuPage->displayUnits();
}

void PerformanceView::updateSnapshot(const domain::SystemSnapshot& snapshot)
{
    for (auto* page : m_pages) {
        page->recordSnapshot(snapshot);
    }
    // Hidden widgets need no update; showEvent() catches up from the history.
    if (isVisible()) {
        refreshVisible();
    }
}

void PerformanceView::setCurrentPage(Page page)
{
    m_sidebar->setCurrentRow(static_cast<int>(page));
}

PerformanceView::Page PerformanceView::currentPage() const
{
    return static_cast<Page>(m_stack->currentIndex());
}

QString PerformanceView::sidebarText(Page page) const
{
    const int row = static_cast<int>(page);
    assert(row >= 0 && row < m_sidebar->count() && "performance page out of range");
    return m_sidebar->item(row)->text();
}

CpuPerformancePage* PerformanceView::cpuPage() const
{
    return m_cpuPage;
}

MemoryPerformancePage* PerformanceView::memoryPage() const
{
    return m_memoryPage;
}

DiskPerformancePage* PerformanceView::diskPage() const
{
    return m_diskPage;
}

NetworkPerformancePage* PerformanceView::networkPage() const
{
    return m_networkPage;
}

void PerformanceView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshVisible();
}

void PerformanceView::refreshVisible()
{
    m_pages[static_cast<std::size_t>(m_stack->currentIndex())]->refresh();
    for (std::size_t index = 0; index < m_pages.size(); ++index) {
        auto* item = m_sidebar->item(static_cast<int>(index));
        const QString text = sidebarEntry(*m_pages[index]);
        if (item->text() != text) {
            item->setText(text);
        }
    }
}

} // namespace verdandi::ui
