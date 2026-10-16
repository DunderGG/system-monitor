#pragma once

#include <QColor>
#include <QLabel>

class QEvent;

namespace sysmon::ui
{

/**
 * What the colour of a piece of text says. Every tone's colour reaches WCAG's
 * 4.5:1 contrast for normal text on the window and base colours of Qt's light
 * and dark Windows palettes, which the views are drawn on (DC-105).
 */
enum class TextTone
{
    /** The palette's text colour. */
    Normal,
    /** Secondary text, such as a readout's name, a device description, or a status that is not active. */
    Dimmed,
    /** A good state, such as Healthy or an adapter that is Up. */
    Good,
    Warning,
    Critical,
};

/** Opacity of dimmed text. The charts draw their muted text at the same opacity. */
inline constexpr qreal kDimmedTextAlpha = 0.6;

/** Returns textColor at kDimmedTextAlpha. */
[[nodiscard]] QColor dimmedTextColor(const QColor& textColor);

/**
 * Returns the colour of text of tone, given the palette's text colour and the
 * background the text is drawn on. Good, Warning and Critical are a green, an
 * amber and a red, each with a darker shade for a light background and a
 * lighter one for a dark background.
 */
[[nodiscard]] QColor textToneColor(TextTone tone, const QColor& textColor, const QColor& background);

/**
 * A label drawn in the colour of its tone (see textToneColor()), and in bold
 * for Good, Warning and Critical. It works the colour out from the text and
 * window colours it inherits, and again whenever its palette changes, so it
 * follows a change of colour scheme.
 *
 * Not thread-safe; use only on the UI thread.
 */
class TonedLabel : public QLabel
{
    Q_OBJECT

public:
    explicit TonedLabel(TextTone tone = TextTone::Normal, QWidget* parent = nullptr);
    ~TonedLabel() override = default;

    void setTone(TextTone tone);
    [[nodiscard]] TextTone tone() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    void applyTone();

    TextTone m_tone;
};

} // namespace sysmon::ui
