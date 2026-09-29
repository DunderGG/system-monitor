#pragma once

#include <array>
#include <cstddef>

#include <QString>
#include <QWidget>

#include "domain/system_snapshot.h"

class QListWidget;
class QShowEvent;
class QStackedWidget;

namespace sysmon::ui
{

class CpuPerformancePage;
class DiskPerformancePage;
class MemoryPerformancePage;
class NetworkPerformancePage;
class PerformancePage;

/**
 * Performance tab: a sidebar listing CPU, memory, disk, and network with a
 * one-line summary each, and the selected resource's detail page with
 * full-size history charts.
 *
 * Every snapshot is recorded into every page's history, so the charts are
 * complete whenever they are shown. The widgets themselves (the selected
 * page and the sidebar summaries) are refreshed only while this view is
 * visible, and again when it is shown or another page is selected.
 */
class PerformanceView : public QWidget
{
    Q_OBJECT

public:
    /** Pages in sidebar order. */
    enum class Page
    {
        Cpu,
        Memory,
        Disk,
        Network,
    };

    explicit PerformanceView(QWidget* parent = nullptr);
    ~PerformanceView() override = default;

    /** Records a newly received snapshot, and refreshes the widgets if the view is visible. */
    void updateSnapshot(const domain::SystemSnapshot& snapshot);

    void setCurrentPage(Page page);
    [[nodiscard]] Page currentPage() const;

    /** Returns the sidebar entry for page: its title and summary on separate lines. */
    [[nodiscard]] QString sidebarText(Page page) const;

    [[nodiscard]] CpuPerformancePage* cpuPage() const;
    [[nodiscard]] MemoryPerformancePage* memoryPage() const;
    [[nodiscard]] DiskPerformancePage* diskPage() const;
    [[nodiscard]] NetworkPerformancePage* networkPage() const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    static constexpr std::size_t kPageCount = 4;

    void refreshVisible();

    QListWidget* m_sidebar{nullptr};
    QStackedWidget* m_stack{nullptr};
    CpuPerformancePage* m_cpuPage{nullptr};
    MemoryPerformancePage* m_memoryPage{nullptr};
    DiskPerformancePage* m_diskPage{nullptr};
    NetworkPerformancePage* m_networkPage{nullptr};
    std::array<PerformancePage*, kPageCount> m_pages{};
};

} // namespace sysmon::ui
