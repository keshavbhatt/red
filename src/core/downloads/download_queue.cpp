#include "core/downloads/download_queue.h"

#include "core/downloads/download_runner.h"
#include "core/downloads/media_info.h"
#include "core/logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>

using namespace Qt::StringLiterals;

namespace red::core {

DownloadQueue::DownloadQueue(QObject* parent)
    : QAbstractListModel(parent)
{}

DownloadQueue::~DownloadQueue() = default;

// ---- model -----------------------------------------------------------------

int DownloadQueue::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_jobs.size());
}

QVariant DownloadQueue::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_jobs.size()) {
        return {};
    }
    const DownloadJob& j = m_jobs.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return j.title.isEmpty() ? j.url : j.title;
    case IdRole:
        return QVariant::fromValue(j.id);
    case UploaderRole:
        return j.uploader;
    case ThumbnailRole:
        return j.thumbnail;
    case StateRole:
        return QVariant::fromValue(j.state);
    case ProgressRole:
        return j.progress();
    case StatusLineRole:
        return j.statusLine();
    case FilePathRole:
        return j.primaryFile();
    case IsPlaylistRole:
        return j.isPlaylist();
    case ErrorRole:
        return j.error;
    default:
        return {};
    }
}

QHash<int, QByteArray> DownloadQueue::roleNames() const
{
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {UploaderRole, "uploader"},
        {ThumbnailRole, "thumbnail"},
        {StateRole, "state"},
        {ProgressRole, "progress"},
        {StatusLineRole, "statusLine"},
        {FilePathRole, "filePath"},
        {IsPlaylistRole, "isPlaylist"},
        {ErrorRole, "error"},
    };
}

// ---- configuration ---------------------------------------------------------

void DownloadQueue::setEnginePaths(const EnginePaths& paths)
{
    m_paths = paths;
    schedule();
}

void DownloadQueue::setCookiesProvider(CookiesProvider provider)
{
    m_cookies = std::move(provider);
}

void DownloadQueue::setMaxConcurrent(int count)
{
    m_maxConcurrent = std::max(1, count);
    schedule();
}

// ---- commands --------------------------------------------------------------

quint64 DownloadQueue::add(DownloadJob job)
{
    for (const DownloadJob& existing : m_jobs) {
        if (existing.url == job.url && existing.options.kind == job.options.kind && !existing.isFinished()) {
            qCInfo(lcDownloads) << "duplicate download ignored:" << job.url;
            return existing.id;
        }
    }
    job.id = m_nextId++;
    job.state = DownloadState::Queued;
    job.createdAt = QDateTime::currentDateTime();
    beginInsertRows({}, 0, 0);
    m_jobs.prepend(job);
    endInsertRows();
    while (m_jobs.size() > kMaxEntries) {
        // Drop the oldest finished entry; never an active one.
        for (int i = static_cast<int>(m_jobs.size()) - 1; i >= 0; --i) {
            if (m_jobs.at(i).isFinished()) {
                beginRemoveRows({}, i, i);
                m_jobs.removeAt(i);
                endRemoveRows();
                break;
            }
        }
        break;
    }
    Q_EMIT jobAdded(job.id);
    Q_EMIT changed();
    schedule();
    return job.id;
}

void DownloadQueue::pause(quint64 id)
{
    DownloadJob* j = find(id);
    if (j == nullptr || !(j->isActive() || j->state == DownloadState::Queued)) {
        return;
    }
    j->state = DownloadState::Paused;
    j->bytesPerSecond = 0;
    j->etaSeconds = 0;
    stopRunner(id);
    updateRow(rowOf(id));
    Q_EMIT changed();
    notifyActive();
    schedule();
}

void DownloadQueue::resume(quint64 id)
{
    DownloadJob* j = find(id);
    if (j == nullptr || j->state != DownloadState::Paused) {
        return;
    }
    j->state = DownloadState::Queued;
    j->error.clear();
    updateRow(rowOf(id));
    Q_EMIT changed();
    schedule();
}

void DownloadQueue::cancel(quint64 id)
{
    DownloadJob* j = find(id);
    if (j == nullptr || j->isFinished()) {
        return;
    }
    j->state = DownloadState::Cancelled;
    j->finishedAt = QDateTime::currentDateTime();
    j->bytesPerSecond = 0;
    stopRunner(id);
    updateRow(rowOf(id));
    Q_EMIT jobFinished(id, j->state);
    Q_EMIT changed();
    notifyActive();
    schedule();
}

void DownloadQueue::retry(quint64 id)
{
    DownloadJob* j = find(id);
    if (j == nullptr || !(j->state == DownloadState::Failed || j->state == DownloadState::Cancelled)) {
        return;
    }
    j->state = DownloadState::Queued;
    j->error.clear();
    j->stage.clear();
    j->finishedAt = {};
    updateRow(rowOf(id));
    Q_EMIT changed();
    schedule();
}

void DownloadQueue::remove(quint64 id)
{
    const int row = rowOf(id);
    if (row < 0) {
        return;
    }
    stopRunner(id);
    beginRemoveRows({}, row, row);
    m_jobs.removeAt(row);
    endRemoveRows();
    Q_EMIT changed();
    notifyActive();
    schedule();
}

void DownloadQueue::pauseAll()
{
    const QList<DownloadJob> snapshot = m_jobs;
    for (const DownloadJob& j : snapshot) {
        if (j.isActive() || j.state == DownloadState::Queued) {
            pause(j.id);
        }
    }
}

void DownloadQueue::resumeAll()
{
    const QList<DownloadJob> snapshot = m_jobs;
    for (const DownloadJob& j : snapshot) {
        if (j.state == DownloadState::Paused) {
            resume(j.id);
        }
    }
}

void DownloadQueue::clearFinished()
{
    for (int i = static_cast<int>(m_jobs.size()) - 1; i >= 0; --i) {
        if (m_jobs.at(i).isFinished()) {
            beginRemoveRows({}, i, i);
            m_jobs.removeAt(i);
            endRemoveRows();
        }
    }
    Q_EMIT changed();
}

// ---- queries ---------------------------------------------------------------

std::optional<DownloadJob> DownloadQueue::job(quint64 id) const
{
    for (const DownloadJob& j : m_jobs) {
        if (j.id == id) {
            return j;
        }
    }
    return std::nullopt;
}

int DownloadQueue::rowOf(quint64 id) const
{
    for (int i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

int DownloadQueue::activeCount() const
{
    int n = 0;
    for (const DownloadJob& j : m_jobs) {
        if (j.isActive() || j.state == DownloadState::Queued) {
            ++n;
        }
    }
    return n;
}

DownloadJob* DownloadQueue::find(quint64 id)
{
    const int row = rowOf(id);
    return row < 0 ? nullptr : &m_jobs[row];
}

void DownloadQueue::updateRow(int row, const QList<int>& roles)
{
    if (row >= 0 && row < m_jobs.size()) {
        const QModelIndex idx = index(row);
        Q_EMIT dataChanged(idx, idx, roles);
    }
}

void DownloadQueue::notifyActive()
{
    const int active = activeCount();
    if (active != m_lastActive) {
        m_lastActive = active;
        Q_EMIT activeCountChanged(active);
    }
}

// ---- scheduling ------------------------------------------------------------

void DownloadQueue::schedule()
{
    if (!hasEngine()) {
        return;
    }
    // Oldest queued first (the list is newest-first).
    for (int i = static_cast<int>(m_jobs.size()) - 1; i >= 0 && runningCount() < m_maxConcurrent; --i) {
        DownloadJob& j = m_jobs[i];
        if (j.state == DownloadState::Queued && !m_runners.contains(j.id)) {
            startJob(j);
        }
    }
    notifyActive();
}

void DownloadQueue::startJob(DownloadJob& job)
{
    EnginePaths paths = m_paths;
    std::unique_ptr<QTemporaryFile> cookies;
    if (m_cookies) {
        cookies = m_cookies();
        if (cookies) {
            paths.cookiesFile = cookies->fileName();
        }
    }
    QDir().mkpath(job.options.outputDirectory);
    const QStringList args = downloadArguments(job.options, paths, job.url);
    auto* runner = new DownloadRunner(job.id, m_paths.ytdlp, args, std::move(cookies), this);
    connect(runner, &DownloadRunner::progress, this, &DownloadQueue::handleProgress);
    connect(runner, &DownloadRunner::postprocess, this, &DownloadQueue::handlePostprocess);
    connect(runner, &DownloadRunner::item, this, &DownloadQueue::handleItem);
    connect(runner, &DownloadRunner::fileWritten, this, &DownloadQueue::handleFile);
    connect(runner, &DownloadRunner::finished, this, &DownloadQueue::handleFinished);
    m_runners.insert(job.id, runner);
    job.state = DownloadState::Probing;
    job.error.clear();
    job.stage.clear();
    job.outputFiles.clear();
    updateRow(rowOf(job.id));
    runner->start();
}

void DownloadQueue::stopRunner(quint64 id)
{
    DownloadRunner* runner = m_runners.take(id);
    if (runner == nullptr) {
        return;
    }
    // Detach first so the finished signal of a stopped runner is ignored.
    disconnect(runner, nullptr, this, nullptr);
    runner->stop();
    connect(runner, &DownloadRunner::finished, runner, &QObject::deleteLater);
    if (!runner->isRunning()) {
        runner->deleteLater();
    }
}

// ---- runner events ---------------------------------------------------------

void DownloadQueue::handleProgress(quint64 id, const ProgressEvent& e)
{
    DownloadJob* j = find(id);
    if (j == nullptr) {
        return;
    }
    if (e.status == u"downloading"_s || e.status == u"finished"_s) {
        j->state = DownloadState::Downloading;
        j->downloadedBytes = e.downloaded;
        if (e.totalOrEstimate() > 0) {
            j->totalBytes = e.totalOrEstimate();
        }
        j->bytesPerSecond = e.speed;
        j->etaSeconds = e.eta;
        if (e.count > 0) {
            j->itemCount = e.count;
            j->itemIndex = e.index;
        }
    }
    updateRow(rowOf(id), {ProgressRole, StatusLineRole, StateRole});
}

void DownloadQueue::handlePostprocess(quint64 id, const PostprocessEvent& e)
{
    DownloadJob* j = find(id);
    if (j == nullptr) {
        return;
    }
    if (e.status == u"started"_s) {
        j->state = DownloadState::Processing;
        j->stage = e.label();
        j->bytesPerSecond = 0;
        j->etaSeconds = 0;
    }
    updateRow(rowOf(id), {StatusLineRole, StateRole});
}

void DownloadQueue::handleItem(quint64 id, const ItemEvent& e)
{
    DownloadJob* j = find(id);
    if (j == nullptr) {
        return;
    }
    if (j->isPlaylist()) {
        j->currentItemTitle = e.title;
        if (e.count > 0) {
            j->itemCount = e.count;
            j->itemIndex = e.index;
        }
    } else {
        // Fill in what the dialog did not know (e.g. a pasted URL).
        if (j->title.isEmpty()) {
            j->title = e.title;
        }
        if (j->uploader.isEmpty()) {
            j->uploader = e.uploader;
        }
        if (j->thumbnail.isEmpty()) {
            j->thumbnail = e.thumbnail;
        }
        if (j->videoId.isEmpty()) {
            j->videoId = e.id;
        }
        if (j->duration <= 0) {
            j->duration = e.duration;
        }
    }
    // A new item starts: reset the byte counters so the bar restarts.
    j->downloadedBytes = 0;
    j->totalBytes = 0;
    j->state = DownloadState::Downloading;
    updateRow(rowOf(id));
    Q_EMIT changed();
}

void DownloadQueue::handleFile(quint64 id, const QString& path)
{
    DownloadJob* j = find(id);
    if (j == nullptr || path.isEmpty()) {
        return;
    }
    if (!j->outputFiles.contains(path)) {
        j->outputFiles << path;
    }
}

void DownloadQueue::handleFinished(quint64 id, bool ok, int exitCode, const QString& stderrTail)
{
    DownloadRunner* runner = m_runners.take(id);
    if (runner != nullptr) {
        runner->deleteLater();
    }
    DownloadJob* j = find(id);
    if (j != nullptr) {
        j->bytesPerSecond = 0;
        j->etaSeconds = 0;
        j->stage.clear();
        j->finishedAt = QDateTime::currentDateTime();
        if (ok || (!j->outputFiles.isEmpty() && j->isPlaylist())) {
            // A playlist with some failed entries still produced files; report
            // it as finished with a note rather than hiding the output.
            j->state = DownloadState::Completed;
            if (!ok) {
                j->error = u"Some items could not be downloaded."_s;
            }
            // The progress counters covered one stream at a time; report the
            // files that actually landed on disk.
            qint64 onDisk = 0;
            for (const QString& path : std::as_const(j->outputFiles)) {
                onDisk += std::max<qint64>(0, QFileInfo(path).size());
            }
            if (onDisk > 0) {
                j->totalBytes = onDisk;
            }
            j->downloadedBytes = j->totalBytes;
        } else {
            j->state = DownloadState::Failed;
            j->error = friendlyError(stderrTail, exitCode);
        }
        updateRow(rowOf(id));
        Q_EMIT jobFinished(id, j->state);
    }
    Q_EMIT changed();
    notifyActive();
    schedule();
}

// ---- persistence -----------------------------------------------------------

QString DownloadQueue::defaultFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/downloads.json"_s;
}

bool DownloadQueue::load(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }
    const QJsonObject root = doc.object();
    QList<DownloadJob> loaded;
    const QJsonArray entries = root.value(u"jobs"_s).toArray();
    for (const QJsonValue& v : entries) {
        DownloadJob j = DownloadJob::fromJson(v.toObject());
        if (j.id == 0 || j.url.isEmpty()) {
            continue;
        }
        // Transfers do not survive a restart; --continue picks the .part up.
        if (j.isActive() || j.state == DownloadState::Queued) {
            j.state = DownloadState::Paused;
        }
        loaded << j;
        m_nextId = std::max(m_nextId, j.id + 1);
    }
    beginResetModel();
    m_jobs = loaded;
    endResetModel();
    qCInfo(lcDownloads) << "loaded" << m_jobs.size() << "downloads from" << filePath;
    notifyActive();
    return true;
}

bool DownloadQueue::save(const QString& filePath) const
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QJsonArray entries;
    for (const DownloadJob& j : m_jobs) {
        entries.append(j.toJson());
    }
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcDownloads) << "cannot save downloads to" << filePath << file.errorString();
        return false;
    }
    file.write(
        QJsonDocument(QJsonObject{{u"version"_s, 1}, {u"jobs"_s, entries}}).toJson(QJsonDocument::Compact));
    return file.commit();
}

} // namespace red::core
