#include "services/engine_manager.h"

#include "core/settings/settings.h"
#include "services/logging.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryFile>
#include <QtEnvironmentVariables>

using namespace Qt::StringLiterals;

namespace red::services {

namespace {

constexpr int kUpdateCheckHours = 24;
constexpr int kNetworkTimeoutMs = 60000;
const QString kSumsAsset = u"SHA2-256SUMS"_s;

QString currentOs()
{
#if defined(Q_OS_WIN)
    return u"windows"_s;
#elif defined(Q_OS_MACOS)
    return u"macos"_s;
#else
    return u"linux"_s;
#endif
}

QString readVersionFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed();
}

bool writeVersionFile(const QString& path, const QString& version)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(version.toUtf8());
    return file.commit();
}

QString sha256Of(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace

EngineManager::EngineManager(core::Settings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_network(new QNetworkAccessManager(this))
    , m_os(currentOs())
    , m_cpu(QSysInfo::currentCpuArchitecture())
{
    m_network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    m_network->setTransferTimeout(kNetworkTimeoutMs);
    connect(&m_settings, &core::Settings::engineConfigChanged, this, [this] {
        if (!m_status.isBusy()) {
            detect();
        }
    });
}

EngineManager::~EngineManager() = default;

QString EngineManager::engineDirectory()
{
    return core::engineDirectory();
}

QStringList EngineManager::extraSearchDirs() const
{
    QStringList dirs{engineDirectory()};
    if (qEnvironmentVariableIsSet("SNAP")) {
        dirs << qEnvironmentVariable("SNAP") + u"/usr/bin"_s;
    }
    dirs << QCoreApplication::applicationDirPath();
    return dirs;
}

QString EngineManager::installPath(const core::EngineComponent& component) const
{
    return QDir(engineDirectory()).filePath(component.installName);
}

core::EnginePaths EngineManager::paths() const
{
    core::EnginePaths p;
    p.ytdlp = m_status.ytdlpPath;
    p.ffmpeg = m_status.ffmpegPath;
    p.jsRuntime = m_status.jsRuntime;
    return p;
}

void EngineManager::initialize()
{
    detect();
    if (m_status.isReady() && m_settings.engineAutoUpdate()) {
        checkForUpdates(false);
    }
}

void EngineManager::detect()
{
    QDir().mkpath(engineDirectory());
    Status s;
    s.latestVersion = m_status.latestVersion;
    s.updateAvailable = m_status.updateAvailable;

    // yt-dlp: system binary when the user asked for it, else ours.
    if (m_settings.engineUseSystem()) {
        const QString configured = m_settings.engineSystemPath();
        s.ytdlpPath = !configured.isEmpty() && QFileInfo(configured).isExecutable()
                          ? configured
                          : QStandardPaths::findExecutable(u"yt-dlp"_s);
        s.systemYtdlp = !s.ytdlpPath.isEmpty();
        s.ytdlpVersion = s.systemYtdlp ? u"system"_s : QString();
    }
    if (s.ytdlpPath.isEmpty()) {
        const core::EngineComponent ytdlp = core::ytdlpComponent(m_os, m_cpu);
        const QString ours = installPath(ytdlp);
        if (QFileInfo(ours).isExecutable()) {
            s.ytdlpPath = ours;
            s.ytdlpVersion = readVersionFile(core::engineVersionFilePath());
        }
    }
    // JS runtime: ours first (qjs in the engine dir), then the system.
    s.jsRuntime = core::detectSystemJsRuntime(extraSearchDirs());
    // ffmpeg: the system's (snap/flatpak runtime or distro package), never ours.
    s.ffmpegPath = core::detectSystemFfmpeg(extraSearchDirs());
    s.ffmpegMissing = s.ffmpegPath.isEmpty();

    const bool complete = !s.ytdlpPath.isEmpty() && !s.jsRuntime.isEmpty() && !s.ffmpegMissing;
    s.state = complete ? State::Ready : State::NotInstalled;
    if (s.ffmpegMissing) {
        s.error = tr("ffmpeg is not installed. Red needs it to merge video and audio: %1, then try again.")
                      .arg(core::ffmpegInstallHint());
    }
    m_status = s;
    qCInfo(lcServices) << "engine:" << (complete ? "ready" : "incomplete") << "yt-dlp=" << s.ytdlpPath
                       << s.ytdlpVersion << "js=" << s.jsRuntime << "ffmpeg=" << s.ffmpegPath;
    Q_EMIT statusChanged(m_status);
    if (complete) {
        Q_EMIT ready(paths());
    }
}

void EngineManager::setState(State state, const QString& label, double progress)
{
    m_status.state = state;
    m_status.stepLabel = label;
    m_status.progress = progress;
    Q_EMIT statusChanged(m_status);
}

void EngineManager::fail(const QString& error)
{
    qCWarning(lcServices) << "engine install failed:" << error;
    m_steps.clear();
    m_download.reset();
    m_status.error = error;
    setState(State::Error, error);
    Q_EMIT installFailed(error);
}

// ---- install ---------------------------------------------------------------

void EngineManager::install()
{
    if (m_status.isBusy()) {
        return;
    }
    detect();
    if (m_status.isReady()) {
        return;
    }
    const QString ffmpegError = m_status.ffmpegMissing ? m_status.error : QString();
    m_status.error.clear();
    m_steps.clear();
    if (m_status.ytdlpPath.isEmpty()) {
        queueComponent(core::ytdlpComponent(m_os, m_cpu), true);
    }
    if (m_status.jsRuntime.isEmpty()) {
        queueComponent(core::jsRuntimeComponent(m_os, m_cpu), false);
    }
    if (m_steps.isEmpty()) {
        // Only ffmpeg is missing: nothing to download, the user has to install it.
        m_status.error = ffmpegError;
        setState(State::Error, ffmpegError);
        Q_EMIT installFailed(ffmpegError);
        return;
    }
    m_steps.append([this] { finishInstall(); });
    setState(State::Installing, tr("Preparing…"));
    runNextStep();
}

void EngineManager::update()
{
    if (m_status.isBusy() || m_status.systemYtdlp) {
        return;
    }
    m_status.error.clear();
    m_steps.clear();
    queueComponent(core::ytdlpComponent(m_os, m_cpu), true);
    m_steps.append([this] { finishInstall(); });
    setState(State::Updating, tr("Updating yt-dlp…"));
    runNextStep();
}

void EngineManager::runNextStep()
{
    if (m_steps.isEmpty()) {
        return;
    }
    const auto step = m_steps.takeFirst();
    step();
}

void EngineManager::queueComponent(const core::EngineComponent& component, bool verifySums)
{
    m_steps.append([this, component, verifySums] {
        const QString label =
            component.kind == core::EngineComponent::Kind::YtDlp ? tr("yt-dlp") : tr("JavaScript runtime");
        setState(m_status.state, tr("Looking up %1…").arg(label));
        fetchRelease(component, verifySums, [this, component, label](const QUrl& asset, const QUrl& sums) {
            if (asset.isEmpty()) {
                fail(tr("No %1 build is published for this platform (%2).").arg(label, component.assetName));
                return;
            }
            auto afterDownload = [this, component](const QString& path, const QString& expectedSha) {
                installBinary(component, path, expectedSha);
            };
            if (sums.isEmpty()) {
                downloadFile(asset, tr("Downloading %1…").arg(label),
                             [afterDownload](const QString& path) { afterDownload(path, {}); });
                return;
            }
            fetchText(sums, [this, asset, label, component, afterDownload](const QByteArray& body) {
                const QString sha = core::sha256FromSums(body, component.assetName);
                if (sha.isEmpty()) {
                    fail(tr("The release checksums do not list %1.").arg(component.assetName));
                    return;
                }
                downloadFile(asset, tr("Downloading %1…").arg(label),
                             [afterDownload, sha](const QString& path) { afterDownload(path, sha); });
            });
        });
    });
}

void EngineManager::fetchRelease(const core::EngineComponent& component, bool verifySums,
                                 const std::function<void(const QUrl&, const QUrl&)>& then)
{
    // No REST API (60 requests/hour per IP): "latest/download/<asset>" answers
    // with a redirect to "releases/download/<tag>/<asset>", which names the
    // version. Only that first hop is followed by hand; the download itself
    // follows the rest (the CDN hop carries a signed, tag-less URL).
    const QUrl asset = core::latestAssetUrl(component.repo, component.assetName);
    QNetworkRequest request(asset);
    request.setHeader(QNetworkRequest::UserAgentHeader, u"Red/10 (+https://github.com/keshavbhatt/red)"_s);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply* reply = m_network->head(request);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, component, verifySums, then] {
        reply->deleteLater();
        m_activeReply = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404) {
            then({}, {}); // no such asset for this platform
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            fail(tr("Network error: %1").arg(reply->errorString()));
            return;
        }
        const QUrl resolved = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        const QString tag = core::tagFromReleaseUrl(resolved);
        qCInfo(lcServices) << "release lookup" << component.assetName << "->" << tag;
        if (resolved.isEmpty() || tag.isEmpty()) {
            fail(tr("GitHub did not point at a release for %1.").arg(component.assetName));
            return;
        }
        if (component.kind == core::EngineComponent::Kind::YtDlp) {
            m_pendingVersion = tag;
        }
        QUrl sums;
        if (verifySums) {
            sums =
                QUrl(u"https://github.com/%1/releases/download/%2/%3"_s.arg(component.repo, tag, kSumsAsset));
        }
        then(resolved, sums);
    });
}

void EngineManager::fetchText(const QUrl& url, const std::function<void(const QByteArray&)>& then)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, u"Red/10 (+https://github.com/keshavbhatt/red)"_s);
    request.setRawHeader("Accept", "application/vnd.github+json, text/plain, */*");
    QNetworkReply* reply = m_network->get(request);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, then] {
        reply->deleteLater();
        m_activeReply = nullptr;
        if (reply->error() != QNetworkReply::NoError) {
            fail(tr("Network error: %1").arg(reply->errorString()));
            return;
        }
        then(reply->readAll());
    });
}

void EngineManager::downloadFile(const QUrl& url, const QString& label,
                                 const std::function<void(const QString&)>& then)
{
    m_download = std::make_unique<QTemporaryFile>(QDir(engineDirectory()).filePath(u"download-XXXXXX"_s));
    if (!m_download->open()) {
        fail(tr("Cannot write to %1.").arg(engineDirectory()));
        return;
    }
    setState(m_status.state, label, 0);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, u"Red/10 (+https://github.com/keshavbhatt/red)"_s);
    QNetworkReply* reply = m_network->get(request);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        if (m_download) {
            m_download->write(reply->readAll());
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, label](qint64 received, qint64 total) {
        if (total > 0) {
            setState(m_status.state, label, static_cast<double>(received) / static_cast<double>(total));
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, then] {
        reply->deleteLater();
        m_activeReply = nullptr;
        if (reply->error() != QNetworkReply::NoError) {
            fail(tr("Download failed: %1").arg(reply->errorString()));
            return;
        }
        if (!m_download) {
            return;
        }
        m_download->write(reply->readAll());
        m_download->flush();
        then(m_download->fileName());
    });
}

void EngineManager::installBinary(const core::EngineComponent& component, const QString& tempPath,
                                  const QString& expectedSha)
{
    if (!expectedSha.isEmpty()) {
        setState(m_status.state, tr("Verifying…"));
        const QString actual = sha256Of(tempPath);
        if (actual != expectedSha) {
            fail(tr("Checksum mismatch for %1.").arg(component.assetName));
            return;
        }
    }
    const QString target = installPath(component);
    QFile::remove(target);
    if (!QFile::copy(tempPath, target)) {
        fail(tr("Cannot install %1.").arg(component.installName));
        return;
    }
    QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup |
                                      QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
    m_download.reset();
    if (component.kind == core::EngineComponent::Kind::YtDlp && !m_pendingVersion.isEmpty()) {
        writeVersionFile(core::engineVersionFilePath(), m_pendingVersion);
    }
    qCInfo(lcServices) << "installed" << target;
    runNextStep();
}

void EngineManager::finishInstall()
{
    m_pendingVersion.clear();
    m_status.updateAvailable = false;
    detect();
    if (!m_status.isReady()) {
        fail(m_status.ffmpegMissing ? m_status.error
                                    : tr("The engine is still incomplete after installation."));
        return;
    }
    m_settings.setEngineLastCheck(QDateTime::currentDateTimeUtc());
}

// ---- updates ---------------------------------------------------------------

void EngineManager::checkForUpdates(bool force)
{
    if (m_status.isBusy() || m_status.systemYtdlp || m_status.ytdlpPath.isEmpty()) {
        return;
    }
    const QDateTime last = m_settings.engineLastCheck();
    if (!force && last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < kUpdateCheckHours * 3600) {
        return;
    }
    QNetworkRequest request(core::latestReleaseUrl(core::ytdlpComponent(m_os, m_cpu).repo));
    request.setHeader(QNetworkRequest::UserAgentHeader, u"Red/10 (+https://github.com/keshavbhatt/red)"_s);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply* reply = m_network->head(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qCInfo(lcServices) << "update check failed:" << reply->errorString();
            return;
        }
        const QString latest =
            core::tagFromReleaseUrl(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
        if (latest.isEmpty()) {
            qCInfo(lcServices) << "update check: no tag in the release redirect";
            return;
        }
        m_settings.setEngineLastCheck(QDateTime::currentDateTimeUtc());
        m_status.latestVersion = latest;
        m_status.updateAvailable = core::isNewerVersion(latest, m_status.ytdlpVersion);
        qCInfo(lcServices) << "yt-dlp latest" << latest << "installed" << m_status.ytdlpVersion
                           << (m_status.updateAvailable ? "(update available)" : "(current)");
        Q_EMIT statusChanged(m_status);
        if (m_status.updateAvailable && m_settings.engineAutoUpdate()) {
            update();
        }
    });
}

} // namespace red::services
