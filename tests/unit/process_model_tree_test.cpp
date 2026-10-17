#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QString>

#include "domain/process_identity.h"
#include "domain/process_info.h"
#include "support/process_model_harness.h"
#include "support/process_samples.h"
#include "ui/process_model.h"
#include "ui/process_sort_filter_proxy_model.h"

// ProcessModel: the tree of parent and child processes, built from parent PIDs
// and creation times, and its consistency with the proxy under random change.

using namespace sysmon::domain;
using namespace sysmon::tests;
using namespace std::chrono_literals;
using sysmon::ui::ProcessModel;

namespace
{

using Column = ProcessModel::Column;

} // namespace

TEST(ProcessModel, TreeMode_FirstList_ShowsEachProcessUnderItsParent)
{
    ProcessModel model;
    model.setTreeMode(true);

    model.updateProcesses(
        {
            child(0, "", 0),
            child(4, "System", 0),
            child(100, "smss.exe", 4),
            child(200, "csrss.exe", 150),
            child(300, "wininit.exe", 150),
            child(400, "services.exe", 300),
            child(500, "svchost.exe", 400),
            child(600, "svchost.exe", 400),
        },
        kNow);

    // PID 0 parents nothing, and PID 150 is not listed.
    EXPECT_EQ(treeText(model), "System Idle Process\n"
                               "System\n"
                               "  smss.exe\n"
                               "csrss.exe\n"
                               "wininit.exe\n"
                               "  services.exe\n"
                               "    svchost.exe\n"
                               "    svchost.exe\n");
}

TEST(ProcessModel, TreeMode_ChildListedBeforeItsParent_StillUnderIt)
{
    ProcessModel model;
    model.setTreeMode(true);

    model.updateProcesses({child(500, "svchost.exe", 400), child(400, "services.exe", 300)}, kNow);

    EXPECT_EQ(treeText(model), "services.exe\n  svchost.exe\n");
}

TEST(ProcessModel, TreeMode_ParentPidReusedByYoungerProcess_NotAParent)
{
    ProcessModel model;
    model.setTreeMode(true);

    // The orphan's parent exited and PID 400 now belongs to a later process.
    model.updateProcesses({child(500, "orphan.exe", 400, 500s), child(400, "newcomer.exe", 0, 900s)}, kNow);

    EXPECT_EQ(treeText(model), "orphan.exe\nnewcomer.exe\n");
}

TEST(ProcessModel, TreeMode_ParentCreatedInSameTick_IsAParent)
{
    ProcessModel model;
    model.setTreeMode(true);

    model.updateProcesses({child(400, "launcher.exe", 0, 500s), child(500, "app.exe", 400, 500s)}, kNow);

    EXPECT_EQ(treeText(model), "launcher.exe\n  app.exe\n");
}

TEST(ProcessModel, TreeMode_ProcessesNamingEachOther_NoCycle)
{
    ProcessModel model;
    model.setTreeMode(true);

    model.updateProcesses({child(10, "a.exe", 20, 5s), child(20, "b.exe", 10, 5s), child(30, "self.exe", 30)}, kNow);

    EXPECT_EQ(treeText(model), "b.exe\n  a.exe\nself.exe\n");
}

TEST(ProcessModel, TreeMode_NewChildOfShownParent_InsertedUnderIt)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(400, "services.exe", 0)}, kNow);
    const QPersistentModelIndex services = model.index(0, 0);
    ModelSignalLog log(model);

    model.updateProcesses({child(400, "services.exe", 0), child(500, "svchost.exe", 400)}, kNow);

    EXPECT_EQ(treeText(model), "services.exe\n  svchost.exe\n");
    EXPECT_EQ(log.inserted, (std::vector<std::pair<int, int>>{{0, 0}}));
    EXPECT_EQ(log.resets, 0);
    EXPECT_TRUE(services.isValid());
}

TEST(ProcessModel, TreeMode_NewParentAndChildren_OneInsertPerParent)
{
    ProcessModel model;
    model.setTreeMode(true);
    ModelSignalLog log(model);

    model.updateProcesses({child(400, "services.exe", 0), child(500, "svchost.exe", 400),
                           child(600, "svchost.exe", 400), child(700, "spoolsv.exe", 400)},
                          kNow);

    EXPECT_EQ(log.inserted, (std::vector<std::pair<int, int>>{{0, 0}, {0, 2}}));
}

TEST(ProcessModel, TreeMode_ParentExits_ChildrenMoveToTopLevel)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(4, "System", 0), child(100, "smss.exe", 4), child(200, "user.exe", 100)}, kNow);
    const QPersistentModelIndex user = model.indexOf(child(200, "user.exe", 100).identity());
    ModelSignalLog log(model);

    model.updateProcesses({child(4, "System", 0), child(200, "user.exe", 100)}, kNow);
    // The exited parent stays, with its child under it, until its highlight ends.
    EXPECT_EQ(treeText(model), "System\n  smss.exe\n    user.exe\n");
    model.updateTime(kLater);

    EXPECT_EQ(treeText(model), "System\nuser.exe\n");
    EXPECT_EQ(log.moves, 1);
    EXPECT_EQ(log.removed.size(), 1u);
    EXPECT_EQ(log.resets, 0);
    ASSERT_TRUE(user.isValid());
    EXPECT_FALSE(user.parent().isValid());
    EXPECT_EQ(user.data().toString(), "user.exe");
}

TEST(ProcessModel, TreeMode_ParentAndChildExitTogether_SurvivorsMoveToTopLevel)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses(
        {child(10, "a.exe", 0), child(20, "b.exe", 10), child(30, "c.exe", 20), child(40, "d.exe", 10)}, kNow);

    model.updateProcesses({child(30, "c.exe", 20), child(40, "d.exe", 10)}, kNow);
    model.updateTime(kLater);

    EXPECT_EQ(treeText(model), "c.exe\nd.exe\n");
}

TEST(ProcessModel, TreeMode_ValuesChange_OneSignalPerParentWithChanges)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses(
        {child(4, "System", 0), child(100, "smss.exe", 4), child(200, "other.exe", 0), child(300, "quiet.exe", 200)},
        kNow);
    ModelSignalLog log(model);

    ProcessInfo system = child(4, "System", 0);
    system.threadCount = 200;
    ProcessInfo smss = child(100, "smss.exe", 4);
    smss.workingSetBytes = 4096;
    ProcessInfo other = child(200, "other.exe", 0);
    other.cpuUsagePercent = 1.0f;
    model.updateProcesses({system, smss, other, child(300, "quiet.exe", 200)}, kNow);

    // The top level's two rows, and System's one child. Nothing changed under other.exe.
    ASSERT_EQ(log.changed.size(), 2u);
    std::ranges::sort(log.changed, {}, &CellRange::lastRow);
    EXPECT_EQ(log.changed[0], (CellRange{.firstRow = 0,
                                         .lastRow = 0,
                                         .firstColumn = static_cast<int>(Column::Memory),
                                         .lastColumn = static_cast<int>(Column::Memory)}));
    EXPECT_EQ(log.changed[1], (CellRange{.firstRow = 0,
                                         .lastRow = 1,
                                         .firstColumn = static_cast<int>(Column::Cpu),
                                         .lastColumn = static_cast<int>(Column::Threads)}));
}

TEST(ProcessModel, SetTreeMode_Toggle_ResetsOnceAndRebuilds)
{
    ProcessModel model;
    model.updateProcesses({child(4, "System", 0), child(100, "smss.exe", 4)}, kNow);
    ModelSignalLog log(model);

    model.setTreeMode(true);
    EXPECT_TRUE(model.isTreeMode());
    EXPECT_EQ(treeText(model), "System\n  smss.exe\n");
    EXPECT_EQ(log.resets, 1);

    model.setTreeMode(true);
    EXPECT_EQ(log.resets, 1);

    model.setTreeMode(false);
    EXPECT_FALSE(model.isTreeMode());
    EXPECT_EQ(treeText(model), "System\nsmss.exe\n");
    EXPECT_EQ(log.resets, 2);
}

TEST(ProcessModel, FlatMode_ParentPids_Ignored)
{
    ProcessModel model;

    model.updateProcesses({child(4, "System", 0), child(100, "smss.exe", 4)}, kNow);

    EXPECT_FALSE(model.isTreeMode());
    EXPECT_EQ(treeText(model), "System\nsmss.exe\n");
}

namespace
{

// Visits every row of model depth first, calling visit(index).
template <typename Model, typename Visit>
void forEachRow(const Model& model, Visit visit, const QModelIndex& parent = {})
{
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const QModelIndex index = model.index(row, 0, parent);
        visit(index);
        forEachRow(model, visit, index);
    }
}

// True when the proxy's filter keeps the process for itself: its name or PID
// contains filter. The churn test's names ("p12") and filters (digits) have no
// letters whose case could differ.
bool matchesFilter(const ProcessInfo& info, const std::string& filter)
{
    return info.imageName.find(filter) != std::string::npos ||
           std::to_string(info.pid).find(filter) != std::string::npos;
}

// True when the proxy should show the row at index: it matches the filter or,
// because the proxy filters recursively, one of its descendants does.
bool isKept(const ProcessModel& model, const QModelIndex& index, const std::string& filter)
{
    if (matchesFilter(*model.process(index), filter)) {
        return true;
    }
    for (int row = 0; row < model.rowCount(index); ++row) {
        if (isKept(model, model.index(row, 0, index), filter)) {
            return true;
        }
    }
    return false;
}

// Checks that the proxy shows under proxyParent the kept children of the row
// it maps to, ordered as a sort by CPU, largest first, orders them: by CPU
// usage (missing lowest), then name, PID and creation time, all reversed.
// Then checks each child's level in turn.
void expectSortedAndFiltered(const ProcessModel& model, const sysmon::ui::ProcessSortFilterProxyModel& proxy,
                             const QModelIndex& proxyParent, const std::string& filter, int step)
{
    const QModelIndex sourceParent = proxy.mapToSource(proxyParent);
    std::vector<const ProcessInfo*> expected;
    for (int row = 0; row < model.rowCount(sourceParent); ++row) {
        const QModelIndex child = model.index(row, 0, sourceParent);
        if (isKept(model, child, filter)) {
            expected.push_back(model.process(child));
        }
    }
    const auto sortKey = [](const ProcessInfo* info) {
        return std::tuple{info->cpuUsagePercent, info->imageName, info->pid, info->creationTime};
    };
    std::ranges::sort(expected, [&sortKey](const ProcessInfo* left, const ProcessInfo* right) {
        return sortKey(right) < sortKey(left);
    });

    ASSERT_EQ(proxy.rowCount(proxyParent), static_cast<int>(expected.size())) << "step " << step;
    for (int row = 0; row < proxy.rowCount(proxyParent); ++row) {
        const QModelIndex index = proxy.index(row, 0, proxyParent);
        ASSERT_EQ(model.process(proxy.mapToSource(index))->identity(),
                  expected[static_cast<std::size_t>(row)]->identity())
            << "step " << step << ", row " << row;
        expectSortedAndFiltered(model, proxy, index, filter, step);
    }
}

} // namespace

TEST(ProcessModel, RandomChurnWithFilterAndModeChanges_ModelAndSortedFilteredProxyStayConsistent)
{
    using std::chrono::seconds;
    using std::chrono::steady_clock;

    std::mt19937 random(20261014);
    ProcessModel model;
    model.setTreeMode(true);
    // A sorted, filtered proxy on top, as in the view, checks that the signals are consistent.
    sysmon::ui::ProcessSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.sort(static_cast<int>(Column::Cpu), Qt::DescendingOrder);
    // The filter changes every 40 refreshes, and the mode every 25, as a user would change them.
    const std::array<std::string, 3> filters{"1", "", "6"};
    std::string filter;

    std::vector<ProcessInfo> live;
    // When each process left the list or joined it, as the model was told.
    std::unordered_map<ProcessIdentity, steady_clock::time_point, ProcessIdentityHash> exitedAt;
    std::unordered_map<ProcessIdentity, steady_clock::time_point, ProcessIdentityHash> startedAt;
    std::vector<std::pair<QPersistentModelIndex, ProcessIdentity>> tracked;

    const auto isLive = [&live](const ProcessIdentity& identity) {
        return std::ranges::any_of(live, [&identity](const ProcessInfo& info) { return info.identity() == identity; });
    };
    const auto isRecent = [](const auto& times, const ProcessIdentity& identity, steady_clock::time_point now) {
        const auto it = times.find(identity);
        return it != times.end() && now - it->second < ProcessModel::kHighlightDuration;
    };
    const auto check = [&](steady_clock::time_point now, int step) {
        for (const auto& [index, identity] : tracked) {
            const bool isShown = isLive(identity) || isRecent(exitedAt, identity, now);
            ASSERT_EQ(index.isValid(), isShown) << "step " << step;
            if (isShown) {
                ASSERT_EQ(model.process(index)->identity(), identity) << "step " << step;
            }
        }

        std::size_t rows = 0;
        forEachRow(model, [&](const QModelIndex& index) {
            ++rows;
            const ProcessInfo* childInfo = model.process(index);
            ASSERT_EQ(model.indexOf(childInfo->identity()), index);
            const bool isHighlighted =
                isRecent(exitedAt, childInfo->identity(), now) ||
                (isLive(childInfo->identity()) && isRecent(startedAt, childInfo->identity(), now));
            EXPECT_EQ(index.data(Qt::BackgroundRole).isValid(), isHighlighted) << "step " << step;
            const QModelIndex parent = model.parent(index);
            if (parent.isValid()) {
                const ProcessInfo* parentInfo = model.process(parent);
                EXPECT_EQ(parentInfo->pid, childInfo->parentPid);
                EXPECT_LE(parentInfo->creationTime, childInfo->creationTime);
                // A process that had exited before the child was first listed
                // cannot have started it. A child without a start time was in
                // the first list, before anything exited.
                const auto parentExited = exitedAt.find(parentInfo->identity());
                const auto childStarted = startedAt.find(childInfo->identity());
                if (parentExited != exitedAt.end() && childStarted != startedAt.end()) {
                    EXPECT_GE(parentExited->second, childStarted->second) << "step " << step;
                }
            }
        });
        const auto recentlyExited = std::ranges::count_if(
            exitedAt, [now](const auto& entry) { return now - entry.second < ProcessModel::kHighlightDuration; });
        ASSERT_EQ(rows, live.size() + static_cast<std::size_t>(recentlyExited)) << "step " << step;

        expectSortedAndFiltered(model, proxy, QModelIndex{}, filter, step);
    };

    int64_t clock = 1;
    for (int step = 0; step < 300; ++step) {
        // A refresh every 2 seconds and a tick without a list between them, as the scheduler sends them.
        const auto now = kNow + seconds{2 * step};
        if (step % 40 == 20) {
            filter = filters[static_cast<std::size_t>(step / 40) % filters.size()];
            proxy.setFilterFixedString(QString::fromStdString(filter));
        }
        // Switching the mode resets the model, so it happens before the rows are tracked.
        if (step % 25 == 24) {
            model.setTreeMode(!model.isTreeMode());
        }
        tracked.clear();
        forEachRow(model, [&](const QModelIndex& index) {
            tracked.emplace_back(QPersistentModelIndex(index), model.process(index)->identity());
        });

        std::erase_if(live, [&](const ProcessInfo& info) {
            const bool exits = random() % 8 == 0;
            if (exits) {
                exitedAt[info.identity()] = now;
            }
            return exits;
        });
        for (auto& info : live) {
            info.cpuUsagePercent = static_cast<float>(random() % 100);
        }
        // A PID is reused only after its process exits, so the new process is
        // created later and has a different identity.
        ++clock;
        const auto starts = random() % 6;
        for (unsigned start = 0; start < starts; ++start) {
            // A small PID space makes reuse common. Two running processes never share a PID.
            const auto pid = static_cast<uint32_t>(4 * (1 + random() % 40));
            if (std::ranges::any_of(live, [pid](const ProcessInfo& info) { return info.pid == pid; })) {
                continue;
            }
            // Processes started in one step share a clock tick about half the time.
            clock += static_cast<int64_t>(random() % 2);
            const uint32_t parentPid = !live.empty() && random() % 4 != 0 ? live[random() % live.size()].pid
                                                                          : static_cast<uint32_t>(4 * (random() % 41));
            ProcessInfo info = child(pid, "p" + std::to_string(pid), parentPid, std::chrono::seconds{clock});
            info.cpuUsagePercent = static_cast<float>(random() % 100);
            if (step > 0) {
                startedAt[info.identity()] = now;
            }
            live.push_back(info);
        }

        model.updateProcesses(live, now);
        check(now, step);
        model.updateTime(now + seconds{1});
        check(now + seconds{1}, step);
    }
}

TEST(ProcessModel, TreeMode_ChildOfProcessThatJustExited_ShownUnderIt)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(4, "System", 0), child(400, "launcher.exe", 4)}, kNow);

    // The launcher started a child and exited between two refreshes.
    const auto refresh = kNow + std::chrono::seconds{2};
    model.updateProcesses({child(4, "System", 0), child(500, "app.exe", 400)}, refresh);

    EXPECT_EQ(treeText(model), "System\n  launcher.exe\n    app.exe\n");
    model.updateTime(refresh + ProcessModel::kHighlightDuration);
    EXPECT_EQ(treeText(model), "System\napp.exe\n");
}

TEST(ProcessModel, TreeMode_ChildListedAfterExitedRowWasMarked_NotUnderIt)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(4, "System", 0), child(100, "old.exe", 4)}, kNow);
    model.updateProcesses({child(4, "System", 0)}, kNow + std::chrono::seconds{1});

    // A launcher that was never listed reused PID 100, started child.exe and
    // exited, all after old.exe had exited. old.exe's row is still shown.
    model.updateProcesses({child(4, "System", 0), child(500, "child.exe", 100)}, kNow + std::chrono::seconds{2});

    EXPECT_EQ(treeText(model), "System\n  old.exe\nchild.exe\n");
}

TEST(ProcessModel, SetTreeMode_ChildListedAfterExitedRowWasMarked_NotLinkedToIt)
{
    ProcessModel model;
    model.updateProcesses({child(4, "System", 0), child(100, "old.exe", 4)}, kNow);
    model.updateProcesses({child(4, "System", 0)}, kNow + std::chrono::seconds{1});
    model.updateProcesses({child(4, "System", 0), child(500, "child.exe", 100)}, kNow + std::chrono::seconds{2});

    model.setTreeMode(true);

    EXPECT_EQ(treeText(model), "System\n  old.exe\nchild.exe\n");
}

TEST(ProcessModel, TreeMode_PidReusedWhileExitedRowShown_ChildFindsTheNewProcess)
{
    ProcessModel model;
    model.setTreeMode(true);
    model.updateProcesses({child(400, "old.exe", 0, 400s)}, kNow);

    model.updateProcesses({child(400, "new.exe", 0, 450s), child(500, "app.exe", 400, 460s)}, kNow);

    EXPECT_EQ(treeText(model), "old.exe\nnew.exe\n  app.exe\n");
}
