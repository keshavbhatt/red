#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QUrl>

// Pure knowledge about the download engine's components (ADR-004): which
// release assets fit this machine, where they live, how versions compare.
// The network side is services::EngineManager.
namespace red::core {

struct EngineComponent
{
    enum class Kind
    {
        YtDlp,
        JsRuntime,
    };
    Kind kind = Kind::YtDlp;
    QString assetName;   ///< release asset file name
    QString installName; ///< file name inside the engine directory
    QUrl releaseApi;     ///< GitHub "latest release" JSON
};

/// Components for the running platform/CPU (`cpuArchitecture` as QSysInfo gives it).
[[nodiscard]] EngineComponent ytdlpComponent(const QString& os, const QString& cpu);
[[nodiscard]] EngineComponent jsRuntimeComponent(const QString& os, const QString& cpu);

/// <AppDataLocation>/engine
[[nodiscard]] QString engineDirectory();
[[nodiscard]] QString engineVersionFilePath();

/// yt-dlp tags are dates ("2026.08.19"); compares as dates, falls back to strings.
[[nodiscard]] bool isNewerVersion(const QString& remote, const QString& local);

/// Reads "tag_name" out of a GitHub release JSON; empty on failure.
[[nodiscard]] QString releaseTagFromJson(const QByteArray& json);
/// The browser_download_url of `assetName` in a GitHub release JSON; empty if absent.
[[nodiscard]] QUrl assetUrlFromJson(const QByteArray& json, const QString& assetName);
/// Parses a SHA2-256SUMS file ("<hex>  <name>" lines) for `assetName`.
[[nodiscard]] QString sha256FromSums(const QByteArray& sums, const QString& assetName);

/// A JS runtime found on this machine, as a --js-runtimes spec ("deno:/usr/bin/deno"),
/// searching `extraDirs` first. Empty when none.
[[nodiscard]] QString detectSystemJsRuntime(const QStringList& extraDirs);
/// ffmpeg on this machine (binary path), searching `extraDirs` first. Empty when
/// none — ffmpeg is never downloaded (owner decision, ADR-004 revision): the
/// snap/flatpak runtimes ship it and distro packages exist everywhere.
[[nodiscard]] QString detectSystemFfmpeg(const QStringList& extraDirs);
/// A one-line install hint for the running distro family ("sudo apt install ffmpeg").
[[nodiscard]] QString ffmpegInstallHint();

/// The environment engine processes run with: the system environment minus
/// LD_LIBRARY_PATH / LD_PRELOAD, unless we run confined ($SNAP), where the
/// staged libraries need them. A dev run against the snap runtime, or an
/// AppImage, otherwise breaks system ffmpeg ("libxml2.so.2: cannot open").
[[nodiscard]] QProcessEnvironment engineProcessEnvironment();

/// "deno", "QuickJS", "Node.js" … from a --js-runtimes spec.
[[nodiscard]] QString jsRuntimeLabel(const QString& spec);

} // namespace red::core
