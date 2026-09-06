#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

namespace red::web {

/// The single QWebChannel object exposed to injected scripts as `bridge`
/// (ADR-006/007). JS → C++ are the public slots; C++ → JS are the signals.
/// Keep the surface tiny and documented in scripts/README.md.
class Bridge : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Bridge)

public:
    explicit Bridge(QObject* parent = nullptr);
    ~Bridge() override = default;

public Q_SLOTS:
    /// Called by scripts when their try/catch fires.
    void scriptFailed(const QString& name, const QString& message);
    void log(const QString& message);
    /// media-session.js: what is playing (see core::MediaState::fromJson).
    void mediaState(const QJsonObject& state);
    /// sponsorblock.js: the page started a video and wants its segments.
    void requestSponsorSegments(const QString& videoId);
    /// sponsorblock.js: a segment was skipped (for the counter/toast policy).
    void sponsorSkipped(const QString& category);
    /// TV scripts: Leanback asked to exit / reload / open a link.
    void tvSignal(const QString& signal, const QString& argument);
    /// The custom error page's "Try again" was clicked.
    void retry();
    /// A script asked to download the given URL (e.g. TV mode Ctrl+D).
    void downloadRequested(const QString& url);

Q_SIGNALS:
    // JS → C++ (re-emitted for C++ consumers)
    void scriptFailure(const QString& name, const QString& message);
    void mediaStateChanged(const QJsonObject& state);
    void sponsorSegmentsRequested(const QString& videoId);
    void sponsorSegmentSkipped(const QString& category);
    void tvSignalReceived(const QString& signal, const QString& argument);
    void retryRequested();
    void downloadUrlRequested(const QString& url);

    // C++ → JS (consumed by scripts through the channel)
    void sponsorSegments(const QString& videoId, const QJsonArray& segments);
    /// "play", "pause", "playPause", "next", "previous", "seek" (arg = seconds),
    /// "stop", "mute", "volume" (arg = 0..1).
    void mediaCommand(const QString& command, double argument);
    /// The live config object (same shape as window.__red.config).
    void configChanged(const QJsonObject& config);
    /// Mini player entered/left (mini-player.js toggles the page state).
    void miniPlayerChanged(bool on);
};

} // namespace red::web
