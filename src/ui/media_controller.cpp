#include "ui/media_controller.h"

#include "core/settings/settings.h"
#include "platform/mpris_service.h"
#include "platform/screen_inhibitor.h"
#include "ui/logging.h"
#include "web/web_view.h"

using namespace Qt::StringLiterals;

namespace red::ui {

MediaController::MediaController(core::Settings& settings, web::WebView& view, const QString& desktopEntry,
                                 QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_view(view)
    , m_mpris(platform::MprisService::create(u"Red"_s, desktopEntry))
    , m_inhibitor(platform::ScreenInhibitor::create())
{
    connect(&m_view, &web::WebView::mediaStateChanged, this, &MediaController::handleState);
    connect(m_mpris.get(), &platform::MprisService::commandRequested, this,
            [this](const QString& command, double argument) { m_view.mediaCommand(command, argument); });
    connect(m_mpris.get(), &platform::MprisService::raiseRequested, this, &MediaController::raiseRequested);
    connect(&m_settings, &core::Settings::keepScreenAwakeChanged, this, [this](bool) { updateInhibit(); });
    connect(&m_view, &web::WebView::modeChanged, this, [this](core::AppMode) { updateInhibit(); });
}

MediaController::~MediaController() = default;

void MediaController::handleState(const core::MediaState& state)
{
    const bool wasPlaying = m_state.isPlaying();
    m_state = state;
    m_mpris->setMediaState(state);
    if (wasPlaying != state.isPlaying()) {
        qCDebug(lcUi) << "media" << (state.isPlaying() ? "playing" : "paused") << state.title;
    }
    updateInhibit();
    Q_EMIT stateChanged(state);
}

void MediaController::updateInhibit()
{
    // Music plays in the background by nature: the screen may sleep.
    const bool wanted =
        m_settings.keepScreenAwake() && m_state.isPlaying() && m_view.mode() != core::AppMode::Music;
    m_inhibitor->setInhibited(wanted, tr("Playing a video"));
}

void MediaController::playPause()
{
    m_view.mediaCommand(u"playPause"_s);
}

void MediaController::next()
{
    m_view.mediaCommand(u"next"_s);
}

void MediaController::previous()
{
    m_view.mediaCommand(u"previous"_s);
}

} // namespace red::ui
