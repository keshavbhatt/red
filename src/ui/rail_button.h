#pragma once

#include <QColor>
#include <QToolButton>

class QPainter;

namespace red::ui {

/// A side rail button. Collapsed it is the plain square icon button; expanded
/// it keeps the icon where the collapsed rail has it and draws a label beside
/// it, plus an optional muted second line (why a button is unavailable).
class RailButton : public QToolButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RailButton)

public:
    /// `side`: the collapsed button's width and height in px.
    RailButton(int side, QWidget* parent);
    ~RailButton() override = default;

    /// Label beside the icon, `width` px wide; collapsed: the square button.
    void setExpanded(bool expanded, int width);
    /// Overrides the default label (the action's text without "…").
    void setLabel(const QString& label);
    /// A second, muted line under the label while expanded; empty hides it.
    void setHint(const QString& hint);
    void setColors(const QColor& text, const QColor& muted);
    /// Keyboard focus is arriving (F6, the arrows): draw the ring even if the
    /// window is not active yet and no focus-in event comes.
    void setKeyboardFocus();

protected:
    void paintEvent(QPaintEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    [[nodiscard]] QString label() const;
    void paintFocusRing(QPainter& painter) const;

    int m_side = 40;
    bool m_expanded = false;
    QString m_label;
    QString m_hint;
    QColor m_text;
    QColor m_muted;
    bool m_keyboardFocus = false; ///< reached with Tab / F6 / the arrows: draw the ring
};

} // namespace red::ui
