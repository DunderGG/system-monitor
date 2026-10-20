#include <array>
#include <chrono>

#include <gtest/gtest.h>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLocale>
#include <QPoint>
#include <QPushButton>
#include <QString>

#include "domain/settings.h"
#include "support/settings_samples.h"
#include "ui/settings_dialog.h"

using namespace sysmon::domain;
using std::chrono::milliseconds;
using sysmon::tests::nonDefaultSettings;
using sysmon::ui::SettingsDialog;

namespace
{

// Selects the item of comboBox that stands for value.
template <typename Enum> void selectItem(QComboBox* comboBox, Enum value)
{
    comboBox->setCurrentIndex(comboBox->findData(static_cast<int>(value)));
}

template <typename Enum> Enum selectedItem(const QComboBox* comboBox)
{
    return static_cast<Enum>(comboBox->currentData().toInt());
}

// Clicks one of the standard buttons of dialog's button box.
void clickButton(const SettingsDialog& dialog, QDialogButtonBox::StandardButton button)
{
    dialog.buttonBox()->button(button)->click();
}

} // namespace

TEST(SettingsDialog, Construction_ControlsShowTheSettings)
{
    const SettingsDialog dialog(nonDefaultSettings());

    EXPECT_DOUBLE_EQ(dialog.refreshIntervalSpinBox()->value(), 2.5);
    EXPECT_EQ(selectedItem<LogLevel>(dialog.logLevelComboBox()), LogLevel::Debug);
    EXPECT_EQ(selectedItem<ByteUnits>(dialog.byteUnitsComboBox()), ByteUnits::Decimal);
    EXPECT_EQ(selectedItem<NetworkRateUnits>(dialog.networkRateUnitsComboBox()), NetworkRateUnits::BitsPerSecond);
    EXPECT_FALSE(dialog.cpuPanelCheckBox()->isChecked());
    EXPECT_FALSE(dialog.memoryPanelCheckBox()->isChecked());
    EXPECT_FALSE(dialog.diskPanelCheckBox()->isChecked());
    EXPECT_FALSE(dialog.networkPanelCheckBox()->isChecked());
    EXPECT_FALSE(dialog.uptimePanelCheckBox()->isChecked());
}

TEST(SettingsDialog, ControlsEdited_SettingsReturnsWhatTheyShow)
{
    const SettingsDialog dialog(Settings{});

    dialog.refreshIntervalSpinBox()->setValue(2.5);
    selectItem(dialog.logLevelComboBox(), LogLevel::Debug);
    selectItem(dialog.byteUnitsComboBox(), ByteUnits::Decimal);
    selectItem(dialog.networkRateUnitsComboBox(), NetworkRateUnits::BitsPerSecond);
    dialog.cpuPanelCheckBox()->setChecked(false);
    dialog.memoryPanelCheckBox()->setChecked(false);
    dialog.diskPanelCheckBox()->setChecked(false);
    dialog.networkPanelCheckBox()->setChecked(false);
    dialog.uptimePanelCheckBox()->setChecked(false);

    EXPECT_EQ(dialog.settings(), nonDefaultSettings());
}

TEST(SettingsDialog, SetSettings_EachLogLevelAndUnit_SettingsReturnsIt)
{
    struct Case
    {
        const char* name{};
        Settings settings{};
    };
    const std::array cases{
        Case{"Trace", {.logLevel = LogLevel::Trace}},
        Case{"Debug", {.logLevel = LogLevel::Debug}},
        Case{"Info", {.logLevel = LogLevel::Info}},
        Case{"Warn", {.logLevel = LogLevel::Warn}},
        Case{"Error", {.logLevel = LogLevel::Error}},
        Case{"Critical", {.logLevel = LogLevel::Critical}},
        Case{"Off", {.logLevel = LogLevel::Off}},
        Case{"Binary", {.displayUnits = {.bytes = ByteUnits::Binary}}},
        Case{"Decimal", {.displayUnits = {.bytes = ByteUnits::Decimal}}},
        Case{"BytesPerSecond", {.displayUnits = {.networkRates = NetworkRateUnits::BytesPerSecond}}},
        Case{"BitsPerSecond", {.displayUnits = {.networkRates = NetworkRateUnits::BitsPerSecond}}},
    };
    // Starts from settings that differ in every value, so each case's value is set, not kept.
    SettingsDialog dialog(nonDefaultSettings());

    for (const auto& [name, settings] : cases) {
        SCOPED_TRACE(name);
        dialog.setSettings(settings);
        EXPECT_EQ(dialog.settings(), settings);
        // A value without an item would leave the combo box with none selected.
        EXPECT_NE(dialog.logLevelComboBox()->currentIndex(), -1);
        EXPECT_NE(dialog.byteUnitsComboBox()->currentIndex(), -1);
        EXPECT_NE(dialog.networkRateUnitsComboBox()->currentIndex(), -1);
    }
}

TEST(SettingsDialog, IntervalTypedBetweenSteps_NearerStepReturnedAndShownWhenEditingEnds)
{
    const SettingsDialog dialog(Settings{});
    auto* spinBox = dialog.refreshIntervalSpinBox();

    spinBox->setValue(1.3);
    const milliseconds beforeEditingEnds = dialog.settings().refreshInterval;
    emit spinBox->editingFinished();

    EXPECT_EQ(beforeEditingEnds, milliseconds{1500});
    EXPECT_DOUBLE_EQ(spinBox->value(), 1.5);
    EXPECT_EQ(dialog.settings().refreshInterval, milliseconds{1500});
    EXPECT_DOUBLE_EQ(spinBox->singleStep(), 0.5);
}

TEST(SettingsDialog, IntervalUnderACommaLocale_ShownWithADecimalPoint)
{
    const QLocale previous;
    QLocale::setDefault(QLocale(QLocale::Swedish, QLocale::Sweden));
    const SettingsDialog dialog(Settings{.refreshInterval = milliseconds{2500}});
    const QString text = dialog.refreshIntervalSpinBox()->text();
    QLocale::setDefault(previous);

    EXPECT_EQ(text, "2.5 s");
}

TEST(SettingsDialog, SetSettings_IntervalThatCannotBeChosen_ShowsTheNearestThatCan)
{
    struct Case
    {
        const char* name{};
        milliseconds interval{};
        milliseconds expected{};
    };
    const std::array cases{
        Case{"BelowMinimum", milliseconds{100}, kMinRefreshInterval},
        Case{"AboveMaximum", milliseconds{20'000}, kMaxRefreshInterval},
        Case{"BetweenSteps", milliseconds{1234}, milliseconds{1000}},
    };
    SettingsDialog dialog(Settings{});

    for (const auto& [name, interval, expected] : cases) {
        SCOPED_TRACE(name);
        dialog.setSettings(Settings{.refreshInterval = interval});
        EXPECT_EQ(dialog.settings().refreshInterval, expected);
        EXPECT_DOUBLE_EQ(dialog.refreshIntervalSpinBox()->value(), static_cast<double>(expected.count()) / 1000.0);
    }
}

TEST(SettingsDialog, RestoreDefaults_ShowsTheDefaultsAndStaysOpen)
{
    SettingsDialog dialog(nonDefaultSettings());
    dialog.setAttribute(Qt::WA_DontShowOnScreen);
    dialog.show();

    clickButton(dialog, QDialogButtonBox::RestoreDefaults);

    EXPECT_EQ(dialog.settings(), Settings{});
    EXPECT_TRUE(dialog.isVisible());
}

TEST(SettingsDialog, Shown_FieldsOfGeneralAndUnitsLineUp)
{
    SettingsDialog dialog(Settings{});
    dialog.setAttribute(Qt::WA_DontShowOnScreen);

    dialog.show();

    const int intervalLeft = dialog.refreshIntervalSpinBox()->mapTo(&dialog, QPoint{}).x();
    EXPECT_EQ(dialog.byteUnitsComboBox()->mapTo(&dialog, QPoint{}).x(), intervalLeft);
    EXPECT_EQ(dialog.networkRateUnitsComboBox()->mapTo(&dialog, QPoint{}).x(), intervalLeft);
}

TEST(SettingsDialog, OkAndCancel_AcceptAndReject)
{
    struct Case
    {
        const char* name{};
        QDialogButtonBox::StandardButton button{};
        QDialog::DialogCode expected{};
    };
    const std::array cases{
        Case{"Ok", QDialogButtonBox::Ok, QDialog::Accepted},
        Case{"Cancel", QDialogButtonBox::Cancel, QDialog::Rejected},
    };

    for (const auto& [name, button, expected] : cases) {
        SCOPED_TRACE(name);
        SettingsDialog dialog(Settings{});
        dialog.setAttribute(Qt::WA_DontShowOnScreen);
        dialog.show();

        clickButton(dialog, button);

        EXPECT_EQ(dialog.result(), expected);
        EXPECT_FALSE(dialog.isVisible());
    }
}
