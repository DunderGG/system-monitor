#pragma once

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QStyleHints>
#include <QWidget>

// Measures text contrast as WCAG defines it, for tests that check that text is
// readable in both colour schemes (DC-105).

namespace sysmon::tests
{

/** WCAG's minimum contrast ratio for normal text (level AA). */
inline constexpr double kMinimumTextContrast = 4.5;

/** WCAG's relative luminance of an opaque colour. */
inline double relativeLuminance(const QColor& color)
{
    const auto linear = [](double channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

/** WCAG's contrast ratio between two opaque colours, from 1 to 21. */
inline double contrastRatio(const QColor& first, const QColor& second)
{
    const double firstLuminance = relativeLuminance(first);
    const double secondLuminance = relativeLuminance(second);
    return (std::max(firstLuminance, secondLuminance) + 0.05) / (std::min(firstLuminance, secondLuminance) + 0.05);
}

/** Returns foreground, which may be translucent, as drawn over the opaque background. */
inline QColor blendedOver(const QColor& foreground, const QColor& background)
{
    const double alpha = foreground.alphaF();
    const auto blend = [alpha](double over, double under) { return alpha * over + (1.0 - alpha) * under; };
    return QColor::fromRgbF(static_cast<float>(blend(foreground.redF(), background.redF())),
                            static_cast<float>(blend(foreground.greenF(), background.greenF())),
                            static_cast<float>(blend(foreground.blueF(), background.blueF())));
}

/**
 * Renders window and returns the contrast of label's text as drawn: the
 * strongest contrast between a pixel inside label and the pixel at its
 * top-left corner, which shows the background the label is drawn on. The image
 * has an alpha channel, so Qt draws text with greyscale rather than subpixel
 * antialiasing, and no pixel is more extreme than the text's own colour.
 */
inline double renderedTextContrast(QWidget& window, const QWidget& label)
{
    QImage image(window.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    window.render(&image);
    const QRect area = QRect(label.mapTo(&window, QPoint(0, 0)), label.size()).intersected(image.rect());
    const QColor background = image.pixelColor(area.topLeft());
    double strongest = 1.0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            strongest = std::max(strongest, contrastRatio(image.pixelColor(x, y), background));
        }
    }
    return strongest;
}

/** Sets the application's colour scheme while it exists, and returns to the system's afterwards. */
class ScopedColorScheme
{
public:
    explicit ScopedColorScheme(Qt::ColorScheme scheme)
    {
        QGuiApplication::styleHints()->setColorScheme(scheme);
        QCoreApplication::processEvents();
    }

    ~ScopedColorScheme()
    {
        QGuiApplication::styleHints()->unsetColorScheme();
        QCoreApplication::processEvents();
    }

    ScopedColorScheme(const ScopedColorScheme&) = delete;
    ScopedColorScheme& operator=(const ScopedColorScheme&) = delete;
    ScopedColorScheme(ScopedColorScheme&&) = delete;
    ScopedColorScheme& operator=(ScopedColorScheme&&) = delete;
};

} // namespace sysmon::tests
