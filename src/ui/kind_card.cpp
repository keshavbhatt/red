#include "ui/kind_card.h"

#include "core/theme/theme_service.h"
#include "ui/icons.h"
#include "ui/red_style.h"

#include <QEvent>
#include <QFocusEvent>
#include <QPainter>
#include <QPainterPath>

namespace red::ui {

namespace {
constexpr int kPad = 12;
constexpr int kIcon = 22;
constexpr int kRadius = 10;
} // namespace

KindCard::KindCard(const QString& icon, const QString& title, const QString& hint, core::ThemeService& theme,
                   QWidget* parent)
    : QAbstractButton(parent)
    , m_icon(icon)
    , m_hint(hint)
    , m_theme(theme)
{
    setText(title);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this,
            [this](Qt::ColorScheme) { update(); });
}

KindCard::~KindCard() = default;

QSize KindCard::sizeHint() const
{
    const QFontMetrics fm(font());
    return {kPad * 2 + kIcon + 10 + fm.horizontalAdvance(m_hint), kPad * 2 + fm.height() * 2 + 2};
}

QSize KindCard::minimumSizeHint() const
{
    const QFontMetrics fm(font());
    return {kPad * 2 + kIcon + 10 + 90, kPad * 2 + fm.height() * 2 + 2};
}

void KindCard::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, kRadius, kRadius);
    const bool on = isChecked();
    p.fillPath(path, on ? t.hover : (m_hover ? t.elevated : t.panel));
    p.setPen(QPen(on ? t.control : t.border, on ? 2 : 1));
    p.drawPath(path);
    // Keyboard focus is a dashed neutral outline inside the border, so it never
    // reads as the solid blue of the picked card. Mouse users never see it.
    if (m_keyboardFocus && hasFocus()) {
        QPainterPath ring;
        ring.addRoundedRect(r.adjusted(3, 3, -3, -3), kRadius - 3, kRadius - 3);
        p.setPen(QPen(t.muted, 1, Qt::DashLine));
        p.drawPath(ring);
    }

    const QColor fg = isEnabled() ? t.text : t.muted;
    const QPixmap icon = icons::pixmap(m_icon, on ? t.control : fg, kIcon, devicePixelRatioF());
    const int iconY = (height() - kIcon) / 2;
    p.drawPixmap(kPad, iconY, icon);

    QFont titleFont = font();
    titleFont.setWeight(QFont::DemiBold);
    const QFontMetrics tm(titleFont);
    const QFontMetrics hm(font());
    const int textX = kPad + kIcon + 10;
    const int textW = width() - textX - kPad;
    const int block = tm.height() + hm.height() + 2;
    int y = (height() - block) / 2;
    p.setFont(titleFont);
    p.setPen(fg);
    p.drawText(QRect(textX, y, textW, tm.height()), Qt::AlignLeft | Qt::AlignVCenter, text());
    y += tm.height() + 2;
    p.setFont(font());
    p.setPen(t.muted);
    p.drawText(QRect(textX, y, textW, hm.height()), Qt::AlignLeft | Qt::AlignVCenter,
               hm.elidedText(m_hint, Qt::ElideRight, textW));
}

void KindCard::enterEvent(QEnterEvent* event)
{
    m_hover = true;
    update();
    QAbstractButton::enterEvent(event);
}

void KindCard::leaveEvent(QEvent* event)
{
    m_hover = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void KindCard::focusInEvent(QFocusEvent* event)
{
    const Qt::FocusReason reason = event->reason();
    m_keyboardFocus = reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason;
    update();
    QAbstractButton::focusInEvent(event);
}

void KindCard::focusOutEvent(QFocusEvent* event)
{
    m_keyboardFocus = false;
    update();
    QAbstractButton::focusOutEvent(event);
}

void KindCard::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::EnabledChange) {
        update();
    }
    QAbstractButton::changeEvent(event);
}

} // namespace red::ui
