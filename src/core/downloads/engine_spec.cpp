#include "core/downloads/engine_spec.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace red::core {

namespace {

const QUrl kYtDlpApi(u"https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest"_s);
const QUrl kQuickJsApi(u"https://api.github.com/repos/quickjs-ng/quickjs/releases/latest"_s);

bool isArm64(const QString& cpu)
{
    return cpu == u"arm64"_s || cpu == u"aarch64"_s;
}

} // namespace

EngineComponent ytdlpComponent(const QString& os, const QString& cpu)
{
    EngineComponent c;
    c.kind = EngineComponent::Kind::YtDlp;
    c.releaseApi = kYtDlpApi;
    if (os == u"windows"_s) {
        c.assetName = isArm64(cpu) ? u"yt-dlp_arm64.exe"_s : u"yt-dlp.exe"_s;
        c.installName = u"yt-dlp.exe"_s;
    } else if (os == u"macos"_s) {
        c.assetName = u"yt-dlp_macos"_s;
        c.installName = u"yt-dlp"_s;
    } else {
        // Standalone glibc build: no Python on the host (ADR-004).
        c.assetName = isArm64(cpu) ? u"yt-dlp_linux_aarch64"_s : u"yt-dlp_linux"_s;
        c.installName = u"yt-dlp"_s;
    }
    return c;
}

EngineComponent jsRuntimeComponent(const QString& os, const QString& cpu)
{
    EngineComponent c;
    c.kind = EngineComponent::Kind::JsRuntime;
    c.releaseApi = kQuickJsApi;
    if (os == u"windows"_s) {
        c.assetName = u"qjs-windows-x86_64.exe"_s;
        c.installName = u"qjs.exe"_s;
    } else if (os == u"macos"_s) {
        c.assetName = isArm64(cpu) ? u"qjs-darwin-arm64"_s : u"qjs-darwin-x86_64"_s;
        c.installName = u"qjs"_s;
    } else {
        c.assetName = isArm64(cpu) ? u"qjs-linux-aarch64"_s : u"qjs-linux-x86_64"_s;
        c.installName = u"qjs"_s; // yt-dlp requires the executable to be named qjs
    }
    return c;
}

QString engineDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/engine"_s;
}

QString engineVersionFilePath()
{
    return engineDirectory() + u"/yt-dlp.version"_s;
}

bool isNewerVersion(const QString& remote, const QString& local)
{
    if (remote.isEmpty()) {
        return false;
    }
    if (local.isEmpty()) {
        return true;
    }
    const QDate r = QDate::fromString(remote.section(u'.', 0, 2), u"yyyy.MM.dd"_s);
    const QDate l = QDate::fromString(local.section(u'.', 0, 2), u"yyyy.MM.dd"_s);
    if (r.isValid() && l.isValid()) {
        if (r != l) {
            return r > l;
        }
        // Same day: a ".1" hotfix suffix wins.
        return remote.section(u'.', 3).toInt() > local.section(u'.', 3).toInt();
    }
    return QString::compare(remote, local) > 0;
}

QString releaseTagFromJson(const QByteArray& json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    return doc.isObject() ? doc.object().value(u"tag_name"_s).toString() : QString();
}

QUrl assetUrlFromJson(const QByteArray& json, const QString& assetName)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return {};
    }
    const QJsonArray assets = doc.object().value(u"assets"_s).toArray();
    for (const auto& v : assets) {
        const QJsonObject a = v.toObject();
        if (a.value(u"name"_s).toString() == assetName) {
            return QUrl(a.value(u"browser_download_url"_s).toString());
        }
    }
    return {};
}

QString sha256FromSums(const QByteArray& sums, const QString& assetName)
{
    const QStringList lines = QString::fromUtf8(sums).split(u'\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QStringList parts = line.simplified().split(u' ');
        if (parts.size() >= 2 && parts.last() == assetName && parts.first().size() == 64) {
            return parts.first().toLower();
        }
    }
    return {};
}

QString detectSystemJsRuntime(const QStringList& extraDirs)
{
    // Deno is yt-dlp's recommended runtime; a system qjs works as well.
    const QString deno = QStandardPaths::findExecutable(u"deno"_s, extraDirs);
    if (!deno.isEmpty()) {
        return u"deno:"_s + deno;
    }
    const QString denoPath = QStandardPaths::findExecutable(u"deno"_s);
    if (!denoPath.isEmpty()) {
        return u"deno:"_s + denoPath;
    }
    const QString qjs = QStandardPaths::findExecutable(u"qjs"_s, extraDirs);
    if (!qjs.isEmpty()) {
        return u"quickjs:"_s + qjs;
    }
    const QString qjsPath = QStandardPaths::findExecutable(u"qjs"_s);
    if (!qjsPath.isEmpty()) {
        return u"quickjs:"_s + qjsPath;
    }
    return {};
}

QString detectSystemFfmpeg(const QStringList& extraDirs)
{
    const QString local = QStandardPaths::findExecutable(u"ffmpeg"_s, extraDirs);
    if (!local.isEmpty()) {
        return local;
    }
    return QStandardPaths::findExecutable(u"ffmpeg"_s);
}

QString ffmpegInstallHint()
{
    QFile osRelease(u"/etc/os-release"_s);
    QString id;
    if (osRelease.open(QIODevice::ReadOnly)) {
        const QString text = QString::fromUtf8(osRelease.readAll());
        for (const QString& line : text.split(u'\n')) {
            if (line.startsWith(u"ID="_s) || line.startsWith(u"ID_LIKE="_s)) {
                id += line.section(u'=', 1).remove(u'"').toLower() + u' ';
            }
        }
    }
    if (id.contains(u"arch"_s)) {
        return u"sudo pacman -S ffmpeg"_s;
    }
    if (id.contains(u"fedora"_s) || id.contains(u"rhel"_s)) {
        return u"sudo dnf install ffmpeg"_s;
    }
    if (id.contains(u"suse"_s)) {
        return u"sudo zypper install ffmpeg"_s;
    }
    if (id.contains(u"debian"_s) || id.contains(u"ubuntu"_s)) {
        return u"sudo apt install ffmpeg"_s;
    }
    return u"install the ffmpeg package with your package manager"_s;
}

QProcessEnvironment engineProcessEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!env.contains(u"SNAP"_s)) {
        env.remove(u"LD_LIBRARY_PATH"_s);
        env.remove(u"LD_PRELOAD"_s);
    }
    // tar/xz/ffmpeg helpers must be findable even from a minimal launcher PATH.
    const QString path = env.value(u"PATH"_s);
    if (!path.split(u':').contains(u"/usr/bin"_s)) {
        env.insert(u"PATH"_s, path.isEmpty() ? u"/usr/bin:/bin"_s : path + u":/usr/bin:/bin"_s);
    }
    // yt-dlp's own update nag and colour output are noise in our logs.
    env.insert(u"NO_COLOR"_s, u"1"_s);
    return env;
}

QString jsRuntimeLabel(const QString& spec)
{
    const QString name = spec.section(u':', 0, 0);
    if (name == u"deno"_s) {
        return u"Deno"_s;
    }
    if (name == u"quickjs"_s) {
        return u"QuickJS"_s;
    }
    if (name == u"node"_s) {
        return u"Node.js"_s;
    }
    if (name == u"bun"_s) {
        return u"Bun"_s;
    }
    return name;
}

} // namespace red::core
