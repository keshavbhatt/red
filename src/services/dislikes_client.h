#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QNetworkAccessManager;

namespace red::core {
class Settings;
}

namespace red::services {

/// Return YouTube Dislike (FEATURES D14): fetches the estimated dislike count
/// for a video from returnyoutubedislikeapi.com, cached per session.
class DislikesClient : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DislikesClient)

public:
    explicit DislikesClient(core::Settings& settings, QObject* parent = nullptr);
    ~DislikesClient() override = default;

    void request(const QString& videoId);

    /// Parses the API's JSON body; -1 when the count is missing.
    [[nodiscard]] static double parseDislikes(const QByteArray& body);

Q_SIGNALS:
    void dislikesReady(const QString& videoId, double count);

private:
    core::Settings& m_settings;
    QNetworkAccessManager* m_network;
    QHash<QString, double> m_cache;
    QSet<QString> m_inFlight;
};

} // namespace red::services
