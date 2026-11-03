#include "web/swipe_indicator.h"

#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace red::web {

namespace {
constexpr int kSize = 44;   // the circle
constexpr int kMargin = 20; // from the edge once fully in
} // namespace

SwipeIndicator::SwipeIndicator(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    resize(kSize, kSize);
    hide();
}

void SwipeIndicator::setProgress(double progress)
{
    m_progress = progress;
    if (std::abs(progress) < 0.02 || parentWidget() == nullptr) {
        hide();
        return;
    }
    const int w = parentWidget()->width();
    const int travel = kSize + kMargin; // from just outside the edge to kMargin inside it
    const int offset = static_cast<int>(std::abs(progress) * travel);
    const int x = progress > 0 ? -kSize + offset : w - offset;
    move(x, (parentWidget()->height() - kSize) / 2);
    raise();
    show();
    update();
}

void SwipeIndicator::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool ready = std::abs(m_progress) >= 1.0;
    // Ready: a solid light circle with a dark arrow; on the way: dark and see-through.
    const QColor circle = ready ? QColor(241, 241, 241) : QColor(15, 15, 15, 200);
    const QColor arrow = ready ? QColor(15, 15, 15) : QColor(241, 241, 241);
    p.setPen(Qt::NoPen);
    p.setBrush(circle);
    p.drawEllipse(rect().adjusted(1, 1, -1, -1));

    const double c = kSize / 2.0;
    const double s = 7.0;
    const double dir = m_progress > 0 ? -1.0 : 1.0; // Back points left
    QPainterPath path;
    path.moveTo(c - dir * s * 1.2, c);
    path.lineTo(c + dir * s * 1.2, c);
    path.moveTo(c + dir * s * 0.1, c - s);
    path.lineTo(c + dir * s * 1.2, c);
    path.lineTo(c + dir * s * 0.1, c + s);
    p.setPen(QPen(arrow, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
}

} // namespace red::web
