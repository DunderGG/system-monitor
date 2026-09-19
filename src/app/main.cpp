#include "app_logging.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QMainWindow>
#include <QStandardPaths>

#include <spdlog/spdlog.h>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    QCoreApplication::setApplicationName("System Monitor");
    QCoreApplication::setOrganizationName("System Monitor");

    QCommandLineParser commandLineParser;
    commandLineParser.setApplicationDescription("A lightweight Windows desktop system monitor");
    commandLineParser.addHelpOption();
    commandLineParser.addOption({
        {"l", "log-level"},
        "Set the development log level: trace, debug, info, warn, error, critical, or off.",
        "level",
        "info",
    });
    commandLineParser.process(application);

    const std::string logLevelValue = commandLineParser.value("log-level").toStdString();
    const auto logLevel = sysmon::app::parseLogLevel(logLevelValue);
    if (!logLevel.has_value()) {
        commandLineParser.showHelp(EXIT_FAILURE);
    }

    const QString logDirectory = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(logDirectory);
    const auto logFilePath = std::filesystem::path{logDirectory.toStdWString()} / "system_monitor.log";
    sysmon::app::configureLogging(logFilePath, *logLevel);
    spdlog::info("Application started with log level '{}'", logLevelValue);

    QMainWindow mainWindow;
    mainWindow.show();

    const int exitCode = application.exec();
    spdlog::info("Application exited with code {}", exitCode);
    spdlog::shutdown();
    return exitCode;
}
