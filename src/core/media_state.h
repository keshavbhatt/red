#pragma once

#include <QJsonObject>
#include <QString>

// What is playing in the page, as reported by media-session.js (ADR-007).
namespace red::core {

struct MediaState
{
    enum class Playback
    {
        None,    ///< no media element / nothing loaded
        Playing,
        Paused,
    };

    Playback playback = Playback::None;
    QString videoId;
    QString title;
    QString artist;  ///< channel name
    QString artwork; ///< thumbnail URL
    double duration = 0; ///< seconds
    double position = 0; ///< seconds at the time of the report
    qint64 reportedAtMs = 0; ///< monotonic ms when `position` was sampled
    bool canGoNext = false;
    bool canGoPrevious = false;
    bool live = false;

    [[nodiscard]] bool isPlaying() const { return playback == Playback::Playing; }
    [[nodiscard]] bool hasMedia() const { return playback != Playback::None; }
    [[nodiscard]] static MediaState fromJson(const QJsonObject& object, qint64 nowMs);
};

inline MediaState MediaState::fromJson(const QJsonObject& o, qint64 nowMs)
{
    MediaState s;
    const QString state = o.value(QStringLiteral("state")).toString();
    s.playback = state == QStringLiteral("playing") ? Playback::Playing
                 : state == QStringLiteral("paused") ? Playback::Paused
                                                     : Playback::None;
    s.videoId = o.value(QStringLiteral("videoId")).toString();
    s.title = o.value(QStringLiteral("title")).toString();
    s.artist = o.value(QStringLiteral("artist")).toString();
    s.artwork = o.value(QStringLiteral("artwork")).toString();
    s.duration = o.value(QStringLiteral("duration")).toDouble();
    s.position = o.value(QStringLiteral("position")).toDouble();
    s.canGoNext = o.value(QStringLiteral("canGoNext")).toBool();
    s.canGoPrevious = o.value(QStringLiteral("canGoPrevious")).toBool();
    s.live = o.value(QStringLiteral("live")).toBool();
    s.reportedAtMs = nowMs;
    return s;
}

} // namespace red::core
