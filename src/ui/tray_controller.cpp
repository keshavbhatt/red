#include "ui/tray_controller.h"

#include "core/settings/settings.h"
#include "ui/actions.h"
#include "ui/icons.h"
#include "ui/logging.h"

#include <QAction>
#include <QApplication>
#include <QFontMetrics>
#include <QMenu>
#include <QSystemTrayIcon>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
constexpr int kNowPlayingWidth = 260; ///< px; about 40 characters of menu text
}

TrayController::TrayController(core::Settings& settings, Actions& actions, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_actions(actions)
    , m_icon(icons::brand())
{
    buildMenu();
    applyTrayVisibility();
    connect(&m_settings, &core::Settings::trayEnabledChanged, this, [this](bool) { applyTrayVisibility(); });
}

TrayController::~TrayController() = default;

bool TrayController::isAvailable() const
{
    return m_tray != nullptr && m_tray->isVisible() && QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayController::buildMenu()
{
    m_menu = new QMenu();
    m_nowPlaying = m_menu->addAction(tr("Nothing playing"));
    m_nowPlaying->setEnabled(false);
    m_menu->addAction(m_actions.playPause);
    m_menu->addAction(m_actions.previous);
    m_menu->addAction(m_actions.next);
    m_menu->addSeparator();
    m_menu->addAction(m_actions.showHide);
    m_menu->addAction(m_actions.downloads);
    m_menu->addAction(m_actions.miniPlayer);
    m_menu->addSeparator();
    m_menu->addAction(m_actions.youtubeMode); // an exclusive group: radio items
    m_menu->addAction(m_actions.musicMode);
    m_menu->addAction(m_actions.tvMode);
    m_menu->addSeparator();
    m_menu->addAction(m_actions.settings);
    m_menu->addAction(m_actions.about);
    m_menu->addSeparator();
    m_menu->addAction(m_actions.quit);
    // Text only: the rail gives icons to a handful of these actions and a
    // half-iconed menu looks broken. Media keys have no readable chord.
    for (QAction* action : m_menu->actions()) {
        action->setIconVisibleInMenu(false);
    }
    for (QAction* action : {m_actions.playPause, m_actions.previous, m_actions.next}) {
        action->setShortcutVisibleInContextMenu(false);
    }
}

void TrayController::applyTrayVisibility()
{
    const bool wanted = m_settings.trayEnabled() && QSystemTrayIcon::isSystemTrayAvailable();
    if (wanted && m_tray == nullptr) {
        m_tray = new QSystemTrayIcon(m_icon, this);
        m_tray->setContextMenu(m_menu);
        connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                Q_EMIT toggleRequested();
            } else if (reason == QSystemTrayIcon::MiddleClick) {
                m_actions.playPause->trigger();
            }
        });
        updateTooltip();
        m_tray->show();
        qCInfo(lcUi) << "tray icon shown";
    } else if (!wanted && m_tray != nullptr) {
        m_tray->hide();
        m_tray->deleteLater();
        m_tray = nullptr;
        qCInfo(lcUi) << "tray icon hidden";
    }
}

void TrayController::setWindowVisible(bool visible)
{
    m_windowVisible = visible;
    m_actions.showHide->setText(visible ? tr("Hide window") : tr("Show Red"));
}

void TrayController::setMediaState(const core::MediaState& state)
{
    m_media = state;
    const bool has = state.hasMedia();
    // A long title would stretch the whole menu: elide it to a sensible width.
    const QString title = state.title.isEmpty() ? tr("Now playing") : state.title;
    const QFontMetrics metrics(m_menu->font());
    m_nowPlaying->setText(has ? metrics.elidedText(title, Qt::ElideRight, kNowPlayingWidth)
                              : tr("Nothing playing"));
    m_nowPlaying->setToolTip(has ? title : QString());
    m_actions.playPause->setText(state.isPlaying() ? tr("Pause") : tr("Play"));
    m_actions.playPause->setEnabled(has);
    m_actions.next->setEnabled(has && state.canGoNext);
    m_actions.previous->setEnabled(has && state.canGoPrevious);
    updateTooltip();
}

void TrayController::updateTooltip()
{
    if (m_tray == nullptr) {
        return;
    }
    QString tip = u"Red"_s;
    if (m_media.hasMedia() && !m_media.title.isEmpty()) {
        tip += (m_media.isPlaying() ? u" — ▶ "_s : u" — ⏸ "_s) + m_media.title;
        if (!m_media.artist.isEmpty()) {
            tip += u" · "_s + m_media.artist;
        }
    }
    m_tray->setToolTip(tip);
}

} // namespace red::ui
