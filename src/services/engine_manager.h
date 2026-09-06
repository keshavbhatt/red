#pragma once

#include "core/downloads/download_options.h"
#include "core/downloads/engine_spec.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryFile;

namespace red::core {
class Settings;
}

namespace red::services {

/// Provisions and updates the download engine (ADR-004): yt-dlp, a JavaScript
/// runtime and ffmpeg, into <AppData>/engine, verifying SHA-256 where the
/// release publishes sums. Everything is asynchronous; progress goes out as
/// status changes for the UI's setup sheet and settings card.
class EngineManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(EngineManager)

public:
    enum class State
    {
        Unknown,      ///< initialize() not called yet
        NotInstalled, ///< something is missing; install() will fetch it
        Installing,
        Ready,
        Updating,
        Error,
    };

    struct Status
    {
        State state = State::Unknown;
        QString ytdlpPath;
        QString ytdlpVersion;
        QString jsRuntime;    ///< --js-runtimes spec
        QString ffmpegPath;
        QString latestVersion;
        bool updateAvailable = false;
        QString stepLabel;    ///< "Downloading yt-dlp…" while installing
        double progress = -1; ///< 0..1 of the current step, -1 indeterminate
        QString error;
        bool systemYtdlp = false;

        [[nodiscard]] bool isReady() const { return state == State::Ready; }
        [[nodiscard]] bool isBusy() const { return state == State::Installing || state == State::Updating; }
    };

    explicit EngineManager(core::Settings& settings, QObject* parent = nullptr);
    ~EngineManager() override;

    /// Detects installed/system components. Emits statusChanged and, when
    /// everything is present, ready().
    void initialize();
    /// Fetches whatever is missing. No-op while busy.
    void install();
    /// Asks GitHub for the latest yt-dlp tag; honours the daily throttle unless forced.
    void checkForUpdates(bool force);
    /// Re-downloads yt-dlp (and, if the runtime is ours, qjs).
    void update();

    [[nodiscard]] const Status& status() const { return m_status; }
    [[nodiscard]] core::EnginePaths paths() const;
    [[nodiscard]] bool isReady() const { return m_status.isReady(); }
    [[nodiscard]] static QString engineDirectory();

Q_SIGNALS:
    void statusChanged(const red::services::EngineManager::Status& status);
    void ready(const red::core::EnginePaths& paths);
    void installFailed(const QString& error);

private:
    struct Step;
    void detect();
    void setState(State state, const QString& label = {}, double progress = -1);
    void fail(const QString& error);
    void runNextStep();
    void queueComponent(const core::EngineComponent& component, bool verifySums);
    void fetchRelease(const core::EngineComponent& component, bool verifySums,
                      const std::function<void(const QUrl& asset, const QUrl& sums)>& then);
    void downloadFile(const QUrl& url, const QString& label, const std::function<void(const QString& path)>& then);
    void fetchText(const QUrl& url, const std::function<void(const QByteArray&)>& then);
    void installBinary(const core::EngineComponent& component, const QString& tempPath, const QString& expectedSha);
    void installArchive(const core::EngineComponent& component, const QString& tempPath);
    void finishInstall();
    [[nodiscard]] QString installPath(const core::EngineComponent& component) const;
    [[nodiscard]] QStringList extraSearchDirs() const;

    core::Settings& m_settings;
    QNetworkAccessManager* m_network;
    Status m_status;
    QList<std::function<void()>> m_steps;
    QString m_pendingVersion;
    QString m_os;
    QString m_cpu;
    std::unique_ptr<QTemporaryFile> m_download;
    QNetworkReply* m_activeReply = nullptr;
};

} // namespace red::services

Q_DECLARE_METATYPE(red::services::EngineManager::Status)
