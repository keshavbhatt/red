#pragma once

#include <QWidget>

namespace red::web {

/// The arrow that slides in from the edge of the page during a two-finger swipe (ADR-022):
/// from the left for Back, from the right for Forward. It fills in once letting go would
/// navigate. Lives over the web view, never takes input.
class SwipeIndicator : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SwipeIndicator)

public:
    explicit SwipeIndicator(QWidget* parent);

    /// -1 … 1 as core::SwipeTracker::progress; 0 hides it.
    void setProgress(double progress);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double m_progress = 0.0;
};

} // namespace red::web
