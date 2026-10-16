#include <array>

#include <gtest/gtest.h>
#include <QColor>
#include <QPalette>
#include <QWidget>

#include "text_contrast.h"
#include "ui/text_tone.h"

using sysmon::test::blendedOver;
using sysmon::test::contrastRatio;
using sysmon::test::kMinimumTextContrast;
using sysmon::ui::dimmedTextColor;
using sysmon::ui::TextTone;
using sysmon::ui::textToneColor;
using sysmon::ui::TonedLabel;

namespace
{

constexpr std::array kTones{TextTone::Normal, TextTone::Dimmed, TextTone::Good, TextTone::Warning, TextTone::Critical};

// The text, window and base colours of Qt's light and dark Windows palettes
// with the windows11 style, which draws views on the base colour.
struct Scheme
{
    QColor text;
    QColor window;
    QColor base;
};

const Scheme kLight{
    .text = QColor(0x00, 0x00, 0x00), .window = QColor(0xf3, 0xf3, 0xf3), .base = QColor(0xff, 0xff, 0xff)};
const Scheme kDark{
    .text = QColor(0xff, 0xff, 0xff), .window = QColor(0x1e, 0x1e, 0x1e), .base = QColor(0x2d, 0x2d, 0x2d)};

QPalette paletteOf(const Scheme& scheme)
{
    QPalette palette;
    palette.setColor(QPalette::WindowText, scheme.text);
    palette.setColor(QPalette::Window, scheme.window);
    return palette;
}

QColor drawnColor(const TonedLabel& label)
{
    return label.palette().color(label.foregroundRole());
}

} // namespace

TEST(TextTone, TextToneColor_EveryToneReadableOnWindowAndBaseOfBothSchemes)
{
    for (const Scheme& scheme : {kLight, kDark}) {
        for (const TextTone tone : kTones) {
            const QColor color = textToneColor(tone, scheme.text, scheme.window);
            for (const QColor& background : {scheme.window, scheme.base}) {
                EXPECT_GE(contrastRatio(blendedOver(color, background), background), kMinimumTextContrast)
                    << "tone " << static_cast<int>(tone) << " on " << background.name().toStdString();
            }
        }
    }
}

TEST(TextTone, DimmedTextColor_TextColourAtReducedOpacity)
{
    const QColor dimmed = dimmedTextColor(QColor(0x12, 0x34, 0x56));

    EXPECT_EQ(dimmed.rgb(), QColor(0x12, 0x34, 0x56).rgb());
    EXPECT_NEAR(dimmed.alphaF(), sysmon::ui::kDimmedTextAlpha, 0.01);
}

TEST(TextTone, TextToneColor_StatusTonesLighterOnDarkBackground)
{
    for (const TextTone tone : {TextTone::Good, TextTone::Warning, TextTone::Critical}) {
        const QColor onLight = textToneColor(tone, kLight.text, kLight.window);
        const QColor onDark = textToneColor(tone, kDark.text, kDark.window);
        EXPECT_GT(onDark.lightnessF(), onLight.lightnessF()) << static_cast<int>(tone);
    }
}

TEST(TonedLabel, StatusTones_BoldOthersNot)
{
    TonedLabel label;

    EXPECT_FALSE(label.font().bold());
    for (const TextTone tone : kTones) {
        label.setTone(tone);
        const bool isStatus = tone == TextTone::Good || tone == TextTone::Warning || tone == TextTone::Critical;
        EXPECT_EQ(label.font().bold(), isStatus) << static_cast<int>(tone);
        EXPECT_EQ(label.tone(), tone);
    }
}

TEST(TonedLabel, ParentPaletteChanges_ColourFollowsTheNewScheme)
{
    QWidget parent;
    parent.setPalette(paletteOf(kLight));
    auto* good = new TonedLabel(TextTone::Good, &parent);
    auto* dimmed = new TonedLabel(TextTone::Dimmed, &parent);
    ASSERT_EQ(drawnColor(*good), textToneColor(TextTone::Good, kLight.text, kLight.window));
    ASSERT_EQ(drawnColor(*dimmed), dimmedTextColor(kLight.text));

    parent.setPalette(paletteOf(kDark));

    EXPECT_EQ(drawnColor(*good), textToneColor(TextTone::Good, kDark.text, kDark.window));
    EXPECT_EQ(drawnColor(*dimmed), dimmedTextColor(kDark.text));
}

TEST(TonedLabel, SetTone_ChangesTheColour)
{
    QWidget parent;
    parent.setPalette(paletteOf(kDark));
    auto* label = new TonedLabel(TextTone::Dimmed, &parent);

    label->setTone(TextTone::Critical);

    EXPECT_EQ(drawnColor(*label), textToneColor(TextTone::Critical, kDark.text, kDark.window));
}
