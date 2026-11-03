#pragma once

#include <QWidget>

class QVariantAnimation;

namespace red::web {

/// The arrow that slides in from the edge of the page during a two-finger swipe (ADR-022), as
/// in Chrome: from the left for Back, from the right for Forward, with a soft ring behind it that
/// grows with the swipe. Once letting go would navigate, it turns Red's red; when the swipe
/// fires it stays red for a moment and fades. Lives over the web view, never takes input.
class SwipeIndicator : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SwipeIndicator)

public:
    explicit SwipeIndicator(QWidget* parent);

    /// -1 … 1 as core::SwipeTracker::progress; 0 hides it.
    void setProgress(double progress);
    /// The swipe navigated: show the red arrow briefly, then fade out.
    void confirm();
    /// Follow the app's light or dark scheme.
    void setDark(bool dark);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void place();

    double m_progress = 0.0;
    double m_opacity = 1.0;
    bool m_dark = true;
    QVariantAnimation* m_fade = nullptr;
};

} // namespace red::web
