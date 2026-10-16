#pragma once

#include <QToolButton>

class QTimer;

namespace red::ui {

/// "Back to YouTube" in TV mode, where the rail is hidden: a pill in the top
/// left corner that appears while the mouse moves over the window and fades
/// out again, so the way out is visible without taking room from Leanback.
class TvExitButton : public QToolButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TvExitButton)

public:
    explicit TvExitButton(QWidget* host);
    ~TvExitButton() override = default;

    /// On while TV mode is on: shows the button on entry, then again whenever
    /// the mouse moves over the host.
    void setActive(bool active);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void reveal();

    QWidget* m_host = nullptr;
    QTimer* m_timer = nullptr;
    bool m_active = false;
};

} // namespace red::ui
