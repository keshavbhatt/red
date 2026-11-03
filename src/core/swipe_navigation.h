#pragma once

#include <QPoint>

// Two-finger touchpad swipe to go back and forward (issue #18, ADR-022). Pure bookkeeping:
// the view feeds it the touchpad gesture as it arrives (with its begin / update / end phases)
// and the scroll the page handed back unused; it answers how far the swipe has gone and,
// once the fingers lift, whether to navigate.
//
// Only unused scroll counts, so a strip that can still scroll sideways keeps the swipe, and
// the page engine latches a gesture to the strip it started on: scrolling one to its end
// never turns into Back halfway through.
namespace red::core {

enum class SwipeAction
{
    None,
    Back,
    Forward
};

class SwipeTracker
{
public:
    /// The swipe has to travel this share of the view's width, at most kMaxCommitPx.
    static constexpr double kCommitShare = 0.25;
    static constexpr int kMaxCommitPx = 220;

    /// Fingers down. `inverted`: the platform reports natural scrolling (deltas follow the
    /// content rather than the wheel), as QWheelEvent::inverted().
    void begin(bool inverted);
    /// A movement of the gesture as it reached the page (before the page saw it).
    void update(QPoint pixelDelta);
    /// Scroll from this gesture that the page did not use.
    void unused(QPoint pixelDelta);
    /// Fingers up. Unused scroll still on its way may arrive before finish().
    void end();
    /// Decides and resets: Back or Forward when the swipe went far enough sideways.
    [[nodiscard]] SwipeAction finish(int viewWidth);
    void cancel();

    [[nodiscard]] bool active() const { return m_active; }
    /// -1 … 1: how far towards Forward (negative) or Back (positive) the swipe has gone,
    /// 1 or -1 once it would navigate.
    [[nodiscard]] double progress(int viewWidth) const;

private:
    [[nodiscard]] bool mostlySideways() const;

    bool m_active = false;
    bool m_inverted = false;
    int m_sumX = 0; // absolute movement of the whole gesture, to tell sideways from vertical
    int m_sumY = 0;
    int m_fingerX = 0; // unused sideways scroll in finger direction: positive = fingers right
};

} // namespace red::core
