#include "ui/rail_button.h"

#include <QFocusEvent>
#include <QPainterPath>
#include <QStyleOptionToolButton>
#include <QStylePainter>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
constexpr int kLabelGap = 12; ///< icon to label
constexpr int kEndPadding = 8;
} // namespace

RailButton::RailButton(int side, QWidget* parent)
    : QToolButton(parent)
    , m_side(side)
{
    setFixedSize(side, side);
}

void RailButton::setExpanded(bool expanded, int width)
{
    m_expanded = expanded;
    setFixedSize(expanded ? width : m_side, m_side);
    update();
}

void RailButton::setLabel(const QString& label)
{
    m_label = label;
    update();
}

void RailButton::setHint(const QString& hint)
{
    if (hint == m_hint) {
        return;
    }
    m_hint = hint;
    update();
}

void RailButton::setColors(const QColor& text, const QColor& muted)
{
    m_text = text;
    m_muted = muted;
    update();
}

QString RailButton::label() const
{
    if (!m_label.isEmpty()) {
        return m_label;
    }
    QString text = defaultAction() != nullptr ? defaultAction()->text() : QToolButton::text();
    text.remove(u'…');
    text.replace(u"&&"_s, u"&"_s);
    return text.trimmed();
}

/// Keyboard focus only: a dashed neutral ring, as on the download cards. The
/// mouse never moves focus here (Qt::TabFocus), so clicks leave no ring.
void RailButton::paintFocusRing(QPainter& painter) const
{
    if (!m_keyboardFocus || !hasFocus()) {
        return;
    }
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath ring;
    ring.addRoundedRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5), 9, 9);
    painter.setPen(QPen(m_muted, 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(ring);
}

void RailButton::focusInEvent(QFocusEvent* event)
{
    switch (event->reason()) {
    case Qt::TabFocusReason:
    case Qt::BacktabFocusReason:
    case Qt::ShortcutFocusReason:
    case Qt::OtherFocusReason:
        m_keyboardFocus = true;
        break;
    case Qt::MouseFocusReason:
        m_keyboardFocus = false;
        break;
    default:
        break; // the window coming back keeps whatever it was
    }
    QToolButton::focusInEvent(event);
}

void RailButton::setKeyboardFocus()
{
    m_keyboardFocus = true;
    update();
}

void RailButton::focusOutEvent(QFocusEvent* event)
{
    QToolButton::focusOutEvent(event);
    update();
}

void RailButton::paintEvent(QPaintEvent* event)
{
    if (!m_expanded) {
        QToolButton::paintEvent(event);
        QPainter painter(this);
        paintFocusRing(painter);
        return;
    }
    QStylePainter painter(this);
    QStyleOptionToolButton option;
    initStyleOption(&option);
    // The style draws the background (hover, checked, the red pill) from the
    // style sheet; icon and label are laid out here, left-aligned.
    option.text.clear();
    option.icon = QIcon();
    painter.drawComplexControl(QStyle::CC_ToolButton, option);
    paintFocusRing(painter);

    const QSize iconSz = iconSize();
    const QRect iconRect((m_side - iconSz.width()) / 2, (height() - iconSz.height()) / 2, iconSz.width(),
                         iconSz.height());
    icon().paint(&painter, iconRect, Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled,
                 isChecked() ? QIcon::On : QIcon::Off);

    const int textLeft = iconRect.right() + 1 + kLabelGap;
    QRect textRect(textLeft, 0, width() - textLeft - kEndPadding, height());
    QFont labelFont = font();
    if (isCheckable() && isChecked()) {
        labelFont.setWeight(QFont::DemiBold);
    }
    painter.setFont(labelFont);
    painter.setPen(isEnabled() ? m_text : m_muted);
    const QString text = painter.fontMetrics().elidedText(label(), Qt::ElideRight, textRect.width());
    if (m_hint.isEmpty()) {
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
        return;
    }
    QFont hintFont = font();
    hintFont.setPointSizeF(font().pointSizeF() - 1.5);
    const int labelHeight = painter.fontMetrics().height();
    const int hintHeight = QFontMetrics(hintFont).height();
    const int top = (height() - labelHeight - hintHeight) / 2;
    painter.drawText(QRect(textLeft, top, textRect.width(), labelHeight), Qt::AlignLeft | Qt::AlignVCenter,
                     text);
    painter.setFont(hintFont);
    painter.setPen(m_muted);
    painter.drawText(QRect(textLeft, top + labelHeight, textRect.width(), hintHeight),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QFontMetrics(hintFont).elidedText(m_hint, Qt::ElideRight, textRect.width()));
}

} // namespace red::ui
