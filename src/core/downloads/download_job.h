#pragma once

#include "core/downloads/download_options.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>

// One entry of the download queue (FEATURES E6). Pure data + JSON; the
// process is driven by DownloadRunner, the list by DownloadQueue.
namespace red::core {

enum class DownloadState
{
    Queued,
    Probing,     ///< reading metadata before the transfer starts
    Downloading,
    Processing,  ///< merging / converting / embedding
    Paused,
    Completed,
    Failed,
    Cancelled,
};

[[nodiscard]] bool isActiveState(DownloadState state);
[[nodiscard]] bool isFinishedState(DownloadState state);
[[nodiscard]] QString stateLabel(DownloadState state);

struct DownloadJob
{
    quint64 id = 0;
    QString url;         ///< what yt-dlp is given
    QString videoId;     ///< YouTube id when known (thumbnail, dedupe)
    QString title;
    QString uploader;
    QString thumbnail;   ///< URL
    double duration = 0;
    DownloadOptions options;
    DownloadState state = DownloadState::Queued;

    // progress (runtime, partly persisted for the resumed view)
    qint64 downloadedBytes = 0;
    qint64 totalBytes = 0;
    double bytesPerSecond = 0; ///< runtime only
    int etaSeconds = 0;        ///< runtime only
    QString stage;             ///< "Merging streams", … runtime only
    int itemIndex = 0;         ///< playlist: current item (1-based)
    int itemCount = 0;         ///< playlist: total items
    QString currentItemTitle;  ///< playlist: current item

    QStringList outputFiles;   ///< final paths (after_move)
    QString error;
    QDateTime createdAt;
    QDateTime finishedAt;

    [[nodiscard]] bool isPlaylist() const { return options.isPlaylist; }
    [[nodiscard]] bool isActive() const { return isActiveState(state); }
    [[nodiscard]] bool isFinished() const { return isFinishedState(state); }
    /// 0..1, or -1 when unknown.
    [[nodiscard]] double progress() const;
    /// A short status line for the card ("12.4 MB of 118 MB · 3.2 MB/s · 0:32").
    [[nodiscard]] QString statusLine() const;
    /// The primary output file (first), or empty.
    [[nodiscard]] QString primaryFile() const;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static DownloadJob fromJson(const QJsonObject& object);
};

} // namespace red::core

Q_DECLARE_METATYPE(red::core::DownloadState)
