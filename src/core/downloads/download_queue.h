#pragma once

#include "core/downloads/download_job.h"
#include "core/downloads/download_options.h"
#include "core/downloads/ytdlp_output.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>

#include <functional>
#include <memory>

class QTemporaryFile;

namespace red::core {

class DownloadRunner;

/// The persistent download list and its scheduler (FEATURES E6). Newest
/// first. Runs at most `maxConcurrent` yt-dlp processes; everything else waits
/// in Queued. Pure QtCore — the engine paths and cookies come from outside.
class DownloadQueue : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DownloadQueue)

public:
    enum Role
    {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        UploaderRole,
        ThumbnailRole,
        StateRole,
        ProgressRole,
        StatusLineRole,
        FilePathRole,
        IsPlaylistRole,
        ErrorRole,
    };

    /// Produces a fresh Netscape cookies file for a run, or nullptr for none.
    using CookiesProvider = std::function<std::unique_ptr<QTemporaryFile>()>;

    explicit DownloadQueue(QObject* parent = nullptr);
    ~DownloadQueue() override;

    // QAbstractListModel
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /// The engine must be provided before anything runs; until then jobs stay Queued.
    void setEnginePaths(const EnginePaths& paths);
    [[nodiscard]] bool hasEngine() const { return !m_paths.ytdlp.isEmpty(); }
    void setCookiesProvider(CookiesProvider provider);
    void setMaxConcurrent(int count);
    /// Directory where each single-video job keeps "<id>.jpg" (its thumbnail,
    /// fetched by yt-dlp with the session); empty = off.
    void setThumbnailDirectory(const QString& dir);
    [[nodiscard]] int maxConcurrent() const { return m_maxConcurrent; }

    /// Adds and schedules. Returns the id. A job for the same url+kind that is
    /// still active or queued is not added twice (returns the existing id).
    quint64 add(DownloadJob job);
    void pause(quint64 id);
    void resume(quint64 id);
    void cancel(quint64 id);
    void retry(quint64 id);
    void remove(quint64 id);
    /// Points the card at a local copy of the thumbnail (kept by the UI).
    void setThumbnail(quint64 id, const QString& path);
    void pauseAll();
    void resumeAll();
    void clearFinished();

    [[nodiscard]] std::optional<DownloadJob> job(quint64 id) const;
    [[nodiscard]] int rowOf(quint64 id) const;
    [[nodiscard]] const QList<DownloadJob>& jobs() const { return m_jobs; }
    [[nodiscard]] int activeCount() const;
    [[nodiscard]] int runningCount() const { return static_cast<int>(m_runners.size()); }

    bool load(const QString& filePath);
    bool save(const QString& filePath) const;
    [[nodiscard]] static QString defaultFilePath();

    static constexpr int kMaxEntries = 300;

Q_SIGNALS:
    void jobFinished(quint64 id, red::core::DownloadState state);
    void jobAdded(quint64 id);
    void jobRemoved(quint64 id); ///< removed by the user, cleared, or aged out
    void activeCountChanged(int count);
    void changed(); ///< anything worth persisting happened

private:
    void schedule();
    void startJob(DownloadJob& job);
    void stopRunner(quint64 id);
    void updateRow(int row, const QList<int>& roles = {});
    DownloadJob* find(quint64 id);
    void handleProgress(quint64 id, const ProgressEvent& event);
    void handlePostprocess(quint64 id, const PostprocessEvent& event);
    void handleItem(quint64 id, const ItemEvent& event);
    void handleFile(quint64 id, const QString& path);
    void handleFinished(quint64 id, bool ok, int exitCode, const QString& stderrTail);
    void notifyActive();

    QList<DownloadJob> m_jobs;
    QHash<quint64, DownloadRunner*> m_runners;
    EnginePaths m_paths;
    QString m_thumbnailDir;
    CookiesProvider m_cookies;
    int m_maxConcurrent = 2;
    quint64 m_nextId = 1;
    int m_lastActive = -1;
};

} // namespace red::core
