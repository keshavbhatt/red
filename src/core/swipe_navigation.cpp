#include "core/swipe_navigation.h"

#include <algorithm>
#include <cstdlib>

namespace red::core {

void SwipeTracker::begin(bool inverted)
{
    m_active = true;
    m_inverted = inverted;
    m_sumX = 0;
    m_sumY = 0;
    m_fingerX = 0;
}

void SwipeTracker::update(QPoint pixelDelta)
{
    if (!m_active) {
        return;
    }
    m_sumX += std::abs(pixelDelta.x());
    m_sumY += std::abs(pixelDelta.y());
}

void SwipeTracker::unused(QPoint pixelDelta)
{
    if (!m_active) {
        return;
    }
    // A wheel delta is positive towards the left. With classic scrolling the content moves
    // against the fingers, so fingers going right give a negative delta; natural scrolling
    // (inverted) flips that. Back is always fingers to the right.
    m_fingerX += m_inverted ? pixelDelta.x() : -pixelDelta.x();
}

void SwipeTracker::end()
{
    // Nothing to do yet: the decision waits for finish(), so late unused scroll still counts.
}

bool SwipeTracker::mostlySideways() const
{
    return m_sumX > 2 * m_sumY;
}

double SwipeTracker::progress(int viewWidth) const
{
    if (!m_active || !mostlySideways() || viewWidth <= 0) {
        return 0.0;
    }
    const double needed = std::min(static_cast<double>(kMaxCommitPx), viewWidth * kCommitShare);
    return std::clamp(m_fingerX / needed, -1.0, 1.0);
}

SwipeAction SwipeTracker::finish(int viewWidth)
{
    const double p = progress(viewWidth);
    cancel();
    if (p >= 1.0) {
        return SwipeAction::Back;
    }
    if (p <= -1.0) {
        return SwipeAction::Forward;
    }
    return SwipeAction::None;
}

void SwipeTracker::cancel()
{
    m_active = false;
    m_sumX = 0;
    m_sumY = 0;
    m_fingerX = 0;
}

} // namespace red::core
