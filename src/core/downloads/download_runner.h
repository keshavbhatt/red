#pragma once

#include "core/downloads/download_job.h"
#include "core/downloads/ytdlp_output.h"

#include <QObject>
#include <QProcess>
#include <QString>

#include <memory>

class QTemporaryFile;

namespace red::core {

/// Runs one yt-dlp process for one job and turns its output into typed
/// events (ADR-005). Owns the cookies temp file for the lifetime of the
/// process (ADR-006). Pure QtCore.
class DownloadRunner : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DownloadRunner)

public:
    DownloadRunner(quint64 jobId, const QString& program, const QStringList& arguments,
                   std::unique_ptr<QTemporaryFile> cookiesFile, QObject* parent = nullptr);
    ~DownloadRunner() override;

    void start();
    /// Terminates the process; `finished` follows with ok=false.
    void stop();
    [[nodiscard]] quint64 jobId() const { return m_jobId; }
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] QString commandLine() const;

Q_SIGNALS:
    void progress(quint64 jobId, const red::core::ProgressEvent& event);
    void postprocess(quint64 jobId, const red::core::PostprocessEvent& event);
    void item(quint64 jobId, const red::core::ItemEvent& event);
    void fileWritten(quint64 jobId, const QString& path);
    void message(quint64 jobId, const red::core::MessageEvent& event);
    /// exitCode is -1 when the process failed to start or was killed.
    void finished(quint64 jobId, bool ok, int exitCode, const QString& stderrTail);

private:
    void readStdout();
    void readStderr();
    void handleLine(const QString& line, bool fromStderr);
    void handleFinished(int exitCode, QProcess::ExitStatus status);

    quint64 m_jobId;
    QString m_program;
    QStringList m_arguments;
    std::unique_ptr<QTemporaryFile> m_cookies;
    QProcess* m_process = nullptr;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
    QStringList m_stderrTail;
    bool m_stopping = false;
    bool m_sawError = false;
};

} // namespace red::core
