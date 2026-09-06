#include "core/downloads/download_runner.h"

#include "core/downloads/engine_spec.h"
#include "core/logging.h"

#include <QTemporaryFile>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace red::core {

namespace {
constexpr int kStderrTailLines = 40;
constexpr int kTerminateGraceMs = 3000;
} // namespace

DownloadRunner::DownloadRunner(quint64 jobId, const QString& program, const QStringList& arguments,
                               std::unique_ptr<QTemporaryFile> cookiesFile, QObject* parent)
    : QObject(parent)
    , m_jobId(jobId)
    , m_program(program)
    , m_arguments(arguments)
    , m_cookies(std::move(cookiesFile))
    , m_process(new QProcess(this))
{
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setProcessEnvironment(engineProcessEnvironment());
    connect(m_process, &QProcess::readyReadStandardOutput, this, &DownloadRunner::readStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &DownloadRunner::readStderr);
    connect(m_process, &QProcess::finished, this, &DownloadRunner::handleFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCWarning(lcDownloads) << "job" << m_jobId << "failed to start" << m_program;
            Q_EMIT finished(m_jobId, false, -1, u"Could not start the download engine (%1)."_s.arg(m_program));
        }
    });
}

DownloadRunner::~DownloadRunner()
{
    // The cookies file (ADR-006) must not outlive the process; QTemporaryFile
    // removes it on destruction, after the process is gone.
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void DownloadRunner::start()
{
    qCInfo(lcDownloads).noquote() << "job" << m_jobId << "start:" << commandLine();
    m_process->start(m_program, m_arguments, QIODevice::ReadOnly);
}

void DownloadRunner::stop()
{
    if (m_process->state() == QProcess::NotRunning) {
        return;
    }
    m_stopping = true;
    m_process->terminate();
    QTimer::singleShot(kTerminateGraceMs, this, [this] {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }
    });
}

bool DownloadRunner::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

QString DownloadRunner::commandLine() const
{
    QStringList shown;
    for (const QString& a : m_arguments) {
        // Keep the log readable: the long progress templates add nothing.
        shown << (a.startsWith(u"download:"_s) || a.startsWith(u"postprocess:"_s) || a.startsWith(u"before_dl:"_s)
                      ? u"<template>"_s
                      : a);
    }
    return m_program + u' ' + shown.join(u' ');
}

void DownloadRunner::readStdout()
{
    m_stdoutBuffer += m_process->readAllStandardOutput();
    qsizetype newline = -1;
    while ((newline = m_stdoutBuffer.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_stdoutBuffer.left(newline));
        m_stdoutBuffer.remove(0, newline + 1);
        handleLine(line, false);
    }
}

void DownloadRunner::readStderr()
{
    m_stderrBuffer += m_process->readAllStandardError();
    qsizetype newline = -1;
    while ((newline = m_stderrBuffer.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_stderrBuffer.left(newline));
        m_stderrBuffer.remove(0, newline + 1);
        handleLine(line, true);
    }
}

void DownloadRunner::handleLine(const QString& line, bool fromStderr)
{
    const auto event = parseOutputLine(line);
    if (!event) {
        return;
    }
    if (fromStderr) {
        m_stderrTail << line.trimmed();
        while (m_stderrTail.size() > kStderrTailLines) {
            m_stderrTail.removeFirst();
        }
    }
    std::visit(
        [this](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, ProgressEvent>) {
                Q_EMIT progress(m_jobId, e);
            } else if constexpr (std::is_same_v<T, PostprocessEvent>) {
                Q_EMIT postprocess(m_jobId, e);
            } else if constexpr (std::is_same_v<T, ItemEvent>) {
                Q_EMIT item(m_jobId, e);
            } else if constexpr (std::is_same_v<T, FileEvent>) {
                Q_EMIT fileWritten(m_jobId, e.path);
            } else if constexpr (std::is_same_v<T, MessageEvent>) {
                if (e.level == MessageEvent::Level::Error) {
                    m_sawError = true;
                    qCWarning(lcDownloads).noquote() << "job" << m_jobId << "error:" << e.text;
                } else if (e.level == MessageEvent::Level::Warning) {
                    qCInfo(lcDownloads).noquote() << "job" << m_jobId << "warning:" << e.text;
                } else {
                    qCDebug(lcDownloads).noquote() << "job" << m_jobId << e.text;
                }
                Q_EMIT message(m_jobId, e);
            }
        },
        *event);
}

void DownloadRunner::handleFinished(int exitCode, QProcess::ExitStatus status)
{
    readStdout();
    readStderr();
    if (!m_stdoutBuffer.isEmpty()) {
        handleLine(QString::fromUtf8(m_stdoutBuffer), false);
        m_stdoutBuffer.clear();
    }
    if (!m_stderrBuffer.isEmpty()) {
        handleLine(QString::fromUtf8(m_stderrBuffer), true);
        m_stderrBuffer.clear();
    }
    const bool ok = !m_stopping && status == QProcess::NormalExit && exitCode == 0;
    qCInfo(lcDownloads) << "job" << m_jobId << "finished ok=" << ok << "exit=" << exitCode
                        << "stopping=" << m_stopping;
    Q_EMIT finished(m_jobId, ok, m_stopping ? -1 : exitCode, m_stderrTail.join(u'\n'));
}

} // namespace red::core
