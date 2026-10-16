#include "ui/overlay_button.h"

#include "ui/icons.h"

#include <QApplication>
#include <QEnterEvent>
#include <QEvent>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
constexpr int kVisibleMs = 3000;
} // namespace

OverlayButton::OverlayButton(QWidget* host, const QString& icon, const QString& text, Corner corner,
                             int margin)
    : QToolButton(host)
    , m_host(host)
    , m_corner(corner)
    , m_margin(margin)
{
    setObjectName(u"redOverlayButton"_s);
    setText(text);
    setToolButtonStyle(text.isEmpty() ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    setIcon(icons::themed(icon, Qt::white));
    setIconSize(QSize(18, 18));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus); // arrows, Enter and Space belong to the page
    // The video behind is dark whatever the theme: the pill matches the full screen hint.
    setStyleSheet(
        text.isEmpty()
            ? u"QToolButton#redOverlayButton {"
              u"  background-color: rgba(0, 0, 0, 190); border: 1px solid rgba(255, 255, 255, 60);"
              u"  border-radius: 17px; min-width: 32px; max-width: 32px; min-height: 32px; max-height: 32px;"
              u"}"
              u"QToolButton#redOverlayButton:hover { background-color: rgba(40, 40, 40, 230); }"_s
            : u"QToolButton#redOverlayButton {"
              u"  background-color: rgba(0, 0, 0, 190); color: white; font-weight: 600;"
              u"  padding: 8px 16px 8px 12px; border: 1px solid rgba(255, 255, 255, 60); border-radius: 18px;"
              u"}"
              u"QToolButton#redOverlayButton:hover { background-color: rgba(40, 40, 40, 230); }"_s);
    hide();

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(kVisibleMs);
    connect(m_timer, &QTimer::timeout, this, &QWidget::hide);
}

void OverlayButton::setActive(bool active)
{
    if (active == m_active) {
        return;
    }
    m_active = active;
    if (active) {
        // The page widget (Chromium's render view) gets the mouse, not the host:
        // watch the application and keep the events that land in this window.
        qApp->installEventFilter(this);
        reveal(); // once on entry, next to the mode's hint
    } else {
        qApp->removeEventFilter(this);
        m_timer->stop();
        hide();
    }
}

void OverlayButton::reveal()
{
    adjustSize();
    const int x = m_corner == Corner::TopLeft ? m_margin : m_host->width() - width() - m_margin;
    move(x, m_margin);
    if (!isVisible()) {
        show();
        raise();
    }
    if (!underMouse()) {
        m_timer->start();
    }
}

bool OverlayButton::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseMove && watched->isWidgetType() &&
        static_cast<QWidget*>(watched)->window() == m_host->window()) {
        reveal();
    } else if (event->type() == QEvent::Resize && watched == m_host && isVisible()) {
        reveal(); // keep to its corner while the window is resized
    }
    return QToolButton::eventFilter(watched, event);
}

void OverlayButton::enterEvent(QEnterEvent* event)
{
    QToolButton::enterEvent(event);
    m_timer->stop(); // stays while the pointer is on it
}

void OverlayButton::leaveEvent(QEvent* event)
{
    QToolButton::leaveEvent(event);
    if (m_active) {
        m_timer->start();
    }
}

} // namespace red::ui
