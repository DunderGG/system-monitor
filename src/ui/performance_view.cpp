#include "ui/performance_view.h"

#include <cassert>

#include <QHBoxLayout>
#include <QListWidget>
#include <QShowEvent>
#include <QStackedWidget>

#include "ui/cpu_performance_page.h"
#include "ui/disk_performance_page.h"
#include "ui/memory_performance_page.h"
#include "ui/network_performance_page.h"

namespace sysmon::ui
{

namespace
{

constexpr int kSidebarWidth = 200;
constexpr int kSidebarItemSpacing = 4;

QString sidebarEntry(const PerformancePage& page)
{
    return page.title() + "\n" + page.summary();
}

} // namespace

PerformanceView::PerformanceView(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_sidebar = new QListWidget(this);
    m_sidebar->setFixedWidth(kSidebarWidth);
    m_sidebar->setSpacing(kSidebarItemSpacing);
    layout->addWidget(m_sidebar);

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

} // namespace sysmon::ui
