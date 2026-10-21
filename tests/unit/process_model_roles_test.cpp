#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <QIcon>
#include <QImage>
#include <QModelIndex>
#include <QString>
#include <QVariant>

#include "domain/process_info.h"
#include "domain/settings.h"
#include "support/process_model_harness.h"
#include "support/process_samples.h"
#include "ui/dashboard_formatting.h"
#include "ui/process_formatting.h"
#include "ui/process_model.h"

// ProcessModel: what each column shows in each role, from the header titles
// and tooltips to the cells' text, alignment, icons and tooltips.

using namespace verdandi::domain;
using namespace verdandi::tests;
using verdandi::ui::formatByteRate;
using verdandi::ui::formatBytes;
using verdandi::ui::formatCount;
using verdandi::ui::formatPercent;
using verdandi::ui::kNotAvailable;
using verdandi::ui::processDetailsToolTip;
using verdandi::ui::processIoToolTip;
using verdandi::ui::processMemoryToolTip;
using verdandi::ui::ProcessModel;

namespace
{

using Column = ProcessModel::Column;

} // namespace

TEST(ProcessModel, HeaderData_NamesColumnsInDisplayOrder)
{
    ProcessModel model;

    std::vector<QString> titles;
    for (int section = 0; section < model.columnCount(); ++section) {
        titles.push_back(model.headerData(section, Qt::Horizontal).toString());
    }

    EXPECT_EQ(titles, (std::vector<QString>{"Name", "PID", "CPU", "Memory", "I/O", "Network", "Threads", "Handles"}));
    EXPECT_FALSE(model.headerData(ProcessModel::kColumnCount, Qt::Horizontal).isValid());
    EXPECT_FALSE(model.headerData(0, Qt::Vertical).isValid());
}

TEST(ProcessModel, HeaderData_NumericHeadersAlignRightLikeTheirValues)
{
    ProcessModel model;

    const auto alignment = [&model](int section) {
        return model.headerData(section, Qt::Horizontal, Qt::TextAlignmentRole).value<Qt::Alignment>();
    };

    EXPECT_EQ(alignment(static_cast<int>(Column::Name)), Qt::AlignLeft | Qt::AlignVCenter);
    for (int section = static_cast<int>(Column::Pid); section < ProcessModel::kColumnCount; ++section) {
        EXPECT_EQ(alignment(section), Qt::AlignRight | Qt::AlignVCenter) << section;
    }
}

TEST(ProcessModel, HeaderData_MeasuredValuesSortLargestFirstOnFirstClick)
{
    ProcessModel model;

    const auto initialOrder = [&model](Column column) {
        return model.headerData(static_cast<int>(column), Qt::Horizontal, Qt::InitialSortOrderRole)
            .value<Qt::SortOrder>();
    };

    EXPECT_EQ(initialOrder(Column::Name), Qt::AscendingOrder);
    EXPECT_EQ(initialOrder(Column::Pid), Qt::AscendingOrder);
    // Network has no values to sort by, so the names decide, A to Z.
    EXPECT_EQ(initialOrder(Column::Network), Qt::AscendingOrder);
    for (const Column column : {Column::Cpu, Column::Memory, Column::Io, Column::Threads, Column::Handles}) {
        EXPECT_EQ(initialOrder(column), Qt::DescendingOrder) << static_cast<int>(column);
    }
}

TEST(ProcessModel, HeaderData_ToolTipsSayWhatValuesCount)
{
    ProcessModel model;

    const auto toolTip = [&model](Column column) {
        return model.headerData(static_cast<int>(column), Qt::Horizontal, Qt::ToolTipRole).toString();
    };

    EXPECT_TRUE(toolTip(Column::Cpu).contains("all logical processors"));
    EXPECT_TRUE(toolTip(Column::Memory).startsWith("Private working set"));
    EXPECT_TRUE(toolTip(Column::Memory).contains("Task Manager"));
    EXPECT_TRUE(toolTip(Column::Io).contains("not only disk"));
    EXPECT_TRUE(toolTip(Column::Network).contains("ETW"));
}

TEST(ProcessModel, Data_ValueColumns_ShowLatestValuesFormatted)
{
    ProcessModel model;
    ProcessInfo explorer = process(800, "explorer.exe");
    explorer.cpuUsagePercent = 12.54f;
    explorer.workingSetBytes = 3'145'728;        // 3.0 MiB
    explorer.privateWorkingSetBytes = 1'572'864; // 1.5 MiB
    explorer.privateBytes = 2'097'152;           // 2.0 MiB
    explorer.ioReadBytesPerSec = 1024;
    explorer.ioWriteBytesPerSec = 1024;
    explorer.threadCount = 1234;
    explorer.handleCount = 56'789;

    model.updateProcesses({explorer}, kNow);

    EXPECT_EQ(firstRowText(model, Column::Cpu), formatPercent(12.54f));
    EXPECT_EQ(firstRowText(model, Column::Memory), formatBytes(explorer.privateWorkingSetBytes, ByteUnits::Binary));
    EXPECT_EQ(firstRowText(model, Column::Io), formatByteRate(2048, ByteUnits::Binary));
    EXPECT_EQ(firstRowText(model, Column::Threads), formatCount(1234));
    EXPECT_EQ(firstRowText(model, Column::Handles), formatCount(56'789));
    EXPECT_EQ(firstRowText(model, Column::Io, Qt::ToolTipRole), processIoToolTip(explorer, ByteUnits::Binary));
    EXPECT_EQ(firstRowText(model, Column::Memory, Qt::ToolTipRole), processMemoryToolTip(explorer, ByteUnits::Binary));
}

TEST(ProcessModel, Data_Memory_ShowsPrivateWorkingSetNotSharedPages)
{
    ProcessModel model;
    ProcessInfo browser = process(800, "browser.exe");
    browser.workingSetBytes = 397'082'214;        // 378.7 MiB, of which 171.7 MiB is shared
    browser.privateWorkingSetBytes = 217'055'232; // 207.0 MiB

    model.updateProcesses({browser}, kNow);

    EXPECT_EQ(firstRowText(model, Column::Memory), formatBytes(browser.privateWorkingSetBytes, ByteUnits::Binary));
    EXPECT_EQ(firstRowText(model, Column::Memory, Qt::ToolTipRole), processMemoryToolTip(browser, ByteUnits::Binary));
}

TEST(ProcessModel, SetByteUnits_MemoryAndIoInThemAndSignalledForEveryRow)
{
    ProcessModel model;
    model.setTreeMode(true);
    ProcessInfo services = child(400, "services.exe", 0);
    services.privateWorkingSetBytes = 1'572'864;
    ProcessInfo svchost = child(500, "svchost.exe", 400);
    svchost.ioReadBytesPerSec = 1024;
    svchost.ioWriteBytesPerSec = 1024;
    model.updateProcesses({services, svchost, child(800, "explorer.exe", 0)}, kNow);
    ModelSignalLog log(model);
    constexpr int kMemory = static_cast<int>(Column::Memory);
    constexpr int kIo = static_cast<int>(Column::Io);

    model.setByteUnits(ByteUnits::Decimal);
    model.setByteUnits(ByteUnits::Decimal);

    EXPECT_EQ(model.byteUnits(), ByteUnits::Decimal);
    EXPECT_EQ(firstRowText(model, Column::Memory), formatBytes(services.privateWorkingSetBytes, ByteUnits::Decimal));
    EXPECT_EQ(firstRowText(model, Column::Memory, Qt::ToolTipRole), processMemoryToolTip(services, ByteUnits::Decimal));
    const QModelIndex svchostIo = model.index(0, kIo, model.index(0, 0));
    EXPECT_EQ(svchostIo.data().toString(), formatByteRate(2048, ByteUnits::Decimal));
    EXPECT_EQ(svchostIo.data(Qt::ToolTipRole).toString(), processIoToolTip(svchost, ByteUnits::Decimal));
    // One block for each parent's rows, the top level's two and services.exe's one, and none for the repeat.
    // The blocks come in no set order. A predicate, because clang-cl rejects MSVC's vectorized find of a CellRange.
    const auto isSignalled = [&log](const CellRange& block) {
        return std::ranges::any_of(log.changed, [&block](const CellRange& changed) { return changed == block; });
    };
    ASSERT_EQ(log.changed.size(), 2u);
    EXPECT_TRUE(isSignalled({.firstRow = 0, .lastRow = 1, .firstColumn = kMemory, .lastColumn = kIo}));
    EXPECT_TRUE(isSignalled({.firstRow = 0, .lastRow = 0, .firstColumn = kMemory, .lastColumn = kIo}));
}

TEST(ProcessModel, Data_ZeroRates_ShowZeroNotMissing)
{
    ProcessModel model;
    ProcessInfo idle = process(800, "idle.exe");
    idle.cpuUsagePercent = 0.0f;
    idle.ioReadBytesPerSec = 0;
    idle.ioWriteBytesPerSec = 0;

    model.updateProcesses({idle}, kNow);

    EXPECT_EQ(firstRowText(model, Column::Cpu), formatPercent(0.0f));
    EXPECT_EQ(firstRowText(model, Column::Io), formatByteRate(0, ByteUnits::Binary));
    EXPECT_FALSE(model.index(0, static_cast<int>(Column::Cpu)).data(Qt::ForegroundRole).isValid());
}

TEST(ProcessModel, Data_MissingValues_ShowNotAvailableInPlaceholderColour)
{
    ProcessModel model;
    ProcessInfo firstSeen = process(800, "explorer.exe");
    firstSeen.ioReadBytesPerSec = 4096;

    model.updateProcesses({firstSeen}, kNow);

    for (const Column column : {Column::Cpu, Column::Io, Column::Network}) {
        EXPECT_EQ(firstRowText(model, column), kNotAvailable) << static_cast<int>(column);
        EXPECT_TRUE(model.index(0, static_cast<int>(column)).data(Qt::ForegroundRole).isValid())
            << static_cast<int>(column);
    }
    EXPECT_EQ(firstRowText(model, Column::Io, Qt::ToolTipRole), processIoToolTip(firstSeen, ByteUnits::Binary));
    EXPECT_FALSE(model.index(0, static_cast<int>(Column::Name)).data(Qt::ForegroundRole).isValid());
}

TEST(ProcessModel, Data_NumbersAlignRightAndNamesDoNot)
{
    ProcessModel model;
    model.updateProcesses({process(4, "System")}, kNow);

    const auto alignment = [&model](int column) { return model.index(0, column).data(Qt::TextAlignmentRole); };

    EXPECT_FALSE(alignment(static_cast<int>(Column::Name)).isValid());
    for (int column = static_cast<int>(Column::Pid); column < ProcessModel::kColumnCount; ++column) {
        EXPECT_EQ(alignment(column).value<Qt::Alignment>(), Qt::AlignRight | Qt::AlignVCenter) << column;
    }
}

namespace
{

// The icon's 16-pixel image, as the view draws it at 100% scale.
QImage iconImage(const QVariant& decoration)
{
    return decoration.value<QIcon>().pixmap(16, 16).toImage();
}

bool hasVisiblePixel(const QImage& image)
{
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).alpha() > 0) {
                return true;
            }
        }
    }
    return false;
}

ProcessInfo withDetails(uint32_t pid, ProcessDetailsStatus status)
{
    ProcessInfo info = process(pid, "p" + std::to_string(pid));
    info.detailsStatus = status;
    return info;
}

} // namespace

TEST(ProcessModel, Data_NameColumn_AccessIndicatorPerDetailsStatus)
{
    ProcessModel model;
    model.updateProcesses(
        {withDetails(1, ProcessDetailsStatus::Available), withDetails(2, ProcessDetailsStatus::AccessDenied),
         withDetails(3, ProcessDetailsStatus::NotQueried), withDetails(4, ProcessDetailsStatus::Unavailable)},
        kNow);

    std::vector<QImage> images;
    for (int row = 0; row < 4; ++row) {
        const QVariant decoration = model.index(row, static_cast<int>(Column::Name)).data(Qt::DecorationRole);
        ASSERT_TRUE(decoration.canConvert<QIcon>()) << row;
        images.push_back(iconImage(decoration));
        EXPECT_FALSE(model.index(row, static_cast<int>(Column::Pid)).data(Qt::DecorationRole).isValid()) << row;
    }

    // A blank icon keeps the names of processes with details aligned with the others.
    EXPECT_EQ(images[0].size(), images[1].size());
    EXPECT_FALSE(hasVisiblePixel(images[0]));
    for (std::size_t status = 1; status < images.size(); ++status) {
        EXPECT_TRUE(hasVisiblePixel(images[status])) << status;
    }
    EXPECT_NE(images[1], images[2]);
    EXPECT_NE(images[1], images[3]);
    EXPECT_NE(images[2], images[3]);
}

TEST(ProcessModel, Data_NameToolTip_GivesDetailsOrWhyMissing)
{
    ProcessModel model;
    ProcessInfo available = withDetails(1, ProcessDetailsStatus::Available);
    available.imagePath = "C:/Apps/p1.exe";
    available.commandLine = "p1.exe --run";
    const ProcessInfo denied = withDetails(2, ProcessDetailsStatus::AccessDenied);
    model.updateProcesses({available, denied}, kNow);

    EXPECT_EQ(model.index(0, 0).data(Qt::ToolTipRole).toString(), processDetailsToolTip(available));
    EXPECT_EQ(model.index(1, 0).data(Qt::ToolTipRole).toString(), processDetailsToolTip(denied));
}

TEST(ProcessModel, Data_NameToolTip_ExitedProcess_AddsExitedAfterDetails)
{
    ProcessModel model;
    const ProcessInfo denied = withDetails(2, ProcessDetailsStatus::AccessDenied);
    model.updateProcesses({process(4, "System"), denied}, kNow);

    model.updateProcesses({process(4, "System")}, kNow);

    const QString toolTip = model.indexOf(denied.identity()).data(Qt::ToolTipRole).toString();
    EXPECT_TRUE(toolTip.startsWith(processDetailsToolTip(denied)));
    EXPECT_TRUE(toolTip.endsWith("<p>This process has exited; its row is removed in a moment.</p>"));
}

TEST(ProcessModel, HeaderData_NameToolTip_ExplainsTheIndicator)
{
    ProcessModel model;

    const QString toolTip =
        model.headerData(static_cast<int>(Column::Name), Qt::Horizontal, Qt::ToolTipRole).toString();

    EXPECT_TRUE(toolTip.contains("icon"));
    EXPECT_TRUE(toolTip.contains("could not be read"));
}
