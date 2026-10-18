#include "ui/text_tone.h"

#include <QEvent>
#include <QFont>
#include <QPalette>

namespace sysmon::ui
{

namespace
{

// A darker shade for a light background and a lighter one for a dark
// background. Each reaches 4.5:1 on the light palette's window (#f3f3f3) and
// base (#ffffff) colours, or on the dark palette's window (#1e1e1e) and base
// (#2d2d2d) colours (docs/coding_guidelines.md, "Text colours").
constexpr QColor kGoodOnLight{0x1e, 0x7b, 0x45};
constexpr QColor kGoodOnDark{0x3c, 0xb3, 0x71};
constexpr QColor kWarningOnLight{0xa0, 0x5a, 0x00};
constexpr QColor kWarningOnDark{0xd9, 0x82, 0x00};
constexpr QColor kCriticalOnLight{0xc6, 0x28, 0x28};
constexpr QColor kCriticalOnDark{0xff, 0x6b, 0x6b};

// The label keeps its tone's colour in this role, which a label does not
// otherwise use, and draws its text with it. It never sets WindowText or
// Window, so they stay inherited and describe the current colour scheme
// whenever the palette changes.
constexpr QPalette::ColorRole kToneRole = QPalette::PlaceholderText;

bool isBold(TextTone tone)
{
    return tone == TextTone::Good || tone == TextTone::Warning || tone == TextTone::Critical;
}

} // namespace

QColor dimmedTextColor(const QColor& textColor)
{
    QColor color = textColor;
    color.setAlphaF(static_cast<float>(kDimmedTextAlpha));
    return color;
}

QColor textToneColor(TextTone tone, const QColor& textColor, const QColor& background)
{
    const bool isDark = background.lightnessF() < 0.5f;
    switch (tone) {
        case TextTone::Dimmed:
            return dimmedTextColor(textColor);
        case TextTone::Good:
            return isDark ? kGoodOnDark : kGoodOnLight;
        case TextTone::Warning:
            return isDark ? kWarningOnDark : kWarningOnLight;
        case TextTone::Critical:
            return isDark ? kCriticalOnDark : kCriticalOnLight;
        case TextTone::Normal:
        default:
            return textColor;
    }
}

TonedLabel::TonedLabel(TextTone tone, QWidget* parent) : QLabel(parent), m_tone(tone)
{
    setForegroundRole(kToneRole);
    applyTone();
}

void TonedLabel::setTone(TextTone tone)
{
    if (tone == m_tone) {
        return;
    }
    m_tone = tone;
    applyTone();
}

TextTone TonedLabel::tone() const
{
    return m_tone;
}

void TonedLabel::changeEvent(QEvent* event)
{
    QLabel::changeEvent(event);
    if (event->type() == QEvent::PaletteChange) {
        applyTone();
    }
}

void TonedLabel::applyTone()
{
    if (font().bold() != isBold(m_tone)) {
        QFont toneFont = font();
        toneFont.setBold(isBold(m_tone));
        setFont(toneFont);
    }

    // Setting the palette sends another PaletteChange, which finds the colour
    // already set and stops.
    const QPalette current = palette();
    const QColor color = textToneColor(m_tone, current.color(QPalette::WindowText), current.color(backgroundRole()));
    if (current.color(kToneRole) != color) {
        QPalette toned = current;
        toned.setColor(kToneRole, color);
        setPalette(toned);
    }
}

} // namespace sysmon::ui
