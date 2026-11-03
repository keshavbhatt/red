#include "web/swipe_indicator.h"

#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>

namespace red::web {

namespace {
constexpr int kBubble = 40;  // the arrow's circle
constexpr int kRingMax = 92; // the ring behind it at the end of the swipe
constexpr int kMargin = 56;  // from the edge to the bubble once fully in: the whole ring shows
constexpr int kBox = kRingMax + 4;
const QColor kAccent(0xFF, 0x00, 0x33); // Red's brand red (DESIGN.md `brand`)
} // namespace

SwipeIndicator::SwipeIndicator(QWidget* parent)
    : QWidget(parent)
    , m_fade(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    resize(kBox, kBox);
    hide();
    m_fade->setStartValue(1.0);
    m_fade->setEndValue(0.0);
    m_fade->setDuration(220);
    connect(m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_opacity = v.toDouble();
        update();
    });
    connect(m_fade, &QVariantAnimation::finished, this, [this] {
        m_progress = 0.0;
        m_opacity = 1.0;
        hide();
    });
}

void SwipeIndicator::setDark(bool dark)
{
    m_dark = dark;
}

void SwipeIndicator::place()
{
    if (parentWidget() == nullptr) {
        return;
    }
    // The bubble's centre travels from just outside the edge to kMargin inside it.
    const double travel = kBubble + kMargin;
    const double eased = 1.0 - std::pow(1.0 - std::min(1.0, std::abs(m_progress)), 2.0);
    const double inside = -kBubble / 2.0 + eased * travel; // centre's distance from the edge
    const int w = parentWidget()->width();
    const double cx = m_progress > 0 ? inside : w - inside;
    move(static_cast<int>(cx - kBox / 2.0), (parentWidget()->height() - kBox) / 2);
}

void SwipeIndicator::setProgress(double progress)
{
    if (m_fade->state() == QAbstractAnimation::Running) {
        return; // the confirmation owns it until it has faded
    }
    m_progress = progress;
    m_opacity = 1.0;
    if (std::abs(progress) < 0.02) {
        hide();
        return;
    }
    place();
    raise();
    show();
    update();
}

void SwipeIndicator::confirm()
{
    if (!isVisible()) {
        return;
    }
    m_progress = m_progress >= 0 ? 1.0 : -1.0;
    place();
    update();
    m_fade->stop();
    m_fade->setStartValue(1.0);
    m_fade->start();
}

void SwipeIndicator::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacity);
    const double a = std::min(1.0, std::abs(m_progress));
    const bool ready = a >= 1.0;
    const QPointF c(width() / 2.0, height() / 2.0);

    // The ring: grows with the swipe, a tint of the accent once ready.
    const double ring = kBubble + (kRingMax - kBubble) * a;
    QColor ringColor = ready ? kAccent : (m_dark ? QColor(255, 255, 255) : QColor(0, 0, 0));
    ringColor.setAlphaF(ready ? 0.28 : 0.10 + 0.08 * a);
    p.setPen(Qt::NoPen);
    p.setBrush(ringColor);
    p.drawEllipse(c, ring / 2.0, ring / 2.0);

    // The bubble.
    QColor bubble = m_dark ? QColor(0x28, 0x28, 0x28) : QColor(0xFF, 0xFF, 0xFF);
    QColor arrow = m_dark ? QColor(0xF1, 0xF1, 0xF1) : QColor(0x0F, 0x0F, 0x0F);
    if (ready) {
        bubble = kAccent;
        arrow = Qt::white;
    }
    if (!m_dark && !ready) {
        p.setPen(QPen(QColor(0, 0, 0, 40), 1.0));
    }
    p.setBrush(bubble);
    p.drawEllipse(c, kBubble / 2.0, kBubble / 2.0);

    const double s = 6.5;
    const double dir = m_progress > 0 ? -1.0 : 1.0; // Back points left
    QPainterPath path;
    path.moveTo(c.x() - dir * s * 1.2, c.y());
    path.lineTo(c.x() + dir * s * 1.2, c.y());
    path.moveTo(c.x() + dir * s * 0.1, c.y() - s);
    path.lineTo(c.x() + dir * s * 1.2, c.y());
    path.lineTo(c.x() + dir * s * 0.1, c.y() + s);
    p.setPen(QPen(arrow, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
}

} // namespace red::web
