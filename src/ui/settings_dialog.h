#pragma once

#include <chrono>

#include <QDialog>

#include "domain/settings.h"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;

namespace sysmon::ui
{

/**
 * Settings dialog: edits a copy of the user's preferences (DC-124). It shows
 * the refresh interval in seconds, the log level, the byte and network
 * throughput units, and a check box for each dashboard card. OK accepts,
 * Cancel and Escape reject, and Restore Defaults sets every control to the
 * defaults without closing the dialog. The dialog changes nothing itself:
 * whoever opened it reads settings() once it is accepted.
 *
 * The interval is chosen in steps of half a second
 * (domain::nearestRefreshInterval()): setSettings() shows a given interval as
 * the nearest step, a value typed between two steps moves to the nearer one
 * when editing ends, and settings() returns the step the spin box is nearest.
 *
 * Each combo box's items hold their enumerator, cast to int, as item data.
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /** Opens with the controls showing settings. */
    explicit SettingsDialog(const domain::Settings& settings, QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    /** Sets every control to show settings, as Restore Defaults does with the defaults. */
    void setSettings(const domain::Settings& settings);

    /** Returns the settings that the controls show. */
    [[nodiscard]] domain::Settings settings() const;

    [[nodiscard]] QDoubleSpinBox* refreshIntervalSpinBox() const;
    [[nodiscard]] QComboBox* logLevelComboBox() const;
    [[nodiscard]] QComboBox* byteUnitsComboBox() const;
    [[nodiscard]] QComboBox* networkRateUnitsComboBox() const;
    [[nodiscard]] QCheckBox* cpuPanelCheckBox() const;
    [[nodiscard]] QCheckBox* memoryPanelCheckBox() const;
    [[nodiscard]] QCheckBox* diskPanelCheckBox() const;
    [[nodiscard]] QCheckBox* networkPanelCheckBox() const;
    [[nodiscard]] QCheckBox* uptimePanelCheckBox() const;
    [[nodiscard]] QDialogButtonBox* buttonBox() const;

private:
    // The interval that can be chosen nearest to the one the spin box shows.
    [[nodiscard]] std::chrono::milliseconds shownRefreshInterval() const;

    QDoubleSpinBox* m_refreshIntervalSpinBox{nullptr};
    QComboBox* m_logLevelComboBox{nullptr};
    QComboBox* m_byteUnitsComboBox{nullptr};
    QComboBox* m_networkRateUnitsComboBox{nullptr};
    QCheckBox* m_cpuPanelCheckBox{nullptr};
    QCheckBox* m_memoryPanelCheckBox{nullptr};
    QCheckBox* m_diskPanelCheckBox{nullptr};
    QCheckBox* m_networkPanelCheckBox{nullptr};
    QCheckBox* m_uptimePanelCheckBox{nullptr};
    QDialogButtonBox* m_buttonBox{nullptr};
};

} // namespace sysmon::ui
