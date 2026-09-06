#pragma once

#include "core/downloads/download_options.h"
#include "core/downloads/media_info.h"

#include <QHash>
#include <QObject>
#include <QUrl>

#include <functional>
#include <memory>

class QProcess;
class QTemporaryFile;

namespace red::services {

/// Runs `yt-dlp -J` for a URL and hands back a typed MediaInfo (FEATURES E3).
/// Playlists and channels are probed flat (entries without formats).
class MediaProbe : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MediaProbe)

public:
    using CookiesProvider = std::function<std::unique_ptr<QTemporaryFile>()>;

    explicit MediaProbe(QObject* parent = nullptr);
    ~MediaProbe() override;

    void setEnginePaths(const core::EnginePaths& paths) { m_paths = paths; }
    void setCookiesProvider(CookiesProvider provider) { m_cookies = std::move(provider); }
    [[nodiscard]] bool hasEngine() const { return !m_paths.ytdlp.isEmpty(); }

    /// Starts a probe; returns its id. `flat` for playlists/channels.
    quint64 probe(const QUrl& url, bool flat);
    void cancel(quint64 id);
    void cancelAll();

Q_SIGNALS:
    void finished(quint64 id, const red::core::MediaInfo& info);
    void failed(quint64 id, const QString& error);

private:
    struct Run
    {
        QProcess* process = nullptr;
        std::shared_ptr<QTemporaryFile> cookies; // shared: QHash needs copyable values
    };
    void handleFinished(quint64 id);

    core::EnginePaths m_paths;
    CookiesProvider m_cookies;
    QHash<quint64, Run> m_runs;
    quint64 m_nextId = 1;
};

} // namespace red::services
