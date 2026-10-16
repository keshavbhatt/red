#include "ui/tv_exit_button.h"

#include "ui/icons.h"

#include <QApplication>
#include <QEnterEvent>
#include <QEvent>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
constexpr int kMargin = 24;
constexpr int kVisibleMs = 3000;
} // namespace

TvExitButton::TvExitButton(QWidget* host)
    : QToolButton(host)
    , m_host(host)
{
    setObjectName(u"tvExitButton"_s);
    setText(tr("Back to YouTube"));
    setToolTip(tr("Leave TV mode  (Ctrl+T)"));
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setIcon(icons::themed(u"back"_s, Qt::white));
    setIconSize(QSize(18, 18));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus); // arrows and Enter belong to Leanback
    // Leanback is dark whatever the theme: the pill matches the full screen hint.
    setStyleSheet(
        u"QToolButton#tvExitButton {"
        u"  background-color: rgba(0, 0, 0, 190); color: white; font-weight: 600;"
        u"  padding: 8px 16px 8px 12px; border: 1px solid rgba(255, 255, 255, 60); border-radius: 18px;"
        u"}"
        u"QToolButton#tvExitButton:hover { background-color: rgba(40, 40, 40, 230); }"_s);
    hide();

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(kVisibleMs);
    connect(m_timer, &QTimer::timeout, this, &QWidget::hide);
}

void TvExitButton::setActive(bool active)
{
    if (active == m_active) {
        return;
    }
    m_active = active;
    if (active) {
        // The page widget (Chromium's render view) gets the mouse, not the host:
        // watch the application and keep the events that land in this window.
        qApp->installEventFilter(this);
        reveal(); // once on entry, next to the TV mode hint
    } else {
        qApp->removeEventFilter(this);
        m_timer->stop();
        hide();
    }
}

void TvExitButton::reveal()
{
    adjustSize();
    move(kMargin, kMargin);
    if (!isVisible()) {
        show();
        raise();
    }
    if (!underMouse()) {
        m_timer->start();
    }
}

bool TvExitButton::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseMove && watched->isWidgetType() &&
        static_cast<QWidget*>(watched)->window() == m_host->window()) {
        reveal();
    }
    return QToolButton::eventFilter(watched, event);
}

void TvExitButton::enterEvent(QEnterEvent* event)
{
    QToolButton::enterEvent(event);
    m_timer->stop(); // stays while the pointer is on it
}

void TvExitButton::leaveEvent(QEvent* event)
{
    QToolButton::leaveEvent(event);
    if (m_active) {
        m_timer->start();
    }
}

} // namespace red::ui
