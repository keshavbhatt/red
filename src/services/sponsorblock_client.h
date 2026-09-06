#pragma once

#include "core/blocking/sponsorblock.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;

namespace red::core {
class Settings;
}

namespace red::services {

/// Fetches SponsorBlock segments for a video (ADR-007), caching per video id
/// for the session. Category policy comes from Settings; the result is the
/// page-ready JSON array.
class SponsorBlockClient : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SponsorBlockClient)

public:
    explicit SponsorBlockClient(core::Settings& settings, QObject* parent = nullptr);
    ~SponsorBlockClient() override = default;

    /// Emits segmentsReady (possibly empty) once, from cache or the network.
    void request(const QString& videoId);
    void clearCache();

Q_SIGNALS:
    void segmentsReady(const QString& videoId, const QJsonArray& segments);

private:
    core::Settings& m_settings;
    QNetworkAccessManager* m_network;
    QHash<QString, QList<core::SponsorSegment>> m_cache;
    QSet<QString> m_inFlight;
};

} // namespace red::services
