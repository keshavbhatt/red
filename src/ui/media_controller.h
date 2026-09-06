#pragma once

#include "core/media_state.h"

#include <QObject>

#include <memory>

namespace red::core {
class Settings;
}
namespace red::platform {
class MprisService;
class ScreenInhibitor;
} // namespace red::platform
namespace red::web {
class WebView;
}

namespace red::ui {

/// Routes the page's media state to MPRIS, the screen inhibitor and the tray
/// (ADR-007), and MPRIS/tray commands back to the page.
class MediaController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MediaController)

public:
    MediaController(core::Settings& settings, web::WebView& view, const QString& desktopEntry,
                    QObject* parent = nullptr);
    ~MediaController() override;

    [[nodiscard]] const core::MediaState& state() const { return m_state; }
    void playPause();
    void next();
    void previous();

Q_SIGNALS:
    void stateChanged(const core::MediaState& state);
    void raiseRequested();

private:
    void handleState(const core::MediaState& state);
    void updateInhibit();

    core::Settings& m_settings;
    web::WebView& m_view;
    std::unique_ptr<platform::MprisService> m_mpris;
    std::unique_ptr<platform::ScreenInhibitor> m_inhibitor;
    core::MediaState m_state;
};

} // namespace red::ui
