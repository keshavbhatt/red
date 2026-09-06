#include "core/downloads/download_job.h"

#include "core/downloads/media_info.h"

#include <QJsonArray>

using namespace Qt::StringLiterals;

namespace red::core {

bool isActiveState(DownloadState state)
{
    return state == DownloadState::Probing || state == DownloadState::Downloading ||
           state == DownloadState::Processing;
}

bool isFinishedState(DownloadState state)
{
    return state == DownloadState::Completed || state == DownloadState::Failed || state == DownloadState::Cancelled;
}

QString stateLabel(DownloadState state)
{
    switch (state) {
    case DownloadState::Queued:
        return u"Queued"_s;
    case DownloadState::Probing:
        return u"Starting"_s;
    case DownloadState::Downloading:
        return u"Downloading"_s;
    case DownloadState::Processing:
        return u"Processing"_s;
    case DownloadState::Paused:
        return u"Paused"_s;
    case DownloadState::Completed:
        return u"Finished"_s;
    case DownloadState::Failed:
        return u"Failed"_s;
    case DownloadState::Cancelled:
        return u"Cancelled"_s;
    }
    return {};
}

double DownloadJob::progress() const
{
    if (state == DownloadState::Completed) {
        return 1.0;
    }
    if (totalBytes <= 0) {
        return -1.0;
    }
    return std::clamp(static_cast<double>(downloadedBytes) / static_cast<double>(totalBytes), 0.0, 1.0);
}

QString DownloadJob::statusLine() const
{
    switch (state) {
    case DownloadState::Queued:
        return u"Queued"_s;
    case DownloadState::Probing:
        return u"Reading video info…"_s;
    case DownloadState::Downloading: {
        QString line;
        if (totalBytes > 0) {
            line = u"%1 of %2"_s.arg(formatBytes(downloadedBytes), formatBytes(totalBytes));
        } else if (downloadedBytes > 0) {
            line = formatBytes(downloadedBytes);
        } else {
            line = u"Starting…"_s;
        }
        if (bytesPerSecond > 0) {
            line += u" · %1/s"_s.arg(formatBytes(static_cast<qint64>(bytesPerSecond)));
        }
        if (etaSeconds > 0) {
            line += u" · %1"_s.arg(formatDuration(etaSeconds));
        }
        if (itemCount > 1) {
            line += u" · %1 of %2"_s.arg(itemIndex).arg(itemCount);
        }
        return line;
    }
    case DownloadState::Processing:
        return stage.isEmpty() ? u"Processing…"_s : stage + u"…"_s;
    case DownloadState::Paused:
        if (totalBytes > 0) {
            return u"Paused · %1 of %2"_s.arg(formatBytes(downloadedBytes), formatBytes(totalBytes));
        }
        return u"Paused"_s;
    case DownloadState::Completed: {
        QString line = u"Finished"_s;
        if (totalBytes > 0) {
            line += u" · "_s + formatBytes(totalBytes);
        }
        if (itemCount > 1) {
            line += u" · %1 items"_s.arg(itemCount);
        }
        return line;
    }
    case DownloadState::Failed:
        return error.isEmpty() ? u"Failed"_s : u"Failed · "_s + error;
    case DownloadState::Cancelled:
        return u"Cancelled"_s;
    }
    return {};
}

QString DownloadJob::primaryFile() const
{
    return outputFiles.isEmpty() ? QString() : outputFiles.first();
}

QJsonObject DownloadJob::toJson() const
{
    QJsonArray files;
    for (const QString& f : outputFiles) {
        files.append(f);
    }
    return {
        {u"id"_s, static_cast<qint64>(id)},
        {u"url"_s, url},
        {u"videoId"_s, videoId},
        {u"title"_s, title},
        {u"uploader"_s, uploader},
        {u"thumbnail"_s, thumbnail},
        {u"duration"_s, duration},
        {u"options"_s, options.toJson()},
        {u"state"_s, static_cast<int>(state)},
        {u"downloadedBytes"_s, static_cast<qint64>(downloadedBytes)},
        {u"totalBytes"_s, static_cast<qint64>(totalBytes)},
        {u"itemIndex"_s, itemIndex},
        {u"itemCount"_s, itemCount},
        {u"outputFiles"_s, files},
        {u"error"_s, error},
        {u"createdAt"_s, createdAt.toString(Qt::ISODate)},
        {u"finishedAt"_s, finishedAt.toString(Qt::ISODate)},
    };
}

DownloadJob DownloadJob::fromJson(const QJsonObject& o)
{
    DownloadJob j;
    j.id = static_cast<quint64>(o.value(u"id"_s).toDouble());
    j.url = o.value(u"url"_s).toString();
    j.videoId = o.value(u"videoId"_s).toString();
    j.title = o.value(u"title"_s).toString();
    j.uploader = o.value(u"uploader"_s).toString();
    j.thumbnail = o.value(u"thumbnail"_s).toString();
    j.duration = o.value(u"duration"_s).toDouble();
    j.options = DownloadOptions::fromJson(o.value(u"options"_s).toObject());
    j.state = static_cast<DownloadState>(std::clamp(o.value(u"state"_s).toInt(), 0, 7));
    j.downloadedBytes = static_cast<qint64>(o.value(u"downloadedBytes"_s).toDouble());
    j.totalBytes = static_cast<qint64>(o.value(u"totalBytes"_s).toDouble());
    j.itemIndex = o.value(u"itemIndex"_s).toInt();
    j.itemCount = o.value(u"itemCount"_s).toInt();
    const QJsonArray files = o.value(u"outputFiles"_s).toArray();
    for (const QJsonValue& f : files) {
        j.outputFiles << f.toString();
    }
    j.error = o.value(u"error"_s).toString();
    j.createdAt = QDateTime::fromString(o.value(u"createdAt"_s).toString(), Qt::ISODate);
    j.finishedAt = QDateTime::fromString(o.value(u"finishedAt"_s).toString(), Qt::ISODate);
    return j;
}

} // namespace red::core
