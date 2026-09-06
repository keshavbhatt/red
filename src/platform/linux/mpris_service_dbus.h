#pragma once

#include "platform/mpris_service.h"

#include <QElapsedTimer>
#include <QVariantMap>

namespace red::platform::linux_ {

/// The Linux MprisService: registers org.mpris.MediaPlayer2.red and emits
/// PropertiesChanged when the page reports a new state.
class MprisDBusService : public MprisService
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MprisDBusService)

public:
    MprisDBusService(const QString& identity, const QString& desktopEntry, QObject* parent = nullptr);
    ~MprisDBusService() override;

    void setMediaState(const core::MediaState& state) override;
    [[nodiscard]] bool isRegistered() const override { return m_registered; }

    // Read by the adaptors.
    [[nodiscard]] const core::MediaState& state() const { return m_state; }
    [[nodiscard]] QString identity() const { return m_identity; }
    [[nodiscard]] QString desktopEntry() const { return m_desktopEntry; }
    [[nodiscard]] QString playbackStatus() const;
    [[nodiscard]] QVariantMap metadata() const;
    [[nodiscard]] qlonglong positionUs() const;
    [[nodiscard]] double volume() const { return 1.0; }
    [[nodiscard]] QString trackId() const;

Q_SIGNALS:
    void openUriRequested(const QString& uri);

private:
    void emitPropertiesChanged(const QVariantMap& changed);

    QString m_identity;
    QString m_desktopEntry;
    QString m_serviceName;
    core::MediaState m_state;
    QElapsedTimer m_clock;
    bool m_registered = false;
};

} // namespace red::platform::linux_
