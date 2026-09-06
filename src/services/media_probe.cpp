#include "services/media_probe.h"

#include "core/downloads/ytdlp_output.h"
#include "services/logging.h"

#include <QProcess>
#include <QTemporaryFile>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace red::services {

namespace {
constexpr int kProbeTimeoutMs = 120000;
} // namespace

MediaProbe::MediaProbe(QObject* parent)
    : QObject(parent)
{}

MediaProbe::~MediaProbe()
{
    cancelAll();
}

quint64 MediaProbe::probe(const QUrl& url, bool flat)
{
    const quint64 id = m_nextId++;
    if (!hasEngine()) {
        QTimer::singleShot(0, this, [this, id] { Q_EMIT failed(id, tr("The download engine is not ready.")); });
        return id;
    }
    Run run;
    core::EnginePaths paths = m_paths;
    if (m_cookies) {
        run.cookies = std::shared_ptr<QTemporaryFile>(m_cookies().release());
        if (run.cookies) {
            paths.cookiesFile = run.cookies->fileName();
        }
    }
    run.process = new QProcess(this);
    run.process->setProcessChannelMode(QProcess::SeparateChannels);
    const QStringList args = core::probeArguments(paths, url.toString(), flat);
    connect(run.process, &QProcess::finished, this, [this, id] { handleFinished(id); });
    connect(run.process, &QProcess::errorOccurred, this, [this, id](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            handleFinished(id);
        }
    });
    QTimer::singleShot(kProbeTimeoutMs, run.process, [this, id] {
        const auto it = m_runs.constFind(id);
        if (it != m_runs.constEnd() && it->process->state() != QProcess::NotRunning) {
            qCWarning(lcServices) << "probe" << id << "timed out";
            it->process->kill();
        }
    });
    qCInfo(lcServices) << "probe" << id << (flat ? "flat" : "single") << url.toString();
    run.process->start(m_paths.ytdlp, args, QIODevice::ReadOnly);
    m_runs.insert(id, run);
    return id;
}

void MediaProbe::cancel(quint64 id)
{
    const auto it = m_runs.find(id);
    if (it == m_runs.end()) {
        return;
    }
    QProcess* p = it->process;
    disconnect(p, nullptr, this, nullptr);
    m_runs.erase(it);
    if (p->state() != QProcess::NotRunning) {
        p->kill();
        connect(p, &QProcess::finished, p, &QObject::deleteLater);
    } else {
        p->deleteLater();
    }
}

void MediaProbe::cancelAll()
{
    const QList<quint64> ids = m_runs.keys();
    for (const quint64 id : ids) {
        cancel(id);
    }
}

void MediaProbe::handleFinished(quint64 id)
{
    const auto it = m_runs.find(id);
    if (it == m_runs.end()) {
        return;
    }
    QProcess* p = it->process;
    m_runs.erase(it); // drops the cookies file
    p->deleteLater();
    if (p->error() == QProcess::FailedToStart) {
        Q_EMIT failed(id, tr("Could not start the download engine."));
        return;
    }
    const QByteArray out = p->readAllStandardOutput();
    const QString err = QString::fromUtf8(p->readAllStandardError());
    if (p->exitStatus() != QProcess::NormalExit || p->exitCode() != 0 || out.trimmed().isEmpty()) {
        const QString message = core::friendlyError(err, p->exitCode());
        qCWarning(lcServices) << "probe" << id << "failed:" << message;
        Q_EMIT failed(id, message);
        return;
    }
    QString parseError;
    const core::MediaInfo info = core::parseMediaInfo(out, &parseError);
    if (info.id.isEmpty() && info.title.isEmpty()) {
        Q_EMIT failed(id, parseError.isEmpty() ? tr("No video information was returned.") : parseError);
        return;
    }
    qCInfo(lcServices) << "probe" << id << "ok:" << info.title << info.formats.size() << "formats"
                       << info.entries.size() << "entries";
    Q_EMIT finished(id, info);
}

} // namespace red::services
