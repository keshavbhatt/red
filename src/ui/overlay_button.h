#pragma once

#include <QToolButton>

class QTimer;

namespace red::ui {

/// A dark pill floating over the page in a corner of the window while the
/// rail is hidden: "Back to YouTube" in TV mode, "back to the full window" in
/// the mini player. Shows when it is activated and whenever the mouse moves
/// over the window, then hides again, so the way out is visible without
/// covering the video.
class OverlayButton : public QToolButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OverlayButton)

public:
    enum class Corner
    {
        TopLeft,
        TopRight,
    };

    /// `text` empty: an icon-only round button (the tooltip names it).
    OverlayButton(QWidget* host, const QString& icon, const QString& text, Corner corner, int margin);
    ~OverlayButton() override = default;

    /// On while its mode is on: shows the button now, then again whenever the
    /// mouse moves over the host.
    void setActive(bool active);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void reveal();

    QWidget* m_host = nullptr;
    QTimer* m_timer = nullptr;
    Corner m_corner = Corner::TopLeft;
    int m_margin = 0;
    bool m_active = false;
};

} // namespace red::ui
