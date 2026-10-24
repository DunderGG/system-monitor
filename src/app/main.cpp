#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <spdlog/spdlog.h>

#include "app_logging.h"
#include "domain/settings.h"
#include "file_locations.h"
#include "instance_server.h"
#include "monitoring/sampling_scheduler.h"
#include "persistence/settings_file.h"
#include "platform/windows/current_process.h"
#include "platform/windows/process_terminator.h"
#include "power_event_filter.h"
#include "settings_service.h"
#include "single_instance.h"
#include "system_collectors.h"
#include "ui/main_window.h"
#include "ui/processes_view.h"
#include "verdandi_product_version.h"
#include "verdandi_version.h"

namespace
{

// Returns the log levels' names as a sentence lists them: "trace, debug, ..., or off".
QString logLevelNameList()
{
    QString list;
    for (const verdandi::domain::LogLevelName& entry : verdandi::domain::kLogLevelNames) {
        if (!list.isEmpty()) {
            list += entry.level == verdandi::domain::kLogLevelNames.back().level ? ", or " : ", ";
        }
        list += QString::fromUtf8(entry.name.data(), static_cast<qsizetype>(entry.name.size()));
    }
    return list;
}

// Returns a path that Qt gives, with forward slashes, with the backslashes
// that the log and the message boxes show.
std::filesystem::path nativePath(const QString& path)
{
    return std::filesystem::path{QDir::toNativeSeparators(path).toStdWString()};
}

// Runs the monitor until its window closes and returns the exit code. The
// scheduler, its collectors and the window are local to this function, so
// they are destroyed when it returns, before main() shuts the logger down:
// their teardown can log, and spdlog's free functions dereference the default
// logger without a check once spdlog::shutdown() has dropped it.
// logFileProblem is why the log file at logFilePath cannot be written, if it
// cannot. instanceClaim holds the instance mutex, if this process created it.
int runMonitor(QApplication& application, const verdandi::domain::Settings& settings,
               const std::filesystem::path& settingsPath, const std::filesystem::path& logFilePath,
               const std::optional<std::string>& logFileProblem, const verdandi::app::InstanceClaim& instanceClaim)
{
    verdandi::monitoring::SamplingScheduler scheduler;
    verdandi::app::registerSystemCollectors(scheduler);
    verdandi::app::applyRefreshInterval(scheduler, settings.refreshInterval);
    verdandi::app::SettingsService settingsService(settingsPath, settings, scheduler);
    // Sampling pauses while the system sleeps, and no rate spans the sleep (DC-129).
    verdandi::app::PowerEventFilter powerEventFilter(scheduler);
    application.installNativeEventFilter(&powerEventFilter);

    verdandi::ui::MainWindow mainWindow;
    mainWindow.setSettings(settings);
    mainWindow.processesView()->setProcessTerminator(verdandi::platform::terminateOwnedProcess);
    if (const auto monitor = verdandi::platform::currentProcessIdentity()) {
        mainWindow.processesView()->setMonitorIdentity(*monitor);
    }

    QObject::connect(&scheduler, &verdandi::monitoring::SamplingScheduler::snapshotReady, &mainWindow,
                     &verdandi::ui::MainWindow::onSnapshotReady, Qt::QueuedConnection);
    // The window applies what its views show; the service applies the rest and saves the file (DC-127).
    QObject::connect(&mainWindow, &verdandi::ui::MainWindow::settingsChanged, &mainWindow,
                     [&settingsService, &mainWindow](const verdandi::domain::Settings& changed) {
                         if (const auto error = settingsService.change(changed)) {
                             mainWindow.showSettingsNotSaved(QString::fromStdWString(settingsService.path().wstring()),
                                                             QString::fromStdString(*error));
                         }
                     });

    // Later starts ask through a server named after the mutex to bring the
    // window to the front (DC-142). Declared after the window, so that it
    // stops listening when this function returns, before the window is
    // destroyed; a later start then waits for the mutex.
    verdandi::app::InstanceServer instanceServer([&mainWindow] { mainWindow.bringToFront(); });
    if (instanceClaim.mutex) {
        if (const auto problem = instanceServer.listen(QString::fromStdString(instanceClaim.mutex->name()))) {
            spdlog::warn("Later starts of Verdandi cannot bring this window to the front: {}", *problem);
        }
    }

    scheduler.start();
    mainWindow.show();
    if (logFileProblem) {
        mainWindow.showLogNotWritten(QString::fromStdWString(logFilePath.wstring()),
                                     QString::fromStdString(*logFileProblem));
    }

    const int exitCode = application.exec();

    scheduler.stop();
    return exitCode;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    // With no organization name, Qt's folders for the settings and the log are
    // %APPDATA%\Verdandi and %LOCALAPPDATA%\Verdandi, which hold them unless
    // the monitor is portable (DC-131). --version and the About box give the
    // version with the commit of the build (DC-136).
    QCoreApplication::setApplicationName("Verdandi");
    QCoreApplication::setApplicationVersion(VERDANDI_PRODUCT_VERSION);

    // A GUI program has no console, so Qt shows the help, the version and
    // these errors in a message box, unless standard output is redirected
    // (DC-134).
    const QString logLevelNames = logLevelNameList();
    QCommandLineParser commandLineParser;
    commandLineParser.setApplicationDescription(VERDANDI_DESCRIPTION);
    commandLineParser.addHelpOption();
    commandLineParser.addVersionOption();
    commandLineParser.addOption({
        {"l", "log-level"},
        QString("Set the log level for this run, in place of the one in the settings: %1.").arg(logLevelNames),
        "level",
    });
    commandLineParser.process(application);

    std::optional<verdandi::domain::LogLevel> commandLineLogLevel;
    if (commandLineParser.isSet("log-level")) {
        const QString logLevelName = commandLineParser.value("log-level");
        commandLineLogLevel = verdandi::domain::parseLogLevel(logLevelName.toStdString());
        if (!commandLineLogLevel) {
            // Worded as Qt words an unknown option.
            QCommandLineParser::showMessageAndExit(
                QCommandLineParser::MessageType::Error,
                QString("%1: Unknown log level '%2'. Use %3.")
                    .arg(QCoreApplication::applicationName(), logLevelName, logLevelNames),
                EXIT_FAILURE);
        }
    }

    // One monitor runs for each user in each logon session, since two would
    // share the settings and the log (DC-142). A later start asks the running
    // one to bring its window to the front and exits, before it reads the
    // settings or opens the log, and without using its options. instanceClaim
    // holds the mutex until main() returns, after the log has been closed.
    const verdandi::app::InstanceClaim instanceClaim = verdandi::app::claimSingleInstance(
        QCoreApplication::applicationName().toStdString(), verdandi::app::kRunningInstanceTimeout);
    if (instanceClaim.result == verdandi::app::InstanceClaimResult::BroughtToFront) {
        // The default logger writes to standard output alone, if it is redirected (DC-134).
        spdlog::info("Verdandi is already running, and brought its window to the front");
        return EXIT_SUCCESS;
    }
    if (instanceClaim.result == verdandi::app::InstanceClaimResult::NotAnswered) {
        QCommandLineParser::showMessageAndExit(
            QCommandLineParser::MessageType::Error,
            QString("Verdandi is already running, but did not respond within %1 seconds. If its window does not "
                    "appear, end Verdandi in Task Manager and start it again.")
                .arg(std::chrono::duration_cast<std::chrono::seconds>(verdandi::app::kRunningInstanceTimeout).count()),
            EXIT_FAILURE);
    }

    // The settings are read before the logger is configured, because they hold
    // its level; what reading them found is logged once it is (DC-127). They
    // roam with the user's profile, as Windows keeps settings, and the log
    // stays on this machine, unless a settings file beside verdandi.exe keeps
    // both beside it (DC-131). The first save creates the settings' directory.
    const verdandi::app::FileLocations fileLocations = verdandi::app::chooseFileLocations(
        nativePath(QCoreApplication::applicationDirPath()),
        nativePath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)),
        nativePath(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)));
    const std::filesystem::path& settingsPath = fileLocations.settingsPath;
    const auto loadedSettings = verdandi::persistence::loadSettingsFile(settingsPath);
    const verdandi::domain::Settings settings =
        loadedSettings ? loadedSettings->settings : verdandi::domain::Settings{};

    // A log file that cannot be written leaves the log to a redirected
    // standard output alone, and the window says so once (DC-135).
    const std::filesystem::path& logFilePath = fileLocations.logFilePath;
    const verdandi::domain::LogLevel logLevel = commandLineLogLevel.value_or(settings.logLevel);
    const auto logFileProblem = verdandi::app::configureLogging(logFilePath, verdandi::app::toSpdlogLevel(logLevel));
    spdlog::info("Verdandi {} started with log level '{}' from {}", VERDANDI_PRODUCT_VERSION,
                 verdandi::domain::logLevelName(logLevel),
                 commandLineLogLevel ? "the --log-level option" : "the settings");
    if (fileLocations.isPortable) {
        spdlog::info("Portable: a settings file beside verdandi.exe keeps the settings and the log beside it");
    }
    if (logFileProblem) {
        spdlog::error("Log file {} cannot be written: {}", logFilePath.string(), *logFileProblem);
    }
    verdandi::app::logSettingsRead(settingsPath, loadedSettings);
    if (instanceClaim.result == verdandi::app::InstanceClaimResult::Unchecked) {
        spdlog::warn("Cannot tell whether Verdandi is already running, so this one runs regardless: {}",
                     instanceClaim.problem);
    }

    const int exitCode = runMonitor(application, settings, settingsPath, logFilePath, logFileProblem, instanceClaim);

    spdlog::info("Application exited with code {}", exitCode);
    spdlog::shutdown();
    return exitCode;
}
