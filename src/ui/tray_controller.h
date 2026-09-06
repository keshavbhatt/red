#pragma once

#include "core/media_state.h"

#include <QIcon>
#include <QObject>

class QAction;
class QMenu;
class QSystemTrayIcon;

namespace red::core {
class Settings;
}

namespace red::ui {

class Actions;

/// System tray icon with playback controls and the app menu (FEATURES S4).
/// Never the only way back to the window: callers must check isAvailable()
/// before hiding.
class TrayController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TrayController)

public:
    TrayController(core::Settings& settings, Actions& actions, QObject* parent = nullptr);
    ~TrayController() override;

    [[nodiscard]] bool isAvailable() const;
    void setWindowVisible(bool visible);
    void setMediaState(const core::MediaState& state);

Q_SIGNALS:
    void toggleRequested();

private:
    void buildMenu();
    void applyTrayVisibility();
    void updateTooltip();

    core::Settings& m_settings;
    Actions& m_actions;
    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_menu = nullptr;
    QAction* m_nowPlaying = nullptr;
    QIcon m_icon;
    core::MediaState m_media;
    bool m_windowVisible = true;
};

} // namespace red::ui
