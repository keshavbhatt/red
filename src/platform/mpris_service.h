#pragma once

#include "core/media_state.h"

#include <QObject>

#include <memory>

namespace red::platform {

/// Exposes the page's playback to the desktop as an MPRIS player (FEATURES
/// D7, ADR-007): media keys, KDE/GNOME media applets, `playerctl`. The Linux
/// backend registers org.mpris.MediaPlayer2.red over QDBus; other platforms
/// get a null implementation.
class MprisService : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MprisService)

public:
    using QObject::QObject;
    ~MprisService() override = default;

    /// The platform backend, never null (a no-op where MPRIS does not exist).
    [[nodiscard]] static std::unique_ptr<MprisService>
    create(const QString& identity, const QString& desktopEntry, QObject* parent = nullptr);

    virtual void setMediaState(const core::MediaState& state) = 0;
    [[nodiscard]] virtual bool isRegistered() const = 0;

Q_SIGNALS:
    /// "play", "pause", "playPause", "stop", "next", "previous", "seek" (seconds), "volume" (0..1).
    void commandRequested(const QString& command, double argument);
    void raiseRequested();
    void quitRequested();
};

} // namespace red::platform
